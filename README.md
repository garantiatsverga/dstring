# dstring.h — Lightweight C String Library

A minimal, high-performance string library for C with **Small String Optimization (SSO)** and a built-in cached hash. Designed for systems where every byte counts.

---

## Overview

| Feature | Description |
|---------|-------------|
| **Size** | 16 bytes (fits in cache line) |
| **SSO** | Up to 14 chars stored inline (no malloc) |
| **Hash** | Built-in FNV-1a hash (cached for heap strings, O(1) access) |
| **Binary safe** | Supports embedded nulls |
| **Error handling** | Silent failure with error flag |
| **Max length** | 2^31 - 1 chars (~2.1 GB) |
| **Portability** | C99/C11, no external dependencies |

---

## API Reference

### Types

```c
typedef struct dstring dstring;
```

### Constants

```c
#define STR_SSO_MAX 14          // Max inline characters
#define STR_MODE_SSO 0x80       // SSO marker bit in meta byte
#define STR_LEN_MASK 0x3F       // SSO length mask (lower 6 bits)
#define DS_INIT { .raw = {0} }  // Initializer
```

---

### Core Functions

| Function | Time | Description |
|----------|------|-------------|
| `dstring ds_init(const char* s)` | O(n) | Create from C string (calls strlen) |
| `dstring ds_init_len(const char* s, uint32_t len)` | O(n) | Create from buffer with known length |
| `void ds_free(dstring* s)` | O(1) | Free heap memory, zero struct |
| `const char* ds_data(const dstring* s)` | O(1) | Get raw data pointer (null-safe) |
| `uint32_t ds_len(const dstring* s)` | O(1) | Get length |
| `bool ds_is_sso(const dstring* s)` | O(1) | Check if string is inline |
| `bool ds_is_heap(const dstring* s)` | O(1) | Check if string is on heap |
| `bool ds_ok(const dstring* s)` | O(1) | Check if valid (no error) |
| `bool ds_empty(const dstring* s)` | O(1) | Check if empty |

---

### Manipulation

| Function | Time | Description |
|----------|------|-------------|
| `dstring ds_cat(const dstring* a, const dstring* b)` | O(n) | Concatenate two strings |
| `dstring ds_sub(const dstring* s, uint32_t start, uint32_t len)` | O(n) | Extract substring |
| `dstring ds_clone(const dstring* s)` | O(n) | Deep copy |
| `dstring ds_push(const dstring* s, char c)` | O(n) | Append single char (creates new string) |
| `int ds_cmp(const dstring* a, const dstring* b)` | O(n) | Lexicographic comparison |
| `uint32_t ds_hash(const dstring* s)` | O(1) for heap | Get hash (cached for heap, computed for SSO) |

---

### Utility Functions

| Function | Time | Description |
|----------|------|-------------|
| `const char* ds_cstr(const dstring* s)` | O(1) | Convert to C string (no copy) |
| `bool ds_contains(const dstring* s, char c)` | O(n) | Check if contains character |
| `int32_t ds_find(const dstring* s, char c)` | O(n) | Find first occurrence of char |
| `dstring ds_trim(const dstring* s)` | O(n) | Trim whitespace from both ends |

---

## Performance Guarantees

### Time Complexities

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `ds_init_len()` | O(n) | Copy n bytes + compute hash |
| `ds_init()` | O(n) | Calls strlen() first |
| `ds_len()` | O(1) | Direct field access |
| `ds_data()` | O(1) | Pointer dereference |
| `ds_hash()` | O(1) for heap, O(n) for SSO | Hash cached for heap strings |
| `ds_cat()` | O(n) | n = len(a) + len(b) |
| `ds_sub()` | O(n) | n = substring length |
| `ds_clone()` | O(n) | n = string length |
| `ds_push()` | O(n) | n = new length |
| `ds_cmp()` | O(n) | n = min(len(a), len(b)) |
| `ds_free()` | O(1) | Single free, then memset |

### Memory Bounds

| Scenario | Allocation | Bytes |
|----------|------------|-------|
| `len <= 14` | None (SSO) | 16 bytes on stack |
| `len > 14` | 1 malloc | 16 bytes struct + len + 1 heap |
| Error state | None | 16 bytes, all zero |
| Max string length | 2^31 - 1 chars | ~2.1 GB |

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

dstring s = ds_init_len(buffer, pos);
// ... use s ...
ds_free(&s);
```

**Why:** One malloc, one free. O(n) for the whole batch, not O(n²).

### ❌ Bad: Per-Char Push in Loop

```c
dstring s = ds_init("");
for (int i = 0; i < 100000; i++) {
    dstring tmp = ds_push(&s, 'A' + (i % 26));  // 100,000 mallocs
    ds_free(&s);
    s = tmp;
}
ds_free(&s);
```

**Why:** Each push allocates new memory. O(n²) time, huge fragmentation.

**Better alternative:**
```c
// Use a C buffer for building
char buffer[100001];
for (int i = 0; i < 100000; i++) {
    buffer[i] = 'A' + (i % 26);
}
buffer[100000] = '\0';
dstring s = ds_init_len(buffer, 100000);
ds_free(&s);
```

### ✅ Good: Substring Extraction

```c
dstring full = ds_init("Hello, World!");
dstring part = ds_sub(&full, 7, 5);  // "World"
printf("%s\n", ds_data(&part));
ds_free(&part);
ds_free(&full);
```

### ✅ Good: Hash-Based Comparisons

```c
dstring a = ds_init("key");
dstring b = ds_init("key");

// Fast path: hash check first (O(1) for heap strings)
if (ds_hash(&a) == ds_hash(&b) && ds_cmp(&a, &b) == 0) {
    // Strings are equal
}

ds_free(&a);
ds_free(&b);
```

### ✅ Good: Using ds_trim

```c
dstring raw = ds_init("  Hello, World!  ");
dstring trimmed = ds_trim(&raw);
printf("'%s'\n", ds_data(&trimmed));  // "Hello, World!"
ds_free(&trimmed);
ds_free(&raw);
```

---

## Error Handling

All functions are **silent on failure**. Check `ds_ok()` after operations.

| Scenario | Behavior |
|----------|----------|
| `ds_init(NULL)` | Returns empty SSO string, ds_ok = true |
| `malloc()` fails | Returns error string, ds_ok = false |
| `len >= 2^31` | Returns error string, ds_ok = false |
| `ds_data()` on error | Returns `""` (safe, never NULL) |
| `ds_len()` on error | Returns `0` |
| `ds_free()` on error | Safe (no-op) |
| Double `ds_free()` | Safe (idempotent) |

```c
dstring s = ds_init_len(data, len);
if (!ds_ok(&s)) {
    // Handle error: allocation failed or invalid len
    fprintf(stderr, "Failed to create string\n");
    return;
}
```

---

## Design Notes

### Memory Layout (16 bytes)

```
SSO Mode (len <= 14):
┌─────────────────────────────┬──────────┐
│  char small[15]             │  uint8_t meta │
│  14 chars + '\0'           │  0x80|len   │
└─────────────────────────────┴──────────┘

Heap Mode (len > 14):
┌──────────────────┬──────────┬──────────┐
│  char* ptr (8B)  │  len (4B) │  hash (4B) │
│                  │ 31 bits   │ 31 bits    │
└──────────────────┴──────────┴──────────┘
```

### Mode Detection

- **SSO**: The meta byte has bit 7 set (`0x80 | length`)
- **Heap**: Bit 31 of the hash field is always 0 (hash masked to 31 bits)
- **Discrimination**: Check bit 7 of byte 15 (meta) or bit 31 of hash

### Hash Caching Strategy

- **Heap strings**: Hash computed once at creation, cached in `heap.hash` (31-bit)
- **SSO strings**: Hash computed on-the-fly (≤14 chars, very fast)
- **Hash access**: O(1) for heap strings, O(n) for SSO strings
- **Hash collision handling**: `ds_cmp()` performs full comparison after hash match

---

## Benchmark Results

On **Intel Pentium P6200 @ 2.13 GHz** (2010) with GCC 15.2.0 `-O3`:

| Test | dstring ops/sec | std::string ops/sec |
|------|----------------|---------------------|
| SSO creation (5 chars) | 2,127,107,438 | 104,276,091 |
| Heap creation (47 chars) | 35,262,062 | 25,904,065 |
| `ds_len()` / `.size()` (O(1)) | 2,170,000,000 | 2,110,000,000 |
| `strlen()` (O(n), 5 chars) | 210,000,000 | 210,000,000 |
| `strlen()` (O(n), 22 chars) | 140,000,000 | 140,000,000 |
| Hash computation (5 chars) | 107,175,371 | 150,616,988 |
| Hash computation (11 chars) | 56,087,709 | 77,324,882 |
| Concatenation (1 char, 100k iters) | 238,523 | 194,903,279 |
| Mass allocation (33M strings) | 39,236,190 | 49,309,970 |
| Copy / clone | 38,346,096 | 25,793,454 |

---

## Comparison

| Metric | `dstring` | `std::string` (C++) | SDS (Redis) |
|--------|-----------|----------------------|-------------|
| **Size (bytes)** | 16 | 32 (64-bit) | 3–17 (depends on length) |
| **SSO limit (chars)** | 14 | ~15 | None |
| **Hash built-in** | ✅ Yes (cached for heap) | ❌ No | ❌ No |
| **Max string length** | 2^31 - 1 (~2.1 GB) | 2^63 - 1 | 2^32 - 1 |
| **Error handling** | Silent (ds_ok) | Exceptions | Silent (null on fail) |
| **Binary safe** | ✅ Yes | ✅ Yes | ✅ Yes |
| **Heap allocation overhead** | 16-byte struct + len | 32-byte struct | 3–17 byte header |
| **SSO creation** | 2,127M ops/s | 104M ops/s | 28.6M ops/s |
| **Heap creation** | 35.3M ops/s | 25.9M ops/s | 32.7M ops/s |
| **O(1) length access** | 2,170M ops/s | 2,110M ops/s | 2,130M ops/s |
| **Hash access** | O(1) for heap (cached) | N/A | N/A |
| **Concatenation (push char)** | 0.24M ops/s* | 195M ops/s** | 36.7M ops/s |
| **Mass allocation** | 39.2M ops/s | 49.3M ops/s | 15.4M ops/s |
| **Memory fragmentation risk** | Low (exact fit) | Medium (geometric growth) | Medium (geometric growth) |
| **C compatibility** | ✅ C99/C11 | ❌ C++ only | ✅ C99 |
| **Header-only** | ✅ Yes | ✅ Yes (STL) | ❌ No (requires sds.c) |
| **Dependencies** | None (libc only) | STL | sys/types.h |

\* `ds_push` creates a new string each call (immutable API)  
\*\* `std::string::push_back` uses amortized O(1) growth with reserve

*All benchmarks measured on Intel Pentium P6200 @ 2.13 GHz, GCC 15.2.0 `-O3`. Test scripts provided in 'tests/' directory, launched via 'python build.py'.*

**⚠️ Make sure you have at least 2+ GB of free RAM before launching the stress tests!**

---

## Portability

| Platform | Support |
|----------|---------|
| Linux / BSD | ✅ Full |
| Windows (MinGW/MSVC) | ✅ Full |
| macOS | ✅ Full |
| ARM / ARM64 | ✅ Full |
| AVR / ESP32 | ✅ Full |
| RISC-V | ✅ Full |

Compiles with:
- GCC 4.8+
- Clang 3.0+
- MSVC 2015+
- TinyCC
- Any C99/C11 compiler

---

## Limitations

1. **Immutable API**: Operations create new strings rather than modifying in place
2. **No capacity management**: Heap strings allocated with exact fit
3. **Max string length**: 2^31 - 1 chars (~2.1 GB)
4. **No locale support**: Byte-wise comparison only
5. **Single-byte chars only**: No built-in UTF-8 handling (though binary-safe)
6. **Hash collisions**: 31-bit hash may collide; use `ds_cmp()` for verification

---

## License

MIT / Public Domain — use as you wish.