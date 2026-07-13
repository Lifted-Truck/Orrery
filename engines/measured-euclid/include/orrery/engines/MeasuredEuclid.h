// MeasuredEuclid.h — onsets distributed with maximal evenness under a measure.
//
// Implements the IEngine contract (tick/latch). Source of truth: this dir's
// measured-euclid-spec.md; behavioral reference: measured-euclid.html. Depends
// ONLY on the contract (orrery_core).
//
// A density curve w(t) over the bar defines dμ = w(t)dt; k onsets sit at equal
// intervals of accumulated measure (crowd where w is high, spread where low).
// Flat measure + full quantize recovers classic Euclid — E(k,n) is a special
// case, not a mode. sourceId = μ-space onset index (0..k-1), stable across all
// curve manipulation; changes meaning only when k changes (§2.3).
#pragma once

#include <span>

#include "orrery/Contract.h"
#include "orrery/Pcg32.h"
#include "orrery/Types.h"

namespace orrery {

class MeasuredEuclid : public IEngine {
public:
    static constexpr int kM = 512;   // measure resolution
    enum class Preset { Flat, RampUp, RampDown, Waves, Beats, Rand };

    MeasuredEuclid();

    void seed(uint64_t projectSeed, uint64_t stream);

    // ── IEngine ──────────────────────────────────────────────────────────────
    void tick(TickContext&) override;
    std::span<const TriggerEvent> latchedEvents() const override;
    void handleGesture(const GestureEvent&) override;
    void handleMidiIn(const MidiPerturbation&) override;
    void saveState(Chunk&) const override;
    void loadState(const Chunk&) override;
    void writeTrace(TraceWriter&) const override;

    // ── Parameters (spec §3; all latch at bar) ───────────────────────────────
    void setK(int k);
    void setN(int n);
    void setPhase(double p);
    void setQuantize(double q);
    void setBreathe(bool on, int periodBars = 8);
    void setDrawnPreset(Preset p);            // load slot A (edit target)
    int  sourceCount() const { return k_; }

    // ── Introspection (tests / GUI) ──────────────────────────────────────────
    double onsetPhase(int i) const { return latched_[i].barPhase; }
    int64_t generation() const { return generation_; }
    // Forward CDF U(t) of the ACTIVE (latched) measure — exposed so the inverse-
    // accuracy test can check U(invU(u)) ≈ u.
    double forwardU(double t) const;
    double invU(double u) const;              // over the active measure

private:
    void buildCDF();                          // cdf_ from wActive_
    void computeOnsets();                      // latch → onset set
    void loadPreset(double* w, Preset p);
    static double wrap(double x);

    double wDrawn_[kM];   // slot A (edit target)
    double wActive_[kM];  // sounding (latched)
    double cdf_[kM + 1];  // CDF of wActive_, normalized to [0,1]

    int    k_ = 7;
    int    n_ = 16;
    double phase_ = 0.0;
    double q_ = 0.0;
    bool   breathe_ = false;
    int    breathePeriod_ = 8;
    int64_t generation_ = 0;
    int64_t barCount_ = 0;

    TriggerEvent latched_[kMaxSources];
    int          latchedCount_ = 0;

    Pcg32 rng_;

    static constexpr double kWMin = 0.02;
};

} // namespace orrery
