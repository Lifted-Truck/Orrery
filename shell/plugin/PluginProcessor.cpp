// PluginProcessor.cpp — the studio shell adapter chain.
#include "PluginProcessor.h"
#include "TraceDrain.h"

#if ORRERY_WITH_EDITOR
#include "gui/PluginEditor.h"
#endif

#include <cmath>

using namespace orrery;

namespace {
using P = juce::ParameterID;
constexpr double kEps = 1e-9;

// "rate" choices → (Division, stepsPerBar). The latch interval is the
// scheduling lap: engine barPhase spreads across one interval, and consecutive
// latches tile the timeline exactly (no overlap). Uses the tested core clock.
const juce::StringArray kRateNames { "1/16", "1/8", "1/4", "1/2 bar", "1 bar" };
void applyRate(int idx, ClockConfig& c) {
    switch (idx) {
        case 0: c.division = Division::Step; c.stepsPerBar = 16; break;
        case 1: c.division = Division::Step; c.stepsPerBar = 8;  break;
        case 2: c.division = Division::Step; c.stepsPerBar = 4;  break;
        case 3: c.division = Division::Half; break;
        default: c.division = Division::Bar; break;
    }
    c.barsPerLap = 1.0;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout OrreryProcessor::createLayout() {
    using FloatParam  = juce::AudioParameterFloat;
    using IntParam    = juce::AudioParameterInt;
    using BoolParam   = juce::AudioParameterBool;
    using ChoiceParam = juce::AudioParameterChoice;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(
        std::make_unique<FloatParam>(P{"gain", 1}, "Gain",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f),
        std::make_unique<IntParam>(P{"sources", 1}, "Sources", 1, 16, 8),
        std::make_unique<ChoiceParam>(P{"rate", 1}, "Rate", kRateNames, 4 /*1 bar*/),
        std::make_unique<FloatParam>(P{"gateMs", 1}, "Gate",
            juce::NormalisableRange<float>(5.0f, 500.0f, 1.0f), 60.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")),
        std::make_unique<FloatParam>(P{"quantizeOut", 1}, "Quantize",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f),
        std::make_unique<BoolParam>(P{"walkOn", 1}, "Walk", false),
        std::make_unique<IntParam>(P{"walkStep", 1}, "Walk Step", 0, 6, 1),
        std::make_unique<BoolParam>(P{"accentOn", 1}, "Accent", false),
        std::make_unique<IntParam>(P{"accentCount", 1}, "Accent Count", 0, 12, 3),
        // Elastic Euclid engine (spec §4).
        std::make_unique<IntParam>(P{"wells", 1}, "Lattice n", 1, 64, 16),
        std::make_unique<FloatParam>(P{"repulsion", 1}, "Repulsion",
            juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f),
        std::make_unique<FloatParam>(P{"lattice", 1}, "Lattice Pull",
            juce::NormalisableRange<float>(0.0f, 2.0f), 0.6f),
        std::make_unique<FloatParam>(P{"damping", 1}, "Damping",
            juce::NormalisableRange<float>(0.02f, 2.0f), 0.35f),
        std::make_unique<FloatParam>(P{"relax", 1}, "Relax",
            juce::NormalisableRange<float>(0.01f, 0.6f), 0.08f,
            juce::AudioParameterFloatAttributes().withLabel("s")),
        // Internal audio on/off: off → Orrery is a silent MIDI generator
        // (drive other tracks); on → it also sounds via the fallback voices.
        std::make_unique<BoolParam>(P{"internalAudio", 1}, "Internal Audio", true),
        // Internal transport (used ONLY when the host provides no ppq — the
        // standalone). Default on so the standalone plays out of the box.
        std::make_unique<BoolParam>(P{"run", 1}, "Run", true),
        // Onboard voice controls (VOICE rail section). tune is voice-only
        // monitoring pitch (default +12 = an octave up); MIDI-out is unchanged.
        std::make_unique<IntParam>(P{"voiceTune", 1}, "Tune", -24, 24, 12),
        std::make_unique<FloatParam>(P{"voiceDecay", 1}, "Decay",
            juce::NormalisableRange<float>(40.0f, 600.0f, 1.0f), 220.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")),
        std::make_unique<FloatParam>(P{"voiceTransient", 1}, "Transient",
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.45f),
        std::make_unique<FloatParam>(P{"voiceDrop", 1}, "Drop",
            juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 14.0f,
            juce::AudioParameterFloatAttributes().withLabel("st")));
    return layout;
}

OrreryProcessor::OrreryProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "ORRERY", createLayout()) {
    p_.gain        = apvts_.getRawParameterValue("gain");
    p_.sources     = apvts_.getRawParameterValue("sources");
    p_.rate        = apvts_.getRawParameterValue("rate");
    p_.gateMs      = apvts_.getRawParameterValue("gateMs");
    p_.quantizeOut = apvts_.getRawParameterValue("quantizeOut");
    p_.walkOn      = apvts_.getRawParameterValue("walkOn");
    p_.walkStep    = apvts_.getRawParameterValue("walkStep");
    p_.accentOn    = apvts_.getRawParameterValue("accentOn");
    p_.accentCount = apvts_.getRawParameterValue("accentCount");
    p_.wells       = apvts_.getRawParameterValue("wells");
    p_.repulsion   = apvts_.getRawParameterValue("repulsion");
    p_.lattice     = apvts_.getRawParameterValue("lattice");
    p_.damping     = apvts_.getRawParameterValue("damping");
    p_.relax       = apvts_.getRawParameterValue("relax");
    p_.internalAudio = apvts_.getRawParameterValue("internalAudio");
    p_.run           = apvts_.getRawParameterValue("run");
    p_.voiceTune      = apvts_.getRawParameterValue("voiceTune");
    p_.voiceDecay     = apvts_.getRawParameterValue("voiceDecay");
    p_.voiceTransient = apvts_.getRawParameterValue("voiceTransient");
    p_.voiceDrop      = apvts_.getRawParameterValue("voiceDrop");
}

OrreryProcessor::~OrreryProcessor() {
    // The host may destroy us without a releaseResources() first; a juce::Thread
    // must be stopped before deletion (juce_Thread.cpp assertion otherwise).
    if (drain_)       { drain_->signalThreadShouldExit(); drain_->stopThread(500); drain_.reset(); }
    if (virtualMidi_) { virtualMidi_->signalThreadShouldExit(); virtualMidi_->stopThread(500); virtualMidi_.reset(); }
}

void OrreryProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    sampleRate_ = sampleRate;
    slotRng_.seed(projectSeed_, 0);
    offset_.seed(projectSeed_, 0xFFFF);
    engine_.seed(projectSeed_, 1);   // slot 0's engine randomness stream
    voices_.prepare(sampleRate);
    pendingCount_ = 0;
    lastK_ = -1;
    manualGen_ = -1;
    internalPpq_ = 0.0;
    lastEndPpq_ = 0.0;
    if (enableTraceDrain_) {
        auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("orrery-trace.jsonl");
        drain_ = std::make_unique<TraceDrain>(&traceRing_, file);
        drain_->startThread();
    }
    // Open the CoreMIDI virtual source so hosts (esp. Ableton) can route
    // Orrery's generated MIDI to other tracks. Notes are pushed from the audio
    // thread into midiOutRing_ and sent here off-thread.
    if (enableVirtualMidi_ && virtualMidi_ == nullptr) {
        virtualMidi_ = std::make_unique<VirtualMidiOut>(&midiOutRing_, "Orrery");
        virtualMidi_->startThread();
    }
}

void OrreryProcessor::releaseResources() {
    if (drain_)       { drain_->signalThreadShouldExit(); drain_->stopThread(500); drain_.reset(); }
    if (virtualMidi_) { virtualMidi_->signalThreadShouldExit(); virtualMidi_->stopThread(500); virtualMidi_.reset(); }
}

bool OrreryProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void OrreryProcessor::applyParams() {
    ClockConfig cfg;
    applyRate(static_cast<int>(*p_.rate), cfg);
    clockCfg_ = cfg;

    router_.quantizeOut = *p_.quantizeOut;

    auto& w = offset_.walk();
    w.enabled = *p_.walkOn > 0.5f;
    w.targetT = true; w.targetV = false;
    w.step = static_cast<int>(*p_.walkStep);
    w.range = 12; w.rateBars = 1;

    auto& a = offset_.accent();
    a.enabled = *p_.accentOn > 0.5f;
    a.accents = static_cast<int>(*p_.accentCount);
    a.amount = 24;

    // Elastic Euclid engine parameters (spec §4).
    engine_.setN(static_cast<int>(*p_.wells));
    engine_.setRepulsion(*p_.repulsion);
    engine_.setLattice(*p_.lattice);
    engine_.setDamping(*p_.damping);
    engine_.setRelax(*p_.relax);

    // Onboard voice controls.
    voices_.setParams(VoiceParams{
        static_cast<int>(*p_.voiceTune),
        *p_.voiceDecay / 1000.0f,
        *p_.voiceTransient,
        *p_.voiceDrop});

    // "sources" param → Add/Remove gestures (bounded ≤32 iterations, no alloc).
    const int target = juce::jlimit(1, kMaxSources, static_cast<int>(*p_.sources));
    int cur = engine_.sourceCount();
    while (cur < target) { engine_.handleGesture({GestureEvent::Type::Add, 0, 0.0f}); ++cur; }
    while (cur > target) { engine_.handleGesture({GestureEvent::Type::Remove, 0, 0.0f}); --cur; }
}

void OrreryProcessor::doLatch(int64_t gen, double lapPpq, const TransportState& ts,
                              bool schedule) {
    TickContext ctx; ctx.generation = gen; ctx.clock = clockCfg_; ctx.tempoBpm = ts.bpm;
    ctx.rng = &slotRng_;
    engine_.tick(ctx);
    genViz_.store(gen, std::memory_order_relaxed);

    const int k = engine_.sourceCount();
    int32_t present[kMaxSources];
    for (int i = 0; i < k; ++i) present[i] = i;
    offset_.runGenerators(gen, std::span<const int32_t>(present, static_cast<size_t>(k)));

    // Hand a POD trace record to the drain thread (no formatting here).
    auto ev = engine_.latchedEvents();
    TraceRecordPod pod; pod.gen = gen; pod.count = static_cast<int32_t>(ev.size());
    for (size_t i = 0; i < ev.size() && i < kMaxSources; ++i) pod.ev[i] = ev[i];
    traceRing_.push(pod);

    if (!schedule) return;   // manual TICK while stopped: evolve silently

    const double lapLen = clockmath::latchIntervalQuarters(clockCfg_, ts);
    double gateQ = gateQuarters_;
    if (gateQ > lapLen * 0.95) gateQ = lapLen * 0.95;

    for (const auto& e : ev) {
        const MidiNote note = router_.route(e, offset_.cell(e.sourceId));
        const double ppqOn  = lapPpq + note.barPhase * lapLen;
        const double ppqOff = ppqOn + gateQ;
        if (pendingCount_ < kMaxPending)
            pending_[pendingCount_++] = {ppqOn,  true,  note.channel, note.note, note.velocity};
        if (pendingCount_ < kMaxPending)
            pending_[pendingCount_++] = {ppqOff, false, note.channel, note.note, note.velocity};
    }
}

void OrreryProcessor::clearPending(juce::MidiBuffer& midi) {
    // Emit the pending note-OFFS immediately (their note-ons already went out;
    // dropping them would leave stuck notes on whatever the MIDI drives).
    for (int i = 0; i < pendingCount_; ++i) {
        const SchedEvent& e = pending_[i];
        if (e.isOn) continue;
        midi.addEvent(juce::MidiMessage::noteOff(e.channel, e.note), 0);
        midiOutRing_.push(orrery::MidiOutEvent{
            static_cast<int16_t>(e.note), static_cast<int16_t>(e.velocity),
            static_cast<int8_t>(e.channel), 0});
    }
    pendingCount_ = 0;
}

void OrreryProcessor::sweepPending(const TransportState& ts, juce::MidiBuffer& midi) {
    const double spq = clockmath::samplesPerQuarter(ts);
    const double startPpq = ts.ppqAtBlockStart;
    const double endPpq = startPpq + static_cast<double>(ts.blockSize) / spq;
    int w = 0;
    for (int i = 0; i < pendingCount_; ++i) {
        SchedEvent& e = pending_[i];
        if (e.ppq < endPpq - kEps) {
            int off = static_cast<int>(std::llround((e.ppq - startPpq) * spq));
            off = juce::jlimit(0, ts.blockSize - 1, off);
            if (e.isOn) {
                midi.addEvent(juce::MidiMessage::noteOn(e.channel, e.note,
                                  static_cast<juce::uint8>(e.velocity)), off);
                voices_.noteOn(e.note, e.velocity);
            } else {
                midi.addEvent(juce::MidiMessage::noteOff(e.channel, e.note), off);
            }
            // Mirror to the CoreMIDI virtual port (Ableton routing path). SPSC
            // push only — no alloc/lock; the drain thread does the CoreMIDI send.
            midiOutRing_.push(orrery::MidiOutEvent{
                static_cast<int16_t>(e.note), static_cast<int16_t>(e.velocity),
                static_cast<int8_t>(e.channel), static_cast<int8_t>(e.isOn ? 1 : 0)});
        } else {
            pending_[w++] = e;  // still in the future — keep
        }
    }
    pendingCount_ = w;
}

void OrreryProcessor::renderBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi,
                                  const TransportState& ts) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, numSamples);

    applyParams();
    gateQuarters_ = (*p_.gateMs / 1000.0) * (ts.bpm / 60.0);

    // GUI/test gestures (SPSC, drained at block start).
    GestureEvent g;
    while (gestureRing_.pop(g)) engine_.handleGesture(g);

    // Offset-cell hand edits (contract §5 — same queue philosophy). Set/reset
    // pin the cell; generators flow around pins at the next bar.
    OffsetEdit oe;
    while (offsetEdits_.pop(oe)) {
        switch (oe.type) {
            case OffsetEdit::Type::Transpose: offset_.setTranspose(oe.id, oe.value); break;
            case OffsetEdit::Type::VelOffset: offset_.setVelOffset(oe.id, oe.value); break;
            case OffsetEdit::Type::ResetCell: offset_.resetCell(oe.id); break;
            case OffsetEdit::Type::UnlockAll: offset_.unlockAll(); break;
        }
    }

    // MIDI in → engine perturbation, then repurpose the buffer for our output.
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        if (m.isNoteOn()) {
            MidiPerturbation pert;
            pert.channel = m.getChannel(); pert.note = m.getNoteNumber();
            pert.velocity = m.getVelocity(); pert.amount = 1.0f;
            engine_.handleMidiIn(pert);
        }
    }
    midi.clear();
    midi.ensureSize(2048);

    if (ts.isPlaying) {
        // Transport discontinuity (loop wrap / relocate / start): scheduled
        // events belong to the old timeline — drop them (offs flushed) rather
        // than fire them as a burst at the new position.
        const double blockQ = static_cast<double>(ts.blockSize) / clockmath::samplesPerQuarter(ts);
        if (std::abs(ts.ppqAtBlockStart - lastEndPpq_) > 0.26 && pendingCount_ > 0)
            clearPending(midi);
        lastEndPpq_ = ts.ppqAtBlockStart + blockQ;

        LatchPoint lp[64];
        const int nl = clockmath::computeLatches(clockCfg_, ts, lp, 64);
        for (int i = 0; i < nl; ++i) doLatch(lp[i].index, lp[i].ppq, ts, /*schedule=*/true);
    } else {
        // Stopped: nothing scheduled may survive (a stalled timeline never
        // drains — pending would clog and then burst on play). Manual TICK
        // evolves the pattern silently (spec §2.4 latch semantics).
        if (pendingCount_ > 0) clearPending(midi);
        lastEndPpq_ = ts.ppqAtBlockStart;
        int mt = manualTicks_.exchange(0, std::memory_order_relaxed);
        for (int i = 0; i < mt; ++i) { ++manualGen_; doLatch(manualGen_, ts.ppqAtBlockStart, ts, /*schedule=*/false); }
    }

    sweepPending(ts, midi);
    // Internal audio toggle: when off, Orrery is a silent MIDI generator (MIDI
    // still flows on the plugin-API bus + the virtual port).
    if (p_.internalAudio->load(std::memory_order_relaxed) > 0.5f)
        voices_.render(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), numSamples,
                       p_.gain->load(std::memory_order_relaxed));

    kViz_.store(engine_.sourceCount(), std::memory_order_relaxed);

    // Publish the GUI snapshot (wait-free; fixed copies only — no alloc).
    {
        GuiSnapshot& s = snapshots_.writeSlot();
        s.k = engine_.sourceCount();
        s.n = engine_.latticeWells();
        for (int i = 0; i < s.k; ++i) { s.theta[i] = engine_.theta(i); s.omega[i] = engine_.omega(i); }
        for (int i = 0; i < kMaxSources; ++i) s.cells[i] = offset_.cell(i);
        s.gen = engine_.generation();
        s.bpm = ts.bpm; s.ppq = ts.ppqAtBlockStart;
        s.timeSigNum = ts.timeSigNum; s.timeSigDen = ts.timeSigDen;
        s.isPlaying = ts.isPlaying;
        s.hasHostTransport = hasHostTransport_;
        snapshots_.publish();
    }
}

void OrreryProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    TransportState ts;
    ts.sampleRate = sampleRate_;
    ts.blockSize = buffer.getNumSamples();
    ts.isPlaying = false;
    ts.ppqAtBlockStart = 0.0;

    // Host transport requires an actual ppq position — a playhead alone isn't
    // enough. The JUCE standalone (and some bridges) provide none, and without
    // this fallback the plugin can NEVER play there: isPlaying stays false and
    // no latch ever fires.
    bool hostTransport = false;
    if (auto* ph = getPlayHead()) {
        if (const auto pos = ph->getPosition()) {
            if (const auto ppq = pos->getPpqPosition()) {
                hostTransport = true;
                ts.ppqAtBlockStart = *ppq;
                ts.isPlaying = pos->getIsPlaying();
                if (const auto bpm = pos->getBpm()) ts.bpm = *bpm;
                if (const auto sig = pos->getTimeSignature()) {
                    ts.timeSigNum = sig->numerator; ts.timeSigDen = sig->denominator;
                }
            }
        }
    }
    if (!hostTransport) {
        // Internal free-run clock, gated by the `run` param (header RUN chip).
        ts.isPlaying = p_.run->load(std::memory_order_relaxed) > 0.5f;
        ts.ppqAtBlockStart = internalPpq_;
        if (ts.isPlaying)
            internalPpq_ += static_cast<double>(ts.blockSize) / clockmath::samplesPerQuarter(ts);
    }
    hasHostTransport_ = hostTransport;
    renderBlock(buffer, midi, ts);
}

juce::AudioProcessorEditor* OrreryProcessor::createEditor() {
#if ORRERY_WITH_EDITOR
    return new OrreryEditor(*this);   // the shell chrome + active IEngineView
#else
    return nullptr;                    // headless builds (RT test)
#endif
}

void OrreryProcessor::getStateInformation(juce::MemoryBlock& dest) {
    Chunk eng; engine_.saveState(eng);
    Chunk off; offset_.saveCells(off);
    juce::ValueTree vt("ORRERY_STATE");
    vt.setProperty("seed", juce::int64(projectSeed_), nullptr);
    vt.setProperty("apvts", apvts_.copyState().toXmlString(), nullptr);
    vt.setProperty("engine", juce::var(juce::MemoryBlock(eng.bytes.data(), eng.bytes.size())), nullptr);
    vt.setProperty("offset", juce::var(juce::MemoryBlock(off.bytes.data(), off.bytes.size())), nullptr);
    juce::MemoryOutputStream mos(dest, false);
    vt.writeToStream(mos);
}

void OrreryProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto vt = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (!vt.isValid()) return;
    projectSeed_ = static_cast<uint64_t>(static_cast<juce::int64>(vt.getProperty("seed")));
    if (auto xml = juce::parseXML(vt.getProperty("apvts").toString()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
    if (const auto* mb = vt.getProperty("engine").getBinaryData()) {
        Chunk c; c.bytes.assign(static_cast<const uint8_t*>(mb->getData()),
                                static_cast<const uint8_t*>(mb->getData()) + mb->getSize());
        engine_.loadState(c);
    }
    if (const auto* mb = vt.getProperty("offset").getBinaryData()) {
        Chunk c; c.bytes.assign(static_cast<const uint8_t*>(mb->getData()),
                                static_cast<const uint8_t*>(mb->getData()) + mb->getSize());
        offset_.loadCells(c);
    }
    // Reseed streams from the restored seed (rng stream position is not part of
    // saved state in O1b — deterministic from this reset point).
    slotRng_.seed(projectSeed_, 0);
    offset_.seed(projectSeed_, 0xFFFF);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new OrreryProcessor();
}
