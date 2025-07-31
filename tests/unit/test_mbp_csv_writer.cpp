#include <iostream>
#include <fstream>
#include <cassert>
#include <sstream>
#include <chrono>
#include "../../include/mbp_csv_writer.h"
#include "../../include/order_book.h"
#include "../../include/mbo_parser.h"

void test_csv_writer_initialization() {
    std::cout << "Testing CSV Writer Initialization..." << std::endl;
    
    MbpCsvWriter writer("test_output.csv");
    
    bool init_result = writer.initialize();
    assert(init_result == true);
    
    writer.flush();
    writer.close();
    
    std::ifstream file("test_output.csv");
    assert(file.is_open());
    
    std::string header;
    std::getline(file, header);
    
    std::cout << "Actual header: '" << header << "'" << std::endl;
    
    assert(!header.empty());
    assert(header.find("action") != std::string::npos);
    
    file.close();
    std::remove("test_output.csv");
    
    std::cout << "✅ CSV Writer Initialization test passed!" << std::endl;
}

void test_snapshot_writing() {
    std::cout << "Testing Snapshot Writing..." << std::endl;
    
    MbpCsvWriter writer("test_output.csv");
    
    bool init_result = writer.initialize();
    assert(init_result == true);
    
    OrderBook order_book;
    
    MboEvent bid_event;
    bid_event.ts_event = std::chrono::nanoseconds(1000000000);
    bid_event.action = 'A';
    bid_event.side = 'B';
    bid_event.price = 100.50;
    bid_event.size = 1000;
    bid_event.order_id = 12345;
    bid_event.sequence = 1;
    
    order_book.processEvent(bid_event);
    
    MboEvent ask_event;
    ask_event.ts_event = std::chrono::nanoseconds(1000000001);
    ask_event.action = 'A';
    ask_event.side = 'A';
    ask_event.price = 100.60;
    ask_event.size = 500;
    ask_event.order_id = 12346;
    ask_event.sequence = 2;
    
    order_book.processEvent(ask_event);
    
    MbpSnapshot snapshot = order_book.generateSnapshot(bid_event);
    
    bool result = writer.writeSnapshot(snapshot, 0);
    assert(result == true);
    
    writer.close();
    
    std::ifstream file("test_output.csv");
    std::string line;
    std::getline(file, line);
    std::getline(file, line);
    
    assert(!line.empty());
    assert(line.find("A") != std::string::npos);
    assert(line.find("B") != std::string::npos);
    
    file.close();
    std::remove("test_output.csv");
    
    std::cout << "✅ Snapshot Writing test passed!" << std::endl;
}

void test_performance_bulk_writing() {
    std::cout << "Testing Performance Bulk Writing..." << std::endl;
    
    MbpCsvWriter writer("test_output.csv");
    
    bool init_result = writer.initialize();
    assert(init_result == true);
    
    OrderBook order_book;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; i++) {
        MboEvent event;
        event.ts_event = std::chrono::nanoseconds(1000000000 + i);
        event.action = 'A';
        event.side = 'B';
        event.price = 100.0 + i * 0.01;
        event.size = 1000;
        event.order_id = i + 1000;
        event.sequence = i;
        
        order_book.processEvent(event);
        MbpSnapshot snapshot = order_book.generateSnapshot(event);
        
        writer.writeSnapshot(snapshot, i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    writer.close();
    
    std::cout << "✅ Bulk writing performance: " << duration.count() 
              << " microseconds for 1000 snapshots" << std::endl;
    std::cout << "✅ Performance: " << (1000.0 / duration.count() * 1000000) 
              << " snapshots/second" << std::endl;
    
    std::remove("test_output.csv");
    
    std::cout << "✅ Performance Bulk Writing test passed!" << std::endl;
}

int main() {
    std::cout << "=== MBP CSV Writer Unit Tests ===" << std::endl;
    
    test_csv_writer_initialization();
    test_snapshot_writing();
    test_performance_bulk_writing();
    
    std::cout << "🎉 All MBP CSV Writer tests passed!" << std::endl;
    return 0;
}
