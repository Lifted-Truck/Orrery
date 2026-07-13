// MidiRouter.h — the MIDI router mapping (sequencer-studio-architecture.md §4).
//
// Pure mapping only: (TriggerEvent + decorated OffsetCell) → MidiNote intent.
// Per-engine channel, note map (engineBaseNote per sourceId, §2.4), base
// velocity, gate length, and quantizeOut (blend emitted timing toward a grid —
// the physics/measure timing is never altered, only the output schedule). The
// actual MIDI byte emission and input-routing matrix are shell adapters (O1b);
// this stays framework-free and unit-testable.
#pragma once

#include "orrery/OffsetLayer.h"
#include "orrery/Types.h"

namespace orrery {

class MidiRouter {
public:
    MidiRouter();

    // Config (contract §4).
    int    outChannel      = 1;      // 1..16
    int    baseVelocity    = 100;    // pre-offset velocity
    int    gateSamples     = 0;      // 0 = host/adapter default
    float  quantizeOut     = 0.0f;   // 0..1 blend toward the quantize grid
    int    quantizeGrid    = 16;     // grid divisions per lap for quantizeOut

    // engineBaseNote per sourceId (§2.4). Default: stacked chromatic spread.
    void setNoteMapEntry(int32_t sourceId, int note);
    int  noteFor(int32_t sourceId) const { return noteMap_[sourceId]; }

    // Map one decorated trigger to a MIDI note-on intent. `cell` is the offset
    // cell for this event's sourceId (already composed by the offset layer).
    MidiNote route(const TriggerEvent& ev, const OffsetCell& cell) const;

    // Blend a lap-phase toward the quantize grid by `quantizeOut`. Exposed for
    // testing; route() applies it to the emitted barPhase.
    double quantizePhase(double barPhase) const;

private:
    int noteMap_[kMaxSources];
};

} // namespace orrery
