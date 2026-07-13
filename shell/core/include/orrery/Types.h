// Types.h — shared value types for the Orrery studio core.
//
// Framework-free (no JUCE). These are the plain-old-data types that cross the
// IEngine seam and flow engine → offset layer → router. Contract source:
// sequencer-studio-architecture.md §1, §2, §3, §4.
#pragma once

#include <cstdint>

namespace orrery {

// Max sources any single engine may expose (contract §1.1: "max 32 sources per
// engine"). Fixed capacity → RT-safe, no allocation on the tick path.
inline constexpr int kMaxSources = 32;

// ── §1.1 TriggerEvent ───────────────────────────────────────────────────────
// One latched trigger emitted by an engine for the next lap. barPhase is the
// event's position WITHIN the lap (not the clock latch offset); the shell maps
// it to a sample-accurate time via the clock's lap window.
struct TriggerEvent {
    double  barPhase = 0.0;  // [0,1) position within the lap
    int32_t sourceId = 0;    // stable identity within this engine (§1.2)
    float   energy   = 0.0f; // [0,1] engine-defined intensity signal
};

// ── §3 Clock configuration ──────────────────────────────────────────────────
// Which musical division latches the engine (calls tick()).
enum class Division { Bar, Half, Step };

struct ClockConfig {
    double   barsPerLap = 1.0;              // 0.25 .. 4 (contract §3)
    Division division   = Division::Bar;    // BAR / HALF / STEP
    int      stepsPerBar = 16;              // used only when division == Step
};

// Host transport snapshot for one audio block. Supplied by the shell adapter
// (reads AudioPlayHead); the core never reads a wall clock.
struct TransportState {
    double sampleRate      = 48000.0;
    double bpm             = 120.0;
    double ppqAtBlockStart = 0.0;  // quarter-note position of the first sample
    int    blockSize       = 512;  // samples in this block
    int    timeSigNum      = 4;
    int    timeSigDen      = 4;
    bool   isPlaying       = true;
};

// A latch boundary located within the current block.
struct LatchPoint {
    int     sampleOffset = 0;  // [0, blockSize) — where in the block to tick
    int64_t index        = 0;  // monotonic latch/generation counter from ppq 0
    double  ppq          = 0.0;// absolute quarter-note position of the latch
};

// ── §2 Offset layer cell ─────────────────────────────────────────────────────
struct OffsetCell {
    int8_t transpose = 0;      // −24..+24 semitones
    int8_t velOffset = 0;      // −64..+64
    bool   lockT     = false;  // hand-edit lock (T) — generators skip when set
    bool   lockV     = false;  // hand-edit lock (V)
};

// ── §4 Router output ─────────────────────────────────────────────────────────
// A resolved MIDI note-on intent. The core produces this; the O1b adapter turns
// it into actual MIDI bytes at the scheduled sample offset.
struct MidiNote {
    int    channel     = 1;    // 1..16
    int    note        = 60;   // 0..127
    int    velocity    = 100;  // 1..127
    double barPhase    = 0.0;  // [0,1) scheduling phase (post quantizeOut)
    int    gateSamples = 0;    // note length in samples (0 = router default)
};

} // namespace orrery
