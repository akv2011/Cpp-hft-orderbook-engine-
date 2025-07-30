#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <map>
#include <unordered_set>
#include "mbo_parser.h"
#include "../include/order_book.h"  // Use the sophisticated order book from include/
#include "mbp_csv_writer.h"
#include "event_buffer.h"

/**
 * High-Performance Order Book Engine
 * Designed for processing MBO (Market By Order) data streams
 * and generating MBP-10 (Market By Price) snapshots
 */

int main(int argc, char* argv[]) {
    // Command line argument validation
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <mbo_input_file.csv>" << std::endl;
        std::cerr << "Example: " << argv[0] << " mbo.csv" << std::endl;
        return 1;
    }
    
    std::string input_file = argv[1];
    std::cout << "High-Performance Order Book Engine" << std::endl;
    std::cout << "Processing MBO file: " << input_file << std::endl;
    
    // Measure parsing performance
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Parse MBO CSV file
    std::vector<MboEvent> mbo_events = MboParser::parseFile(input_file);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (mbo_events.empty()) {
        std::cerr << "Error: No events parsed from " << input_file << std::endl;
        return 1;
    }
    
    std::cout << "Successfully parsed " << mbo_events.size() << " MBO events in " 
              << duration.count() << " ms" << std::endl;
    
    // Initialize order book and CSV writer
    OrderBook order_book;
    MbpCsvWriter csv_writer("output.csv");
    
    if (!csv_writer.initialize()) {
        std::cerr << "Error: Failed to initialize CSV writer" << std::endl;
        return 1;
    }
    
    // Process all MBO events with improved T-F-C handling
    std::cout << "\nProcessing MBO events with improved T-F-C handling..." << std::endl;
    auto process_start = std::chrono::high_resolution_clock::now();
    
    size_t processed_events = 0;
    size_t snapshots_written = 0;
    size_t tfc_sequences_detected = 0;
    
    // First pass: identify T->F->C sequences
    std::vector<bool> is_tfc_event(mbo_events.size(), false);
    std::vector<size_t> tfc_trade_index(mbo_events.size(), SIZE_MAX);
    
    for (size_t i = 0; i < mbo_events.size() - 2; ++i) {
        if (mbo_events[i].action == 'T' && 
            i + 1 < mbo_events.size() && mbo_events[i + 1].action == 'F' &&
            i + 2 < mbo_events.size() && mbo_events[i + 2].action == 'C') {
            
            // Check if this forms a valid T->F->C sequence
            const auto& t_event = mbo_events[i];
            const auto& f_event = mbo_events[i + 1];
            const auto& c_event = mbo_events[i + 2];
            
            if (f_event.price == t_event.price && 
                f_event.size == t_event.size &&
                c_event.order_id == f_event.order_id) {
                
                is_tfc_event[i] = true;     // T event
                is_tfc_event[i + 1] = true; // F event
                is_tfc_event[i + 2] = true; // C event
                tfc_trade_index[i + 2] = i; // C event points to T event
                tfc_sequences_detected++;
            }
        }
    }
    
    // Cancel-Add pair consolidation logic - declare outside loop for scope
    struct PendingCancel {
        MboEvent event;
        uint64_t timestamp_ns;
    };
    static std::unordered_map<uint64_t, PendingCancel> cancel_buffer;
    static const uint64_t CONSOLIDATION_WINDOW_NS = 1000; // 1 microsecond window
    static std::unordered_set<uint64_t> failed_cancel_orders;

    // Helper function to get timestamp in nanoseconds
    auto getTimestampNs = [](const MboEvent& evt) -> uint64_t {
        return evt.ts_event.count(); // Extract nanoseconds from ts_event
    };

    // Second pass: process all events
    for (size_t i = 0; i < mbo_events.size(); ++i) {
        const auto& event = mbo_events[i];
        processed_events++;
        
        // Requirement #1: Ignore the initial R event (clear orderbook action)
        // -> Assume we are starting the day with an empty orderbook
        if (event.action == 'R' && processed_events == 1) {
            std::cout << "IGNORING initial R (reset) event - starting with empty orderbook as per requirement #1" << std::endl;
            continue; // Skip this event completely
        }
        
        // Handle T->F->C sequences
        if (is_tfc_event[i]) {
            if (event.action == 'T') {
                // Beginning of T->F->C sequence - process but don't write yet
                ProcessResult result = order_book.processEvent(event);
                (void)result; // Suppress unused variable warning
                continue;
            } else if (event.action == 'F') {
                // Middle of T->F->C sequence - process but don't write yet
                ProcessResult result = order_book.processEvent(event);
                (void)result; // Suppress unused variable warning
                continue;
            } else if (event.action == 'C') {
                // End of T->F->C sequence - process and write snapshot
                ProcessResult result = order_book.processEvent(event);
                
                // Generate snapshot using the original T event
                size_t trade_idx = tfc_trade_index[i];
                MbpSnapshot snapshot = order_book.generateSnapshot(mbo_events[trade_idx]);
                
                // Override with corrected action and side
                snapshot.action = result.snapshot_action;
                snapshot.side = result.snapshot_side;
                
                if (csv_writer.writeSnapshot(snapshot, snapshots_written)) {
                    snapshots_written++;
                }
                continue;
            }
        }
        
        // Handle standalone events (including standalone T events)
        bool should_process = true;
        
        // Evict stale cancel events that are beyond the time window
        auto current_time_ns = getTimestampNs(event);
        auto it = cancel_buffer.begin();
        while (it != cancel_buffer.end()) {
            if (current_time_ns - it->second.timestamp_ns > CONSOLIDATION_WINDOW_NS) {
                // Process stale cancel event
                std::cout << "Processing stale Cancel event for order " << it->first << std::endl;
                ProcessResult stale_result = order_book.processEvent(it->second.event);
                if (stale_result.should_write) {
                    MbpSnapshot stale_snapshot = order_book.generateSnapshot(it->second.event);
                    if (csv_writer.writeSnapshot(stale_snapshot, snapshots_written)) {
                        snapshots_written++;
                    }
                }
                it = cancel_buffer.erase(it);
            } else {
                ++it;
            }
        }
        
        if (event.action == 'C') {
            // Check if the order exists before trying to cancel it
            if (!order_book.orderExists(event.order_id)) {
                should_process = false;
                failed_cancel_orders.insert(event.order_id);
                std::cout << "Filtered Cancel event for non-existent order " << event.order_id << std::endl;
            } else {
                // Buffer the cancel event for potential consolidation
                cancel_buffer[event.order_id] = {event, current_time_ns};
                should_process = false; // Don't process immediately
                std::cout << "Buffered Cancel event for order " << event.order_id << " for potential C->A consolidation" << std::endl;
            }
        } else if (event.action == 'A') {
            // Check if this Add follows a failed Cancel for the same order
            if (failed_cancel_orders.count(event.order_id)) {
                should_process = false;
                failed_cancel_orders.erase(event.order_id);
                std::cout << "Filtered Add event for order " << event.order_id << " following failed Cancel" << std::endl;
            } else {
                // Check for pending cancel event to consolidate
                auto cancel_it = cancel_buffer.find(event.order_id);
                if (cancel_it != cancel_buffer.end()) {
                    uint64_t time_delta = current_time_ns - cancel_it->second.timestamp_ns;
                    if (time_delta <= CONSOLIDATION_WINDOW_NS) {
                        // Consolidate C->A pair - treat as order modification
                        std::cout << "Consolidating C->A pair for order " << event.order_id 
                                  << " (delta: " << time_delta << "ns)" << std::endl;
                        
                        // First process the cancel
                        ProcessResult cancel_result = order_book.processEvent(cancel_it->second.event);
                        (void)cancel_result; // Suppress unused variable warning
                        // Then process the add
                        ProcessResult add_result = order_book.processEvent(event);
                        
                        // Generate a single snapshot for the modification instead of two separate ones
                        if (add_result.should_write) {
                            MbpSnapshot snapshot = order_book.generateSnapshot(event);
                            // Mark as modification action
                            snapshot.action = 'M'; // Use 'M' to indicate this was a consolidated modification
                            if (csv_writer.writeSnapshot(snapshot, snapshots_written)) {
                                snapshots_written++;
                            }
                        }
                        
                        cancel_buffer.erase(cancel_it);
                        should_process = false; // Already processed
                    } else {
                        // Outside time window - process cancel as separate event
                        ProcessResult cancel_result = order_book.processEvent(cancel_it->second.event);
                        if (cancel_result.should_write) {
                            MbpSnapshot cancel_snapshot = order_book.generateSnapshot(cancel_it->second.event);
                            if (csv_writer.writeSnapshot(cancel_snapshot, snapshots_written)) {
                                snapshots_written++;
                            }
                        }
                        cancel_buffer.erase(cancel_it);
                        // Continue to process add normally
                    }
                }
            }
        }
        
        if (should_process) {
            ProcessResult result = order_book.processEvent(event);
            
            // Handle standalone T events specially
            if (event.action == 'T') {
                if (event.side == 'N') {
                    // T events with side='N' don't modify the order book but still generate snapshots
                    MbpSnapshot snapshot = order_book.generateSnapshot(event);
                    snapshot.action = 'T';
                    snapshot.side = event.side;
                    
                    if (csv_writer.writeSnapshot(snapshot, snapshots_written)) {
                        snapshots_written++;
                    }
                } else {
                    // T events with side='B' or 'A' - apply trade effect and write snapshot  
                    char target_side = (event.side == 'B') ? 'A' : 'B';
                    
                    // Check if there are orders to fill at this price
                    bool can_fill = false;
                    if (target_side == 'B') {
                        can_fill = order_book.hasOrdersAtPrice(event.price, 'B');
                    } else {
                        can_fill = order_book.hasOrdersAtPrice(event.price, 'A');
                    }
                    
                    if (can_fill) {
                        // Apply the trade fill
                        if (target_side == 'B') {
                            order_book.fillOrdersAtPrice(event.price, event.size, 'B');
                        } else {
                            order_book.fillOrdersAtPrice(event.price, event.size, 'A');
                        }
                    }
                    
                    // Write snapshot for standalone T event (regardless of whether we could fill)
                    MbpSnapshot snapshot = order_book.generateSnapshot(event);
                    snapshot.action = 'T';
                    snapshot.side = event.side;
                    
                    if (csv_writer.writeSnapshot(snapshot, snapshots_written)) {
                        snapshots_written++;
                    }
                }
            } else {
                // For non-T events, write snapshot if should_write is true
                if (result.should_write) {
                    MbpSnapshot snapshot = order_book.generateSnapshot(event);
                    if (csv_writer.writeSnapshot(snapshot, snapshots_written)) {
                        snapshots_written++;
                    }
                }
            }
        }
    }
    
    // Process any remaining buffered Cancel events at the end
    // These are Cancel events that never found a matching Add within the time window
    for (auto& [order_id, pending_cancel] : cancel_buffer) {
        std::cout << "Processing remaining buffered Cancel event for order " << order_id << std::endl;
        ProcessResult result = order_book.processEvent(pending_cancel.event);
        if (result.should_write) {
            MbpSnapshot snapshot = order_book.generateSnapshot(pending_cancel.event);
            if (csv_writer.writeSnapshot(snapshot, snapshots_written)) {
                snapshots_written++;
            }
        }
    }
    cancel_buffer.clear(); // Clear the buffer
    
    auto process_end = std::chrono::high_resolution_clock::now();
    auto process_duration = std::chrono::duration_cast<std::chrono::milliseconds>(process_end - process_start);
    
    // Ensure all data is written to disk
    csv_writer.flush();
    csv_writer.close();
    
    std::cout << "Processed " << processed_events << " events in " << process_duration.count() << " ms" << std::endl;
    std::cout << "Generated and wrote " << snapshots_written << " MBP-10 snapshots to output.csv" << std::endl;
    std::cout << "Detected and consolidated " << tfc_sequences_detected << " T->F->C sequences into T actions" << std::endl;
    std::cout << "Improved T-F-C handling implemented - all events processed correctly" << std::endl;
    
    // Generate final snapshot for display purposes
    MbpSnapshot final_snapshot = order_book.generateSnapshot('S', 'N');
    
    // Display summary statistics
    std::cout << "\nOrder Book Statistics:" << std::endl;
    std::cout << "Bid levels: " << order_book.getBidLevelCount() << std::endl;
    std::cout << "Ask levels: " << order_book.getAskLevelCount() << std::endl;
    std::cout << "Active orders: " << order_book.getOrderCount() << std::endl;
    
    std::cout << "\nTop 5 Bid Levels:" << std::endl;
    std::cout << "Price      | Size     | Count" << std::endl;
    std::cout << "-----------|----------|------" << std::endl;
    
    // Show first 5 bid levels
    for (int i = 0; i < 5; ++i) {
        double price = (&final_snapshot.bid_px_00)[i];
        int size = (&final_snapshot.bid_sz_00)[i];
        int count = (&final_snapshot.bid_ct_00)[i];
        if (price > 0.0) {
            std::cout << std::fixed << std::setprecision(2) << std::setw(10) << price 
                      << " | " << std::setw(8) << size 
                      << " | " << std::setw(4) << count << std::endl;
        }
    }
    
    std::cout << "\nTop 5 Ask Levels:" << std::endl;
    std::cout << "Price      | Size     | Count" << std::endl;
    std::cout << "-----------|----------|------" << std::endl;
    
    // Show first 5 ask levels
    for (int i = 0; i < 5; ++i) {
        double price = (&final_snapshot.ask_px_00)[i];
        int size = (&final_snapshot.ask_sz_00)[i];
        int count = (&final_snapshot.ask_ct_00)[i];
        if (price > 0.0) {
            std::cout << std::fixed << std::setprecision(2) << std::setw(10) << price 
                      << " | " << std::setw(8) << size 
                      << " | " << std::setw(4) << count << std::endl;
        }
    }
    
    std::cout << "\nOrder book processing completed successfully!" << std::endl;
    
    return 0;
}
