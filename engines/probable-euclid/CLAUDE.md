# CLAUDE.md — engines/probable-euclid/ (engine territory)

Sub-charter. Read root `../../CLAUDE.md` and contract
`../../sequencer-studio-architecture.md` first. Standard `IEngine` (bar-latch).

## §Domain — source of truth
`probable-euclid-spec.md` (this dir). `sourceId` = **grid step index** —
offsets attach to positions, not voices ("step 7 is always +5 semitones").

## The model in one line
Engine state is a **probability field over the grid**, not a pattern; each bar
samples one seeded, latched Bernoulli realization. Density d is continuous
(d=5.5 hovers between E(5,n) and E(6,n)); even⇄clustered is a continuous
anneal, not a mode switch.

## Reference oracle
`probable-euclid.html` (shipped as-designed).

## Core musical decision (do not "fix")
The evenness family is nested greedy farthest-point insertion, which **cannot**
coincide with Bjorklund at every k (E(3,8) ⊄ E(4,8)). Continuity in density is
bought by accepting near-Euclid at some k — exact Bjorklund at integer d is NOT
a requirement here. `energy` = realized step probability; velocity-follows-
probability is core (backbone = groove, low-p = ghost notes) → ship the offset
layer's `contour` generator defaulted ON for this engine.

## Determinism obligations
Bar realization is a pure function of (projectSeed, engineSlot, barIndex, n,
rollCounter) via **PCG32** (the contract RNG, per spec §2.4 — the prototype's
mulberry32 is not contractual; only determinism is). Same seed → identical
sequence of realizations (a deterministic performance of a stochastic object).
ROLL increments the mixed-in counter; FREEZE holds the current realization.

## Implementation notes + measured findings (O3 — landed, `src/ProbableEuclid.cpp`)
- **PHASE-1 CONTRACT CONSTRAINT: `n` capped at 32.** `sourceId` = grid step
  index must fit the offset layer's 32 cells (contract `kMaxSources`). The
  spec's n∈[4,64] needs the offset-layer capacity widened to 64 — a gated
  contract change (filed, DECISIONS #14). n≤32 is fully usable meanwhile.
- **§6.2 evenness floor calibrated to 0.80** (spec said "calibrate; a regression
  floor"). The greedy nested family degrades to ~0.80× Bjorklund evenness at
  small n — e.g. E(3,5) gives clustered {0,1,2} vs even {0,2,4} (equal
  min-distance; lowest-index tie-break picks the clustered one). This IS the
  documented §2.1 trade-off and matches the prototype. Gate floor: 0.79.
- **Σp_i ≈ d is a LOW-TEMPERATURE / pure-even property, not universal.** At
  τ→0, c=0 the field encodes density exactly (Σp = d to 1e-16). Clump (non-
  permutation blended ranks) and high temperature INTENTIONALLY decouple count
  from density — spreading probability mass is what temperature does (envelope
  reaches |Σp−d|≈4.5 at τ=1). The gate asserts the exact calibration + a
  NON-DECREASING density response (Σp never drops as d rises; plateaus at
  saturation), and characterizes the decoupling — the spec's blanket
  "<0.05 for all τ,c" over-claims. (DECISIONS #14.)
- `sourceCount()` = n (all grid steps are addressable offset-sources);
  `latchedEvents()` returns the realized subset. Kick→ROLL, Add/Remove nudge
  density, Drag sets the clump anchor.
