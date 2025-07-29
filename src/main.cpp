#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include "mbo_parser.h"
#include "order_book.h"
#include "mbp_csv_writer.h"

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
    
    // Process all MBO events and generate snapshots after each event
    std::cout << "\nProcessing MBO events and generating snapshots..." << std::endl;
    auto process_start = std::chrono::high_resolution_clock::now();
    
    size_t processed_events = 0;
    size_t snapshots_written = 0;
    
    for (const auto& event : mbo_events) {
        // Process the event
        bool event_processed = order_book.processEvent(event);
        
        if (event_processed) {
            processed_events++;
            
            // Generate snapshot after each event that modifies the book
            // Only generate snapshots for events that actually change the order book state
            if (event.action == 'A' || event.action == 'C' || event.action == 'F' || event.action == 'R') {
                MbpSnapshot snapshot = order_book.generateSnapshot();
                
                // Write snapshot to CSV with proper row indexing
                if (csv_writer.writeSnapshot(snapshot, snapshots_written)) {
                    snapshots_written++;
                } else {
                    std::cerr << "Warning: Failed to write snapshot for event " << processed_events << std::endl;
                }
            }
        }
    }
    
    auto process_end = std::chrono::high_resolution_clock::now();
    auto process_duration = std::chrono::duration_cast<std::chrono::milliseconds>(process_end - process_start);
    
    // Ensure all data is written to disk
    csv_writer.flush();
    csv_writer.close();
    
    std::cout << "Processed " << processed_events << " events in " << process_duration.count() << " ms" << std::endl;
    std::cout << "Generated and wrote " << snapshots_written << " MBP-10 snapshots to output.csv" << std::endl;
    
    // Generate final snapshot for display purposes
    MbpSnapshot final_snapshot = order_book.generateSnapshot();
    
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
