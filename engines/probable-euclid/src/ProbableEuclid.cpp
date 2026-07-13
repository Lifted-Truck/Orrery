// ProbableEuclid.cpp — probability-field rhythm engine.
// Greedy evenness, logistic field, and per-bar sampling matched to
// probable-euclid.html (RNG is the contract's PCG32 per spec §2.4, not the
// prototype's mulberry32 — the realization is stochastic, only its determinism
// is contractual).
#include "orrery/engines/ProbableEuclid.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace orrery {

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
static double wrap01(double x) { return std::fmod(std::fmod(x, 1.0) + 1.0, 1.0); }

double ProbableEuclid::cdist(double a, double b) { return std::abs(wrap01(b - a + 0.5) - 0.5); }

ProbableEuclid::ProbableEuclid() {
    computeField();
    resample(0);
}

void ProbableEuclid::seed(uint64_t projectSeed, uint64_t stream) {
    projectSeed_ = projectSeed; stream_ = stream;
}

// Cap at 32 (Phase-1: sourceId=step-index must fit the 32-cell offset layer).
void ProbableEuclid::setN(int n)          { n_ = clampi(n, 4, kMaxSources); }
void ProbableEuclid::setDensity(double d) { density_ = clampd(d, 0.0, n_); }
void ProbableEuclid::setTemperature(double t){ temperature_ = clampd(t, 0.0, 1.0); }
void ProbableEuclid::setClump(double c)   { clump_ = clampd(c, 0.0, 1.0); }
void ProbableEuclid::setAnchor(double a)  { anchor_ = wrap01(a); }
void ProbableEuclid::setFrozen(bool f)    { frozen_ = f; }

// Greedy farthest-point insertion (nested even family). Lowest-index tie-break
// (strict-improvement compare over ascending c), matching the prototype.
void ProbableEuclid::greedyEvenRanks(int n, int* rank) {
    bool chosen[kMaxSources] = {false};
    int  order[kMaxSources];
    chosen[0] = true; order[0] = 0; int cnt = 1;
    while (cnt < n) {
        int best = -1; double bestScore = -1.0;
        for (int c = 0; c < n; ++c) {
            if (chosen[c]) continue;
            double mind = 1e18;
            for (int s = 0; s < n; ++s) if (chosen[s]) mind = std::min(mind, cdist((double)c / n, (double)s / n));
            if (mind > bestScore + 1e-12) { bestScore = mind; best = c; }
        }
        chosen[best] = true; order[cnt++] = best;
    }
    for (int r = 0; r < n; ++r) rank[order[r]] = r;
}

void ProbableEuclid::computeField() {
    int rE[kMaxSources]; greedyEvenRanks(n_, rE);

    // Clump ranks: steps ordered by circular distance to the anchor, ascending
    // (stable → lowest index first on ties).
    int idx[kMaxSources]; for (int i = 0; i < n_; ++i) idx[i] = i;
    std::stable_sort(idx, idx + n_, [&](int a, int b) {
        return cdist((double)a / n_, anchor_) < cdist((double)b / n_, anchor_);
    });
    int rC[kMaxSources]; for (int r = 0; r < n_; ++r) rC[idx[r]] = r;

    const double s = std::max(0.02, temperature_ * n_ * 0.22);
    for (int i = 0; i < n_; ++i) {
        const double rank = (1.0 - clump_) * rE[i] + clump_ * rC[i];
        probs_[i] = 1.0 / (1.0 + std::exp((rank + 0.5 - density_) / s));
    }
}

void ProbableEuclid::resample(int64_t barIndex) {
    // Per-bar RNG: pure function of (projectSeed, slot, bar, n, rollCounter) —
    // a deterministic performance of the stochastic field (spec §2.4).
    Pcg32 br;
    const uint64_t st = projectSeed_
                      + static_cast<uint64_t>(barIndex) * 0x9E3779B97F4A7C15ULL
                      + static_cast<uint64_t>(static_cast<uint32_t>(rollCounter_)) * 0xD1B54A32D192ED03ULL;
    const uint64_t sq = (stream_ << 16) ^ static_cast<uint64_t>(n_);
    br.seed(st, sq);

    latchedCount_ = 0;
    for (int i = 0; i < n_; ++i) {
        if (br.nextFloat() < probs_[i]) {
            latched_[latchedCount_].barPhase = static_cast<double>(i) / n_;
            latched_[latchedCount_].sourceId = i;
            latched_[latchedCount_].energy   = static_cast<float>(probs_[i]);
            ++latchedCount_;
        }
    }
}

void ProbableEuclid::tick(TickContext& ctx) {
    generation_ = ctx.generation;
    computeField();                 // field is the live distribution
    if (!frozen_) resample(ctx.generation);   // sampling is latched (FREEZE holds)
}

std::span<const TriggerEvent> ProbableEuclid::latchedEvents() const {
    return { latched_, static_cast<size_t>(latchedCount_) };
}

void ProbableEuclid::handleGesture(const GestureEvent& g) {
    switch (g.type) {
        case GestureEvent::Type::Kick: roll(); break;                       // ROLL
        case GestureEvent::Type::Add:    setDensity(density_ + 1.0); break; // density nudge
        case GestureEvent::Type::Remove: setDensity(density_ - 1.0); break;
        case GestureEvent::Type::Drag:   setAnchor(static_cast<double>(g.value)); break;
        case GestureEvent::Type::CurveEdit: break;
    }
}

void ProbableEuclid::roll() {
    ++rollCounter_;
    if (!frozen_) { computeField(); resample(generation_); }
}

void ProbableEuclid::handleMidiIn(const MidiPerturbation&) {}  // no play-in perturbation

void ProbableEuclid::backbone(bool* out) const {
    int rE[kMaxSources]; greedyEvenRanks(n_, rE);
    int idx[kMaxSources]; for (int i = 0; i < n_; ++i) idx[i] = i;
    std::stable_sort(idx, idx + n_, [&](int a, int b) {
        return cdist((double)a / n_, anchor_) < cdist((double)b / n_, anchor_);
    });
    int rC[kMaxSources]; for (int r = 0; r < n_; ++r) rC[idx[r]] = r;
    for (int i = 0; i < n_; ++i) {
        const double rank = (1.0 - clump_) * rE[i] + clump_ * rC[i];
        out[i] = (rank + 0.5) < density_;
    }
}

void ProbableEuclid::evenRanks(int* out) const { greedyEvenRanks(n_, out); }

double ProbableEuclid::expectedCount() const {
    double sum = 0.0; for (int i = 0; i < n_; ++i) sum += probs_[i]; return sum;
}

void ProbableEuclid::saveState(Chunk& c) const {
    c.put<int32_t>(1);
    c.put<int32_t>(n_);
    c.put<double>(density_); c.put<double>(temperature_); c.put<double>(clump_); c.put<double>(anchor_);
    c.put<int32_t>(frozen_ ? 1 : 0);
    c.put<int64_t>(generation_);
    c.put<int32_t>(rollCounter_);
    c.put<uint64_t>(projectSeed_); c.put<uint64_t>(stream_);
}

void ProbableEuclid::loadState(const Chunk& c) {
    c.rewind();
    (void)c.get<int32_t>();
    n_ = c.get<int32_t>();
    density_ = c.get<double>(); temperature_ = c.get<double>(); clump_ = c.get<double>(); anchor_ = c.get<double>();
    frozen_ = c.get<int32_t>() != 0;
    generation_ = c.get<int64_t>();
    rollCounter_ = c.get<int32_t>();
    projectSeed_ = c.get<uint64_t>(); stream_ = c.get<uint64_t>();
    computeField();
    resample(generation_);
}

void ProbableEuclid::writeTrace(TraceWriter& tw) const {
    std::string s = "{\"kind\":\"probable\",\"gen\":" + std::to_string(generation_)
                  + ",\"n\":" + std::to_string(n_) + ",\"d\":" + std::to_string(density_)
                  + ",\"steps\":[";
    for (int i = 0; i < latchedCount_; ++i) { if (i) s += ','; s += std::to_string(latched_[i].sourceId); }
    s += "]}";
    tw.writeLine(s);
}

} // namespace orrery
