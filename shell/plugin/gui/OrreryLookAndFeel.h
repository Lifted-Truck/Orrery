// OrreryLookAndFeel.h — the one LookAndFeel, drawing from Theme tokens only.
//
// Sliders render as the mockup's param rows (4px track, amber fill, ringed
// thumb). TextButtons with componentID "chip" / "chip-cyan" render as the
// mockup's generator/routing chips (toggled = filled accent).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class OrreryLookAndFeel : public juce::LookAndFeel_V4 {
public:
    OrreryLookAndFeel();

    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h,
                          float pos, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&,
                        bool highlighted, bool down) override;
};
