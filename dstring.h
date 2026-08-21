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
#define STR_SSO_FLAG   0x80
#define STR_SSO_LEN_MASK 0x7F

typedef struct {
    union {
        // Heap mode (long strings)
        struct {
            char* ptr;
            uint32_t len;
            uint32_t capacity;
        };
        // SSO mode (short strings, <= 14 chars)
        struct {
            char small[STR_SSO_MAX + 1];  // 15 bytes: 14 chars + '\0'
            uint8_t sso_len;              // 1 byte: flag | length (0..14)
        };
    };
} string;  // 16 bytes total

#define STR_INIT { .small = {0}, .sso_len = 0 }

// === Basic functions ===

static inline bool is_sso(const string* s) {
    return s != NULL && (s->sso_len & STR_SSO_FLAG) != 0;
}

static inline bool strok(const string* s) {
    return s != NULL && s->sso_len != 0xFF;
}

static inline const char* strdata(const string* s) {
    if (s == NULL || !strok(s)) return "";
    if (is_sso(s)) {
        return s->small;
    } else {
        return s->ptr ? s->ptr : "";
    }
}

static inline uint32_t strlen_s(const string* s) {
    if (s == NULL || !strok(s)) return 0;
    if (is_sso(s)) {
        return s->sso_len & STR_SSO_LEN_MASK;
    } else {
        return s->len;
    }
}

static inline bool strempty(const string* s) {
    return s == NULL || strlen_s(s) == 0;
}

static inline uint32_t strhash(const string* s) {
    if (s == NULL || !strok(s)) return 0;

    const char* data = strdata(s);
    uint32_t len = strlen_s(s);

    uintptr_t data_ptr = (uintptr_t)data;
    uintptr_t len_ptr = (uintptr_t)(is_sso(s) ?
        (const void*)&s->sso_len :
        (const void*)&s->len);

    uint32_t hash = (uint32_t)(data_ptr ^ len_ptr ^ (uintptr_t)len);

    if (len >= 2) {
        hash ^= (uint8_t)data[0] | ((uint8_t)data[1] << 8);
    } else if (len == 1) {
        hash ^= (uint8_t)data[0];
    }

    hash ^= hash >> 16;
    hash *= 0x9e3779b9;
    hash ^= hash >> 16;

    return hash;
}

// === Initialization ===

static inline string initstr_len(const char* str, uint32_t len) {
    string s = STR_INIT;

    if (str == NULL || len == 0) return s;

    // Проверка переполнения (uint32_t не может превысить SIZE_MAX на 64-bit)
    if (len >= UINT_MAX) {
        s.sso_len = 0xFF;
        return s;
    }

    if (len <= STR_SSO_MAX) {
        // SSO mode: 14 chars max
        memcpy(s.small, str, len);
        s.small[len] = '\0';
        s.sso_len = STR_SSO_FLAG | (uint8_t)len;
    } else {
        // Heap mode
        s.ptr = (char*)malloc(len + 1);
        if (s.ptr == NULL) {
            s.sso_len = 0xFF;
            return s;
        }
        memcpy(s.ptr, str, len);
        s.ptr[len] = '\0';
        s.len = len;
        s.capacity = len;
    }
    return s;
}

static inline string initstr(const char* str) {
    if (str == NULL) return (string)STR_INIT;
    return initstr_len(str, (uint32_t)strlen(str));
}

// === Free ===

static inline void freestr(string* s) {
    if (s == NULL) return;
    if (!is_sso(s) && s->ptr) {
        free(s->ptr);
        s->ptr = NULL;
    }
    memset(s, 0, sizeof(string));
}

// === Concatenation ===

static inline bool prepcat(const string* in1, const string* in2) {
    if (!in1 || !in2 || !strok(in1) || !strok(in2)) return false;
    uint64_t total = (uint64_t)strlen_s(in1) + strlen_s(in2) + 1;
    return total <= UINT_MAX;
}

static inline string strcat_s(const string* in1, const string* in2) {
    string out = STR_INIT;

    if (!prepcat(in1, in2)) {
        out.sso_len = 0xFF;
        return out;
    }

    uint32_t len1 = strlen_s(in1);
    uint32_t len2 = strlen_s(in2);
    const char* data1 = strdata(in1);
    const char* data2 = strdata(in2);
    uint32_t total = len1 + len2;

    if (total <= STR_SSO_MAX && total > 0) {
        memcpy(out.small, data1, len1);
        memcpy(out.small + len1, data2, len2);
        out.small[total] = '\0';
        out.sso_len = STR_SSO_FLAG | (uint8_t)total;
        return out;
    }

    out.ptr = (char*)malloc(total + 1);
    if (out.ptr == NULL) {
        out.sso_len = 0xFF;
        return out;
    }

    memcpy(out.ptr, data1, len1);
    memcpy(out.ptr + len1, data2, len2 + 1);

    out.len = total;
    out.capacity = total;

    return out;
}

// === Substring ===

static inline string strsub(const string* s, uint32_t start, uint32_t length) {
    string out = STR_INIT;

    if (!s || !strok(s)) {
        out.sso_len = 0xFF;
        return out;
    }

    uint32_t total_len = strlen_s(s);
    if (start > total_len || length == 0) return out;
    if (start + length > total_len) length = total_len - start;

    return initstr_len(strdata(s) + start, length);
}

// === Clone ===

static inline string strclone(const string* s) {
    if (!s || !strok(s)) return (string)STR_INIT;
    return initstr_len(strdata(s), strlen_s(s));
}

// === Comparison ===

static inline int strcmp_s(const string* a, const string* b) {
    if (a == b) return 0;
    if (!a || !b) return (a ? 1 : -1);

    const char* da = strdata(a);
    const char* db = strdata(b);
    uint32_t la = strlen_s(a);
    uint32_t lb = strlen_s(b);

    int cmp = memcmp(da, db, (la < lb) ? la : lb);
    if (cmp != 0) return cmp;
    return (la > lb) ? 1 : (la < lb) ? -1 : 0;
}

// === Push character ===

static inline string strpush(const string* s, char c) {
    string out = STR_INIT;

    if (!s || !strok(s)) {
        out.sso_len = 0xFF;
        return out;
    }

    uint32_t old_len = strlen_s(s);
    if (old_len >= UINT_MAX - 1) {
        out.sso_len = 0xFF;
        return out;
    }

    const char* data = strdata(s);
    uint32_t new_len = old_len + 1;

    if (new_len <= STR_SSO_MAX) {
        memcpy(out.small, data, old_len);
        out.small[old_len] = c;
        out.small[new_len] = '\0';
        out.sso_len = STR_SSO_FLAG | (uint8_t)new_len;
    } else {
        out.ptr = (char*)malloc(new_len + 1);
        if (out.ptr == NULL) {
            out.sso_len = 0xFF;
            return out;
        }
        memcpy(out.ptr, data, old_len);
        out.ptr[old_len] = c;
        out.ptr[new_len] = '\0';
        out.len = new_len;
        out.capacity = new_len;
    }

    return out;
}

#endif // DSTRING_H