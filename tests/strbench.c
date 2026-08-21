#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "../dstring.h"

// BENCHMARK PARAMETERS
#define BENCH_SSO_ITERATIONS         1000000
#define BENCH_HEAP_ITERATIONS        1000000
#define BENCH_LEN_ITERATIONS         10000000
#define BENCH_HASH_ITERATIONS        10000000
#define BENCH_CONCAT_ITERATIONS      100000
#define BENCH_COPY_ITERATIONS        1000000
#define BENCH_MASS_ALLOC_MULTIPLIER  8  // adaptive arena size

// Color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// Timer
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    double time_sec(void) {
        LARGE_INTEGER freq, counter;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&counter);
        return (double)counter.QuadPart / freq.QuadPart;
    }
#elif defined(__APPLE__) && defined(__MACH__)
    #include <mach/mach_time.h>
    double time_sec(void) {
        static mach_timebase_info_data_t info = {0};
        if (info.denom == 0) mach_timebase_info(&info);
        uint64_t time = mach_absolute_time();
        return (double)(time * info.numer) / (info.denom * 1e9);
    }
#else
    #define _POSIX_C_SOURCE 199309L
    #include <time.h>
    double time_sec(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec + ts.tv_nsec / 1e9;
    }
#endif

// Test result printer
void print_result(const char* test_name, double elapsed, double ops_per_sec, const char* status) {
    printf("  %-45s ", test_name);
    if (strcmp(status, "PASS") == 0) {
        printf(COLOR_GREEN "[PASS]" COLOR_RESET);
    } else if (strcmp(status, "FAIL") == 0) {
        printf(COLOR_RED "[FAIL]" COLOR_RESET);
    } else {
        printf(COLOR_YELLOW "[WARN]" COLOR_RESET);
    }
    printf("  %8.3f sec  %12.0f ops/sec\n", elapsed, ops_per_sec);
}

// SSO vs heap allocation
void bench_sso_vs_heap(void) {
    printf(COLOR_CYAN "\nSSO VS HEAP ALLOCATION\n" COLOR_RESET);
    
    const char* short_str = "Hello";
    const char* long_str = "This is a long string for heap allocation test";
    
    // SSO test
    double start = time_sec();
    for (int i = 0; i < BENCH_SSO_ITERATIONS; i++) {
        string s = initstr(short_str);
        freestr(&s);
    }
    double sso_time = time_sec() - start;
    
    // Heap test
    start = time_sec();
    for (int i = 0; i < BENCH_HEAP_ITERATIONS; i++) {
        string s = initstr(long_str);
        freestr(&s);
    }
    double heap_time = time_sec() - start;
    
    print_result("SSO (short string)", sso_time, BENCH_SSO_ITERATIONS / sso_time, "PASS");
    print_result("Heap (long string)", heap_time, BENCH_HEAP_ITERATIONS / heap_time, "PASS");
    printf("  Ratio: SSO is %.1fx faster\n", heap_time / sso_time);
}

// strlen_s() vs strlen()
void bench_len_comparison(void) {
    printf(COLOR_CYAN "\nLENGTH ACCESS: O(1) vs O(n)\n" COLOR_RESET);
    
    const char* texts[] = {
        "Hi",
        "Hello",
        "Hello, World!",
        "This is a medium string.",
        "This is a quite long string for testing"
    };
    int num_texts = sizeof(texts) / sizeof(texts[0]);
    
    // Create strings once
    string strings[5];
    for (int i = 0; i < num_texts; i++) {
        strings[i] = initstr(texts[i]);
    }
    
    // strlen_s() test
    printf("\n  " COLOR_BOLD "strlen_s (O(1)):" COLOR_RESET "\n");
    for (int i = 0; i < num_texts; i++) {
        double start = time_sec();
        volatile uint32_t len = 0;
        for (int j = 0; j < BENCH_LEN_ITERATIONS; j++) {
            len = strlen_s(&strings[i]);
        }
        double elapsed = time_sec() - start;
        print_result(texts[i], elapsed, BENCH_LEN_ITERATIONS / elapsed, "PASS");
    }
    
    // strlen() test
    printf("\n  " COLOR_BOLD "strlen (O(n)):" COLOR_RESET "\n");
    for (int i = 0; i < num_texts; i++) {
        const char* data = strdata(&strings[i]);
        double start = time_sec();
        volatile size_t len = 0;
        for (int j = 0; j < BENCH_LEN_ITERATIONS; j++) {
            len = strlen(data);
        }
        double elapsed = time_sec() - start;
        print_result(texts[i], elapsed, BENCH_LEN_ITERATIONS / elapsed, "PASS");
    }
    
    // Cleanup
    for (int i = 0; i < num_texts; i++) {
        freestr(&strings[i]);
    }
}

// strhash() performance
void bench_hash(void) {
    printf(COLOR_CYAN "\nHASH COMPUTATION\n" COLOR_RESET);
    
    const char* texts[] = {"Hello", "World", "dstring", "benchmark", "performance"};
    int num_texts = sizeof(texts) / sizeof(texts[0]);
    
    string strings[5];
    for (int i = 0; i < num_texts; i++) {
        strings[i] = initstr(texts[i]);
    }
    
    for (int i = 0; i < num_texts; i++) {
        double start = time_sec();
        volatile uint32_t hash = 0;
        for (int j = 0; j < BENCH_HASH_ITERATIONS; j++) {
            hash = strhash(&strings[i]);
        }
        double elapsed = time_sec() - start;
        print_result(texts[i], elapsed, BENCH_HASH_ITERATIONS / elapsed, "PASS");
    }
    
    for (int i = 0; i < num_texts; i++) {
        freestr(&strings[i]);
    }
}

// Concatenation stress
void bench_concat(void) {
    printf(COLOR_CYAN "\nCONCATENATION STRESS\n" COLOR_RESET);
    
    string s = initstr("");
    
    double start = time_sec();
    for (int i = 0; i < BENCH_CONCAT_ITERATIONS; i++) {
        char c = 'A' + (i % 26);
        string tmp = strpush(&s, c);
        freestr(&s);
        s = tmp;
        if (!strok(&s)) {
            print_result("concat loop", 0.0, 0.0, "FAIL");
            freestr(&s);
            return;
        }
    }
    double elapsed = time_sec() - start;
    
    printf("  Final string length: %u\n", strlen_s(&s));
    print_result("push operations", elapsed, BENCH_CONCAT_ITERATIONS / elapsed, "PASS");
    
    freestr(&s);
}

// Massive allocation test
void bench_mass_alloc(void) {
    printf(COLOR_CYAN "\nMASSIVE ALLOCATION\n" COLOR_RESET);
    
    // Determine arena size based on available memory
    size_t arena_size;
    #if defined(__arm__) || defined(__aarch64__)
        arena_size = 128UL * 1024 * 1024;  // 128 MB
    #else
        arena_size = 512UL * 1024 * 1024;  // 512 MB
    #endif
    
    printf("  Arena size: %zu MB\n", arena_size / (1024 * 1024));
    
    void* arena = malloc(arena_size);
    if (arena == NULL) {
        printf(COLOR_RED "  [FAIL] Cannot allocate arena\n" COLOR_RESET);
        return;
    }
    
    string* strings = (string*)arena;
    uint64_t count = 0;
    const char* sample = "Test string";
    
    double start = time_sec();
    while (count < arena_size / sizeof(string) - 1) {
        strings[count] = initstr(sample);
        if (!strok(&strings[count])) break;
        count++;
    }
    double elapsed = time_sec() - start;
    
    printf("  Strings created: %llu\n", (unsigned long long)count);
    print_result("mass allocation", elapsed, count / elapsed, "PASS");
    
    // Cleanup
    for (uint64_t i = 0; i < count; i++) {
        freestr(&strings[i]);
    }
    free(arena);
}

// Copy vs clone
void bench_copy_vs_clone(void) {
    printf(COLOR_CYAN "\nCOPY vs CLONE\n" COLOR_RESET);
    
    const char* source = "This is a test string for copying";
    
    // Clone test
    string original = initstr(source);
    double start = time_sec();
    for (int i = 0; i < BENCH_COPY_ITERATIONS; i++) {
        string copy = strclone(&original);
        freestr(&copy);
    }
    double clone_time = time_sec() - start;
    
    // Manual copy test (using initstr_len)
    start = time_sec();
    for (int i = 0; i < BENCH_COPY_ITERATIONS; i++) {
        string copy = initstr_len(strdata(&original), strlen_s(&original));
        freestr(&copy);
    }
    double manual_time = time_sec() - start;
    
    print_result("strclone()", clone_time, BENCH_COPY_ITERATIONS / clone_time, "PASS");
    print_result("manual copy", manual_time, BENCH_COPY_ITERATIONS / manual_time, "PASS");
    printf("  Ratio: strclone is %.1fx faster\n", manual_time / clone_time);
    
    freestr(&original);
}

// Main
int main(void) {
    printf(COLOR_BOLD "dstring.h PERFORMANCE BENCHMARK\n" COLOR_RESET);
    
    // System info
    #if defined(__linux__)
        printf("  OS: Linux\n");
    #elif defined(_WIN64)
        printf("  OS: Windows x64\n");
    #elif defined(_WIN32)
        printf("  OS: Windows x86\n");
    #elif defined(__APPLE__) && defined(__MACH__)
        printf("  OS: macOS\n");
    #else
        printf("  OS: Is unknown, but we work anyway! =D\n");
    #endif
    
    #if defined(__x86_64__)
        printf("  Arch: x86_64\n");
    #elif defined(__arm__)
        printf("  Arch: ARM\n");
    #elif defined(__aarch64__)
        printf("  Arch: ARM64\n");
    #else
        printf("  Arch: Is unknown, but we work anyway! =D\n");
    #endif
    
    printf("  String size: %zu bytes\n", sizeof(string));
    printf("  SSO limit: %d bytes\n", STR_SSO_MAX + 1);
    printf("  Iterations: SSO=%d, Heap=%d, Len=%d, Hash=%d, Concat=%d, Copy=%d\n",
           BENCH_SSO_ITERATIONS, BENCH_HEAP_ITERATIONS, BENCH_LEN_ITERATIONS,
           BENCH_HASH_ITERATIONS, BENCH_CONCAT_ITERATIONS, BENCH_COPY_ITERATIONS);
    printf(COLOR_BOLD "\nRunning benchmarks...\n" COLOR_RESET);
    
    bench_sso_vs_heap();
    bench_len_comparison();
    bench_hash();
    bench_concat();
    bench_mass_alloc();
    bench_copy_vs_clone();
    
    printf(COLOR_GREEN "\nAll benchmarks completed\n" COLOR_RESET);
    return 0;
}