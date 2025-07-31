#pragma once

#include "mbo_parser.h"
#include <vector>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

// Event buffer for consolidating high-frequency trading events
class EventBuffer {
public:
    EventBuffer();
    
    bool addEvent(const MboEvent& event);
    std::chrono::nanoseconds getWindowTimestamp() const;
    bool isEmpty() const;
    size_t size() const;
    
    size_t applyOrderAnnihilation();
    size_t applySameLevelBatching();
    
    const std::vector<MboEvent>& getConsolidatedEvents() const;
    void clear();
    
    struct ConsolidationStats {
        size_t original_count;
        size_t annihilated_pairs;
        size_t batched_events;
        size_t final_count;
    };
    
    ConsolidationStats getLastStats() const;

private:
    static constexpr std::chrono::nanoseconds WINDOW_THRESHOLD = std::chrono::milliseconds(1);
    
    std::vector<MboEvent> events_;
    std::chrono::nanoseconds window_timestamp_;
    ConsolidationStats last_stats_;
    
    bool belongsToCurrentWindow(const MboEvent& event) const;
    void consolidateAtPriceLevel(const std::string& key, 
                                std::vector<MboEvent>& group_events,
                                std::vector<MboEvent>& consolidated_result);
};
