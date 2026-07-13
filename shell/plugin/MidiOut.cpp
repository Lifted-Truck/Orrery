// MidiOut.cpp — virtual CoreMIDI source + off-thread note forwarding.
#include "MidiOut.h"

using namespace orrery;

VirtualMidiOut::VirtualMidiOut(SpscRing<MidiOutEvent, 512>* ring, juce::String portName)
    : juce::Thread("orrery-midi-out"), ring_(ring), portName_(std::move(portName)) {
    // Create the virtual source on the message thread (before the drain runs).
    // Null on platforms/hosts that disallow it — the plugin-API bus still works.
    device_ = juce::MidiOutput::createNewDevice(portName_);
}

VirtualMidiOut::~VirtualMidiOut() = default;

void VirtualMidiOut::run() {
    while (!threadShouldExit()) {
        MidiOutEvent e;
        while (ring_->pop(e)) {
            if (!device_) continue;
            const auto msg = e.isOn
                ? juce::MidiMessage::noteOn(e.channel, e.note, static_cast<juce::uint8>(e.velocity))
                : juce::MidiMessage::noteOff(e.channel, e.note);
            device_->sendMessageNow(msg);
        }
        wait(1);  // ~1 ms latency; keeps the audio thread free of CoreMIDI calls
    }
}
