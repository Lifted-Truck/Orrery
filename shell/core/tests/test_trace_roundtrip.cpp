// test_trace_roundtrip.cpp — a trace round-trips losslessly (§6): serialize the
// stub engine's latched events to JSONL, parse them back, compare bit-for-bit.
#include "orrery/Pcg32.h"
#include "orrery/StubEngine.h"
#include "orrery/Trace.h"
#include "test_util.h"

#include <vector>

using namespace orrery;

static void run() {
    StubEngine eng(6);
    Pcg32 rng; rng.seed(2026, 0);
    StringTraceWriter tw;

    // Keep the emitted events per generation so we can diff against the parse.
    std::vector<std::vector<TriggerEvent>> emitted;
    const int gens = 25;
    for (int g = 0; g < gens; ++g) {
        TickContext ctx; ctx.generation = g; ctx.rng = &rng;
        eng.tick(ctx);
        auto ev = eng.latchedEvents();
        emitted.emplace_back(ev.begin(), ev.end());
        eng.writeTrace(tw);
    }

    CHECK_EQ((int)tw.lines().size(), gens);

    for (int g = 0; g < gens; ++g) {
        trace::EngineRecord rec;
        CHECK(trace::parseEngineLine(tw.lines()[g], rec));
        CHECK_EQ(rec.gen, (int64_t)g);
        CHECK_EQ(rec.events.size(), emitted[g].size());
        for (size_t i = 0; i < rec.events.size(); ++i) {
            // %.17g / strtod → exact IEEE round-trip, so require exact equality.
            CHECK(rec.events[i].barPhase == emitted[g][i].barPhase);
            CHECK_EQ(rec.events[i].sourceId, emitted[g][i].sourceId);
            CHECK(rec.events[i].energy == emitted[g][i].energy);
        }
    }

    // A non-engine line is rejected.
    {
        trace::EngineRecord rec;
        CHECK(!trace::parseEngineLine("{\"kind\":\"offset\",\"gen\":0,\"cells\":[]}", rec));
    }
}

RUN_MAIN()
