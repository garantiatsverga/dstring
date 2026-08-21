## dstring.h — Lightweight C String Library

A minimal, high-performance string library for C with **Small String Optimization (SSO)** and a built-in hash. Designed for systems where every byte counts.

---

## Overview

| Feature | Description |
|---------|-------------|
| **Size** | 16 bytes (fits in cache line) |
| **SSO** | Up to 14 chars stored inline (no malloc) |
| **Hash** | Built-in XOR-based hash (O(1)) |
| **Binary safe** | Supports embedded nulls |
| **Error handling** | Silent failure with error flag |
| **Portability** | C99/C11, no external dependencies |

---

## API Reference

### Types

```c
typedef struct string string;
```

### Constants

```c
#define STR_SSO_MAX 14          // Max inline characters
#define STR_SSO_FLAG 0x80       // SSO marker bit
#define STR_INIT { .small = {0}, .sso_len = 0 }
```

---

### Core Functions

| Function | Time | Description |
|----------|------|-------------|
| `string initstr(const char* s)` | O(n) | Create from C string (calls strlen) |
| `string initstr_len(const char* s, uint32_t len)` | O(1) | Create from C string with known length |
| `void freestr(string* s)` | O(1) | Free heap memory, zero struct |
| `const char* strdata(const string* s)` | O(1) | Get raw data pointer (null-safe) |
| `uint32_t strlen_s(const string* s)` | O(1) | Get length |
| `bool is_sso(const string* s)` | O(1) | Check if string is inline |
| `bool strok(const string* s)` | O(1) | Check if valid (no error) |
| `bool strempty(const string* s)` | O(1) | Check if empty |

---

### Manipulation

| Function | Time | Description |
|----------|------|-------------|
| `string strcat_s(const string* a, const string* b)` | O(n) | Concatenate two strings |
| `string strsub(const string* s, uint32_t start, uint32_t len)` | O(n) | Extract substring |
| `string strclone(const string* s)` | O(n) | Deep copy |
| `string strpush(const string* s, char c)` | O(n) | Append single char (creates new string) |
| `int strcmp_s(const string* a, const string* b)` | O(n) | Lexicographic comparison |
| `uint32_t strhash(const string* s)` | O(1) | Compute hash (cached for SSO) |

---

## Performance Guarantees

### Time Complexities

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `initstr_len()` | O(1) | Only if len is known |
| `initstr()` | O(n) | Calls strlen() |
| `strlen_s()` | O(1) | Direct field access |
| `strdata()` | O(1) | Pointer dereference |
| `strhash()` | O(1) | XOR of pointers (constant) |
| `strcat_s()` | O(n) | n = len(a) + len(b) |
| `strsub()` | O(n) | n = substring length |
| `strclone()` | O(n) | n = string length |
| `strpush()` | O(n) | n = new length |
| `freestr()` | O(1) | Single free, then memset |

### Memory Bounds

| Scenario | Allocation | Bytes |
|----------|------------|-------|
| `len <= 14` | None (SSO) | 16 bytes on stack |
| `len > 14` | 1 malloc | 16 bytes struct + len + 1 heap |
| Error state | None | 16 bytes, all zero |

---

## Usage Examples

### ✅ Good: Batch Concatenation

```c
// Build once, allocate once
char buffer[4096];
int pos = 0;

for (int i = 0; i < 100000; i++) {
    pos += snprintf(buffer + pos, sizeof(buffer) - pos, "%d,", i);
}

string s = initstr_len(buffer, pos);
// ... use s ...
freestr(&s);
```

**Why:** One malloc, one free. O(n) for the whole batch, not O(n²).

### ❌ Bad: Per-Char Push

```c
string s = initstr("");
for (int i = 0; i < 100000; i++) {
    s = strpush(&s, 'A' + (i % 26));  // 100,000 mallocs
    freestr(&old);
}
```

**Why:** Each push allocates new memory. O(n²) time, huge fragmentation.

### ✅ Good: Substring Extraction

```c
string full = initstr("Hello, World!");
string part = strsub(&full, 7, 5);  // "World"
printf("%s\n", strdata(&part));
freestr(&part);
freestr(&full);
```

### ✅ Good: Hash-Based Comparisons

```c
string a = initstr("key");
string b = initstr("key");

if (strhash(&a) == strhash(&b) && strcmp_s(&a, &b) == 0) {
    // Fast path: hash check first, then memcmp
}

freestr(&a);
freestr(&b);
```

---

## Error Handling

All functions are **silent on failure**. Check `strok()` after operations.

| Scenario | Behavior |
|----------|----------|
| `initstr(NULL)` | Returns empty string, strok = true |
| `malloc()` fails | Returns error string, strok = false |
| `len > UINT_MAX` | Returns error string, strok = false |
| `strdata()` on error | Returns `""` (safe, never NULL) |
| `strlen_s()` on error | Returns `0` |

```c
string s = initstr_len(data, len);
if (!strok(&s)) {
    // Handle error: allocation failed or invalid len
}
```

---

## Benchmark Results

On **Intel Pentium P6200 @ 2.13 GHz** (2010) with `-O3`:

| Test | ops/sec |
|------|---------|
| SSO creation (14 chars) | 920,000,000 |
| Heap creation (100 chars) | 38,000,000 |
| `strlen_s()` (O(1)) | 2,170,000,000 |
| `strlen()` (O(n)) | 144,000,000 |
| `strhash()` (XOR) | 385,000,000 |
| `strcat_s()` (SSO) | 36,000,000 |
| Mass allocation (33M strings) | 40,000,000 |

---

## Comparison

| Metric | `dstring` | `std::string` (C++) | SDS (Redis) |
|--------|-----------|----------------------|-------------|
| **Size (bytes)** | 16 | 24 (32bit) or 32 (64bit) | 3–17 (depends on length) |
| **SSO limit (chars)** | 14 | ~15 | None |
| **Hash built-in** | ✅ Yes | ❌ No | ❌ No |
| **Error handling** | Silent (strok) | Exceptions | Silent (null on fail) |
| **Binary safe** | ✅ Yes | ✅ Yes | ✅ Yes |
| **Heap allocation overhead** | 16-byte struct + len | 32-byte struct | 3–17 byte header |
| **`initstr()` / `sdsnew()` (SSO)** | 920,000,000 ops/s | 86,600,000 ops/s | 28,600,000 ops/s |
| **`initstr()` / `sdsnew()` (Heap)** | 38,000,000 ops/s | 25,000,000 ops/s | 32,700,000 ops/s |
| **`strlen_s()` / `sdslen()` (O(1))** | 2,170,000,000 ops/s | 2,110,000,000 ops/s | 2,130,000,000 ops/s |
| **`strlen()` (O(n)) — 5 chars** | 2,170,000,000 ops/s *(using `strlen_s`)* | 210,000,000 ops/s | 236,000,000 ops/s |
| **`strlen()` (O(n)) — 22 chars** | 2,170,000,000 ops/s *(using `strlen_s`)* | 140,000,000 ops/s | 141,000,000 ops/s |
| **Hash computation** | 385,000,000 ops/s | 75,000,000–140,000,000 ops/s *(custom FNV-1a)* | Not applicable |
| **Concatenation (1 char, 100k iterations)** | 244,000 ops/s | 175,000,000 ops/s | 36,700,000 ops/s |
| **Mass allocation (10M strings)** | 40,000,000 ops/s | 50,500,000 ops/s | 15,400,000 ops/s |
| **Copy / clone** | 23,000,000 ops/s | 25,000,000 ops/s | 22,600,000 ops/s |
| **Memory fragmentation risk** | Low (exact fit) | Medium (geometric growth) | Low (exact fit) |
| **C++/C compatibility** | C only | C++ only | C only |
| **Header-only** | ✅ Yes | ✅ Yes (STL) | ❌ No (requires `sds.c`) |
| **Dependencies** | None (aside of libc) | None (aside of STL) | `sys/types.h` |
*All benchmarks measured on Intel Pentium P6200 @ 2.13 GHz, GCC 15.2.0 `-O3`. `std::string` concat uses `reserve()` to avoid reallocations; `dstring` and SDS use exact allocation.*

*You can run the tests for dstring.h on your own: they are provided in the 'tests/' directory, and can be launched by running 'python build.py'.*
**Make sure you have at least 2+ GB of free RAM before launching the stress test!**
---

## Portability

| Platform | Support |
|----------|---------|
| Linux / BSD | ✅ Full |
| Windows (MinGW/MSVC) | ✅ Full |
| macOS | ✅ Full |
| ARM / ARM64 | ✅ Full |
| AVR / ESP32 | ✅ With constraints |

Compiles with:
- GCC 4.8+
- Clang 3.0+
- MSVC 2015+
- TinyCC
- Any C99/C11 compiler

---

## License

MIT / Public Domain — use as you wish.