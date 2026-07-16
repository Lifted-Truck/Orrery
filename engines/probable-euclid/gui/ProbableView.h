// ProbableView.h — Probable Euclid's own visualization (engine-territory GUI).
//
// The prototype's radial field: probability bars outward from the ring (height
// ∝ p_i per grid step), realized onsets as solid dots, high-probability
// backbone as cyan diamonds inside the ring, a bar-boundary pulse. Reads only
// GuiSnapshot (per-step phase + energy=p_i + realized). Depends on the GUI seam
// + Theme; JUCE lives here (engine gui/ adapter zone).
#pragma once

#include "gui/IEngineView.h"
#include "gui/Theme.h"

class ProbableView : public IEngineView {
public:
    ProbableView() { setOpaque(true); }
    void updateSnapshot(const orrery::GuiSnapshot& s) override { snap_ = s; repaint(); }
    juce::String engineName() const override { return "PROBABLE"; }
    void paint(juce::Graphics&) override;

private:
    orrery::GuiSnapshot snap_;
};
