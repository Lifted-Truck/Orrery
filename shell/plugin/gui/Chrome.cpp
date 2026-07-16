// Chrome.cpp — shell frame rendering (header / tabs / routing strip).
#include "gui/Chrome.h"

#include <cmath>

using namespace orrery;

// ── HeaderBar ────────────────────────────────────────────────────────────────
HeaderBar::HeaderBar() {
    tick_.setComponentID("chip");
    tick_.onClick = [this] { if (onTick) onTick(); };
    addAndMakeVisible(tick_);
}

void HeaderBar::update(const GuiSnapshot& s) {
    bpm_ = s.bpm; sigN_ = s.timeSigNum; sigD_ = s.timeSigDen; playing_ = s.isPlaying;
    const double qPerBar = 4.0 * sigN_ / std::max(1, sigD_);
    bar_ = (int)std::floor(s.ppq / std::max(1e-9, qPerBar)) + 1;
    tick_.setVisible(!playing_);
    repaint();
}

void HeaderBar::resized() {
    tick_.setBounds(getWidth() - 160, (getHeight() - 22) / 2, 52, 22);
}

void HeaderBar::paint(juce::Graphics& g) {
    g.fillAll(theme::bg);
    g.setColour(theme::line);
    g.fillRect(0, getHeight() - 1, getWidth(), 1);

    // Wordmark: ORR E RY with the E in amber (prototype header).
    g.setFont(theme::sans(15.0f, true).withExtraKerningFactor(0.20f));
    int x = theme::pad;
    auto piece = [&](const juce::String& t, juce::Colour c) {
        g.setColour(c);
        const int w = (int)std::ceil(juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), t));
        g.drawText(t, x, 0, w + 2, getHeight(), juce::Justification::centredLeft);
        x += w;
    };
    piece("ORR", theme::text); piece("E", theme::amber); piece("RY", theme::text);

    g.setFont(theme::mono(10.5f).withExtraKerningFactor(0.04f));
    g.setColour(theme::dim);
    g.drawText("sequencer studio", x + 14, 0, 160, getHeight(), juce::Justification::centredLeft);

    // Transport chip, right-aligned. Pill last.
    const int pillW = 96, pillH = 22;
    const auto pill = juce::Rectangle<int>(getWidth() - theme::pad - pillW,
                                           (getHeight() - pillH) / 2, pillW, pillH);
    juce::String t = juce::String(bpm_, 1) + " BPM · " + juce::String(sigN_) + "/" + juce::String(sigD_)
                   + " · bar " + juce::String(bar_);
    g.setFont(theme::mono(11.0f));
    g.setColour(theme::text);
    g.drawText(t, pill.getX() - 250, 0, 242, getHeight(), juce::Justification::centredRight);

    if (playing_) {
        g.setColour(theme::cyan.withAlpha(0.35f));
        g.drawRoundedRectangle(pill.toFloat(), pillH / 2.0f, 1.0f);
        g.setColour(theme::cyan);
        g.fillEllipse((float)pill.getX() + 9, pill.getCentreY() - 3.0f, 6, 6);
        g.setFont(theme::label(10.0f));
        g.drawText("HOST SYNC", pill.withTrimmedLeft(20), juce::Justification::centred);
    } else {
        g.setColour(theme::dim);
        g.setFont(theme::label(10.0f));
        g.drawText("STOPPED", pill, juce::Justification::centred);
    }
}

// ── EngineTabs ───────────────────────────────────────────────────────────────
juce::Rectangle<int> EngineTabs::tabArea(int i) const {
    int x = 12;
    for (int j = 0; j < i; ++j)
        x += 40 + 14 * (tabs_[(size_t)j].name.length() + tabs_[(size_t)j].kicker.length()) / 2;
    const auto& t = tabs_[(size_t)i];
    return { x, 0, 40 + 14 * (t.name.length() + t.kicker.length()) / 2, getHeight() };
}

void EngineTabs::paint(juce::Graphics& g) {
    // Gradient panel→transparent, hairline below (mockup .tabs).
    g.setGradientFill(juce::ColourGradient(theme::panel, 0, 0, theme::bg, 0, (float)getHeight(), false));
    g.fillAll();
    g.setColour(theme::line);
    g.fillRect(0, getHeight() - 1, getWidth(), 1);

    for (int i = 0; i < (int)tabs_.size(); ++i) {
        const auto& t = tabs_[(size_t)i];
        const auto r = tabArea(i);
        const bool sel = i == selected;
        g.setFont(theme::label(11.0f));
        g.setColour(sel ? theme::amber : t.enabled ? theme::dim : theme::dim.withAlpha(0.4f));
        g.drawText(t.name, r.withTrimmedRight(r.getWidth() / 3), juce::Justification::centredLeft);
        g.setFont(theme::mono(9.0f));
        g.setColour(theme::dim.withAlpha(t.enabled ? 0.8f : 0.35f));
        g.drawText(t.kicker, r, juce::Justification::centredRight);
        if (sel) {
            g.setColour(theme::amber);
            g.fillRect(r.getX(), getHeight() - 2, r.getWidth() - 8, 2);
        }
    }
    g.setFont(theme::mono(10.0f));
    g.setColour(theme::dim);
    g.drawText("slot 1 / 1", getWidth() - 90, 0, 74, getHeight(), juce::Justification::centredRight);
}

void EngineTabs::mouseUp(const juce::MouseEvent& e) {
    for (int i = 0; i < (int)tabs_.size(); ++i)
        if (tabs_[(size_t)i].enabled && tabArea(i).contains(e.getPosition())) {
            selected = i;
            if (onSelect) onSelect(i);
            repaint();
            return;
        }
}

// ── RoutingBar ───────────────────────────────────────────────────────────────
RoutingBar::RoutingBar(juce::AudioProcessorValueTreeState& apvts,
                       std::function<bool()> portOpen, juce::String seedHex)
    : portOpen_(std::move(portOpen)), seedHex_(std::move(seedHex)) {
    audioChip_.setComponentID("chip");
    audioChip_.setClickingTogglesState(true);
    audioAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, "internalAudio", audioChip_);
    addAndMakeVisible(audioChip_);
}

void RoutingBar::resized() {
    audioChip_.setBounds(360, (getHeight() - 22) / 2, 58, 22);
}

void RoutingBar::paint(juce::Graphics& g) {
    g.fillAll(theme::bg);
    g.setColour(theme::line);
    g.fillRect(0, 0, getWidth(), 1);

    g.setFont(theme::mono(11.0f));
    const int cy = 0, h = getHeight();

    g.setColour(theme::dim);  g.drawText("OUT",  theme::pad,       cy, 34, h, juce::Justification::centredLeft);
    g.setColour(theme::text); g.drawText("ch 1", theme::pad + 36,  cy, 44, h, juce::Justification::centredLeft);

    const bool open = portOpen_ && portOpen_();
    g.setColour(open ? theme::cyan : theme::dim);
    g.fillEllipse(112.0f, h / 2.0f - 3.5f, 7, 7);
    g.drawText("Orrery", 126, cy, 52, h, juce::Justification::centredLeft);
    g.setColour(theme::dim);
    g.drawText(open ? "virtual port active" : "virtual port off", 182, cy, 150, h,
               juce::Justification::centredLeft);

    g.setColour(theme::dim);
    g.drawText("internal audio", 428, cy, 110, h, juce::Justification::centredLeft);

    g.setColour(theme::dim);
    g.drawText("seed " + seedHex_, getWidth() - 200, cy, 200 - theme::pad, h,
               juce::Justification::centredRight);
}
