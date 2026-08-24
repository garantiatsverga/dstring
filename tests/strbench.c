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
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

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

void print_result(const char* test_name, double elapsed, double ops_per_sec) {
    printf("  %-50s %8.3f sec  %12.0f ops/sec\n", test_name, elapsed, ops_per_sec);
}

void print_result_mem(const char* test_name, double elapsed, double ops_per_sec,
                      mem_stats before, mem_stats after) {
    printf("  %-50s %8.3f sec  %12.0f ops/sec\n", test_name, elapsed, ops_per_sec);
    printf("    Memory: Heap %6.2f→%6.2f MB | RSS %6.2f→%6.2f MB | Δ %+.2f MB\n",
           before.heap_used, after.heap_used,
           before.rss_used, after.rss_used,
           after.rss_used - before.rss_used);
}

void print_header(const char* header) {
    printf(COLOR_CYAN "\n%s\n" COLOR_RESET, header);
    printf("  %-50s %8s  %12s\n", "Test", "Time", "Ops/Sec");
    printf("  %-50s %8s  %12s\n", "----", "----", "-------");
}

// ============================================================================
// 1. DSTRING CREATION SCENARIOS (from previous benchmark)
// ============================================================================

void bench_creation_scenarios(void) {
    print_header("DSTRING CREATION SCENARIOS");
    
    const int iterations = 1000000;
    
    // 1. Empty string
    double start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring s = ds_init("");
        ds_free(&s);
    }
    print_result("Create empty string", time_sec() - start, iterations / (time_sec() - start));
    
    // 2. 1-char string
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring s = ds_init("A");
        ds_free(&s);
    }
    print_result("Create 1-char string", time_sec() - start, iterations / (time_sec() - start));
    
    // 3. 7-char string (half SSO)
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring s = ds_init("Hello!!");
        ds_free(&s);
    }
    print_result("Create 7-char string", time_sec() - start, iterations / (time_sec() - start));
    
    // 4. 14-char string (max SSO)
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring s = ds_init("12345678901234");
        ds_free(&s);
    }
    print_result("Create 14-char string (max SSO)", time_sec() - start, iterations / (time_sec() - start));
    
    // 5. 15-char string (min heap)
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring s = ds_init("123456789012345");
        ds_free(&s);
    }
    print_result("Create 15-char string (min heap)", time_sec() - start, iterations / (time_sec() - start));
    
    // 6. 100-char string (heap)
    const char* str100 = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
    start = time_sec();
    for (int i = 0; i < iterations / 10; i++) {
        dstring s = ds_init(str100);
        ds_free(&s);
    }
    print_result("Create 100-char string", time_sec() - start, (iterations / 10) / (time_sec() - start));
    
    // 7. 1000-char string (large heap)
    char* str1000 = malloc(1001);
    memset(str1000, 'x', 1000);
    str1000[1000] = '\0';
    start = time_sec();
    for (int i = 0; i < iterations / 100; i++) {
        dstring s = ds_init(str1000);
        ds_free(&s);
    }
    print_result("Create 1000-char string", time_sec() - start, (iterations / 100) / (time_sec() - start));
    free(str1000);
}

// ============================================================================
// 2. DSTRING OPERATION SCENARIOS (from previous benchmark)
// ============================================================================

void bench_operation_scenarios(void) {
    print_header("DSTRING OPERATION SCENARIOS");
    
    const int iterations = 1000000;
    
    // Setup test strings
    dstring sso_str = ds_init("Hello");
    dstring heap_str = ds_init("This is a much longer string that goes to the heap");
    
    // 1. Length access
    double start = time_sec();
    volatile uint32_t len = 0;
    for (int i = 0; i < iterations * 10; i++) {
        len += ds_len(&sso_str);
        len += ds_len(&heap_str);
    }
    print_result("Length access (SSO + heap)", time_sec() - start, (iterations * 20) / (time_sec() - start));
    
    // 2. Data access
    start = time_sec();
    volatile const char* data;
    for (int i = 0; i < iterations * 10; i++) {
        data = ds_data(&sso_str);
        data = ds_data(&heap_str);
    }
    print_result("Data access", time_sec() - start, (iterations * 20) / (time_sec() - start));
    
    // 3. Empty check
    start = time_sec();
    volatile bool empty;
    for (int i = 0; i < iterations * 10; i++) {
        empty = ds_empty(&sso_str);
        empty = ds_empty(&heap_str);
    }
    print_result("Empty check", time_sec() - start, (iterations * 20) / (time_sec() - start));
    
    // 4. Hash computation (SSO - always computes)
    start = time_sec();
    volatile uint32_t hash;
    for (int i = 0; i < iterations; i++) {
        hash = ds_hash(&sso_str);
    }
    print_result("Hash SSO (recomputes)", time_sec() - start, iterations / (time_sec() - start));
    
    // 5. Hash computation (heap - cached)
    start = time_sec();
    for (int i = 0; i < iterations * 10; i++) {
        hash = ds_hash(&heap_str);
    }
    print_result("Hash heap (cached)", time_sec() - start, (iterations * 10) / (time_sec() - start));
    
    // 6. Clone SSO
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring clone = ds_clone(&sso_str);
        ds_free(&clone);
    }
    print_result("Clone SSO", time_sec() - start, iterations / (time_sec() - start));
    
    // 7. Clone heap
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring clone = ds_clone(&heap_str);
        ds_free(&clone);
    }
    print_result("Clone heap", time_sec() - start, iterations / (time_sec() - start));
    
    // 8. Substring SSO
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring sub = ds_sub(&sso_str, 1, 3);
        ds_free(&sub);
    }
    print_result("Substring SSO", time_sec() - start, iterations / (time_sec() - start));
    
    // 9. Substring heap
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring sub = ds_sub(&heap_str, 5, 10);
        ds_free(&sub);
    }
    print_result("Substring heap", time_sec() - start, iterations / (time_sec() - start));
    
    // 10. Contains char
    start = time_sec();
    volatile bool found;
    for (int i = 0; i < iterations; i++) {
        found = ds_contains(&heap_str, 'z');
    }
    print_result("Contains char", time_sec() - start, iterations / (time_sec() - start));
    
    // 11. Find char
    start = time_sec();
    volatile int32_t pos;
    for (int i = 0; i < iterations; i++) {
        pos = ds_find(&heap_str, 'l');
    }
    print_result("Find char", time_sec() - start, iterations / (time_sec() - start));
    
    // 12. Trim
    dstring padded = ds_init("   Hello World   ");
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring trimmed = ds_trim(&padded);
        ds_free(&trimmed);
    }
    print_result("Trim", time_sec() - start, iterations / (time_sec() - start));
    ds_free(&padded);
    
    ds_free(&sso_str);
    ds_free(&heap_str);
}

// ============================================================================
// 3. DSTRING VIEW SCENARIOS (from previous benchmark)
// ============================================================================

void bench_view_scenarios(void) {
    print_header("DSTRING_VIEW SCENARIOS");
    
    const int iterations = 10000000;
    
    dstring str = ds_init("The quick brown fox jumps over the lazy dog");
    dstring_view view = dsv_from_dstring(&str);
    
    // 1. View creation from dstring
    double start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring_view v = dsv_from_dstring(&str);
        volatile const char* d = v.data;
        (void)d;
    }
    print_result("Create view from dstring", time_sec() - start, iterations / (time_sec() - start));
    
    // 2. View from C string
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring_view v = dsv_from_cstr("Hello, World!");
        volatile uint32_t l = v.len;
        (void)l;
    }
    print_result("Create view from C string", time_sec() - start, iterations / (time_sec() - start));
    
    // 3. Substring (O(1))
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring_view sub = dsv_sub(&view, 4, 15);
        volatile uint32_t l = sub.len;
        (void)l;
    }
    print_result("View substring", time_sec() - start, iterations / (time_sec() - start));
    
    // 4. Remove prefix
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring_view v = view;
        dsv_remove_prefix(&v, 4);
        volatile const char* d = v.data;
        (void)d;
    }
    print_result("Remove prefix", time_sec() - start, iterations / (time_sec() - start));
    
    // 5. Remove suffix
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring_view v = view;
        dsv_remove_suffix(&v, 4);
        volatile uint32_t l = v.len;
        (void)l;
    }
    print_result("Remove suffix", time_sec() - start, iterations / (time_sec() - start));
    
    // 6. Find char
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        volatile int32_t p = dsv_find(&view, 'j');
        (void)p;
    }
    print_result("Find char", time_sec() - start, iterations / (time_sec() - start));
    
    // 7. Hash (lazy - first call computes)
    dstring_view hash_view = dsv_from_cstr("Hash me please!");
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        volatile uint32_t h = dsv_hash(&hash_view);
        (void)h;
    }
    print_result("Hash (cached after first)", time_sec() - start, iterations / (time_sec() - start));
    
    // 8. Hash const (always computes)
    start = time_sec();
    for (int i = 0; i < iterations / 10; i++) {
        volatile uint32_t h = dsv_hash_const(&hash_view);
        (void)h;
    }
    print_result("Hash const (always computes)", time_sec() - start, (iterations / 10) / (time_sec() - start));
    
    // 9. Split at delimiter
    start = time_sec();
    for (int i = 0; i < iterations / 10; i++) {
        dstring_view v = view;
        dstring_view token = dsv_split_at(&v, ' ');
        volatile uint32_t l = token.len;
        (void)l;
    }
    print_result("Split at delimiter", time_sec() - start, (iterations / 10) / (time_sec() - start));
    
    // 10. Starts with
    dstring_view prefix = dsv_from_cstr("The");
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        volatile bool result = dsv_starts_with(&view, &prefix);
        (void)result;
    }
    print_result("Starts with", time_sec() - start, iterations / (time_sec() - start));
    
    // 11. Ends with
    dstring_view suffix = dsv_from_cstr("dog");
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        volatile bool result = dsv_ends_with(&view, &suffix);
        (void)result;
    }
    print_result("Ends with", time_sec() - start, iterations / (time_sec() - start));
    
    // 12. Trim view
    dstring_view padded = dsv_from_cstr("   Hello World   ");
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring_view trimmed = dsv_trim(&padded);
        volatile uint32_t l = trimmed.len;
        (void)l;
    }
    print_result("Trim view", time_sec() - start, iterations / (time_sec() - start));
    
    ds_free(&str);
}

// ============================================================================
// 4. DSTRING ARENA SCENARIOS (from previous benchmark)
// ============================================================================

void bench_arena_scenarios(void) {
    print_header("DSTRING_ARENA SCENARIOS");
    
    const int iterations = 100000;
    
    // 1. Create arena (small)
    double start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring_arena arena = dsa_create(16);
        dsa_free(&arena);
    }
    print_result("Create/free arena (16B)", time_sec() - start, iterations / (time_sec() - start));
    
    // 2. Create arena (large)
    start = time_sec();
    for (int i = 0; i < iterations / 10; i++) {
        dstring_arena arena = dsa_create(65536);
        dsa_free(&arena);
    }
    print_result("Create/free arena (64KB)", time_sec() - start, (iterations / 10) / (time_sec() - start));
    
    // 3. Append to pre-allocated arena
    dstring_arena arena = dsa_create(65536);
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dsa_clear(&arena);
        dsa_append(&arena, "Hello, World!");
    }
    print_result("Append + clear (pre-allocated)", time_sec() - start, iterations / (time_sec() - start));
    
    // 4. Push chars to arena (O(1))
    start = time_sec();
    dsa_clear(&arena);
    for (int i = 0; i < iterations * 100; i++) {
        dsa_push(&arena, 'x');
    }
    print_result("Push 10M chars (O(1))", time_sec() - start, (iterations * 100) / (time_sec() - start));
    
    // 5. Append dstring to arena
    dstring source = ds_init("Source string for appending");
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dsa_clear(&arena);
        dsa_append_dstring(&arena, &source);
    }
    print_result("Append dstring to arena", time_sec() - start, iterations / (time_sec() - start));
    
    // 6. Manual growth control
    start = time_sec();
    for (int i = 0; i < iterations / 10; i++) {
        dsa_extend_by_multiplier(&arena, 1.5f);
    }
    print_result("Manual extend (1.5x)", time_sec() - start, (iterations / 10) / (time_sec() - start));
    
    // 7. Convert to dstring
    dsa_clear(&arena);
    dsa_append(&arena, "Convert me to dstring!");
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring converted = dsa_to_dstring(&arena);
        ds_free(&converted);
    }
    print_result("Convert to dstring", time_sec() - start, iterations / (time_sec() - start));
    
    // 8. Create view from arena
    start = time_sec();
    for (int i = 0; i < iterations * 10; i++) {
        dstring_view view = dsa_to_view(&arena);
        volatile uint32_t l = view.len;
        (void)l;
    }
    print_result("Create view from arena", time_sec() - start, (iterations * 10) / (time_sec() - start));
    
    // 9. Shrink to fit
    dsa_clear(&arena);
    dsa_append(&arena, "Small");
    start = time_sec();
    for (int i = 0; i < iterations / 10; i++) {
        dsa_shrink_to_fit(&arena);
    }
    print_result("Shrink to fit", time_sec() - start, (iterations / 10) / (time_sec() - start));
    
    dsa_free(&arena);
    ds_free(&source);
}

// ============================================================================
// 5. CROSS-COMPONENT SCENARIOS (from previous benchmark)
// ============================================================================

void bench_cross_scenarios(void) {
    print_header("CROSS-COMPONENT SCENARIOS");
    
    const int iterations = 1000000;
    
    // 1. dstring -> view -> substring -> dstring
    dstring original = ds_init("Convert through all components");
    double start = time_sec();
    for (int i = 0; i < iterations / 10; i++) {
        dstring_view view = dsv_from_dstring(&original);
        dstring_view sub = dsv_sub(&view, 5, 10);
        dstring converted = dsv_to_dstring(&sub);
        ds_free(&converted);
    }
    print_result("dstring->view->sub->dstring", time_sec() - start, (iterations / 10) / (time_sec() - start));
    
    // 2. dstring -> arena -> append -> dstring
    start = time_sec();
    for (int i = 0; i < iterations / 10; i++) {
        dstring_arena arena = dsa_create(64);
        dsa_append_dstring(&arena, &original);
        dsa_append(&arena, " + more");
        dstring result = dsa_to_dstring(&arena);
        dsa_free(&arena);
        ds_free(&result);
    }
    print_result("dstring->arena->append->dstring", time_sec() - start, (iterations / 10) / (time_sec() - start));
    
    // 3. Arena -> view operations
    dstring_arena arena = dsa_create(128);
    dsa_append(&arena, "Arena data for view operations");
    dstring_view arena_view = dsa_to_view(&arena);
    start = time_sec();
    for (int i = 0; i < iterations; i++) {
        dstring_view sub = dsv_sub(&arena_view, 3, 10);
        volatile uint32_t l = sub.len;
        (void)l;
    }
    print_result("Arena view substring", time_sec() - start, iterations / (time_sec() - start));
    
    dsa_free(&arena);
    ds_free(&original);
}

// ============================================================================
// 6. STRING BUILDING SCENARIOS (NEW - critical!)
// ============================================================================

void bench_string_building(void) {
    print_header("STRING BUILDING SCENARIOS (CRITICAL!)");
    
    const int iterations = 100000;
    
    // 1. dstring with ds_push (O(n²) - BAD!)
    printf("\n  " COLOR_RED "ds_push (O(n²) - NOT RECOMMENDED):" COLOR_RESET "\n");
    double start = time_sec();
    dstring str_push = ds_init("");
    for (int i = 0; i < iterations; i++) {
        dstring temp = ds_push(&str_push, 'A' + (i % 26));
        ds_free(&str_push);
        str_push = temp;
    }
    print_result("ds_push 100K chars (O(n²))", time_sec() - start, iterations / (time_sec() - start));
    printf("    Final length: %u\n", ds_len(&str_push));
    ds_free(&str_push);
    
    // 2. Arena with default growth (1.5x)
    printf("\n  " COLOR_GREEN "Arena push (O(1) amortized):" COLOR_RESET "\n");
    start = time_sec();
    dstring_arena arena_default = dsa_create(16);
    for (int i = 0; i < iterations; i++) {
        dsa_push(&arena_default, 'A' + (i % 26));
    }
    print_result("Arena push 100K (1.5x growth)", time_sec() - start, iterations / (time_sec() - start));
    printf("    Final len: %u, capacity: %u, waste: %.2f%%\n", 
           dsa_len(&arena_default), dsa_capacity(&arena_default),
           (1.0 - (double)dsa_len(&arena_default) / dsa_capacity(&arena_default)) * 100);
    dsa_free(&arena_default);
    
    // 3. Arena pre-allocated (no reallocation!)
    printf("\n  " COLOR_GREEN "Arena pre-allocated (zero realloc):" COLOR_RESET "\n");
    start = time_sec();
    dstring_arena arena_prealloc = dsa_create(iterations + 1);
    for (int i = 0; i < iterations; i++) {
        dsa_push(&arena_prealloc, 'A' + (i % 26));
    }
    print_result("Arena push 100K (pre-allocated)", time_sec() - start, iterations / (time_sec() - start));
    printf("    Final len: %u, capacity: %u, waste: %.2f%%\n", 
           dsa_len(&arena_prealloc), dsa_capacity(&arena_prealloc),
           (1.0 - (double)dsa_len(&arena_prealloc) / dsa_capacity(&arena_prealloc)) * 100);
    dsa_free(&arena_prealloc);
    
    // 4. Appending various types
    printf("\n  " COLOR_GREEN "Appending scenarios:" COLOR_RESET "\n");
    
    start = time_sec();
    dstring_arena arena_append = dsa_create(1024);
    for (int i = 0; i < iterations / 10; i++) {
        dsa_append(&arena_append, "Hello, World! ");
    }
    print_result("Append C strings (10K)", time_sec() - start, (iterations / 10) / (time_sec() - start));
    dsa_free(&arena_append);
    
    dstring source = ds_init("Source string for appending");
    start = time_sec();
    dstring_arena arena_dstr = dsa_create(1024);
    for (int i = 0; i < iterations / 10; i++) {
        dsa_append_dstring(&arena_dstr, &source);
    }
    print_result("Append dstrings (10K)", time_sec() - start, (iterations / 10) / (time_sec() - start));
    dsa_free(&arena_dstr);
    ds_free(&source);
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf(COLOR_BOLD "\nCOMPLETE UNIFIED DSTRING BENCHMARK\n" COLOR_RESET);
    printf("  ALL scenarios - creation, operations, views, arena, building\n");
    printf("  String size: %zu bytes\n", sizeof(dstring));
    printf("  View size: %zu bytes\n", sizeof(dstring_view));
    printf("  Arena size: %zu bytes\n", sizeof(dstring_arena));
    printf("  SSO limit: %d bytes\n\n", STR_SSO_MAX + 1);
    
    bench_creation_scenarios();
    bench_operation_scenarios();
    bench_view_scenarios();
    bench_arena_scenarios();
    bench_cross_scenarios();
    bench_string_building();
    
    printf(COLOR_GREEN "\nAll benchmarks completed!\n" COLOR_RESET);
    return 0;
}