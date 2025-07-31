#include "../include/order_book.h"
#include "mbo_parser.h"
#include <iostream>
#include <algorithm>

OrderBook::OrderBook() : sequence_counter_(0), trade_state_(TradeState::NORMAL), 
                         pending_trade_side_('\0'), pending_actual_trade_side_('\0'), 
                         pending_trade_price_(0.0), pending_trade_size_(0),
                         last_fill_was_trade_(false) {
    orders_.reserve(10000);
}

ProcessResult OrderBook::processEvent(const MboEvent& event) {
    ++sequence_counter_;
    
    switch (event.action) {
        case 'A':
            return processAddEvent(event);
        case 'C':
            return processCancelEvent(event);
        case 'T':
            return processTradeEvent(event);
        case 'F':
            return processFillEvent(event);
        case 'R':
            return processResetEvent(event);
        default:
            std::cerr << "Warning: Unknown action '" << event.action << "'" << std::endl;
            return {false, ' ', ' '};
    }
}

ProcessResult OrderBook::processAddEvent(const MboEvent& event) {
    if (event.order_id == 0) {
        return {true, 'A', event.side};
    }
    
    if (orders_.find(event.order_id) != orders_.end()) {
        std::cerr << "Warning: Order " << event.order_id << " already exists" << std::endl;
        return {false, ' ', ' '};
    }
    
    addOrder(event.order_id, event.price, event.size, event.side);
    last_fill_was_trade_ = false;
    return {true, 'A', event.side};
}

ProcessResult OrderBook::processCancelEvent(const MboEvent& event) {
    if (event.order_id == 0) {
        return {true, 'C', event.side};
    }

    if (last_fill_was_trade_ && trade_state_ == TradeState::EXPECTING_FILL) {
        char target_side = getOppositeSide(pending_trade_side_);
        double trade_price = pending_trade_price_;
        uint64_t trade_size = pending_trade_size_;
        
        if (target_side == 'B') {
            auto level_it = bid_levels_.find(trade_price);
            if (level_it != bid_levels_.end()) {
                fillOrdersAtLevel(level_it->second, trade_size, target_side);
                if (level_it->second.total_size == 0) {
                    bid_levels_.erase(level_it);
                }
            }
        } else if (target_side == 'A') {
            auto level_it = ask_levels_.find(trade_price);
            if (level_it != ask_levels_.end()) {
                fillOrdersAtLevel(level_it->second, trade_size, target_side);
                if (level_it->second.total_size == 0) {
                    ask_levels_.erase(level_it);
                }
            }
        }
        
        char filled_side = pending_actual_trade_side_;
        
        trade_state_ = TradeState::NORMAL;
        pending_trade_side_ = '\0';
        pending_actual_trade_side_ = '\0';
        pending_trade_price_ = 0.0;
        pending_trade_size_ = 0;
        last_fill_was_trade_ = false;
        
        return {true, 'T', filled_side};
    }
    
    auto it = orders_.find(event.order_id);
    if (it == orders_.end()) {
        return {true, 'C', 'N'};
    }
    
    char side = it->second.side;
    
    uint64_t cancel_size = event.size;
    
    if (cancel_size == 0) {
        cancelOrder(event.order_id);
    } else {
        cancelOrder(event.order_id, cancel_size);
    }
    
    return {true, 'C', side};
}

ProcessResult OrderBook::processTradeEvent(const MboEvent& event) {
    if (event.side == 'N') {
        return {true, 'T', 'N'};
    }
    
    trade_state_ = TradeState::EXPECTING_FILL;
    pending_trade_side_ = event.side;
    pending_trade_price_ = event.price;
    pending_trade_size_ = event.size;
    
    return {false, 'T', event.side};
}

ProcessResult OrderBook::processFillEvent(const MboEvent& event) {
    if (trade_state_ != TradeState::EXPECTING_FILL) {
        std::cerr << "Warning: Unexpected fill event" << std::endl;
        return {false, ' ', ' '};
    }
    
    last_fill_was_trade_ = true;
    
    pending_actual_trade_side_ = event.side;
    
    return {false, ' ', ' '};
}

ProcessResult OrderBook::processResetEvent(const MboEvent& event) {
    clear();
    return {true, 'R', 'N'};
}

void OrderBook::addOrder(uint64_t order_id, double price, uint64_t size, char side) {
    OrderData order_data(price, size, side);
    orders_[order_id] = order_data;
    
    if (side == 'B') {
        updateBidLevel(price, static_cast<int64_t>(size), 1, order_id);
        
        auto level_it = bid_levels_.find(price);
        if (level_it != bid_levels_.end()) {
            orders_[order_id].level_iterator = &(*level_it);
        }
    } else if (side == 'A') {
        updateAskLevel(price, static_cast<int64_t>(size), 1, order_id);
        
        auto level_it = ask_levels_.find(price);
        if (level_it != ask_levels_.end()) {
            orders_[order_id].level_iterator = &(*level_it);
        }
    }
}

void OrderBook::cancelOrder(uint64_t order_id, uint64_t cancel_size) {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return;
    }
    
    OrderData& order = it->second;
    uint64_t actual_cancel_size = (cancel_size == 0) ? order.size : std::min(cancel_size, order.size);
    
    // Update price levels - reduce size but don't change order count yet
    if (order.side == 'B') {
        updateBidLevel(order.price, -static_cast<int64_t>(actual_cancel_size), 0, 0);
        // Also update the order in the queue
        updateOrderInQueue(bid_levels_[order.price], order_id, actual_cancel_size);
    } else if (order.side == 'A') {
        updateAskLevel(order.price, -static_cast<int64_t>(actual_cancel_size), 0, 0);
        // Also update the order in the queue
        updateOrderInQueue(ask_levels_[order.price], order_id, actual_cancel_size);
    }
    
    // Update order size
    order.size -= actual_cancel_size;
    
    // If order size becomes zero, remove the order completely
    if (order.size == 0) {
        // Decrement order count at the price level
        if (order.side == 'B') {
            updateBidLevel(order.price, 0, -1, 0);
        } else if (order.side == 'A') {
            updateAskLevel(order.price, 0, -1, 0);
        }
        
        // Remove from orders map
        orders_.erase(it);
    }
}

void OrderBook::updateBidLevel(double price, int64_t size_delta, int32_t count_delta, uint64_t order_id) {
    auto it = bid_levels_.find(price);
    
    if (it == bid_levels_.end()) {
        if (size_delta > 0) {
            bid_levels_[price] = LevelData(price, static_cast<uint64_t>(size_delta), order_id);
            bid_levels_[price].order_count = static_cast<uint32_t>(count_delta);
        }
    } else {
        it->second.total_size = static_cast<uint64_t>(
            static_cast<int64_t>(it->second.total_size) + size_delta);
        it->second.order_count = static_cast<uint32_t>(
            static_cast<int32_t>(it->second.order_count) + count_delta);
        
        if (count_delta > 0 && order_id != 0) {
            it->second.order_queue.emplace(order_id, static_cast<uint64_t>(size_delta));
        }
        
        if (it->second.total_size == 0 || it->second.order_count == 0) {
            bid_levels_.erase(it);
        }
    }
}

void OrderBook::updateAskLevel(double price, int64_t size_delta, int32_t count_delta, uint64_t order_id) {
    auto it = ask_levels_.find(price);
    
    if (it == ask_levels_.end()) {
        if (size_delta > 0) {
            ask_levels_[price] = LevelData(price, static_cast<uint64_t>(size_delta), order_id);
            ask_levels_[price].order_count = static_cast<uint32_t>(count_delta);
        }
    } else {
        it->second.total_size = static_cast<uint64_t>(
            static_cast<int64_t>(it->second.total_size) + size_delta);
        it->second.order_count = static_cast<uint32_t>(
            static_cast<int32_t>(it->second.order_count) + count_delta);
        
        if (count_delta > 0 && order_id != 0) {
            it->second.order_queue.emplace(order_id, static_cast<uint64_t>(size_delta));
        }
        
        if (it->second.total_size == 0 || it->second.order_count == 0) {
            ask_levels_.erase(it);
        }
    }
}

MbpSnapshot OrderBook::generateSnapshot(const MboEvent& event) const {
    MbpSnapshot snapshot;
    snapshot.sequence_number = event.sequence;
    snapshot.action = event.action;
    snapshot.side = event.side;
    snapshot.timestamp = event.ts_event;
    
    snapshot.event_price = event.price;
    snapshot.event_size = event.size;
    snapshot.event_order_id = event.order_id;
    snapshot.event_flags = event.flags;
    snapshot.event_ts_in_delta = event.ts_in_delta;
    
    double* bid_prices[] = {&snapshot.bid_px_00, &snapshot.bid_px_01, &snapshot.bid_px_02, &snapshot.bid_px_03, &snapshot.bid_px_04,
                           &snapshot.bid_px_05, &snapshot.bid_px_06, &snapshot.bid_px_07, &snapshot.bid_px_08, &snapshot.bid_px_09};
    
    uint64_t* bid_sizes[] = {&snapshot.bid_sz_00, &snapshot.bid_sz_01, &snapshot.bid_sz_02, &snapshot.bid_sz_03, &snapshot.bid_sz_04,
                            &snapshot.bid_sz_05, &snapshot.bid_sz_06, &snapshot.bid_sz_07, &snapshot.bid_sz_08, &snapshot.bid_sz_09};
    
    uint32_t* bid_counts[] = {&snapshot.bid_ct_00, &snapshot.bid_ct_01, &snapshot.bid_ct_02, &snapshot.bid_ct_03, &snapshot.bid_ct_04,
                             &snapshot.bid_ct_05, &snapshot.bid_ct_06, &snapshot.bid_ct_07, &snapshot.bid_ct_08, &snapshot.bid_ct_09};
    
    double* ask_prices[] = {&snapshot.ask_px_00, &snapshot.ask_px_01, &snapshot.ask_px_02, &snapshot.ask_px_03, &snapshot.ask_px_04,
                           &snapshot.ask_px_05, &snapshot.ask_px_06, &snapshot.ask_px_07, &snapshot.ask_px_08, &snapshot.ask_px_09};
    
    uint64_t* ask_sizes[] = {&snapshot.ask_sz_00, &snapshot.ask_sz_01, &snapshot.ask_sz_02, &snapshot.ask_sz_03, &snapshot.ask_sz_04,
                            &snapshot.ask_sz_05, &snapshot.ask_sz_06, &snapshot.ask_sz_07, &snapshot.ask_sz_08, &snapshot.ask_sz_09};
    
    uint32_t* ask_counts[] = {&snapshot.ask_ct_00, &snapshot.ask_ct_01, &snapshot.ask_ct_02, &snapshot.ask_ct_03, &snapshot.ask_ct_04,
                             &snapshot.ask_ct_05, &snapshot.ask_ct_06, &snapshot.ask_ct_07, &snapshot.ask_ct_08, &snapshot.ask_ct_09};
    
    auto bid_it = bid_levels_.begin();
    for (int i = 0; i < 10; ++i) {
        if (bid_it != bid_levels_.end()) {
            *bid_prices[i] = bid_it->second.price;
            *bid_sizes[i] = bid_it->second.total_size;
            *bid_counts[i] = bid_it->second.order_count;
            ++bid_it;
        } else {
            break;
        }
    }
    
    auto ask_it = ask_levels_.begin();
    for (int i = 0; i < 10; ++i) {
        if (ask_it != ask_levels_.end()) {
            *ask_prices[i] = ask_it->second.price;
            *ask_sizes[i] = ask_it->second.total_size;
            *ask_counts[i] = ask_it->second.order_count;
            ++ask_it;
        } else {
            break;
        }
    }
    
    return snapshot;
}

MbpSnapshot OrderBook::generateSnapshot(char action, char side) const {
    MboEvent dummy_event;
    dummy_event.action = action;
    dummy_event.side = side;
    dummy_event.ts_event = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch());
    dummy_event.price = 0.0;
    dummy_event.size = 0;
    dummy_event.order_id = 0;
    
    return generateSnapshot(dummy_event);
}

void OrderBook::clear() {
    bid_levels_.clear();
    ask_levels_.clear();
    orders_.clear();
    
    trade_state_ = TradeState::NORMAL;
    pending_trade_side_ = '\0';
    pending_actual_trade_side_ = '\0';
    pending_trade_price_ = 0.0;
    pending_trade_size_ = 0;
    last_fill_was_trade_ = false;
}

void OrderBook::processTradeFill(char trade_side, double price, uint64_t size) {
    char target_side = getOppositeSide(trade_side);
    
    if (target_side == 'B') {
        auto level_it = bid_levels_.find(price);
        if (level_it != bid_levels_.end()) {
            fillOrdersAtLevel(level_it->second, size, target_side);
            if (level_it->second.total_size == 0) {
                bid_levels_.erase(level_it);
            }
        }
    } else if (target_side == 'A') {
        auto level_it = ask_levels_.find(price);
        if (level_it != ask_levels_.end()) {
            fillOrdersAtLevel(level_it->second, size, target_side);
            if (level_it->second.total_size == 0) {
                ask_levels_.erase(level_it);
            }
        }
    }
}

char OrderBook::getOppositeSide(char side) const {
    if (side == 'B') return 'A';
    if (side == 'A') return 'B';
    return '\0';
}

void OrderBook::fillOrdersAtLevel(LevelData& level, uint64_t fill_size, char side) {
    uint64_t remaining_fill = fill_size;
    
    while (remaining_fill > 0 && !level.order_queue.empty()) {
        OrderEntry& front_order = level.order_queue.front();
        
        if (front_order.size <= remaining_fill) {
            remaining_fill -= front_order.size;
            level.total_size -= front_order.size;
            level.order_count--;
            
            orders_.erase(front_order.order_id);
            
            level.order_queue.pop();
        } else {
            front_order.size -= remaining_fill;
            level.total_size -= remaining_fill;
            
            auto order_it = orders_.find(front_order.order_id);
            if (order_it != orders_.end()) {
                order_it->second.size = front_order.size;
            }
            
            remaining_fill = 0;
        }
    }
}

void OrderBook::updateOrderInQueue(LevelData& level, uint64_t order_id, uint64_t cancel_size) {
    std::queue<OrderEntry> new_queue;
    bool order_found = false;
    
    while (!level.order_queue.empty()) {
        OrderEntry entry = level.order_queue.front();
        level.order_queue.pop();
        
        if (entry.order_id == order_id) {
            order_found = true;
            if (entry.size > cancel_size) {
                entry.size -= cancel_size;
                new_queue.push(entry);
            }
        } else {
            new_queue.push(entry);
        }
    }
    
    level.order_queue = new_queue;
}

std::pair<double, double> OrderBook::getBestBidAsk() const {
    double best_bid = 0.0;
    double best_ask = 0.0;
    
    if (!bid_levels_.empty()) {
        best_bid = bid_levels_.begin()->first;
    }
    
    if (!ask_levels_.empty()) {
        best_ask = ask_levels_.begin()->first;
    }
    
    return std::make_pair(best_bid, best_ask);
}

double OrderBook::getBestBidPrice() const {
    if (!bid_levels_.empty()) {
        return bid_levels_.begin()->first;
    }
    return 0.0;
}

double OrderBook::getBestAskPrice() const {
    if (!ask_levels_.empty()) {
        return ask_levels_.begin()->first;
    }
    return 0.0;
}

bool OrderBook::hasOrdersAtPrice(double price, char side) const {
    if (side == 'B') {
        auto it = bid_levels_.find(price);
        return it != bid_levels_.end() && it->second.total_size > 0;
    } else if (side == 'A') {
        auto it = ask_levels_.find(price);
        return it != ask_levels_.end() && it->second.total_size > 0;
    }
    return false;
}

void OrderBook::fillOrdersAtPrice(double price, uint64_t size, char side) {
    if (side == 'B') {
        auto it = bid_levels_.find(price);
        if (it != bid_levels_.end()) {
            fillOrdersAtLevel(it->second, size, side);
            if (it->second.total_size == 0) {
                bid_levels_.erase(it);
            }
        }
    } else if (side == 'A') {
        auto it = ask_levels_.find(price);
        if (it != ask_levels_.end()) {
            fillOrdersAtLevel(it->second, size, side);
            if (it->second.total_size == 0) {
                ask_levels_.erase(it);
            }
        }
    }
}

Top10State OrderBook::captureTop10State() const {
    Top10State state;
    
    // Capture bid levels (highest to lowest price)
    int bid_index = 0;
    for (auto it = bid_levels_.begin(); it != bid_levels_.end() && bid_index < 10; ++it, ++bid_index) {
        state.bid_prices[bid_index] = it->first;
        state.bid_sizes[bid_index] = it->second.total_size;
        state.bid_counts[bid_index] = it->second.order_count;
    }
    
    // Capture ask levels (lowest to highest price)
    int ask_index = 0;
    for (auto it = ask_levels_.begin(); it != ask_levels_.end() && ask_index < 10; ++it, ++ask_index) {
        state.ask_prices[ask_index] = it->first;
        state.ask_sizes[ask_index] = it->second.total_size;
        state.ask_counts[ask_index] = it->second.order_count;
    }
    
    return state;
}
