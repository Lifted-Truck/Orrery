// test_determinism.cpp — bit-identity of the whole core pipeline (§6).
// clock-driven stub → offset layer → router. Same seed ⇒ identical MIDI stream;
// different seed ⇒ different; save/load mid-run ⇒ identical continuation.
#include "orrery/MidiRouter.h"
#include "orrery/OffsetLayer.h"
#include "orrery/Pcg32.h"
#include "orrery/StubEngine.h"
#include "test_util.h"

#include <vector>

using namespace orrery;

// Fold one resolved MIDI note into a running hash (field-by-field so struct
// padding never leaks into the digest).
static uint64_t foldNote(uint64_t h, const MidiNote& n) {
    h = fnv1a(&n.channel, sizeof n.channel, h);
    h = fnv1a(&n.note, sizeof n.note, h);
    h = fnv1a(&n.velocity, sizeof n.velocity, h);
    h = fnv1a(&n.barPhase, sizeof n.barPhase, h);
    h = fnv1a(&n.gateSamples, sizeof n.gateSamples, h);
    return h;
}

static uint64_t runPipeline(uint64_t seed, int gens) {
    Pcg32 slot; slot.seed(seed, 0);
    OffsetLayer ol; ol.seed(seed, 0xFFFF);
    ol.walk()   = WalkParams{true, true, true, 2, 12, 1, 1.0f};
    ol.accent() = AccentParams{true, 3, 20, 1.0f};
    MidiRouter router; router.quantizeOut = 0.5f; router.quantizeGrid = 16;
    StubEngine eng(8);
    std::vector<int32_t> present = {0,1,2,3,4,5,6,7};

    uint64_t h = 1469598103934665603ULL;
    for (int g = 0; g < gens; ++g) {
        TickContext ctx; ctx.generation = g; ctx.tempoBpm = 120.0; ctx.rng = &slot;
        eng.tick(ctx);
        ol.runGenerators(g, present);
        for (const auto& ev : eng.latchedEvents())
            h = foldNote(h, router.route(ev, ol.cell(ev.sourceId)));
    }
    return h;
}

static void run() {
    // Reproducible: two runs from the same seed are bit-identical.
    CHECK_EQ(runPipeline(0xABCDEF, 64), runPipeline(0xABCDEF, 64));

    // Seed-sensitive: a different project seed diverges.
    CHECK(runPipeline(0xABCDEF, 64) != runPipeline(0x123456, 64));

    // Save/load mid-run reproduces the continuation exactly.
    {
        const uint64_t seed = 7;
        Pcg32 slotA; slotA.seed(seed, 0);
        StubEngine a(8);
        for (int g = 0; g < 10; ++g) { TickContext c; c.generation = g; c.rng = &slotA; a.tick(c); }

        Chunk snap; a.saveState(snap);
        StubEngine b(1); b.loadState(snap);

        // Continue both from an identically-seeded rng position.
        Pcg32 rA; rA.seed(seed, 42);
        Pcg32 rB; rB.seed(seed, 42);
        uint64_t hA = 1469598103934665603ULL, hB = hA;
        for (int g = 10; g < 30; ++g) {
            TickContext ca; ca.generation = g; ca.rng = &rA; a.tick(ca);
            TickContext cb; cb.generation = g; cb.rng = &rB; b.tick(cb);
            for (const auto& ev : a.latchedEvents()) hA = fnv1a(&ev.energy, sizeof ev.energy, hA);
            for (const auto& ev : b.latchedEvents()) hB = fnv1a(&ev.energy, sizeof ev.energy, hB);
        }
        CHECK_EQ(hA, hB);
    }
}

RUN_MAIN()
