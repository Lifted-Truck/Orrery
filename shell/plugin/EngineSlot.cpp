// EngineSlot.cpp — per-engine param application + snapshot fill.
#include "EngineSlot.h"

#include <algorithm>
#include <cmath>

namespace orrery {

static float pv(juce::AudioProcessorValueTreeState& p, const char* id) {
    return p.getRawParameterValue(id)->load(std::memory_order_relaxed);
}

void EngineSlot::applyParams(juce::AudioProcessorValueTreeState& p) {
    switch (kind_) {
        case EngineKind::Elastic: {
            elastic_.setN((int)pv(p, "wells"));
            elastic_.setRepulsion(pv(p, "repulsion"));
            elastic_.setLattice(pv(p, "lattice"));
            elastic_.setDamping(pv(p, "damping"));
            elastic_.setRelax(pv(p, "relax"));
            elastic_.setFrozen(pv(p, "freeze") > 0.5f);
            // "sources" → particle count via Add/Remove (Elastic only).
            const int target = juce::jlimit(1, kMaxSources, (int)pv(p, "sources"));
            int cur = elastic_.sourceCount();
            while (cur < target) { elastic_.handleGesture({GestureEvent::Type::Add, 0, 0.0f}); ++cur; }
            while (cur > target) { elastic_.handleGesture({GestureEvent::Type::Remove, 0, 0.0f}); --cur; }
            break;
        }
        case EngineKind::Measured:
            measured_.setK((int)pv(p, "m_k"));
            measured_.setN((int)pv(p, "m_n"));
            measured_.setPhase(pv(p, "m_phase"));
            measured_.setQuantize(pv(p, "m_quantize"));
            measured_.setBreathe(pv(p, "m_breathe") > 0.5f, (int)pv(p, "m_breathePeriod"));
            measured_.setPresetCycles((int)pv(p, "m_cycles"));
            measured_.setPresetSlope(pv(p, "m_slope"));
            measured_.setPresetSubdiv((int)pv(p, "m_subdiv"));
            break;
        case EngineKind::Probable:
            probable_.setN((int)pv(p, "p_n"));
            probable_.setDensity(pv(p, "p_density"));
            probable_.setTemperature(pv(p, "p_temperature"));
            probable_.setClump(pv(p, "p_clump"));
            probable_.setAnchor(pv(p, "p_anchor"));
            probable_.setFrozen(pv(p, "p_freeze") > 0.5f || pv(p, "freeze") > 0.5f);
            break;
    }
}

void EngineSlot::fillSnapshot(GuiSnapshot& s) const {
    s.kind = kind_;
    for (int i = 0; i < kMaxSources; ++i) s.realized[i] = 0;

    switch (kind_) {
        case EngineKind::Elastic: {
            const int k = elastic_.sourceCount();
            s.k = k; s.n = elastic_.latticeWells(); s.gen = elastic_.generation();
            for (int i = 0; i < k; ++i) {
                s.theta[i] = elastic_.theta(i);
                s.omega[i] = elastic_.omega(i);   // signed → whiskers
                s.energy[i] = (float)std::min(1.0, std::abs(elastic_.omega(i)) / 2.0);
                s.realized[i] = 1;
            }
            break;
        }
        case EngineKind::Measured: {
            const int k = measured_.sourceCount();
            s.k = k; s.n = measured_.gridN(); s.gen = measured_.generation();
            for (int i = 0; i < k; ++i) {
                s.theta[i] = measured_.onsetPhase(i);
                s.omega[i] = 0.0;
                s.energy[i] = measured_.onsetEnergy(i);
                s.realized[i] = 1;
            }
            measured_.fillCurve(s.curve, 64, false);
            measured_.fillCurve(s.curveDrawn, 64, true);
            break;
        }
        case EngineKind::Probable: {
            // Sources ARE the grid steps: phase = i/n, energy = p_i, realized =
            // whether this step's Bernoulli fired this bar.
            const int n = probable_.gridSteps();
            s.k = n; s.n = n; s.gen = probable_.generation();
            for (int i = 0; i < n; ++i) {
                s.theta[i] = (double)i / n;
                s.omega[i] = 0.0;
                s.energy[i] = (float)probable_.prob(i);
                s.realized[i] = 0;
            }
            for (const auto& ev : probable_.latchedEvents())
                if (ev.sourceId >= 0 && ev.sourceId < n) s.realized[ev.sourceId] = 1;
            break;
        }
    }
}

} // namespace orrery
