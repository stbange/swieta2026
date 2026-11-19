#ifndef SWIETA_TEST_HARNESS_H
#define SWIETA_TEST_HARNESS_H

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define ASSERT_TRUE(cond) do {                                                     \
    if (!(cond)) {                                                                 \
        fprintf(stderr, "  FAIL %s:%d: expected true: %s\n", __FILE__, __LINE__, #cond); \
        g_tests_failed++;                                                          \
        return;                                                                    \
    }                                                                              \
} while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ_INT(a, b) do {                                                   \
    long long _va = (long long)(a), _vb = (long long)(b);                          \
    if (_va != _vb) {                                                              \
        fprintf(stderr, "  FAIL %s:%d: %lld != %lld (%s vs %s)\n",                 \
                __FILE__, __LINE__, _va, _vb, #a, #b);                             \
        g_tests_failed++;                                                          \
        return;                                                                    \
    }                                                                              \
} while (0)

#define ASSERT_NEAR(a, b, eps) do {                                                \
    float _va = (float)(a), _vb = (float)(b);                                      \
    if (fabsf(_va - _vb) > (eps)) {                                                \
        fprintf(stderr, "  FAIL %s:%d: %.5f != %.5f (eps %.5f)\n",                 \
                __FILE__, __LINE__, _va, _vb, (float)(eps));                       \
        g_tests_failed++;                                                          \
        return;                                                                    \
    }                                                                              \
} while (0)

#define RUN_TEST(fn) do {                                                          \
    int _before = g_tests_failed;                                                  \
    g_tests_run++;                                                                 \
    fn();                                                                          \
    if (g_tests_failed == _before) {                                               \
        printf("[PASS] %s\n", #fn);                                                \
    } else {                                                                       \
        printf("[FAIL] %s\n", #fn);                                                \
    }                                                                              \
} while (0)

#endif /* SWIETA_TEST_HARNESS_H */
