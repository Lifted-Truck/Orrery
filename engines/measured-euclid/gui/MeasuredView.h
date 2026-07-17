// MeasuredView.h — Measured Euclid's own visualization (engine-territory GUI).
//
// The prototype's two-lane language: a density lane (the SOUNDING measure,
// amber area fill; pending hand edits as a dimmed overlay) above an onset lane
// (grid ticks, amber onset dots, cyan flat-measure ghosts with deformation
// links). Interactive — three draw modes on the density lane:
//   DRAW   free paint (CurveEdit per touched bin, segment-interpolated);
//   BEZIER drag control points, smooth Catmull-Rom curve committed on release;
//   STEPS  paint fills whole n-grid cells (snaps to the selected grid).
// Preset chips load FLAT/RISE/FALL/WAVES/BEATS/RAND. All edits hit the drawn
// buffer — latched to sounding at the next bar (spec §2.4). A cyan playhead
// sweeps while playing. Reads only GuiSnapshot; depends on the GUI seam +
// Theme + this engine's header. JUCE lives here (engine gui/ adapter zone).
#pragma once

#include <vector>

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
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    enum class Mode { Draw, Bezier, Steps };
    struct Lane { float x0, x1, top, bot; };

    Lane densityLane() const;
    void setMode(Mode);
    void paintAt(const juce::MouseEvent&);           // Draw + Steps
    void bezierDown(const juce::MouseEvent&);
    void bezierDrag(const juce::MouseEvent&);
    void bezierCommit();                              // stream the sampled curve
    float bezierValueAt(float t) const;               // Catmull-Rom through pts_
    juce::Point<float> laneToNorm(juce::Point<float>) const;
    juce::Point<float> normToLane(juce::Point<float>) const;

    orrery::GuiSnapshot snap_;
    juce::TextButton presets_[6];
    juce::TextButton modes_[3];
    Mode  mode_ = Mode::Draw;

    // Free/steps paint state.
    int   lastBin_ = -1;
    float lastVal_ = 0.0f;

    // Bezier state: control points in normalized (t, v), sorted by t.
    std::vector<juce::Point<float>> pts_;
    int  dragPt_ = -1;
};
