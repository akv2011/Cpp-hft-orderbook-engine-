#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include "order_book.h"

// High-performance CSV writer for MBP-10 snapshots
class MbpCsvWriter {
public:
    explicit MbpCsvWriter(const std::string& filename = "output.csv");
    ~MbpCsvWriter();
    
    bool initialize();
    bool writeSnapshot(const MbpSnapshot& snapshot, uint64_t row_index = 0);
    void flush();
    void close();
    
    size_t getSnapshotCount() const { return snapshot_count_; }

private:
    std::string filename_;
    std::ofstream file_stream_;
    std::vector<char> write_buffer_;
    size_t snapshot_count_;
    bool is_initialized_;
    
    static constexpr size_t BUFFER_SIZE = 64 * 1024;
    
    void flushBuffer();
    void appendToBuffer(const std::string& data);
    void appendToBuffer(const char* data, size_t length);
    
    std::string formatTimestamp(const std::chrono::nanoseconds& timestamp) const;
    std::string formatPrice(double price) const;
    std::string formatSize(uint64_t size) const;
    std::string formatCount(uint32_t count) const;
    std::string buildCsvRow(const MbpSnapshot& snapshot, uint64_t row_index) const;
    
    static const char* CSV_HEADER;
};
