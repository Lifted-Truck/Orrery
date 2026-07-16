// MeasuredView.h — Measured Euclid's own visualization (engine-territory GUI).
//
// The prototype's two-lane language: a density lane (the SOUNDING measure,
// amber area fill; pending hand edits as a dimmed overlay) above an onset lane
// (grid ticks, amber onset dots, cyan flat-measure ghosts with deformation
// links). Interactive: DRAG in the density lane paints the curve (CurveEdit
// gesture per touched bin — latched to sounding at the next bar, spec §2.4);
// preset chips load FLAT/RISE/FALL/WAVES/BEATS/RAND. A cyan playhead sweeps
// while playing. Reads only GuiSnapshot; depends on the GUI seam + Theme +
// this engine's header (for kM). JUCE lives here (engine gui/ adapter zone).
#pragma once

#include "gui/IEngineView.h"
#include "gui/Theme.h"
#include "orrery/engines/MeasuredEuclid.h"

class MeasuredView : public IEngineView {
public:
    MeasuredView();

    void updateSnapshot(const orrery::GuiSnapshot& s) override { snap_ = s; repaint(); }
    juce::String engineName() const override { return "MEASURED"; }

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    struct Lane { float x0, x1, top, bot; };
    Lane densityLane() const;
    void paintAt(const juce::MouseEvent&);

    orrery::GuiSnapshot snap_;
    juce::TextButton presets_[6];
    int   lastBin_ = -1;
    float lastVal_ = 0.0f;
};
