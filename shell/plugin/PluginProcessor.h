// PluginProcessor.h — the Orrery studio shell (JUCE lives here and only here).
//
// Wraps orrery_core: adapts the host (AudioPlayHead, MIDI, APVTS) to the
// framework-free contract, drives the clock→engine→offset→router chain, emits
// MIDI + fallback voices, and streams a trace off-thread. The RT-critical work
// is in renderBlock(), which is transport-injected so a headless test can drive
// it under an allocation hook (the O1b no-alloc gate).
#pragma once

#include <atomic>
#include <memory>

#include <juce_audio_utils/juce_audio_utils.h>

#include "orrery/Clock.h"
#include "orrery/MidiRouter.h"
#include "orrery/OffsetLayer.h"
#include "orrery/Types.h"
#include "orrery/engines/ElasticEuclid.h"

#include "Lockfree.h"
#include "MidiOut.h"
#include "Voices.h"

namespace orrery {

// A POD trace record handed audio→drain (the audio thread never formats JSON).
struct TraceRecordPod {
    int64_t      gen   = 0;
    int32_t      count = 0;
    TriggerEvent ev[kMaxSources];
};

} // namespace orrery

class TraceDrain;  // background JSONL writer (TraceDrain.cpp)

class OrreryProcessor final : public juce::AudioProcessor {
public:
    OrreryProcessor();
    ~OrreryProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // The RT-critical inner loop, transport-injected for headless testing.
    void renderBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&,
                     const orrery::TransportState&);

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Orrery"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    bool isBusesLayoutSupported(const BusesLayout&) const override;

    // GUI/test affordances (message thread).
    void pushGesture(const orrery::GestureEvent& g) { gestureRing_.push(g); }
    void requestManualTick() { manualTicks_.fetch_add(1, std::memory_order_relaxed); }
    // Headless tests disable the off-thread trace drain (no file IO / no second
    // allocating thread) before prepareToPlay.
    void setTraceDrainEnabled(bool on) { enableTraceDrain_ = on; }
    // Headless tests also skip opening a real CoreMIDI virtual port.
    void setVirtualMidiEnabled(bool on) { enableVirtualMidi_ = on; }
    int64_t vizGeneration() const { return genViz_.load(std::memory_order_relaxed); }
    int vizSources() const { return kViz_.load(std::memory_order_relaxed); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applyParams();                       // APVTS → core config (audio thread)
    void doLatch(int64_t gen, double lapPpq,
                 const orrery::TransportState& ts, juce::MidiBuffer&, int blockOff);
    void sweepPending(const orrery::TransportState& ts, juce::MidiBuffer&);

    // ── Core (framework-free) ────────────────────────────────────────────────
    orrery::ElasticEuclid engine_;   // O2: the equilibrium-rhythm engine
    orrery::OffsetLayer offset_;
    orrery::MidiRouter  router_;
    orrery::ClockConfig clockCfg_;
    orrery::Pcg32       slotRng_;
    orrery::VoiceBank   voices_;
    uint64_t            projectSeed_ = 0xACE0FBA5EULL;  // default project seed

    // ── Mini-scheduler: lap-phase events → sample-accurate note on/off ───────
    struct SchedEvent { double ppq; bool isOn; int channel; int note; int velocity; };
    static constexpr int kMaxPending = 512;
    SchedEvent pending_[kMaxPending];
    int        pendingCount_ = 0;

    // ── Adapters ─────────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts_;
    struct Params {  // cached raw pointers — audio-thread reads, no map lookup
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* sources = nullptr;
        std::atomic<float>* rate = nullptr;
        std::atomic<float>* gateMs = nullptr;
        std::atomic<float>* quantizeOut = nullptr;
        std::atomic<float>* walkOn = nullptr;
        std::atomic<float>* walkStep = nullptr;
        std::atomic<float>* accentOn = nullptr;
        std::atomic<float>* accentCount = nullptr;
        // Elastic Euclid engine params (spec §4).
        std::atomic<float>* wells = nullptr;
        std::atomic<float>* repulsion = nullptr;
        std::atomic<float>* lattice = nullptr;
        std::atomic<float>* damping = nullptr;
        std::atomic<float>* relax = nullptr;
        std::atomic<float>* internalAudio = nullptr;  // gate the fallback voices
    } p_;
    orrery::SpscRing<orrery::GestureEvent, 256>    gestureRing_;
    orrery::SpscRing<orrery::TraceRecordPod, 256>  traceRing_;
    orrery::SpscRing<orrery::MidiOutEvent, 512>    midiOutRing_;   // audio → virtual port
    std::unique_ptr<TraceDrain>    drain_;
    std::unique_ptr<VirtualMidiOut> virtualMidi_;

    double  sampleRate_ = 48000.0;
    int     lastK_      = -1;         // for the "sources" param → gesture diff
    double  gateQuarters_ = 0.1;
    std::atomic<int>     manualTicks_{0};
    std::atomic<int64_t> genViz_{0};
    std::atomic<int>     kViz_{8};
    int64_t manualGen_ = -1;
    bool    enableTraceDrain_ = true;
    bool    enableVirtualMidi_ = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrreryProcessor)
};
