#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <queue>
#include <chrono>
#include <cstdint>

struct MboEvent;

// Top-10 orderbook state for snapshot filtering
struct Top10State {
    double bid_prices[10];
    uint64_t bid_sizes[10];
    uint32_t bid_counts[10];
    
    double ask_prices[10];
    uint64_t ask_sizes[10];
    uint32_t ask_counts[10];
    
    Top10State() {
        for (int i = 0; i < 10; ++i) {
            bid_prices[i] = ask_prices[i] = 0.0;
            bid_sizes[i] = ask_sizes[i] = 0;
            bid_counts[i] = ask_counts[i] = 0;
        }
    }
    
    bool operator==(const Top10State& other) const {
        for (int i = 0; i < 10; ++i) {
            if (bid_prices[i] != other.bid_prices[i] || 
                bid_sizes[i] != other.bid_sizes[i] || 
                bid_counts[i] != other.bid_counts[i]) {
                return false;
            }
            
            if (ask_prices[i] != other.ask_prices[i] || 
                ask_sizes[i] != other.ask_sizes[i] || 
                ask_counts[i] != other.ask_counts[i]) {
                return false;
            }
        }
        return true;
    }
    
    // Alternative market-relevant change detection
    bool hasMarketRelevantChange(const Top10State& other) const {
        // Best levels changed
        if (bid_prices[0] != other.bid_prices[0] || bid_sizes[0] != other.bid_sizes[0] ||
            ask_prices[0] != other.ask_prices[0] || ask_sizes[0] != other.ask_sizes[0]) {
            return true;
        }
        
        // Level appeared/disappeared
        for (int i = 0; i < 10; ++i) {
            bool had_bid = (bid_prices[i] > 0.0);
            bool has_bid = (other.bid_prices[i] > 0.0);
            bool had_ask = (ask_prices[i] > 0.0);  
            bool has_ask = (other.ask_prices[i] > 0.0);
            
            if (had_bid != has_bid || had_ask != has_ask) {
                return true;
            }
        }
        
        // Existing level size/count changed
        for (int i = 0; i < 10; ++i) {
            if (bid_prices[i] > 0.0 && other.bid_prices[i] > 0.0) {
                if (bid_sizes[i] != other.bid_sizes[i] || bid_counts[i] != other.bid_counts[i]) {
                    return true;
                }
            }
            if (ask_prices[i] > 0.0 && other.ask_prices[i] > 0.0) {
                if (ask_sizes[i] != other.ask_sizes[i] || ask_counts[i] != other.ask_counts[i]) {
                    return true;
                }
            }
        }
        
        return false;
    }
    
    bool operator!=(const Top10State& other) const {
        return !(*this == other);
    }
};

// MBO event processing result
struct ProcessResult {
    bool should_write;
    char snapshot_action;
    char snapshot_side;
    
    operator bool() const { return true; }
};

// Order entry for FIFO tracking
struct OrderEntry {
    uint64_t order_id;
    uint64_t size;
    
    OrderEntry(uint64_t id, uint64_t sz) : order_id(id), size(sz) {}
};

// Price level aggregated data
struct LevelData {
    double price;
    uint64_t total_size;
    uint32_t order_count;
    std::queue<OrderEntry> order_queue;
    
    LevelData() : price(0.0), total_size(0), order_count(0) {}
    LevelData(double p, uint64_t size, uint64_t order_id) 
        : price(p), total_size(size), order_count(1) {
        order_queue.emplace(order_id, size);
    }
};

// Individual order data
struct OrderData {
    double price;
    uint64_t size;
    char side;
    void* level_iterator;
    
    OrderData() : price(0.0), size(0), side('\0'), level_iterator(nullptr) {}
    OrderData(double p, uint64_t s, char sd) : price(p), size(s), side(sd), level_iterator(nullptr) {}
};

// MBP-10 snapshot with 60 fields
struct MbpSnapshot {
    std::chrono::nanoseconds timestamp;
    uint64_t sequence_number;
    char action;
    char side;
    int32_t depth;
    
    double event_price;
    uint64_t event_size;
    uint64_t event_order_id;
    uint8_t event_flags;
    int32_t event_ts_in_delta;
    
    // Bid levels (00-09)
    double bid_px_00, bid_px_01, bid_px_02, bid_px_03, bid_px_04;
    double bid_px_05, bid_px_06, bid_px_07, bid_px_08, bid_px_09;
    
    uint64_t bid_sz_00, bid_sz_01, bid_sz_02, bid_sz_03, bid_sz_04;
    uint64_t bid_sz_05, bid_sz_06, bid_sz_07, bid_sz_08, bid_sz_09;
    
    uint32_t bid_ct_00, bid_ct_01, bid_ct_02, bid_ct_03, bid_ct_04;
    uint32_t bid_ct_05, bid_ct_06, bid_ct_07, bid_ct_08, bid_ct_09;
    
    // Ask levels (00-09)
    double ask_px_00, ask_px_01, ask_px_02, ask_px_03, ask_px_04;
    double ask_px_05, ask_px_06, ask_px_07, ask_px_08, ask_px_09;
    
    uint64_t ask_sz_00, ask_sz_01, ask_sz_02, ask_sz_03, ask_sz_04;
    uint64_t ask_sz_05, ask_sz_06, ask_sz_07, ask_sz_08, ask_sz_09;
    
    uint32_t ask_ct_00, ask_ct_01, ask_ct_02, ask_ct_03, ask_ct_04;
    uint32_t ask_ct_05, ask_ct_06, ask_ct_07, ask_ct_08, ask_ct_09;
    
    MbpSnapshot() : timestamp(0), sequence_number(0), action('S'), side('N'), depth(0),
                    event_price(0.0), event_size(0), event_order_id(0), event_flags(0), event_ts_in_delta(0) {
        bid_px_00 = bid_px_01 = bid_px_02 = bid_px_03 = bid_px_04 = 0.0;
        bid_px_05 = bid_px_06 = bid_px_07 = bid_px_08 = bid_px_09 = 0.0;
        
        bid_sz_00 = bid_sz_01 = bid_sz_02 = bid_sz_03 = bid_sz_04 = 0;
        bid_sz_05 = bid_sz_06 = bid_sz_07 = bid_sz_08 = bid_sz_09 = 0;
        
        bid_ct_00 = bid_ct_01 = bid_ct_02 = bid_ct_03 = bid_ct_04 = 0;
        bid_ct_05 = bid_ct_06 = bid_ct_07 = bid_ct_08 = bid_ct_09 = 0;
        
        ask_px_00 = ask_px_01 = ask_px_02 = ask_px_03 = ask_px_04 = 0.0;
        ask_px_05 = ask_px_06 = ask_px_07 = ask_px_08 = ask_px_09 = 0.0;
        
        ask_sz_00 = ask_sz_01 = ask_sz_02 = ask_sz_03 = ask_sz_04 = 0;
        ask_sz_05 = ask_sz_06 = ask_sz_07 = ask_sz_08 = ask_sz_09 = 0;
        
        ask_ct_00 = ask_ct_01 = ask_ct_02 = ask_ct_03 = ask_ct_04 = 0;
        ask_ct_05 = ask_ct_06 = ask_ct_07 = ask_ct_08 = ask_ct_09 = 0;
    }
};

// High-performance order book
class OrderBook {
public:
    OrderBook();
    
    ProcessResult processEvent(const MboEvent& event);
    MbpSnapshot generateSnapshot(const MboEvent& event) const;
    MbpSnapshot generateSnapshot(char action = 'S', char side = 'N') const;
    Top10State captureTop10State() const;
    
    // Statistics
    size_t getBidLevelCount() const { return bid_levels_.size(); }
    size_t getAskLevelCount() const { return ask_levels_.size(); }
    size_t getOrderCount() const { return orders_.size(); }
    
    bool orderExists(uint64_t order_id) const { return orders_.find(order_id) != orders_.end(); }
    
    std::pair<double, double> getBestBidAsk() const;
    double getBestBidPrice() const;
    double getBestAskPrice() const;
    
    // Trade state
    bool isInTradeSequence() const { return trade_state_ == TradeState::EXPECTING_FILL; }
    bool wasLastFillFromTrade() const { return last_fill_was_trade_; }
    void resetTradeFlag() { last_fill_was_trade_ = false; }
    
    void clear();
    void addOrder(uint64_t order_id, double price, uint64_t size, char side);
    bool hasOrdersAtPrice(double price, char side) const;
    void fillOrdersAtPrice(double price, uint64_t size, char side);

private:
    using BidLevels = std::map<double, LevelData, std::greater<double>>;
    using AskLevels = std::map<double, LevelData, std::less<double>>;
    
    BidLevels bid_levels_;
    AskLevels ask_levels_;
    std::unordered_map<uint64_t, OrderData> orders_;
    uint64_t sequence_counter_;
    
    // Trade state machine
    enum class TradeState {
        NORMAL,
        EXPECTING_FILL
    };
    
    TradeState trade_state_;
    char pending_trade_side_;
    char pending_actual_trade_side_;
    double pending_trade_price_;
    uint64_t pending_trade_size_;
    bool last_fill_was_trade_;
    
    ProcessResult processAddEvent(const MboEvent& event);
    ProcessResult processCancelEvent(const MboEvent& event);
    ProcessResult processTradeEvent(const MboEvent& event);
    ProcessResult processFillEvent(const MboEvent& event);
    ProcessResult processResetEvent(const MboEvent& event);
    
    void cancelOrder(uint64_t order_id, uint64_t cancel_size = 0);
    void updateBidLevel(double price, int64_t size_delta, int32_t count_delta, uint64_t order_id = 0);
    void updateAskLevel(double price, int64_t size_delta, int32_t count_delta, uint64_t order_id = 0);
    
    void processTradeFill(char trade_side, double price, uint64_t size);
    char getOppositeSide(char side) const;
    void fillOrdersAtLevel(LevelData& level, uint64_t fill_size, char side);
    void updateOrderInQueue(LevelData& level, uint64_t order_id, uint64_t cancel_size);
};
