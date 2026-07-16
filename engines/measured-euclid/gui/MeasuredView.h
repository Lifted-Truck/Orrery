// MeasuredView.h — Measured Euclid's own visualization (engine-territory GUI).
//
// The prototype's two-lane language: a density lane (the active measure curve,
// amber area fill) above an onset lane (grid ticks, amber onset dots, cyan
// flat-measure ghosts with deformation lines to the warped positions). Reads
// only GuiSnapshot (curve[64] + onset phases + energies); depends on the GUI
// seam + Theme. JUCE lives here (engine gui/ adapter zone).
#pragma once

#include "gui/IEngineView.h"
#include "gui/Theme.h"

class MeasuredView : public IEngineView {
public:
    MeasuredView() { setOpaque(true); }
    void updateSnapshot(const orrery::GuiSnapshot& s) override { snap_ = s; repaint(); }
    juce::String engineName() const override { return "MEASURED"; }
    void paint(juce::Graphics&) override;

private:
    orrery::GuiSnapshot snap_;
};
