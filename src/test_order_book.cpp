#include <iostream>
#include <cassert>
#include <chrono>
#include "order_book.h"
#include "mbo_parser.h"

void testOrderBookBasics() {
    std::cout << "Testing Order Book Basic Operations..." << std::endl;
    OrderBook book;
    
    // Test initial state
    assert(book.getBidLevelCount() == 0);
    assert(book.getAskLevelCount() == 0);
    assert(book.getOrderCount() == 0);
    
    // Add some orders
    book.addOrder(1001, 100.50, 1000, 'B'); // Bid order
    book.addOrder(1002, 100.75, 500, 'A');  // Ask order
    
    assert(book.getBidLevelCount() == 1);
    assert(book.getAskLevelCount() == 1);
    assert(book.getOrderCount() == 2);
    
    // Test snapshot generation
    MbpSnapshot snapshot = book.generateSnapshot();
    
    // Verify bid level
    assert(snapshot.bid_px_00 == 100.50);
    assert(snapshot.bid_sz_00 == 1000);
    assert(snapshot.bid_ct_00 == 1);
    
    // Verify ask level
    assert(snapshot.ask_px_00 == 100.75);
    assert(snapshot.ask_sz_00 == 500);
    assert(snapshot.ask_ct_00 == 1);
    
    // Verify remaining levels are empty
    for (int i = 1; i < 10; i++) {
        assert((&snapshot.bid_px_00)[i] == 0.0);
        assert((&snapshot.ask_px_00)[i] == 0.0);
        assert((&snapshot.bid_sz_00)[i] == 0);
        assert((&snapshot.ask_sz_00)[i] == 0);
        assert((&snapshot.bid_ct_00)[i] == 0);
        assert((&snapshot.ask_ct_00)[i] == 0);
    }
    
    std::cout << "✓ Basic add operations passed" << std::endl;
}

void testOrderBookLevels() {
    std::cout << "Testing Order Book Price Level Management..." << std::endl;
    OrderBook book;
    
    // Add multiple orders at different price levels
    book.addOrder(1001, 100.50, 1000, 'B');  // Bid
    book.addOrder(1002, 100.25, 500, 'B');   // Lower bid
    book.addOrder(1003, 100.75, 750, 'B');   // Higher bid
    book.addOrder(1004, 100.50, 250, 'B');   // Same price as first bid
    
    book.addOrder(2001, 100.90, 400, 'A');   // Ask
    book.addOrder(2002, 101.25, 600, 'A');   // Higher ask
    book.addOrder(2003, 101.00, 800, 'A');   // Mid ask
    
    assert(book.getBidLevelCount() == 3);
    assert(book.getAskLevelCount() == 3);
    assert(book.getOrderCount() == 7);
    
    // Test snapshot generation
    MbpSnapshot snapshot = book.generateSnapshot();
    
    // Verify bid levels are sorted highest to lowest
    assert(snapshot.bid_px_00 == 100.75); // Highest bid
    assert(snapshot.bid_sz_00 == 750);
    assert(snapshot.bid_ct_00 == 1);
    
    assert(snapshot.bid_px_01 == 100.50); // Second highest
    assert(snapshot.bid_sz_01 == 1250); // 1000 + 250
    assert(snapshot.bid_ct_01 == 2);
    
    assert(snapshot.bid_px_02 == 100.25); // Lowest bid
    assert(snapshot.bid_sz_02 == 500);
    assert(snapshot.bid_ct_02 == 1);
    
    // Verify ask levels are sorted lowest to highest
    assert(snapshot.ask_px_00 == 100.90); // Lowest ask
    assert(snapshot.ask_sz_00 == 400);
    assert(snapshot.ask_ct_00 == 1);
    
    assert(snapshot.ask_px_01 == 101.00); // Second lowest
    assert(snapshot.ask_sz_01 == 800);
    assert(snapshot.ask_ct_01 == 1);
    
    assert(snapshot.ask_px_02 == 101.25); // Highest ask
    assert(snapshot.ask_sz_02 == 600);
    assert(snapshot.ask_ct_02 == 1);
    
    std::cout << "✓ Price level management and sorting passed" << std::endl;
}

void testOrderCancellation() {
    std::cout << "Testing Order Cancellation..." << std::endl;
    OrderBook book;
    
    // Add orders at same price level
    book.addOrder(1001, 100.50, 1000, 'B');
    book.addOrder(1002, 100.50, 500, 'B');
    book.addOrder(2001, 100.75, 750, 'A');
    
    assert(book.getBidLevelCount() == 1);
    assert(book.getAskLevelCount() == 1);
    assert(book.getOrderCount() == 3);
    
    // Test snapshot before cancellation
    MbpSnapshot snapshot1 = book.generateSnapshot();
    assert(snapshot1.bid_px_00 == 100.50);
    assert(snapshot1.bid_sz_00 == 1500); // 1000 + 500
    assert(snapshot1.bid_ct_00 == 2);
    
    // Cancel one order at the bid level
    MboEvent cancel1{std::chrono::nanoseconds(0), 'C', 'B', 100.50, 1000, 1001};
    assert(book.processEvent(cancel1));
    
    assert(book.getBidLevelCount() == 1); // Level still exists
    assert(book.getOrderCount() == 2); // One order removed
    
    // Test snapshot after cancellation
    MbpSnapshot snapshot2 = book.generateSnapshot();
    assert(snapshot2.bid_px_00 == 100.50);
    assert(snapshot2.bid_sz_00 == 500); // Only remaining order
    assert(snapshot2.bid_ct_00 == 1);
    
    // Cancel the last order at this level
    MboEvent cancel2{std::chrono::nanoseconds(0), 'C', 'B', 100.50, 500, 1002};
    assert(book.processEvent(cancel2));
    
    assert(book.getBidLevelCount() == 0); // Level removed
    assert(book.getOrderCount() == 1); // Only ask order remains
    
    // Test final snapshot
    MbpSnapshot snapshot3 = book.generateSnapshot();
    // All bid levels should be empty
    for (int i = 0; i < 10; i++) {
        assert((&snapshot3.bid_px_00)[i] == 0.0);
        assert((&snapshot3.bid_sz_00)[i] == 0);
        assert((&snapshot3.bid_ct_00)[i] == 0);
    }
    // Ask level should still exist
    assert(snapshot3.ask_px_00 == 100.75);
    assert(snapshot3.ask_sz_00 == 750);
    assert(snapshot3.ask_ct_00 == 1);
    
    std::cout << "✓ Order cancellation and level management passed" << std::endl;
}

void testPartialCancellation() {
    std::cout << "Testing Partial Order Cancellation..." << std::endl;
    OrderBook book;
    
    // Add one order
    book.addOrder(1001, 100.50, 1000, 'B');
    
    // Test initial snapshot
    MbpSnapshot snapshot1 = book.generateSnapshot();
    assert(snapshot1.bid_px_00 == 100.50);
    assert(snapshot1.bid_sz_00 == 1000);
    assert(snapshot1.bid_ct_00 == 1);
    
    // Partially cancel the order (cancel 300 out of 1000)
    MboEvent partialCancel1{std::chrono::nanoseconds(0), 'C', 'B', 100.50, 300, 1001};
    assert(book.processEvent(partialCancel1));
    
    assert(book.getBidLevelCount() == 1); // Level still exists
    assert(book.getOrderCount() == 1); // Order still exists
    
    // Test snapshot after partial cancellation
    MbpSnapshot snapshot2 = book.generateSnapshot();
    assert(snapshot2.bid_px_00 == 100.50);
    assert(snapshot2.bid_sz_00 == 700); // 1000 - 300
    assert(snapshot2.bid_ct_00 == 1); // Order still exists
    
    // Another partial cancellation (cancel 200 more)
    MboEvent partialCancel2{std::chrono::nanoseconds(0), 'C', 'B', 100.50, 200, 1001};
    assert(book.processEvent(partialCancel2));
    
    MbpSnapshot snapshot3 = book.generateSnapshot();
    assert(snapshot3.bid_px_00 == 100.50);
    assert(snapshot3.bid_sz_00 == 500); // 700 - 200
    assert(snapshot3.bid_ct_00 == 1);
    
    // Cancel remaining quantity (full cancellation)
    MboEvent fullCancel{std::chrono::nanoseconds(0), 'C', 'B', 100.50, 500, 1001};
    assert(book.processEvent(fullCancel));
    
    assert(book.getBidLevelCount() == 0); // Level removed
    assert(book.getOrderCount() == 0); // Order removed
    
    // Test final snapshot
    MbpSnapshot snapshot4 = book.generateSnapshot();
    for (int i = 0; i < 10; i++) {
        assert((&snapshot4.bid_px_00)[i] == 0.0);
        assert((&snapshot4.bid_sz_00)[i] == 0);
        assert((&snapshot4.bid_ct_00)[i] == 0);
    }
    
    std::cout << "✓ Partial cancellation handling passed" << std::endl;
}

void testOverCancellation() {
    std::cout << "Testing Over-cancellation Protection..." << std::endl;
    OrderBook book;
    
    // Add order
    book.addOrder(1001, 100.50, 500, 'B');
    
    // Try to cancel more than available (over-cancellation)
    MboEvent overCancel{std::chrono::nanoseconds(0), 'C', 'B', 100.50, 1000, 1001}; // Cancel 1000 but only 500 available
    assert(book.processEvent(overCancel)); // Should still succeed
    
    // Order should be fully removed
    assert(book.getBidLevelCount() == 0);
    assert(book.getOrderCount() == 0);
    
    std::cout << "✓ Over-cancellation protection passed" << std::endl;
}

void testCancellationAcrossLevels() {
    std::cout << "Testing Cancellation Across Different Price Levels..." << std::endl;
    OrderBook book;
    
    // Add orders at different price levels
    book.addOrder(1001, 100.75, 1000, 'B'); // Higher bid
    book.addOrder(1002, 100.50, 800, 'B');  // Lower bid
    book.addOrder(2001, 101.00, 600, 'A');  // Lower ask
    book.addOrder(2002, 101.50, 400, 'A');  // Higher ask
    
    // Test initial snapshot
    MbpSnapshot snapshot1 = book.generateSnapshot();
    assert(snapshot1.bid_ct_00 == 1); // 2 bid levels
    assert(snapshot1.ask_ct_00 == 1); // 2 ask levels
    assert(snapshot1.bid_px_00 == 100.75); // Highest bid first
    assert(snapshot1.bid_sz_00 == 1000);
    assert(snapshot1.ask_px_00 == 101.00); // Lowest ask first
    assert(snapshot1.ask_sz_00 == 600);
    
    // Cancel from higher bid level (partial)
    MboEvent cancel1{std::chrono::nanoseconds(0), 'C', 'B', 100.75, 300, 1001};
    assert(book.processEvent(cancel1));
    
    MbpSnapshot snapshot2 = book.generateSnapshot();
    assert(snapshot2.bid_px_00 == 100.75);
    assert(snapshot2.bid_sz_00 == 700); // Reduced from 1000
    assert(snapshot2.bid_px_01 == 100.50);
    assert(snapshot2.bid_sz_01 == 800); // Unchanged
    assert(snapshot2.ask_px_00 == 101.00);
    assert(snapshot2.ask_sz_00 == 600); // Unchanged
    assert(snapshot2.ask_px_01 == 101.50);
    assert(snapshot2.ask_sz_01 == 400); // Unchanged
    
    // Cancel entire higher ask level
    MboEvent cancel2{std::chrono::nanoseconds(0), 'C', 'A', 101.50, 400, 2002};
    assert(book.processEvent(cancel2));
    
    assert(book.getBidLevelCount() == 2); // Still two bid levels
    assert(book.getAskLevelCount() == 1); // Only one ask level remains
    
    MbpSnapshot snapshot3 = book.generateSnapshot();
    assert(snapshot3.ask_px_00 == 101.00);
    assert(snapshot3.ask_sz_00 == 600);
    // Second ask level should be empty
    assert(snapshot3.ask_px_01 == 0.0);
    assert(snapshot3.ask_sz_01 == 0);
    assert(snapshot3.ask_ct_01 == 0);
    
    std::cout << "✓ Cross-level cancellation isolation passed" << std::endl;
}

void testMultiOrderLevelCancellation() {
    std::cout << "Testing Multi-Order Level Cancellation..." << std::endl;
    OrderBook book;
    
    // Add multiple orders at the same price level
    book.addOrder(1001, 100.50, 1000, 'B');
    book.addOrder(1002, 100.50, 750, 'B');
    book.addOrder(1003, 100.50, 500, 'B');
    
    // Test initial snapshot
    MbpSnapshot snapshot1 = book.generateSnapshot();
    assert(snapshot1.bid_px_00 == 100.50);
    assert(snapshot1.bid_sz_00 == 2250); // 1000 + 750 + 500
    assert(snapshot1.bid_ct_00 == 3);
    
    // Partially cancel first order
    MboEvent cancel1{std::chrono::nanoseconds(0), 'C', 'B', 100.50, 250, 1001};
    assert(book.processEvent(cancel1));
    
    MbpSnapshot snapshot2 = book.generateSnapshot();
    assert(snapshot2.bid_px_00 == 100.50);
    assert(snapshot2.bid_sz_00 == 2000); // 2250 - 250
    assert(snapshot2.bid_ct_00 == 3);
    
    // Fully cancel second order
    MboEvent cancel2{std::chrono::nanoseconds(0), 'C', 'B', 100.50, 750, 1002};
    assert(book.processEvent(cancel2));
    
    MbpSnapshot snapshot3 = book.generateSnapshot();
    assert(snapshot3.bid_px_00 == 100.50);
    assert(snapshot3.bid_sz_00 == 1250); // 750 (from 1001 after partial cancel) + 500 (from 1003)
    assert(snapshot3.bid_ct_00 == 2);
    
    // Partially cancel first order again
    MboEvent cancel3{std::chrono::nanoseconds(0), 'C', 'B', 100.50, 250, 1001};
    assert(book.processEvent(cancel3));
    
    MbpSnapshot snapshot4 = book.generateSnapshot();
    assert(snapshot4.bid_px_00 == 100.50);
    assert(snapshot4.bid_sz_00 == 1000); // 500 (remaining from 1001) + 500 (from 1003)
    assert(snapshot4.bid_ct_00 == 2);
    
    std::cout << "✓ Multi-order level cancellation passed" << std::endl;
}

void testTradeEventHandling() {
    std::cout << "Testing Trade Event Handling (T-F-C sequence)..." << std::endl;
    OrderBook book;
    
    // Add orders
    book.addOrder(1001, 100.50, 100, 'B');
    book.addOrder(1002, 100.50, 50, 'B');
    book.addOrder(2001, 100.75, 75, 'A');
    book.addOrder(2002, 100.75, 25, 'A');
    
    // Test T-F-C sequence (Trade-Fill-Cancel)
    MboEvent trade{std::chrono::nanoseconds(0), 'T', 'A', 100.50, 30, 0};
    assert(book.processEvent(trade));
    
    MbpSnapshot snapshot1 = book.generateSnapshot();
    assert(snapshot1.bid_px_00 == 100.50);
    assert(snapshot1.bid_sz_00 == 150); // 100 + 50
    assert(snapshot1.bid_ct_00 == 2);
    assert(snapshot1.ask_px_00 == 100.75);
    assert(snapshot1.ask_sz_00 == 100); // 75 + 25
    assert(snapshot1.ask_ct_00 == 2);
    
    // Fill event should reduce ask side (opposite of trade side)
    MboEvent fill{std::chrono::nanoseconds(0), 'F', 'A', 100.75, 30, 2001};
    assert(book.processEvent(fill));
    
    MbpSnapshot snapshot2 = book.generateSnapshot();
    assert(snapshot2.bid_px_00 == 100.50);
    assert(snapshot2.bid_sz_00 == 150); // Bids unchanged
    assert(snapshot2.ask_px_00 == 100.75);
    assert(snapshot2.ask_sz_00 == 70);  // 100 - 30 = 70 (45 + 25)
    assert(snapshot2.ask_ct_00 == 2);  // Both orders still exist
    
    std::cout << "✓ Basic trade event handling passed" << std::endl;
}

void testTradeEventFIFO() {
    std::cout << "Testing Trade Event FIFO Policy..." << std::endl;
    OrderBook book;
    
    // Add multiple orders at same price level (FIFO order)
    book.addOrder(2001, 100.75, 20, 'A'); // First in queue
    book.addOrder(2002, 100.75, 30, 'A'); // Second in queue
    book.addOrder(2003, 100.75, 40, 'A'); // Third in queue
    
    MbpSnapshot snapshot1 = book.generateSnapshot();
    assert(snapshot1.ask_px_00 == 100.75);
    assert(snapshot1.ask_sz_00 == 90); // 20 + 30 + 40
    
    // Trade and fill that should affect multiple orders in FIFO order
    MboEvent trade{std::chrono::nanoseconds(0), 'T', 'B', 100.75, 35, 0}; // Trade from bid side
    assert(book.processEvent(trade));
    
    MboEvent fill{std::chrono::nanoseconds(0), 'F', 'A', 100.75, 35, 2001}; // Fill ask side
    assert(book.processEvent(fill));
    
    // Should fill first order completely (20) and second order partially (15)
    MbpSnapshot snapshot2 = book.generateSnapshot();
    assert(snapshot2.ask_px_00 == 100.75);
    assert(snapshot2.ask_sz_00 == 55); // 90 - 35 = 55
    assert(snapshot2.ask_ct_00 == 2); // One order removed
    
    std::cout << "✓ Trade FIFO policy passed" << std::endl;
}

void testTradeEventIgnoreSideN() {
    std::cout << "Testing Trade Event Side 'N' Ignored..." << std::endl;
    OrderBook book;
    
    // Add orders
    book.addOrder(1001, 100.50, 100, 'B');
    book.addOrder(2001, 100.75, 100, 'A');
    
    MbpSnapshot snapshot1 = book.generateSnapshot();
    uint64_t initial_bid_size = snapshot1.bid_sz_00;
    uint64_t initial_ask_size = snapshot1.ask_sz_00;
    
    // Trade with side 'N' should be ignored
    MboEvent trade{std::chrono::nanoseconds(0), 'T', 'N', 100.62, 50, 0};
    assert(book.processEvent(trade)); // Should return true but do nothing
    
    MbpSnapshot snapshot2 = book.generateSnapshot();
    assert(snapshot2.bid_sz_00 == initial_bid_size);
    assert(snapshot2.ask_sz_00 == initial_ask_size);
    
    std::cout << "✓ Trade side 'N' ignored correctly" << std::endl;
}

void testTradeEventOppositeSideLogic() {
    std::cout << "Testing Trade Event Opposite Side Logic..." << std::endl;
    OrderBook book;
    
    // Add orders
    book.addOrder(1001, 100.50, 100, 'B');
    book.addOrder(2001, 100.75, 100, 'A');
    
    // Trade from ask side (A) should affect bid side orders when filled
    MboEvent trade1{std::chrono::nanoseconds(0), 'T', 'A', 100.50, 25, 0};
    assert(book.processEvent(trade1));
    
    MboEvent fill1{std::chrono::nanoseconds(0), 'F', 'A', 100.75, 25, 2001};
    assert(book.processEvent(fill1));
    
    MbpSnapshot snapshot1 = book.generateSnapshot();
    assert(snapshot1.bid_px_00 == 100.50);
    assert(snapshot1.bid_sz_00 == 100); // Bid unchanged
    assert(snapshot1.ask_px_00 == 100.75);
    assert(snapshot1.ask_sz_00 == 75);  // Ask reduced by 25
    
    // Trade from bid side (B) should affect ask side orders when filled
    MboEvent trade2{std::chrono::nanoseconds(0), 'T', 'B', 100.75, 30, 0};
    assert(book.processEvent(trade2));
    
    MboEvent fill2{std::chrono::nanoseconds(0), 'F', 'B', 100.50, 30, 1001};
    assert(book.processEvent(fill2));
    
    MbpSnapshot snapshot2 = book.generateSnapshot();
    assert(snapshot2.bid_px_00 == 100.50);
    assert(snapshot2.bid_sz_00 == 70);  // Bid reduced by 30
    assert(snapshot2.ask_px_00 == 100.75);
    assert(snapshot2.ask_sz_00 == 75);  // Ask unchanged from previous
    
    std::cout << "✓ Opposite side logic passed" << std::endl;
}

void testResetEvent() {
    std::cout << "Testing Reset Event..." << std::endl;
    OrderBook book;
    
    // Add multiple orders
    book.addOrder(1001, 100.50, 1000, 'B');
    book.addOrder(1002, 100.25, 500, 'B');
    book.addOrder(2001, 100.75, 750, 'A');
    book.addOrder(2002, 101.00, 600, 'A');
    
    assert(book.getBidLevelCount() == 2);
    assert(book.getAskLevelCount() == 2);
    assert(book.getOrderCount() == 4);
    
    // Process reset event
    MboEvent reset{std::chrono::nanoseconds(0), 'R', 'N', 0.0, 0, 0};
    assert(book.processEvent(reset));
    
    // Book should be completely empty
    assert(book.getBidLevelCount() == 0);
    assert(book.getAskLevelCount() == 0);
    assert(book.getOrderCount() == 0);
    
    MbpSnapshot snapshot = book.generateSnapshot();
    
    // All price, size, and count fields should be 0
    for (int i = 0; i < 10; i++) {
        assert((&snapshot.bid_px_00)[i] == 0.0);
        assert((&snapshot.ask_px_00)[i] == 0.0);
        assert((&snapshot.bid_sz_00)[i] == 0);
        assert((&snapshot.ask_sz_00)[i] == 0);
        assert((&snapshot.bid_ct_00)[i] == 0);
        assert((&snapshot.ask_ct_00)[i] == 0);
    }
    
    std::cout << "✓ Reset event handling passed" << std::endl;
}

void testMbpSnapshotGeneration() {
    std::cout << "Testing MBP-10 Snapshot Generation..." << std::endl;
    OrderBook book;
    
    // Test with exactly 10 levels on each side
    for (int i = 0; i < 10; i++) {
        // Add bid levels from 100.10 down to 100.01
        book.addOrder(1000 + i, 100.10 - i * 0.01, 100 + i * 10, 'B');
        // Add ask levels from 100.20 up to 100.29
        book.addOrder(2000 + i, 100.20 + i * 0.01, 200 + i * 10, 'A');
    }
    
    MbpSnapshot snapshot = book.generateSnapshot();
    
    // Verify all 10 bid levels (sorted highest to lowest)
    for (int i = 0; i < 10; i++) {
        double expected_price = 100.10 - i * 0.01;
        uint64_t expected_size = 100 + i * 10;
        assert(std::abs((&snapshot.bid_px_00)[i] - expected_price) < 0.001);
        assert((&snapshot.bid_sz_00)[i] == expected_size);
        assert((&snapshot.bid_ct_00)[i] == 1);
    }
    
    // Verify all 10 ask levels (sorted lowest to highest)
    for (int i = 0; i < 10; i++) {
        double expected_price = 100.20 + i * 0.01;
        uint64_t expected_size = 200 + i * 10;
        assert(std::abs((&snapshot.ask_px_00)[i] - expected_price) < 0.001);
        assert((&snapshot.ask_sz_00)[i] == expected_size);
        assert((&snapshot.ask_ct_00)[i] == 1);
    }
    
    std::cout << "✓ 10-level snapshot generation passed" << std::endl;
    
    // Test with more than 10 levels (should only show top 10)
    book.clear();
    for (int i = 0; i < 15; i++) {
        book.addOrder(3000 + i, 95.00 + i * 0.50, 50, 'B');
        book.addOrder(4000 + i, 105.00 + i * 0.25, 75, 'A');
    }
    
    snapshot = book.generateSnapshot();
    
    // Should show top 10 bids (highest prices)
    for (int i = 0; i < 10; i++) {
        double expected_price = 102.00 - i * 0.50; // Top 10 from 102.00 down
        assert(std::abs((&snapshot.bid_px_00)[i] - expected_price) < 0.001);
        assert((&snapshot.bid_sz_00)[i] == 50);
        assert((&snapshot.bid_ct_00)[i] == 1);
    }
    
    // Should show top 10 asks (lowest prices)
    for (int i = 0; i < 10; i++) {
        double expected_price = 105.00 + i * 0.25; // Bottom 10 from 105.00 up
        assert(std::abs((&snapshot.ask_px_00)[i] - expected_price) < 0.001);
        assert((&snapshot.ask_sz_00)[i] == 75);
        assert((&snapshot.ask_ct_00)[i] == 1);
    }
    
    std::cout << "✓ >10 level truncation passed" << std::endl;
    
    // Test with fewer than 10 levels
    book.clear();
    book.addOrder(5001, 99.50, 300, 'B');
    book.addOrder(5002, 99.25, 400, 'B');
    book.addOrder(5003, 100.75, 200, 'A');
    
    snapshot = book.generateSnapshot();
    
    // First 2 bid levels should have data
    assert(snapshot.bid_px_00 == 99.50);
    assert(snapshot.bid_sz_00 == 300);
    assert(snapshot.bid_ct_00 == 1);
    
    assert(snapshot.bid_px_01 == 99.25);
    assert(snapshot.bid_sz_01 == 400);
    assert(snapshot.bid_ct_01 == 1);
    
    // Remaining bid levels should be 0
    for (int i = 2; i < 10; i++) {
        assert((&snapshot.bid_px_00)[i] == 0.0);
        assert((&snapshot.bid_sz_00)[i] == 0);
        assert((&snapshot.bid_ct_00)[i] == 0);
    }
    
    // First ask level should have data
    assert(snapshot.ask_px_00 == 100.75);
    assert(snapshot.ask_sz_00 == 200);
    assert(snapshot.ask_ct_00 == 1);
    
    // Remaining ask levels should be 0
    for (int i = 1; i < 10; i++) {
        assert((&snapshot.ask_px_00)[i] == 0.0);
        assert((&snapshot.ask_sz_00)[i] == 0);
        assert((&snapshot.ask_ct_00)[i] == 0);
    }
    
    std::cout << "✓ <10 level padding passed" << std::endl;
    std::cout << "✓ MBP-10 snapshot generation tests passed" << std::endl;
}

int main() {
    testOrderBookBasics();
    testOrderBookLevels();
    testOrderCancellation();
    testPartialCancellation();
    testOverCancellation();
    testCancellationAcrossLevels();
    testMultiOrderLevelCancellation();
    testTradeEventHandling();
    testTradeEventFIFO();
    testTradeEventIgnoreSideN();
    testTradeEventOppositeSideLogic();
    testResetEvent();
    testMbpSnapshotGeneration();
    
    std::cout << "\n✅ All Order Book tests passed!" << std::endl;
    return 0;
}
