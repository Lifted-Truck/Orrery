// test_probable_determinism.cpp — spec §6.4 (determinism bit-identity) + §6.5
// (freeze invariant: no resampling while frozen).
#include "orrery/engines/ProbableEuclid.h"
#include "test_util.h"

#include <vector>

using namespace orrery;

static std::vector<int> realized(const ProbableEuclid& e) {
    std::vector<int> v; for (const auto& ev : e.latchedEvents()) v.push_back(ev.sourceId); return v;
}

static void run() {
    // (§6.4) Determinism: same (seed, param timeline) → bit-identical realization
    // sequence. A deterministic performance of a stochastic object.
    {
        ProbableEuclid a, b;
        a.seed(0xBEEF, 2); b.seed(0xBEEF, 2);
        for (auto* e : {&a, &b}) { e->setN(16); e->setDensity(6.0); e->setTemperature(0.35); }
        for (int t = 0; t < 64; ++t) {
            if (t == 20) { a.setClump(0.4); b.setClump(0.4); }
            if (t == 40) { a.setDensity(9.0); b.setDensity(9.0); }
            TickContext ca; ca.generation = t; a.tick(ca);
            TickContext cb; cb.generation = t; b.tick(cb);
            CHECK(realized(a) == realized(b));
        }
        // Different seed → different realization sequence (randomness flows).
        ProbableEuclid p, q;
        p.seed(0xBEEF, 2); q.seed(0x1234, 2);
        for (auto* e : {&p, &q}) { e->setN(16); e->setDensity(6.0); e->setTemperature(0.35); }
        bool anyDiff = false;
        for (int t = 0; t < 32; ++t) {
            TickContext cp; cp.generation = t; p.tick(cp);
            TickContext cq; cq.generation = t; q.tick(cq);
            if (realized(p) != realized(q)) anyDiff = true;
        }
        CHECK(anyDiff);
    }

    // (§6.5) Freeze invariant: while frozen, the realization does not change
    // across bar boundaries OR parameter/field edits.
    {
        ProbableEuclid e; e.seed(5, 1); e.setN(16); e.setDensity(7.0); e.setTemperature(0.4);
        TickContext c0; c0.generation = 0; e.tick(c0);
        const std::vector<int> held = realized(e);

        e.setFrozen(true);
        for (int t = 1; t < 20; ++t) {
            if (t == 5) e.setDensity(11.0);        // field edit — display only while frozen
            if (t == 10) e.setClump(0.7);
            if (t == 15) e.roll();                 // ROLL is suppressed while frozen
            TickContext c; c.generation = t; e.tick(c);
            CHECK(realized(e) == held);            // never resampled
        }
        // Unfreeze → next tick resamples (with the edited field it will differ).
        e.setFrozen(false);
        TickContext c; c.generation = 20; e.tick(c);
        CHECK(realized(e) != held);
    }
}

RUN_MAIN()
