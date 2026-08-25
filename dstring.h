#ifndef DSTRING_H
#define DSTRING_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>
#include <stddef.h>

// COMPILER-SPECIFIC OPTIMIZATIONS

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

// THREAD SAFETY CONFIGURATION

#ifndef DS_THREAD_SAFE
    #define DS_THREAD_SAFE 0
#endif

#if DS_THREAD_SAFE
    #include <stdatomic.h>
#endif

// ERROR HANDLING POLICY

#define DS_ERR_POLICY_EMPTY  0
#define DS_ERR_POLICY_MARKER 1
#define DS_ERR_POLICY_ABORT  2

#ifndef DS_ERROR_POLICY
    #define DS_ERROR_POLICY DS_ERR_POLICY_MARKER
#endif

// HASH STRATEGY CONFIGURATION

#define DS_HASH_EAGER  0
#define DS_HASH_LAZY   1
#define DS_HASH_HYBRID 2

#ifndef DS_HASH_STRATEGY
    #define DS_HASH_STRATEGY DS_HASH_HYBRID
#endif

// SSO CONFIGURATION

#define STR_SSO_MAX    14
#define STR_MODE_SSO   0x80
#define STR_LEN_MASK   0x3F
#define STR_ERROR_MARKER 0xFF

#define DSA_MIN_CAPACITY 16

// SMART STRING LENGTH

#if defined(__GNUC__) || defined(__clang__)
    #define DS_STRLEN(str) \
        (__builtin_constant_p(str) ? (sizeof(str) - 1) : strlen(str))
#else
    #define DS_STRLEN(str) strlen(str)
#endif

#define DS_LITERAL(str) ds_init_len((str), (uint32_t)(sizeof(str) - 1))
#define DS_SMART(str) ds_init_len((str), (uint32_t)DS_STRLEN(str))

// DSTRING STRUCTURE (16 bytes)

#if DS_THREAD_SAFE
typedef struct {
    union {
        struct {
            char* ptr;
            uint32_t len;
            _Atomic uint32_t hash;
        } heap;
        
        struct {
            char small[15];
            uint8_t meta;
        } sso;
        
        uint8_t raw[16];
    };
} dstring;
#else
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
#endif

_Static_assert(sizeof(dstring) == 16, "dstring must be exactly 16 bytes");

#define DS_INIT ((dstring){ .raw = {0} })
#define DS_EMPTY ((dstring){ .raw = {0} })

// DSTRING_VIEW STRUCTURE (16 bytes)

typedef struct {
    const char* data;
    uint32_t len;
    uint32_t hash;
} dstring_view;

#define DSV_INIT ((dstring_view){ .data = NULL, .len = 0, .hash = 0 })
#define DSV_EMPTY ((dstring_view){ .data = "", .len = 0, .hash = 0 })
#define DSV_LITERAL(str) ((dstring_view){ .data = str, .len = sizeof(str)-1, .hash = 0 })

// DSTRING_ARENA STRUCTURE (16 bytes)

typedef struct {
    char* data;
    uint32_t len;
    uint32_t capacity;
} dstring_arena;

#define DSA_INIT ((dstring_arena){ .data = NULL, .len = 0, .capacity = 0 })

// DSTRING: ERROR HANDLING

DS_ALWAYS_INLINE
dstring ds_make_error(void) {
    dstring out = DS_INIT;
    
#if DS_ERROR_POLICY == DS_ERR_POLICY_EMPTY
    out.sso.meta = STR_MODE_SSO | 0;
#elif DS_ERROR_POLICY == DS_ERR_POLICY_MARKER
    out.sso.meta = STR_ERROR_MARKER;
    out.sso.small[0] = '\0';
#else
    abort();
#endif
    return out;
}

// DSTRING: MODE DETECTION

DS_ALWAYS_INLINE DS_PURE
bool ds_is_sso(const dstring* s) {
    if (DS_UNLIKELY(s == NULL)) return false;
    return (s->sso.meta & STR_MODE_SSO) && 
           ((s->sso.meta & STR_LEN_MASK) <= STR_SSO_MAX);
}

DS_ALWAYS_INLINE DS_PURE
bool ds_is_heap(const dstring* s) {
    if (DS_UNLIKELY(s == NULL)) return false;
    
#if DS_THREAD_SAFE
    uint32_t hash = atomic_load_explicit(&s->heap.hash, memory_order_relaxed);
    return (hash & 0x80000000) == 0 && s->heap.ptr != NULL;
#else
    return (s->heap.hash & 0x80000000) == 0 && s->heap.ptr != NULL;
#endif
}

DS_ALWAYS_INLINE DS_PURE
bool ds_is_error(const dstring* s) {
    if (DS_UNLIKELY(s == NULL)) return true;
    return s->sso.meta == STR_ERROR_MARKER;
}

DS_ALWAYS_INLINE DS_PURE
bool ds_ok(const dstring* s) {
    if (DS_UNLIKELY(s == NULL)) return false;
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

// DSTRING: HASH COMPUTATION

DS_ALWAYS_INLINE DS_CONST
uint32_t ds_compute_hash(const char* DS_RESTRICT data, uint32_t len) {
    if (DS_UNLIKELY(data == NULL || len == 0)) return 0;
    
    const uint32_t FNV_PRIME = 16777619u;
    const uint32_t FNV_OFFSET = 2166136261u;
    
    uint32_t hash1 = FNV_OFFSET;
    uint32_t hash2 = FNV_OFFSET;
    uint32_t i = 0;
    
    // Process 8 bytes at a time with dual streams
    for (; i + 8 <= len; i += 8) {
        hash1 ^= (uint8_t)data[i];
        hash1 *= FNV_PRIME;
        hash1 ^= (uint8_t)data[i + 2];
        hash1 *= FNV_PRIME;
        hash1 ^= (uint8_t)data[i + 4];
        hash1 *= FNV_PRIME;
        hash1 ^= (uint8_t)data[i + 6];
        hash1 *= FNV_PRIME;
        
        hash2 ^= (uint8_t)data[i + 1];
        hash2 *= FNV_PRIME;
        hash2 ^= (uint8_t)data[i + 3];
        hash2 *= FNV_PRIME;
        hash2 ^= (uint8_t)data[i + 5];
        hash2 *= FNV_PRIME;
        hash2 ^= (uint8_t)data[i + 7];
        hash2 *= FNV_PRIME;
    }
    
    // Process remaining bytes
    for (; i < len; i++) {
        hash1 ^= (uint8_t)data[i];
        hash1 *= FNV_PRIME;
    }
    
    // Combine streams
    return (hash1 ^ (hash2 * FNV_PRIME)) & 0x7FFFFFFF;
}

// DSTRING: CORE FUNCTIONS

DS_ALWAYS_INLINE DS_PURE DS_RETURNS_NONNULL
const char* ds_data(const dstring* s) {
    if (DS_UNLIKELY(s == NULL || !ds_ok(s))) return "";
    
    if (ds_is_heap(s)) {
        return s->heap.ptr;
    } else {
        return s->sso.small;
    }
}

DS_ALWAYS_INLINE DS_PURE
uint32_t ds_len(const dstring* s) {
    if (DS_UNLIKELY(s == NULL || !ds_ok(s))) return 0;
    
    if (ds_is_heap(s)) {
        return s->heap.len;
    } else {
        return s->sso.meta & STR_LEN_MASK;
    }
}

DS_ALWAYS_INLINE DS_PURE
bool ds_empty(const dstring* s) {
    return s == NULL || !ds_ok(s) || ds_len(s) == 0;
}

// DSTRING: HASH ACCESS

DS_ALWAYS_INLINE
uint32_t ds_hash(dstring* s) {
    if (DS_UNLIKELY(s == NULL || !ds_ok(s))) return 0;
    
    if (ds_is_sso(s)) {
        return ds_compute_hash(s->sso.small, s->sso.meta & STR_LEN_MASK);
    } else {
#if DS_THREAD_SAFE
        uint32_t cached = atomic_load_explicit(&s->heap.hash, memory_order_acquire);
        if (cached != 0) return cached;
        
        uint32_t hash = ds_compute_hash(s->heap.ptr, s->heap.len);
        uint32_t expected = 0;
        if (atomic_compare_exchange_strong_explicit(
                &s->heap.hash, &expected, hash,
                memory_order_release, memory_order_relaxed)) {
            return hash;
        }
        return expected;
#else
        if (s->heap.hash != 0) return s->heap.hash;
        
        uint32_t hash = ds_compute_hash(s->heap.ptr, s->heap.len);
        s->heap.hash = hash;
        return hash;
#endif
    }
}

DS_ALWAYS_INLINE DS_PURE
uint32_t ds_hash_const(const dstring* s) {
    if (DS_UNLIKELY(s == NULL || !ds_ok(s))) return 0;
    
    if (ds_is_sso(s)) {
        return ds_compute_hash(s->sso.small, s->sso.meta & STR_LEN_MASK);
    } else {
#if DS_THREAD_SAFE
        uint32_t cached = atomic_load_explicit(&s->heap.hash, memory_order_acquire);
        if (cached != 0) return cached;
#else
        if (s->heap.hash != 0) return s->heap.hash;
#endif
        return ds_compute_hash(s->heap.ptr, s->heap.len);
    }
}

DS_ALWAYS_INLINE DS_PURE
bool ds_hash_cached(const dstring* s) {
    if (DS_UNLIKELY(s == NULL || !ds_ok(s))) return false;
    
#if DS_THREAD_SAFE
    if (ds_is_heap(s)) {
        return atomic_load_explicit(&s->heap.hash, memory_order_acquire) != 0;
    }
    return false;
#else
    return ds_is_heap(s) && s->heap.hash != 0;
#endif
}

DS_ALWAYS_INLINE
void ds_cache_hash(dstring* s) {
    if (DS_UNLIKELY(s == NULL || !ds_ok(s))) return;
    
    if (ds_is_heap(s)) {
        uint32_t hash = ds_compute_hash(s->heap.ptr, s->heap.len);
#if DS_THREAD_SAFE
        atomic_store_explicit(&s->heap.hash, hash, memory_order_release);
#else
        s->heap.hash = hash;
#endif
    }
}

DS_ALWAYS_INLINE
void ds_invalidate_hash(dstring* s) {
    if (DS_UNLIKELY(s == NULL)) return;
    
    if (ds_is_heap(s)) {
#if DS_THREAD_SAFE
        atomic_store_explicit(&s->heap.hash, 0, memory_order_release);
#else
        s->heap.hash = 0;
#endif
    }
}

// DSTRING: INITIALIZATION

DS_ALWAYS_INLINE
dstring ds_init_len(const char* DS_RESTRICT str, uint32_t len) {
    dstring s = DS_INIT;
    
    if (DS_UNLIKELY(str == NULL || len == 0)) {
        s.sso.meta = STR_MODE_SSO | 0;
        return s;
    }
    
    if (DS_UNLIKELY(len >= 0x80000000)) {
        return ds_make_error();
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
            return ds_make_error();
        }
        memcpy(s.heap.ptr, str, len);
        s.heap.ptr[len] = '\0';
        s.heap.len = len;
        s.heap.hash = 0;
    }
    return s;
}

DS_ALWAYS_INLINE
dstring ds_init(const char* str) {
    if (DS_UNLIKELY(str == NULL)) return ds_make_error();
    return ds_init_len(str, (uint32_t)strlen(str));
}

// NEW: Direct construction from repeated character (like std::string constructor)
DS_ALWAYS_INLINE
dstring ds_init_fill(char c, uint32_t count) {
    dstring s = DS_INIT;
    
    if (DS_UNLIKELY(count == 0)) {
        s.sso.meta = STR_MODE_SSO | 0;
        return s;
    }
    
    if (DS_UNLIKELY(count >= 0x80000000)) {
        return ds_make_error();
    }
    
    if (DS_LIKELY(count <= STR_SSO_MAX)) {
        // SSO path
        memset(s.sso.small, c, count);
        s.sso.small[count] = '\0';
        s.sso.meta = STR_MODE_SSO | (uint8_t)count;
    } else {
        // Heap path - single allocation + fill (no copy needed!)
        s.heap.ptr = (char*)malloc(count + 1);
        if (DS_UNLIKELY(s.heap.ptr == NULL)) {
            return ds_make_error();
        }
        
        memset(s.heap.ptr, c, count);
        s.heap.ptr[count] = '\0';
        s.heap.len = count;
        s.heap.hash = 0;
    }
    return s;
}

// NEW: Take ownership of existing heap-allocated buffer (zero-copy)
DS_ALWAYS_INLINE
dstring ds_init_take(char* DS_RESTRICT str, uint32_t len) {
    dstring s = DS_INIT;
    
    if (DS_UNLIKELY(str == NULL || len == 0)) {
        if (str != NULL) free(str);
        s.sso.meta = STR_MODE_SSO | 0;
        return s;
    }
    
    if (DS_UNLIKELY(len >= 0x80000000)) {
        free(str);
        return ds_make_error();
    }
    
    if (DS_LIKELY(len <= STR_SSO_MAX)) {
        // Small string - copy and free original
        memcpy(s.sso.small, str, len);
        s.sso.small[len] = '\0';
        s.sso.meta = STR_MODE_SSO | (uint8_t)len;
        free(str);
    } else {
        // Large string - take ownership (zero-copy!)
        s.heap.ptr = str;
        s.heap.len = len;
        s.heap.hash = 0;
    }
    return s;
}

// NEW: Create dstring from arena, taking ownership of the buffer
DS_ALWAYS_INLINE
dstring ds_from_arena_take(dstring_arena* arena) {
    dstring s = DS_INIT;
    
    if (DS_UNLIKELY(arena == NULL || arena->data == NULL)) {
        return ds_make_error();
    }
    
    if (arena->len <= STR_SSO_MAX) {
        // Small string - copy and free arena buffer
        memcpy(s.sso.small, arena->data, arena->len);
        s.sso.small[arena->len] = '\0';
        s.sso.meta = STR_MODE_SSO | (uint8_t)arena->len;
        
        free(arena->data);
        arena->data = NULL;
        arena->len = 0;
        arena->capacity = 0;
    } else {
        // Large string - take ownership of arena's buffer
        s.heap.ptr = arena->data;
        s.heap.len = arena->len;
        s.heap.hash = 0;
        
        // Reset arena (don't free data)
        arena->data = NULL;
        arena->len = 0;
        arena->capacity = 0;
    }
    
    return s;
}

// DSTRING: FREE

DS_ALWAYS_INLINE
void ds_free(dstring* s) {
    if (DS_UNLIKELY(s == NULL)) return;
    
    if (ds_is_heap(s) && s->heap.ptr) {
        free(s->heap.ptr);
    }
    
    memset(s->raw, 0, sizeof(s->raw));
}

// DSTRING: CONCATENATION

DS_ALWAYS_INLINE
dstring ds_cat(const dstring* DS_RESTRICT in1, 
               const dstring* DS_RESTRICT in2) {
    dstring out = DS_INIT;
    
    if (DS_UNLIKELY(!in1 || !in2 || !ds_ok(in1) || !ds_ok(in2))) {
        return ds_make_error();
    }
    
    uint32_t len1 = ds_len(in1);
    uint32_t len2 = ds_len(in2);
    
    if (DS_UNLIKELY(len1 > 0x7FFFFFFE - len2)) {
        return ds_make_error();
    }
    
    uint32_t total = len1 + len2;
    
    const char* data1 = ds_data(in1);
    const char* data2 = ds_data(in2);
    
    if (DS_LIKELY(total <= STR_SSO_MAX && total > 0)) {
        // SSO path
        memcpy(out.sso.small, data1, len1);
        memcpy(out.sso.small + len1, data2, len2);
        out.sso.small[total] = '\0';
        out.sso.meta = STR_MODE_SSO | (uint8_t)total;
        return out;
    }
    
    // Heap path
    out.heap.ptr = (char*)malloc(total + 1);
    if (DS_UNLIKELY(out.heap.ptr == NULL)) {
        return ds_make_error();
    }
    
    memcpy(out.heap.ptr, data1, len1);
    memcpy(out.heap.ptr + len1, data2, len2 + 1);
    
    out.heap.len = total;
    out.heap.hash = 0;
    
    return out;
}

// DSTRING: SUBSTRING

DS_ALWAYS_INLINE
dstring ds_sub(const dstring* DS_RESTRICT s, 
               uint32_t start, 
               uint32_t length) {
    dstring out = DS_INIT;
    
    if (DS_UNLIKELY(!s || !ds_ok(s))) {
        return ds_make_error();
    }
    
    uint32_t total_len = ds_len(s);
    
    if (DS_UNLIKELY(start > total_len)) {
        out.sso.meta = STR_MODE_SSO | 0;
        return out;
    }
    
    if (DS_UNLIKELY(length == 0)) {
        out.sso.meta = STR_MODE_SSO | 0;
        return out;
    }
    
    if (DS_UNLIKELY(length > total_len - start)) {
        length = total_len - start;
    }
    
    return ds_init_len(ds_data(s) + start, length);
}

// DSTRING: CLONE

DS_ALWAYS_INLINE
dstring ds_clone(const dstring* s) {
    if (DS_UNLIKELY(!s || !ds_ok(s))) return ds_make_error();
    return ds_init_len(ds_data(s), ds_len(s));
}

// DSTRING: PUSH CHARACTER

DS_ALWAYS_INLINE
dstring ds_push(const dstring* DS_RESTRICT s, char c) {
    dstring out = DS_INIT;
    
    if (DS_UNLIKELY(!s || !ds_ok(s))) {
        return ds_make_error();
    }
    
    uint32_t old_len = ds_len(s);
    
    if (DS_UNLIKELY(old_len >= 0x7FFFFFFE)) {
        return ds_make_error();
    }
    
    const char* data = ds_data(s);
    uint32_t new_len = old_len + 1;
    
    if (DS_LIKELY(new_len <= STR_SSO_MAX)) {
        // SSO path
        memcpy(out.sso.small, data, old_len);
        out.sso.small[old_len] = c;
        out.sso.small[new_len] = '\0';
        out.sso.meta = STR_MODE_SSO | (uint8_t)new_len;
    } else {
        // Heap path
        out.heap.ptr = (char*)malloc(new_len + 1);
        if (DS_UNLIKELY(out.heap.ptr == NULL)) {
            return ds_make_error();
        }
        memcpy(out.heap.ptr, data, old_len);
        out.heap.ptr[old_len] = c;
        out.heap.ptr[new_len] = '\0';
        out.heap.len = new_len;
        out.heap.hash = 0;
    }
    
    return out;
}

// DSTRING: COMPARISON

DS_ALWAYS_INLINE DS_PURE
int ds_cmp(const dstring* DS_RESTRICT a, const dstring* DS_RESTRICT b) {
    if (DS_UNLIKELY(a == b)) return 0;
    if (DS_UNLIKELY(!a || !b || !ds_ok(a) || !ds_ok(b))) {
        if (a && ds_ok(a)) return 1;
        if (b && ds_ok(b)) return -1;
        return 0;
    }
    
    uint32_t la = ds_len(a);
    uint32_t lb = ds_len(b);
    if (DS_UNLIKELY(la != lb)) {
        return (la > lb) ? 1 : -1;
    }
    
    // Fast path for large strings using cached hashes
    if (DS_LIKELY(la > 32) && ds_is_heap(a) && ds_is_heap(b)) {
#if DS_THREAD_SAFE
        uint32_t hash_a = atomic_load_explicit(&a->heap.hash, memory_order_acquire);
        uint32_t hash_b = atomic_load_explicit(&b->heap.hash, memory_order_acquire);
#else
        uint32_t hash_a = a->heap.hash;
        uint32_t hash_b = b->heap.hash;
#endif
        if (hash_a != 0 && hash_b != 0 && hash_a != hash_b) {
            return hash_a < hash_b ? -1 : 1;
        }
    }
    
    return memcmp(ds_data(a), ds_data(b), la);
}

// DSTRING: UTILITY FUNCTIONS

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
    if (DS_UNLIKELY(!s || !ds_ok(s))) return ds_make_error();
    
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

// DSTRING_VIEW: CREATION

DS_ALWAYS_INLINE
dstring_view dsv_from_dstring(const dstring* s) {
    dstring_view view = DSV_INIT;
    if (DS_LIKELY(s && ds_ok(s))) {
        view.data = ds_data(s);
        view.len = ds_len(s);
        
        if (ds_is_heap(s)) {
#if DS_THREAD_SAFE
            view.hash = atomic_load_explicit(&s->heap.hash, memory_order_acquire);
#else
            view.hash = s->heap.hash;
#endif
        }
    }
    return view;
}

DS_ALWAYS_INLINE
dstring_view dsv_from_cstr(const char* str) {
    dstring_view view = DSV_INIT;
    if (DS_LIKELY(str != NULL)) {
        view.data = str;
        view.len = (uint32_t)strlen(str);
    }
    return view;
}

DS_ALWAYS_INLINE
dstring_view dsv_from_buffer(const char* data, uint32_t len) {
    dstring_view view = DSV_INIT;
    if (DS_LIKELY(data != NULL && len > 0)) {
        view.data = data;
        view.len = len;
    }
    return view;
}

// DSTRING_VIEW: BASIC OPERATIONS

DS_ALWAYS_INLINE DS_PURE
bool dsv_ok(const dstring_view* view) {
    return view != NULL && (view->len == 0 || view->data != NULL);
}

DS_ALWAYS_INLINE DS_PURE DS_RETURNS_NONNULL
const char* dsv_data(const dstring_view* view) {
    return (view != NULL && view->data != NULL) ? view->data : "";
}

DS_ALWAYS_INLINE DS_PURE
uint32_t dsv_len(const dstring_view* view) {
    return view != NULL ? view->len : 0;
}

DS_ALWAYS_INLINE DS_PURE
bool dsv_empty(const dstring_view* view) {
    return view == NULL || view->len == 0;
}

// DSTRING_VIEW: HASH COMPUTATION

DS_ALWAYS_INLINE DS_PURE
bool dsv_has_hash(const dstring_view* view) {
    return view != NULL && view->hash != 0;
}

DS_NO_INLINE DS_COLD
static uint32_t dsv_compute_hash_slow(dstring_view* view) {
    if (view != NULL && view->len > 0 && view->data != NULL) {
        view->hash = ds_compute_hash(view->data, view->len);
    }
    return view != NULL ? view->hash : 0;
}

DS_ALWAYS_INLINE
uint32_t dsv_hash(dstring_view* view) {
    if (DS_LIKELY(view != NULL && view->hash != 0)) {
        return view->hash;
    }
    return dsv_compute_hash_slow(view);
}

DS_ALWAYS_INLINE DS_PURE
uint32_t dsv_hash_const(const dstring_view* view) {
    if (view == NULL || view->len == 0 || view->data == NULL) return 0;
    
    if (view->hash != 0) {
        return view->hash;
    }
    return ds_compute_hash(view->data, view->len);
}

DS_ALWAYS_INLINE
void dsv_cache_hash(dstring_view* view) {
    if (view != NULL && view->len > 0 && view->data != NULL && view->hash == 0) {
        view->hash = ds_compute_hash(view->data, view->len);
    }
}

DS_ALWAYS_INLINE
void dsv_invalidate_hash(dstring_view* view) {
    if (view != NULL) view->hash = 0;
}

// DSTRING_VIEW: SUBSTRING

DS_ALWAYS_INLINE
dstring_view dsv_sub(const dstring_view* DS_RESTRICT view, 
                     uint32_t start, 
                     uint32_t length) {
    dstring_view result = DSV_INIT;
    
    if (DS_UNLIKELY(view == NULL || view->data == NULL || start >= view->len)) {
        return result;
    }
    
    uint32_t max_len = view->len - start;
    length = (length < max_len) ? length : max_len;
    
    result.data = view->data + start;
    result.len = length;
    
    return result;
}

// DSTRING_VIEW: MODIFICATION

DS_ALWAYS_INLINE
void dsv_remove_prefix(dstring_view* view, uint32_t n) {
    if (DS_UNLIKELY(view == NULL || view->data == NULL || n > view->len)) return;
    view->data += n;
    view->len -= n;
    view->hash = 0;
}

DS_ALWAYS_INLINE
void dsv_remove_suffix(dstring_view* view, uint32_t n) {
    if (DS_UNLIKELY(view == NULL || view->data == NULL || n > view->len)) return;
    view->len -= n;
    view->hash = 0;
}

// DSTRING_VIEW: COMPARISON

DS_ALWAYS_INLINE
int dsv_cmp(dstring_view* a, dstring_view* b) {
    if (DS_UNLIKELY(a == b)) return 0;
    if (DS_UNLIKELY(a == NULL || b == NULL)) return (a != NULL) ? 1 : -1;
    if (DS_UNLIKELY(a->data == NULL || b->data == NULL)) {
        if (a->data != NULL) return 1;
        if (b->data != NULL) return -1;
        return 0;
    }
    
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
    if (DS_UNLIKELY(a == NULL || b == NULL)) return false;
    if (DS_UNLIKELY(a->data == NULL || b->data == NULL)) {
        return a->len == 0 && b->len == 0;
    }
    if (DS_UNLIKELY(a->len != b->len)) return false;
    
    if (DS_LIKELY(a->len > 32)) {
        uint32_t hash_a = dsv_hash(a);
        uint32_t hash_b = dsv_hash(b);
        if (DS_UNLIKELY(hash_a != hash_b)) return false;
    }
    
    return memcmp(a->data, b->data, a->len) == 0;
}

// DSTRING_VIEW: SEARCH OPERATIONS

DS_ALWAYS_INLINE DS_PURE
int32_t dsv_find(const dstring_view* view, char c) {
    if (DS_UNLIKELY(view == NULL || view->data == NULL)) return -1;
    
    const char* result = memchr(view->data, c, view->len);
    return result != NULL ? (int32_t)(result - view->data) : -1;
}

DS_ALWAYS_INLINE DS_PURE
int32_t dsv_rfind(const dstring_view* view, char c) {
    if (DS_UNLIKELY(view == NULL || view->data == NULL)) return -1;
    
    for (int32_t i = (int32_t)view->len - 1; i >= 0; i--) {
        if (view->data[i] == c) return i;
    }
    return -1;
}

DS_ALWAYS_INLINE
int32_t dsv_find_sub(dstring_view* haystack, 
                     dstring_view* needle) {
    if (DS_UNLIKELY(haystack == NULL || needle == NULL)) return -1;
    if (DS_UNLIKELY(haystack->data == NULL || needle->data == NULL)) return -1;
    if (DS_UNLIKELY(needle->len == 0)) return 0;
    if (DS_UNLIKELY(needle->len > haystack->len)) return -1;
    
    if (needle->len == 1) {
        return dsv_find(haystack, needle->data[0]);
    }
    
    for (uint32_t i = 0; i <= haystack->len - needle->len; i++) {
        if (haystack->data[i] == needle->data[0] &&
            memcmp(haystack->data + i, needle->data, needle->len) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

// DSTRING_VIEW: UTILITY FUNCTIONS

DS_ALWAYS_INLINE
dstring dsv_to_dstring(const dstring_view* view) {
    if (DS_UNLIKELY(view == NULL || !dsv_ok(view))) {
        return ds_make_error();
    }
    
    dstring result = ds_init_len(view->data, view->len);
    
    if (view->hash != 0 && ds_is_heap(&result)) {
#if DS_THREAD_SAFE
        atomic_store_explicit(&result.heap.hash, view->hash, memory_order_release);
#else
        result.heap.hash = view->hash;
#endif
    }
    
    return result;
}

DS_ALWAYS_INLINE
dstring_view dsv_split_at(dstring_view* view, char delimiter) {
    dstring_view token = DSV_INIT;
    
    if (DS_UNLIKELY(view == NULL || view->data == NULL || view->len == 0)) {
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
    if (view == NULL || prefix == NULL) return false;
    if (view->data == NULL || prefix->data == NULL) return false;
    if (prefix->len > view->len) return false;
    return memcmp(view->data, prefix->data, prefix->len) == 0;
}

DS_ALWAYS_INLINE DS_PURE
bool dsv_ends_with(dstring_view* view, 
                   dstring_view* suffix) {
    if (view == NULL || suffix == NULL) return false;
    if (view->data == NULL || suffix->data == NULL) return false;
    if (suffix->len > view->len) return false;
    return memcmp(view->data + view->len - suffix->len, 
                  suffix->data, suffix->len) == 0;
}

DS_ALWAYS_INLINE
dstring_view dsv_trim(const dstring_view* view) {
    dstring_view result = DSV_INIT;
    if (DS_UNLIKELY(view == NULL || view->data == NULL)) return result;
    
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

// DSTRING_ARENA: BASIC OPERATIONS

DS_ALWAYS_INLINE DS_PURE DS_RETURNS_NONNULL
const char* dsa_data(const dstring_arena* arena) {
    return (arena != NULL && arena->data != NULL) ? arena->data : "";
}

DS_ALWAYS_INLINE DS_PURE
uint32_t dsa_len(const dstring_arena* arena) {
    return arena != NULL ? arena->len : 0;
}

DS_ALWAYS_INLINE DS_PURE
bool dsa_empty(const dstring_arena* arena) {
    return arena == NULL || arena->len == 0;
}

DS_ALWAYS_INLINE DS_PURE
uint32_t dsa_capacity(const dstring_arena* arena) {
    return arena != NULL ? arena->capacity : 0;
}

// DSTRING_ARENA: CREATION AND DESTRUCTION

DS_ALWAYS_INLINE
dstring_arena dsa_create(uint32_t initial_capacity) {
    dstring_arena arena = DSA_INIT;
    
    if (initial_capacity > 0) {
        if (initial_capacity < DSA_MIN_CAPACITY) {
            initial_capacity = DSA_MIN_CAPACITY;
        }
        
        arena.data = (char*)malloc(initial_capacity);
        if (DS_LIKELY(arena.data != NULL)) {
            arena.data[0] = '\0';
            arena.capacity = initial_capacity;
        }
    }
    
    return arena;
}

DS_ALWAYS_INLINE
void dsa_free(dstring_arena* arena) {
    if (arena != NULL && arena->data != NULL) {
        free(arena->data);
    }
    if (arena != NULL) {
        arena->data = NULL;
        arena->len = 0;
        arena->capacity = 0;
    }
}

// DSTRING_ARENA: MANUAL EXTENSION

DS_NO_INLINE DS_COLD
static bool dsa_extend_by_multiplier(dstring_arena* arena, float multiplier) {
    if (arena == NULL || arena->data == NULL || multiplier <= 1.0f) return false;
    
    if (arena->capacity > UINT32_MAX / multiplier) {
        return false;
    }
    
    uint32_t new_capacity = (uint32_t)(arena->capacity * multiplier);
    
    if (new_capacity <= arena->capacity) {
        if (arena->capacity > UINT32_MAX - DSA_MIN_CAPACITY) {
            return false;
        }
        new_capacity = arena->capacity + DSA_MIN_CAPACITY;
    }
    
    char* new_data = (char*)realloc(arena->data, new_capacity);
    if (new_data == NULL) return false;
    
    arena->data = new_data;
    arena->capacity = new_capacity;
    return true;
}

DS_ALWAYS_INLINE
bool dsa_extend_to(dstring_arena* arena, uint32_t required_capacity) {
    if (arena == NULL || arena->data == NULL) return false;
    
    if (arena->capacity >= required_capacity) return true;
    
    char* new_data = (char*)realloc(arena->data, required_capacity);
    if (new_data == NULL) return false;
    
    arena->data = new_data;
    arena->capacity = required_capacity;
    return true;
}

DS_NO_INLINE DS_COLD
static bool dsa_reserve_with_growth(dstring_arena* arena,
                                    uint32_t additional_bytes,
                                    float growth_factor) {
    if (arena == NULL || growth_factor <= 1.0f) return false;
    
    if (arena->len > UINT32_MAX - additional_bytes - 1) {
        return false;
    }
    
    uint32_t required = arena->len + additional_bytes + 1;
    
    if (arena->data != NULL && arena->capacity >= required) {
        return true;
    }
    
    uint32_t current_capacity = arena->capacity;
    if (current_capacity == 0) {
        current_capacity = DSA_MIN_CAPACITY;
    }
    
    uint32_t new_capacity = current_capacity;
    while (new_capacity < required) {
        if (new_capacity > UINT32_MAX / growth_factor) {
            new_capacity = required;
            break;
        }
        new_capacity = (uint32_t)(new_capacity * growth_factor);
        if (new_capacity <= current_capacity) {
            new_capacity = required;
        }
    }
    
    if (arena->data == NULL) {
        arena->data = (char*)malloc(new_capacity);
        if (arena->data == NULL) return false;
        arena->data[0] = '\0';
    } else {
        char* new_data = (char*)realloc(arena->data, new_capacity);
        if (new_data == NULL) return false;
        arena->data = new_data;
    }
    
    arena->capacity = new_capacity;
    return true;
}

DS_ALWAYS_INLINE
bool dsa_reserve(dstring_arena* arena, uint32_t additional_bytes) {
    return dsa_reserve_with_growth(arena, additional_bytes, 1.5f);
}

// DSTRING_ARENA: APPENDING OPERATIONS

DS_ALWAYS_INLINE
bool dsa_append_len(dstring_arena* DS_RESTRICT arena, 
                    const char* DS_RESTRICT data, 
                    uint32_t len) {
    if (DS_UNLIKELY(arena == NULL || data == NULL || len == 0)) return false;
    
    if (DS_UNLIKELY(arena->len > UINT32_MAX - len - 1)) {
        return false;
    }
    
    if (DS_LIKELY(arena->data != NULL && arena->len + len < arena->capacity)) {
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
    if (DS_UNLIKELY(arena == NULL || data == NULL || len == 0)) return false;
    
    if (DS_UNLIKELY(arena->len > UINT32_MAX - len - 1)) {
        return false;
    }
    
    if (DS_LIKELY(arena->data != NULL && arena->len + len < arena->capacity)) {
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
    if (DS_UNLIKELY(arena == NULL || str == NULL)) return false;
    return dsa_append_len(arena, str, (uint32_t)strlen(str));
}

DS_ALWAYS_INLINE
bool dsa_push(dstring_arena* arena, char c) {
    if (DS_UNLIKELY(arena == NULL)) return false;
    return dsa_append_len(arena, &c, 1);
}

DS_ALWAYS_INLINE
bool dsa_append_dstring(dstring_arena* arena, const dstring* s) {
    if (DS_UNLIKELY(arena == NULL || s == NULL || !ds_ok(s))) return false;
    return dsa_append_len(arena, ds_data(s), ds_len(s));
}

DS_ALWAYS_INLINE
bool dsa_append_view(dstring_arena* arena, const dstring_view* view) {
    if (DS_UNLIKELY(arena == NULL || view == NULL || !dsv_ok(view))) return false;
    return dsa_append_len(arena, view->data, view->len);
}

// DSTRING_ARENA: CONVERSION

DS_ALWAYS_INLINE
dstring dsa_to_dstring(const dstring_arena* arena) {
    if (DS_UNLIKELY(arena == NULL || arena->data == NULL)) {
        return ds_make_error();
    }
    return ds_init_len(arena->data, arena->len);
}

DS_ALWAYS_INLINE
dstring_view dsa_to_view(const dstring_arena* arena) {
    dstring_view view = DSV_INIT;
    if (arena != NULL && arena->data != NULL) {
        view.data = arena->data;
        view.len = arena->len;
    }
    return view;
}

// DSTRING_ARENA: OPERATIONS

DS_ALWAYS_INLINE
void dsa_clear(dstring_arena* arena) {
    if (arena != NULL && arena->data != NULL) {
        arena->len = 0;
        arena->data[0] = '\0';
    }
}

DS_ALWAYS_INLINE
void dsa_shrink_to_fit(dstring_arena* arena) {
    if (arena == NULL || arena->data == NULL) return;
    
    uint32_t new_capacity = arena->len + 1;
    if (new_capacity == arena->capacity) return;
    
    char* new_data = (char*)realloc(arena->data, new_capacity);
    if (new_data != NULL) {
        arena->data = new_data;
        arena->capacity = new_capacity;
    }
}

DS_ALWAYS_INLINE DS_PURE DS_RETURNS_NONNULL
const char* dsa_cstr(const dstring_arena* arena) {
    return dsa_data(arena);
}

#endif // DSTRING_H