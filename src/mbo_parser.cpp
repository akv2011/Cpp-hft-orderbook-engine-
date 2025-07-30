#include "mbo_parser.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <memory>

// Parse CSV file and return vector of MBO events
std::vector<MboEvent> MboParser::parseFile(const std::string& filename) {
    std::vector<MboEvent> events;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return events;
    }
    
    // Reserve space for performance (estimate based on file size)
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Estimate ~100 bytes per line, reserve space accordingly
    size_t estimated_lines = file_size / 100;
    events.reserve(estimated_lines);
    
    std::string line;
    bool is_header = true;
    
    // Use a larger buffer for better I/O performance
    constexpr size_t BUFFER_SIZE = 65536;
    auto buffer = std::make_unique<char[]>(BUFFER_SIZE);
    file.rdbuf()->pubsetbuf(buffer.get(), BUFFER_SIZE);
    
    while (std::getline(file, line)) {
        // Skip header row
        if (is_header) {
            is_header = false;
            continue;
        }
        
        // Skip empty lines
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

// Parse single CSV line using high-performance C-style parsing
bool MboParser::parseLine(const char* line, MboEvent& event) {
    const char* ptr = line;
    
    // Skip ts_recv (first field) - we don't need it
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    // Parse ts_event (second field)
    const char* ts_start = ptr;
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    // Extract timestamp string (null-terminate temporarily)
    size_t ts_len = ptr - ts_start - 1; // -1 for comma
    char ts_buffer[64];
    if (ts_len >= sizeof(ts_buffer)) return false;
    
    std::memcpy(ts_buffer, ts_start, ts_len);
    ts_buffer[ts_len] = '\0';
    event.ts_event = parseTimestamp(ts_buffer);
    
    // Skip rtype, publisher_id, instrument_id (fields 3, 4, 5)
    for (int i = 0; i < 3; ++i) {
        ptr = skipToNextField(ptr);
        if (!ptr) return false;
    }
    
    // Parse action (field 6)
    event.action = *ptr;
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    // Parse side (field 7)
    event.side = *ptr;
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    // Parse price (field 8)
    char* endptr;
    event.price = fastParseDouble(ptr, &endptr);
    if (endptr == ptr) {
        // Empty price field, set to 0
        event.price = 0.0;
    }
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    // Parse size (field 9)
    event.size = fastParseUInt64(ptr, &endptr);
    if (endptr == ptr) {
        event.size = 0;
    }
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    // Skip channel_id (field 10)
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    // Parse order_id (field 11)
    event.order_id = fastParseUInt64(ptr, &endptr);
    if (endptr == ptr) {
        event.order_id = 0;
    }
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    // Parse flags (field 12)
    event.flags = static_cast<uint8_t>(fastParseUInt64(ptr, &endptr));
    if (endptr == ptr) {
        event.flags = 0;
    }
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    // Parse ts_in_delta (field 13)
    event.ts_in_delta = static_cast<int32_t>(fastParseUInt64(ptr, &endptr));
    if (endptr == ptr) {
        event.ts_in_delta = 0;
    }
    ptr = skipToNextField(ptr);
    if (!ptr) return false;
    
    // Parse sequence (field 14)
    event.sequence = fastParseUInt64(ptr, &endptr);
    if (endptr == ptr) {
        event.sequence = 0;
    }
    
    return true;
}

// Fast timestamp parsing from ISO 8601 format: 2025-07-17T08:05:03.360677248Z
std::chrono::nanoseconds MboParser::parseTimestamp(const char* timestamp_str) {
    // Parse: YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ
    int year, month, day, hour, minute, second;
    uint64_t nanosec_part = 0;
    
    // Parse the main timestamp components
    if (sscanf(timestamp_str, "%d-%d-%dT%d:%d:%d", 
               &year, &month, &day, &hour, &minute, &second) != 6) {
        return std::chrono::nanoseconds(0);
    }
    
    // Find the decimal point for nanoseconds
    const char* dot_pos = strchr(timestamp_str, '.');
    if (dot_pos) {
        dot_pos++; // Move past the dot
        
        // Parse nanoseconds (up to 9 digits)
        char ns_buffer[10] = {0}; // 9 digits + null terminator
        int ns_digits = 0;
        
        while (ns_digits < 9 && dot_pos[ns_digits] >= '0' && dot_pos[ns_digits] <= '9') {
            ns_buffer[ns_digits] = dot_pos[ns_digits];
            ns_digits++;
        }
        
        // Pad with zeros if less than 9 digits
        while (ns_digits < 9) {
            ns_buffer[ns_digits++] = '0';
        }
        
        nanosec_part = strtoull(ns_buffer, nullptr, 10);
    }
    
    // Convert to time_point (simplified calculation)
    // This is a simplified conversion for demonstration
    // In production, you'd want to use a proper date/time library
    
    // Calculate days since epoch (approximate for demonstration)
    int days_since_epoch = (year - 1970) * 365 + (year - 1969) / 4 - (year - 1901) / 100 + (year - 1601) / 400;
    days_since_epoch += (month - 1) * 30 + day - 1; // Rough approximation
    
    uint64_t total_seconds = static_cast<uint64_t>(days_since_epoch) * 86400ULL + 
                            hour * 3600ULL + minute * 60ULL + second;
    
    uint64_t total_nanoseconds = total_seconds * 1000000000ULL + nanosec_part;
    
    return std::chrono::nanoseconds(total_nanoseconds);
}

// Fast double parsing optimized for price fields
double MboParser::fastParseDouble(const char* str, char** endptr) {
    // Use standard library but with some optimizations
    return strtod(str, endptr);
}

// Fast integer parsing optimized for size and order_id fields
uint64_t MboParser::fastParseUInt64(const char* str, char** endptr) {
    return strtoull(str, endptr, 10);
}

// Skip to next comma-separated field
const char* MboParser::skipToNextField(const char* ptr) {
    while (*ptr && *ptr != ',') {
        ptr++;
    }
    
    if (*ptr == ',') {
        return ptr + 1; // Skip the comma
    }
    
    return nullptr; // End of line or error
}
