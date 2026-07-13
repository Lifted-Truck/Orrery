# PROBABLE EUCLID — Engine Specification

**Status:** Prototype validated (`probable-euclid.html`), shipped as-designed.
**Scope:** Engine only; shell concerns per `sequencer-studio-architecture.md`.

---

## 1. Concept

The engine's state is a **probability field over the grid**, not a pattern. Each bar samples one realization from the field (seeded, latched). This makes onset density a *continuous* parameter — d = 5.5 is a meaningful rhythm that hovers between E(5,n) and E(6,n) — and makes the even⇄clustered axis a continuous anneal instead of a mode switch.

## 2. Core model

### 2.1 Evenness priority (nested family)
Greedy farthest-point insertion over the n grid positions (start at 0; repeatedly add the position maximizing minimum circular distance to the chosen set; deterministic lowest-index tie-break). Produces `rankEven[i]`: a priority ordering whose every prefix of length k is a near-maximally-even k-pattern.

**Documented trade-off (do not "fix"):** a nested family cannot coincide with Bjorklund at every k — e.g., E(3,8) ⊄ E(4,8). Continuity in density is bought by accepting near-Euclid at some k values. This is the engine's fundamental design decision; exact Bjorklund at integer d is *not* a requirement.

### 2.2 Clump blend
`rankClump[i]` = ordering by circular distance to anchor θ_a, ascending.
Effective rank: `rank[i] = (1−c)·rankEven[i] + c·rankClump[i]`, c = clump ∈ [0,1].

### 2.3 Field
Logistic threshold on effective rank at density d with temperature τ:
```
s    = max(0.02, τ · n · 0.22)
p_i  = 1 / (1 + exp((rank[i] + 0.5 − d) / s))
```
Properties: τ→0 gives the deterministic prefix (the **backbone**) plus a fractional boundary step; Σp_i ≈ d automatically; τ spreads probability mass across rank-adjacent steps.

### 2.4 Sampling (bar-latched, deterministic)
At each bar boundary (unless FROZEN): independent Bernoulli per step with `p_i`, using RNG `pcg32(mix(projectSeed, engineSlot, barIndex, n))` — a pure function of (seed, bar), so the same project seed always yields the identical sequence of realizations: a deterministic performance of a stochastic object. Manual ROLL re-samples the current bar (increments a roll counter mixed into the hash). FREEZE holds the current realization; field edits continue to display live but do not resample.

Realized k varies bar to bar (density breathes). This is intended; an optional `exactK` mode (systematic weighted sampling without replacement to hit round(d) exactly) is a v1.1 parameter, default off.

### 2.5 Trigger identity & energy
- `sourceId` = grid step index (offsets attach to *positions*, not voices — coherent with the studio contract; document in GUI so hand-set offsets read as "step 7 is always +5 semitones").
- `energy` = p_i of the realized step. **Velocity-follows-probability is core to the musical result** (backbone reads as groove, low-p events as ghost ornaments); ship the offset layer's `contour` generator defaulted ON for this engine.

## 3. Parameters

| id | range | default | latch |
|---|---|---|---|
| `density` | 0..n (continuous) | 5.0 | field live; sampling per bar |
| `temperature` | 0..1 | 0.2 |〃 |
| `clump` | 0..1 | 0 | 〃 |
| `anchor` | 0..1 (draggable on ring) | 0 | 〃 |
| `n` | 4..64 | 16 | bar |
| `freeze` | bool | off | immediate |
| `exactK` (v1.1) | bool | off | bar |

The field itself (p_i display) updates live — it is a distribution, not sounding state; only *sampling* is latched. This is the correct reading of the latch philosophy for a stochastic engine.

## 4. GUI

Radial probability bars outward from the ring (opacity + height ∝ p_i), realized onsets as solid dots (radius ∝ p), τ=0 backbone as cyan diamonds inside the ring, draggable anchor marker (visible when clump > 0), bar-boundary pulse, seed display + RESEED, readout: realization string, realized k, expected k = Σp_i, field entropy Σ H(p_i) in bits (the honest "how random is this" number).

## 5. Determinism & trace

Per-bar record: `{bar, d, τ, c, anchor, n, seedHash, probs[] (quantized u8), realization[], energies[]}`. Field quantized to 8-bit for trace weight.

## 6. Acceptance tests

1. **Backbone recovery:** τ = 0.001, integer d = k: realization equals the greedy prefix pattern deterministically, all k ≤ n ≤ 32.
2. **Prefix evenness quality:** for all k < n ≤ 32, greedy prefix's evenness score (sum of pairwise circular distances) ≥ 0.97 × Bjorklund E(k,n)'s score. (Calibrate the constant once against brute force; the point is a regression floor, not a proof.)
3. **Expected count:** |Σp_i − d| < 0.05 for d ∈ [0.5, n−0.5], τ ∈ [0, 1], c ∈ [0, 1].
4. **Determinism:** identical (projectSeed, parameter timeline) → bit-identical realization sequence and trace.
5. **Freeze invariant:** no resampling while frozen, across bar boundaries and parameter edits.

## 7. Roadmap

- **Hand-painted field with per-cell locks:** direct probability editing on the radial bars; locked cells excluded from the rank→field computation and held at painted values. Same pins-and-flow coexistence model as the offset layer — hand edits are constraints, the generative field fills around them.
- **Field as modulation port:** accept an external curve as the clump field (Measured Euclid's w(t) → this engine's clustering), unifying the two engines' expressive controls; expose p_i as an output signal.
- **Correlated sampling:** optional repulsive sampling (Poisson-disk / determinantal-style) so realized onsets avoid adjacency even at high τ — evenness enforced at the *sample* level, not just the field level.
- Swing/microtiming stage post-sampling.
