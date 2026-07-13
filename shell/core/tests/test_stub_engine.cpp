// test_stub_engine.cpp — the slot contract: sourceId stability under Add/Remove
// (§1.2) and state save/load round-trip.
#include "orrery/Pcg32.h"
#include "orrery/StubEngine.h"
#include "test_util.h"

using namespace orrery;

static void tickOnce(StubEngine& e, Pcg32& rng, int64_t gen) {
    TickContext ctx; ctx.generation = gen; ctx.rng = &rng;
    e.tick(ctx);
}

static void run() {
    Pcg32 rng; rng.seed(5, 0);
    StubEngine e(8);
    tickOnce(e, rng, 0);
    CHECK_EQ(e.sourceCount(), 8);
    {
        auto ev = e.latchedEvents();
        CHECK_EQ((int)ev.size(), 8);
        for (int i = 0; i < 8; ++i) CHECK_EQ(ev[i].sourceId, i);   // ids 0..7
        CHECK_NEAR(ev[0].barPhase, 0.0, 1e-12);
        CHECK_NEAR(ev[4].barPhase, 0.5, 1e-12);                    // even spread
    }

    // Remove is LIFO — id 7 vanishes, survivors 0..6 keep identity.
    e.handleGesture({GestureEvent::Type::Remove, 0, 0.0f});
    tickOnce(e, rng, 1);
    CHECK_EQ(e.sourceCount(), 7);
    {
        auto ev = e.latchedEvents();
        CHECK_EQ((int)ev.size(), 7);
        for (int i = 0; i < 7; ++i) CHECK_EQ(ev[i].sourceId, i);
    }

    // Add restores id 7 with a fresh (zero-bias) identity.
    e.handleGesture({GestureEvent::Type::Add, 0, 0.0f});
    tickOnce(e, rng, 2);
    CHECK_EQ(e.sourceCount(), 8);
    CHECK_EQ(e.latchedEvents()[7].sourceId, 7);

    // Remove never drops below one source.
    for (int i = 0; i < 20; ++i) e.handleGesture({GestureEvent::Type::Remove, 0, 0.0f});
    CHECK_EQ(e.sourceCount(), 1);

    // Save / load round-trip reproduces state exactly.
    {
        Pcg32 r2; r2.seed(11, 0);
        StubEngine a(5);
        for (int g = 0; g < 7; ++g) tickOnce(a, r2, g);
        a.handleGesture({GestureEvent::Type::Kick, 2, 0.3f});
        Chunk chunk; a.saveState(chunk);

        StubEngine b(1);
        b.loadState(chunk);
        CHECK_EQ(b.sourceCount(), a.sourceCount());

        // From the same rng position, both engines tick identically.
        Pcg32 ra; ra.seed(50, 0);
        Pcg32 rb; rb.seed(50, 0);
        tickOnce(a, ra, 100);
        tickOnce(b, rb, 100);
        auto ea = a.latchedEvents();
        auto eb = b.latchedEvents();
        CHECK_EQ(ea.size(), eb.size());
        for (size_t i = 0; i < ea.size(); ++i) {
            CHECK_EQ(ea[i].sourceId, eb[i].sourceId);
            CHECK(ea[i].energy == eb[i].energy);
        }
    }
}

RUN_MAIN()
