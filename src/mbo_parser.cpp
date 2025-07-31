#include "mbo_parser.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <memory>

std::vector<MboEvent> MboParser::parseFile(const std::string& filename) {
    std::vector<MboEvent> events;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return events;
    }
    
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    size_t estimated_lines = file_size / 100;
    events.reserve(estimated_lines);
    
    std::string line;
    bool is_header = true;
    
    constexpr size_t BUFFER_SIZE = 65536;
    auto buffer = std::make_unique<char[]>(BUFFER_SIZE);
    file.rdbuf()->pubsetbuf(buffer.get(), BUFFER_SIZE);
    
    while (std::getline(file, line)) {
        if (is_header) {
            is_header = false;
            continue;
        }
        
        if (line.empty()) {
            continue;
        }
        
        MboEvent event;
        if (parseLine(line.c_str(), event)) {
            events.emplace_back(std::move(event));
        }
    }
    
    file.close();
    std::cout << "Parsed " << events.size() << " MBO events from " << filename << std::endl;
    return events;
}

bool MboParser::parseLine(const char* line, MboEvent& event) {
    const char* ptr = line;
    
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    const char* ts_start = ptr;
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    size_t ts_len = ptr - ts_start - 1;
    char ts_buffer[64];
    if (ts_len >= sizeof(ts_buffer)) return false;
    
    std::memcpy(ts_buffer, ts_start, ts_len);
    ts_buffer[ts_len] = '\0';
    event.ts_event = parseTimestamp(ts_buffer);
    
    for (int i = 0; i < 3; ++i) {
        ptr = skipToNextField(ptr);
        if (!ptr) return false;
    }
    
    event.action = *ptr;
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    event.side = *ptr;
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    char* endptr;
    event.price = fastParseDouble(ptr, &endptr);
    if (endptr == ptr) {
        event.price = 0.0;
    }
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    event.size = fastParseUInt64(ptr, &endptr);
    if (endptr == ptr) {
        event.size = 0;
    }
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    event.order_id = fastParseUInt64(ptr, &endptr);
    if (endptr == ptr) {
        event.order_id = 0;
    }
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    event.flags = static_cast<uint8_t>(fastParseUInt64(ptr, &endptr));
    if (endptr == ptr) {
        event.flags = 0;
    }
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    event.ts_in_delta = static_cast<int32_t>(fastParseUInt64(ptr, &endptr));
    if (endptr == ptr) {
        event.ts_in_delta = 0;
    }
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    event.sequence = fastParseUInt64(ptr, &endptr);
    if (endptr == ptr) {
        event.sequence = 0;
    }
    
    return true;
}

std::chrono::nanoseconds MboParser::parseTimestamp(const char* timestamp_str) {
    int year, month, day, hour, minute, second;
    uint64_t nanosec_part = 0;
    
    if (sscanf(timestamp_str, "%d-%d-%dT%d:%d:%d", 
               &year, &month, &day, &hour, &minute, &second) != 6) {
        return std::chrono::nanoseconds(0);
    }
    
    const char* dot_pos = strchr(timestamp_str, '.');
    if (dot_pos) {
        dot_pos++;
        
        char ns_buffer[10] = {0};
        int ns_digits = 0;
        
        while (ns_digits < 9 && dot_pos[ns_digits] >= '0' && dot_pos[ns_digits] <= '9') {
            ns_buffer[ns_digits] = dot_pos[ns_digits];
            ns_digits++;
        }
        
        while (ns_digits < 9) {
            ns_buffer[ns_digits++] = '0';
        }
        
        nanosec_part = strtoull(ns_buffer, nullptr, 10);
    }
    
    static const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    int days_since_epoch = 0;
    
    for (int y = 1970; y < year; y++) {
        bool is_leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
        days_since_epoch += is_leap ? 366 : 365;
    }
    
    bool is_leap_year = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    for (int m = 1; m < month; m++) {
        days_since_epoch += days_in_month[m - 1];
        if (m == 2 && is_leap_year) {
            days_since_epoch += 1;
        }
    }
    
    days_since_epoch += day - 1;
    
    uint64_t total_seconds = static_cast<uint64_t>(days_since_epoch) * 86400ULL + 
                            hour * 3600ULL + minute * 60ULL + second;
    
    uint64_t total_nanoseconds = total_seconds * 1000000000ULL + nanosec_part;
    
    return std::chrono::nanoseconds(total_nanoseconds);
}

double MboParser::fastParseDouble(const char* str, char** endptr) {
    return strtod(str, endptr);
}

uint64_t MboParser::fastParseUInt64(const char* str, char** endptr) {
    return strtoull(str, endptr, 10);
}

const char* MboParser::skipToNextField(const char* ptr) {
    while (*ptr && *ptr != ',') {
        ptr++;
    }
    
    if (*ptr == ',') {
        return ptr + 1;
    }
    
    return nullptr;
}
