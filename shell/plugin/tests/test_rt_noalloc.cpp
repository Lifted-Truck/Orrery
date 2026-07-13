// test_rt_noalloc.cpp — the O1b RT-safety gate.
//
// Drives the processor's renderBlock() in steady state under a thread-local
// allocation hook and asserts ZERO heap allocations on the audio path. This is
// the enforceable version of the core's by-construction claim: with a real
// audio callback to instrument, prove no alloc/lock/log on the tick path.
//
// The hook is thread_local so the (disabled here) trace-drain thread could not
// contaminate the count even if running. Warm-up runs first so one-time lazy
// growth (MidiBuffer capacity, etc.) is excluded from the steady-state measure.
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

#include "PluginProcessor.h"

static thread_local long  tlAllocs = 0;
static thread_local bool  tlGuard  = false;

void* operator new(std::size_t n) {
    if (tlGuard) ++tlAllocs;
    if (n == 0) n = 1;
    void* p = std::malloc(n);
    if (!p) throw std::bad_alloc();
    return p;
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void  operator delete(void* p) noexcept { std::free(p); }
void  operator delete[](void* p) noexcept { std::free(p); }
void  operator delete(void* p, std::size_t) noexcept { std::free(p); }
void  operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main() {
    // Bring up a message manager so JUCE tears down cleanly in this console app
    // (otherwise the leak detector / shutdown singleton assert on exit).
    juce::ScopedJuceInitialiser_GUI juceInit;

    const double sr = 48000.0;
    const int    block = 512;

    OrreryProcessor proc;
    proc.setTraceDrainEnabled(false);   // no file IO / no second thread
    proc.prepareToPlay(sr, block);

    juce::AudioBuffer<float> buf(2, block);
    juce::MidiBuffer midi;
    midi.ensureSize(8192);

    const double spq = sr * 60.0 / 128.0;
    const double blockQ = block / spq;
    double ppq = 0.0;

    auto makeTs = [&](double p) {
        orrery::TransportState ts;
        ts.sampleRate = sr; ts.bpm = 128.0; ts.blockSize = block;
        ts.ppqAtBlockStart = p; ts.isPlaying = true;
        ts.timeSigNum = 4; ts.timeSigDen = 4;
        return ts;
    };

    // Warm up (excluded from the measure) and confirm the pipeline emits MIDI —
    // so a zero-alloc result can't be an artifact of nothing happening.
    long noteOns = 0;
    for (int i = 0; i < 300; ++i) {
        midi.clear();
        proc.renderBlock(buf, midi, makeTs(ppq));
        for (const auto meta : midi)
            if (meta.getMessage().isNoteOn()) ++noteOns;
        ppq += blockQ;
    }

    int failures = 0;
    if (noteOns <= 0) { std::printf("FAIL: pipeline produced no note-ons\n"); ++failures; }
    else              { std::printf("ok: pipeline emitted %ld note-ons in warm-up\n", noteOns); }

    // Measure: 4000 steady-state blocks, zero allocations expected.
    tlAllocs = 0;
    tlGuard = true;
    for (int i = 0; i < 4000; ++i) {
        midi.clear();
        proc.renderBlock(buf, midi, makeTs(ppq));
        ppq += blockQ;
    }
    tlGuard = false;

    if (tlAllocs != 0) {
        std::printf("FAIL: %ld heap allocation(s) on the audio path\n", tlAllocs);
        ++failures;
    } else {
        std::printf("ok: 0 allocations across 4000 renderBlocks\n");
    }

    proc.releaseResources();
    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("RT no-alloc gate: PASS\n");
    return 0;
}
