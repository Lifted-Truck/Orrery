// StubEngine.cpp — implementation of the slot-proving stub engine.
#include "orrery/StubEngine.h"

#include <algorithm>

namespace orrery {

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

StubEngine::StubEngine(int initialSources) {
    k_ = std::clamp(initialSources, 1, kMaxSources);
}

void StubEngine::tick(TickContext& ctx) {
    generation_ = ctx.generation;
    for (int i = 0; i < k_; ++i) {
        // Even spread across the lap — a trivial Euclid E(k,k). Real engines
        // compute this from physics/measure; the stub just fills the seam.
        events_[i].barPhase = static_cast<double>(i) / static_cast<double>(k_);
        events_[i].sourceId = i;
        float e = ctx.rng ? ctx.rng->nextFloat() : 0.0f;
        events_[i].energy = clampf(e + energyBias_[i], 0.0f, 1.0f);
    }
}

std::span<const TriggerEvent> StubEngine::latchedEvents() const {
    return { events_, static_cast<size_t>(k_) };
}

void StubEngine::handleGesture(const GestureEvent& g) {
    switch (g.type) {
        case GestureEvent::Type::Add:
            if (k_ < kMaxSources) { energyBias_[k_] = 0.0f; ++k_; }  // new id = k_-1, zero bias
            break;
        case GestureEvent::Type::Remove:
            if (k_ > 1) { --k_; }  // LIFO: survivors 0..k_-1 keep identity + offsets
            break;
        case GestureEvent::Type::Kick:
        case GestureEvent::Type::Drag:
        case GestureEvent::Type::CurveEdit:
            if (g.sourceId >= 0 && g.sourceId < kMaxSources)
                energyBias_[g.sourceId] = clampf(energyBias_[g.sourceId] + g.value, -1.0f, 1.0f);
            break;
    }
}

void StubEngine::handleMidiIn(const MidiPerturbation& m) {
    const int id = ((m.note % k_) + k_) % k_;
    energyBias_[id] = clampf(energyBias_[id] + m.amount * (m.velocity / 127.0f), -1.0f, 1.0f);
}

void StubEngine::saveState(Chunk& c) const {
    c.put<int32_t>(k_);
    c.put<int64_t>(generation_);
    for (int i = 0; i < kMaxSources; ++i) c.put<float>(energyBias_[i]);
}

void StubEngine::loadState(const Chunk& c) {
    c.rewind();
    k_          = c.get<int32_t>();
    generation_ = c.get<int64_t>();
    for (int i = 0; i < kMaxSources; ++i) energyBias_[i] = c.get<float>();
}

void StubEngine::writeTrace(TraceWriter& tw) const {
    trace::writeEngineGeneration(generation_, latchedEvents(), tw);
}

} // namespace orrery
