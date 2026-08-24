#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <malloc.h>
#include <fstream>

// Color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

#define CONCAT_ITERS 100000
#define STRINGS_INIT 1000000
#define ARENA_REUSE_CYCLES 1000
#define HUGE_STRING_SIZE (10 * 1024 * 1024)  // 10 MB

// Arena for all devices - SAME AS C VERSION!
#if defined(__arm__) || defined(__aarch64__)
    #define ARENA_SIZE (512UL * 1024 * 1024) // 512 MB for ARM
#else
    #define ARENA_SIZE (2UL * 1024 * 1024 * 1024) // 2 GB for x86
#endif

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
    double heap_used;
    double rss_used;
};

MemStats get_memory_stats() {
    MemStats stats = {0, 0};
    struct mallinfo mi = mallinfo();
    stats.heap_used = mi.uordblks / (1024.0 * 1024.0);
    
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            stats.rss_used = std::atol(line.c_str() + 6) / 1024.0;
            break;
        }
    }
    return stats;
}

// ============================================================================
// TEST 1: Massive String Allocation (FAIR - uses 2GB arena!)
// ============================================================================

void bench_stress_alloc() {
    std::cout << COLOR_CYAN "\n=== TEST 1: MASSIVE STRING ALLOCATION ===\n" COLOR_RESET;
    std::cout << "Consequent string allocation until the arena is full\n";
    std::cout << "   String size: " << sizeof(std::string) << " bytes\n";
    std::cout << "   Arena size: " << (ARENA_SIZE / (1024 * 1024)) << " MB\n";
    
    MemStats before = get_memory_stats();
    
    // Allocate 2GB arena (same as C version)
    void* arena = malloc(ARENA_SIZE);
    if (arena == NULL) {
        std::cout << COLOR_RED "   Failed to allocate the arena\n" COLOR_RESET;
        return;
    }
    std::cout << "   Arena has been allocated\n";
    
    // Calculate max strings that fit in arena
    // Each std::string is 32 bytes, plus we need space for string data
    // For "Hello" (5 chars + null = 6 bytes), each string needs ~38 bytes total
    // But to be fair, let's just use the same approach: fill with string objects
    size_t max_strings = ARENA_SIZE / sizeof(std::string);
    
    std::cout << "   Strings max: ~" << max_strings << "\n";
    
    // Use placement new to construct strings in the arena
    std::string* strings = static_cast<std::string*>(arena);
    size_t count = 0;
    
    Timer timer;
    const char* samples[] = {"Hello", "World!!!", "Pikachu, I choose you!"};
    int sample_count = 3;
    
    // Construct strings in-place
    for (size_t i = 0; i < max_strings; i++) {
        new (&strings[i]) std::string(samples[i % sample_count]);
        count++;
    }
    
    double elapsed = timer.elapsed_sec();
    MemStats after = get_memory_stats();
    
    std::cout << "   Strings created: " << count << "\n";
    std::cout << "   Time: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    std::cout << "   Speed: " << std::fixed << std::setprecision(0) 
              << count / elapsed << " strings per sec\n";
    std::cout << "   Memory: Heap " << std::fixed << std::setprecision(2) 
              << after.heap_used << " MB, RSS " << after.rss_used << " MB (Δ " 
              << std::showpos << (after.rss_used - before.rss_used) << " MB)\n"
              << std::noshowpos;
    
    // Destroy strings
    timer.reset();
    for (size_t i = 0; i < count; i++) {
        strings[i].~basic_string();
    }
    elapsed = timer.elapsed_sec();
    std::cout << "   Freed: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    
    free(arena);
    after = get_memory_stats();
    std::cout << "   After cleanup: Heap " << std::fixed << std::setprecision(2)
              << after.heap_used << " MB, RSS " << after.rss_used << " MB\n";
}

// ============================================================================
// TEST 2: 1 Million Strings (same as C version)
// ============================================================================

void bench_1m_strings() {
    std::cout << COLOR_CYAN "\n=== TEST 2: 1 MILLION STRINGS ===\n" COLOR_RESET;
    std::cout << "Test: " << STRINGS_INIT << " strings (short & long)\n";
    
    const char* short_str = "Hello";
    const char* long_str = "This is a long string for testing heap allocation.";
    
    MemStats before = get_memory_stats();
    std::vector<std::string> strings;
    strings.reserve(STRINGS_INIT);
    
    Timer timer;
    for (int i = 0; i < STRINGS_INIT; i++) {
        strings.emplace_back((i % 2 == 0) ? short_str : long_str);
    }
    double elapsed = timer.elapsed_sec();
    MemStats after = get_memory_stats();
    
    std::cout << "   " << STRINGS_INIT << " strings created\n";
    std::cout << "   Time: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    std::cout << "   Speed: " << std::fixed << std::setprecision(0) 
              << STRINGS_INIT / elapsed << " strings per sec\n";
    std::cout << "   Memory: Heap " << std::fixed << std::setprecision(2)
              << after.heap_used << " MB, RSS " << after.rss_used << " MB (Δ "
              << std::showpos << (after.rss_used - before.rss_used) << " MB)\n"
              << std::noshowpos;
    
    timer.reset();
    strings.clear();
    strings.shrink_to_fit();
    elapsed = timer.elapsed_sec();
    std::cout << "   Freed: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
}

// ============================================================================
// TEST 3: Arena Reuse (same as C version)
// ============================================================================

void bench_arena_reuse() {
    std::cout << COLOR_CYAN "\n=== TEST 3: ARENA REUSE (reserve + clear) ===\n" COLOR_RESET;
    std::cout << "Arena reuse: " << ARENA_REUSE_CYCLES << " cycles of append+clear\n";
    
    MemStats before = get_memory_stats();
    std::string arena;
    arena.reserve(65536);
    
    Timer timer;
    for (int cycle = 0; cycle < ARENA_REUSE_CYCLES; cycle++) {
        arena.clear();
        arena += "Hello, World!";
        
        for (int i = 0; i < 1000; i++) {
            arena.push_back('x');
        }
        
        volatile char c = arena[0];
        (void)c;
    }
    double elapsed = timer.elapsed_sec();
    MemStats after = get_memory_stats();
    
    std::cout << "   Cycles: " << ARENA_REUSE_CYCLES << "\n";
    std::cout << "   Total appends: " << ARENA_REUSE_CYCLES * 1000 << "\n";
    std::cout << "   Time: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    std::cout << "   Speed: " << std::fixed << std::setprecision(0)
              << (ARENA_REUSE_CYCLES * 1000.0) / elapsed << " appends per sec\n";
    std::cout << "   Capacity preserved: " << arena.capacity() << " bytes\n";
    std::cout << "   Memory: Heap " << std::fixed << std::setprecision(2)
              << after.heap_used << " MB, RSS " << after.rss_used << " MB (Δ "
              << std::showpos << (after.rss_used - before.rss_used) << " MB)\n"
              << std::noshowpos;
}

// ============================================================================
// TEST 4: Concatenation Strategies (same as C version)
// ============================================================================

void bench_concat_comparison() {
    std::cout << COLOR_CYAN "\n=== TEST 4: CONCATENATION STRATEGIES ===\n" COLOR_RESET;
    
    // 4a. Without reserve
    std::cout << "\n" COLOR_YELLOW "4a. push_back without reserve:" COLOR_RESET "\n";
    MemStats before = get_memory_stats();
    std::string s;
    Timer timer;
    
    for (int i = 0; i < CONCAT_ITERS; i++) {
        s.push_back('A' + (i % 26));
    }
    
    double elapsed = timer.elapsed_sec();
    MemStats after = get_memory_stats();
    std::cout << "   String length: " << s.size() << "\n";
    std::cout << "   Time: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    std::cout << "   Speed: " << std::fixed << std::setprecision(0)
              << CONCAT_ITERS / elapsed << " operations per sec\n";
    std::cout << "   Memory: Δ " << std::showpos << (after.rss_used - before.rss_used) 
              << " MB\n" << std::noshowpos;
    
    // 4b. With reserve
    std::cout << "\n" COLOR_GREEN "4b. push_back with reserve:" COLOR_RESET "\n";
    before = get_memory_stats();
    std::string s_reserved;
    s_reserved.reserve(CONCAT_ITERS + 1);
    timer.reset();
    
    for (int i = 0; i < CONCAT_ITERS; i++) {
        s_reserved.push_back('A' + (i % 26));
    }
    
    elapsed = timer.elapsed_sec();
    after = get_memory_stats();
    std::cout << "   String length: " << s_reserved.size() << "\n";
    std::cout << "   Time: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    std::cout << "   Speed: " << std::fixed << std::setprecision(0)
              << CONCAT_ITERS / elapsed << " operations per sec\n";
    std::cout << "   Capacity: " << s_reserved.capacity() << ", waste: "
              << std::fixed << std::setprecision(2)
              << (1.0 - (double)s_reserved.size() / s_reserved.capacity()) * 100 << "%\n";
    std::cout << "   Memory: Δ " << std::showpos << (after.rss_used - before.rss_used)
              << " MB\n" << std::noshowpos;
}

// ============================================================================
// TEST 5: Huge String Handling (same as C version)
// ============================================================================

void bench_huge_strings() {
    std::cout << COLOR_CYAN "\n=== TEST 5: HUGE STRING HANDLING ===\n" COLOR_RESET;
    
    std::string huge_data(HUGE_STRING_SIZE - 1, 'x');
    
    MemStats before = get_memory_stats();
    Timer timer;
    std::string huge = huge_data;
    double elapsed = timer.elapsed_sec();
    MemStats after = get_memory_stats();
    
    std::cout << "   Created " << huge.size() << "-byte string\n";
    std::cout << "   Time: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    std::cout << "   Hash: " << std::hash<std::string>{}(huge) << "\n";
    std::cout << "   Memory: Δ " << std::showpos << (after.rss_used - before.rss_used)
              << " MB\n" << std::noshowpos;
    
    timer.reset();
    size_t hash = std::hash<std::string>{}(huge);
    elapsed = timer.elapsed_sec();
    std::cout << "   Hash lookup: " << std::fixed << std::setprecision(3) 
              << elapsed << " sec (hash: " << hash << ")\n";
    
    timer.reset();
    size_t pos = huge.find('y');
    elapsed = timer.elapsed_sec();
    std::cout << "   Find 'y': " << std::fixed << std::setprecision(3) 
              << elapsed << " sec (position: " << (pos == std::string::npos ? -1 : (int)pos) << ")\n";
}

// ============================================================================
// TEST 6: View Stress Test (same as C version)
// ============================================================================

void bench_view_stress() {
    std::cout << COLOR_CYAN "\n=== TEST 6: VIEW STRESS TEST ===\n" COLOR_RESET;
    
    const int num_views = 1000000;
    std::string source = "The quick brown fox jumps over the lazy dog. This is a test string for view operations.";
    
    MemStats before = get_memory_stats();
    Timer timer;
    std::vector<std::string_view> views;
    views.reserve(num_views);
    
    for (int i = 0; i < num_views; i++) {
        views.emplace_back(source.data() + (i % 20), 15);
    }
    double elapsed = timer.elapsed_sec();
    MemStats after = get_memory_stats();
    
    std::cout << "   Created " << num_views << " views\n";
    std::cout << "   Time: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    std::cout << "   Speed: " << std::fixed << std::setprecision(0)
              << num_views / elapsed << " views per sec\n";
    std::cout << "   Memory: Heap " << std::fixed << std::setprecision(2)
              << (after.heap_used - before.heap_used) << " MB ("
              << (num_views * sizeof(std::string_view)) / (1024.0 * 1024.0)
              << " MB for views)\n";
    
    timer.reset();
    volatile size_t total_len = 0;
    for (int i = 0; i < num_views; i++) {
        total_len += views[i].size();
    }
    elapsed = timer.elapsed_sec();
    std::cout << "   Processed all views: " << std::fixed << std::setprecision(3)
              << elapsed << " sec (total len: " << total_len << ")\n";
}

// ============================================================================
// TEST 7: Binary Data Stress (same as C version)
// ============================================================================

void bench_binary_stress() {
    std::cout << COLOR_CYAN "\n=== TEST 7: BINARY DATA STRESS ===\n" COLOR_RESET;
    
    const int binary_size = 1024 * 1024;  // 1 MB
    char* binary = new char[binary_size];
    
    srand(42);
    for (int i = 0; i < binary_size; i++) {
        binary[i] = rand() % 256;
    }
    
    MemStats before = get_memory_stats();
    Timer timer;
    std::string bin_str(binary, binary_size);
    double elapsed = timer.elapsed_sec();
    MemStats after = get_memory_stats();
    
    std::cout << "   Created " << bin_str.size() << "-byte binary string\n";
    std::cout << "   Time: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    std::cout << "   Hash: " << std::hash<std::string>{}(bin_str) << "\n";
    std::cout << "   Memory: Δ " << std::showpos << (after.rss_used - before.rss_used)
              << " MB\n" << std::noshowpos;
    
    bool intact = (memcmp(bin_str.data(), binary, binary_size) == 0);
    std::cout << "   Data integrity: " << (intact ? COLOR_GREEN "PASS" COLOR_RESET : COLOR_RED "FAIL" COLOR_RESET) << "\n";
    
    delete[] binary;
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << COLOR_BOLD "std::string STRESS TEST SUITE (FAIR COMPARISON)\n" COLOR_RESET;
    std::cout << "   Platform: ";
    
    #if defined(_WIN64)
        std::cout << "Windows x64\n";
    #elif defined(_WIN32)
        std::cout << "Windows x86\n";
    #elif defined(__APPLE__) && defined(__MACH__)
        std::cout << "macOS\n";
    #elif defined(__linux__)
        std::cout << "Linux\n";
    #else
        std::cout << "Unknown\n";
    #endif
    
    std::cout << "   String size: " << sizeof(std::string) << " bytes\n";
    std::cout << "   String_view size: " << sizeof(std::string_view) << " bytes\n";
    std::cout << "   SSO limit: ~15 bytes\n\n";
    
    bench_stress_alloc();
    bench_1m_strings();
    bench_arena_reuse();
    bench_concat_comparison();
    bench_huge_strings();
    bench_view_stress();
    bench_binary_stress();
    
    std::cout << COLOR_GREEN "\nAll stress tests completed!\n" COLOR_RESET;
    return 0;
}
