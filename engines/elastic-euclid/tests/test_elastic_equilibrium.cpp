// test_elastic_equilibrium.cpp — spec §8.1: E(k,n) is the equilibrium of the
// physics. Two robustly-true, non-trivial properties (do not weaken):
//
//   (A) STABILITY — from an exact-onset start, the system holds the pattern
//       over 200 generations (a wrong-signed force would drift off) and is a
//       settled fixed point.
//   (B) BASIN RECOVERY — from Bjorklund-ADJACENT perturbed starts (positional,
//       no kick; §8.1 says "Bjorklund-adjacent random starts"), the dynamics
//       relax to a rotation of E(k,n) within 200 ticks. Non-triviality: many
//       DISTINCT perturbed starts all converge to the same Euclidean attractor.
//
// CHARACTERIZED LIMITATION (measured, documented in this dir's CLAUDE.md, not
// weakened): the basin is finite. Displacements beyond ~0.5 lattice cell, and
// velocity kicks, can land the system in a metastable non-Euclid minimum;
// convergence completes by ~200 ticks or not at all. §8.1's "adjacent" is load-
// bearing — global convergence from arbitrary starts is NOT claimed.
#include "orrery/engines/ElasticEuclid.h"
#include "test_util.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

using namespace orrery;

static double wrapd(double x) { return std::fmod(std::fmod(x, 1.0) + 1.0, 1.0); }
static double sdistd(double a, double b) { return wrapd(b - a + 0.5) - 0.5; }

// RMS distance from the particle set to the nearest rotation/alignment of
// E(k,n) — the prototype's ghost metric. 0 ⇒ exactly on some rotation.
static double rmsToEuclid(const ElasticEuclid& e, int k, int n) {
    bool pat[64]; ElasticEuclid::bjorklund(k, n, pat);
    std::vector<double> on; for (int i = 0; i < n; ++i) if (pat[i]) on.push_back((double)i / n);
    std::vector<double> th; for (int i = 0; i < k; ++i) th.push_back(wrapd(e.theta(i)));
    std::sort(th.begin(), th.end());
    double best = 1e18;
    for (int r = 0; r < n; ++r) {
        std::vector<double> g; for (double o : on) g.push_back(wrapd(o + (double)r / n));
        std::sort(g.begin(), g.end());
        const int m = (int)std::min(g.size(), th.size());
        for (size_t off = 0; off < g.size(); ++off) {
            double e2 = 0; for (int i = 0; i < m; ++i) { double d = sdistd(th[i], g[(i + off) % g.size()]); e2 += d * d; }
            best = std::min(best, e2);
        }
    }
    return std::sqrt(best / std::max(1, (int)std::min(on.size(), th.size())));
}

// Quantized particle cells == E(k,n) up to some rotation r∈[0,n)?
static bool isRotationOfEuclid(const ElasticEuclid& e, int k, int n) {
    bool pat[64]; ElasticEuclid::bjorklund(k, n, pat);
    std::vector<int> target; for (int i = 0; i < n; ++i) if (pat[i]) target.push_back(i);
    std::set<int> occ; for (int i = 0; i < k; ++i) occ.insert(ElasticEuclid::quantizeCell(e.theta(i), n));
    if ((int)occ.size() != k) return false;
    for (int r = 0; r < n; ++r) {
        std::set<int> shifted; for (int c : target) shifted.insert((c + r) % n);
        if (shifted == occ) return true;
    }
    return false;
}

static void setup(ElasticEuclid& e, int k, int n) {
    e.setN(n);
    while (e.sourceCount() < k) e.handleGesture({GestureEvent::Type::Add, 0, 0.0f});
    while (e.sourceCount() > k) e.handleGesture({GestureEvent::Type::Remove, 0, 0.0f});
}

static void tickN(ElasticEuclid& e, int n) {
    for (int t = 0; t < n; ++t) { TickContext c; c.generation = t; e.tick(c); }
}

// Max per-particle phase drift over `extra` more ticks — a settled fixed point
// barely moves; a still-migrating system moves measurably.
static double driftOver(ElasticEuclid& e, int k, int extra) {
    double before[kMaxSources]; for (int i = 0; i < k; ++i) before[i] = e.theta(i);
    tickN(e, extra);
    double mx = 0; for (int i = 0; i < k; ++i) mx = std::max(mx, std::abs(sdistd(before[i], e.theta(i))));
    return mx;
}

static const int kPairs[][2] = {
    {2,5},{3,8},{4,8},{5,8},{3,7},{5,16},{7,16},{9,16},{4,16},
    {11,16},{5,13},{2,3},{7,12},{3,4},{6,16},{5,12},{8,16},{13,16}};
static constexpr int kNumPairs = (int)(sizeof(kPairs) / sizeof(kPairs[0]));

static void run() {
    // (A) STABILITY: exact-onset start holds E(k,n) and is a settled fixed point.
    {
        int pass = 0;
        for (const auto& p : kPairs) {
            const int k = p[0], n = p[1];
            ElasticEuclid e; e.seed(1, 1); setup(e, k, n);
            bool pat[64]; ElasticEuclid::bjorklund(k, n, pat);
            std::vector<int> on; for (int i = 0; i < n; ++i) if (pat[i]) on.push_back(i);
            for (int i = 0; i < k; ++i) e.handleGesture({GestureEvent::Type::Drag, i, (float)((double)on[i] / n)});
            tickN(e, 200);
            const bool onPattern = isRotationOfEuclid(e, k, n);
            const double drift = driftOver(e, k, 50);      // settled?
            if (onPattern && drift < 5e-3) ++pass;
            else std::printf("  STABILITY E(%d,%d): onPattern=%d drift=%.2e\n", k, n, onPattern, drift);
            CHECK(onPattern);
            CHECK(drift < 5e-3);
        }
        std::printf("(A) stability: %d/%d pairs hold E(k,n) as a settled fixed point\n", pass, kNumPairs);
    }

    // (B) BASIN RECOVERY: distinct Bjorklund-adjacent perturbed starts each relax
    //     to a rotation of E(k,n). Displacement ±0.4 lattice cell (within basin),
    //     no kick. Non-trivial: every start is measurably off-equilibrium
    //     (startRms>0.003) yet all converge to the SAME Euclidean attractor.
    {
        int pass = 0, total = 0;
        double worstStartRms = 0.0;
        for (const auto& p : kPairs) {
            const int k = p[0], n = p[1];
            bool pat[64]; ElasticEuclid::bjorklund(k, n, pat);
            std::vector<int> on; for (int i = 0; i < n; ++i) if (pat[i]) on.push_back(i);
            for (uint32_t s = 1; s <= 4; ++s) {
                ++total;
                ElasticEuclid e; e.seed(0x5000 + s, 1); setup(e, k, n);
                Pcg32 jr; jr.seed(s, 7);
                for (int i = 0; i < k; ++i) {
                    const double j = (jr.nextFloat() - 0.5f) * 2.0 * 0.4 / n;  // ±0.4 cell
                    e.handleGesture({GestureEvent::Type::Drag, i, (float)((double)on[i] / n + j)});
                }
                const double startRms = rmsToEuclid(e, k, n);   // input off-equilibrium
                double th0[kMaxSources]; for (int i = 0; i < k; ++i) th0[i] = e.theta(i);
                tickN(e, 200);
                const bool onPattern = isRotationOfEuclid(e, k, n);
                const double drift = driftOver(e, k, 50);
                // Physics actually migrated the particles (kills the no-op
                // hypothesis): for clearly-displaced starts the net move is real.
                double netMove = 0; for (int i = 0; i < k; ++i) netMove = std::max(netMove, std::abs(sdistd(th0[i], e.theta(i))));
                worstStartRms = std::max(worstStartRms, startRms);
                const bool migrated = startRms < 0.02 || netMove > 0.01;
                const bool ok = onPattern && drift < 5e-3 && startRms > 0.003 && migrated;
                if (ok) ++pass;
                else std::printf("  RECOVERY E(%d,%d) seed %u: onPattern=%d drift=%.2e startRms=%.4f netMove=%.4f\n",
                                 k, n, s, onPattern, drift, startRms, netMove);
                CHECK(ok);
            }
        }
        std::printf("(B) basin recovery: %d/%d perturbed starts relaxed to E(k,n) "
                    "(all startRms>0.003, max=%.4f)\n", pass, total, worstStartRms);
    }
}

RUN_MAIN()
