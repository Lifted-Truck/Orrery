// ElasticView.h — Elastic Euclid's own visualization (engine-territory GUI).
//
// The first IEngineView: the prototype ring language ported to JUCE — lattice
// ticks (length ∝ pull), amber particles with id + energy whiskers, hollow cyan
// E(k,n) ghost diamonds at the nearest rotation, playhead sweep, generation /
// Δ-equilibrium readout. Lives in the elastic territory because the view IS
// part of the organ; it depends only on the GUI seam (IEngineView/Theme/
// Snapshot) and this engine's own headers. JUCE is allowed here (gui/ is the
// adapter zone; the engine CORE stays framework-free — enforced by the
// boundary gate's gui/ exemption).
#pragma once

#include <vector>

#include "gui/IEngineView.h"
#include "gui/Theme.h"
#include "orrery/engines/ElasticEuclid.h"

class ElasticView : public IEngineView {
public:
    ElasticView();

    void updateSnapshot(const orrery::GuiSnapshot&) override;
    juce::String engineName() const override { return "ELASTIC"; }

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;   // click a particle → kick it

private:
    void computeGhosts();   // nearest-rotation E(k,n) alignment (prototype port)

    orrery::GuiSnapshot snap_;
    std::vector<double> ghosts_;   // ghost phases [0,1)
    double rms_ = 0.0;             // Δ from equilibrium (prototype metric)
};
