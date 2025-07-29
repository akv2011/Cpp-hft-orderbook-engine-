#include "mbp_csv_writer.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// CSV header matching the exact format from sample mbp.csv
const char* MbpCsvWriter::CSV_HEADER = 
    ",ts_recv,ts_event,rtype,publisher_id,instrument_id,action,side,depth,price,size,flags,ts_in_delta,sequence,"
    "bid_px_00,bid_sz_00,bid_ct_00,ask_px_00,ask_sz_00,ask_ct_00,"
    "bid_px_01,bid_sz_01,bid_ct_01,ask_px_01,ask_sz_01,ask_ct_01,"
    "bid_px_02,bid_sz_02,bid_ct_02,ask_px_02,ask_sz_02,ask_ct_02,"
    "bid_px_03,bid_sz_03,bid_ct_03,ask_px_03,ask_sz_03,ask_ct_03,"
    "bid_px_04,bid_sz_04,bid_ct_04,ask_px_04,ask_sz_04,ask_ct_04,"
    "bid_px_05,bid_sz_05,bid_ct_05,ask_px_05,ask_sz_05,ask_ct_05,"
    "bid_px_06,bid_sz_06,bid_ct_06,ask_px_06,ask_sz_06,ask_ct_06,"
    "bid_px_07,bid_sz_07,bid_ct_07,ask_px_07,ask_sz_07,ask_ct_07,"
    "bid_px_08,bid_sz_08,bid_ct_08,ask_px_08,ask_sz_08,ask_ct_08,"
    "bid_px_09,bid_sz_09,bid_ct_09,ask_px_09,ask_sz_09,ask_ct_09,"
    "symbol,order_id";

MbpCsvWriter::MbpCsvWriter(const std::string& filename)
    : filename_(filename), snapshot_count_(0), is_initialized_(false) {
    write_buffer_.reserve(BUFFER_SIZE);
}

MbpCsvWriter::~MbpCsvWriter() {
    close();
}

bool MbpCsvWriter::initialize() {
    if (is_initialized_) {
        return true;
    }
    
    file_stream_.open(filename_, std::ios::out | std::ios::trunc);
    if (!file_stream_.is_open()) {
        std::cerr << "Error: Could not open output file: " << filename_ << std::endl;
        return false;
    }
    
    // Write CSV header
    appendToBuffer(CSV_HEADER);
    appendToBuffer("\n");
    
    is_initialized_ = true;
    return true;
}

bool MbpCsvWriter::writeSnapshot(const MbpSnapshot& snapshot, uint64_t row_index) {
    if (!is_initialized_) {
        std::cerr << "Error: CSV writer not initialized. Call initialize() first." << std::endl;
        return false;
    }
    
    std::string csv_row = buildCsvRow(snapshot, row_index);
    appendToBuffer(csv_row);
    appendToBuffer("\n");
    
    snapshot_count_++;
    
    // Auto-flush if buffer is getting full
    if (write_buffer_.size() > BUFFER_SIZE * 0.8) {
        flushBuffer();
    }
    
    return true;
}

void MbpCsvWriter::flush() {
    flushBuffer();
    if (file_stream_.is_open()) {
        file_stream_.flush();
    }
}

void MbpCsvWriter::close() {
    if (is_initialized_) {
        flushBuffer();
        file_stream_.close();
        is_initialized_ = false;
    }
}

void MbpCsvWriter::flushBuffer() {
    if (!write_buffer_.empty() && file_stream_.is_open()) {
        file_stream_.write(write_buffer_.data(), write_buffer_.size());
        write_buffer_.clear();
    }
}

void MbpCsvWriter::appendToBuffer(const std::string& data) {
    appendToBuffer(data.c_str(), data.length());
}

void MbpCsvWriter::appendToBuffer(const char* data, size_t length) {
    write_buffer_.insert(write_buffer_.end(), data, data + length);
}

std::string MbpCsvWriter::formatTimestamp(const std::chrono::nanoseconds& timestamp) const {
    // Convert nanoseconds to time_point
    auto time_point = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(timestamp)
    );
    
    // Get time_t for formatting
    auto time_t_val = std::chrono::system_clock::to_time_t(time_point);
    
    // Get nanoseconds part
    auto duration_since_epoch = timestamp;
    auto seconds_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(duration_since_epoch);
    auto nanoseconds_part = duration_since_epoch - seconds_since_epoch;
    
    // Format as ISO 8601: 2025-07-17T08:05:03.360677248Z
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%dT%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(9) << nanoseconds_part.count() << "Z";
    
    return oss.str();
}

std::string MbpCsvWriter::formatPrice(double price) const {
    if (price == 0.0) {
        return "";
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << price;
    return oss.str();
}

std::string MbpCsvWriter::formatSize(uint64_t size) const {
    if (size == 0) {
        return "0";
    }
    return std::to_string(size);
}

std::string MbpCsvWriter::formatCount(uint32_t count) const {
    if (count == 0) {
        return "0";
    }
    return std::to_string(count);
}

std::string MbpCsvWriter::buildCsvRow(const MbpSnapshot& snapshot, uint64_t row_index) const {
    std::ostringstream row;
    
    // Format timestamp for both ts_recv and ts_event
    std::string timestamp_str = formatTimestamp(snapshot.timestamp);
    
    // Build the row following the exact format from sample mbp.csv
    row << row_index << ","                           // Row index (first column)
        << timestamp_str << ","                       // ts_recv
        << timestamp_str << ","                       // ts_event  
        << "10,"                                      // rtype (constant 10)
        << "2,"                                       // publisher_id (constant 2)
        << "1108,"                                    // instrument_id (constant 1108)
        << "S,"                                       // action (S for snapshot)
        << "N,"                                       // side (N for none/snapshot)
        << "0,"                                       // depth (0 for snapshot)
        << ","                                        // price (empty for snapshot)
        << "0,"                                       // size (0 for snapshot)
        << "0,"                                       // flags (0 for snapshot)
        << "0,"                                       // ts_in_delta (0 for snapshot)
        << snapshot.sequence_number << ",";           // sequence
    
    // Add bid levels 00-09 (price, size, count for each level)
    const double* bid_prices = &snapshot.bid_px_00;
    const uint64_t* bid_sizes = &snapshot.bid_sz_00;
    const uint32_t* bid_counts = &snapshot.bid_ct_00;
    
    const double* ask_prices = &snapshot.ask_px_00;
    const uint64_t* ask_sizes = &snapshot.ask_sz_00;
    const uint32_t* ask_counts = &snapshot.ask_ct_00;
    
    // Interleave bid and ask levels: bid_px_00, bid_sz_00, bid_ct_00, ask_px_00, ask_sz_00, ask_ct_00, ...
    for (int i = 0; i < 10; ++i) {
        // Bid level
        row << formatPrice(bid_prices[i]) << ","
            << formatSize(bid_sizes[i]) << ","
            << formatCount(bid_counts[i]) << ",";
            
        // Ask level
        row << formatPrice(ask_prices[i]) << ","
            << formatSize(ask_sizes[i]) << ","
            << formatCount(ask_counts[i]);
            
        if (i < 9) {
            row << ",";
        }
    }
    
    // Add symbol and order_id (constants from sample)
    row << ",ARL,0";
    
    return row.str();
}
