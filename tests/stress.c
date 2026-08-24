#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <malloc.h>
#include "../dstring.h"

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
#define MASSIVE_STRINGS 10000000
#define HUGE_STRING_SIZE (10 * 1024 * 1024)  // 10 MB

// Timer
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

double time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Memory tracking
typedef struct {
    double heap_used;
    double rss_used;
} mem_stats;

mem_stats get_memory_stats(void) {
    mem_stats stats = {0};
    struct mallinfo mi = mallinfo();
    stats.heap_used = mi.uordblks / (1024.0 * 1024.0);
    
    FILE* fp = fopen("/proc/self/status", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                stats.rss_used = atol(line + 6) / 1024.0;
                break;
            }
        }
        fclose(fp);
    }
    return stats;
}

// Arena for all devices
#if defined(__arm__) || defined(__aarch64__)
    #define ARENA_SIZE (512UL * 1024 * 1024) // 512 MB
#else
    #define ARENA_SIZE (2UL * 1024 * 1024 * 1024) // 2 GB
#endif

// ============================================================================
// TEST 1: Consequent string allocation until arena is full
// ============================================================================

void bench_stress_alloc(void) {
    printf(COLOR_CYAN "\n=== TEST 1: MASSIVE STRING ALLOCATION ===\n" COLOR_RESET);
    printf("Consequent string allocation until the arena is full\n");
    printf("   String size: %zu bytes\n", sizeof(dstring));
    printf("   Arena size: %zu MB\n", ARENA_SIZE / (1024 * 1024));
    printf("   Strings max: ~%zu\n", ARENA_SIZE / sizeof(dstring));

    mem_stats before = get_memory_stats();
    void* arena = malloc(ARENA_SIZE);
    if (arena == NULL) {
        printf(COLOR_RED "   Failed to allocate the arena\n" COLOR_RESET);
        return;
    }
    printf("   Arena has been allocated\n");

    dstring* strings = (dstring*)arena;
    uint64_t count = 0;
    double start = time_sec();

    const char* samples[] = {"Hello", "World!!!", "Pikachu, I choose you!"};
    int sample_count = sizeof(samples) / sizeof(samples[0]);

    while (count < ARENA_SIZE / sizeof(dstring) - 1) {
        int idx = count % sample_count;
        strings[count] = ds_init(samples[idx]);
        if (!ds_ok(&strings[count])) break;
        count++;
    }

    double elapsed = time_sec() - start;
    mem_stats after = get_memory_stats();
    
    printf("   Strings created: %llu\n", (unsigned long long)count);
    printf("   Time: %.3f sec\n", elapsed);
    printf("   Speed: %.0f strings per sec\n", count / elapsed);
    printf("   Memory: Heap %.2f MB, RSS %.2f MB (Δ %+.2f MB)\n",
           after.heap_used, after.rss_used, after.rss_used - before.rss_used);

    // Free
    start = time_sec();
    for (uint64_t i = 0; i < count; i++) {
        ds_free(&strings[i]);
    }
    elapsed = time_sec() - start;
    printf("   Freed: %.3f sec\n", elapsed);
    
    free(arena);
    after = get_memory_stats();
    printf("   After cleanup: Heap %.2f MB, RSS %.2f MB\n", after.heap_used, after.rss_used);
}

// ============================================================================
// TEST 2: 1 million strings (short & long)
// ============================================================================

void bench_1m_strings(void) {
    printf(COLOR_CYAN "\n=== TEST 2: 1 MILLION STRINGS ===\n" COLOR_RESET);
    printf("Test: %d strings (short & long)\n", STRINGS_INIT);

    const char* short_str = "Hello";
    const char* long_str = "This is a long string for testing heap allocation.";

    mem_stats before = get_memory_stats();
    dstring* strings = (dstring*)malloc(STRINGS_INIT * sizeof(dstring));
    if (strings == NULL) {
        printf(COLOR_RED "   Failed to allocate memory\n" COLOR_RESET);
        return;
    }

    double start = time_sec();
    for (int i = 0; i < STRINGS_INIT; i++) {
        strings[i] = (i % 2 == 0) ? ds_init(short_str) : ds_init(long_str);
        if (!ds_ok(&strings[i])) {
            printf(COLOR_RED "   Error at string №%d\n" COLOR_RESET, i);
            break;
        }
    }
    double elapsed = time_sec() - start;
    mem_stats after = get_memory_stats();
    
    printf("   %d strings created\n", STRINGS_INIT);
    printf("   Time: %.3f sec\n", elapsed);
    printf("   Speed: %.0f strings per sec\n", STRINGS_INIT / elapsed);
    printf("   Memory: Heap %.2f MB, RSS %.2f MB (Δ %+.2f MB)\n",
           after.heap_used, after.rss_used, after.rss_used - before.rss_used);

    // Free
    start = time_sec();
    for (int i = 0; i < STRINGS_INIT; i++) {
        ds_free(&strings[i]);
    }
    elapsed = time_sec() - start;
    printf("   Freed: %.3f sec\n", elapsed);

    free(strings);
}

// ============================================================================
// TEST 3: Arena reuse (persistent buffer)
// ============================================================================

void bench_arena_reuse(void) {
    printf(COLOR_CYAN "\n=== TEST 3: ARENA REUSE ===\n" COLOR_RESET);
    printf("Arena reuse: %d cycles of append+clear\n", ARENA_REUSE_CYCLES);
    
    mem_stats before = get_memory_stats();
    dstring_arena arena = dsa_create(65536);
    
    double start = time_sec();
    for (int cycle = 0; cycle < ARENA_REUSE_CYCLES; cycle++) {
        dsa_clear(&arena);
        dsa_append(&arena, "Hello, World!");
        
        for (int i = 0; i < 1000; i++) {
            dsa_push(&arena, 'x');
        }
        
        // Process
        volatile char c = arena.data[0];
        (void)c;
    }
    double elapsed = time_sec() - start;
    mem_stats after = get_memory_stats();
    
    printf("   Cycles: %d\n", ARENA_REUSE_CYCLES);
    printf("   Total appends: %d\n", ARENA_REUSE_CYCLES * 1000);
    printf("   Time: %.3f sec\n", elapsed);
    printf("   Speed: %.0f appends per sec\n", (ARENA_REUSE_CYCLES * 1000.0) / elapsed);
    printf("   Capacity preserved: %u bytes\n", dsa_capacity(&arena));
    printf("   Memory: Heap %.2f MB, RSS %.2f MB (Δ %+.2f MB)\n",
           after.heap_used, after.rss_used, after.rss_used - before.rss_used);
    
    dsa_free(&arena);
}

// ============================================================================
// TEST 4: O(n²) vs O(1) - The Critical Comparison
// ============================================================================

void bench_concat_comparison(void) {
    printf(COLOR_CYAN "\n=== TEST 4: CONCATENATION STRATEGIES ===\n" COLOR_RESET);
    
    // 4a. Old method: ds_push (O(n²))
    printf("\n" COLOR_YELLOW "4a. ds_push (O(n²) - NOT RECOMMENDED):" COLOR_RESET "\n");
    mem_stats before = get_memory_stats();
    dstring s = ds_init("");
    double start = time_sec();

    for (int i = 0; i < CONCAT_ITERS; i++) {
        dstring tmp = ds_push(&s, 'A' + (i % 26));
        ds_free(&s);
        s = tmp;
        if (!ds_ok(&s)) {
            printf(COLOR_RED "   Error on iteration №%d\n" COLOR_RESET, i);
            break;
        }
    }

    double elapsed = time_sec() - start;
    mem_stats after = get_memory_stats();
    printf("   String length: %u\n", ds_len(&s));
    printf("   Time: %.3f sec\n", elapsed);
    printf("   Speed: %.0f operations per sec\n", CONCAT_ITERS / elapsed);
    printf("   Memory: Δ %+.2f MB\n", after.rss_used - before.rss_used);
    ds_free(&s);

    // 4b. New method: Arena (O(1))
    printf("\n" COLOR_GREEN "4b. Arena push (O(1) - RECOMMENDED):" COLOR_RESET "\n");
    before = get_memory_stats();
    dstring_arena arena = dsa_create(16);
    start = time_sec();

    for (int i = 0; i < CONCAT_ITERS; i++) {
        dsa_push(&arena, 'A' + (i % 26));
    }

    elapsed = time_sec() - start;
    after = get_memory_stats();
    printf("   String length: %u\n", dsa_len(&arena));
    printf("   Time: %.3f sec\n", elapsed);
    printf("   Speed: %.0f operations per sec\n", CONCAT_ITERS / elapsed);
    printf("   Capacity: %u, waste: %.2f%%\n", 
           dsa_capacity(&arena),
           (1.0 - (double)dsa_len(&arena) / dsa_capacity(&arena)) * 100);
    printf("   Memory: Δ %+.2f MB\n", after.rss_used - before.rss_used);
    
    // 4c. Arena pre-allocated (zero reallocations)
    printf("\n" COLOR_GREEN "4c. Arena pre-allocated (zero realloc):" COLOR_RESET "\n");
    before = get_memory_stats();
    dstring_arena arena_prealloc = dsa_create(CONCAT_ITERS + 1);
    start = time_sec();

    for (int i = 0; i < CONCAT_ITERS; i++) {
        dsa_push(&arena_prealloc, 'A' + (i % 26));
    }

    elapsed = time_sec() - start;
    after = get_memory_stats();
    printf("   String length: %u\n", dsa_len(&arena_prealloc));
    printf("   Time: %.3f sec\n", elapsed);
    printf("   Speed: %.0f operations per sec\n", CONCAT_ITERS / elapsed);
    printf("   Capacity: %u, waste: %.2f%%\n", 
           dsa_capacity(&arena_prealloc),
           (1.0 - (double)dsa_len(&arena_prealloc) / dsa_capacity(&arena_prealloc)) * 100);
    printf("   Memory: Δ %+.2f MB\n", after.rss_used - before.rss_used);
    
    dsa_free(&arena);
    dsa_free(&arena_prealloc);
}

// ============================================================================
// TEST 5: Huge string handling
// ============================================================================

void bench_huge_strings(void) {
    printf(COLOR_CYAN "\n=== TEST 5: HUGE STRING HANDLING ===\n" COLOR_RESET);
    
    // Create 10 MB string
    char* huge_data = malloc(HUGE_STRING_SIZE);
    if (!huge_data) {
        printf(COLOR_RED "   Failed to allocate source\n" COLOR_RESET);
        return;
    }
    memset(huge_data, 'x', HUGE_STRING_SIZE - 1);
    huge_data[HUGE_STRING_SIZE - 1] = '\0';
    
    mem_stats before = get_memory_stats();
    double start = time_sec();
    dstring huge = ds_init(huge_data);
    double elapsed = time_sec() - start;
    mem_stats after = get_memory_stats();
    
    printf("   Created %u-byte string\n", ds_len(&huge));
    printf("   Time: %.3f sec\n", elapsed);
    printf("   Hash: %u\n", ds_hash(&huge));
    printf("   Memory: Δ %+.2f MB\n", after.rss_used - before.rss_used);
    
    // Test operations on huge string
    start = time_sec();
    uint32_t hash = ds_hash(&huge);
    elapsed = time_sec() - start;
    printf("   Hash lookup: %.3f sec (cached: %u)\n", elapsed, hash);
    
    start = time_sec();
    int32_t pos = ds_find(&huge, 'y');
    elapsed = time_sec() - start;
    printf("   Find 'y': %.3f sec (position: %d)\n", elapsed, pos);
    
    ds_free(&huge);
    free(huge_data);
}

// ============================================================================
// TEST 6: View stress test
// ============================================================================

void bench_view_stress(void) {
    printf(COLOR_CYAN "\n=== TEST 6: VIEW STRESS TEST ===\n" COLOR_RESET);
    
    const int num_views = 1000000;
    dstring source = ds_init("The quick brown fox jumps over the lazy dog. This is a test string for view operations.");
    
    // Create many views
    mem_stats before = get_memory_stats();
    double start = time_sec();
    dstring_view* views = malloc(num_views * sizeof(dstring_view));
    
    for (int i = 0; i < num_views; i++) {
        views[i] = dsv_sub(&(dstring_view){ds_data(&source), ds_len(&source), 0}, 
                           i % 20, 15);
    }
    double elapsed = time_sec() - start;
    mem_stats after = get_memory_stats();
    
    printf("   Created %d views\n", num_views);
    printf("   Time: %.3f sec\n", elapsed);
    printf("   Speed: %.0f views per sec\n", num_views / elapsed);
    printf("   Memory: Heap %.2f MB (%.2f MB for views)\n", 
           after.heap_used - before.heap_used,
           (num_views * sizeof(dstring_view)) / (1024.0 * 1024.0));
    
    // Process all views
    start = time_sec();
    volatile uint32_t total_len = 0;
    for (int i = 0; i < num_views; i++) {
        total_len += views[i].len;
    }
    elapsed = time_sec() - start;
    printf("   Processed all views: %.3f sec (total len: %u)\n", elapsed, total_len);
    
    free(views);
    ds_free(&source);
}

// ============================================================================
// TEST 7: Binary data stress
// ============================================================================

void bench_binary_stress(void) {
    printf(COLOR_CYAN "\n=== TEST 7: BINARY DATA STRESS ===\n" COLOR_RESET);
    
    const int binary_size = 1024 * 1024;  // 1 MB of binary data
    char* binary = malloc(binary_size);
    
    // Fill with random binary data (including nulls)
    srand(42);
    for (int i = 0; i < binary_size; i++) {
        binary[i] = rand() % 256;  // Includes 0x00
    }
    
    mem_stats before = get_memory_stats();
    double start = time_sec();
    dstring bin_str = ds_init_len(binary, binary_size);
    double elapsed = time_sec() - start;
    mem_stats after = get_memory_stats();
    
    printf("   Created %u-byte binary string\n", ds_len(&bin_str));
    printf("   Time: %.3f sec\n", elapsed);
    printf("   Hash: %u\n", ds_hash(&bin_str));
    printf("   Memory: Δ %+.2f MB\n", after.rss_used - before.rss_used);
    
    // Verify data integrity
    bool intact = (memcmp(ds_data(&bin_str), binary, binary_size) == 0);
    printf("   Data integrity: %s\n", intact ? COLOR_GREEN "PASS" COLOR_RESET : COLOR_RED "FAIL" COLOR_RESET);
    
    ds_free(&bin_str);
    free(binary);
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf(COLOR_BOLD "dstring.h STRESS TEST SUITE\n" COLOR_RESET);
    printf("   Platform: ");

    #if defined(_WIN64)
        printf("Windows x64\n");
    #elif defined(_WIN32)
        printf("Windows x86\n");
    #elif defined(__APPLE__) && defined(__MACH__)
        printf("macOS\n");
    #elif defined(__linux__)
        printf("Linux\n");
    #elif defined(__FreeBSD__)
        printf("FreeBSD\n");
    #elif defined(__arm__)
        printf("ARM\n");
    #elif defined(__aarch64__)
        printf("ARM64\n");
    #else
        printf("Is unknown, but we're working anyway!\n");
    #endif
    
    printf("   String size: %zu bytes\n", sizeof(dstring));
    printf("   View size: %zu bytes\n", sizeof(dstring_view));
    printf("   Arena size: %zu bytes\n", sizeof(dstring_arena));
    printf("   SSO limit: %d bytes\n\n", STR_SSO_MAX + 1);
    
    bench_stress_alloc();
    bench_1m_strings();
    bench_arena_reuse();
    bench_concat_comparison();
    bench_huge_strings();
    bench_view_stress();
    bench_binary_stress();
    
    printf(COLOR_GREEN "\nAll stress tests completed!\n" COLOR_RESET);
    return 0;
}