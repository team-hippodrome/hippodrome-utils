/* Minimal test helpers: CHECK keeps going and counts failures, main returns the count. */
#ifndef HU_TEST_H
#define HU_TEST_H
#include <stdio.h>

static int hu_test_failures;
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);   \
            hu_test_failures++;                                                        \
        }                                                                              \
    } while (0)
#define CHECK_EQ(a, b)                                                                              \
    do {                                                                                            \
        long long _a = (long long)(a), _b = (long long)(b);                                         \
        if (_a != _b) {                                                                             \
            fprintf(stderr, "%s:%d: %s == %s failed: %lld != %lld\n", __FILE__, __LINE__, #a, #b, _a, _b); \
            hu_test_failures++;                                                                     \
        }                                                                                           \
    } while (0)
#define TEST_DONE()                                                        \
    do {                                                                   \
        if (hu_test_failures == 0) printf("ok\n");                         \
        return hu_test_failures != 0;                                      \
    } while (0)
#endif
