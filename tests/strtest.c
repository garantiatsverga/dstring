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

// === DSTRING TESTS ===

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

int test_errors(void) {
    TEST("Errors: NULL and overflow");
    
    dstring s1 = ds_init(NULL);
    const char* data1 = ds_data(&s1);
    if (data1 == NULL || strcmp(data1, "") != 0 || ds_len(&s1) != 0) {
        FAIL();
    }
    ds_free(&s1);
    
    dstring s2 = ds_init_len("Too long", UINT_MAX);
    if (ds_ok(&s2) || ds_len(&s2) != 0 || strcmp(ds_data(&s2), "") != 0) {
        FAIL();
    }
    ds_free(&s2);
    
    PASS();
    return 0;
}

int test_hash(void) {
    TEST("Hash: different strings have different hashes");
    dstring s1 = ds_init("Hello");
    dstring s2 = ds_init("World");
    dstring s3 = ds_init("Hello");
    
    uint32_t h1 = ds_hash(&s1);
    uint32_t h2 = ds_hash(&s2);
    uint32_t h3 = ds_hash(&s3);
    
    if (h1 == h2 || h1 != h3) {
        ds_free(&s1);
        ds_free(&s2);
        ds_free(&s3);
        FAIL();
    }
    
    ds_free(&s1);
    ds_free(&s2);
    ds_free(&s3);
    PASS();
    return 0;
}

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
    
    dstring s1 = ds_init("12345678901234");  // 14 chars
    assert(ds_ok(&s1));
    assert(ds_len(&s1) == 14);
    assert(ds_is_sso(&s1));
    ds_free(&s1);
    
    dstring s2 = ds_init("123456789012345");  // 15 chars
    assert(ds_ok(&s2));
    assert(ds_len(&s2) == 15);
    assert(!ds_is_sso(&s2));
    ds_free(&s2);
    
    PASS();
    return 0;
}

int test_empty(void) {
    TEST("Empty: ds_init(\"\") returns empty string");
    dstring s = ds_init("");
    assert(ds_ok(&s));
    assert(ds_len(&s) == 0);
    assert(strcmp(ds_data(&s), "") == 0);
    assert(ds_is_sso(&s));
    ds_free(&s);
    PASS();
    return 0;
}

// === DSTRING_VIEW TESTS ===

int test_view_creation(void) {
    TEST("View: creation from dstring");
    dstring s = ds_init("Hello, World!");
    dstring_view view = dsv_from_dstring(&s);
    
    assert(dsv_ok(&view));
    assert(dsv_len(&view) == 13);
    assert(strcmp(dsv_data(&view), "Hello, World!") == 0);
    
    ds_free(&s);
    PASS();
    return 0;
}

int test_view_substring(void) {
    TEST("View: substring (O(1))");
    dstring s = ds_init("Hello, World!");
    dstring_view view = dsv_from_dstring(&s);
    dstring_view sub = dsv_sub(&view, 7, 5);
    
    assert(dsv_ok(&sub));
    assert(dsv_len(&sub) == 5);
    assert(strncmp(dsv_data(&sub), "World", 5) == 0);
    
    ds_free(&s);
    PASS();
    return 0;
}

int test_view_hash_lazy(void) {
    TEST("View: lazy hash caching");
    dstring s = ds_init("Hash me please!");
    dstring_view view = dsv_from_dstring(&s);
    
    // Hash should be computed lazily
    uint32_t hash1 = dsv_hash(&view);
    assert(hash1 != 0);
    assert(view.hash == hash1);  // Cached
    
    // Second call should use cache
    uint32_t hash2 = dsv_hash(&view);
    assert(hash1 == hash2);
    
    ds_free(&s);
    PASS();
    return 0;
}

int test_view_modification(void) {
    TEST("View: remove_prefix/remove_suffix");
    dstring s = ds_init("Hello, World!");
    dstring_view view = dsv_from_dstring(&s);
    
    dsv_remove_prefix(&view, 7);
    assert(dsv_len(&view) == 6);
    assert(strncmp(dsv_data(&view), "World!", 6) == 0);
    
    dsv_remove_suffix(&view, 1);
    assert(dsv_len(&view) == 5);
    assert(strncmp(dsv_data(&view), "World", 5) == 0);
    
    ds_free(&s);
    PASS();
    return 0;
}

// === DSTRING_ARENA TESTS ===

int test_arena_creation(void) {
    TEST("Arena: creation and basic append");
    dstring_arena arena = dsa_create(64);
    
    assert(dsa_len(&arena) == 0);
    assert(dsa_capacity(&arena) == 64);
    
    dsa_append(&arena, "Hello");
    dsa_append(&arena, ", World!");
    
    assert(dsa_len(&arena) == 13);
    assert(strcmp(dsa_cstr(&arena), "Hello, World!") == 0);
    
    dsa_free(&arena);
    PASS();
    return 0;
}

int test_arena_reuse(void) {
    TEST("Arena: clear and reuse");
    dstring_arena arena = dsa_create(1024);
    
    for (int i = 0; i < 5; i++) {
        dsa_append(&arena, "Data");
        assert(dsa_len(&arena) == 4);
        dsa_clear(&arena);
        assert(dsa_len(&arena) == 0);
        assert(dsa_capacity(&arena) == 1024);  // Capacity preserved!
    }
    
    dsa_free(&arena);
    PASS();
    return 0;
}

int test_arena_import_dstring(void) {
    TEST("Arena: import from dstring");
    dstring s = ds_init("Hello from dstring!");
    dstring_arena arena = dsa_create(64);
    
    dsa_append_dstring(&arena, &s);
    
    assert(dsa_len(&arena) == ds_len(&s));
    assert(strcmp(dsa_cstr(&arena), ds_data(&s)) == 0);
    
    dsa_free(&arena);
    ds_free(&s);
    PASS();
    return 0;
}

int test_arena_manual_growth(void) {
    TEST("Arena: manual growth control");
    dstring_arena arena = dsa_create(16);
    uint32_t old_capacity = dsa_capacity(&arena);
    
    dsa_extend_by_multiplier(&arena, 2.0f);
    assert(dsa_capacity(&arena) > old_capacity);
    
    dsa_extend_to(&arena, 1000);
    assert(dsa_capacity(&arena) == 1000);
    
    dsa_free(&arena);
    PASS();
    return 0;
}

int test_arena_to_dstring(void) {
    TEST("Arena: conversion to dstring");
    dstring_arena arena = dsa_create(64);
    dsa_append(&arena, "Convert me!");
    
    dstring s = dsa_to_dstring(&arena);
    assert(ds_ok(&s));
    assert(ds_len(&s) == dsa_len(&arena));
    assert(strcmp(ds_data(&s), dsa_cstr(&arena)) == 0);
    
    dsa_free(&arena);
    ds_free(&s);
    PASS();
    return 0;
}

// === MAIN ===

int main(void) {
    printf(COLOR_BOLD "dstring.h TEST SUITE\n" COLOR_RESET);
    
    int (*tests[])(void) = {
        // dstring tests
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
        // dstring_view tests
        test_view_creation,
        test_view_substring,
        test_view_hash_lazy,
        test_view_modification,
        // dstring_arena tests
        test_arena_creation,
        test_arena_reuse,
        test_arena_import_dstring,
        test_arena_manual_growth,
        test_arena_to_dstring,
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