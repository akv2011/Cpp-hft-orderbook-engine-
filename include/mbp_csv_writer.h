#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include "order_book.h"

/**
 * High-performance CSV writer for MBP-10 snapshots
 * Writes formatted snapshots to output.csv file matching the exact format
 * of the sample mbp.csv file
 */
class MbpCsvWriter {
public:
    // Constructor
    explicit MbpCsvWriter(const std::string& filename = "output.csv");
    
    // Destructor - ensures file is properly closed
    ~MbpCsvWriter();
    
    // Initialize writer and write CSV header
    bool initialize();
    
    // Write a single MBP snapshot to the CSV file
    bool writeSnapshot(const MbpSnapshot& snapshot, uint64_t row_index = 0);
    
    // Flush all buffered data to disk
    void flush();
    
    // Close the file explicitly
    void close();
    
    // Get number of snapshots written
    size_t getSnapshotCount() const { return snapshot_count_; }

private:
    std::string filename_;
    std::ofstream file_stream_;
    std::vector<char> write_buffer_;  // Buffered writer for performance
    size_t snapshot_count_;
    bool is_initialized_;
    
    // Buffer management
    static constexpr size_t BUFFER_SIZE = 64 * 1024;  // 64KB buffer
    
    // Write buffered data to file when buffer gets full
    void flushBuffer();
    
    // Append string to buffer
    void appendToBuffer(const std::string& data);
    void appendToBuffer(const char* data, size_t length);
    
    // Format timestamp to ISO 8601 format (matching sample)
    std::string formatTimestamp(const std::chrono::nanoseconds& timestamp) const;
    
    // Format double to string with proper precision
    std::string formatPrice(double price) const;
    
    // Format integer fields
    std::string formatSize(uint64_t size) const;
    std::string formatCount(uint32_t count) const;
    
    // Build the complete CSV row string for a snapshot
    std::string buildCsvRow(const MbpSnapshot& snapshot, uint64_t row_index) const;
    
    // CSV header constant (matching the exact format from sample mbp.csv)
    static const char* CSV_HEADER;
};
