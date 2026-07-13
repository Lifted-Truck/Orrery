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
Bar realization is a pure function of (projectSeed, engineSlot, barIndex, n)
via pcg32 — same seed → identical sequence of realizations (a deterministic
performance of a stochastic object). ROLL increments a mixed-in counter;
FREEZE holds the current realization.
