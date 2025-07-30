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

// Helper function to extract the top 10 levels from bid/ask maps
struct Top10Levels {
    std::vector<std::pair<double, uint64_t>> levels; // price, total_size pairs
    
    bool operator==(const Top10Levels& other) const {
        if (levels.size() != other.levels.size()) return false;
        for (size_t i = 0; i < levels.size(); ++i) {
            if (levels[i].first != other.levels[i].first || 
                levels[i].second != other.levels[i].second) {
                return false;
            }
        }
        return true;
    }
    
    bool operator!=(const Top10Levels& other) const {
        return !(*this == other);
    }
};

Top10Levels getTop10Bids(const OrderBook& book) {
    Top10Levels result;
    result.levels.reserve(10);
    
    // Get the best bid and ask to access internal levels
    auto bid_ask = book.getBestBidAsk();
    
    // Since we can't access private members, we'll extract from snapshot
    MbpSnapshot snapshot = book.generateSnapshot('S', 'N');
    
    // Extract bid levels from snapshot
    const double* bid_prices = &snapshot.bid_px_00;
    const uint64_t* bid_sizes = &snapshot.bid_sz_00;
    
    for (int i = 0; i < 10; ++i) {
        if (bid_prices[i] > 0.0) {  // Valid price level
            result.levels.emplace_back(bid_prices[i], bid_sizes[i]);
        }
    }
    
    return result;
}

Top10Levels getTop10Asks(const OrderBook& book) {
    Top10Levels result;
    result.levels.reserve(10);
    
    // Extract from snapshot since we can't access private members directly
    MbpSnapshot snapshot = book.generateSnapshot('S', 'N');
    
    // Extract ask levels from snapshot
    const double* ask_prices = &snapshot.ask_px_00;
    const uint64_t* ask_sizes = &snapshot.ask_sz_00;
    
    for (int i = 0; i < 10; ++i) {
        if (ask_prices[i] > 0.0) {  // Valid price level
            result.levels.emplace_back(ask_prices[i], ask_sizes[i]);
        }
    }
    
    return result;
}

/**
 * High-Performance Order Book Engine
 * Designed for processing MBO (Market By Order) data streams
 * and generating MBP-10 (Market By Price) snapshots with top-10-only optimization
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
    
    // Process all MBO events with TOP-10-ONLY optimization
    std::cout << "\nProcessing MBO events with top-10-only optimization..." << std::endl;
    auto process_start = std::chrono::high_resolution_clock::now();
    
    size_t processed_events = 0;
    size_t snapshots_written = 0;
    size_t tfc_sequences_detected = 0;
    
    // Variables to track T->F->C sequences
    struct PendingTradeSequence {
        MboEvent trade_event;
        bool expecting_fill = false;
        bool expecting_cancel = false;
    };
    
    PendingTradeSequence pending_sequence;
    
    for (const auto& event : mbo_events) {
        processed_events++;
        
        // Get top 10 levels before processing the event
        Top10Levels bids_before = getTop10Bids(order_book);
        Top10Levels asks_before = getTop10Asks(order_book);
        
        // Process the first R event normally (it should write a snapshot)
        if (event.action == 'R' && processed_events == 1) {
            std::cout << "Processing initial R (reset) event - will write snapshot" << std::endl;
            ProcessResult result = order_book.processEvent(event);
            if (result.should_write) {
                MbpSnapshot snapshot = order_book.generateSnapshot(event);
                if (csv_writer.writeSnapshot(snapshot, snapshots_written)) {
                    snapshots_written++;
                }
            }
            continue;
        }
        
        // Handle T->F->C sequence detection
        if (event.action == 'T') {
            // Start of potential T->F->C sequence
            pending_sequence.trade_event = event;
            pending_sequence.expecting_fill = true;
            pending_sequence.expecting_cancel = false;
            
            // Process the T event
            ProcessResult result = order_book.processEvent(event);
            // Don't check top-10 changes yet - wait to see if this is part of a sequence
            continue;
            
        } else if (event.action == 'F' && pending_sequence.expecting_fill) {
            // Middle of T->F->C sequence
            pending_sequence.expecting_fill = false;
            pending_sequence.expecting_cancel = true;
            
            // Process the F event
            ProcessResult result = order_book.processEvent(event);
            // Don't check top-10 changes yet - wait for complete sequence
            continue;
            
        } else if (event.action == 'C' && pending_sequence.expecting_cancel) {
            // End of T->F->C sequence - this completes the sequence
            pending_sequence.expecting_fill = false;
            pending_sequence.expecting_cancel = false;
            
            // Process the C event
            ProcessResult result = order_book.processEvent(event);
            
            // Get top 10 levels after processing the complete T->F->C sequence
            Top10Levels bids_after = getTop10Bids(order_book);
            Top10Levels asks_after = getTop10Asks(order_book);
            
            // Only write snapshot if top 10 levels changed
            if (bids_before != bids_after || asks_before != asks_after) {
                // Generate snapshot using original trade event as base
                MbpSnapshot snapshot = order_book.generateSnapshot(pending_sequence.trade_event);
                
                // Override the action and side with the corrected values from ProcessResult
                snapshot.action = result.snapshot_action;
                snapshot.side = result.snapshot_side;
                
                if (csv_writer.writeSnapshot(snapshot, snapshots_written)) {
                    snapshots_written++;
                    tfc_sequences_detected++;
                }
            }
            continue;
            
        } else {
            // Handle case where we were expecting part of a T->F->C sequence but got something else
            if (pending_sequence.expecting_fill || pending_sequence.expecting_cancel) {
                // We have a standalone T event that didn't form a T->F->C sequence
                // Check if it changed the top 10 levels
                Top10Levels bids_after_t = getTop10Bids(order_book);
                Top10Levels asks_after_t = getTop10Asks(order_book);
                
                if (bids_before != bids_after_t || asks_before != asks_after_t) {
                    MbpSnapshot t_snapshot = order_book.generateSnapshot(pending_sequence.trade_event);
                    if (csv_writer.writeSnapshot(t_snapshot, snapshots_written)) {
                        snapshots_written++;
                    }
                }
                
                // Reset sequence tracking
                pending_sequence.expecting_fill = false;
                pending_sequence.expecting_cancel = false;
                
                // Update the "before" state for the current event
                bids_before = bids_after_t;
                asks_before = asks_after_t;
            }
            
            // Process current event
            bool should_process = true;
            static std::unordered_set<uint64_t> failed_cancel_orders;
            
            if (event.action == 'C') {
                // Check if the order exists before trying to cancel it
                if (!order_book.orderExists(event.order_id)) {
                    should_process = false;
                    failed_cancel_orders.insert(event.order_id);
                    std::cout << "Filtered Cancel event for non-existent order " << event.order_id << std::endl;
                }
            } else if (event.action == 'A') {
                // Check if this Add follows a failed Cancel for the same order
                if (failed_cancel_orders.count(event.order_id)) {
                    should_process = false;
                    failed_cancel_orders.erase(event.order_id);
                    std::cout << "Filtered Add event for order " << event.order_id << " following failed Cancel" << std::endl;
                }
            }
            
            if (should_process) {
                ProcessResult result = order_book.processEvent(event);
                
                // Get top 10 levels after processing the event
                Top10Levels bids_after = getTop10Bids(order_book);
                Top10Levels asks_after = getTop10Asks(order_book);
                
                // Only write snapshot if the top 10 levels changed AND the event should write
                if (result.should_write && (bids_before != bids_after || asks_before != asks_after)) {
                    MbpSnapshot snapshot = order_book.generateSnapshot(event);
                    if (csv_writer.writeSnapshot(snapshot, snapshots_written)) {
                        snapshots_written++;
                    }
                }
            }
        }
    }
    
    // Handle case where loop ended with a pending T event (standalone T at the end)
    if (pending_sequence.expecting_fill || pending_sequence.expecting_cancel) {
        // Get final state and check if T event changed anything
        Top10Levels bids_final = getTop10Bids(order_book);
        Top10Levels asks_final = getTop10Asks(order_book);
        
        // We need to compare against state before the T event - but we don't have that stored
        // For safety, just write the standalone T event
        MbpSnapshot t_snapshot = order_book.generateSnapshot(pending_sequence.trade_event);
        if (csv_writer.writeSnapshot(t_snapshot, snapshots_written)) {
            snapshots_written++;
        }
    }
    
    auto process_end = std::chrono::high_resolution_clock::now();
    auto process_duration = std::chrono::duration_cast<std::chrono::milliseconds>(process_end - process_start);
    
    // Ensure all data is written to disk
    csv_writer.flush();
    csv_writer.close();
    
    std::cout << "Processed " << processed_events << " events in " << process_duration.count() << " ms" << std::endl;
    std::cout << "Generated and wrote " << snapshots_written << " MBP-10 snapshots to output.csv" << std::endl;
    std::cout << "Detected and consolidated " << tfc_sequences_detected << " T->F->C sequences into T actions" << std::endl;
    std::cout << "TOP-10-ONLY optimization implemented - snapshots written only when top 10 levels change" << std::endl;
    
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
