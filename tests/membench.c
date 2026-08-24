#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <malloc.h>
#include <unistd.h>
#include "../dstring.h"

// Color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// Timer (define before time.h if needed)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

double time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Memory tracking (using mallinfo instead of mallinfo2 for compatibility)
typedef struct {
    double heap_used;      // Heap memory in MB
    double total_used;     // RSS in MB
} mem_stats;

mem_stats get_memory_stats(void) {
    mem_stats stats = {0};
    
    // Get heap usage from mallinfo (more compatible than mallinfo2)
    struct mallinfo mi = mallinfo();
    stats.heap_used = mi.uordblks / (1024.0 * 1024.0);
    
    // Get process memory from /proc
    FILE* fp = fopen("/proc/self/status", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                // Parse RSS in kB
                long rss_kb = atol(line + 6);
                stats.total_used = rss_kb / 1024.0;
                break;
            }
        }
        fclose(fp);
    }
    
    return stats;
}

void print_memory(const char* label) {
    mem_stats stats = get_memory_stats();
    printf("  %-30s Heap: %8.2f MB | RSS: %8.2f MB\n", 
           label, stats.heap_used, stats.total_used);
}

// Test result printer with memory
void print_result_mem(const char* test_name, double elapsed, double ops_per_sec, 
                      mem_stats before, mem_stats after) {
    printf("  %-45s %8.3f sec  %12.0f ops/sec\n", test_name, elapsed, ops_per_sec);
    printf("    Memory: Heap %6.2f→%6.2f MB | RSS %6.2f→%6.2f MB | Δ %+.2f MB\n",
           before.heap_used, after.heap_used,
           before.total_used, after.total_used,
           after.total_used - before.total_used);
}

void print_header(const char* header) {
    printf(COLOR_CYAN "\n%s\n" COLOR_RESET, header);
    printf("  %-45s %8s  %12s\n", "Test", "Time", "Ops/Sec");
    printf("  %-45s %8s  %12s\n", "----", "----", "-------");
}

// ============================================================================
// DSTRING CREATION SCENARIOS WITH MEMORY
// ============================================================================

void bench_creation_memory(void) {
    print_header("DSTRING CREATION - MEMORY USAGE");
    
    const int iterations = 1000000;
    mem_stats before, after;
    
    // 1. Empty string
    before = get_memory_stats();
    double start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring s = ds_init("");
        ds_free(&s);
    }
    after = get_memory_stats();
    print_result_mem("Create empty string", time_sec() - start, 
                     iterations / (time_sec() - start), before, after);
    
    // 2. SSO string (14 chars)
    before = get_memory_stats();
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring s = ds_init("12345678901234");
        ds_free(&s);
    }
    after = get_memory_stats();
    print_result_mem("Create 14-char (SSO)", time_sec() - start, 
                     iterations / (time_sec() - start), before, after);
    
    // 3. Heap string (15 chars)
    before = get_memory_stats();
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring s = ds_init("123456789012345");
        ds_free(&s);
    }
    after = get_memory_stats();
    print_result_mem("Create 15-char (heap)", time_sec() - start, 
                     iterations / (time_sec() - start), before, after);
    
    // 4. Large heap string (1000 chars)
    char* str1000 = malloc(1001);
    memset(str1000, 'x', 1000);
    str1000[1000] = '\0';
    
    before = get_memory_stats();
    start = time_sec();
    for (int i = 0; i < iterations / 10; i++) {
        dstring s = ds_init(str1000);
        ds_free(&s);
    }
    after = get_memory_stats();
    print_result_mem("Create 1000-char (heap)", time_sec() - start, 
                     (iterations / 10) / (time_sec() - start), before, after);
    free(str1000);
}

// ============================================================================
// DSTRING OPERATION SCENARIOS WITH MEMORY
// ============================================================================

void bench_operations_memory(void) {
    print_header("DSTRING OPERATIONS - MEMORY USAGE");
    
    const int iterations = 1000000;
    mem_stats before, after;
    
    // Setup
    dstring sso_str = ds_init("Hello");
    dstring heap_str = ds_init("This is a much longer string that goes to the heap");
    
    // 1. Clone SSO (no heap allocation)
    before = get_memory_stats();
    double start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring clone = ds_clone(&sso_str);
        ds_free(&clone);
    }
    after = get_memory_stats();
    print_result_mem("Clone SSO (no heap)", time_sec() - start, 
                     iterations / (time_sec() - start), before, after);
    
    // 2. Clone heap (allocates)
    before = get_memory_stats();
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring clone = ds_clone(&heap_str);
        ds_free(&clone);
    }
    after = get_memory_stats();
    print_result_mem("Clone heap (allocates)", time_sec() - start, 
                     iterations / (time_sec() - start), before, after);
    
    // 3. Substring SSO
    before = get_memory_stats();
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring sub = ds_sub(&sso_str, 1, 3);
        ds_free(&sub);
    }
    after = get_memory_stats();
    print_result_mem("Substring SSO", time_sec() - start, 
                     iterations / (time_sec() - start), before, after);
    
    // 4. Substring heap
    before = get_memory_stats();
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring sub = ds_sub(&heap_str, 5, 10);
        ds_free(&sub);
    }
    after = get_memory_stats();
    print_result_mem("Substring heap", time_sec() - start, 
                     iterations / (time_sec() - start), before, after);
    
    ds_free(&sso_str);
    ds_free(&heap_str);
}

// ============================================================================
// DSTRING VIEW SCENARIOS WITH MEMORY
// ============================================================================

void bench_view_memory(void) {
    print_header("DSTRING_VIEW - MEMORY USAGE (Zero Allocation!)");
    
    const int iterations = 10000000;
    mem_stats before, after;
    
    dstring str = ds_init("The quick brown fox jumps over the lazy dog");
    dstring_view view = dsv_from_dstring(&str);
    
    // 1. View creation (zero allocation)
    before = get_memory_stats();
    double start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring_view v = dsv_from_dstring(&str);
        volatile const char* d = v.data;
        (void)d;
    }
    after = get_memory_stats();
    print_result_mem("Create views (no alloc)", time_sec() - start, 
                     iterations / (time_sec() - start), before, after);
    
    // 2. View substring (zero allocation)
    before = get_memory_stats();
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring_view sub = dsv_sub(&view, 4, 15);
        volatile uint32_t l = sub.len;
        (void)l;
    }
    after = get_memory_stats();
    print_result_mem("View substring (no alloc)", time_sec() - start, 
                     iterations / (time_sec() - start), before, after);
    
    // 3. Split views (zero allocation)
    before = get_memory_stats();
    start = time_sec();
    for (int i = 0; i < iterations / 10; i++) {
        dstring_view v = view;
        dstring_view token = dsv_split_at(&v, ' ');
        volatile uint32_t l = token.len;
        (void)l;
    }
    after = get_memory_stats();
    print_result_mem("Split views (no alloc)", time_sec() - start, 
                     (iterations / 10) / (time_sec() - start), before, after);
    
    ds_free(&str);
}

// ============================================================================
// DSTRING ARENA SCENARIOS WITH MEMORY
// ============================================================================

void bench_arena_memory(void) {
    print_header("DSTRING_ARENA - MEMORY USAGE");
    
    const int iterations = 100000;
    mem_stats before, after;
    
    // 1. Create arena (16 bytes)
    before = get_memory_stats();
    double start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring_arena arena = dsa_create(16);
        dsa_free(&arena);
    }
    after = get_memory_stats();
    print_result_mem("Create/free arena (16B)", time_sec() - start, 
                     iterations / (time_sec() - start), before, after);
    
    // 2. Create large arena (64KB)
    before = get_memory_stats();
    start = time_sec();
    for (int i = 0; i < iterations / 10; i++) {
        dstring_arena arena = dsa_create(65536);
        dsa_free(&arena);
    }
    after = get_memory_stats();
    print_result_mem("Create/free arena (64KB)", time_sec() - start, 
                     (iterations / 10) / (time_sec() - start), before, after);
    
    // 3. Arena reuse (no additional allocation)
    dstring_arena arena = dsa_create(65536);
    before = get_memory_stats();
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dsa_clear(&arena);
        dsa_append(&arena, "Hello, World!");
        for (int j = 0; j < 100; j++) {
            dsa_push(&arena, 'x');
        }
    }
    after = get_memory_stats();
    print_result_mem("Arena reuse (no realloc)", time_sec() - start, 
                     iterations / (time_sec() - start), before, after);
    
    // 4. Arena growth (with reallocation)
    dstring_arena growing = dsa_create(16);
    before = get_memory_stats();
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dsa_push(&growing, 'x');
    }
    after = get_memory_stats();
    print_result_mem("Arena growth (reallocs)", time_sec() - start, 
                     iterations / (time_sec() - start), before, after);
    
    printf("    Final arena capacity: %u bytes (%.2f MB)\n", 
           dsa_capacity(&growing), dsa_capacity(&growing) / (1024.0 * 1024.0));
    
    dsa_free(&arena);
    dsa_free(&growing);
}

// ============================================================================
// MASSIVE ALLOCATION WITH MEMORY TRACKING
// ============================================================================

void bench_mass_memory(void) {
    print_header("MASSIVE ALLOCATION - MEMORY TRACKING");
    
    const size_t max_strings = 1000000;  // 1 million strings
    mem_stats before, after;
    
    // Allocate array
    before = get_memory_stats();
    dstring* strings = malloc(max_strings * sizeof(dstring));
    printf("  Allocated array: %.2f MB for %zu dstring objects\n",
           (max_strings * sizeof(dstring)) / (1024.0 * 1024.0), max_strings);
    
    // Create strings
    double start = time_sec();
    for (size_t i = 0; i < max_strings; i++) {
        strings[i] = ds_init("Test string");
    }
    after = get_memory_stats();
    
    printf("  Created %zu strings\n", max_strings);
    printf("  Time: %.3f sec\n", time_sec() - start);
    printf("  Memory: Heap %.2f MB, RSS %.2f MB (Δ %+.2f MB)\n",
           after.heap_used, after.total_used, after.total_used - before.total_used);
    printf("  Per string: %.2f bytes heap\n", 
           ((after.heap_used - before.heap_used) * 1024 * 1024) / max_strings);
    
    // Free strings
    start = time_sec();
    for (size_t i = 0; i < max_strings; i++) {
        ds_free(&strings[i]);
    }
    printf("  Freed: %.3f sec\n", time_sec() - start);
    
    free(strings);
    after = get_memory_stats();
    printf("  After cleanup: Heap %.2f MB, RSS %.2f MB\n", 
           after.heap_used, after.total_used);
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf(COLOR_BOLD "\nDSTRING MEMORY-TRACKING BENCHMARK\n" COLOR_RESET);
    printf("  Testing memory usage for all scenarios\n");
    printf("  String size: %zu bytes\n", sizeof(dstring));
    printf("  View size: %zu bytes\n", sizeof(dstring_view));
    printf("  Arena size: %zu bytes\n", sizeof(dstring_arena));
    
    mem_stats initial = get_memory_stats();
    printf("\n  Initial memory: Heap %.2f MB, RSS %.2f MB\n\n", 
           initial.heap_used, initial.total_used);
    
    bench_creation_memory();
    bench_operations_memory();
    bench_view_memory();
    bench_arena_memory();
    bench_mass_memory();
    
    mem_stats final = get_memory_stats();
    printf(COLOR_GREEN "\nBenchmarks completed!\n" COLOR_RESET);
    printf("  Final memory: Heap %.2f MB, RSS %.2f MB\n", 
           final.heap_used, final.total_used);
    printf("  Total change: %+.2f MB\n", final.total_used - initial.total_used);
    
    return 0;
}