#pragma once

#include "mbo_parser.h"
#include <vector>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

/**
 * High-performance event buffer for consolidating high-frequency trading events
 * within microsecond-precision time windows. This buffer implements sophisticated
 * consolidation logic to reduce snapshot generation by eliminating redundant
 * micro-events that don't represent meaningful order book changes.
 */
class EventBuffer {
public:
    /**
     * Constructor initializes an empty buffer
     */
    EventBuffer();
    
    /**
     * Add an event to the buffer. The buffer will determine if this event
     * belongs to the current time window or if the current buffer should be
     * processed first.
     * 
     * @param event The MBO event to add
     * @return true if event was added to current buffer, false if buffer needs processing first
     */
    bool addEvent(const MboEvent& event);
    
    /**
     * Get the current buffer window's timestamp (timestamp of first event in buffer)
     * @return nanosecond timestamp defining the current window
     */
    std::chrono::nanoseconds getWindowTimestamp() const;
    
    /**
     * Check if the buffer is empty
     * @return true if buffer contains no events
     */
    bool isEmpty() const;
    
    /**
     * Get the number of events currently in the buffer
     * @return count of events in buffer
     */
    size_t size() const;
    
    /**
     * Apply order annihilation logic: remove Add/Cancel pairs with same order_id
     * This eliminates short-lived orders that should not generate snapshots
     * @return number of event pairs removed
     */
    size_t applyOrderAnnihilation();
    
    /**
     * Apply same-level batching logic: consolidate multiple events at same price/side
     * This handles patterns like A,A,A,A,A,A,A by batching them into single events
     * @return number of events consolidated (original count - final count)
     */
    size_t applySameLevelBatching();
    
    /**
     * Get the final consolidated events ready for order book processing
     * @return vector of consolidated events in chronological order
     */
    const std::vector<MboEvent>& getConsolidatedEvents() const;
    
    /**
     * Clear the buffer and prepare for next time window
     */
    void clear();
    
    /**
     * Get statistics about the last consolidation operation
     */
    struct ConsolidationStats {
        size_t original_count;
        size_t annihilated_pairs;
        size_t batched_events;
        size_t final_count;
    };
    
    ConsolidationStats getLastStats() const;

private:
    // Time window configuration
    static constexpr std::chrono::nanoseconds WINDOW_THRESHOLD = std::chrono::milliseconds(1);
    
    // Current buffer of events
    std::vector<MboEvent> events_;
    
    // Timestamp of the current window (timestamp of first event)
    std::chrono::nanoseconds window_timestamp_;
    
    // Statistics from last consolidation
    ConsolidationStats last_stats_;
    
    // Helper function to determine if an event belongs to current window
    bool belongsToCurrentWindow(const MboEvent& event) const;
    
    // Helper function to consolidate events at same price/side
    void consolidateAtPriceLevel(const std::string& key, 
                                std::vector<MboEvent>& group_events,
                                std::vector<MboEvent>& consolidated_result);
};
