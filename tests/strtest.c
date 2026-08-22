#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "../dstring.h"

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

#define TEST(name) printf(COLOR_CYAN "[TEST]" COLOR_RESET " %s... ", name); fflush(stdout)
#define PASS()      printf(COLOR_GREEN "PASS\n" COLOR_RESET)
#define FAIL()      do { printf(COLOR_RED "FAIL\n" COLOR_RESET); return 1; } while(0)

// SSO (short strings)
int test_sso(void) {
    TEST("SSO: short string (5 chars)");
    dstring s = ds_init("Hello");
    assert(ds_ok(&s));
    assert(ds_len(&s) == 5);
    assert(strcmp(ds_data(&s), "Hello") == 0);
    assert(ds_is_sso(&s));
    ds_free(&s);
    PASS();
    return 0;
}

// Heap (long strings)
int test_heap(void) {
    TEST("Heap: long string (23 chars)");
    const char* long_str = "Bulbasaur, I choose you!";
    dstring s = ds_init(long_str);
    assert(ds_ok(&s));
    assert(ds_len(&s) == strlen(long_str));
    assert(strcmp(ds_data(&s), long_str) == 0);
    assert(!ds_is_sso(&s));
    ds_free(&s);
    PASS();
    return 0;
}

// Concatenation
int test_concat(void) {
    TEST("ds_cat: Hello + World = HelloWorld");
    dstring s1 = ds_init("Hello");
    dstring s2 = ds_init("World");
    dstring s3 = ds_cat(&s1, &s2);
    
    assert(ds_ok(&s3));
    assert(ds_len(&s3) == 10);
    assert(strcmp(ds_data(&s3), "HelloWorld") == 0);
    
    ds_free(&s1);
    ds_free(&s2);
    ds_free(&s3);
    PASS();
    return 0;
}

// Substring
int test_substr(void) {
    TEST("ds_sub: 'HelloWorld'[5..9] = 'World'");
    dstring s1 = ds_init("HelloWorld");
    dstring s2 = ds_sub(&s1, 5, 5);
    
    assert(ds_ok(&s2));
    assert(ds_len(&s2) == 5);
    assert(strcmp(ds_data(&s2), "World") == 0);
    
    ds_free(&s1);
    ds_free(&s2);
    PASS();
    return 0;
}

// Binary data
int test_binary(void) {
    TEST("Binary: data with embedded null bytes");
    const char data[] = "A\0B\0C";
    dstring s = ds_init_len(data, 5);
    
    assert(ds_ok(&s));
    assert(ds_len(&s) == 5);
    assert(memcmp(ds_data(&s), data, 5) == 0);
    
    ds_free(&s);
    PASS();
    return 0;
}

// Error handling
int test_errors(void) {
    TEST("Errors: NULL and overflow");
    
    // NULL string should be valid empty string
    dstring s1 = ds_init(NULL);
    const char* data1 = ds_data(&s1);
    if (data1 == NULL) {
        printf(COLOR_RED "\n  ERROR: ds_init(NULL) should return \"\", not NULL\n" COLOR_RESET);
        return 1;
    }
    if (strcmp(data1, "") != 0) {
        printf(COLOR_RED "\n  ERROR: ds_init(NULL) should return \"\", got '%s'\n" COLOR_RESET, data1);
        return 1;
    }
    if (ds_len(&s1) != 0) {
        printf(COLOR_RED "\n  ERROR: ds_init(NULL) should have length 0, got %u\n" COLOR_RESET, ds_len(&s1));
        return 1;
    }
    ds_free(&s1);
    
    // Overflow should set error marker
    dstring s2 = ds_init_len("Too long", UINT_MAX);
    const char* data2 = ds_data(&s2);
    if (ds_ok(&s2)) {
        printf(COLOR_RED "\n  ERROR: ds_init_len(UINT_MAX) should be error\n" COLOR_RESET);
        return 1;
    }
    if (ds_len(&s2) != 0) {
        printf(COLOR_RED "\n  ERROR: Error string should have length 0\n" COLOR_RESET);
        return 1;
    }
    if (data2 == NULL) {
        printf(COLOR_RED "\n  ERROR: ds_data() returned NULL for error string\n" COLOR_RESET);
        return 1;
    }
    if (strcmp(data2, "") != 0) {
        printf(COLOR_RED "\n  ERROR: ds_data() should return \"\" for error string\n" COLOR_RESET);
        return 1;
    }
    ds_free(&s2);
    
    PASS();
    return 0;
}

// Hash function
int test_hash(void) {
    TEST("Hash: different strings have different hashes");
    dstring s1 = ds_init("Hello");
    dstring s2 = ds_init("World");
    dstring s3 = ds_init("Hello");
    
    uint32_t h1 = ds_hash(&s1);
    uint32_t h2 = ds_hash(&s2);
    uint32_t h3 = ds_hash(&s3);
    
    if (h1 == h2) {
        printf(COLOR_RED "\n  ERROR: Hash collision: Hello and World both have hash %u\n" COLOR_RESET, h1);
        ds_free(&s1);
        ds_free(&s2);
        ds_free(&s3);
        return 1;
    }
    
    if (h1 != h3) {
        printf(COLOR_RED "\n  ERROR: Same string (Hello) has different hashes: %u vs %u\n" COLOR_RESET, h1, h3);
        ds_free(&s1);
        ds_free(&s2);
        ds_free(&s3);
        return 1;
    }
    
    ds_free(&s1);
    ds_free(&s2);
    ds_free(&s3);
    PASS();
    return 0;
}

// Double free safety
int test_free(void) {
    TEST("Free: double free does not crash");
    dstring s = ds_init("Test");
    ds_free(&s);
    ds_free(&s);
    assert(!ds_is_sso(&s));
    assert(ds_len(&s) == 0);
    PASS();
    return 0;
}

int test_sso_boundary(void) {
    TEST("SSO: 14 chars max");
    
    // Test 1: 14 chars - should fit SSO exactly
    const char* test_str = "12345678901234";  // 14 chars
    dstring s1 = ds_init(test_str);
    
    assert(ds_ok(&s1));
    assert(ds_len(&s1) == 14);
    assert(ds_is_sso(&s1));
    assert(strcmp(ds_data(&s1), test_str) == 0);
    ds_free(&s1);
    
    // Test 2: 15 chars - should go to heap
    dstring s2 = ds_init("123456789012345");  // 15 chars
    
    assert(ds_ok(&s2));
    assert(ds_len(&s2) == 15);
    assert(!ds_is_sso(&s2));
    ds_free(&s2);
    
    PASS();
    return 0;
}

// Empty string
int test_empty(void) {
    TEST("Empty: ds_init(\"\") returns empty string");
    dstring s = ds_init("");
    assert(ds_ok(&s));
    assert(ds_len(&s) == 0);
    assert(strcmp(ds_data(&s), "") == 0);
    assert(ds_is_sso(&s));  // Empty string should be SSO
    ds_free(&s);
    PASS();
    return 0;
}

int main(void) {
    printf(COLOR_BOLD "dstring.h TEST SUITE\n" COLOR_RESET);
    
    int (*tests[])(void) = {
        test_sso,
        test_heap,
        test_concat,
        test_substr,
        test_binary,
        test_errors,
        test_hash,
        test_free,
        test_sso_boundary,
        test_empty,
        NULL
    };
    int total = sizeof(tests) / sizeof(tests[0]) - 1;
    int passed = 0;
    int failed = 0;
    
    printf("\n");
    for (int i = 0; tests[i] != NULL; i++) {
        if (tests[i]() == 0) {
            passed++;
        } else {
            failed++;
        }
    }
    
    printf(COLOR_BOLD "\nRESULTS\n" COLOR_RESET);
    printf("  Total:  %d\n", total);
    printf(COLOR_GREEN "  Passed: %d\n" COLOR_RESET, passed);
    if (failed > 0) {
        printf(COLOR_RED "  Failed: %d\n" COLOR_RESET, failed);
    } else {
        printf("  Failed: 0\n");
    }
    printf(COLOR_BOLD "=================\n" COLOR_RESET);
    
    return failed > 0 ? 1 : 0;
}