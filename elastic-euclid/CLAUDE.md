# CLAUDE.md — elastic-euclid/ (engine territory)

Sub-charter. Read root `../CLAUDE.md` and contract
`../sequencer-studio-architecture.md` first. Implements the `IEngine`
contract; depends on the contract only, never on the shell's internals or
another engine.

## §Domain — source of truth
`elastic-euclid-spec.md` (this dir) — the engine's model, physics, clocking,
determinism, and acceptance tests. **Precedence:** the spec's shell sections
(§3 format/threading, §4 MIDI mapping, per-particle note table) are SUPERSEDED
by the contract; its model/physics/clocking/determinism/test sections stand
(root DECISIONS #1). `sourceId` = particle id.

## The model in one line
Euclidean pattern E(k,n) as the **equilibrium of a dynamical system**:
k particles on a circle, pairwise repulsion (evenness) + lattice potential
(grid) + damping; perturbations relax back over audible bar-to-bar generations.
An iterated map clocked by musical time — `pattern[t+1] = relax(pattern[t])` —
NOT continuous flow (validated + rejected; spec §Core design decision).

## Reference oracle
`elastic-euclid-2.html` (canonical, clocked iterated map). The deprecated
continuous-flow prototype `elastic-euclid.html` referenced in the spec is NOT
in the repo — the -2 prototype is the behavioral reference to match.

## Acceptance tests (Layer-0 — spec §8, do not weaken)
Equilibrium correctness (E(k,n) recovery ≤200 ticks), determinism bit-identity
(seeded PCG32), ±1-sample timing (44.1/48/96k, blocks 32..2048, mid-block
ticks), no-alloc in processBlock (debug allocation hooks), k-change latch
invariant (no discontinuous jump of other particles within a bar).

## Determinism obligations
Fixed SUB=24 substeps/tick; double precision; no wall-clock; all randomness
(kick impulses, add-particle jitter) from the shell's seeded PCG32 for this
slot. Trace one JSON line per generation.
