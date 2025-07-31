#include "event_buffer.h"
#include <algorithm>
#include <sstream>
#include <iostream>

EventBuffer::EventBuffer() : window_timestamp_(0) {
    events_.reserve(100);
    last_stats_ = {0, 0, 0, 0};
}

bool EventBuffer::addEvent(const MboEvent& event) {
    if (isEmpty()) {
        window_timestamp_ = event.ts_event;
        events_.push_back(event);
        return true;
    }
    
    if (belongsToCurrentWindow(event)) {
        events_.push_back(event);
        return true;
    }
    
    return false;
}

std::chrono::nanoseconds EventBuffer::getWindowTimestamp() const {
    return window_timestamp_;
}

bool EventBuffer::isEmpty() const {
    return events_.empty();
}

size_t EventBuffer::size() const {
    return events_.size();
}

size_t EventBuffer::applyOrderAnnihilation() {
    if (events_.empty()) return 0;
    
    std::unordered_map<uint64_t, std::vector<size_t>> add_events;
    std::unordered_map<uint64_t, std::vector<size_t>> cancel_events;
    
    for (size_t i = 0; i < events_.size(); ++i) {
        const auto& event = events_[i];
        if (event.action == 'A') {
            add_events[event.order_id].push_back(i);
        } else if (event.action == 'C') {
            cancel_events[event.order_id].push_back(i);
        }
    }
    
    std::unordered_set<size_t> indices_to_remove;
    size_t pairs_removed = 0;
    
    for (const auto& [order_id, add_indices] : add_events) {
        auto cancel_it = cancel_events.find(order_id);
        if (cancel_it != cancel_events.end()) {
            const auto& cancel_indices = cancel_it->second;
            
            size_t pairs_to_match = std::min(add_indices.size(), cancel_indices.size());
            
            for (size_t i = 0; i < pairs_to_match; ++i) {
                indices_to_remove.insert(add_indices[i]);
                indices_to_remove.insert(cancel_indices[i]);
                pairs_removed++;
            }
        }
    }
    
    std::vector<size_t> sorted_indices(indices_to_remove.begin(), indices_to_remove.end());
    std::sort(sorted_indices.rbegin(), sorted_indices.rend());
    
    for (size_t idx : sorted_indices) {
        events_.erase(events_.begin() + idx);
    }
    
    return pairs_removed;
}

size_t EventBuffer::applySameLevelBatching() {
    if (events_.empty()) return 0;
    
    size_t original_count = events_.size();
    
    std::unordered_map<std::string, std::vector<MboEvent>> grouped_events;
    
    for (const auto& event : events_) {
        if (event.action == 'A' || event.action == 'C') {
            std::ostringstream key_stream;
            key_stream << event.action << "_" << event.side << "_" << std::fixed << event.price;
            std::string key = key_stream.str();
            grouped_events[key].push_back(event);
        } else {
            std::ostringstream key_stream;
            key_stream << "SINGLE_" << event.sequence;
            std::string key = key_stream.str();
            grouped_events[key].push_back(event);
        }
    }
    
    std::vector<MboEvent> consolidated_events;
    consolidated_events.reserve(grouped_events.size());
    
    for (auto& [key, group] : grouped_events) {
        if (group.size() == 1) {
            consolidated_events.push_back(group[0]);
        } else {
            consolidateAtPriceLevel(key, group, consolidated_events);
        }
    }
    
    std::sort(consolidated_events.begin(), consolidated_events.end(), 
              [](const MboEvent& a, const MboEvent& b) {
                  return a.sequence < b.sequence;
              });
    
    events_ = std::move(consolidated_events);
    
    return original_count - events_.size();
}

const std::vector<MboEvent>& EventBuffer::getConsolidatedEvents() const {
    return events_;
}

void EventBuffer::clear() {
    events_.clear();
    window_timestamp_ = std::chrono::nanoseconds(0);
    last_stats_ = {0, 0, 0, 0};
}

EventBuffer::ConsolidationStats EventBuffer::getLastStats() const {
    return last_stats_;
}

bool EventBuffer::belongsToCurrentWindow(const MboEvent& event) const {
    auto time_diff = std::abs((event.ts_event - window_timestamp_).count());
    return time_diff <= WINDOW_THRESHOLD.count();
}

void EventBuffer::consolidateAtPriceLevel(const std::string& key, 
                                        std::vector<MboEvent>& group_events,
                                        std::vector<MboEvent>& consolidated_result) {
    if (group_events.empty()) return;
    
    MboEvent consolidated = group_events[0];
    
    uint64_t total_size = 0;
    for (const auto& event : group_events) {
        total_size += event.size;
    }
    
    consolidated.size = total_size;
    
    uint64_t min_sequence = group_events[0].sequence;
    for (const auto& event : group_events) {
        if (event.sequence < min_sequence) {
            min_sequence = event.sequence;
        }
    }
    consolidated.sequence = min_sequence;
    
    consolidated_result.push_back(consolidated);
}
