// Contract.h — the IEngine seam (sequencer-studio-architecture.md §1).
//
// This is THE organ boundary. Every engine imports this header and nothing else
// of its neighbors. A change here is a contract-version event (human-gated).
//
// CONTRACT v1.1 (2026-07-29). v1.0 defined the latch `IEngine` only. v1.1 adds
// the two CLOCKING VARIANTS beside it — `ITickEngine` (per-tick) and
// `IFreeTransportEngine` (continuous) — designed together in one event, because
// two pending engines need off the latch for the same underlying reason
// (brief lathe-2026-07-23-001 / DECISIONS #22; Kuramoto / DECISIONS #8).
// The latch seam is UNCHANGED and remains the default; variants are additive,
// so every existing engine keeps compiling untouched.
// Still absent pending its own gate: pitch-native output resolution
// (Torus — DECISIONS #9).
#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "orrery/Pcg32.h"
#include "orrery/Types.h"

namespace orrery {

// Context handed to an engine at each latch. The engine advances one generation
// and must be deterministic given (state, rng). RT-safe: no allocation, no
// locking, no wall-clock reads downstream of here.
struct TickContext {
    int64_t     generation = 0;      // monotonic latch index for this slot
    ClockConfig clock;               // this slot's division / barsPerLap
    double      tempoBpm   = 120.0;  // informational only — never for timing math
    Pcg32*      rng        = nullptr;// per-slot deterministic stream (non-owning)
};

// A hand gesture from the GUI (drained from an SPSC queue at block start).
struct GestureEvent {
    enum class Type { Kick, Drag, Add, Remove, CurveEdit };
    Type    type     = Type::Kick;
    int32_t sourceId = 0;
    float   value    = 0.0f;  // gesture-defined payload
};

// Incoming MIDI mapped to an engine perturbation; each engine defines its own
// semantics (e.g. elastic = kick impulse on the matched particle).
struct MidiPerturbation {
    int   channel  = 1;
    int   note     = 60;
    int   velocity = 100;
    float amount   = 1.0f;
};

// Growable byte buffer for save/loadState. NOT on the tick path — allocation is
// fine here. Little tagged POD reader/writer so engine state round-trips.
struct Chunk {
    std::vector<uint8_t> bytes;
    mutable size_t       readPos = 0;

    template <typename T>
    void put(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>, "Chunk::put needs POD");
        const auto* p = reinterpret_cast<const uint8_t*>(&v);
        bytes.insert(bytes.end(), p, p + sizeof(T));
    }
    // Bounds-checked: reading past the end yields a zeroed T instead of an
    // out-of-bounds read. A short/truncated chunk is a real scenario (a state
    // written by a build with different capacity constants — see the v1.2
    // kMaxSources change), and silently walking off the buffer is the worst
    // possible response to it.
    template <typename T>
    T get() const {
        T v{};
        if (readPos + sizeof(T) > bytes.size()) { readPos = bytes.size(); return v; }
        std::memcpy(&v, bytes.data() + readPos, sizeof(T));
        readPos += sizeof(T);
        return v;
    }
    bool exhausted() const { return readPos >= bytes.size(); }
    void rewind() const { readPos = 0; }
};

// Trace sink (contract §6). A line-oriented JSONL writer; the concrete shell
// implementation drains a ring buffer off-thread. Engines and the offset layer
// emit one record per generation. The core ships StringTraceWriter (Trace.h).
class TraceWriter {
public:
    virtual ~TraceWriter() = default;
    virtual void writeLine(const std::string& jsonLine) = 0;
};

// ── §1.1 IEngine ─────────────────────────────────────────────────────────────
class IEngine {
public:
    virtual ~IEngine() = default;

    // Advance one generation, latch the event set for the next division.
    // Deterministic given (state, ctx.rng). RT-safe: no alloc/lock/log/wall-clock.
    virtual void tick(TickContext& ctx) = 0;

    // The latched events valid until the next tick(). Backed by fixed storage.
    virtual std::span<const TriggerEvent> latchedEvents() const = 0;

    virtual void handleGesture(const GestureEvent&)       = 0;
    virtual void handleMidiIn(const MidiPerturbation&)    = 0;

    virtual void saveState(Chunk&) const = 0;
    virtual void loadState(const Chunk&) = 0;

    virtual void writeTrace(TraceWriter&) const = 0;
};

// ── §1.1b Clocking variants (contract v1.1) ─────────────────────────────────
// Shared obligations for BOTH variants — identical to the latch seam, because
// they are what make an engine an engine here: deterministic given
// (state, seed, gesture log); RT-safe `advance` (no alloc / lock / log /
// wall-clock read); fixed-capacity storage; never emit MIDI; `sourceId` stable
// per §1.2. Only the CLOCK differs.
//
// Randomness note (DECISIONS #22): the substrate does NOT dictate an engine's
// generator. Engines own their streams (Lathe's LATHE keeps mulberry32 so its
// port-pin stays bit-exact); the station seeds what it owns. Determinism binds
// at the seam — identical inputs ⇒ identical event stream — not at the RNG.

// Per-tick context. (Named apart from the latch seam's `TickContext`, which is
// a *latch/generation* context despite its name — renaming that would break
// every shipped engine, so the variants take distinct names instead.)
struct TickEngineContext {
    int64_t tick     = 0;        // monotonic transport tick
    double  tempoBpm = 120.0;    // informational; never for timing math
    Pcg32*  rng      = nullptr;  // optional — engines may own their own
};

// One step per transport tick, firing ON ticks (Lathe's LATHE).
class ITickEngine {
public:
    virtual ~ITickEngine() = default;
    // Advance exactly one tick; return the events fired ON this tick. The span
    // is valid until the next call (fixed-capacity, no per-call allocation).
    virtual std::span<const TickEvent> tickAdvance(const TickEngineContext&) = 0;

    virtual void handleGesture(const GestureEvent&)    = 0;
    virtual void handleMidiIn(const MidiPerturbation&) = 0;
    virtual void saveState(Chunk&) const = 0;
    virtual void loadState(const Chunk&) = 0;
    virtual void writeTrace(TraceWriter&) const = 0;
};

// Continuous integration over a block; no tick, no latch (Kuramoto rotors).
struct FreeTransportContext {
    double  sampleRate = 48000.0;
    int32_t blockSize  = 512;
    double  tempoBpm   = 120.0;
    double  stepSize   = 1.0 / 48000.0;  // fixed integration step h (seconds)
    Pcg32*  rng        = nullptr;
};

class IFreeTransportEngine {
public:
    virtual ~IFreeTransportEngine() = default;
    // Integrate across one audio block; return triggers at sample-accurate
    // offsets within it. Span valid until the next call.
    virtual std::span<const FreeEvent> advanceBlock(const FreeTransportContext&) = 0;

    virtual void handleGesture(const GestureEvent&)    = 0;
    virtual void handleMidiIn(const MidiPerturbation&) = 0;
    virtual void saveState(Chunk&) const = 0;
    virtual void loadState(const Chunk&) = 0;
    virtual void writeTrace(TraceWriter&) const = 0;
};

} // namespace orrery
