#ifndef ORDER_BOOK_H
#define ORDER_BOOK_H

#include <map>
#include <cstdint>

struct PriceLevel {
    int64_t size;
    uint64_t order_id;
};

class OrderBook {
public:
    void add_order(uint64_t order_id, double price, int64_t size, bool is_ask);
    void remove_order(uint64_t order_id, bool is_ask);
    void reset();

    // Make bids and asks public for direct access in main loop
    std::map<double, PriceLevel, std::greater<double>> bids;
    std::map<double, PriceLevel> asks;

private:
    // ...existing private members...
};

#endif // ORDER_BOOK_H