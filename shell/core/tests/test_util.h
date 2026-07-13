// test_util.h — tiny framework-free assertion harness for the core ctests.
// Each test file defines `static void run();` then RUN_MAIN(); a nonzero exit
// means failure (the ctest contract).
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        auto _a = (a);                                                         \
        auto _b = (b);                                                         \
        if (!(_a == _b)) {                                                     \
            std::printf("FAIL %s:%d  (%s == %s)\n", __FILE__, __LINE__, #a,    \
                        #b);                                                   \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                  \
    do {                                                                       \
        double _d = std::fabs(double(a) - double(b));                         \
        if (_d > (tol)) {                                                      \
            std::printf("FAIL %s:%d  |%s - %s| = %g > %g\n", __FILE__,         \
                        __LINE__, #a, #b, _d, double(tol));                    \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// FNV-1a over a byte range — for bit-identity hashing of event streams.
static inline uint64_t fnv1a(const void* data, size_t n, uint64_t h = 1469598103934665603ULL) {
    const auto* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}

#define RUN_MAIN()                                                            \
    int main() {                                                              \
        run();                                                                \
        if (g_failures) {                                                     \
            std::printf("%d failure(s)\n", g_failures);                       \
            return 1;                                                         \
        }                                                                     \
        std::printf("ok\n");                                                  \
        return 0;                                                             \
    }
