#ifndef DSTRING_COMPLETE_V3_H
#define DSTRING_COMPLETE_V3_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>
#include <stddef.h>

/* ============================================================================
 * COMPILER-SPECIFIC OPTIMIZATIONS
 * ============================================================================ */

#if defined(__GNUC__) || defined(__clang__)
    #define DS_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define DS_UNLIKELY(x) __builtin_expect(!!(x), 0)
    #define DS_RESTRICT __restrict__
    #define DS_ALWAYS_INLINE __attribute__((always_inline)) static inline
    #define DS_NO_INLINE __attribute__((noinline))
    #define DS_HOT __attribute__((hot))
    #define DS_COLD __attribute__((cold))
    #define DS_PURE __attribute__((pure))
    #define DS_CONST __attribute__((const))
    #define DS_NONNULL __attribute__((nonnull))
    #define DS_RETURNS_NONNULL __attribute__((returns_nonnull))
    #define DS_PREFETCH(addr, rw, locality) __builtin_prefetch(addr, rw, locality)
    #define DS_ASSUME(expr) do { if (!(expr)) __builtin_unreachable(); } while(0)
    #define DS_UNREACHABLE() __builtin_unreachable()
    #define DS_CONSTANT_P(expr) __builtin_constant_p(expr)
#elif defined(_MSC_VER)
    #define DS_LIKELY(x)   (x)
    #define DS_UNLIKELY(x) (x)
    #define DS_RESTRICT __restrict
    #define DS_ALWAYS_INLINE __forceinline static
    #define DS_NO_INLINE __declspec(noinline)
    #define DS_HOT
    #define DS_COLD
    #define DS_PURE
    #define DS_CONST
    #define DS_NONNULL
    #define DS_RETURNS_NONNULL
    #define DS_PREFETCH(addr, rw, locality)
    #define DS_ASSUME(expr) __assume(expr)
    #define DS_UNREACHABLE() __assume(0)
    #define DS_CONSTANT_P(expr) 0
#else
    #define DS_LIKELY(x)   (x)
    #define DS_UNLIKELY(x) (x)
    #define DS_RESTRICT
    #define DS_ALWAYS_INLINE static inline
    #define DS_NO_INLINE
    #define DS_HOT
    #define DS_COLD
    #define DS_PURE
    #define DS_CONST
    #define DS_NONNULL
    #define DS_RETURNS_NONNULL
    #define DS_PREFETCH(addr, rw, locality)
    #define DS_ASSUME(expr)
    #define DS_UNREACHABLE()
    #define DS_CONSTANT_P(expr) 0
#endif

/* ============================================================================
 * HASH STRATEGY CONFIGURATION
 * ============================================================================ */

#define DS_HASH_EAGER  0
#define DS_HASH_LAZY   1
#define DS_HASH_HYBRID 2

#ifndef DS_HASH_STRATEGY
    #define DS_HASH_STRATEGY DS_HASH_HYBRID
#endif

#if DS_HASH_STRATEGY != DS_HASH_EAGER && \
    DS_HASH_STRATEGY != DS_HASH_LAZY && \
    DS_HASH_STRATEGY != DS_HASH_HYBRID
    #error "Invalid DS_HASH_STRATEGY. Use 0 (eager), 1 (lazy), or 2 (hybrid)"
#endif

/* ============================================================================
 * SSO CONFIGURATION
 * ============================================================================ */

#define STR_SSO_MAX    14
#define STR_MODE_SSO   0x80
#define STR_LEN_MASK   0x3F
#define STR_ERROR_MARKER 0xFF  // Explicit error marker

#define DSA_MIN_CAPACITY 16

/* ============================================================================
 * SMART STRING LENGTH
 * ============================================================================ */

#if defined(__GNUC__) || defined(__clang__)
    #define DS_STRLEN(str) \
        (__builtin_constant_p(str) ? (sizeof(str) - 1) : strlen(str))
#else
    #define DS_STRLEN(str) strlen(str)
#endif

#define DS_LITERAL(str) ds_init_len((str), (uint32_t)(sizeof(str) - 1))
#define DS_SMART(str) ds_init_len((str), (uint32_t)DS_STRLEN(str))

/* ============================================================================
 * DSTRING STRUCTURE (16 bytes)
 * ============================================================================ */

typedef struct {
    union {
        struct {
            char* ptr;
            uint32_t len;
            uint32_t hash;
        } heap;
        
        struct {
            char small[15];
            uint8_t meta;
        } sso;
        
        uint8_t raw[16];
    };
} dstring;

#define DS_INIT { .raw = {0} }
#define DS_EMPTY ((dstring){ .raw = {0} })

/* ============================================================================
 * DSTRING_VIEW STRUCTURE (16 bytes)
 * ============================================================================ */

typedef struct {
    const char* data;
    uint32_t len;
    uint32_t hash;
} dstring_view;

#define DSV_INIT { .data = NULL, .len = 0, .hash = 0 }
#define DSV_EMPTY ((dstring_view){ .data = "", .len = 0, .hash = 0 })
#define DSV_LITERAL(str) ((dstring_view){ .data = str, .len = sizeof(str)-1, .hash = 0 })

/* ============================================================================
 * DSTRING_ARENA STRUCTURE (16 bytes)
 * ============================================================================ */

typedef struct {
    char* data;
    uint32_t len;
    uint32_t capacity;
} dstring_arena;

#define DSA_INIT { .data = NULL, .len = 0, .capacity = 0 }

/* ============================================================================
 * DSTRING: MODE DETECTION (with explicit error handling)
 * ============================================================================ */

DS_ALWAYS_INLINE DS_PURE
bool ds_is_sso(const dstring* s) {
    return (s->sso.meta & STR_MODE_SSO) && 
           ((s->sso.meta & STR_LEN_MASK) <= STR_SSO_MAX);
}

DS_ALWAYS_INLINE DS_PURE
bool ds_is_heap(const dstring* s) {
    return (s->heap.hash & 0x80000000) == 0 && s->heap.ptr != NULL;
}

DS_ALWAYS_INLINE DS_PURE
bool ds_is_error(const dstring* s) {
    // Error marker: meta = 0xFF (SSO bit set, but length exceeds SSO_MAX)
    return s->sso.meta == STR_ERROR_MARKER;
}

DS_ALWAYS_INLINE DS_PURE
bool ds_ok(const dstring* s) {
    if (DS_UNLIKELY(s == NULL)) return false;
    
    // Check for error marker first
    if (DS_UNLIKELY(ds_is_error(s))) return false;
    
    if (ds_is_sso(s)) {
        return (s->sso.meta & STR_LEN_MASK) <= STR_SSO_MAX;
    }
    
    if (ds_is_heap(s)) {
        return s->heap.ptr != NULL && 
               s->heap.len > STR_SSO_MAX &&
               s->heap.len < 0x80000000;
    }
    
    return false;
}

/* ============================================================================
 * DSTRING: HASH COMPUTATION
 * ============================================================================ */

DS_ALWAYS_INLINE DS_CONST
uint32_t ds_compute_hash(const char* DS_RESTRICT data, uint32_t len) {
    uint32_t hash = 2166136261u;
    uint32_t i = 0;
    
    for (; i + 4 <= len; i += 4) {
        hash ^= (uint8_t)data[i];
        hash *= 16777619u;
        hash ^= (uint8_t)data[i + 1];
        hash *= 16777619u;
        hash ^= (uint8_t)data[i + 2];
        hash *= 16777619u;
        hash ^= (uint8_t)data[i + 3];
        hash *= 16777619u;
    }
    
    switch (len - i) {
        case 3: hash ^= (uint8_t)data[i + 2]; hash *= 16777619u;
        case 2: hash ^= (uint8_t)data[i + 1]; hash *= 16777619u;
        case 1: hash ^= (uint8_t)data[i]; hash *= 16777619u;
        default: break;
    }
    
    return hash & 0x7FFFFFFF;
}

/* ============================================================================
 * DSTRING: CORE FUNCTIONS (with error handling)
 * ============================================================================ */

DS_ALWAYS_INLINE DS_PURE DS_RETURNS_NONNULL
const char* ds_data(const dstring* s) {
    if (DS_UNLIKELY(s == NULL || !ds_ok(s))) return "";
    
    uintptr_t sso_ptr = (uintptr_t)s->sso.small;
    uintptr_t heap_ptr = (uintptr_t)s->heap.ptr;
    int is_heap = ds_is_heap(s);
    
    return (const char*)(is_heap ? heap_ptr : sso_ptr);
}

DS_ALWAYS_INLINE DS_PURE
uint32_t ds_len(const dstring* s) {
    if (DS_UNLIKELY(s == NULL || !ds_ok(s))) return 0;
    
    uint32_t sso_len = s->sso.meta & STR_LEN_MASK;
    uint32_t heap_len = s->heap.len;
    int is_heap = ds_is_heap(s);
    
    return is_heap ? heap_len : sso_len;
}

DS_ALWAYS_INLINE DS_PURE
bool ds_empty(const dstring* s) {
    return s == NULL || !ds_ok(s) || ds_len(s) == 0;
}

/* ============================================================================
 * DSTRING: HASH ACCESS (strategy-aware)
 * ============================================================================ */

DS_ALWAYS_INLINE
uint32_t ds_hash(dstring* s) {
    if (DS_UNLIKELY(s == NULL || !ds_ok(s))) return 0;
    
    if (ds_is_sso(s)) {
        return ds_compute_hash(s->sso.small, s->sso.meta & STR_LEN_MASK);
    } else {
        if (s->heap.hash != 0) {
            return s->heap.hash;
        }
        
        uint32_t hash = ds_compute_hash(s->heap.ptr, s->heap.len);
        s->heap.hash = hash;
        return hash;
    }
}

DS_ALWAYS_INLINE DS_PURE
uint32_t ds_hash_const(const dstring* s) {
    if (DS_UNLIKELY(s == NULL || !ds_ok(s))) return 0;
    
    if (ds_is_sso(s)) {
        return ds_compute_hash(s->sso.small, s->sso.meta & STR_LEN_MASK);
    } else {
        if (s->heap.hash != 0) {
            return s->heap.hash;
        }
        return ds_compute_hash(s->heap.ptr, s->heap.len);
    }
}

DS_ALWAYS_INLINE DS_PURE
bool ds_hash_cached(const dstring* s) {
    if (DS_UNLIKELY(s == NULL || !ds_ok(s))) return false;
    return ds_is_heap(s) && s->heap.hash != 0;
}

DS_ALWAYS_INLINE
void ds_cache_hash(dstring* s) {
    if (DS_UNLIKELY(s == NULL || !ds_ok(s))) return;
    
    if (ds_is_heap(s) && s->heap.hash == 0) {
        s->heap.hash = ds_compute_hash(s->heap.ptr, s->heap.len);
    }
}

DS_ALWAYS_INLINE
void ds_invalidate_hash(dstring* s) {
    if (DS_UNLIKELY(s == NULL)) return;
    
    if (ds_is_heap(s)) {
        s->heap.hash = 0;
    }
}

/* ============================================================================
 * DSTRING: INITIALIZATION (strategy-aware, with error handling)
 * ============================================================================ */

DS_ALWAYS_INLINE
dstring ds_init_len(const char* DS_RESTRICT str, uint32_t len) {
    dstring s = DS_INIT;
    
    if (DS_UNLIKELY(str == NULL || len == 0)) {
        s.sso.meta = STR_MODE_SSO | 0;
        return s;
    }
    
    if (DS_UNLIKELY(len >= 0x80000000)) {
        // Explicit error marker
        s.sso.meta = STR_ERROR_MARKER;  // 0xFF
        s.sso.small[0] = '\0';  // Ensure empty data
        return s;
    }
    
    if (DS_LIKELY(len <= STR_SSO_MAX)) {
        // SSO path
        memcpy(s.sso.small, str, len);
        s.sso.small[len] = '\0';
        s.sso.meta = STR_MODE_SSO | (uint8_t)len;
    } else {
        // Heap path
        s.heap.ptr = (char*)malloc(len + 1);
        if (DS_UNLIKELY(s.heap.ptr == NULL)) {
            s.sso.meta = STR_ERROR_MARKER;
            s.sso.small[0] = '\0';
            return s;
        }
        memcpy(s.heap.ptr, str, len);
        s.heap.ptr[len] = '\0';
        s.heap.len = len;
        
        // Strategy-dependent hash initialization
        #if DS_HASH_STRATEGY == DS_HASH_EAGER
            s.heap.hash = ds_compute_hash(str, len);
        #elif DS_HASH_STRATEGY == DS_HASH_LAZY
            s.heap.hash = 0;
        #elif DS_HASH_STRATEGY == DS_HASH_HYBRID
            s.heap.hash = 0;
        #endif
    }
    return s;
}

DS_ALWAYS_INLINE
dstring ds_init(const char* str) {
    if (DS_UNLIKELY(str == NULL)) return (dstring)DS_INIT;
    return ds_init_len(str, (uint32_t)strlen(str));
}

/* ============================================================================
 * DSTRING: FREE
 * ============================================================================ */

DS_ALWAYS_INLINE
void ds_free(dstring* s) {
    if (DS_UNLIKELY(s == NULL)) return;
    
    if (ds_is_heap(s) && s->heap.ptr) {
        free(s->heap.ptr);
    }
    
    // Clear all 16 bytes
    s->raw[0] = 0; s->raw[1] = 0; s->raw[2] = 0; s->raw[3] = 0;
    s->raw[4] = 0; s->raw[5] = 0; s->raw[6] = 0; s->raw[7] = 0;
    s->raw[8] = 0; s->raw[9] = 0; s->raw[10] = 0; s->raw[11] = 0;
    s->raw[12] = 0; s->raw[13] = 0; s->raw[14] = 0; s->raw[15] = 0;
}

/* ============================================================================
 * DSTRING: CONCATENATION
 * ============================================================================ */

DS_ALWAYS_INLINE
dstring ds_cat(const dstring* DS_RESTRICT in1, 
                              const dstring* DS_RESTRICT in2) {
    dstring out = DS_INIT;
    
    if (DS_UNLIKELY(!in1 || !in2 || !ds_ok(in1) || !ds_ok(in2))) {
        out.sso.meta = STR_ERROR_MARKER;
        out.sso.small[0] = '\0';
        return out;
    }
    
    uint32_t len1 = ds_len(in1);
    uint32_t len2 = ds_len(in2);
    uint32_t total = len1 + len2;
    
    if (DS_UNLIKELY(total >= 0x7FFFFFFF)) {
        out.sso.meta = STR_ERROR_MARKER;
        out.sso.small[0] = '\0';
        return out;
    }
    
    const char* data1 = ds_data(in1);
    const char* data2 = ds_data(in2);
    
    if (DS_LIKELY(total <= STR_SSO_MAX && total > 0)) {
        memcpy(out.sso.small, data1, len1);
        memcpy(out.sso.small + len1, data2, len2);
        out.sso.small[total] = '\0';
        out.sso.meta = STR_MODE_SSO | (uint8_t)total;
        return out;
    }
    
    out.heap.ptr = (char*)malloc(total + 1);
    if (DS_UNLIKELY(out.heap.ptr == NULL)) {
        out.sso.meta = STR_ERROR_MARKER;
        out.sso.small[0] = '\0';
        return out;
    }
    
    memcpy(out.heap.ptr, data1, len1);
    memcpy(out.heap.ptr + len1, data2, len2 + 1);
    
    out.heap.len = total;
    out.heap.hash = 0;
    
    return out;
}

/* ============================================================================
 * DSTRING: SUBSTRING
 * ============================================================================ */

DS_ALWAYS_INLINE
dstring ds_sub(const dstring* DS_RESTRICT s, 
                              uint32_t start, 
                              uint32_t length) {
    dstring out = DS_INIT;
    
    if (DS_UNLIKELY(!s || !ds_ok(s))) {
        out.sso.meta = STR_ERROR_MARKER;
        out.sso.small[0] = '\0';
        return out;
    }
    
    uint32_t total_len = ds_len(s);
    if (DS_UNLIKELY(start > total_len || length == 0)) {
        out.sso.meta = STR_MODE_SSO | 0;
        return out;
    }
    if (DS_UNLIKELY(start + length > total_len)) length = total_len - start;
    
    return ds_init_len(ds_data(s) + start, length);
}

/* ============================================================================
 * DSTRING: CLONE
 * ============================================================================ */

DS_ALWAYS_INLINE
dstring ds_clone(const dstring* s) {
    if (DS_UNLIKELY(!s || !ds_ok(s))) return (dstring)DS_INIT;
    return ds_init_len(ds_data(s), ds_len(s));
}

/* ============================================================================
 * DSTRING: COMPARISON
 * ============================================================================ */

DS_ALWAYS_INLINE DS_PURE
int ds_cmp(const dstring* DS_RESTRICT a, const dstring* DS_RESTRICT b) {
    if (DS_UNLIKELY(a == b)) return 0;
    if (DS_UNLIKELY(!a || !b || !ds_ok(a) || !ds_ok(b))) return (a ? 1 : -1);
    
    uint32_t la = ds_len(a);
    uint32_t lb = ds_len(b);
    if (DS_UNLIKELY(la != lb)) {
        return (la > lb) ? 1 : -1;
    }
    
    if (DS_LIKELY(ds_is_heap(a) && ds_is_heap(b))) {
        if (DS_UNLIKELY(a->heap.hash != b->heap.hash)) {
            return a->heap.hash < b->heap.hash ? -1 : 1;
        }
    }
    
    const char* da = ds_data(a);
    const char* db = ds_data(b);
    
    return memcmp(da, db, la);
}

/* ============================================================================
 * DSTRING: PUSH CHARACTER
 * ============================================================================ */

DS_ALWAYS_INLINE
dstring ds_push(const dstring* DS_RESTRICT s, char c) {
    dstring out = DS_INIT;
    
    if (DS_UNLIKELY(!s || !ds_ok(s))) {
        out.sso.meta = STR_ERROR_MARKER;
        out.sso.small[0] = '\0';
        return out;
    }
    
    uint32_t old_len = ds_len(s);
    if (DS_UNLIKELY(old_len >= 0x7FFFFFFE)) {
        out.sso.meta = STR_ERROR_MARKER;
        out.sso.small[0] = '\0';
        return out;
    }
    
    const char* data = ds_data(s);
    uint32_t new_len = old_len + 1;
    
    if (DS_LIKELY(new_len <= STR_SSO_MAX)) {
        memcpy(out.sso.small, data, old_len);
        out.sso.small[old_len] = c;
        out.sso.small[new_len] = '\0';
        out.sso.meta = STR_MODE_SSO | (uint8_t)new_len;
    } else {
        out.heap.ptr = (char*)malloc(new_len + 1);
        if (DS_UNLIKELY(out.heap.ptr == NULL)) {
            out.sso.meta = STR_ERROR_MARKER;
            out.sso.small[0] = '\0';
            return out;
        }
        memcpy(out.heap.ptr, data, old_len);
        out.heap.ptr[old_len] = c;
        out.heap.ptr[new_len] = '\0';
        out.heap.len = new_len;
        out.heap.hash = 0;
    }
    
    return out;
}

/* ============================================================================
 * DSTRING: UTILITY FUNCTIONS
 * ============================================================================ */

DS_ALWAYS_INLINE DS_PURE DS_RETURNS_NONNULL
const char* ds_cstr(const dstring* s) {
    return ds_data(s);
}

DS_ALWAYS_INLINE DS_PURE
bool ds_contains(const dstring* s, char c) {
    if (DS_UNLIKELY(!s || !ds_ok(s))) return false;
    const char* data = ds_data(s);
    uint32_t len = ds_len(s);
    return memchr(data, c, len) != NULL;
}

DS_ALWAYS_INLINE DS_PURE
int32_t ds_find(const dstring* s, char c) {
    if (DS_UNLIKELY(!s || !ds_ok(s))) return -1;
    const char* data = ds_data(s);
    uint32_t len = ds_len(s);
    const char* result = memchr(data, c, len);
    return result ? (int32_t)(result - data) : -1;
}

DS_ALWAYS_INLINE
dstring ds_trim(const dstring* s) {
    if (DS_UNLIKELY(!s || !ds_ok(s))) return (dstring)DS_INIT;
    
    const char* data = ds_data(s);
    uint32_t len = ds_len(s);
    uint32_t start = 0;
    uint32_t end = len;
    
    while (start < end && (data[start] == ' ' || data[start] == '\t' || 
           data[start] == '\n' || data[start] == '\r')) {
        start++;
    }
    
    while (end > start && (data[end-1] == ' ' || data[end-1] == '\t' || 
           data[end-1] == '\n' || data[end-1] == '\r')) {
        end--;
    }
    
    return ds_init_len(data + start, end - start);
}

/* ============================================================================
 * DSTRING_VIEW: CREATION
 * ============================================================================ */

DS_ALWAYS_INLINE
dstring_view dsv_from_dstring(const dstring* s) {
    dstring_view view = DSV_INIT;
    if (DS_LIKELY(s && ds_ok(s))) {
        view.data = ds_data(s);
        view.len = ds_len(s);
        
        if (ds_is_heap(s) && s->heap.hash != 0) {
            view.hash = s->heap.hash;
        }
    }
    return view;
}

DS_ALWAYS_INLINE
dstring_view dsv_from_cstr(const char* str) {
    dstring_view view = DSV_INIT;
    if (DS_LIKELY(str)) {
        view.data = str;
        view.len = (uint32_t)strlen(str);
    }
    return view;
}

DS_ALWAYS_INLINE
dstring_view dsv_from_buffer(const char* data, uint32_t len) {
    dstring_view view = DSV_INIT;
    if (DS_LIKELY(data && len > 0)) {
        view.data = data;
        view.len = len;
    }
    return view;
}

/* ============================================================================
 * DSTRING_VIEW: BASIC OPERATIONS
 * ============================================================================ */

DS_ALWAYS_INLINE DS_PURE
bool dsv_ok(const dstring_view* view) {
    return view != NULL && (view->len == 0 || view->data != NULL);
}

DS_ALWAYS_INLINE DS_PURE DS_RETURNS_NONNULL
const char* dsv_data(const dstring_view* view) {
    return (view && view->data) ? view->data : "";
}

DS_ALWAYS_INLINE DS_PURE
uint32_t dsv_len(const dstring_view* view) {
    return view ? view->len : 0;
}

DS_ALWAYS_INLINE DS_PURE
bool dsv_empty(const dstring_view* view) {
    return !view || view->len == 0;
}

/* ============================================================================
 * DSTRING_VIEW: HASH COMPUTATION
 * ============================================================================ */

DS_ALWAYS_INLINE DS_PURE
bool dsv_has_hash(const dstring_view* view) {
    return view && view->hash != 0;
}

DS_NO_INLINE DS_COLD
static uint32_t dsv_compute_hash_slow(dstring_view* view) {
    if (view->len > 0) {
        view->hash = ds_compute_hash(view->data, view->len);
    }
    return view->hash;
}

DS_ALWAYS_INLINE
uint32_t dsv_hash(dstring_view* view) {
    if (DS_LIKELY(view && view->hash != 0)) {
        return view->hash;
    }
    return view ? dsv_compute_hash_slow(view) : 0;
}

DS_ALWAYS_INLINE DS_PURE
uint32_t dsv_hash_const(const dstring_view* view) {
    if (!view || view->len == 0) return 0;
    
    if (view->hash != 0) {
        return view->hash;
    }
    return ds_compute_hash(view->data, view->len);
}

DS_ALWAYS_INLINE
void dsv_cache_hash(dstring_view* view) {
    if (view && view->len > 0 && view->hash == 0) {
        view->hash = ds_compute_hash(view->data, view->len);
    }
}

DS_ALWAYS_INLINE
void dsv_invalidate_hash(dstring_view* view) {
    if (view) view->hash = 0;
}

/* ============================================================================
 * DSTRING_VIEW: SUBSTRING
 * ============================================================================ */

DS_ALWAYS_INLINE
dstring_view dsv_sub(const dstring_view* DS_RESTRICT view, 
                                    uint32_t start, 
                                    uint32_t length) {
    dstring_view result = DSV_INIT;
    
    if (DS_UNLIKELY(!view || start >= view->len)) {
        return result;
    }
    
    uint32_t max_len = view->len - start;
    length = (length < max_len) ? length : max_len;
    
    result.data = view->data + start;
    result.len = length;
    
    return result;
}

/* ============================================================================
 * DSTRING_VIEW: MODIFICATION
 * ============================================================================ */

DS_ALWAYS_INLINE
void dsv_remove_prefix(dstring_view* view, uint32_t n) {
    if (DS_UNLIKELY(!view || n > view->len)) return;
    view->data += n;
    view->len -= n;
    view->hash = 0;
}

DS_ALWAYS_INLINE
void dsv_remove_suffix(dstring_view* view, uint32_t n) {
    if (DS_UNLIKELY(!view || n > view->len)) return;
    view->len -= n;
    view->hash = 0;
}

/* ============================================================================
 * DSTRING_VIEW: COMPARISON
 * ============================================================================ */

DS_ALWAYS_INLINE
int dsv_cmp(dstring_view* a, dstring_view* b) {
    if (DS_UNLIKELY(a == b)) return 0;
    if (DS_UNLIKELY(!a || !b)) return (a ? 1 : -1);
    
    if (DS_UNLIKELY(a->len != b->len)) {
        return (a->len > b->len) ? 1 : -1;
    }
    
    if (DS_LIKELY(a->len > 32)) {
        uint32_t hash_a = dsv_hash(a);
        uint32_t hash_b = dsv_hash(b);
        if (DS_UNLIKELY(hash_a != hash_b)) {
            return (hash_a > hash_b) ? 1 : -1;
        }
    }
    
    return memcmp(a->data, b->data, a->len);
}

DS_ALWAYS_INLINE
bool dsv_equal(dstring_view* a, dstring_view* b) {
    if (DS_UNLIKELY(a == b)) return true;
    if (DS_UNLIKELY(!a || !b)) return false;
    if (DS_UNLIKELY(a->len != b->len)) return false;
    
    if (DS_LIKELY(a->len > 32)) {
        uint32_t hash_a = dsv_hash(a);
        uint32_t hash_b = dsv_hash(b);
        if (DS_UNLIKELY(hash_a != hash_b)) return false;
    }
    
    return memcmp(a->data, b->data, a->len) == 0;
}

/* ============================================================================
 * DSTRING_VIEW: SEARCH OPERATIONS
 * ============================================================================ */

DS_ALWAYS_INLINE DS_PURE
int32_t dsv_find(const dstring_view* view, char c) {
    if (DS_UNLIKELY(!view)) return -1;
    
    const char* result = memchr(view->data, c, view->len);
    return result ? (int32_t)(result - view->data) : -1;
}

DS_ALWAYS_INLINE DS_PURE
int32_t dsv_rfind(const dstring_view* view, char c) {
    if (DS_UNLIKELY(!view)) return -1;
    
    for (int32_t i = (int32_t)view->len - 1; i >= 0; i--) {
        if (view->data[i] == c) return i;
    }
    return -1;
}

DS_ALWAYS_INLINE
int32_t dsv_find_sub(dstring_view* haystack, 
                                    dstring_view* needle) {
    if (DS_UNLIKELY(!haystack || !needle)) return -1;
    if (DS_UNLIKELY(needle->len == 0)) return 0;
    if (DS_UNLIKELY(needle->len > haystack->len)) return -1;
    
    if (needle->len == 1) {
        return dsv_find(haystack, needle->data[0]);
    }
    
    if (DS_LIKELY(needle->len > 16 && haystack->len > 64)) {
        uint32_t needle_hash = dsv_hash(needle);
        
        for (uint32_t i = 0; i <= haystack->len - needle->len; i++) {
            if (haystack->data[i] != needle->data[0]) continue;
            
            uint32_t window_hash = ds_compute_hash(haystack->data + i, needle->len);
            if (window_hash != needle_hash) continue;
            
            if (memcmp(haystack->data + i, needle->data, needle->len) == 0) {
                return (int32_t)i;
            }
        }
        return -1;
    }
    
    for (uint32_t i = 0; i <= haystack->len - needle->len; i++) {
        if (haystack->data[i] != needle->data[0]) continue;
        if (memcmp(haystack->data + i, needle->data, needle->len) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* ============================================================================
 * DSTRING_VIEW: UTILITY FUNCTIONS
 * ============================================================================ */

DS_ALWAYS_INLINE
dstring dsv_to_dstring(const dstring_view* view) {
    if (DS_UNLIKELY(!view || !dsv_ok(view))) return (dstring)DS_INIT;
    
    dstring result = ds_init_len(view->data, view->len);
    
    if (view->hash != 0 && ds_is_heap(&result)) {
        result.heap.hash = view->hash;
    }
    
    return result;
}

DS_ALWAYS_INLINE
dstring_view dsv_split_at(dstring_view* view, char delimiter) {
    dstring_view token = DSV_INIT;
    
    if (DS_UNLIKELY(!view || view->len == 0)) {
        return token;
    }
    
    const char* delimiter_pos = memchr(view->data, delimiter, view->len);
    
    if (delimiter_pos == NULL) {
        token = *view;
        view->data += view->len;
        view->len = 0;
        view->hash = 0;
    } else {
        uint32_t pos = (uint32_t)(delimiter_pos - view->data);
        token.data = view->data;
        token.len = pos;
        token.hash = 0;
        
        view->data = delimiter_pos + 1;
        view->len -= pos + 1;
        view->hash = 0;
    }
    
    return token;
}

DS_ALWAYS_INLINE DS_PURE
bool dsv_starts_with(dstring_view* view, 
                                    dstring_view* prefix) {
    if (!view || !prefix || prefix->len > view->len) return false;
    return memcmp(view->data, prefix->data, prefix->len) == 0;
}

DS_ALWAYS_INLINE DS_PURE
bool dsv_ends_with(dstring_view* view, 
                                  dstring_view* suffix) {
    if (!view || !suffix || suffix->len > view->len) return false;
    return memcmp(view->data + view->len - suffix->len, 
                  suffix->data, suffix->len) == 0;
}

DS_ALWAYS_INLINE
dstring_view dsv_trim(const dstring_view* view) {
    dstring_view result = DSV_INIT;
    if (DS_UNLIKELY(!view)) return result;
    
    const char* data = view->data;
    uint32_t len = view->len;
    uint32_t start = 0;
    uint32_t end = len;
    
    while (start < end && (data[start] == ' ' || data[start] == '\t' || 
           data[start] == '\n' || data[start] == '\r')) {
        start++;
    }
    
    while (end > start && (data[end-1] == ' ' || data[end-1] == '\t' || 
           data[end-1] == '\n' || data[end-1] == '\r')) {
        end--;
    }
    
    result.data = data + start;
    result.len = end - start;
    result.hash = 0;
    
    return result;
}

/* ============================================================================
 * DSTRING_ARENA: BASIC OPERATIONS
 * ============================================================================ */

DS_ALWAYS_INLINE DS_PURE DS_RETURNS_NONNULL
const char* dsa_data(const dstring_arena* arena) {
    return (arena && arena->data) ? arena->data : "";
}

DS_ALWAYS_INLINE DS_PURE
uint32_t dsa_len(const dstring_arena* arena) {
    return arena ? arena->len : 0;
}

DS_ALWAYS_INLINE DS_PURE
bool dsa_empty(const dstring_arena* arena) {
    return !arena || arena->len == 0;
}

DS_ALWAYS_INLINE DS_PURE
uint32_t dsa_capacity(const dstring_arena* arena) {
    return arena ? arena->capacity : 0;
}

/* ============================================================================
 * DSTRING_ARENA: CREATION AND DESTRUCTION
 * ============================================================================ */

DS_ALWAYS_INLINE
dstring_arena dsa_create(uint32_t initial_capacity) {
    dstring_arena arena = DSA_INIT;
    
    if (initial_capacity > 0) {
        if (initial_capacity < DSA_MIN_CAPACITY) {
            initial_capacity = DSA_MIN_CAPACITY;
        }
        
        arena.data = (char*)malloc(initial_capacity);
        if (DS_LIKELY(arena.data)) {
            arena.data[0] = '\0';
            arena.capacity = initial_capacity;
        }
    }
    
    return arena;
}

DS_ALWAYS_INLINE
void dsa_free(dstring_arena* arena) {
    if (arena && arena->data) {
        free(arena->data);
    }
    if (arena) {
        arena->data = NULL;
        arena->len = 0;
        arena->capacity = 0;
    }
}

/* ============================================================================
 * DSTRING_ARENA: MANUAL EXTENSION
 * ============================================================================ */

DS_NO_INLINE DS_COLD
static bool dsa_extend_by_multiplier(dstring_arena* arena, float multiplier) {
    if (!arena || !arena->data || multiplier <= 1.0f) return false;
    
    uint32_t new_capacity = (uint32_t)(arena->capacity * multiplier);
    
    if (new_capacity <= arena->capacity) {
        new_capacity = arena->capacity + DSA_MIN_CAPACITY;
    }
    
    char* new_data = (char*)realloc(arena->data, new_capacity);
    if (!new_data) return false;
    
    arena->data = new_data;
    arena->capacity = new_capacity;
    return true;
}

DS_ALWAYS_INLINE
bool dsa_extend_to(dstring_arena* arena, uint32_t required_capacity) {
    if (!arena || !arena->data) return false;
    
    if (arena->capacity >= required_capacity) return true;
    
    char* new_data = (char*)realloc(arena->data, required_capacity);
    if (!new_data) return false;
    
    arena->data = new_data;
    arena->capacity = required_capacity;
    return true;
}

DS_NO_INLINE DS_COLD
static bool dsa_reserve_with_growth(dstring_arena* arena,
                                     uint32_t additional_bytes,
                                     float growth_factor) {
    if (!arena || growth_factor <= 1.0f) return false;
    
    uint32_t required = arena->len + additional_bytes + 1;
    
    if (arena->data && arena->capacity >= required) {
        return true;
    }
    
    uint32_t current_capacity = arena->capacity;
    if (current_capacity == 0) {
        current_capacity = DSA_MIN_CAPACITY;
    }
    
    uint32_t new_capacity = current_capacity;
    while (new_capacity < required) {
        new_capacity = (uint32_t)(new_capacity * growth_factor);
        if (new_capacity <= current_capacity) {
            new_capacity = required;
        }
    }
    
    if (arena->data == NULL) {
        arena->data = (char*)malloc(new_capacity);
        if (!arena->data) return false;
        arena->data[0] = '\0';
    } else {
        char* new_data = (char*)realloc(arena->data, new_capacity);
        if (!new_data) return false;
        arena->data = new_data;
    }
    
    arena->capacity = new_capacity;
    return true;
}

DS_ALWAYS_INLINE
bool dsa_reserve(dstring_arena* arena, uint32_t additional_bytes) {
    return dsa_reserve_with_growth(arena, additional_bytes, 1.5f);
}

/* ============================================================================
 * DSTRING_ARENA: APPENDING OPERATIONS
 * ============================================================================ */

DS_ALWAYS_INLINE
bool dsa_append_len(dstring_arena* DS_RESTRICT arena, 
                                   const char* DS_RESTRICT data, 
                                   uint32_t len) {
    if (DS_UNLIKELY(!arena || !data || len == 0)) return false;
    
    if (DS_LIKELY(arena->data && arena->len + len < arena->capacity)) {
        memcpy(arena->data + arena->len, data, len);
        arena->len += len;
        arena->data[arena->len] = '\0';
        return true;
    }
    
    if (DS_UNLIKELY(!dsa_reserve(arena, len))) return false;
    
    memcpy(arena->data + arena->len, data, len);
    arena->len += len;
    arena->data[arena->len] = '\0';
    return true;
}

DS_ALWAYS_INLINE
bool dsa_append_len_with_growth(dstring_arena* arena,
                                               const char* data,
                                               uint32_t len,
                                               float growth_factor) {
    if (DS_UNLIKELY(!arena || !data || len == 0)) return false;
    
    if (DS_LIKELY(arena->data && arena->len + len < arena->capacity)) {
        memcpy(arena->data + arena->len, data, len);
        arena->len += len;
        arena->data[arena->len] = '\0';
        return true;
    }
    
    if (DS_UNLIKELY(!dsa_reserve_with_growth(arena, len, growth_factor))) return false;
    
    memcpy(arena->data + arena->len, data, len);
    arena->len += len;
    arena->data[arena->len] = '\0';
    return true;
}

DS_ALWAYS_INLINE
bool dsa_append(dstring_arena* arena, const char* str) {
    if (DS_UNLIKELY(!arena || !str)) return false;
    return dsa_append_len(arena, str, (uint32_t)strlen(str));
}

DS_ALWAYS_INLINE
bool dsa_push(dstring_arena* arena, char c) {
    if (DS_UNLIKELY(!arena)) return false;
    return dsa_append_len(arena, &c, 1);
}

DS_ALWAYS_INLINE
bool dsa_append_dstring(dstring_arena* arena, const dstring* s) {
    if (DS_UNLIKELY(!arena || !s || !ds_ok(s))) return false;
    return dsa_append_len(arena, ds_data(s), ds_len(s));
}

DS_ALWAYS_INLINE
bool dsa_append_view(dstring_arena* arena, const dstring_view* view) {
    if (DS_UNLIKELY(!arena || !view || !dsv_ok(view))) return false;
    return dsa_append_len(arena, view->data, view->len);
}

/* ============================================================================
 * DSTRING_ARENA: CONVERSION
 * ============================================================================ */

DS_ALWAYS_INLINE
dstring dsa_to_dstring(const dstring_arena* arena) {
    if (DS_UNLIKELY(!arena || !arena->data)) return (dstring)DS_INIT;
    return ds_init_len(arena->data, arena->len);
}

DS_ALWAYS_INLINE
dstring_view dsa_to_view(const dstring_arena* arena) {
    dstring_view view = DSV_INIT;
    if (arena && arena->data) {
        view.data = arena->data;
        view.len = arena->len;
    }
    return view;
}

/* ============================================================================
 * DSTRING_ARENA: OPERATIONS
 * ============================================================================ */

DS_ALWAYS_INLINE
void dsa_clear(dstring_arena* arena) {
    if (arena && arena->data) {
        arena->len = 0;
        arena->data[0] = '\0';
    }
}

DS_ALWAYS_INLINE
void dsa_shrink_to_fit(dstring_arena* arena) {
    if (!arena || !arena->data) return;
    
    uint32_t new_capacity = arena->len + 1;
    if (new_capacity == arena->capacity) return;
    
    char* new_data = (char*)realloc(arena->data, new_capacity);
    if (new_data) {
        arena->data = new_data;
        arena->capacity = new_capacity;
    }
}

DS_ALWAYS_INLINE DS_PURE DS_RETURNS_NONNULL
const char* dsa_cstr(const dstring_arena* arena) {
    return dsa_data(arena);
}

#endif /* DSTRING_COMPLETE_V3_H */