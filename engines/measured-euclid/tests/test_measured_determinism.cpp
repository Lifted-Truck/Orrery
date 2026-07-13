// test_measured_determinism.cpp — spec §6.2 (inverse accuracy), §6.3 (latch
// invariant), §6.5 (determinism bit-identity + save/load).
#include "orrery/engines/MeasuredEuclid.h"
#include "test_util.h"

#include <vector>

using namespace orrery;

static bool sameOnsets(const MeasuredEuclid& a, const MeasuredEuclid& b) {
    if (a.sourceCount() != b.sourceCount()) return false;
    auto ea = a.latchedEvents(), eb = b.latchedEvents();
    if (ea.size() != eb.size()) return false;
    for (size_t i = 0; i < ea.size(); ++i)
        if (ea[i].barPhase != eb[i].barPhase || ea[i].sourceId != eb[i].sourceId) return false;
    return true;
}

static void run() {
    // (§6.2) inverse accuracy: |U(invU(u)) − u| < 1e-6 over random u & smooth
    // curves (exact inverse of the piecewise-linear CDF, so this validates the
    // CDF construction rather than tolerating slop).
    {
        for (auto pr : {MeasuredEuclid::Preset::Waves, MeasuredEuclid::Preset::Beats,
                        MeasuredEuclid::Preset::Rand, MeasuredEuclid::Preset::RampUp}) {
            MeasuredEuclid e; e.seed(1234, 5); e.setDrawnPreset(pr);
            TickContext c; c.generation = 0; e.tick(c);   // builds the CDF
            Pcg32 r; r.seed(99, 1);
            double worst = 0;
            for (int i = 0; i < 10000; ++i) {
                const double u = r.nextFloat();
                const double back = e.forwardU(e.invU(u));
                worst = std::max(worst, std::fabs(back - u));
            }
            CHECK(worst < 1e-6);
        }
    }

    // (§6.3) latch invariant: params changed mid-bar do NOT recompute onsets
    // until the next tick.
    {
        MeasuredEuclid e; e.setN(16); e.setK(7); e.setDrawnPreset(MeasuredEuclid::Preset::Beats);
        TickContext c; c.generation = 0; e.tick(c);
        std::vector<double> before;
        for (const auto& ev : e.latchedEvents()) before.push_back(ev.barPhase);

        e.setK(3); e.setQuantize(1.0);
        e.handleGesture({GestureEvent::Type::CurveEdit, 0, 2.0f});   // RampDown
        // No tick → latched set is unchanged.
        CHECK_EQ((int)e.latchedEvents().size(), (int)before.size());
        bool unchanged = true;
        auto ev = e.latchedEvents();
        for (size_t i = 0; i < before.size(); ++i) if (ev[i].barPhase != before[i]) unchanged = false;
        CHECK(unchanged);

        TickContext c2; c2.generation = 1; e.tick(c2);               // now it recomputes
        CHECK_EQ(e.sourceCount(), 3);
    }

    // (§6.5) determinism: same seed + edit sequence → bit-identical; save/load.
    {
        MeasuredEuclid a, b;
        a.seed(0xABCD, 3); b.seed(0xABCD, 3);
        for (auto* e : {&a, &b}) {
            e->setN(16); e->setK(7); e->setDrawnPreset(MeasuredEuclid::Preset::Rand);
            e->setBreathe(true, 8);
        }
        for (int t = 0; t < 40; ++t) {
            if (t == 10) { a.setQuantize(0.5); b.setQuantize(0.5); }
            if (t == 20) { a.handleGesture({GestureEvent::Type::Add,0,0}); b.handleGesture({GestureEvent::Type::Add,0,0}); }
            TickContext ca; ca.generation = t; a.tick(ca);
            TickContext cb; cb.generation = t; b.tick(cb);
            CHECK(sameOnsets(a, b));
        }

        Chunk snap; a.saveState(snap);
        MeasuredEuclid d; d.loadState(snap);
        TickContext ca; ca.generation = 40; a.tick(ca);
        TickContext cd; cd.generation = 40; d.tick(cd);
        CHECK(sameOnsets(a, d));
    }
}

RUN_MAIN()
