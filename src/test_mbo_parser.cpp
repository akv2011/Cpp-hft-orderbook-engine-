#include <iostream>
#include <cassert>
#include <fstream>
#include "mbo_parser.h"

// Simple unit test for MBO parser
void testMboParser() {
    std::cout << "Running MBO Parser Unit Tests..." << std::endl;
    
    // Create a test CSV file
    std::string test_filename = "test_mbo.csv";
    std::ofstream test_file(test_filename);
    test_file << "ts_recv,ts_event,rtype,publisher_id,instrument_id,action,side,price,size,channel_id,order_id,flags,ts_in_delta,sequence,symbol\n";
    test_file << "2025-07-17T08:05:03.360842448Z,2025-07-17T08:05:03.360677248Z,160,2,1108,A,B,5.510000000,100,0,817593,130,165200,851012,ARL\n";
    test_file << "2025-07-17T08:05:03.360848793Z,2025-07-17T08:05:03.360683462Z,160,2,1108,A,A,21.330000000,200,0,817597,130,165331,851013,ARL\n";
    test_file << "2025-07-17T08:05:03.361492517Z,2025-07-17T08:05:03.361327319Z,160,2,1108,C,B,5.510000000,50,0,817593,130,165198,851022,ARL\n";
    test_file.close();
    
    // Parse the test file
    std::vector<MboEvent> events = MboParser::parseFile(test_filename);
    
    // Verify results
    assert(events.size() == 3);
    
    // Test first event (Add Bid)
    assert(events[0].action == 'A');
    assert(events[0].side == 'B');
    assert(events[0].price == 5.51);
    assert(events[0].size == 100);
    assert(events[0].order_id == 817593);
    
    // Test second event (Add Ask)
    assert(events[1].action == 'A');
    assert(events[1].side == 'A');
    assert(events[1].price == 21.33);
    assert(events[1].size == 200);
    assert(events[1].order_id == 817597);
    
    // Test third event (Cancel)
    assert(events[2].action == 'C');
    assert(events[2].side == 'B');
    assert(events[2].price == 5.51);
    assert(events[2].size == 50);
    assert(events[2].order_id == 817593);
    
    // Clean up
    std::remove(test_filename.c_str());
    
    std::cout << "✓ All MBO Parser tests passed!" << std::endl;
}

int main() {
    testMboParser();
    return 0;
}
