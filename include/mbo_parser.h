#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

// High-performance MBO (Market By Order) event structure
struct MboEvent {
    std::chrono::nanoseconds ts_event;  // Event timestamp
    char action;                        // A=Add, C=Cancel, T=Trade, R=Reset
    char side;                          // B=Bid, A=Ask, N=None
    double price;                       // Price level
    uint64_t size;                      // Order size
    uint64_t order_id;                  // Unique order identifier
    
    // Default constructor
    MboEvent() : ts_event(0), action('\0'), side('\0'), price(0.0), size(0), order_id(0) {}
    
    // Parameterized constructor for testing
    MboEvent(std::chrono::nanoseconds ts, char act, char sd, double pr, uint64_t sz, uint64_t oid)
        : ts_event(ts), action(act), side(sd), price(pr), size(sz), order_id(oid) {}
};

// High-performance MBO CSV parser class
class MboParser {
public:
    // Parse CSV file and return vector of MBO events
    static std::vector<MboEvent> parseFile(const std::string& filename);
    
    // Parse single CSV line (for performance-critical parsing)
    static bool parseLine(const char* line, MboEvent& event);
    
private:
    // Fast timestamp parsing from ISO 8601 format
    static std::chrono::nanoseconds parseTimestamp(const char* timestamp_str);
    
    // Fast double parsing
    static double fastParseDouble(const char* str, char** endptr);
    
    // Fast integer parsing
    static uint64_t fastParseUInt64(const char* str, char** endptr);
    
    // Skip to next comma-separated field
    static const char* skipToNextField(const char* ptr);
};
