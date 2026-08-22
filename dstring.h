#ifndef DSTRING_H
#define DSTRING_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <stddef.h>

// === SSO Configuration ===
#define STR_SSO_MAX    14               // Max inline chars (14)
#define STR_MODE_SSO   0x80             // High bit of mode byte
#define STR_LEN_MASK   0x3F             // Lower 6 bits for length (max 63)

typedef struct {
    union {
        // Heap mode (long strings) - 16 bytes on 64-bit
        struct {
            char* ptr;          // 8 bytes
            uint32_t len;       // 4 bytes
            uint32_t capacity;  // 4 bytes
        } heap;
        
        // SSO mode (short strings) - 16 bytes
        struct {
            char small[15];     // 15 bytes: 14 chars + '\0'
            uint8_t meta;       // 1 byte: mode | length
        } sso;
        
        // Raw access for safe type-punning
        uint8_t raw[16];
    };
} dstring;  // Exactly 16 bytes

#define DS_INIT { .raw = {0} }

// === Mode Detection (UB-free) ===

static inline uint8_t ds_mode(const dstring* s) {
    if (s == NULL) return 0;
    // Read the last byte (metadata byte in SSO, or last byte of capacity in heap)
    return s->raw[15];
}

static inline bool ds_is_sso(const dstring* s) {
    if (s == NULL) return false;
    uint8_t meta = ds_mode(s);
    // SSO if high bit is set and length field is valid
    return (meta & STR_MODE_SSO) && 
           ((meta & STR_LEN_MASK) <= STR_SSO_MAX);
}

static inline bool ds_is_heap(const dstring* s) {
    if (s == NULL) return false;
    uint8_t meta = ds_mode(s);
    // Heap mode if high bit is clear (capacity's high byte won't have bit 7 set
    // unless capacity > 2^31, which we prevent)
    return !(meta & STR_MODE_SSO) && s->heap.ptr != NULL;
}

static inline bool ds_ok(const dstring* s) {
    if (s == NULL) return false;
    if (ds_is_sso(s)) {
        return (s->sso.meta & STR_LEN_MASK) <= STR_SSO_MAX;
    }
    if (ds_is_heap(s)) {
        return s->heap.ptr != NULL && 
               s->heap.len > STR_SSO_MAX && 
               s->heap.capacity >= s->heap.len &&
               s->heap.capacity < (1U << 31);  // Ensure high bit of capacity is clear
    }
    return false;
}

// === Core Functions ===

static inline const char* ds_data(const dstring* s) {
    if (s == NULL || !ds_ok(s)) return "";
    return ds_is_sso(s) ? s->sso.small : (s->heap.ptr ? s->heap.ptr : "");
}

static inline uint32_t ds_len(const dstring* s) {
    if (s == NULL || !ds_ok(s)) return 0;
    return ds_is_sso(s) ? (s->sso.meta & STR_LEN_MASK) : s->heap.len;
}

static inline bool ds_empty(const dstring* s) {
    return s == NULL || ds_len(s) == 0;
}

static inline uint32_t ds_hash(const dstring* s) {
    if (s == NULL || !ds_ok(s)) return 0;
    
    const char* data = ds_data(s);
    uint32_t len = ds_len(s);
    
    // FNV-1a hash
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < len; i++) {
        hash ^= (uint8_t)data[i];
        hash *= 16777619u;
    }
    return hash;
}

// === Initialization ===

static inline dstring ds_init_len(const char* str, uint32_t len) {
    dstring s = DS_INIT;
    
    if (str == NULL || len == 0) {
        // Empty string in SSO mode
        s.sso.meta = STR_MODE_SSO | 0;
        return s;
    }
    
    // Prevent capacity from exceeding 2^31 (keeps high byte clear)
    if (len >= (1U << 31)) {
        // Too large - return invalid string
        s.raw[15] = 0xFF;  // Invalid marker
        return s;
    }
    
    if (len <= STR_SSO_MAX) {
        // SSO mode
        memcpy(s.sso.small, str, len);
        s.sso.small[len] = '\0';
        s.sso.meta = STR_MODE_SSO | (uint8_t)len;
    } else {
        // Heap mode
        s.heap.ptr = (char*)malloc(len + 1);
        if (s.heap.ptr == NULL) {
            s.raw[15] = 0xFF;  // Invalid marker
            return s;
        }
        memcpy(s.heap.ptr, str, len);
        s.heap.ptr[len] = '\0';
        s.heap.len = len;
        s.heap.capacity = len;  // High byte will be 0 for len < 2^31
    }
    return s;
}

static inline dstring ds_init(const char* str) {
    if (str == NULL) return (dstring)DS_INIT;
    return ds_init_len(str, (uint32_t)strlen(str));
}

// === Free ===

static inline void ds_free(dstring* s) {
    if (s == NULL) return;
    if (ds_is_heap(s) && s->heap.ptr) {
        free(s->heap.ptr);
    }
    memset(s, 0, sizeof(dstring));
}

// === Concatenation ===

static inline bool ds_cat_ok(const dstring* in1, const dstring* in2) {
    if (!in1 || !in2 || !ds_ok(in1) || !ds_ok(in2)) return false;
    uint64_t total = (uint64_t)ds_len(in1) + ds_len(in2) + 1;
    return total <= (1U << 31) - 1;  // Keep high byte clear
}

static inline dstring ds_cat(const dstring* in1, const dstring* in2) {
    dstring out = DS_INIT;
    
    if (!ds_cat_ok(in1, in2)) {
        out.raw[15] = 0xFF;
        return out;
    }
    
    uint32_t len1 = ds_len(in1);
    uint32_t len2 = ds_len(in2);
    const char* data1 = ds_data(in1);
    const char* data2 = ds_data(in2);
    uint32_t total = len1 + len2;
    
    if (total <= STR_SSO_MAX && total > 0) {
        memcpy(out.sso.small, data1, len1);
        memcpy(out.sso.small + len1, data2, len2);
        out.sso.small[total] = '\0';
        out.sso.meta = STR_MODE_SSO | (uint8_t)total;
        return out;
    }
    
    out.heap.ptr = (char*)malloc(total + 1);
    if (out.heap.ptr == NULL) {
        out.raw[15] = 0xFF;
        return out;
    }
    
    memcpy(out.heap.ptr, data1, len1);
    memcpy(out.heap.ptr + len1, data2, len2 + 1);
    
    out.heap.len = total;
    out.heap.capacity = total;
    
    return out;
}

// === Substring ===

static inline dstring ds_sub(const dstring* s, uint32_t start, uint32_t length) {
    dstring out = DS_INIT;
    
    if (!s || !ds_ok(s)) {
        out.raw[15] = 0xFF;
        return out;
    }
    
    uint32_t total_len = ds_len(s);
    if (start > total_len || length == 0) {
        // Return empty string
        out.sso.meta = STR_MODE_SSO | 0;
        return out;
    }
    if (start + length > total_len) length = total_len - start;
    
    return ds_init_len(ds_data(s) + start, length);
}

// === Clone ===

static inline dstring ds_clone(const dstring* s) {
    if (!s || !ds_ok(s)) return (dstring)DS_INIT;
    return ds_init_len(ds_data(s), ds_len(s));
}

// === Comparison ===

static inline int ds_cmp(const dstring* a, const dstring* b) {
    if (a == b) return 0;
    if (!a || !b) return (a ? 1 : -1);
    
    const char* da = ds_data(a);
    const char* db = ds_data(b);
    uint32_t la = ds_len(a);
    uint32_t lb = ds_len(b);
    
    int cmp = memcmp(da, db, (la < lb) ? la : lb);
    if (cmp != 0) return cmp;
    return (la > lb) ? 1 : (la < lb) ? -1 : 0;
}

// === Push character ===

static inline dstring ds_push(const dstring* s, char c) {
    dstring out = DS_INIT;
    
    if (!s || !ds_ok(s)) {
        out.raw[15] = 0xFF;
        return out;
    }
    
    uint32_t old_len = ds_len(s);
    if (old_len >= (1U << 31) - 2) {
        out.raw[15] = 0xFF;
        return out;
    }
    
    const char* data = ds_data(s);
    uint32_t new_len = old_len + 1;
    
    if (new_len <= STR_SSO_MAX) {
        memcpy(out.sso.small, data, old_len);
        out.sso.small[old_len] = c;
        out.sso.small[new_len] = '\0';
        out.sso.meta = STR_MODE_SSO | (uint8_t)new_len;
    } else {
        out.heap.ptr = (char*)malloc(new_len + 1);
        if (out.heap.ptr == NULL) {
            out.raw[15] = 0xFF;
            return out;
        }
        memcpy(out.heap.ptr, data, old_len);
        out.heap.ptr[old_len] = c;
        out.heap.ptr[new_len] = '\0';
        out.heap.len = new_len;
        out.heap.capacity = new_len;
    }
    
    return out;
}

// === Additional Utility Functions ===

// Convert dstring to C string (returns internal buffer, don't free)
static inline const char* ds_cstr(const dstring* s) {
    return ds_data(s);
}

// Check if string contains a character
static inline bool ds_contains(const dstring* s, char c) {
    if (!s || !ds_ok(s)) return false;
    const char* data = ds_data(s);
    uint32_t len = ds_len(s);
    for (uint32_t i = 0; i < len; i++) {
        if (data[i] == c) return true;
    }
    return false;
}

// Find first occurrence of character
static inline int32_t ds_find(const dstring* s, char c) {
    if (!s || !ds_ok(s)) return -1;
    const char* data = ds_data(s);
    uint32_t len = ds_len(s);
    for (uint32_t i = 0; i < len; i++) {
        if (data[i] == c) return (int32_t)i;
    }
    return -1;
}

// Trim whitespace from both ends
static inline dstring ds_trim(const dstring* s) {
    if (!s || !ds_ok(s)) return (dstring)DS_INIT;
    
    const char* data = ds_data(s);
    uint32_t len = ds_len(s);
    uint32_t start = 0;
    uint32_t end = len;
    
    // Trim leading whitespace
    while (start < end && (data[start] == ' ' || data[start] == '\t' || 
           data[start] == '\n' || data[start] == '\r')) {
        start++;
    }
    
    // Trim trailing whitespace
    while (end > start && (data[end-1] == ' ' || data[end-1] == '\t' || 
           data[end-1] == '\n' || data[end-1] == '\r')) {
        end--;
    }
    
    return ds_init_len(data + start, end - start);
}

#endif // DSTRING_H