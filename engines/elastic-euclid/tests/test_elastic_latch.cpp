// test_elastic_latch.cpp — spec §8.5 + latch semantics (§2.4): adding/removing a
// particle (or a kick) NEVER moves other particles' θ within a bar; the latched
// event set is frozen between ticks.
#include "orrery/engines/ElasticEuclid.h"
#include "test_util.h"

using namespace orrery;

static void run() {
    ElasticEuclid e; e.seed(5, 1); e.setN(16);
    for (int t = 0; t < 10; ++t) { TickContext c; c.generation = t; e.tick(c); }

    const int k0 = e.sourceCount();
    CHECK_EQ(static_cast<int>(e.latchedEvents().size()), k0);

    // Snapshot θ of the current particles.
    double th[kMaxSources];
    for (int i = 0; i < k0; ++i) th[i] = e.theta(i);

    // Add: a new particle appears; the k0 existing particles do NOT move, and
    // the latched set stays frozen until the next tick.
    e.handleGesture({GestureEvent::Type::Add, 0, 0.0f});
    CHECK_EQ(e.sourceCount(), k0 + 1);
    for (int i = 0; i < k0; ++i) CHECK(e.theta(i) == th[i]);          // bit-identical
    CHECK_EQ(static_cast<int>(e.latchedEvents().size()), k0);         // still frozen

    // Kick: stored velocity only — no θ moves until a tick.
    double th2[kMaxSources];
    for (int i = 0; i < e.sourceCount(); ++i) th2[i] = e.theta(i);
    e.handleGesture({GestureEvent::Type::Kick, 0, 1.0f});
    for (int i = 0; i < e.sourceCount(); ++i) CHECK(e.theta(i) == th2[i]);

    // Remove (LIFO): survivors keep their exact θ.
    const int k1 = e.sourceCount();
    double th3[kMaxSources];
    for (int i = 0; i < k1 - 1; ++i) th3[i] = e.theta(i);
    e.handleGesture({GestureEvent::Type::Remove, 0, 0.0f});
    CHECK_EQ(e.sourceCount(), k1 - 1);
    for (int i = 0; i < k1 - 1; ++i) CHECK(e.theta(i) == th3[i]);

    // The next tick unfreezes the latch and lets everything relax together.
    TickContext c; c.generation = 11; e.tick(c);
    CHECK_EQ(static_cast<int>(e.latchedEvents().size()), e.sourceCount());

    // FREEZE (additive check for the loop-lock capability): while frozen, ticks
    // leave θ/ω bit-identical (the pattern repeats exactly); unfreezing after a
    // kick resumes relaxation from the stored state.
    {
        ElasticEuclid f; f.seed(9, 2); f.setN(16);
        for (int t = 0; t < 5; ++t) { TickContext tc; tc.generation = t; f.tick(tc); }
        f.handleGesture({GestureEvent::Type::Kick, 0, 1.0f});   // stored energy
        f.setFrozen(true);
        double th[kMaxSources], om[kMaxSources];
        const int k = f.sourceCount();
        for (int i = 0; i < k; ++i) { th[i] = f.theta(i); om[i] = f.omega(i); }
        for (int t = 5; t < 15; ++t) { TickContext tc; tc.generation = t; f.tick(tc); }
        for (int i = 0; i < k; ++i) { CHECK(f.theta(i) == th[i]); CHECK(f.omega(i) == om[i]); }
        f.setFrozen(false);
        TickContext tc; tc.generation = 15; f.tick(tc);
        bool moved = false;
        for (int i = 0; i < k; ++i) if (f.theta(i) != th[i]) moved = true;
        CHECK(moved);   // the stored kick energy resumes relaxing
    }
}

RUN_MAIN()
