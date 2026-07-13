// MidiOut.h — CoreMIDI virtual-source output (the Ableton routing path).
//
// Ableton Live cannot route a plugin's API-level MIDI output to other tracks
// (its "MIDI From" taps BEFORE the instrument). The documented workaround is a
// virtual MIDI bus: Orrery opens its own CoreMIDI virtual source, and another
// Live track receives it via MIDI From → "Orrery". Notes generated on the audio
// thread are pushed into an SPSC ring (no alloc/lock) and sent here off-thread.
//
// macOS/Linux only (JUCE createNewDevice creates a virtual endpoint there); on
// unsupported platforms the device is null and only the plugin-API bus carries
// MIDI. Reused for every host — Reaper/Bitwig also see the port; Live needs it.
#pragma once

#include <memory>

#include <juce_audio_devices/juce_audio_devices.h>

#include "Lockfree.h"

namespace orrery {

// A POD MIDI note event handed audio→drain.
struct MidiOutEvent {
    int16_t note     = 0;
    int16_t velocity = 0;
    int8_t  channel  = 1;   // 1..16
    int8_t  isOn     = 1;   // 1 = note-on, 0 = note-off
};

} // namespace orrery

// Owns the virtual device and a drain thread that forwards ring events to it.
class VirtualMidiOut : public juce::Thread {
public:
    VirtualMidiOut(orrery::SpscRing<orrery::MidiOutEvent, 512>* ring, juce::String portName);
    ~VirtualMidiOut() override;

    bool isOpen() const { return device_ != nullptr; }
    void run() override;

private:
    orrery::SpscRing<orrery::MidiOutEvent, 512>* ring_;
    juce::String                                 portName_;
    std::unique_ptr<juce::MidiOutput>            device_;
};
