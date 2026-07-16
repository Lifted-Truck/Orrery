// ParamRail.h — the right rail: engine title + label/slider/value rows, all
// bound to APVTS parameters (host automation stays live through attachments).
// Engine-agnostic: it's built from a row spec, so a different engine view can
// present a different row set without new widget code.
#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "gui/Theme.h"

class ParamRail : public juce::Component {
public:
    struct Row {
        juce::String paramId;    // "" = divider
        juce::String label;
        juce::String suffix;     // e.g. "s", "ms"
        int decimals = 2;        // -1 = integer
    };

    ParamRail(juce::AudioProcessorValueTreeState& apvts,
              juce::String engineTitle, std::vector<Row> rows);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Bound {
        Row row;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label>  value;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    void refreshValue(Bound&);

    juce::String title_;
    std::vector<Bound> rows_;
};
