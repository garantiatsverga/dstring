#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../dstring.h"

#define CONCAT_ITERS 100000
#define STRINGS_INIT 1000000

// Timer
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    double time_sec(void) {
        LARGE_INTEGER freq, counter;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&counter);
        return (double)counter.QuadPart / freq.QuadPart;
    }
#elif defined(__APPLE__) || defined(__MACH__)
    #include <mach/mach_time.h>
    double time_sec(void) {
        static mach_timebase_info_data_t info = {0};
        if (info.denom == 0) mach_timebase_info(&info);
        uint64_t time = mach_absolute_time();
        return (double)(time * info.numer) / (info.denom * 1e9);
    }
#else
    // Linux, BSD - everything with POSIX
    #define _POSIX_C_SOURCE 199309L
    #include <time.h>
    double time_sec(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec + ts.tv_nsec / 1e9;
    }
#endif

// Defining the arena for all devices
#if defined(__arm__) || defined(__aarch64__)
    // ARM-устройства (Raspberry Pi, Android, iOS)
    #define ARENA_SIZE (512UL * 1024 * 1024) // 512 MB
#else
    // x86_64 / x86 - going all out
    #define ARENA_SIZE (2UL * 1024 * 1024 * 1024) // 2 GB
#endif

// Main tests
void bench_stress_alloc(void) {
    printf("\nConsequent string allocation untill the arena is full\n");
    printf("   String size: %zu bites\n", sizeof(string));
    printf("   Arena size: %zu MB\n", ARENA_SIZE / (1024 * 1024));
    printf("   Strings max: ~%zu\n", ARENA_SIZE / sizeof(string));

    void* arena = malloc(ARENA_SIZE);
    if (arena == NULL) {
        printf("   Failed to allocate the arena\n");
        return;
    }
    printf("   Arena has been allocated\n");

    string* strings = (string*)arena;
    uint64_t count = 0;
    double start = time_sec();

    const char* samples[] = {"Hello", "World!!!", "Pikachu, I choose you!"};
    int sample_count = sizeof(samples) / sizeof(samples[0]);

    while (count < ARENA_SIZE / sizeof(string) - 1) {
        int idx = count % sample_count;
        strings[count] = initstr(samples[idx]);
        if (!strok(&strings[count])) break;
        count++;
    }

    double elapsed = time_sec() - start;
    printf("   Strings created: %llu\n", (unsigned long long)count);
    printf("   Time: %.3f sec\n", elapsed);
    printf("   Speed: %.0f string per sec\n", count / elapsed);

    // Free
    for (uint64_t i = 0; i < count; i++) {
        freestr(&strings[i]);
    }
    free(arena);
    printf("   Arena has been freed\n");
}

// 1 milloin (by default) strings (for all systems)
void bench_1m_strings(void) {
    printf("\nTest: %d strings (short & long)\n", STRINGS_INIT);

    const char* short_str = "Hello";
    const char* long_str = "This is a long string for testing heap allocation.";

    string* strings = (string*)malloc(STRINGS_INIT * sizeof(string));
    if (strings == NULL) {
        printf("   Failed to allocate memory\n");
        return;
    }

    double start = time_sec();
    for (int i = 0; i < STRINGS_INIT; i++) {
        strings[i] = (i % 2 == 0) ? initstr(short_str) : initstr(long_str);
        if (!strok(&strings[i])) {
            printf("   Error at the string №%d\n", i);
            break;
        }
    }
    double elapsed = time_sec() - start;
    printf("   %d strings created\n", STRINGS_INIT);
    printf("   Time: %.3f sec\n", elapsed);
    printf("   Speed: %.0f strings per sec\n", 1000000.0 / elapsed);

    // Free
    start = time_sec();
    for (int i = 0; i < 1000000; i++) {
        freestr(&strings[i]);
    }
    elapsed = time_sec() - start;
    printf("   Freed: %.3f sec\n", elapsed);

    free(strings);
}

// Concat in a cycle
void bench_concat_stress(void) {
    printf("\nConcatenation in a cycle of %d iterations\n", CONCAT_ITERS);

    string s = initstr("");
    double start = time_sec();

    for (int i = 0; i < CONCAT_ITERS; i++) {
        string tmp = strpush(&s, 'A' + (i % 26));
        freestr(&s);
        s = tmp;
        if (!strok(&s)) {
            printf("   Error on the iteration №%d\n", i);
            break;
        }
    }

    double elapsed = time_sec() - start;
    printf("   String's length: %u\n", strlen_s(&s));
    printf("   Time: %.3f sec\n", elapsed);
    printf("   Speed: %.0f opers per sec\n", 100000.0 / elapsed);

    freestr(&s);
}

int main(void) {
    printf("Stress-test of the module dstring.h\n");
    printf("   Platform: ");

    #if defined(_WIN64)
        printf("Windows x64\n");
    #elif defined(_WIN32)
        printf("Windows x86\n");
    #elif defined(__APPLE__) && defined(__MACH__)
        printf("macOS\n");
    #elif defined(__linux__)
        printf("Linux\n");
    #elif defined(__FreeBSD__)
        printf("FreeBSD\n");
    #elif defined(__arm__)
        printf("ARM\n");
    #elif defined(__aarch64__)
        printf("ARM64\n");
    #else
        printf("Is unknown, but we're working anyway!\n");
    #endif

    bench_stress_alloc();
    bench_1m_strings();
    bench_concat_stress();

    return 0;
}