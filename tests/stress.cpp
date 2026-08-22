#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>
#include <iomanip>

#define CONCAT_ITERS 100000
#define STRINGS_INIT 1000000

// Timer using chrono (C++11)
double time_sec(void) {
    static auto start = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(now - start).count();
}

// Reset timer
void time_reset(void) {
    static auto& start = *new std::chrono::time_point<std::chrono::high_resolution_clock>();
    start = std::chrono::high_resolution_clock::now();
}

// Get elapsed time
double time_elapsed(void) {
    static auto start = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(now - start).count();
}

// Defining the arena for all devices
#if defined(__arm__) || defined(__aarch64__)
    // ARM-devices (Raspberry Pi, Android, iOS)
    #define ARENA_SIZE (512UL * 1024 * 1024) // 512 MB
#else
    // x86_64 / x86 - going all out
    #define ARENA_SIZE (2UL * 1024 * 1024 * 1024) // 2 GB
#endif

// Main tests
void bench_stress_alloc(void) {
    std::cout << "\nConsequent std::string allocation until the arena is full\n";
    std::cout << "   std::string size: " << sizeof(std::string) << " bytes\n";
    std::cout << "   Arena size: " << (ARENA_SIZE / (1024 * 1024)) << " MB\n";
    std::cout << "   Strings max: ~" << (ARENA_SIZE / sizeof(std::string)) << "\n";

    void* arena = malloc(ARENA_SIZE);
    if (arena == NULL) {
        std::cout << "   Failed to allocate the arena\n";
        return;
    }
    std::cout << "   Arena has been allocated\n";

    std::string* strings = (std::string*)arena;
    uint64_t count = 0;
    
    auto start = std::chrono::high_resolution_clock::now();

    const char* samples[] = {"Hello", "World!!!", "Pikachu, I choose you!"};
    int sample_count = sizeof(samples) / sizeof(samples[0]);

    while (count < ARENA_SIZE / sizeof(std::string) - 1) {
        int idx = count % sample_count;
        // Placement new to construct std::string in arena
        new (&strings[count]) std::string(samples[idx]);
        count++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    
    std::cout << "   Strings created: " << count << "\n";
    std::cout << "   Time: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    std::cout << "   Speed: " << std::fixed << std::setprecision(0) << (count / elapsed) << " strings per sec\n";

    // Free (call destructors)
    for (uint64_t i = 0; i < count; i++) {
        strings[i].~basic_string();
    }
    free(arena);
    std::cout << "   Arena has been freed\n";
}

// 1 million strings test
void bench_1m_strings(void) {
    std::cout << "\nTest: " << STRINGS_INIT << " std::string objects (short & long)\n";

    const char* short_str = "Hello";
    const char* long_str = "This is a long string for testing heap allocation.";

    std::vector<std::string> strings;
    strings.reserve(STRINGS_INIT);

    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < STRINGS_INIT; i++) {
        strings.emplace_back((i % 2 == 0) ? short_str : long_str);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    
    std::cout << "   " << STRINGS_INIT << " strings created\n";
    std::cout << "   Time: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    std::cout << "   Speed: " << std::fixed << std::setprecision(0) << (STRINGS_INIT / elapsed) << " strings per sec\n";

    // Free
    start = std::chrono::high_resolution_clock::now();
    strings.clear();
    end = std::chrono::high_resolution_clock::now();
    elapsed = std::chrono::duration<double>(end - start).count();
    
    std::cout << "   Freed: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
}

// Concatenation in a cycle
void bench_concat_stress(void) {
    std::cout << "\nConcatenation in a cycle of " << CONCAT_ITERS << " iterations\n";

    std::string s;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < CONCAT_ITERS; i++) {
        s.push_back('A' + (i % 26));
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    
    std::cout << "   String length: " << s.length() << "\n";
    std::cout << "   Time: " << std::fixed << std::setprecision(3) << elapsed << " sec\n";
    std::cout << "   Speed: " << std::fixed << std::setprecision(0) << (CONCAT_ITERS / elapsed) << " operations per sec\n";
}

// SSO comparison
void bench_sso_comparison(void) {
    std::cout << "\nSSO (Small String Optimization) comparison\n";
    
    const char* short_str = "Hello";
    const char* long_str = "This is a long string for testing heap allocation.";
    
    // Short string test
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; i++) {
        std::string s(short_str);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double short_time = std::chrono::duration<double>(end - start).count();
    
    // Long string test
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; i++) {
        std::string s(long_str);
    }
    end = std::chrono::high_resolution_clock::now();
    double long_time = std::chrono::duration<double>(end - start).count();
    
    std::cout << "   Short strings: " << std::fixed << std::setprecision(3) << short_time << " sec\n";
    std::cout << "   Long strings:  " << std::fixed << std::setprecision(3) << long_time << " sec\n";
    std::cout << "   Ratio: SSO is " << std::fixed << std::setprecision(1) << (long_time / short_time) << "x faster\n";
}

int main(void) {
    std::cout << "Stress-test of std::string (for comparison with dstring.h)\n";
    std::cout << "   Platform: ";

    #if defined(_WIN64)
        std::cout << "Windows x64\n";
    #elif defined(_WIN32)
        std::cout << "Windows x86\n";
    #elif defined(__APPLE__) && defined(__MACH__)
        std::cout << "macOS\n";
    #elif defined(__linux__)
        std::cout << "Linux\n";
    #elif defined(__FreeBSD__)
        std::cout << "FreeBSD\n";
    #elif defined(__arm__)
        std::cout << "ARM\n";
    #elif defined(__aarch64__)
        std::cout << "ARM64\n";
    #else
        std::cout << "Is unknown, but we're working anyway!\n";
    #endif

    std::cout << "   Compiler: ";
    #if defined(__GNUC__)
        std::cout << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "\n";
    #elif defined(__clang__)
        std::cout << "Clang " << __clang_major__ << "." << __clang_minor__ << "\n";
    #elif defined(_MSC_VER)
        std::cout << "MSVC " << _MSC_VER << "\n";
    #else
        std::cout << "Unknown\n";
    #endif

    std::cout << "   C++ Standard: ";
    #if __cplusplus >= 202002L
        std::cout << "C++20\n";
    #elif __cplusplus >= 201703L
        std::cout << "C++17\n";
    #elif __cplusplus >= 201402L
        std::cout << "C++14\n";
    #elif __cplusplus >= 201103L
        std::cout << "C++11\n";
    #else
        std::cout << "Pre-C++11\n";
    #endif

    bench_sso_comparison();
    bench_stress_alloc();
    bench_1m_strings();
    bench_concat_stress();

    std::cout << "\nAll stress tests completed\n";
    return 0;
}