# High-Performance C++ Order Book Engine Makefile
# Optimized for HFT applications with maximum performance

# Compiler and flags (using conda-installed MinGW)
CXX = "C:\Users\arunk\.conda\envs\blockhouse\Library\bin\x86_64-w64-mingw32-g++.exe"
CXXSTD = -std=c++17
SRCDIR = src
INCDIR = include
BINDIR = bin
OBJDIR = build

# Source files for main application
MAIN_SOURCES = $(SRCDIR)/main.cpp $(SRCDIR)/mbo_parser.cpp $(SRCDIR)/order_book.cpp $(SRCDIR)/mbp_csv_writer.cpp $(SRCDIR)/event_buffer.cpp
MAIN_OBJECTS = $(MAIN_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# All source files (for other targets)
ALL_SOURCES = $(wildcard $(SRCDIR)/*.cpp)
ALL_OBJECTS = $(ALL_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

TARGET = $(BINDIR)/orderbook_engine

# Debug flags
DEBUG_FLAGS = -g -O0 -DDEBUG -Wall -Wextra -Wpedantic
DEBUG_TARGET = $(BINDIR)/orderbook_engine_debug

# Release/Optimized flags for maximum performance
RELEASE_FLAGS = -O3 -DNDEBUG -march=native -mtune=native -flto
RELEASE_FLAGS += -ffast-math -funroll-loops -finline-functions
RELEASE_FLAGS += -fomit-frame-pointer -pipe
RELEASE_FLAGS += -Wall -Wextra
RELEASE_TARGET = $(BINDIR)/orderbook_engine_release

# Common flags
COMMON_FLAGS = $(CXXSTD) -I$(INCDIR)

# Default target
all: release

# Create necessary directories (Windows-compatible)
$(OBJDIR):
	@mkdir $(OBJDIR) 2>nul || echo Directory exists

$(BINDIR):
	@mkdir $(BINDIR) 2>nul || echo Directory exists

# Debug build
debug: CXXFLAGS = $(COMMON_FLAGS) $(DEBUG_FLAGS)
debug: $(OBJDIR) $(BINDIR) $(DEBUG_TARGET)

$(DEBUG_TARGET): $(MAIN_OBJECTS)
	$(CXX) $(MAIN_OBJECTS) -o $@ $(CXXFLAGS)

# Release/Optimized build (critical for speed evaluation)
release: CXXFLAGS = $(COMMON_FLAGS) $(RELEASE_FLAGS)
release: $(OBJDIR) $(BINDIR) $(RELEASE_TARGET)

$(RELEASE_TARGET): $(MAIN_OBJECTS)
	$(CXX) $(MAIN_OBJECTS) -o $@ $(CXXFLAGS)

# Object file compilation
$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts (Windows-compatible)
clean:
	@rmdir /S /Q $(OBJDIR) 2>nul || echo Clean complete
	@rmdir /S /Q $(BINDIR) 2>nul || echo Clean complete

# Clean and rebuild
rebuild: clean all

# Install (copy to system path if needed)
install: release
	cp $(RELEASE_TARGET) /usr/local/bin/orderbook_engine

# Run tests
test: release
	@echo "Running basic functionality test..."
	./$(RELEASE_TARGET)
	@echo "Basic test completed successfully!"
	@echo ""
	@echo "Running comprehensive test suite..."
	$(MAKE) -C tests test
	@echo ""
	@echo "🎉 All tests completed successfully!"

# Build object files for tests
build-objects: $(MAIN_OBJECTS)

# Run only main program test
test-main: release
	@echo "Running main program test..."
	./$(RELEASE_TARGET)
	@echo "Main program test completed!"

# Run only test suite
test-suite:
	@echo "Running test suite..."
	$(MAKE) -C tests test

# Profile build (for performance analysis)
profile: CXXFLAGS = $(COMMON_FLAGS) $(RELEASE_FLAGS) -pg
profile: $(OBJDIR) $(BINDIR)
	$(CXX) $(SOURCES) -o $(BINDIR)/orderbook_engine_profile $(CXXFLAGS)

# Show build configuration
info:
	@echo "Build Configuration:"
	@echo "CXX: $(CXX)"
	@echo "Standard: $(CXXSTD)"
	@echo "Release Flags: $(RELEASE_FLAGS)"
	@echo "Debug Flags: $(DEBUG_FLAGS)"
	@echo "Sources: $(SOURCES)"
	@echo "Target: $(RELEASE_TARGET)"

# Help
help:
	@echo "Available targets:"
	@echo "  all/release  - Build optimized release version (default)"
	@echo "  debug        - Build debug version with symbols"
	@echo "  profile      - Build with profiling enabled"
	@echo "  clean        - Remove build artifacts"
	@echo "  rebuild      - Clean and rebuild"
	@echo "  install      - Install to system path"
	@echo "  test         - Run all tests (main program + test suite)"
	@echo "  test-main    - Run only main program test"
	@echo "  test-suite   - Run only comprehensive test suite"
	@echo "  build-objects - Build object files for tests"
	@echo "  info         - Show build configuration"
	@echo "  help         - Show this help"

.PHONY: all release debug clean rebuild install test test-main test-suite build-objects profile info help
