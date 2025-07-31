#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

// MBO event structure
struct MboEvent {
    std::chrono::nanoseconds ts_event;
    char action;
    char side;
    double price;
    uint64_t size;
    uint64_t order_id;
    uint8_t flags;
    int32_t ts_in_delta;
    uint64_t sequence;
    
    MboEvent() : ts_event(0), action('\0'), side('\0'), price(0.0), size(0), order_id(0), flags(0), ts_in_delta(0), sequence(0) {}
    
    MboEvent(std::chrono::nanoseconds ts, char act, char sd, double pr, uint64_t sz, uint64_t oid)
        : ts_event(ts), action(act), side(sd), price(pr), size(sz), order_id(oid), flags(0), ts_in_delta(0), sequence(0) {}
};

// High-performance MBO CSV parser
class MboParser {
public:
    static std::vector<MboEvent> parseFile(const std::string& filename);
    static bool parseLine(const char* line, MboEvent& event);
    
private:
    static std::chrono::nanoseconds parseTimestamp(const char* timestamp_str);
    static double fastParseDouble(const char* str, char** endptr);
    static uint64_t fastParseUInt64(const char* str, char** endptr);
    static const char* skipToNextField(const char* ptr);
};
