// EngineSlot.h — hosts the three engines behind one façade (the multi-engine
// slot). Only the shell knows the concrete engine set (it instantiates them);
// engines stay independent of each other and of the shell — the IEngine seam is
// for THEIR isolation, not the reverse. This class localizes the per-engine
// branching (params, snapshot) that the processor would otherwise scatter.
//
// Selection is driven by the `engine` APVTS param. Each engine keeps its own
// state, so switching preserves what each was doing. The offset layer + router
// remain shell-shared for now (per-engine offset state is a later refinement).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "gui/Snapshot.h"
#include "orrery/Contract.h"
#include "orrery/engines/ElasticEuclid.h"
#include "orrery/engines/MeasuredEuclid.h"
#include "orrery/engines/ProbableEuclid.h"

namespace orrery {

class EngineSlot {
public:
    void seed(uint64_t projectSeed) {
        elastic_.seed(projectSeed, 1);
        measured_.seed(projectSeed, 2);
        probable_.seed(projectSeed, 3);
    }

    void select(EngineKind k) { kind_ = k; }
    EngineKind kind() const { return kind_; }
    IEngine& active() {
        switch (kind_) {
            case EngineKind::Measured: return measured_;
            case EngineKind::Probable: return probable_;
            default:                   return elastic_;
        }
    }

    // Addressable source count for the offset layer (Elastic/Measured: live
    // particles/onsets; Probable: the n grid steps).
    int sourceCount() const {
        switch (kind_) {
            case EngineKind::Measured: return measured_.sourceCount();
            case EngineKind::Probable: return probable_.sourceCount();
            default:                   return elastic_.sourceCount();
        }
    }

    // Apply the active engine's parameter subset from the APVTS.
    void applyParams(juce::AudioProcessorValueTreeState& p);

    // Publish the active engine's view state into the snapshot.
    void fillSnapshot(GuiSnapshot& s) const;

    // Per-engine state (each engine's loadState rewinds its own chunk, so the
    // three are kept separate). All three persist so switching keeps their work.
    void saveState(Chunk& e, Chunk& m, Chunk& p) const {
        elastic_.saveState(e); measured_.saveState(m); probable_.saveState(p);
    }
    void loadState(const Chunk& e, const Chunk& m, const Chunk& p) {
        elastic_.loadState(e); measured_.loadState(m); probable_.loadState(p);
    }

private:
    ElasticEuclid  elastic_;
    MeasuredEuclid measured_;
    ProbableEuclid probable_;
    EngineKind     kind_ = EngineKind::Elastic;
};

} // namespace orrery
