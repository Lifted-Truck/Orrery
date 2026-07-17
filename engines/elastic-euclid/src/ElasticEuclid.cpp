// ElasticEuclid.cpp — implementation of the equilibrium-rhythm engine.
// Physics constants and update order are matched to elastic-euclid-2.html (the
// behavioral reference oracle).
#include "orrery/engines/ElasticEuclid.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace orrery {

static constexpr double kTwoPi = 6.28318530717958647692;

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

double ElasticEuclid::wrap(double x) {
    double r = std::fmod(std::fmod(x, 1.0) + 1.0, 1.0);
    return r;
}
double ElasticEuclid::sdist(double a, double b) { return wrap(b - a + 0.5) - 0.5; }

ElasticEuclid::ElasticEuclid() {
    rng_.seed(0xE1A57C, 0x0E);
    // Default k=5 particles, evenly spread (spec §4). Placed deterministically;
    // the shell reseeds before use.
    k_ = 0;
    for (int i = 0; i < 5; ++i) {
        particles_[k_].theta = wrap(i / 5.0);
        particles_[k_].omega = 0.0;
        ++k_;
    }
    latch();
}

void ElasticEuclid::seed(uint64_t projectSeed, uint64_t stream) {
    rng_.seed(projectSeed, stream);
}

void ElasticEuclid::setN(int n)          { n_ = clampi(n, 1, 64); }
void ElasticEuclid::setRepulsion(double v){ repulsion_ = clampd(v, 0.0, 2.0); }
void ElasticEuclid::setLattice(double v)  { lattice_   = clampd(v, 0.0, 2.0); }
void ElasticEuclid::setDamping(double v)  { damping_   = clampd(v, 0.02, 2.0); }
void ElasticEuclid::setRelax(double s)    { relax_     = clampd(s, 0.01, 0.6); }

// One semi-implicit Euler substep (matches integrate() in the prototype).
void ElasticEuclid::integrate(double dt) {
    const double K = repulsion_ * 0.004;
    const double A = lattice_ * 3.0;
    const double c = damping_ * 8.0;
    double F[kMaxSources];
    for (int i = 0; i < k_; ++i) F[i] = 0.0;
    for (int i = 0; i < k_; ++i) {
        for (int j = i + 1; j < k_; ++j) {
            const double d = sdist(particles_[i].theta, particles_[j].theta);
            const double s = (d > 0.0) - (d < 0.0);
            const double f = K * s / (d * d + kEps);
            F[i] -= f; F[j] += f;
        }
        F[i] += -A * std::sin(kTwoPi * n_ * particles_[i].theta) / n_;
        F[i] += -c * particles_[i].omega;
    }
    for (int i = 0; i < k_; ++i) {
        Particle& p = particles_[i];
        p.omega = clampd(p.omega + F[i] * dt, -kOmegaClamp, kOmegaClamp);
        p.theta = wrap(p.theta + p.omega * dt);
    }
}

void ElasticEuclid::latch() {
    latchedCount_ = k_;
    for (int i = 0; i < k_; ++i) {
        latched_[i].barPhase = particles_[i].theta;
        latched_[i].sourceId = i;
        latched_[i].energy = static_cast<float>(clampd(std::abs(particles_[i].omega) / 2.0, 0.0, 1.0));
    }
}

void ElasticEuclid::tick(TickContext& ctx) {
    if (!frozen_) {
        const double h = relax_ / static_cast<double>(kSub);
        for (int s = 0; s < kSub; ++s) integrate(h);
    }
    generation_ = ctx.generation;
    latch();   // re-snapshot even when frozen so k changes stay visible
}

std::span<const TriggerEvent> ElasticEuclid::latchedEvents() const {
    return { latched_, static_cast<size_t>(latchedCount_) };
}

void ElasticEuclid::addParticle() {
    if (k_ >= kMaxSources) return;
    double th;
    if (k_ == 0) {
        th = 0.0;
    } else {
        double sorted[kMaxSources];
        for (int i = 0; i < k_; ++i) sorted[i] = particles_[i].theta;
        std::sort(sorted, sorted + k_);
        double best = -1.0; int bi = 0;
        for (int i = 0; i < k_; ++i) {
            const double g = wrap(sorted[(i + 1) % k_] - sorted[i]);
            if (g > best) { best = g; bi = i; }
        }
        const double jitter = (rng_.nextU32() * (1.0 / 4294967296.0) - 0.5) * 0.02;
        th = wrap(sorted[bi] + best / 2.0 + jitter);
    }
    particles_[k_].theta = th;
    particles_[k_].omega = 0.0;
    ++k_;
}

void ElasticEuclid::handleGesture(const GestureEvent& g) {
    switch (g.type) {
        case GestureEvent::Type::Add:
            addParticle();
            break;
        case GestureEvent::Type::Remove:
            if (k_ > 1) --k_;   // LIFO: survivors 0..k-1 keep identity + physics
            break;
        case GestureEvent::Type::Kick: {
            // ω_i += uniform(-1.5, 1.5) for every particle (seeded, per-particle
            // draw in index order for determinism). Stored velocity: no effect
            // until the next tick (latch semantics).
            const double mag = g.value != 0.0f ? g.value : 1.0;
            for (int i = 0; i < k_; ++i) {
                const double u = rng_.nextU32() * (1.0 / 4294967296.0);
                particles_[i].omega = clampd(particles_[i].omega + (u - 0.5) * 3.0 * mag,
                                             -kOmegaClamp, kOmegaClamp);
            }
            break;
        }
        case GestureEvent::Type::Drag:
            // Performance gesture: pin θ, zero ω. Takes effect immediately.
            if (g.sourceId >= 0 && g.sourceId < k_) {
                particles_[g.sourceId].theta = wrap(static_cast<double>(g.value));
                particles_[g.sourceId].omega = 0.0;
            }
            break;
        case GestureEvent::Type::CurveEdit:
            break;  // not meaningful for this engine
    }
}

void ElasticEuclid::handleMidiIn(const MidiPerturbation& m) {
    if (k_ <= 0) return;
    const int id = ((m.note % k_) + k_) % k_;
    const double u = rng_.nextU32() * (1.0 / 4294967296.0);
    const double scale = m.amount * (m.velocity / 127.0);
    particles_[id].omega = clampd(particles_[id].omega + (u - 0.5) * 3.0 * scale,
                                  -kOmegaClamp, kOmegaClamp);
}

void ElasticEuclid::saveState(Chunk& c) const {
    c.put<int32_t>(1);          // state version
    c.put<int32_t>(k_);
    c.put<int32_t>(n_);
    c.put<double>(repulsion_);
    c.put<double>(lattice_);
    c.put<double>(damping_);
    c.put<double>(relax_);
    c.put<int64_t>(generation_);
    c.put<uint64_t>(rng_.state);
    c.put<uint64_t>(rng_.inc);
    for (int i = 0; i < kMaxSources; ++i) {
        c.put<double>(particles_[i].theta);
        c.put<double>(particles_[i].omega);
    }
}

void ElasticEuclid::loadState(const Chunk& c) {
    c.rewind();
    (void)c.get<int32_t>();     // version
    k_ = c.get<int32_t>();
    n_ = c.get<int32_t>();
    repulsion_ = c.get<double>();
    lattice_   = c.get<double>();
    damping_   = c.get<double>();
    relax_     = c.get<double>();
    generation_ = c.get<int64_t>();
    rng_.state = c.get<uint64_t>();
    rng_.inc   = c.get<uint64_t>();
    for (int i = 0; i < kMaxSources; ++i) {
        particles_[i].theta = c.get<double>();
        particles_[i].omega = c.get<double>();
    }
    latch();
}

void ElasticEuclid::writeTrace(TraceWriter& tw) const {
    std::string s = "{\"kind\":\"elastic\",\"gen\":" + std::to_string(generation_)
                  + ",\"k\":" + std::to_string(k_) + ",\"n\":" + std::to_string(n_)
                  + ",\"th\":[";
    for (int i = 0; i < k_; ++i) { if (i) s += ','; s += std::to_string(particles_[i].theta); }
    s += "],\"om\":[";
    for (int i = 0; i < k_; ++i) { if (i) s += ','; s += std::to_string(particles_[i].omega); }
    s += "]}";
    tw.writeLine(s);
}

// ── Static helpers ───────────────────────────────────────────────────────────
int ElasticEuclid::quantizeCell(double theta, int n) {
    int cell = static_cast<int>(std::llround(wrap(theta) * n)) % n;
    return (cell + n) % n;
}

// Bjorklund E(k,n) — matches the prototype's recursive construction so the
// engine and its equilibrium oracle agree on the target pattern.
void ElasticEuclid::bjorklund(int k, int n, bool* out) {
    for (int i = 0; i < n; ++i) out[i] = false;
    if (n <= 0) return;
    if (k <= 0) return;
    if (k >= n) { for (int i = 0; i < n; ++i) out[i] = true; return; }

    std::vector<std::vector<int>> A(static_cast<size_t>(k), std::vector<int>{1});
    std::vector<std::vector<int>> B(static_cast<size_t>(n - k), std::vector<int>{0});
    while (B.size() > 1) {
        const size_t m = std::min(A.size(), B.size());
        std::vector<std::vector<int>> A2, B2;
        for (size_t i = 0; i < m; ++i) {
            std::vector<int> merged = A[i];
            merged.insert(merged.end(), B[i].begin(), B[i].end());
            A2.push_back(std::move(merged));
        }
        if (A.size() > B.size())
            for (size_t i = m; i < A.size(); ++i) B2.push_back(A[i]);
        else
            for (size_t i = m; i < B.size(); ++i) B2.push_back(B[i]);
        A = std::move(A2);
        B = std::move(B2);
    }
    int idx = 0;
    auto emit = [&](const std::vector<std::vector<int>>& seq) {
        for (const auto& grp : seq)
            for (int bit : grp)
                if (idx < n) out[idx++] = (bit != 0);
    };
    emit(A);
    emit(B);
}

} // namespace orrery
