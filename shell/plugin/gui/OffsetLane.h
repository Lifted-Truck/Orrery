// OffsetLane.h — the offset-layer lane (contract §2's coexistence UI).
//
// One row per live sourceId: transpose (drag vertically) and velocity offset
// (second sub-row). Hand-editing a cell PINS it (lock); generators flow around
// pins — locked cells render amber with a pin mark. Double-click resets a cell;
// UNLOCK ALL returns everything to generator control. Edits travel to the audio
// thread as OffsetEdit records via the shell (never a direct mutation).
// Generator chips (walk/accent) bind to their APVTS parameters; contour is
// shown disabled until O5 ships it.
#pragma once

#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "gui/Snapshot.h"
#include "gui/Theme.h"

class OffsetLane : public juce::Component {
public:
    explicit OffsetLane(juce::AudioProcessorValueTreeState& apvts);

    void update(const orrery::GuiSnapshot&);
    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    std::function<void(const orrery::OffsetEdit&)> onEdit;

private:
    juce::Rectangle<int> cellRect(int row, int i) const;  // row 0 = T, 1 = V
    int cellAt(const juce::Point<int>&, int& rowOut) const;

    orrery::GuiSnapshot snap_;

    juce::TextButton walk_ { "WALK" }, accent_ { "ACCENT" },
                     contour_ { "CONTOUR" }, unlockAll_ { "UNLOCK ALL" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> walkAttach_, accentAttach_;

    // Drag state.
    int  dragRow_ = -1, dragId_ = -1, dragStart_ = 0, dragValue_ = 0;
};
