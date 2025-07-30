#pragma once

#include <map>
#include <unordered_map>
#include <vector>
#include <queue>
#include <chrono>
#include <cstdint>

// Forward declarations
struct MboEvent;

// Lightweight structure to capture and compare top-10 orderbook state
struct Top10State {
    // Bid levels (prices, sizes, counts for levels 0-9)
    double bid_prices[10];
    uint64_t bid_sizes[10];
    uint32_t bid_counts[10];
    
    // Ask levels (prices, sizes, counts for levels 0-9)
    double ask_prices[10];
    uint64_t ask_sizes[10];
    uint32_t ask_counts[10];
    
    // Default constructor - zero all fields
    Top10State() {
        for (int i = 0; i < 10; ++i) {
            bid_prices[i] = ask_prices[i] = 0.0;
            bid_sizes[i] = ask_sizes[i] = 0;
            bid_counts[i] = ask_counts[i] = 0;
        }
    }
    
    // Equality operator for state comparison
    bool operator==(const Top10State& other) const {
        for (int i = 0; i < 10; ++i) {
            if (bid_prices[i] != other.bid_prices[i] || 
                bid_sizes[i] != other.bid_sizes[i] || 
                bid_counts[i] != other.bid_counts[i] ||
                ask_prices[i] != other.ask_prices[i] || 
                ask_sizes[i] != other.ask_sizes[i] || 
                ask_counts[i] != other.ask_counts[i]) {
                return false;
            }
        }
        return true;
    }
    
    // Inequality operator
    bool operator!=(const Top10State& other) const {
        return !(*this == other);
    }
};

// Result of processing an MBO event, telling the main loop what to do
struct ProcessResult {
    bool should_write;    // True if a snapshot should be written
    char snapshot_action; // Action for the snapshot ('A', 'C', 'T', 'R')
    char snapshot_side;   // Side for the snapshot ('A', 'B', 'N')
    
    // Convert to bool for legacy compatibility with tests
    operator bool() const { return true; } // Most events are processed successfully
};

// Order entry for FIFO tracking within price levels
struct OrderEntry {
    uint64_t order_id;
    uint64_t size;
    
    OrderEntry(uint64_t id, uint64_t sz) : order_id(id), size(sz) {}
};

// Price level data for aggregated order information
struct LevelData {
    double price;
    uint64_t total_size;
    uint32_t order_count;
    
    // FIFO queue for tracking order insertion order (oldest to newest)
    std::queue<OrderEntry> order_queue;
    
    LevelData() : price(0.0), total_size(0), order_count(0) {}
    LevelData(double p, uint64_t size, uint64_t order_id) 
        : price(p), total_size(size), order_count(1) {
        order_queue.emplace(order_id, size);
    }
};

// Individual order data for fast lookups
struct OrderData {
    double price;
    uint64_t size;
    char side;  // 'B' for bid, 'A' for ask
    
    // Iterator to the price level in the order book for O(1) level updates
    // This will be set appropriately when order is added
    void* level_iterator;  // Actual type will be map::iterator
    
    OrderData() : price(0.0), size(0), side('\0'), level_iterator(nullptr) {}
    OrderData(double p, uint64_t s, char sd) : price(p), size(s), side(sd), level_iterator(nullptr) {}
};

// MBP-10 snapshot data structure with all 60 required fields
struct MbpSnapshot {
    std::chrono::nanoseconds timestamp;
    uint64_t sequence_number;
    char action;  // The MBO action that generated this snapshot ('A', 'C', 'T', 'R')
    char side;    // The side for this snapshot ('A', 'B', 'N')
    
    // Original event data for CSV output
    double event_price;
    uint64_t event_size;
    uint64_t event_order_id;
    uint8_t event_flags;
    int32_t event_ts_in_delta;
    
    // Bid levels (00-09, highest to lowest price)
    double bid_px_00, bid_px_01, bid_px_02, bid_px_03, bid_px_04;
    double bid_px_05, bid_px_06, bid_px_07, bid_px_08, bid_px_09;
    
    uint64_t bid_sz_00, bid_sz_01, bid_sz_02, bid_sz_03, bid_sz_04;
    uint64_t bid_sz_05, bid_sz_06, bid_sz_07, bid_sz_08, bid_sz_09;
    
    uint32_t bid_ct_00, bid_ct_01, bid_ct_02, bid_ct_03, bid_ct_04;
    uint32_t bid_ct_05, bid_ct_06, bid_ct_07, bid_ct_08, bid_ct_09;
    
    // Ask levels (00-09, lowest to highest price)
    double ask_px_00, ask_px_01, ask_px_02, ask_px_03, ask_px_04;
    double ask_px_05, ask_px_06, ask_px_07, ask_px_08, ask_px_09;
    
    uint64_t ask_sz_00, ask_sz_01, ask_sz_02, ask_sz_03, ask_sz_04;
    uint64_t ask_sz_05, ask_sz_06, ask_sz_07, ask_sz_08, ask_sz_09;
    
    uint32_t ask_ct_00, ask_ct_01, ask_ct_02, ask_ct_03, ask_ct_04;
    uint32_t ask_ct_05, ask_ct_06, ask_ct_07, ask_ct_08, ask_ct_09;
    
    MbpSnapshot() : timestamp(0), sequence_number(0), action('S'), side('N'), 
                    event_price(0.0), event_size(0), event_order_id(0), event_flags(0), event_ts_in_delta(0) {
        // Initialize all bid fields to 0
        bid_px_00 = bid_px_01 = bid_px_02 = bid_px_03 = bid_px_04 = 0.0;
        bid_px_05 = bid_px_06 = bid_px_07 = bid_px_08 = bid_px_09 = 0.0;
        
        bid_sz_00 = bid_sz_01 = bid_sz_02 = bid_sz_03 = bid_sz_04 = 0;
        bid_sz_05 = bid_sz_06 = bid_sz_07 = bid_sz_08 = bid_sz_09 = 0;
        
        bid_ct_00 = bid_ct_01 = bid_ct_02 = bid_ct_03 = bid_ct_04 = 0;
        bid_ct_05 = bid_ct_06 = bid_ct_07 = bid_ct_08 = bid_ct_09 = 0;
        
        // Initialize all ask fields to 0
        ask_px_00 = ask_px_01 = ask_px_02 = ask_px_03 = ask_px_04 = 0.0;
        ask_px_05 = ask_px_06 = ask_px_07 = ask_px_08 = ask_px_09 = 0.0;
        
        ask_sz_00 = ask_sz_01 = ask_sz_02 = ask_sz_03 = ask_sz_04 = 0;
        ask_sz_05 = ask_sz_06 = ask_sz_07 = ask_sz_08 = ask_sz_09 = 0;
        
        ask_ct_00 = ask_ct_01 = ask_ct_02 = ask_ct_03 = ask_ct_04 = 0;
        ask_ct_05 = ask_ct_06 = ask_ct_07 = ask_ct_08 = ask_ct_09 = 0;
    }
};

// High-performance order book implementation
class OrderBook {
public:
    OrderBook();
    
    // The core method to process an MBO event and update the book
    ProcessResult processEvent(const MboEvent& event);
    
    // Generates a market-by-price snapshot of the current order book state
    MbpSnapshot generateSnapshot(const MboEvent& event) const;
    
    // Backward compatibility method for testing (uses dummy event data)
    MbpSnapshot generateSnapshot(char action = 'S', char side = 'N') const;
    
    // Capture current top-10 state for comparison (for snapshot filtering)
    Top10State captureTop10State() const;
    
    // Statistics and debugging
    size_t getBidLevelCount() const { return bid_levels_.size(); }
    size_t getAskLevelCount() const { return ask_levels_.size(); }
    size_t getOrderCount() const { return orders_.size(); }
    
    // Order existence checking
    bool orderExists(uint64_t order_id) const { return orders_.find(order_id) != orders_.end(); }
    
    // Get best bid and ask prices for trade filtering
    std::pair<double, double> getBestBidAsk() const;
    double getBestBidPrice() const;
    double getBestAskPrice() const;
    
    // Trade state access
    bool isInTradeSequence() const { return trade_state_ == TradeState::EXPECTING_FILL; }
    bool wasLastFillFromTrade() const { return last_fill_was_trade_; }
    void resetTradeFlag() { last_fill_was_trade_ = false; }
    
    // Clear all data (for reset events)
    void clear();
    
    // Methods for testing and manual order manipulation
    void addOrder(uint64_t order_id, double price, uint64_t size, char side);
    
    // Methods for standalone trade processing
    bool hasOrdersAtPrice(double price, char side) const;
    void fillOrdersAtPrice(double price, uint64_t size, char side);

private:
    // Bid levels: std::greater for descending price order (highest first)
    using BidLevels = std::map<double, LevelData, std::greater<double>>;
    
    // Ask levels: std::less for ascending price order (lowest first)
    using AskLevels = std::map<double, LevelData, std::less<double>>;
    
    BidLevels bid_levels_;
    AskLevels ask_levels_;
    
    // Fast O(1) order lookup for cancellations
    std::unordered_map<uint64_t, OrderData> orders_;
    
    // Sequence counter for snapshots
    uint64_t sequence_counter_;
    
    // Trade state machine
    enum class TradeState {
        NORMAL,        // Normal processing
        EXPECTING_FILL // Expecting 'F' after 'T'
    };
    
    TradeState trade_state_;
    char pending_trade_side_;  // Side of the pending trade ('B' or 'A')
    char pending_actual_trade_side_;  // Actual side that was affected during fill
    double pending_trade_price_;
    uint64_t pending_trade_size_;
    bool last_fill_was_trade_;  // Track if the last fill was part of a trade sequence
    
    // Event processing methods
    ProcessResult processAddEvent(const MboEvent& event);
    ProcessResult processCancelEvent(const MboEvent& event);
    ProcessResult processTradeEvent(const MboEvent& event);
    ProcessResult processFillEvent(const MboEvent& event);
    ProcessResult processResetEvent(const MboEvent& event);
    
    // Helper methods
    void cancelOrder(uint64_t order_id, uint64_t cancel_size = 0);
    void updateBidLevel(double price, int64_t size_delta, int32_t count_delta, uint64_t order_id = 0);
    void updateAskLevel(double price, int64_t size_delta, int32_t count_delta, uint64_t order_id = 0);
    
    // Trade processing helpers
    void processTradeFill(char trade_side, double price, uint64_t size);
    char getOppositeSide(char side) const;
    void fillOrdersAtLevel(LevelData& level, uint64_t fill_size, char side);
    void updateOrderInQueue(LevelData& level, uint64_t order_id, uint64_t cancel_size);
};
