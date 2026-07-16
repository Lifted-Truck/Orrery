// Snapshot.h — the GUI's data contract with the audio thread.
//
// Two POD types, no JUCE: GuiSnapshot travels audio→GUI through a TripleBuffer
// (published once per block, read at timer rate); OffsetEdit travels GUI→audio
// through an SPSC ring (contract §5 — offset-cell edits use the same queue
// philosophy as gestures). The GUI NEVER touches engine or offset-layer state
// directly; it reads snapshots and sends edits. This is the whole isolation
// mechanism that keeps views mess-free and the audio thread race-free.
#pragma once

#include <cstdint>

#include "orrery/Types.h"

namespace orrery {

struct GuiSnapshot {
    // Engine view data (Elastic: particle phases/velocities; other engines map
    // their own state onto phase/energy-per-source).
    int32_t k = 0;                        // live sources
    int32_t n = 16;                       // lattice/grid divisions
    double  theta[kMaxSources] = {};      // per-source phase [0,1)
    double  omega[kMaxSources] = {};      // per-source velocity (whiskers)
    int64_t gen = 0;                      // generation counter

    // Offset layer (the pins-and-flow lane).
    OffsetCell cells[kMaxSources];

    // Transport (the header chip + playhead).
    double  bpm = 120.0;
    double  ppq = 0.0;
    int32_t timeSigNum = 4;
    int32_t timeSigDen = 4;
    bool    isPlaying = false;
    bool    hasHostTransport = false;   // false → internal clock (RUN chip shows)
};

// A hand edit on the offset lane, applied on the audio thread at block start.
// Transpose/VelOffset carry absolute values and SET the cell's lock (hand edits
// are pins); ResetCell clears value+locks; UnlockAll returns all cells to
// generator control.
struct OffsetEdit {
    enum class Type : int32_t { Transpose, VelOffset, ResetCell, UnlockAll };
    Type    type  = Type::Transpose;
    int32_t id    = 0;
    int32_t value = 0;
};

} // namespace orrery
