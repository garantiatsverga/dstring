#include <iostream>
#include <string>
#include <string_view>
#include <chrono>
#include <vector>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <algorithm>

// Color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// Timer class
class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    
    Timer() : start_(Clock::now()) {}
    
    double elapsed_sec() const {
        auto end = Clock::now();
        return std::chrono::duration<double>(end - start_).count();
    }
    
    void reset() { start_ = Clock::now(); }
    
private:
    TimePoint start_;
};

// Benchmark parameters
#define BENCH_SSO_ITERATIONS         1000000
#define BENCH_HEAP_ITERATIONS        1000000
#define BENCH_LEN_ITERATIONS         10000000
#define BENCH_HASH_ITERATIONS        10000000
#define BENCH_VIEW_ITERATIONS        10000000
#define BENCH_ARENA_ITERATIONS       100000
#define BENCH_CONCAT_ITERATIONS      100000
#define BENCH_COPY_ITERATIONS        1000000

// Test result printer
void print_result(const char* test_name, double elapsed, double ops_per_sec, const char* status = "PASS") {
    std::cout << "  " << std::left << std::setw(45) << test_name;
    if (strcmp(status, "PASS") == 0) {
        std::cout << COLOR_GREEN "[PASS]" COLOR_RESET;
    } else {
        std::cout << COLOR_RED "[FAIL]" COLOR_RESET;
    }
    std::cout << "  " << std::setw(8) << std::fixed << std::setprecision(3) << elapsed << " sec  ";
    std::cout << std::setw(12) << std::fixed << std::setprecision(0) << ops_per_sec << " ops/sec\n";
}

// 1. SSO vs Heap allocation
void bench_sso_vs_heap() {
    std::cout << COLOR_CYAN "\nSSO VS HEAP ALLOCATION\n" COLOR_RESET;
    
    const std::string short_str = "Hello";
    const std::string long_str = "This is a long string for heap allocation test";
    
    // SSO test
    Timer timer;
    for (int i = 0; i < BENCH_SSO_ITERATIONS; i++) {
        std::string s = short_str;
        volatile char c = s[0];
        (void)c;
    }
    double sso_time = timer.elapsed_sec();
    
    // Heap test
    timer.reset();
    for (int i = 0; i < BENCH_HEAP_ITERATIONS; i++) {
        std::string s = long_str;
        volatile char c = s[0];
        (void)c;
    }
    double heap_time = timer.elapsed_sec();
    
    print_result("SSO (short string)", sso_time, BENCH_SSO_ITERATIONS / sso_time);
    print_result("Heap (long string)", heap_time, BENCH_HEAP_ITERATIONS / heap_time);
    std::cout << "  Ratio: SSO is " << std::fixed << std::setprecision(1) 
              << (heap_time / sso_time) << "x faster\n";
}

// 2. Length access: size() vs strlen()
void bench_len_comparison() {
    std::cout << COLOR_CYAN "\nLENGTH ACCESS: O(1) vs O(n)\n" COLOR_RESET;
    
    const char* texts[] = {
        "Hi",
        "Hello",
        "Hello, World!",
        "This is a medium string.",
        "This is a quite long string for testing"
    };
    int num_texts = sizeof(texts) / sizeof(texts[0]);
    
    std::vector<std::string> strings;
    std::vector<std::string_view> views;
    for (int i = 0; i < num_texts; i++) {
        strings.emplace_back(texts[i]);
        views.emplace_back(strings[i]);
    }
    
    // size() test
    std::cout << "\n  " << COLOR_BOLD "std::string::size() (O(1)):" COLOR_RESET "\n";
    for (int i = 0; i < num_texts; i++) {
        Timer timer;
        volatile size_t len = 0;
        for (int j = 0; j < BENCH_LEN_ITERATIONS; j++) {
            len = strings[i].size();
        }
        double elapsed = timer.elapsed_sec();
        print_result(texts[i], elapsed, BENCH_LEN_ITERATIONS / elapsed);
    }
    
    // strlen() test
    std::cout << "\n  " << COLOR_BOLD "strlen (O(n)):" COLOR_RESET "\n";
    for (int i = 0; i < num_texts; i++) {
        const char* data = strings[i].c_str();
        Timer timer;
        volatile size_t len = 0;
        for (int j = 0; j < BENCH_LEN_ITERATIONS; j++) {
            len = strlen(data);
        }
        double elapsed = timer.elapsed_sec();
        print_result(texts[i], elapsed, BENCH_LEN_ITERATIONS / elapsed);
    }
    
    // string_view::size() test
    std::cout << "\n  " << COLOR_BOLD "std::string_view::size() (O(1)):" COLOR_RESET "\n";
    for (int i = 0; i < num_texts; i++) {
        Timer timer;
        volatile size_t len = 0;
        for (int j = 0; j < BENCH_LEN_ITERATIONS; j++) {
            len = views[i].size();
        }
        double elapsed = timer.elapsed_sec();
        print_result(texts[i], elapsed, BENCH_LEN_ITERATIONS / elapsed);
    }
}

// 3. Hash computation
void bench_hash() {
    std::cout << COLOR_CYAN "\nHASH COMPUTATION\n" COLOR_RESET;
    
    const char* texts[] = {"Hello", "World", "dstring", "benchmark", "performance"};
    int num_texts = sizeof(texts) / sizeof(texts[0]);
    
    std::vector<std::string> strings;
    for (int i = 0; i < num_texts; i++) {
        strings.emplace_back(texts[i]);
    }
    
    // std::hash<std::string>
    std::cout << "\n  " << COLOR_BOLD "std::hash<std::string>:" COLOR_RESET "\n";
    for (int i = 0; i < num_texts; i++) {
        Timer timer;
        volatile size_t h = 0;
        for (int j = 0; j < BENCH_HASH_ITERATIONS; j++) {
            h = std::hash<std::string>{}(strings[i]);
        }
        double elapsed = timer.elapsed_sec();
        print_result(texts[i], elapsed, BENCH_HASH_ITERATIONS / elapsed);
    }
    
    // Custom FNV-1a (same as C version)
    auto fnv1a = [](std::string_view s) -> uint32_t {
        uint32_t hash = 2166136261u;
        for (char c : s) {
            hash ^= (uint8_t)c;
            hash *= 16777619u;
        }
        return hash & 0x7FFFFFFF;
    };
    
    std::cout << "\n  " << COLOR_BOLD "Custom FNV-1a:" COLOR_RESET "\n";
    for (int i = 0; i < num_texts; i++) {
        Timer timer;
        volatile uint32_t h = 0;
        for (int j = 0; j < BENCH_HASH_ITERATIONS; j++) {
            h = fnv1a(strings[i]);
        }
        double elapsed = timer.elapsed_sec();
        print_result(texts[i], elapsed, BENCH_HASH_ITERATIONS / elapsed);
    }
}

// 4. String_view operations
void bench_views() {
    std::cout << COLOR_CYAN "\nSTRING_VIEW OPERATIONS\n" COLOR_RESET;
    
    std::string s = "The quick brown fox jumps over the lazy dog";
    std::string_view view(s);
    
    // Substring creation (O(1))
    Timer timer;
    for (int i = 0; i < BENCH_VIEW_ITERATIONS; i++) {
        std::string_view sub = view.substr(4, 15);
        volatile size_t len = sub.size();
        (void)len;
    }
    double sub_time = timer.elapsed_sec();
    print_result("View substring (O(1))", sub_time, BENCH_VIEW_ITERATIONS / sub_time);
    
    // std::string substring (allocates)
    timer.reset();
    for (int i = 0; i < BENCH_VIEW_ITERATIONS / 100; i++) {
        std::string sub = s.substr(4, 15);
        volatile size_t len = sub.size();
        (void)len;
    }
    double string_sub_time = timer.elapsed_sec();
    print_result("std::string substring (allocates)", string_sub_time, 
                 (BENCH_VIEW_ITERATIONS / 100) / string_sub_time);
    
    std::cout << "  Ratio: View substring is " << std::fixed << std::setprecision(0)
              << ((string_sub_time / (BENCH_VIEW_ITERATIONS / 100)) / 
                  (sub_time / BENCH_VIEW_ITERATIONS)) << "x faster\n";
    
    // Search
    timer.reset();
    for (int i = 0; i < BENCH_VIEW_ITERATIONS; i++) {
        volatile size_t pos = view.find('j');
        (void)pos;
    }
    double find_time = timer.elapsed_sec();
    print_result("View find", find_time, BENCH_VIEW_ITERATIONS / find_time);
}

// 5. Arena-like pattern (using reserve + clear)
void bench_arena_pattern() {
    std::cout << COLOR_CYAN "\nARENA-LIKE PATTERN (reserve + clear)\n" COLOR_RESET;
    
    // Test 1: With reserve (arena-like)
    std::string arena;
    arena.reserve(65536);
    
    Timer timer;
    for (int cycle = 0; cycle < BENCH_ARENA_ITERATIONS; cycle++) {
        arena.clear();  // Keeps capacity!
        arena += "Hello, World!";
        for (int i = 0; i < 100; i++) {
            arena.push_back('x');
        }
    }
    double arena_time = timer.elapsed_sec();
    print_result("String reuse (100K cycles)", arena_time, 
                 BENCH_ARENA_ITERATIONS / arena_time);
    
    // Test 2: Without reserve (reallocation)
    timer.reset();
    for (int cycle = 0; cycle < BENCH_ARENA_ITERATIONS / 100; cycle++) {
        std::string s = "Hello, World!";
        for (int i = 0; i < 100; i++) {
            s.push_back('x');
        }
    }
    double no_reserve_time = timer.elapsed_sec();
    print_result("String without reserve (1K cycles)", no_reserve_time, 
                 (BENCH_ARENA_ITERATIONS / 100) / no_reserve_time);
    
    std::cout << "  Ratio: Reuse is " << std::fixed << std::setprecision(0)
              << ((no_reserve_time / (BENCH_ARENA_ITERATIONS / 100)) / 
                  (arena_time / BENCH_ARENA_ITERATIONS)) << "x faster\n";
}

// 6. Concatenation stress
void bench_concat() {
    std::cout << COLOR_CYAN "\nCONCATENATION STRESS\n" COLOR_RESET;
    
    std::string s;
    s.reserve(BENCH_CONCAT_ITERATIONS + 1);  // Pre-allocate
    
    Timer timer;
    for (int i = 0; i < BENCH_CONCAT_ITERATIONS; i++) {
        char c = 'A' + (i % 26);
        s.push_back(c);
    }
    double elapsed = timer.elapsed_sec();
    
    std::cout << "  Final string length: " << s.size() << "\n";
    print_result("push_back operations (reserved)", elapsed, BENCH_CONCAT_ITERATIONS / elapsed);
}

// 7. Massive allocation
void bench_mass_alloc() {
    std::cout << COLOR_CYAN "\nMASSIVE ALLOCATION\n" COLOR_RESET;
    
    const size_t max_strings = 10000000;  // 10 million strings
    std::vector<std::string> strings;
    strings.reserve(max_strings);
    
    Timer timer;
    for (size_t i = 0; i < max_strings; i++) {
        strings.emplace_back("Test string");
    }
    double elapsed = timer.elapsed_sec();
    
    std::cout << "  Strings created: " << strings.size() << "\n";
    std::cout << "  Memory used: " << (strings.size() * sizeof(std::string)) / (1024.0 * 1024.0) 
              << " MB for string objects\n";
    print_result("mass allocation", elapsed, strings.size() / elapsed);
}

// 8. Copy vs string_view
void bench_copy_vs_view() {
    std::cout << COLOR_CYAN "\nCOPY vs STRING_VIEW\n" COLOR_RESET;
    
    const std::string original = "This is a test string for copying";
    
    // Copy test
    Timer timer;
    for (int i = 0; i < BENCH_COPY_ITERATIONS; i++) {
        std::string copy = original;
        volatile size_t len = copy.size();
        (void)len;
    }
    double copy_time = timer.elapsed_sec();
    
    // String_view creation (no copy!)
    timer.reset();
    for (int i = 0; i < BENCH_COPY_ITERATIONS * 10; i++) {
        std::string_view view = original;
        volatile size_t len = view.size();
        (void)len;
    }
    double view_time = timer.elapsed_sec();
    
    print_result("std::string copy", copy_time, BENCH_COPY_ITERATIONS / copy_time);
    print_result("string_view creation (no copy)", view_time, 
                 (BENCH_COPY_ITERATIONS * 10) / view_time);
    std::cout << "  Ratio: View creation is " << std::fixed << std::setprecision(0)
              << ((copy_time / BENCH_COPY_ITERATIONS) / 
                  (view_time / (BENCH_COPY_ITERATIONS * 10))) << "x faster\n";
}

// Main
int main() {
    std::cout << COLOR_BOLD "std::string PERFORMANCE BENCHMARK\n" COLOR_RESET;
    
    // System info
    #if defined(__linux__)
        std::cout << "  OS: Linux\n";
    #elif defined(_WIN64)
        std::cout << "  OS: Windows x64\n";
    #elif defined(__APPLE__)
        std::cout << "  OS: macOS\n";
    #else
        std::cout << "  OS: Unknown\n";
    #endif
    
    #if defined(__x86_64__)
        std::cout << "  Arch: x86_64\n";
    #elif defined(__arm__) || defined(__aarch64__)
        std::cout << "  Arch: ARM\n";
    #else
        std::cout << "  Arch: Unknown\n";
    #endif
    
    std::cout << "  String size: " << sizeof(std::string) << " bytes\n";
    std::cout << "  String_view size: " << sizeof(std::string_view) << " bytes\n";
    std::cout << "  SSO limit: ~15 bytes (implementation dependent)\n";
    std::cout << COLOR_BOLD "\nRunning benchmarks...\n" COLOR_RESET;
    
    bench_sso_vs_heap();
    bench_len_comparison();
    bench_hash();
    bench_views();
    bench_arena_pattern();
    bench_concat();
    bench_mass_alloc();
    bench_copy_vs_view();
    
    std::cout << COLOR_GREEN "\nAll benchmarks completed\n" COLOR_RESET;
    return 0;
}