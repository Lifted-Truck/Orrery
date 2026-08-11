// test_contract_v11.cpp — contract v1.1: clocking variants + the TOL timing lane.
//
// PROVENANCE: the timing-lane assertions here are the **consumer-proposed
// contract tests** from Lathe's brief `lathe-2026-07-23-001`, committed into the
// PROVIDER's CI per INTEGRATIONS §3 ("consumer-authored, resident-landed"). They
// now gate Orrery's future changes: if a later refactor breaks Lathe's
// expectations, THIS build fails — coordination without conversation. Do not
// weaken them without a linked exchange.
#include "orrery/Contract.h"
#include "orrery/OffsetLayer.h"
#include "test_util.h"

#include <vector>

using namespace orrery;

// ── Minimal variant implementations, to prove the seams are implementable ────
namespace {

// Per-tick engine: source r fires when (tick + r) % period == 0, with a
// deterministic RNG-free overshoot. Stands in for Lathe's LATHE at this seam.
class TickStub : public ITickEngine {
public:
    TickStub(int sources, int period) : n_(sources), period_(period) { fired_.reserve((size_t)sources); }
    std::span<const TickEvent> tickAdvance(const TickEngineContext& ctx) override {
        fired_.clear();
        for (int r = 0; r < n_; ++r) {
            if (period_ > 0 && ((ctx.tick + r) % period_) == 0) {
                TickEvent e;
                e.sourceId = r;
                e.tick = (int32_t)ctx.tick;
                e.vel = 0.8f;
                e.overshootFrac = (float)(((ctx.tick + r) % 7) / 7.0);
                fired_.push_back(e);
            }
        }
        return fired_;
    }
    void handleGesture(const GestureEvent&) override {}
    void handleMidiIn(const MidiPerturbation&) override {}
    void saveState(Chunk& c) const override { c.put<int32_t>(n_); c.put<int32_t>(period_); }
    void loadState(const Chunk& c) override { c.rewind(); n_ = c.get<int32_t>(); period_ = c.get<int32_t>(); }
    void writeTrace(TraceWriter&) const override {}
private:
    int n_, period_;
    std::vector<TickEvent> fired_;
};

// Free-transport engine: a phase accumulator integrated at a fixed step,
// emitting a sample-accurate trigger on each wrap. Stands in for Kuramoto.
class FreeStub : public IFreeTransportEngine {
public:
    explicit FreeStub(double hz) : hz_(hz) { fired_.reserve(64); }
    std::span<const FreeEvent> advanceBlock(const FreeTransportContext& ctx) override {
        fired_.clear();
        for (int s = 0; s < ctx.blockSize; ++s) {
            phase_ += hz_ / ctx.sampleRate;
            if (phase_ >= 1.0) {
                phase_ -= 1.0;
                FreeEvent e; e.sampleOffset = s; e.sourceId = 0; e.energy = 1.0f;
                fired_.push_back(e);
            }
        }
        return fired_;
    }
    void handleGesture(const GestureEvent&) override {}
    void handleMidiIn(const MidiPerturbation&) override {}
    void saveState(Chunk& c) const override { c.put<double>(phase_); }
    void loadState(const Chunk& c) override { c.rewind(); phase_ = c.get<double>(); }
    void writeTrace(TraceWriter&) const override {}
private:
    double hz_, phase_ = 0.0;
    std::vector<FreeEvent> fired_;
};

} // namespace

static void run() {
    // ── (Lathe contract test 1) tolAmount=0 ⇒ grid-exact ────────────────────
    {
        OffsetLayer ol;
        ol.timingParams().tolAmount = 0.0f;
        const double tickTime = 10.0, tickDur = 0.5;
        // Even position (no swing), no per-source offset: the integer tick wins
        // verbatim, whatever the engine's overshoot was.
        CHECK(ol.eventTime(0, /*pos*/0, tickTime, tickDur, 0.9f) == tickTime);
        CHECK(ol.eventTime(0, 0, tickTime, tickDur, 0.0f) == tickTime);
    }

    // ── (Lathe contract test 2) tolAmount=1 ⇒ shifts by overshootFrac·tickDur,
    //     and NEVER alters the tick index the engine reported ─────────────────
    {
        OffsetLayer ol;
        ol.timingParams().tolAmount = 1.0f;
        const double tickTime = 10.0, tickDur = 0.5;
        CHECK_NEAR(ol.eventTime(0, 0, tickTime, tickDur, 0.5f), tickTime + 0.25, 1e-12);
        CHECK_NEAR(ol.eventTime(0, 0, tickTime, tickDur, 1.0f), tickTime + 0.50, 1e-12);
        // The lane is presentation-only: the engine's own event is untouched.
        TickStub eng(4, 4);
        TickEngineContext ctx; ctx.tick = 8;
        auto ev = eng.tickAdvance(ctx);
        CHECK(!ev.empty());
        for (const auto& e : ev) CHECK_EQ(e.tick, 8);   // tick index intact
    }

    // ── Composition: tol + swing + per-source offset all stack ──────────────
    {
        OffsetLayer ol;
        ol.timingParams().tolAmount = 1.0f;
        ol.timingParams().swing = 1.0f;          // odd positions +0.5·tickDur
        ol.setTimingOffset(3, 0.25f);            // +0.25 tick, and PINS the cell
        const double tickTime = 0.0, tickDur = 1.0;
        // pos 1 (odd) → swing 0.5; overshoot 0.25 → tol 0.25; offset 0.25.
        CHECK_NEAR(ol.eventTime(3, 1, tickTime, tickDur, 0.25f), 1.0, 1e-12);
        // Same source at an even position loses only the swing term.
        CHECK_NEAR(ol.eventTime(3, 2, tickTime, tickDur, 0.25f), 0.5, 1e-12);
        // Hand edit is a pin (pins-and-flow, §2.2 rule extended to timing).
        CHECK(ol.timingCell(3).lock);
        CHECK(!ol.timingCell(4).lock);
        ol.unlockAllTiming();
        CHECK(!ol.timingCell(3).lock);
    }

    // Default is unchanged behavior: a fresh layer is grid-exact.
    {
        OffsetLayer ol;
        CHECK(ol.eventTime(0, 1, 5.0, 0.5, 0.87f) == 5.0);
    }

    // ── ITickEngine: implementable + deterministic + state round-trip ────────
    {
        auto stream = [](TickStub& e) {
            std::vector<TickEvent> all;
            for (int64_t t = 0; t < 64; ++t) {
                TickEngineContext c; c.tick = t;
                for (const auto& ev : e.tickAdvance(c)) all.push_back(ev);
            }
            return all;
        };
        TickStub a(5, 4), b(5, 4);
        const auto sa = stream(a), sb = stream(b);
        CHECK(!sa.empty());
        CHECK_EQ(sa.size(), sb.size());
        bool same = true;
        for (size_t i = 0; i < sa.size(); ++i)
            same = same && sa[i].sourceId == sb[i].sourceId && sa[i].tick == sb[i].tick
                        && sa[i].overshootFrac == sb[i].overshootFrac;
        CHECK(same);

        Chunk chunk; a.saveState(chunk);
        TickStub c(1, 1); c.loadState(chunk);
        TickEngineContext cx; cx.tick = 0;
        CHECK_EQ((int)c.tickAdvance(cx).size(), (int)b.tickAdvance(cx).size());
    }

    // ── IFreeTransportEngine: sample-accurate offsets inside the block ───────
    {
        FreeStub f(100.0);   // 100 Hz at 48k → a wrap every 480 samples
        FreeTransportContext ctx; ctx.sampleRate = 48000.0; ctx.blockSize = 512;
        auto ev = f.advanceBlock(ctx);
        CHECK_EQ((int)ev.size(), 1);
        for (const auto& e : ev) CHECK(e.sampleOffset >= 0 && e.sampleOffset < ctx.blockSize);
        // Determinism across two identically-seeded runs.
        FreeStub g(100.0), h(100.0);
        for (int b = 0; b < 8; ++b) {
            auto eg = g.advanceBlock(ctx), eh = h.advanceBlock(ctx);
            CHECK_EQ(eg.size(), eh.size());
            for (size_t i = 0; i < eg.size(); ++i) CHECK_EQ(eg[i].sampleOffset, eh[i].sampleOffset);
        }
    }
}

RUN_MAIN()
