// TraceDrain.h — off-thread JSONL trace writer (contract §6).
//
// Pops POD trace records the audio thread pushed into the SPSC ring and formats
// them to disk via the core trace serializer. All allocation / file IO happens
// here, never on the audio thread.
#pragma once

#include <memory>

#include <juce_core/juce_core.h>

#include "Lockfree.h"
#include "PluginProcessor.h"  // orrery::TraceRecordPod

class TraceDrain : public juce::Thread {
public:
    TraceDrain(orrery::SpscRing<orrery::TraceRecordPod, 256>* ring, juce::File file);
    void run() override;

private:
    void drainOnce();
    orrery::SpscRing<orrery::TraceRecordPod, 256>* ring_;
    juce::File                               file_;
    std::unique_ptr<juce::FileOutputStream>  os_;
};
