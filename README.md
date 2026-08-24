# dstring.h — Lightweight C String Library

A minimal, high-performance string library for C with **Small String Optimization (SSO)**, **non-owning views**, **arena builder**, and a built-in cached hash. Designed for systems where every byte counts.

---

## Overview

| Feature | Description |
|---------|-------------|
| **Size** | 16 bytes for all types (fits in cache line) |
| **SSO** | Up to 14 chars stored inline (no malloc) |
| **Hash** | Built-in FNV-1a hash with lazy caching (O(1) access) |
| **Views** | Zero-copy, non-owning string slices |
| **Arena** | O(1) amortized string builder with manual growth control |
| **Binary safe** | Supports embedded nulls |
| **Error handling** | Silent failure with error flag |
| **Max length** | 2^31 - 1 chars (~2.1 GB) |
| **Portability** | C99/C11, no external dependencies |

---

## Types

### `dstring` (16 bytes)
```c
typedef struct {
    union {
        struct { char* ptr; uint32_t len; uint32_t hash; } heap;
        struct { char small[15]; uint8_t meta; } sso;
        uint8_t raw[16];
    };
} dstring;
```
**Purpose:** String storage with SSO and cached hash.

### `dstring_view` (16 bytes)
```c
typedef struct {
    const char* data;
    uint32_t len;
    uint32_t hash;  // Lazy cached
} dstring_view;
```
**Purpose:** Non-owning, zero-copy string slice with lazy hash caching.

### `dstring_arena` (16 bytes)
```c
typedef struct {
    char* data;
    uint32_t len;
    uint32_t capacity;
} dstring_arena;
```
**Purpose:** Reusable string builder with O(1) amortized appends.

---

## Constants

```c
#define STR_SSO_MAX 14          // Max inline characters
#define STR_MODE_SSO 0x80       // SSO marker bit in meta byte
#define STR_LEN_MASK 0x3F       // SSO length mask (lower 6 bits)
#define DS_INIT { .raw = {0} }  // dstring initializer
#define DSV_INIT { .data = NULL, .len = 0, .hash = 0 }  // view initializer
#define DSA_INIT { .data = NULL, .len = 0, .capacity = 0 }  // arena initializer
```

---

## Core Functions

### dstring Creation & Destruction

| Function | Time | Description |
|----------|------|-------------|
| `dstring ds_init(const char* s)` | O(n) | Create from C string (calls strlen) |
| `dstring ds_init_len(const char* s, uint32_t len)` | O(n) | Create from buffer with known length |
| `void ds_free(dstring* s)` | O(1) | Free heap memory, zero struct |

### dstring Access

| Function | Time | Description |
|----------|------|-------------|
| `const char* ds_data(const dstring* s)` | O(1) | Get raw data pointer (null-safe) |
| `uint32_t ds_len(const dstring* s)` | O(1) | Get length |
| `bool ds_is_sso(const dstring* s)` | O(1) | Check if string is inline |
| `bool ds_is_heap(const dstring* s)` | O(1) | Check if string is on heap |
| `bool ds_ok(const dstring* s)` | O(1) | Check if valid (no error) |
| `bool ds_empty(const dstring* s)` | O(1) | Check if empty |

### dstring Manipulation

| Function | Time | Description |
|----------|------|-------------|
| `dstring ds_cat(const dstring* a, const dstring* b)` | O(n) | Concatenate two strings |
| `dstring ds_sub(const dstring* s, uint32_t start, uint32_t len)` | O(n) | Extract substring |
| `dstring ds_clone(const dstring* s)` | O(n) | Deep copy |
| `dstring ds_push(const dstring* s, char c)` | O(n) | Append single char (creates new string) |
| `int ds_cmp(const dstring* a, const dstring* b)` | O(n) | Lexicographic comparison |
| `uint32_t ds_hash(dstring* s)` | O(1)* | Get hash (cached for heap, computed for SSO) |

*O(1) for heap strings after first access (lazy caching). SSO always computes (≤14 chars).

### dstring Utility

| Function | Time | Description |
|----------|------|-------------|
| `const char* ds_cstr(const dstring* s)` | O(1) | Convert to C string (no copy) |
| `bool ds_contains(const dstring* s, char c)` | O(n) | Check if contains character |
| `int32_t ds_find(const dstring* s, char c)` | O(n) | Find first occurrence of char |
| `dstring ds_trim(const dstring* s)` | O(n) | Trim whitespace from both ends |

---

## dstring_view Functions

### Creation

| Function | Time | Description |
|----------|------|-------------|
| `dstring_view dsv_from_dstring(const dstring* s)` | O(1) | View of dstring |
| `dstring_view dsv_from_cstr(const char* str)` | O(n) | View of C string (strlen) |
| `dstring_view dsv_from_buffer(const char* data, uint32_t len)` | O(1) | View of buffer |

### Operations

| Function | Time | Description |
|----------|------|-------------|
| `dstring_view dsv_sub(const dstring_view* v, uint32_t start, uint32_t len)` | O(1) | Substring view (no copy!) |
| `void dsv_remove_prefix(dstring_view* v, uint32_t n)` | O(1) | Remove from front |
| `void dsv_remove_suffix(dstring_view* v, uint32_t n)` | O(1) | Remove from back |
| `uint32_t dsv_hash(dstring_view* v)` | O(1)* | Lazy hash with caching |
| `int32_t dsv_find(const dstring_view* v, char c)` | O(n) | Find character |
| `int32_t dsv_find_sub(dstring_view* haystack, dstring_view* needle)` | O(n*m) | Find substring |
| `dstring_view dsv_split_at(dstring_view* v, char delim)` | O(n) | Split at delimiter |
| `dstring_view dsv_trim(const dstring_view* v)` | O(1) | Trim whitespace (returns view) |
| `bool dsv_starts_with(dstring_view* v, dstring_view* prefix)` | O(n) | Check prefix |
| `bool dsv_ends_with(dstring_view* v, dstring_view* suffix)` | O(n) | Check suffix |
| `dstring dsv_to_dstring(const dstring_view* v)` | O(n) | Convert to dstring (copies) |

*O(1) after first access (cached). First access is O(n).

---

## dstring_arena Functions

### Creation & Extension

| Function | Time | Description |
|----------|------|-------------|
| `dstring_arena dsa_create(uint32_t initial_capacity)` | O(1) | Create arena |
| `void dsa_free(dstring_arena* arena)` | O(1) | Free arena |
| `bool dsa_extend_by_multiplier(dstring_arena* a, float mult)` | O(n) | Extend by multiplier (e.g., 2.0f) |
| `bool dsa_extend_to(dstring_arena* a, uint32_t required)` | O(n) | Extend to exact capacity |
| `bool dsa_reserve(dstring_arena* a, uint32_t additional)` | O(n) | Reserve additional bytes |

### Appending

| Function | Time | Description |
|----------|------|-------------|
| `bool dsa_append(dstring_arena* a, const char* str)` | O(n)* | Append C string |
| `bool dsa_append_len(dstring_arena* a, const char* data, uint32_t len)` | O(n)* | Append with known length |
| `bool dsa_append_dstring(dstring_arena* a, const dstring* s)` | O(n)* | Append dstring |
| `bool dsa_append_view(dstring_arena* a, const dstring_view* v)` | O(n)* | Append view |
| `bool dsa_push(dstring_arena* a, char c)` | O(1)* | Append single char |

*Amortized O(1) if capacity is sufficient. Reallocation is O(n).

### Conversion & Operations

| Function | Time | Description |
|----------|------|-------------|
| `dstring dsa_to_dstring(const dstring_arena* a)` | O(n) | Convert to dstring (copies) |
| `dstring_view dsa_to_view(const dstring_arena* a)` | O(1) | Create view (no copy) |
| `void dsa_clear(dstring_arena* a)` | O(1) | Clear content (keeps capacity) |
| `void dsa_shrink_to_fit(dstring_arena* a)` | O(n) | Shrink capacity to fit content |
| `uint32_t dsa_capacity(const dstring_arena* a)` | O(1) | Get current capacity |

---

## Usage Examples

### ✅ GOOD: Building Strings with Arena

```c
// Build a CSV line efficiently
dstring_arena arena = dsa_create(1024);  // Pre-allocate 1KB

for (int i = 0; i < 1000; i++) {
    dsa_append(&arena, "value");
    dsa_push(&arena, ',');
}

// Convert to dstring when done
dstring result = dsa_to_dstring(&arena);
dsa_free(&arena);

// Use result...
printf("%.*s\n", (int)ds_len(&result), ds_data(&result));
ds_free(&result);
```

**Why:** O(1) amortized appends. One allocation for entire build. No fragmentation.

### ❌ BAD: Per-Char Push in Loop

```c
// DON'T DO THIS - O(n²) time!
dstring s = ds_init("");
for (int i = 0; i < 100000; i++) {
    dstring tmp = ds_push(&s, 'A' + (i % 26));  // 100,000 mallocs!
    ds_free(&s);
    s = tmp;
}
ds_free(&s);
```

**Why:** Each push allocates new memory and copies all previous data. 100,000 mallocs, 5GB of copying.

**Better alternative:**
```c
// Use arena for building
dstring_arena arena = dsa_create(100001);
for (int i = 0; i < 100000; i++) {
    dsa_push(&arena, 'A' + (i % 26));  // O(1)!
}
dstring s = dsa_to_dstring(&arena);
dsa_free(&arena);
ds_free(&s);
```

---

### ✅ GOOD: Using Views for Zero-Copy Parsing

```c
dstring data = ds_init("key=value;name=test;count=42");
dstring_view view = dsv_from_dstring(&data);

// Parse without any allocation!
while (!dsv_empty(&view)) {
    dstring_view token = dsv_split_at(&view, ';');
    
    // Split key=value
    dstring_view key = dsv_split_at(&token, '=');
    dstring_view value = token;  // Remaining after '='
    
    printf("Key: %.*s, Value: %.*s\n",
           (int)key.len, key.data,
           (int)value.len, value.data);
}

ds_free(&data);
```

**Why:** Zero allocations during parsing. Views are just pointer + length.

### ❌ BAD: Unnecessary Copies

```c
// DON'T DO THIS - creates unnecessary copies
dstring data = ds_init("Hello, World!");
dstring copy1 = ds_clone(&data);  // Unnecessary copy
dstring sub = ds_sub(&copy1, 0, 5);  // Another copy
dstring copy2 = ds_clone(&sub);  // Yet another copy

// Use all copies...
ds_free(&copy2);
ds_free(&sub);
ds_free(&copy1);
ds_free(&data);
```

**Better alternative:**
```c
// Use views for zero-copy access
dstring data = ds_init("Hello, World!");
dstring_view view = dsv_from_dstring(&data);
dstring_view sub = dsv_sub(&view, 0, 5);  // No copy!

// Use sub directly...
printf("%.*s\n", (int)sub.len, sub.data);

ds_free(&data);
```

---

### ✅ GOOD: Hash-Based Comparisons

```c
dstring a = ds_init("this_is_a_very_long_key_that_goes_to_heap");
dstring b = ds_init("this_is_a_very_long_key_that_goes_to_heap");

// Fast path: hash check first (O(1) for heap strings)
if (ds_hash(&a) == ds_hash(&b) && ds_cmp(&a, &b) == 0) {
    // Strings are equal
    printf("Equal!\n");
}

ds_free(&a);
ds_free(&b);
```

**Why:** Hash comparison is O(1) for cached heap strings. Only if hashes match do we compare bytes.

---

### ✅ GOOD: Efficient String Reuse

```c
// Reuse arena for multiple operations
dstring_arena arena = dsa_create(65536);  // Allocate once

for (int batch = 0; batch < 100; batch++) {
    dsa_clear(&arena);  // Reset length, keep capacity
    
    // Build batch
    for (int i = 0; i < 1000; i++) {
        dsa_append(&arena, "data");
        dsa_push(&arena, '\n');
    }
    
    // Process batch
    process(dsa_data(&arena), dsa_len(&arena));
}

dsa_free(&arena);  // Free once
```

**Why:** Zero reallocations after initial allocation. 100 batches, 1 malloc, 1 free.

---

### ✅ GOOD: Error Handling

```c
dstring s = ds_init_len(data, len);
if (!ds_ok(&s)) {
    // Handle error: allocation failed or invalid len
    fprintf(stderr, "Failed to create string\n");
    return;
}

// Safe to use - ds_data() never returns NULL
printf("%s\n", ds_data(&s));
ds_free(&s);
```

---

### ✅ GOOD: Binary Data

```c
// Binary data with embedded nulls
const char binary[] = {'A', '\0', 'B', '\0', 'C'};
dstring s = ds_init_len(binary, 5);

// Works correctly - length is stored explicitly
printf("Length: %u\n", ds_len(&s));  // 5, not 1!
ds_free(&s);
```

---

## Performance Guarantees

### Time Complexities

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `ds_init_len()` | O(n) | Copy n bytes (hash lazy for heap) |
| `ds_init()` | O(n) | Calls strlen() first |
| `ds_len()` | O(1) | Direct field access |
| `ds_data()` | O(1) | Pointer dereference |
| `ds_hash()` | O(1)* | Cached after first access |
| `ds_cat()` | O(n) | n = len(a) + len(b) |
| `ds_sub()` | O(n) | n = substring length |
| `ds_clone()` | O(n) | n = string length |
| `ds_push()` | O(n) | **Avoid - use arena!** |
| `ds_cmp()` | O(n) | n = min(len(a), len(b)) |
| `ds_free()` | O(1) | Single free, then memset |
| `dsv_sub()` | O(1) | **Zero copy!** |
| `dsv_hash()` | O(1)* | Lazy caching |
| `dsa_push()` | O(1)* | Amortized |
| `dsa_append()` | O(n)* | Amortized O(1) |
| `dsa_clear()` | O(1) | Just reset length |

*Amortized or cached after first access.

### Memory Bounds

| Scenario | Allocation | Bytes |
|----------|------------|-------|
| `len <= 14` | None (SSO) | 16 bytes on stack |
| `len > 14` | 1 malloc | 16 bytes struct + len + 1 heap |
| Error state | None | 16 bytes, all zero |
| Max string length | 2^31 - 1 chars | ~2.1 GB |
| View | None | 16 bytes (points to existing data) |
| Arena | 1 malloc | 16 bytes + capacity |

---

## Benchmark Results

On **Intel Pentium P6200 @ 2.13 GHz** (2010) with GCC 14.3.0 `-O3`:

### Speed Comparison (dstring vs std::string)

| Test | dstring | std::string | Winner |
|------|---------|-------------|--------|
| SSO creation (5 chars) | 2,127M/s | 104M/s | **dstring (20x)** |
| Heap creation (47 chars) | 35.3M/s | 25.9M/s | **dstring (1.4x)** |
| Length access O(1) | 2,170M/s | 2,110M/s | Tie |
| Hash (cached) | 2,120M/s | 76-101M/s | **dstring (21-28x)** |
| View substring | 1,060M/s | 1,060M/s | Tie |
| String substring | 88M/s | 119M/s | std::string (1.4x) |
| Arena/reuse push | 529M/s | 361M/s | **dstring (1.5x)** |
| 1M string creation | 24.9M/s | 14.6M/s | **dstring (1.7x)** |
| Mass allocation | 18.8M/s | 17.8M/s | **dstring (1.06x)** |

### Memory Efficiency

| Metric | dstring | std::string |
|--------|---------|-------------|
| Struct size | 16 bytes | 32 bytes |
| Strings in 2GB | 134M | 67M |
| Per string (with data) | 25.4 bytes | 40.7 bytes |
| Memory efficiency | **2x better** | 1x |

### Key Performance Findings

1. **Arena is 1,400x faster than ds_push** for building (529M vs 0.24M ops/sec)
2. **Hash caching is 21-28x faster** than recomputing (2,120M vs 76-101M ops/sec)
3. **Views are zero-allocation** for substrings (1,060M ops/sec)
4. **SSO creation is 20x faster** than C++ (2,127M vs 104M ops/sec)

---

## Comparison

| Metric | `dstring` | `std::string` (C++) | SDS (Redis) |
|--------|-----------|----------------------|-------------|
| **Size (bytes)** | 16 | 32 (64-bit) | 3–17 (depends) |
| **SSO limit (chars)** | 14 | ~15 | None |
| **View type** | ✅ dstring_view (16B) | ✅ string_view (16B) | ❌ None |
| **Arena builder** | ✅ dstring_arena (16B) | ❌ Manual reserve | ❌ None |
| **Hash built-in** | ✅ Yes (lazy cached) | ❌ No | ❌ No |
| **Hash access** | O(1) cached | O(n) every time | N/A |
| **Max string length** | 2^31 - 1 (~2.1 GB) | 2^63 - 1 | 2^32 - 1 |
| **Error handling** | Silent (ds_ok) | Exceptions | Silent |
| **Binary safe** | ✅ Yes | ✅ Yes | ✅ Yes |
| **SSO creation** | 2,127M ops/s | 104M ops/s | 28.6M ops/s |
| **Heap creation** | 35.3M ops/s | 25.9M ops/s | 32.7M ops/s |
| **Arena push** | 529M ops/s | 361M ops/s (reserve) | N/A |
| **Hash cached** | 2,120M ops/s | N/A | N/A |
| **C compatibility** | ✅ C99/C11 | ❌ C++ only | ✅ C99 |
| **Header-only** | ✅ Yes | ✅ Yes (STL) | ❌ No |
| **Dependencies** | None (libc only) | STL | sys/types.h |

*All benchmarks measured on Intel Pentium P6200 @ 2.13 GHz, GCC 14.3.0 `-O3`. Test scripts provided in 'tests/' directory, launched via 'python build.py'.*

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

1. **Immutable API**: `dstring` operations create new strings (use arena for building)
2. **No capacity management**: Heap strings allocated with exact fit
3. **Max string length**: 2^31 - 1 chars (~2.1 GB)
4. **No locale support**: Byte-wise comparison only
5. **Single-byte chars only**: No built-in UTF-8 handling (though binary-safe)
6. **Hash collisions**: 31-bit hash may collide; use `ds_cmp()` for verification
7. **Arena requires manual management**: No automatic growth (user controls via `dsa_extend_by_multiplier`)

---

## License

MIT / Public Domain — use as you wish.