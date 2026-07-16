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

#include "EngineSlot.h"
#include "Lockfree.h"
#include "MidiOut.h"
#include "Voices.h"
#include "gui/Snapshot.h"

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
    void pushOffsetEdit(const orrery::OffsetEdit& e) { offsetEdits_.push(e); }
    bool readSnapshot(orrery::GuiSnapshot& out) { return snapshots_.read(out); }
    void requestManualTick() { manualTicks_.fetch_add(1, std::memory_order_relaxed); }
    juce::AudioProcessorValueTreeState& apvts() { return apvts_; }
    bool virtualMidiOpen() const { return virtualMidi_ != nullptr && virtualMidi_->isOpen(); }
    uint64_t projectSeed() const { return projectSeed_; }
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
    // schedule=false → evolve/decorate/trace only (manual TICK while stopped —
    // spec §2.4: latched state is visible, but nothing is emitted into a
    // timeline that isn't advancing).
    void doLatch(int64_t gen, double lapPpq, const orrery::TransportState& ts,
                 bool schedule);
    void sweepPending(const orrery::TransportState& ts, juce::MidiBuffer&);
    // Drop all pending events, emitting note-offs for the pending offs so
    // downstream instruments aren't left with stuck notes.
    void clearPending(juce::MidiBuffer&);

    // ── Core (framework-free) ────────────────────────────────────────────────
    orrery::EngineSlot  slot_;       // hosts Elastic / Measured / Probable
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
        std::atomic<float>* run = nullptr;            // internal transport (no-host fallback)
        std::atomic<float>* voiceTune = nullptr;
        std::atomic<float>* voiceDecay = nullptr;
        std::atomic<float>* voiceTransient = nullptr;
        std::atomic<float>* voiceDrop = nullptr;
        std::atomic<float>* engineSelect = nullptr;
    } p_;
    orrery::SpscRing<orrery::GestureEvent, 256>    gestureRing_;
    orrery::SpscRing<orrery::OffsetEdit, 256>      offsetEdits_;   // GUI → offset layer
    orrery::SpscRing<orrery::TraceRecordPod, 256>  traceRing_;
    orrery::SpscRing<orrery::MidiOutEvent, 512>    midiOutRing_;   // audio → virtual port
    orrery::TripleBuffer<orrery::GuiSnapshot>      snapshots_;     // audio → GUI
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

    // Internal transport: the standalone (and any host that supplies no ppq)
    // has NO transport, so isPlaying would never be true and nothing would ever
    // play. When the host playhead yields no position, we free-run this clock
    // instead, gated by the `run` parameter. Hosts with real transport are
    // entirely unaffected. (Audio-thread only.)
    double internalPpq_      = 0.0;
    bool   internalRunning_  = false; // rising-edge detect for the RUN chip
    bool   hasHostTransport_ = false;
    double lastEndPpq_       = 0.0;   // discontinuity detection (loop/relocate)
    int    prevEngine_       = -1;    // engine-change detect → flush stale notes
    juce::AudioParameterChoice* engineParam_ = nullptr;  // read by index

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrreryProcessor)
};
