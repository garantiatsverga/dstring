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
    string s = initstr("Hello");
    assert(strok(&s));
    assert(strlen_s(&s) == 5);
    assert(strcmp(strdata(&s), "Hello") == 0);
    assert(is_sso(&s));
    freestr(&s);
    PASS();
    return 0;
}

// Heap (long strings)
int test_heap(void) {
    TEST("Heap: long string (23 chars)");
    const char* long_str = "Bulbasaur, I choose you!";
    string s = initstr(long_str);
    assert(strok(&s));
    assert(strlen_s(&s) == strlen(long_str));
    assert(strcmp(strdata(&s), long_str) == 0);
    assert(!is_sso(&s));
    freestr(&s);
    PASS();
    return 0;
}

// Concatenation
int test_concat(void) {
    TEST("strcat: Hello + World = HelloWorld");
    string s1 = initstr("Hello");
    string s2 = initstr("World");
    string s3 = strcat_s(&s1, &s2);
    
    assert(strok(&s3));
    assert(strlen_s(&s3) == 10);
    assert(strcmp(strdata(&s3), "HelloWorld") == 0);
    
    freestr(&s1);
    freestr(&s2);
    freestr(&s3);
    PASS();
    return 0;
}

// Substring
int test_substr(void) {
    TEST("strsub: 'HelloWorld'[5..9] = 'World'");
    string s1 = initstr("HelloWorld");
    string s2 = strsub(&s1, 5, 5);
    
    assert(strok(&s2));
    assert(strlen_s(&s2) == 5);
    assert(strcmp(strdata(&s2), "World") == 0);
    
    freestr(&s1);
    freestr(&s2);
    PASS();
    return 0;
}

// Binary data
int test_binary(void) {
    TEST("Binary: data with embedded null bytes");
    const char data[] = "A\0B\0C";
    string s = initstr_len(data, 5);
    
    assert(strok(&s));
    assert(strlen_s(&s) == 5);
    assert(memcmp(strdata(&s), data, 5) == 0);
    
    freestr(&s);
    PASS();
    return 0;
}

// Error handling
int test_errors(void) {
    TEST("Errors: NULL and overflow");
    
    // NULL string should be valid empty string
    string s1 = initstr(NULL);
    const char* data1 = strdata(&s1);
    if (data1 == NULL) {
        printf(COLOR_RED "\n  ERROR: initstr(NULL) should return \"\", not NULL\n" COLOR_RESET);
        return 1;
    }
    if (strcmp(data1, "") != 0) {
        printf(COLOR_RED "\n  ERROR: initstr(NULL) should return \"\", got '%s'\n" COLOR_RESET, data1);
        return 1;
    }
    if (strlen_s(&s1) != 0) {
        printf(COLOR_RED "\n  ERROR: initstr(NULL) should have length 0, got %u\n" COLOR_RESET, strlen_s(&s1));
        return 1;
    }
    freestr(&s1);
    
    // Overflow should set error marker
    string s2 = initstr_len("Too long", UINT_MAX);
    const char* data2 = strdata(&s2);
    if (strok(&s2)) {
        printf(COLOR_RED "\n  ERROR: initstr_len(UINT_MAX) should be error\n" COLOR_RESET);
        return 1;
    }
    if (strlen_s(&s2) != 0) {
        printf(COLOR_RED "\n  ERROR: Error string should have length 0\n" COLOR_RESET);
        return 1;
    }
    if (data2 == NULL) {
        printf(COLOR_RED "\n  ERROR: strdata() returned NULL for error string\n" COLOR_RESET);
        return 1;
    }
    if (strcmp(data2, "") != 0) {
        printf(COLOR_RED "\n  ERROR: strdata() should return \"\" for error string\n" COLOR_RESET);
        return 1;
    }
    freestr(&s2);
    
    PASS();
    return 0;
}

// Hash function
int test_hash(void) {
    TEST("Hash: different strings have different hashes");
    string s1 = initstr("Hello");
    string s2 = initstr("World");
    string s3 = initstr("Hello");
    
    uint32_t h1 = strhash(&s1);
    uint32_t h2 = strhash(&s2);
    uint32_t h3 = strhash(&s3);
    
    if (h1 == h2) {
        printf(COLOR_RED "\n  ERROR: Hash collision: Hello and World both have hash %u\n" COLOR_RESET, h1);
        freestr(&s1);
        freestr(&s2);
        freestr(&s3);
        return 1;
    }
    
    if (h1 != h3) {
        printf(COLOR_RED "\n  ERROR: Same string (Hello) has different hashes: %u vs %u\n" COLOR_RESET, h1, h3);
        freestr(&s1);
        freestr(&s2);
        freestr(&s3);
        return 1;
    }
    
    freestr(&s1);
    freestr(&s2);
    freestr(&s3);
    PASS();
    return 0;
}

// Double free safety
int test_free(void) {
    TEST("Free: double free does not crash");
    string s = initstr("Test");
    freestr(&s);
    freestr(&s);
    assert(!is_sso(&s));
    assert(strlen_s(&s) == 0);
    PASS();
    return 0;
}

int test_sso_boundary(void) {
    TEST("SSO: 15 chars max");
    
    // Test 1: 15 chars - should fit SSO exactly
    const char* test_str = "12345678901234";
    string s1 = initstr(test_str);
    
    // DEBUG: print everything
    printf("\n  DEBUG s1:");
    printf("  strok=%d", strok(&s1));
    printf("  is_sso=%d", is_sso(&s1));
    printf("  sso_len=0x%02x", s1.sso_len);
    printf("  len=%u", strlen_s(&s1));
    printf("  small='%.15s'", s1.small);
    printf("  small[15]='%c' (0x%02x)", s1.small[15], (unsigned char)s1.small[15]);
    
    assert(strok(&s1));
    assert(strlen_s(&s1) == 14);
    assert(is_sso(&s1));
    assert(strcmp(strdata(&s1), test_str) == 0);
    freestr(&s1);
    
    // Test 2: 16 chars - should go to heap
    string s2 = initstr("123456789012345");
    printf("\n  DEBUG s2:");
    printf("  strok=%d", strok(&s2));
    printf("  is_sso=%d", is_sso(&s2));
    printf("  sso_len=0x%02x", s2.sso_len);
    printf("  len=%u", strlen_s(&s2));
    printf("  ptr=%p", (void*)s2.ptr);
    
    assert(strok(&s2));
    assert(strlen_s(&s2) == 15);
    assert(!is_sso(&s2));
    freestr(&s2);
    
    PASS();
    return 0;
}

// Empty string
int test_empty(void) {
    TEST("Empty: initstr(\"\") returns empty string");
    string s = initstr("");
    assert(strok(&s));
    assert(strlen_s(&s) == 0);
    assert(strcmp(strdata(&s), "") == 0);
    assert(!is_sso(&s));
    freestr(&s);
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