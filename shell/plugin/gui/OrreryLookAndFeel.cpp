// OrreryLookAndFeel.cpp — token-driven widget rendering.
#include "gui/OrreryLookAndFeel.h"

#include "gui/Theme.h"

using namespace orrery;

OrreryLookAndFeel::OrreryLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, theme::bg);
    setColour(juce::Label::textColourId, theme::text);
    setColour(juce::Slider::textBoxTextColourId, theme::text);
    setColour(juce::TooltipWindow::backgroundColourId, theme::panel);
    setColour(juce::TooltipWindow::textColourId, theme::text);
    setColour(juce::TooltipWindow::outlineColourId, theme::line2);
}

void OrreryLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                                         float pos, float, float,
                                         juce::Slider::SliderStyle, juce::Slider& s) {
    const float cy = (float)y + (float)h * 0.5f;
    const float trackH = 4.0f;
    auto track = juce::Rectangle<float>((float)x, cy - trackH / 2, (float)w, trackH);

    g.setColour(theme::line);
    g.fillRoundedRectangle(track, trackH / 2);

    const float px = juce::jlimit((float)x, (float)(x + w), pos);
    // Cyan for meters (componentID "meter"), amber for parameters.
    const bool meter = s.getComponentID() == "meter";
    g.setColour((meter ? theme::cyan : theme::amber).withAlpha(0.85f));
    g.fillRoundedRectangle(track.withRight(px), trackH / 2);

    g.setColour(theme::panel);
    g.fillEllipse(px - 7.0f, cy - 7.0f, 14.0f, 14.0f);   // ring (panel gap)
    g.setColour(theme::text);
    g.fillEllipse(px - 5.5f, cy - 5.5f, 11.0f, 11.0f);   // thumb
}

void OrreryLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b,
                                             const juce::Colour&, bool highlighted, bool) {
    const bool chip = b.getComponentID().startsWith("chip");
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    if (!chip) { g.setColour(theme::panel); g.fillRoundedRectangle(r, theme::corner); return; }

    const bool cyanChip = b.getComponentID() == "chip-cyan";
    const auto accent = cyanChip ? theme::cyan : theme::amber;
    if (b.getToggleState()) {
        g.setColour(accent);
        g.fillRoundedRectangle(r, theme::corner);
    } else {
        g.setColour(highlighted ? theme::dim : theme::line2);
        g.drawRoundedRectangle(r, theme::corner, 1.0f);
    }
}

void OrreryLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool highlighted, bool) {
    g.setFont(theme::mono(10.0f).withExtraKerningFactor(0.06f));
    juce::Colour c = b.getToggleState() ? theme::bg
                   : highlighted        ? theme::text
                                        : theme::dim;
    if (!b.isEnabled()) c = theme::dim.withAlpha(0.45f);
    g.setColour(c);
    g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}
