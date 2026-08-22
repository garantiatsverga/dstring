#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <cstring>
#include <iomanip>

// Color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// Time
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

// Test result printer
void print_result(const char* test_name, double elapsed, double ops_per_sec, const char* status) {
    std::cout << "  " << std::left << std::setw(45) << test_name;
    if (strcmp(status, "PASS") == 0) {
        std::cout << COLOR_GREEN "[PASS]" COLOR_RESET;
    } else {
        std::cout << COLOR_RED "[FAIL]" COLOR_RESET;
    }
    std::cout << "  " << std::setw(8) << std::fixed << std::setprecision(3) << elapsed << " sec  ";
    std::cout << std::setw(12) << std::fixed << std::setprecision(0) << ops_per_sec << " ops/sec\n";
}

// SSO vs Heap allocation
void bench_sso_vs_heap() {
    std::cout << COLOR_CYAN "\nSSO vs HEAP ALLOCATION\n" COLOR_RESET;
    
    const std::string short_str = "Hello";
    const std::string long_str = "This is a long string for heap allocation test";
    const int iterations = 1000000;
    
    // SSO test
    Timer timer;
    for (int i = 0; i < iterations; i++) {
        std::string s = short_str;
    }
    double sso_time = timer.elapsed_sec();
    
    // Heap test
    timer.reset();
    for (int i = 0; i < iterations; i++) {
        std::string s = long_str;
    }
    double heap_time = timer.elapsed_sec();
    
    print_result("SSO (short string)", sso_time, iterations / sso_time, "PASS");
    print_result("Heap (long string)", heap_time, iterations / heap_time, "PASS");
    std::cout << "  Ratio: SSO is " << (heap_time / sso_time) << "x faster\n";
}

// size() vs strlen()
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
    const int iterations = 10000000;
    
    std::vector<std::string> strings;
    for (int i = 0; i < num_texts; i++) {
        strings.emplace_back(texts[i]);
    }
    
    // size() test (O(1))
    std::cout << "\n  " << COLOR_BOLD "std::string::size() (O(1)):" COLOR_RESET "\n";
    for (int i = 0; i < num_texts; i++) {
        Timer timer;
        volatile size_t len = 0;
        for (int j = 0; j < iterations; j++) {
            len = strings[i].size();
        }
        double elapsed = timer.elapsed_sec();
        print_result(texts[i], elapsed, iterations / elapsed, "PASS");
    }
    
    // strlen() test (O(n))
    std::cout << "\n  " << COLOR_BOLD "strlen (O(n)):" COLOR_RESET "\n";
    for (int i = 0; i < num_texts; i++) {
        const char* data = strings[i].c_str();
        Timer timer;
        volatile size_t len = 0;
        for (int j = 0; j < iterations; j++) {
            len = strlen(data);
        }
        double elapsed = timer.elapsed_sec();
        print_result(texts[i], elapsed, iterations / elapsed, "PASS");
    }
}

// Hash computation (no built-in hash)
void bench_hash() {
    std::cout << COLOR_CYAN "\nHASH COMPUTATION (custom FNV-1a)\n" COLOR_RESET;
    
    const char* texts[] = {"Hello", "World", "dstring", "benchmark", "performance"};
    int num_texts = sizeof(texts) / sizeof(texts[0]);
    const int iterations = 10000000;
    
    std::vector<std::string> strings;
    for (int i = 0; i < num_texts; i++) {
        strings.emplace_back(texts[i]);
    }
    
    // Simple FNV-1a hash
    auto hash_fn = [](const std::string& s) -> uint32_t {
        uint32_t hash = 0x811c9dc5;
        for (char c : s) {
            hash ^= (uint8_t)c;
            hash *= 0x01000193;
        }
        return hash;
    };
    
    for (int i = 0; i < num_texts; i++) {
        Timer timer;
        volatile uint32_t h = 0;
        for (int j = 0; j < iterations; j++) {
            h = hash_fn(strings[i]);
        }
        double elapsed = timer.elapsed_sec();
        print_result(texts[i], elapsed, iterations / elapsed, "PASS");
    }
}

// Concatenation stress
void bench_concat() {
    std::cout << COLOR_CYAN "\nCONCATENATION STRESS\n" COLOR_RESET;
    
    const int iterations = 100000;
    std::string s;
    s.reserve(iterations + 1);  // Pre-allocate to avoid reallocations
    
    Timer timer;
    for (int i = 0; i < iterations; i++) {
        char c = 'A' + (i % 26);
        s.push_back(c);
    }
    double elapsed = timer.elapsed_sec();
    
    std::cout << "  Final string length: " << s.size() << "\n";
    print_result("push_back operations", elapsed, iterations / elapsed, "PASS");
}

// Massive allocation
void bench_mass_alloc() {
    std::cout << COLOR_CYAN "\nMASSIVE ALLOCATION (adaptive)\n" COLOR_RESET;
    
    // Determine count based on available memory
    const size_t arena_size = 512UL * 1024 * 1024;  // 512 MB
    const size_t string_size = sizeof(std::string);  // 32 bytes on 64-bit
    const size_t max_count = arena_size / (string_size + 16);
    
    std::cout << "  Arena size: 512 MB\n";
    std::cout << "  Max strings: ~" << (max_count / 1000000) << " million\n";
    
    std::vector<std::string> strings;
    strings.reserve(max_count);
    
    Timer timer;
    for (size_t i = 0; i < max_count && i < 10000000; i++) {
        strings.emplace_back("Test string");
    }
    double elapsed = timer.elapsed_sec();
    
    std::cout << "  Strings created: " << strings.size() << "\n";
    print_result("mass allocation", elapsed, strings.size() / elapsed, "PASS");
}

// Copy vs clone
void bench_copy_vs_clone() {
    std::cout << COLOR_CYAN "\nCOPY vs CLONE\n" COLOR_RESET;
    
    const std::string original = "This is a test string for copying";
    const int iterations = 1000000;
    
    // Copy test (std::string copy constructor)
    Timer timer;
    for (int i = 0; i < iterations; i++) {
        std::string copy = original;
    }
    double copy_time = timer.elapsed_sec();
    
    // Manual copy test (using assign)
    timer.reset();
    for (int i = 0; i < iterations; i++) {
        std::string copy;
        copy.assign(original.data(), original.size());
    }
    double manual_time = timer.elapsed_sec();
    
    print_result("std::string copy", copy_time, iterations / copy_time, "PASS");
    print_result("manual assign", manual_time, iterations / manual_time, "PASS");
    std::cout << "  Ratio: copy is " << (manual_time / copy_time) << "x faster\n";
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
    std::cout << "  SSO limit: ~15 bytes (implementation dependent)\n";
    std::cout << COLOR_BOLD "\nRunning benchmarks...\n" COLOR_RESET;
    
    bench_sso_vs_heap();
    bench_len_comparison();
    bench_hash();
    bench_concat();
    bench_mass_alloc();
    bench_copy_vs_clone();
    
    std::cout << COLOR_GREEN "\nAll benchmarks completed\n" COLOR_RESET;
    return 0;
}