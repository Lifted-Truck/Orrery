// MidiRouter.cpp — implementation of the §4 router mapping.
#include "orrery/MidiRouter.h"

#include <cmath>

namespace orrery {

MidiRouter::MidiRouter() {
    // Default note map (spec §4): 36 + 3·(id mod 5) — a minor-third spread from
    // C2 cycling every 5 sources, matching the prototype's pitched voice. More
    // musical than a chromatic cluster, and it drives external instruments the
    // same way it sounds onboard.
    for (int i = 0; i < kMaxSources; ++i) noteMap_[i] = 36 + 3 * (i % 5);
}

void MidiRouter::setNoteMapEntry(int32_t sourceId, int note) {
    if (note < 0) note = 0;
    if (note > 127) note = 127;
    noteMap_[sourceId] = note;
}

double MidiRouter::quantizePhase(double barPhase) const {
    if (quantizeOut <= 0.0f || quantizeGrid <= 0) return barPhase;
    const double n = static_cast<double>(quantizeGrid);
    const double snapped = std::round(barPhase * n) / n;
    const double amt = quantizeOut > 1.0f ? 1.0 : static_cast<double>(quantizeOut);
    double out = barPhase + (snapped - barPhase) * amt;
    // Keep the emitted phase within the lap window [0,1).
    if (out < 0.0) out = 0.0;
    if (out >= 1.0) out = std::nextafter(1.0, 0.0);
    return out;
}

MidiNote MidiRouter::route(const TriggerEvent& ev, const OffsetCell& cell) const {
    int pitch = 0, vel = 0;
    OffsetLayer::resolve(cell, noteMap_[ev.sourceId], baseVelocity, pitch, vel);
    pitch = pitch + globalTranspose;
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;

    MidiNote n;
    n.channel     = outChannel;
    n.note        = pitch;
    n.velocity    = vel;
    n.barPhase    = quantizePhase(ev.barPhase);
    n.gateSamples = gateSamples;
    return n;
}

} // namespace orrery
