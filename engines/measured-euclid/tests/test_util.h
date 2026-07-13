// test_util.h — minimal assertion harness (in-territory; engines depend only on
// the contract, never on shell/ or a sibling engine).
#pragma once
#include <cmath>
#include <cstdio>
static int g_failures = 0;
#define CHECK(cond) do { if(!(cond)){ std::printf("FAIL %s:%d  %s\n",__FILE__,__LINE__,#cond); ++g_failures; } } while(0)
#define CHECK_EQ(a,b) do { auto _a=(a); auto _b=(b); if(!(_a==_b)){ std::printf("FAIL %s:%d  (%s == %s)\n",__FILE__,__LINE__,#a,#b); ++g_failures; } } while(0)
#define CHECK_NEAR(a,b,tol) do { double _d=std::fabs(double(a)-double(b)); if(_d>(tol)){ std::printf("FAIL %s:%d  |%s-%s|=%g>%g\n",__FILE__,__LINE__,#a,#b,_d,double(tol)); ++g_failures; } } while(0)
#define RUN_MAIN() int main(){ run(); if(g_failures){ std::printf("%d failure(s)\n",g_failures); return 1;} std::printf("ok\n"); return 0; }

// Genuine recursive Bjorklund E(k,n) → out[n]. This is an INDEPENDENT reference
// (NOT the round(i*n/k) construction the engine uses), so the recovery test is
// a real cross-check, not a tautology. Duplicated in-territory on purpose.
#include <vector>
inline void bjork(int k, int n, bool* out) {
    for (int i = 0; i < n; ++i) out[i] = false;
    if (n <= 0 || k <= 0) return;
    if (k >= n) { for (int i = 0; i < n; ++i) out[i] = true; return; }
    std::vector<std::vector<int>> A(k, std::vector<int>{1}), B(n - k, std::vector<int>{0});
    while (B.size() > 1) {
        size_t m = std::min(A.size(), B.size());
        std::vector<std::vector<int>> A2, B2;
        for (size_t i = 0; i < m; ++i) { auto v = A[i]; v.insert(v.end(), B[i].begin(), B[i].end()); A2.push_back(v); }
        if (A.size() > B.size()) for (size_t i = m; i < A.size(); ++i) B2.push_back(A[i]);
        else                     for (size_t i = m; i < B.size(); ++i) B2.push_back(B[i]);
        A = A2; B = B2;
    }
    int idx = 0;
    for (auto& seq : {A, B}) for (auto& g : seq) for (int bit : g) if (idx < n) out[idx++] = bit != 0;
}
