// Contract.h — the IEngine seam (sequencer-studio-architecture.md §1).
//
// This is THE organ boundary. Every engine imports this header and nothing else
// of its neighbors. A change here is a contract-version event (human-gated).
//
// Frozen-for-Phase-1 (DECISIONS #10): this file defines the tick/latch IEngine
// only. The free-transport variant (IFreeTransportEngine, Kuramoto — DECISIONS
// #8) and pitch-native output resolution (Torus — DECISIONS #9) are deliberately
// absent until each is separately human-gated.
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
    template <typename T>
    T get() const {
        T v{};
        std::memcpy(&v, bytes.data() + readPos, sizeof(T));
        readPos += sizeof(T);
        return v;
    }
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

} // namespace orrery
