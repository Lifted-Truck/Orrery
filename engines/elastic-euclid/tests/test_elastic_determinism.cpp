// test_elastic_determinism.cpp — spec §8.2: identical seed + identical event
// sequence → bit-identical state across runs; + save/load reproduces exactly.
#include "orrery/engines/ElasticEuclid.h"
#include "test_util.h"

using namespace orrery;

// A fixed gesture + tick script exercising randomness (Add jitter, Kick) and
// physics. Returns nothing — the caller compares engine state after each step.
static void step(ElasticEuclid& e, int t) {
    if (t % 7 == 3) e.handleGesture({GestureEvent::Type::Kick, 0, 1.0f});
    if (t % 11 == 5) e.handleGesture({GestureEvent::Type::Add, 0, 0.0f});
    if (t % 13 == 9) e.handleGesture({GestureEvent::Type::Remove, 0, 0.0f});
    if (t % 5 == 2) e.handleMidiIn({1, 60 + t, 100, 0.5f});
    TickContext c; c.generation = t; e.tick(c);
}

static bool identical(const ElasticEuclid& a, const ElasticEuclid& b) {
    if (a.sourceCount() != b.sourceCount()) return false;
    for (int i = 0; i < a.sourceCount(); ++i) {
        if (a.theta(i) != b.theta(i)) return false;   // exact double equality
        if (a.omega(i) != b.omega(i)) return false;
    }
    return true;
}

static void run() {
    // Two engines, same seed + same script → bit-identical throughout.
    {
        ElasticEuclid a, b;
        a.seed(0xDEADBEEF, 2); b.seed(0xDEADBEEF, 2);
        a.setN(16); b.setN(16);
        for (int t = 0; t < 300; ++t) { step(a, t); step(b, t); CHECK(identical(a, b)); }
    }

    // Different seed diverges (randomness actually flows through the physics).
    {
        ElasticEuclid a, b;
        a.seed(1, 0); b.seed(2, 0);
        a.setN(16); b.setN(16);
        for (int t = 0; t < 60; ++t) { step(a, t); step(b, t); }
        CHECK(!identical(a, b));
    }

    // Save/load mid-run reproduces the continuation exactly.
    {
        ElasticEuclid a; a.seed(99, 3); a.setN(16);
        for (int t = 0; t < 40; ++t) step(a, t);

        Chunk snap; a.saveState(snap);
        ElasticEuclid b; b.loadState(snap);
        CHECK(identical(a, b));

        for (int t = 40; t < 90; ++t) { step(a, t); step(b, t); CHECK(identical(a, b)); }
    }
}

RUN_MAIN()
