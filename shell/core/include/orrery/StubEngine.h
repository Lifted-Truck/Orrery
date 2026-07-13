// StubEngine.h — the smallest real IEngine, to prove the slot (ROADMAP O1).
//
// Not a musical engine: k sources evenly spread across the lap, deterministic
// energies from the per-slot PCG32 stream. Its job is to exercise the seam —
// latch/tick, latchedEvents, gestures (Add/Remove with LIFO sourceId stability
// per §1.2), MIDI-in perturbation, save/load round-trip, and trace — so O1 can
// prove the contract without any engine physics. Elastic Euclid replaces it at
// O2.
#pragma once

#include "orrery/Contract.h"
#include "orrery/Trace.h"
#include "orrery/Types.h"

namespace orrery {

class StubEngine : public IEngine {
public:
    explicit StubEngine(int initialSources = 8);

    void tick(TickContext& ctx) override;
    std::span<const TriggerEvent> latchedEvents() const override;

    void handleGesture(const GestureEvent& g) override;
    void handleMidiIn(const MidiPerturbation& m) override;

    void saveState(Chunk& c) const override;
    void loadState(const Chunk& c) override;

    void writeTrace(TraceWriter& tw) const override;

    int sourceCount() const { return k_; }

private:
    int          k_          = 8;
    int64_t      generation_ = 0;
    TriggerEvent events_[kMaxSources];
    float        energyBias_[kMaxSources] = {0.0f};
};

} // namespace orrery
