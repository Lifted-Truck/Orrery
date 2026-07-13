// test_measured_euclid.cpp — spec §6.1 (Euclid recovery) + §6.4 (monotone
// deformation). Recovery cross-checks the engine's round-based output against a
// genuine recursive Bjorklund (independent reference — not a tautology).
#include "orrery/engines/MeasuredEuclid.h"
#include "test_util.h"

#include <set>
#include <vector>

using namespace orrery;

static double wrapd(double x) { return std::fmod(std::fmod(x, 1.0) + 1.0, 1.0); }

static bool isRotationOfEuclid(const std::set<int>& occ, int k, int n) {
    bool pat[64]; bjork(k, n, pat);
    std::vector<int> t; for (int i = 0; i < n; ++i) if (pat[i]) t.push_back(i);
    if ((int)occ.size() != k) return false;
    for (int r = 0; r < n; ++r) {
        std::set<int> s; for (int c : t) s.insert((c + r) % n);
        if (s == occ) return true;
    }
    return false;
}

static void run() {
    // (§6.1) Euclid recovery: flat measure, q=1 → a rotation of E(k,n) for all
    // k<n≤32. Independent Bjorklund reference; occupancy compared up to rotation.
    {
        int pass = 0, total = 0;
        for (int n = 2; n <= 32; ++n) {
            for (int k = 1; k < n; ++k) {
                ++total;
                MeasuredEuclid e; e.setN(n); e.setK(k);
                e.setDrawnPreset(MeasuredEuclid::Preset::Flat);
                e.setQuantize(1.0); e.setPhase(0.0);
                TickContext c; c.generation = 0; e.tick(c);
                std::set<int> occ;
                for (const auto& ev : e.latchedEvents())
                    occ.insert(((int)std::lround(ev.barPhase * n) % n + n) % n);
                if (isRotationOfEuclid(occ, k, n)) ++pass;
                else std::printf("  E(%d,%d): flat+q=1 not a rotation of Bjorklund (got %zu cells)\n",
                                 k, n, occ.size());
                CHECK(isRotationOfEuclid(occ, k, n));
            }
        }
        std::printf("(§6.1) Euclid recovery: %d/%d (k,n) matched Bjorklund up to rotation\n", pass, total);
    }

    // (§6.4) Monotone deformation: onsets strictly increasing in μ-order for any
    // measure at q<1 (no crossings). At q=1, collisions are allowed but must be
    // distinct sourceIds.
    {
        const MeasuredEuclid::Preset presets[] = {
            MeasuredEuclid::Preset::Flat, MeasuredEuclid::Preset::RampUp,
            MeasuredEuclid::Preset::RampDown, MeasuredEuclid::Preset::Waves,
            MeasuredEuclid::Preset::Beats};
        int checked = 0;
        for (auto pr : presets) {
            for (int k : {3, 5, 7, 11}) {
                for (double q : {0.0, 0.3, 0.6, 0.9}) {
                    MeasuredEuclid e; e.seed(7, 1); e.setN(16); e.setK(k);
                    e.setDrawnPreset(pr); e.setQuantize(q); e.setPhase(0.13);
                    TickContext c; c.generation = 0; e.tick(c);
                    auto ev = e.latchedEvents();
                    // μ-order onsets are strictly increasing up to the single wrap.
                    bool strictly = true;
                    for (int i = 0; i + 1 < (int)ev.size(); ++i)
                        if (!(ev[i].barPhase < ev[i + 1].barPhase)) strictly = false;
                    CHECK(strictly);
                    ++checked;
                }
            }
        }
        // q=1 collisions: distinct sourceIds even when phases coincide.
        {
            MeasuredEuclid e; e.setN(4); e.setK(8); e.setDrawnPreset(MeasuredEuclid::Preset::Flat);
            e.setQuantize(1.0); TickContext c; c.generation = 0; e.tick(c);
            std::set<int> ids; auto ev = e.latchedEvents();
            for (const auto& x : ev) ids.insert(x.sourceId);
            CHECK_EQ((int)ids.size(), (int)ev.size());   // all sourceIds distinct
        }
        std::printf("(§6.4) monotone deformation: %d configs strictly increasing at q<1\n", checked);
    }
}

RUN_MAIN()
