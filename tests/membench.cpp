#include <iostream>
#include <string>
#include <string_view>
#include <chrono>
#include <vector>
#include <cstring>
#include <iomanip>
#include <malloc.h>
#include <unistd.h>
#include <fstream>
#include <cstdlib>

// Color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// Timer
class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    double elapsed_sec() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(end - start_).count();
    }
    void reset() { start_ = std::chrono::high_resolution_clock::now(); }
private:
    std::chrono::high_resolution_clock::time_point start_;
};

// Memory tracking
struct MemStats {
    double heap_used;   // Heap memory in MB
    double rss_used;    // RSS in MB
};

MemStats get_memory_stats() {
    MemStats stats = {0, 0};
    
    // Get heap usage from mallinfo (more compatible)
    struct mallinfo mi = mallinfo();
    stats.heap_used = mi.uordblks / (1024.0 * 1024.0);
    
    // Get RSS from /proc
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            long rss_kb = std::atol(line.c_str() + 6);
            stats.rss_used = rss_kb / 1024.0;
            break;
        }
    }
    
    return stats;
}

void print_result_mem(const char* test_name, double elapsed, double ops_per_sec,
                      MemStats before, MemStats after) {
    std::cout << "  " << std::left << std::setw(45) << test_name;
    std::cout << std::setw(8) << std::fixed << std::setprecision(3) << elapsed << " sec  ";
    std::cout << std::setw(12) << std::fixed << std::setprecision(0) << ops_per_sec << " ops/sec\n";
    std::cout << "    Memory: Heap " << std::setw(6) << std::fixed << std::setprecision(2) 
              << before.heap_used << "→" << after.heap_used << " MB | RSS " 
              << before.rss_used << "→" << after.rss_used << " MB | Δ " 
              << std::showpos << (after.rss_used - before.rss_used) << " MB\n"
              << std::noshowpos;
}

void print_header(const char* header) {
    std::cout << COLOR_CYAN "\n" << header << "\n" COLOR_RESET;
    std::cout << "  " << std::left << std::setw(45) << "Test" 
              << std::setw(8) << "Time" << std::setw(12) << "Ops/Sec" << "\n";
    std::cout << "  " << std::left << std::setw(45) << "----" 
              << std::setw(8) << "----" << std::setw(12) << "-------" << "\n";
}

// Creation memory benchmark
void bench_creation_memory() {
    print_header("STD::STRING CREATION - MEMORY USAGE");
    
    const int iterations = 1000000;
    MemStats before, after;
    
    // SSO string
    before = get_memory_stats();
    Timer timer;
    for (int i = 0; i < iterations; i++) {
        std::string s = "Hello";
        volatile char c = s[0];
        (void)c;
    }
    after = get_memory_stats();
    print_result_mem("Create SSO string", timer.elapsed_sec(), 
                     iterations / timer.elapsed_sec(), before, after);
    
    // Heap string
    before = get_memory_stats();
    timer.reset();
    for (int i = 0; i < iterations; i++) {
        std::string s = "This is a long string for heap allocation test";
        volatile char c = s[0];
        (void)c;
    }
    after = get_memory_stats();
    print_result_mem("Create heap string", timer.elapsed_sec(), 
                     iterations / timer.elapsed_sec(), before, after);
    
    // Large heap string (1000 chars)
    std::string large_str(1000, 'x');
    before = get_memory_stats();
    timer.reset();
    for (int i = 0; i < iterations / 10; i++) {
        std::string s = large_str;
        volatile char c = s[0];
        (void)c;
    }
    after = get_memory_stats();
    print_result_mem("Create 1000-char string", timer.elapsed_sec(), 
                     (iterations / 10) / timer.elapsed_sec(), before, after);
}

// Operation memory benchmark
void bench_operations_memory() {
    print_header("STD::STRING OPERATIONS - MEMORY USAGE");
    
    const int iterations = 1000000;
    MemStats before, after;
    
    std::string sso_str = "Hello";
    std::string heap_str = "This is a much longer string that goes to the heap";
    
    // Copy SSO (no heap allocation)
    before = get_memory_stats();
    Timer timer;
    for (int i = 0; i < iterations; i++) {
        std::string copy = sso_str;
        volatile char c = copy[0];
        (void)c;
    }
    after = get_memory_stats();
    print_result_mem("Copy SSO (no heap)", timer.elapsed_sec(), 
                     iterations / timer.elapsed_sec(), before, after);
    
    // Copy heap (allocates)
    before = get_memory_stats();
    timer.reset();
    for (int i = 0; i < iterations; i++) {
        std::string copy = heap_str;
        volatile char c = copy[0];
        (void)c;
    }
    after = get_memory_stats();
    print_result_mem("Copy heap (allocates)", timer.elapsed_sec(), 
                     iterations / timer.elapsed_sec(), before, after);
    
    // Substring SSO
    before = get_memory_stats();
    timer.reset();
    for (int i = 0; i < iterations; i++) {
        std::string sub = sso_str.substr(1, 3);
        volatile char c = sub[0];
        (void)c;
    }
    after = get_memory_stats();
    print_result_mem("Substring SSO", timer.elapsed_sec(), 
                     iterations / timer.elapsed_sec(), before, after);
    
    // Substring heap
    before = get_memory_stats();
    timer.reset();
    for (int i = 0; i < iterations; i++) {
        std::string sub = heap_str.substr(5, 10);
        volatile char c = sub[0];
        (void)c;
    }
    after = get_memory_stats();
    print_result_mem("Substring heap", timer.elapsed_sec(), 
                     iterations / timer.elapsed_sec(), before, after);
}

// View memory benchmark
void bench_view_memory() {
    print_header("STD::STRING_VIEW - MEMORY USAGE (Zero Allocation!)");
    
    const int iterations = 10000000;
    MemStats before, after;
    
    std::string str = "The quick brown fox jumps over the lazy dog";
    std::string_view view(str);
    
    // View creation (zero allocation)
    before = get_memory_stats();
    Timer timer;
    for (int i = 0; i < iterations; i++) {
        std::string_view v = str;
        volatile size_t len = v.size();
        (void)len;
    }
    after = get_memory_stats();
    print_result_mem("Create views (no alloc)", timer.elapsed_sec(), 
                     iterations / timer.elapsed_sec(), before, after);
    
    // View substring (zero allocation)
    before = get_memory_stats();
    timer.reset();
    for (int i = 0; i < iterations; i++) {
        std::string_view sub = view.substr(4, 15);
        volatile size_t len = sub.size();
        (void)len;
    }
    after = get_memory_stats();
    print_result_mem("View substring (no alloc)", timer.elapsed_sec(), 
                     iterations / timer.elapsed_sec(), before, after);
    
    // Find in view (zero allocation)
    before = get_memory_stats();
    timer.reset();
    for (int i = 0; i < iterations; i++) {
        volatile size_t pos = view.find('j');
        (void)pos;
    }
    after = get_memory_stats();
    print_result_mem("View find (no alloc)", timer.elapsed_sec(), 
                     iterations / timer.elapsed_sec(), before, after);
}

// Arena-like pattern memory
void bench_arena_memory() {
    print_header("ARENA-LIKE PATTERN - MEMORY USAGE");
    
    const int iterations = 100000;
    MemStats before, after;
    
    // With reserve
    std::string arena;
    arena.reserve(65536);
    
    before = get_memory_stats();
    Timer timer;
    for (int i = 0; i < iterations; i++) {
        arena.clear();
        arena += "Hello, World!";
        for (int j = 0; j < 100; j++) {
            arena.push_back('x');
        }
    }
    after = get_memory_stats();
    print_result_mem("String reuse (reserved)", timer.elapsed_sec(), 
                     iterations / timer.elapsed_sec(), before, after);
    
    // Without reserve (with reallocation)
    std::string growing;
    
    before = get_memory_stats();
    timer.reset();
    for (int i = 0; i < iterations; i++) {
        growing.push_back('x');
    }
    after = get_memory_stats();
    print_result_mem("String growth (reallocs)", timer.elapsed_sec(), 
                     iterations / timer.elapsed_sec(), before, after);
    
    std::cout << "    Final capacity: " << growing.capacity() << " bytes ("
              << std::fixed << std::setprecision(2) 
              << growing.capacity() / (1024.0 * 1024.0) << " MB)\n";
}

// Massive allocation memory
void bench_mass_memory() {
    print_header("MASSIVE ALLOCATION - MEMORY TRACKING");
    
    const size_t max_strings = 1000000;  // 1 million strings
    MemStats before, after;
    
    before = get_memory_stats();
    std::vector<std::string> strings;
    strings.reserve(max_strings);
    
    std::cout << "  Allocated vector: " << std::fixed << std::setprecision(2)
              << (max_strings * sizeof(std::string)) / (1024.0 * 1024.0) 
              << " MB for " << max_strings << " std::string objects\n";
    
    Timer timer;
    for (size_t i = 0; i < max_strings; i++) {
        strings.emplace_back("Test string");
    }
    after = get_memory_stats();
    
    std::cout << "  Created " << strings.size() << " strings\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(3) 
              << timer.elapsed_sec() << " sec\n";
    std::cout << "  Memory: Heap " << std::fixed << std::setprecision(2) 
              << after.heap_used << " MB, RSS " << after.rss_used << " MB (Δ " 
              << std::showpos << (after.rss_used - before.rss_used) << " MB)\n"
              << std::noshowpos;
    std::cout << "  Per string: " << std::fixed << std::setprecision(2)
              << ((after.heap_used - before.heap_used) * 1024 * 1024) / max_strings 
              << " bytes heap\n";
    
    strings.clear();
    strings.shrink_to_fit();
    after = get_memory_stats();
    std::cout << "  After cleanup: Heap " << std::fixed << std::setprecision(2)
              << after.heap_used << " MB, RSS " << after.rss_used << " MB\n";
}

int main() {
    std::cout << COLOR_BOLD "\nSTD::STRING MEMORY-TRACKING BENCHMARK\n" COLOR_RESET;
    std::cout << "  Testing memory usage for all scenarios\n";
    std::cout << "  String size: " << sizeof(std::string) << " bytes\n";
    std::cout << "  String_view size: " << sizeof(std::string_view) << " bytes\n";
    
    MemStats initial = get_memory_stats();
    std::cout << "\n  Initial memory: Heap " << std::fixed << std::setprecision(2)
              << initial.heap_used << " MB, RSS " << initial.rss_used << " MB\n\n";
    
    bench_creation_memory();
    bench_operations_memory();
    bench_view_memory();
    bench_arena_memory();
    bench_mass_memory();
    
    MemStats final = get_memory_stats();
    std::cout << COLOR_GREEN "\nBenchmarks completed!\n" COLOR_RESET;
    std::cout << "  Final memory: Heap " << std::fixed << std::setprecision(2)
              << final.heap_used << " MB, RSS " << final.rss_used << " MB\n";
    std::cout << "  Total change: " << std::showpos 
              << (final.rss_used - initial.rss_used) << " MB\n" << std::noshowpos;
    
    return 0;
}