// ProbableEuclid.h — a probability field over the grid, sampled per bar.
//
// Implements the IEngine contract (tick/latch). Source of truth: this dir's
// probable-euclid-spec.md; behavioral reference: probable-euclid.html. Depends
// ONLY on the contract (orrery_core).
//
// State is a distribution, not a pattern: a logistic field p_i over n grid
// steps, ranked by nested greedy farthest-point evenness (blended toward an
// anchor by `clump`). Each bar samples one seeded, latched Bernoulli
// realization — a deterministic performance of a stochastic object. sourceId =
// grid step index; energy = realized step's p_i (velocity-follows-probability).
//
// PHASE-1 CONTRACT CONSTRAINT: the spec allows n∈[4,64], but sourceId=step-index
// must fit the offset layer's 32 cells (contract kMaxSources). n is therefore
// capped at 32 here; reaching 64 requires widening the offset-layer capacity —
// a gated contract change (filed, DECISIONS).
#pragma once

#include <span>

#include "orrery/Contract.h"
#include "orrery/Pcg32.h"
#include "orrery/Types.h"

namespace orrery {

class ProbableEuclid : public IEngine {
public:
    ProbableEuclid();

    void seed(uint64_t projectSeed, uint64_t stream);

    // ── IEngine ──────────────────────────────────────────────────────────────
    void tick(TickContext&) override;
    std::span<const TriggerEvent> latchedEvents() const override;
    void handleGesture(const GestureEvent&) override;
    void handleMidiIn(const MidiPerturbation&) override;
    void saveState(Chunk&) const override;
    void loadState(const Chunk&) override;
    void writeTrace(TraceWriter&) const override;

    // ── Parameters (spec §3) ─────────────────────────────────────────────────
    void setN(int n);                 // grid steps (Phase-1 cap: 4..32)
    void setDensity(double d);        // 0..n continuous
    void setTemperature(double t);    // 0..1
    void setClump(double c);          // 0..1
    void setAnchor(double a);         // 0..1
    void setFrozen(bool f);           // hold the current realization
    void roll();                      // re-sample the current bar (ROLL)

    int  gridSteps() const { return n_; }
    int  sourceCount() const { return n_; }   // addressable step-sources
    double prob(int i) const { return probs_[i]; }

    // ── Test/GUI introspection ───────────────────────────────────────────────
    // The τ→0 deterministic backbone (steps with effective rank+0.5 < d).
    void backbone(bool* out) const;   // out[n]
    // Greedy evenness rank of each step (prefix of length k = near-even).
    void evenRanks(int* out) const;   // out[n]
    double expectedCount() const;     // Σ p_i
    int64_t generation() const { return generation_; }

private:
    void computeField();              // probs_ from ranks + params
    void resample(int64_t barIndex);  // latch one Bernoulli realization
    static double cdist(double a, double b);
    static void   greedyEvenRanks(int n, int* rank);

    int    n_ = 16;
    double density_ = 5.0;
    double temperature_ = 0.2;
    double clump_ = 0.0;
    double anchor_ = 0.0;
    bool   frozen_ = false;
    int64_t generation_ = 0;
    int32_t rollCounter_ = 0;

    double  probs_[kMaxSources];
    TriggerEvent latched_[kMaxSources];
    int          latchedCount_ = 0;

    uint64_t projectSeed_ = 1;
    uint64_t stream_ = 1;
};

} // namespace orrery
