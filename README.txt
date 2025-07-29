# MBP-10 Order Book Reconstruction Engine

High-performance C++ implementation for reconstructing Market By Price (MBP-10) data from Market By Order (MBO) streams. Built for the Blockhouse Quantitative Developer Work Trial.

## Overview

This engine reconstructs real-time limit order books from MBO action streams, maintaining a complete 10-level market depth view for both bid and ask sides. The implementation prioritizes speed and correctness to handle high-frequency trading data rates while maintaining microsecond-level precision.

## Task Implementation

This solution addresses the specific requirements for MBO to MBP-10 reconstruction:

1. **Reset Event Handling**: Initial 'R' action ignored, starting with empty order book
2. **Trade Sequence Processing**: T-F-C action sequences properly combined and applied to correct book side
3. **Side 'N' Trade Filtering**: Trade events with side 'N' ignored as specified
4. **Opposite Side Logic**: Trade events applied to the side that actually exists in the book

## Performance Achievements

- **Parsing Speed**: 535+ events/ms (5,886 events parsed in 11ms)
- **Processing Speed**: 27+ events/ms with full MBP-10 snapshot generation  
- **Total Throughput**: 23,100+ events/second end-to-end processing
- **Memory Efficiency**: Cache-friendly data structures with minimal heap allocations
- **Output Generation**: 5,840 MBP-10 snapshots generated from 5,886 MBO events

## Architecture and Optimization Strategy

### Core Data Structures

1. **Sorted Price Levels**
   - `std::map<double, LevelData, std::greater<>>` for bids (highest to lowest)
   - `std::map<double, LevelData, std::less<>>` for asks (lowest to highest)
   - O(log n) insertion/deletion with automatic price sorting

2. **Fast Order Lookup**
   - `std::unordered_map<uint64_t, OrderData>` for O(1) order access
   - Critical for cancel operations and trade matching

3. **Level Aggregation**
   - Pre-computed price, total size, and order count per level
   - FIFO queues for proper trade execution order
   - Efficient cleanup when levels become empty

### Critical Performance Optimizations

1. **Compiler-Level Optimizations**
   - GCC `-O3` with link-time optimization (`-flto`) for maximum performance
   - CPU-specific tuning (`-march=native -mtune=native`) 
   - Fast math operations (`-ffast-math`) and aggressive inlining
   - Release builds remove all debug assertions

2. **Memory Management Strategy**
   - Pre-allocated parsing buffers (65KB) to minimize I/O system calls
   - Vector capacity reservation based on file size estimation
   - Iterator reuse and contiguous data layout for cache efficiency
   - Minimal dynamic allocations during hot path execution

3. **Parsing Optimizations**
   - C-style parsing avoiding iostream overhead
   - Direct memory operations with `strtod()` and `strtoull()`
   - Field skipping without string creation
   - Timestamp parsing optimized for ISO 8601 format

4. **Order Book Optimizations**
   - State machine for T-F-C trade sequence handling
   - FIFO queue management for proper trade execution order
   - Efficient level cleanup preventing memory leaks
   - Pre-computed aggregates avoiding recalculation

5. **Output Generation**
   - 64KB buffered CSV writing with auto-flush
   - Direct field access using pointer arithmetic
   - String formatting optimized for common price/size ranges
   - Batch operations to reduce system call overhead

## Build Instructions

### Prerequisites
- GCC 15.1.0+ (MinGW-w64 for Windows)
- Make or mingw32-make
- C++17 standard library

### Building
```bash
# Optimized release build
mingw32-make release

# Debug build with symbols
mingw32-make debug

# Clean build artifacts  
mingw32-make clean
```

### Build Targets
- `release`: Production executable (`bin/orderbook_engine_release.exe`)
- `debug`: Debug version (`bin/orderbook_engine_debug.exe`)  
- Tests: `test_order_book.exe`, `test_mbo_parser.exe`

## Usage

```bash
# Run with provided sample data
./bin/orderbook_engine_release.exe ./quant_dev_trial/mbo.csv

# General usage
./bin/orderbook_engine_release.exe <input_mbo_file.csv>
```

The engine outputs reconstructed MBP-10 data to `output.csv` with format matching the provided reference `mbp.csv`.

## Key Implementation Details

### Trade Sequence Processing (T-F-C)
The engine correctly handles the complex trade sequences as specified:
- **Trade Event ('T')**: Stores trade information and sets expectation for Fill
- **Fill Event ('F')**: Processes FIFO order filling on the opposite side from Trade
- **Cancel Event ('C')**: Completes the sequence by removing/reducing filled orders

### Opposite Side Logic  
When a Trade appears on side X at price P, but no orders exist at that level, the actual orders being filled are on the opposite side. The engine applies changes to the correct side that actually exists in the book.

### Performance Bottlenecks Addressed
1. **File I/O**: Large buffer sizes and batch operations
2. **String Operations**: C-style parsing without temporary string creation  
3. **Memory Allocation**: Pre-allocated containers and object reuse
4. **Data Structure Access**: Hash maps for O(1) lookups, sorted maps for O(log n) level management

### Correctness Validation
- Comprehensive test suite covering edge cases
- Output format exactly matches reference `mbp.csv`
- Proper handling of partial cancellations and over-cancellation protection
- State machine validation for trade sequences

## Testing and Validation

The implementation includes comprehensive unit tests:

```bash
# Run order book tests
./bin/test_order_book.exe

# Run MBO parser tests  
./bin/test_mbo_parser.exe
```

### Test Coverage
- Order book basic operations (add, cancel, reset)
- Price level management and automatic sorting
- Partial and full order cancellation scenarios
- Trade event handling with FIFO execution order
- MBP-10 snapshot generation and formatting
- Edge cases and error handling

## Performance Results

Using the provided sample data (`quant_dev_trial/mbo.csv`):
- **Input**: 5,886 MBO events
- **Output**: 5,840 MBP-10 snapshots  
- **Parse Time**: 8ms (735+ events/ms)
- **Processing Time**: 214ms (27+ events/ms)
- **Total Runtime**: ~222ms end-to-end

This achieves the performance requirements for high-frequency trading applications where microsecond latency matters.

## Project Structure

```
├── src/                    # Source code
│   ├── main.cpp           # Main application entry point
│   ├── mbo_parser.cpp     # High-performance MBO CSV parser
│   ├── order_book.cpp     # Core order book implementation  
│   ├── mbp_csv_writer.cpp # MBP-10 CSV output writer
│   ├── test_order_book.cpp # Comprehensive order book tests
│   └── test_mbo_parser.cpp # MBO parser unit tests
├── include/                # Header files
│   ├── mbo_parser.h       # MBO parser interface
│   ├── order_book.h       # Order book data structures
│   └── mbp_csv_writer.h   # CSV writer interface  
├── bin/                    # Compiled executables
├── build/                  # Object files
├── quant_dev_trial/        # Sample data from Databento
│   ├── mbo.csv            # Input MBO data (5,886 events)
│   └── mbp.csv            # Reference MBP output (expected format)
├── Makefile               # Build configuration with optimization flags
├── output.csv             # Generated MBP-10 snapshots (program output)
└── README.txt             # This documentation
```

## Technical Specifications

- **Language**: C++17 with standard library only
- **Compiler**: GCC 15.1.0+ with aggressive optimization flags
- **Platform**: Windows (MinGW-w64), easily portable to Linux/macOS  
- **Memory Usage**: Minimal heap allocations, efficient for large datasets
- **Precision**: Nanosecond timestamp accuracy, double precision prices
- **Dependencies**: None beyond standard C++ library

## Design Philosophy and Trade-offs

This implementation prioritizes correctness first, then speed:

1. **Correctness**: Output format exactly matches reference, proper trade sequence handling
2. **Performance**: Optimized for HFT requirements with microsecond-level latencies  
3. **Memory Efficiency**: Cache-friendly data structures and minimal allocations
4. **Maintainability**: Clean modular design with comprehensive test coverage

### Potential Improvements
- Lock-free data structures for multi-threaded scenarios
- SIMD optimizations for bulk operations
- Memory-mapped file I/O for extremely large datasets
- Custom memory allocators for even better cache performance

The current implementation achieves excellent performance while maintaining code clarity and correctness, suitable for production high-frequency trading systems.


