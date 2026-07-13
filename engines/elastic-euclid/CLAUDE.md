# CLAUDE.md — elastic-euclid/ (engine territory)

Sub-charter. Read root `../../CLAUDE.md` and contract
`../../sequencer-studio-architecture.md` first. Implements the `IEngine`
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
(kick impulses, add-particle jitter) from a seeded PCG32. Gesture-time
randomness has no `TickContext`, so the engine OWNS its stream; the shell seeds
it per slot via `ElasticEuclid::seed(projectSeed, stream)` at setup. Trace one
JSON line per generation.

## Implementation notes (O2 — landed, `src/ElasticEuclid.cpp`)
- **`sourceId` = particle array index** (dense 0..k-1, LIFO append/pop). The
  prototype's monotonic `id` was display-only; the contract needs ids in
  [0,32) to key the offset layer, and array-index identity preserves the
  "LIFO removal keeps survivors" invariant (§1.2).
- **Latch snapshot**: `tick()` freezes θ into the event set; gestures mutate the
  live particles but `latchedEvents()` stays frozen until the next tick (§2.4).
- **Gestures are immediate**: Add (gap-midpoint+jitter), Remove (LIFO), Kick
  (ω±1.5 all), Drag (pin θ, ω=0), MIDI-in (kick matched particle). None move
  OTHER particles' θ within a bar (latch invariant, tested).

## Basin characterization (MEASURED — a property, not a bug; do not "fix" by
## weakening the gate)
E(k,n) is a *stable equilibrium*, not a *global attractor*. Empirically
(5 sweeps over 18 (k,n) pairs, `tools`-style probes at build time):
- From an **exact-onset** start the pattern is held for 200+ generations
  (18/18) — a genuine fixed point.
- From **Bjorklund-adjacent** positional perturbations (≲0.4 lattice cell,
  no kick) the system relaxes back to a rotation of E(k,n) (72/72).
- **Beyond ~0.5-cell displacement, and with velocity kicks, metastable
  non-Euclid minima appear** (recovery falls to ~70–90%, then ~50% with kicks).
  A kick's musical job is *variation*, not guaranteed recovery.
- Convergence completes by **~200 ticks or not at all** (200/600/1500 ticks give
  identical outcomes) — non-recovery is a trap, not slow settling.
- The settled equilibrium sits **~0.016 RMS off exact grid cells** at default
  `lattice=0.6` (E(k,n) is *maximally*, not *perfectly*, even) — so the strict
  metric is quantize-to-cell / rotation match, not RMS→0.
Acceptance test §8.1 therefore asserts the two guaranteed properties (stability
+ within-basin recovery), non-trivially (distinct off-equilibrium starts all
reach the same Euclidean attractor + demonstrated particle migration), and
`§8.1`'s word "adjacent" is load-bearing. See `tests/test_elastic_equilibrium.cpp`.
