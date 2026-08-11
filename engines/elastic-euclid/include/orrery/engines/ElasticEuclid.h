// ElasticEuclid.h — Euclidean rhythm as the equilibrium of a dynamical system.
//
// Implements the IEngine contract (tick/latch). Source of truth: this dir's
// elastic-euclid-spec.md (model/physics/clocking/determinism/tests stand; its
// shell sections are superseded by the contract). Behavioral reference:
// elastic-euclid-2.html. Depends ONLY on the contract (orrery_core) — no JUCE,
// no other engine.
//
// The model: k particles on a circle, pairwise repulsion (evenness) + lattice
// potential (grid) + damping. At equilibrium their phases coincide with a
// Bjorklund pattern E(k,n). Perturbations relax back over bar-to-bar
// generations. An iterated map clocked by musical time — pattern[t+1] =
// relax(pattern[t]) — evolution only at latch boundaries.
//
// sourceId = particle array index (dense 0..k-1, LIFO append/pop). This bounds
// the id to [0,kMaxSources) for the offset layer while preserving the "LIFO
// removal keeps survivors" identity invariant (§1.2). The prototype's monotonic
// id was display-only.
#pragma once

#include <span>

#include "orrery/Contract.h"
#include "orrery/Pcg32.h"
#include "orrery/Types.h"

namespace orrery {

class ElasticEuclid : public IEngine {
public:
    ElasticEuclid();

    // Seed the engine's randomness stream (kick impulses, add-particle jitter)
    // from the project seed for this slot. Called by the shell at slot setup;
    // gesture-time randomness has no TickContext, so the engine owns the stream.
    void seed(uint64_t projectSeed, uint64_t stream);

    // ── IEngine ──────────────────────────────────────────────────────────────
    void tick(TickContext&) override;
    std::span<const TriggerEvent> latchedEvents() const override;
    void handleGesture(const GestureEvent&) override;
    void handleMidiIn(const MidiPerturbation&) override;
    void saveState(Chunk&) const override;
    void loadState(const Chunk&) override;
    void writeTrace(TraceWriter&) const override;

    // ── Engine parameters (spec §4; ranges enforced) ─────────────────────────
    void setN(int n);
    void setRepulsion(double v);
    void setLattice(double v);
    void setDamping(double v);
    void setRelax(double seconds);
    // Freeze: hold the loop — tick() skips the physics (θ/ω untouched) so the
    // latched pattern repeats exactly; gestures still store, and relaxation
    // resumes from the stored state on unfreeze. (Human-requested capability
    // beyond the original spec; noted in this territory's CLAUDE.md.)
    void setFrozen(bool f) { frozen_ = f; }
    bool frozen() const { return frozen_; }
    int  latticeWells() const { return n_; }

    // ── Introspection (tests / GUI snapshot; message thread) ─────────────────
    int    sourceCount() const { return k_; }
    double theta(int i) const { return particles_[i].theta; }
    double omega(int i) const { return particles_[i].omega; }
    int64_t generation() const { return generation_; }

    // ── Contract-independent helpers (reused by the acceptance tests) ─────────
    // Bjorklund E(k,n): fills out[n] with the maximally-even onset pattern.
    static void bjorklund(int k, int n, bool* out);
    // Quantize a phase in [0,1) to its lattice cell in [0,n).
    static int  quantizeCell(double theta, int n);

private:
    struct Particle { double theta = 0.0; double omega = 0.0; };

    void integrate(double dt);        // one semi-implicit Euler substep
    void addParticle();               // gap-midpoint + seeded jitter, omega 0
    void latch();                     // snapshot live particles → latchedEvents

    static double wrap(double x);
    static double sdist(double a, double b);

    Particle particles_[kMaxSources];
    int      k_ = 5;
    int      n_ = 16;
    double   repulsion_ = 1.0;
    double   lattice_   = 0.6;
    double   damping_   = 0.35;
    double   relax_     = 0.08;       // simulated seconds of relaxation per tick
    bool     frozen_    = false;
    int64_t  generation_ = 0;

    TriggerEvent latched_[kMaxSources];
    int          latchedCount_ = 0;

    Pcg32 rng_;

    // THIS ENGINE'S cap, from its own spec (§4: k ∈ 1..32) — deliberately NOT
    // the substrate's kMaxSources, which rose to 64 at contract v1.2. Raising
    // the shared ceiling must never silently widen an engine past its spec.
    static constexpr int    kMaxParticles = 32;
    static constexpr int    kSub    = 24;     // substeps per tick (determinism)
    static constexpr double kOmegaClamp = 6.0;
    static constexpr double kEps   = 4e-4;
};

} // namespace orrery
