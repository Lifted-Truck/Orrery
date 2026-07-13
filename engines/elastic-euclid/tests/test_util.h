// test_util.h — minimal assertion harness for the Elastic Euclid ctests.
// Kept in-territory (engines depend only on the contract, never on shell/).
#pragma once

#include <cmath>
#include <cstdio>

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

#define CHECK_EQ(a, b)                                                         \
    do { auto _a=(a); auto _b=(b); if(!(_a==_b)){ std::printf("FAIL %s:%d  (%s == %s)\n", __FILE__,__LINE__,#a,#b); ++g_failures; } } while (0)

#define CHECK_NEAR(a, b, tol)                                                  \
    do { double _d=std::fabs(double(a)-double(b)); if(_d>(tol)){ std::printf("FAIL %s:%d  |%s-%s|=%g>%g\n",__FILE__,__LINE__,#a,#b,_d,double(tol)); ++g_failures; } } while (0)

#define RUN_MAIN()                                                            \
    int main() { run(); if (g_failures) { std::printf("%d failure(s)\n", g_failures); return 1; } std::printf("ok\n"); return 0; }
