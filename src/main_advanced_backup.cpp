#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <map>
#include <unordered_set>
#include "mbo_parser.h"
#include "order_book.h"
#include "mbp_csv_writer.h"
#include "event_buffer.h"

/**
 * Process a consolidated buffer of events with the existing sophisticated logic
 * for T->F->C sequence detection and Cancel-Add pair detection.
 * This function combines the new event consolidation with the proven pattern detection.
 */
struct BufferProcessingStats {
    size_t processed_events = 0;
    size_t snapshots_written = 0;
    size_t tfc_sequences_detected = 0;
    size_t ca_pairs_detected = 0;
    size_t standalone_trades_filtered = 0;
};

BufferProcessingStats processConsolidatedBuffer(const std::vector<MboEvent>& events,
                                              OrderBook& order_book,
                                              MbpCsvWriter& csv_writer,
                                              size_t& global_snapshots_written) {
    BufferProcessingStats stats;
    
    if (events.empty()) return stats;
    
    // Apply the existing sophisticated T->F->C and Cancel-Add detection logic
    std::vector<std::tuple<const MboEvent*, const MboEvent*, const MboEvent*>> tfc_sequences;
    std::unordered_set<const MboEvent*> consumed_events;
    
    // Find T->F->C sequences: Trade -> Fill (same price/size) -> Cancel (same order as Fill)
    for (const auto& t_event : events) {
        if (t_event.action != 'T' || consumed_events.count(&t_event)) continue;
        
        for (const auto& f_event : events) {
            if (f_event.action != 'F' || consumed_events.count(&f_event)) continue;
            if (f_event.price != t_event.price || f_event.size != t_event.size) continue;
            
            for (const auto& c_event : events) {
                if (c_event.action != 'C' || consumed_events.count(&c_event)) continue;
                if (c_event.order_id != f_event.order_id) continue;
                
                // Found complete T->F->C sequence
                tfc_sequences.push_back(std::make_tuple(&t_event, &f_event, &c_event));
                consumed_events.insert(&t_event);
                consumed_events.insert(&f_event);
                consumed_events.insert(&c_event);
                stats.tfc_sequences_detected++;
                break;
            }
            if (consumed_events.count(&t_event)) break;
        }
    }
    
    // Detect Cancel-Add replacement pairs
    std::vector<std::pair<const MboEvent*, const MboEvent*>> ca_pairs;
    for (const auto& c_event : events) {
        if (c_event.action != 'C' || consumed_events.count(&c_event)) continue;
        
        for (const auto& a_event : events) {
            if (a_event.action != 'A' || consumed_events.count(&a_event)) continue;
            // Match by side (both same side - replace order on same side)
            if (a_event.side != c_event.side) continue;
            
            // Found Cancel-Add replacement pair
            ca_pairs.push_back(std::make_pair(&c_event, &a_event));
            consumed_events.insert(&c_event);
            consumed_events.insert(&a_event);
            stats.ca_pairs_detected++;
            break; // One cancel matches with one add
        }
    }
    
    // Process events in chronological order (by sequence number)
    std::vector<const MboEvent*> sorted_events;
    for (const auto& event : events) {
        sorted_events.push_back(&event);
    }
    std::sort(sorted_events.begin(), sorted_events.end(),
              [](const MboEvent* a, const MboEvent* b) {
                  return a->sequence < b->sequence;
              });
    
    // Process all events
    for (const auto* event_ptr : sorted_events) {
        const auto& event = *event_ptr;
        
        if (consumed_events.count(&event)) {
            // This event is part of a sequence
            if (event.action == 'T') {
                // Handle T->F->C sequence
                for (const auto& [t_ptr, f_ptr, c_ptr] : tfc_sequences) {
                    if (t_ptr == &event) {
                        // Process all three events but only write snapshot for T
                        order_book.processEvent(*t_ptr);
                        order_book.processEvent(*f_ptr);
                        order_book.processEvent(*c_ptr);
                        stats.processed_events += 3;
                        
                        // Generate snapshot using T event - this is the benefit of consolidation!
                        // Multiple events get consolidated into a single snapshot
                        MbpSnapshot snapshot = order_book.generateSnapshot(*t_ptr);
                        if (csv_writer.writeSnapshot(snapshot, global_snapshots_written)) {
                            global_snapshots_written++;
                            stats.snapshots_written++;
                        }
                        break;
                    }
                }
            } else if (event.action == 'C') {
                // Handle Cancel-Add replacement pair
                for (const auto& [c_ptr, a_ptr] : ca_pairs) {
                    if (c_ptr == &event) {
                        // Process both events but only write snapshot for Add (the replacement)
                        order_book.processEvent(*c_ptr);
                        order_book.processEvent(*a_ptr);
                        stats.processed_events += 2;
                        
                        // Generate snapshot using Add event (the final state)
                        MbpSnapshot snapshot = order_book.generateSnapshot(*a_ptr);
                        if (csv_writer.writeSnapshot(snapshot, global_snapshots_written)) {
                            global_snapshots_written++;
                            stats.snapshots_written++;
                        }
                        break;
                    }
                }
            }
            // F events and A events that are part of pairs are skipped (handled by their partners)
        } else {
            // Standalone event - use OrderBook's logic
            ProcessResult result = order_book.processEvent(event);
            stats.processed_events++;
            
            // Special filtering for standalone Trade events
            if (event.action == 'T' && result.should_write) {
                // Get current best bid and ask prices
                std::pair<double, double> bid_ask = order_book.getBestBidAsk();
                double best_bid = bid_ask.first;
                double best_ask = bid_ask.second;
                
                // Apply filtering condition: only process trades that hit the spread
                // Ignore trades that occur between the bid and ask (cross trades, dark pool trades)
                bool trade_hits_spread = (event.price == best_bid) || (event.price == best_ask);
                
                if (trade_hits_spread) {
                    // Trade modifies the visible order book - generate snapshot
                    MbpSnapshot snapshot = order_book.generateSnapshot(event);
                    if (csv_writer.writeSnapshot(snapshot, global_snapshots_written)) {
                        global_snapshots_written++;
                        stats.snapshots_written++;
                    }
                } else {
                    // Trade filtered out - increment counter
                    stats.standalone_trades_filtered++;
                }
            } else if (result.should_write) {
                // Non-trade events or trades that don't pass filtering
                MbpSnapshot snapshot = order_book.generateSnapshot(event);
                if (csv_writer.writeSnapshot(snapshot, global_snapshots_written)) {
                    global_snapshots_written++;
                    stats.snapshots_written++;
                }
            }
        }
    }
    
    return stats;
}

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
    
    if (mbo_events.empty()) {
        std::cerr << "Error: No events parsed from " << input_file << std::endl;
        return 1;
    }
    
    // Initialize order book and CSV writer
    OrderBook order_book;
    MbpCsvWriter csv_writer("output.csv");
    
    if (!csv_writer.initialize()) {
        std::cerr << "Error: Failed to initialize CSV writer" << std::endl;
        return 1;
    }
    
    // Process all MBO events using advanced event consolidation
    std::cout << "\nProcessing MBO events with advanced consolidation..." << std::endl;
    auto process_start = std::chrono::high_resolution_clock::now();
    
    size_t processed_events = 0;
    size_t snapshots_written = 0;
    size_t tfc_sequences_detected = 0;
    size_t ca_pairs_detected = 0;
    size_t standalone_trades_filtered = 0;
    size_t buffers_processed = 0;
    size_t total_consolidation_pairs = 0;
    size_t total_batched_events = 0;
    
    // Initialize event buffer for microsecond-precision consolidation
    EventBuffer event_buffer;
    
    // Process events using the new buffering approach
    for (const auto& event : mbo_events) {
        if (!event_buffer.addEvent(event)) {
            // Buffer is full and needs processing - process current buffer first
            if (!event_buffer.isEmpty()) {
                // Apply consolidation logic
                size_t annihilated_pairs = event_buffer.applyOrderAnnihilation();
                size_t batched_events = event_buffer.applySameLevelBatching();
                
                total_consolidation_pairs += annihilated_pairs;
                total_batched_events += batched_events;
                
                // Process the consolidated buffer
                const auto& consolidated_events = event_buffer.getConsolidatedEvents();
                BufferProcessingStats buffer_stats = processConsolidatedBuffer(
                    consolidated_events, order_book, csv_writer, snapshots_written);
                
                // Accumulate statistics
                processed_events += buffer_stats.processed_events;
                tfc_sequences_detected += buffer_stats.tfc_sequences_detected;
                ca_pairs_detected += buffer_stats.ca_pairs_detected;
                standalone_trades_filtered += buffer_stats.standalone_trades_filtered;
                buffers_processed++;
                
                // Clear buffer for next window
                event_buffer.clear();
            }
            
            // Add the current event to the new buffer window
            event_buffer.addEvent(event);
        }
    }
    
    // Process any remaining events in the final buffer
    if (!event_buffer.isEmpty()) {
        // Apply consolidation logic
        size_t annihilated_pairs = event_buffer.applyOrderAnnihilation();
        size_t batched_events = event_buffer.applySameLevelBatching();
        
        total_consolidation_pairs += annihilated_pairs;
        total_batched_events += batched_events;
        
        // Process the consolidated buffer
        const auto& consolidated_events = event_buffer.getConsolidatedEvents();
        BufferProcessingStats buffer_stats = processConsolidatedBuffer(
            consolidated_events, order_book, csv_writer, snapshots_written);
        
        // Accumulate statistics
        processed_events += buffer_stats.processed_events;
        tfc_sequences_detected += buffer_stats.tfc_sequences_detected;
        ca_pairs_detected += buffer_stats.ca_pairs_detected;
        standalone_trades_filtered += buffer_stats.standalone_trades_filtered;
        buffers_processed++;
    }
    
    auto process_end = std::chrono::high_resolution_clock::now();
    auto process_duration = std::chrono::duration_cast<std::chrono::milliseconds>(process_end - process_start);
    
    // Ensure all data is written to disk
    csv_writer.flush();
    csv_writer.close();
    
    std::cout << "Processed " << processed_events << " events in " << process_duration.count() << " ms" << std::endl;
    std::cout << "Generated and wrote " << snapshots_written << " MBP-10 snapshots to output.csv" << std::endl;
    std::cout << "Processed " << buffers_processed << " time windows with advanced consolidation" << std::endl;
    std::cout << "Detected " << tfc_sequences_detected << " T->F->C sequences" << std::endl;
    std::cout << "Detected " << ca_pairs_detected << " Cancel-Add replacement pairs" << std::endl;
    std::cout << "Filtered " << standalone_trades_filtered << " standalone trades (cross/dark pool trades)" << std::endl;
    std::cout << "Annihilated " << total_consolidation_pairs << " Add/Cancel pairs via order annihilation" << std::endl;
    std::cout << "Batched " << total_batched_events << " events via same-level consolidation" << std::endl;
    
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
