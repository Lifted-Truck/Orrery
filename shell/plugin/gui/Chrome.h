// Chrome.h — the shared shell frame: header (wordmark + host-transport chip),
// engine tabs, and the routing strip. Engine-agnostic: everything here reads
// GuiSnapshot or APVTS; nothing knows any engine's internals.
#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>

#include "gui/Snapshot.h"
#include "gui/Theme.h"

// ── Header: ORRERY wordmark · subtitle · BPM/sig/bar · RUN / TICK / pill ─────
class HeaderBar : public juce::Component {
public:
    HeaderBar();
    // Binds the RUN chip to the `run` parameter (internal-transport fallback —
    // shown only when the host provides no transport, e.g. the standalone).
    void bindRun(juce::AudioProcessorValueTreeState&);
    void update(const orrery::GuiSnapshot&);
    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void()> onTick;   // manual TICK while stopped (contract §3)

private:
    juce::TextButton tick_ { "TICK" }, run_ { "RUN" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> runAttach_;
    double bpm_ = 120.0;
    int sigN_ = 4, sigD_ = 4, bar_ = 1;
    bool playing_ = false, hasHost_ = false;
};

// ── Engine tabs: one per registered view; disabled = slot not yet hostable ───
class EngineTabs : public juce::Component {
public:
    struct Tab { juce::String name, kicker; bool enabled = false; };
    void setTabs(std::vector<Tab> tabs) { tabs_ = std::move(tabs); repaint(); }
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

    int selected = 0;
    std::function<void(int)> onSelect;

private:
    juce::Rectangle<int> tabArea(int i) const;
    std::vector<Tab> tabs_;
};

// ── Routing strip: OUT ch · virtual-port status · internal audio · seed ──────
class RoutingBar : public juce::Component {
public:
    // portOpen/seed are polled via callables so this stays processor-agnostic.
    RoutingBar(juce::AudioProcessorValueTreeState& apvts,
               std::function<bool()> portOpen, juce::String seedHex);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::TextButton audioChip_ { "AUDIO" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> audioAttach_;
    std::function<bool()> portOpen_;
    juce::String seedHex_;
};
