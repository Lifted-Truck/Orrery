// MeasuredEuclid.cpp — measure-warped even onset distribution.
// CDF / invCDF / onset construction matched to measured-euclid.html.
#include "orrery/engines/MeasuredEuclid.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace orrery {

static constexpr double kTwoPi = 6.28318530717958647692;
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

double MeasuredEuclid::wrap(double x) { return std::fmod(std::fmod(x, 1.0) + 1.0, 1.0); }

MeasuredEuclid::MeasuredEuclid() {
    rng_.seed(0x0EA5, 0x11);
    loadPreset(wDrawn_, Preset::Flat);
    for (int i = 0; i < kM; ++i) wActive_[i] = wDrawn_[i];
    buildCDF();
}

void MeasuredEuclid::seed(uint64_t projectSeed, uint64_t stream) { rng_.seed(projectSeed, stream); }

void MeasuredEuclid::setK(int k) { k_ = clampi(k, 1, kMaxSources); }
void MeasuredEuclid::setN(int n) { n_ = clampi(n, 1, 64); }
void MeasuredEuclid::setPhase(double p) { phase_ = wrap(p); }
void MeasuredEuclid::setQuantize(double q) { q_ = clampd(q, 0.0, 1.0); }
void MeasuredEuclid::setBreathe(bool on, int period) { breathe_ = on; breathePeriod_ = clampi(period, 2, 64); }
void MeasuredEuclid::setDrawnPreset(Preset p) { loadPreset(wDrawn_, p); }

static double gaussWrap(double t, double mu, double sig) {
    const double d = std::abs(std::fmod(std::fmod(t - mu + 0.5, 1.0) + 1.0, 1.0) - 0.5);
    return std::exp(-0.5 * (d / sig) * (d / sig));
}

void MeasuredEuclid::loadPreset(double* w, Preset p) {
    for (int i = 0; i < kM; ++i) {
        const double t = static_cast<double>(i) / kM;
        switch (p) {
            case Preset::Flat:     w[i] = 0.5; break;
            case Preset::RampUp:   w[i] = 0.02 + 0.96 * t; break;
            case Preset::RampDown: w[i] = 0.98 - 0.96 * t; break;
            case Preset::Waves:    w[i] = 0.5 + 0.4 * std::sin(kTwoPi * 3.0 * t); break;
            case Preset::Beats:
                w[i] = 0.08 + 0.90 * gaussWrap(t, 0.0, 0.025) + 0.45 * gaussWrap(t, 0.25, 0.025)
                     + 0.65 * gaussWrap(t, 0.5, 0.025) + 0.45 * gaussWrap(t, 0.75, 0.025);
                break;
            case Preset::Rand: w[i] = 0.0; break;  // filled below (needs rng)
        }
    }
    if (p == Preset::Rand) {
        double amp[4], ph[4];
        for (int h = 0; h < 4; ++h) { amp[h] = rng_.nextFloat() * 0.4; ph[h] = rng_.nextFloat() * kTwoPi; }
        for (int i = 0; i < kM; ++i) {
            const double t = static_cast<double>(i) / kM;
            double v = 0.5;
            for (int h = 0; h < 4; ++h) v += amp[h] * std::sin(kTwoPi * (h + 1) * t + ph[h]);
            w[i] = clampd(v, 0.0, 1.0);
        }
    }
}

void MeasuredEuclid::buildCDF() {
    cdf_[0] = 0.0;
    for (int i = 0; i < kM; ++i) cdf_[i + 1] = cdf_[i] + std::max(kWMin, wActive_[i]);
    const double total = cdf_[kM] > 0.0 ? cdf_[kM] : 1e-9;
    for (int i = 0; i <= kM; ++i) cdf_[i] /= total;
}

double MeasuredEuclid::invU(double u) const {
    u = wrap(u);
    int lo = 0, hi = kM;
    while (hi - lo > 1) {
        const int mid = (lo + hi) >> 1;
        if (cdf_[mid] <= u) lo = mid; else hi = mid;
    }
    const double span = (cdf_[hi] - cdf_[lo]) > 0.0 ? (cdf_[hi] - cdf_[lo]) : 1e-9;
    return (lo + (u - cdf_[lo]) / span) / kM;
}

double MeasuredEuclid::forwardU(double t) const {
    t = wrap(t);
    const double x = t * kM;
    int j = static_cast<int>(std::floor(x));
    if (j < 0) j = 0; if (j >= kM) j = kM - 1;
    const double frac = x - j;
    return cdf_[j] + frac * (cdf_[j + 1] - cdf_[j]);
}

void MeasuredEuclid::computeOnsets() {
    buildCDF();
    double maxW = kWMin;
    for (int i = 0; i < kM; ++i) maxW = std::max(maxW, wActive_[i]);

    latchedCount_ = k_;
    for (int i = 0; i < k_; ++i) {
        const double u = wrap((i + phase_) / k_);
        double t = invU(u);
        if (q_ > 0.0) {
            // Round-half-UP, tie-robust (spec §6.1): floating-point makes exact
            // half-integer ties land ~1e-16 below x.5 (e.g. 0.3·15 = 4.4999…982),
            // which naive round() sends DOWN — breaking Euclid recovery for
            // gcd(k,n)>1. The 1e-9 bias (>> fp error, << any real gap) restores
            // the spec's round-half-up so E(k,n) recovers for all k<n≤32.
            const double cell = std::floor(t * n_ + 0.5 + 1e-9);
            const double snapped = cell / n_;
            const double d = wrap(snapped - t + 0.5) - 0.5;
            t = wrap(t + d * q_);
        }
        const int idx = clampi(static_cast<int>(std::lround(t * (kM - 1))), 0, kM - 1);
        latched_[i].barPhase = t;
        latched_[i].sourceId = i;
        latched_[i].energy   = static_cast<float>(clampd(wActive_[idx] / maxW, 0.0, 1.0));
    }
}

void MeasuredEuclid::tick(TickContext& ctx) {
    generation_ = ctx.generation;
    barCount_   = ctx.generation;
    if (breathe_) {
        const double m = 0.5 - 0.5 * std::cos(kTwoPi * (barCount_ % breathePeriod_) / breathePeriod_);
        for (int i = 0; i < kM; ++i) wActive_[i] = wDrawn_[i] * (1.0 - m) + 0.5 * m;
    } else {
        for (int i = 0; i < kM; ++i) wActive_[i] = wDrawn_[i];
    }
    computeOnsets();
}

std::span<const TriggerEvent> MeasuredEuclid::latchedEvents() const {
    return { latched_, static_cast<size_t>(latchedCount_) };
}

void MeasuredEuclid::handleGesture(const GestureEvent& g) {
    switch (g.type) {
        case GestureEvent::Type::Add:    setK(k_ + 1); break;
        case GestureEvent::Type::Remove: if (k_ > 1) setK(k_ - 1); break;   // LIFO on μ-index
        case GestureEvent::Type::CurveEdit: {
            const int idx = clampi(static_cast<int>(std::lround(g.value)), 0, 5);
            loadPreset(wDrawn_, static_cast<Preset>(idx));
            break;
        }
        case GestureEvent::Type::Drag:   phase_ = wrap(static_cast<double>(g.value)); break;
        case GestureEvent::Type::Kick:   break;
    }
}

void MeasuredEuclid::handleMidiIn(const MidiPerturbation&) {}  // curve-driven; no play-in

void MeasuredEuclid::saveState(Chunk& c) const {
    c.put<int32_t>(1);
    c.put<int32_t>(k_); c.put<int32_t>(n_);
    c.put<double>(phase_); c.put<double>(q_);
    c.put<int32_t>(breathe_ ? 1 : 0); c.put<int32_t>(breathePeriod_);
    c.put<int64_t>(generation_);
    c.put<uint64_t>(rng_.state); c.put<uint64_t>(rng_.inc);
    for (int i = 0; i < kM; ++i) c.put<double>(wDrawn_[i]);
}

void MeasuredEuclid::loadState(const Chunk& c) {
    c.rewind();
    (void)c.get<int32_t>();
    k_ = c.get<int32_t>(); n_ = c.get<int32_t>();
    phase_ = c.get<double>(); q_ = c.get<double>();
    breathe_ = c.get<int32_t>() != 0; breathePeriod_ = c.get<int32_t>();
    generation_ = c.get<int64_t>();
    rng_.state = c.get<uint64_t>(); rng_.inc = c.get<uint64_t>();
    for (int i = 0; i < kM; ++i) wDrawn_[i] = c.get<double>();
    for (int i = 0; i < kM; ++i) wActive_[i] = wDrawn_[i];
    buildCDF();
}

void MeasuredEuclid::writeTrace(TraceWriter& tw) const {
    std::string s = "{\"kind\":\"measured\",\"gen\":" + std::to_string(generation_)
                  + ",\"k\":" + std::to_string(k_) + ",\"n\":" + std::to_string(n_)
                  + ",\"q\":" + std::to_string(q_) + ",\"onsets\":[";
    for (int i = 0; i < latchedCount_; ++i) { if (i) s += ','; s += std::to_string(latched_[i].barPhase); }
    s += "]}";
    tw.writeLine(s);
}

} // namespace orrery
