# MEASURED EUCLID — Engine Specification

**Status:** Prototype validated (`measured-euclid.html`), no revisions requested.
**Scope:** This spec defines the engine only. Shared concerns — trigger offset layer (per-trigger transpose/velocity, hand + generative editing), clocking, tracing, MIDI output, GUI shell — are defined in `sequencer-studio-architecture.md` and are **not** duplicated here. This engine implements the `IEngine` contract from that document.

---

## 1. Concept

Onsets distributed with **maximal evenness under a measure** rather than under clock time. A density function w(t) over the bar defines dμ = w(t)dt; onsets are placed at equal intervals of accumulated measure. Where the curve is high, onsets crowd; where low, they spread. The flat measure recovers classic even spacing, and flat measure + full grid quantization recovers classic Euclidean patterns — so E(k,n) is a special case, not a separate mode.

Any density function becomes a rhythm generator: hand-drawn curves, metric-weight profiles, LFO-morphed shapes, and (roadmap) curves derived from audio analysis of another track.

## 2. Core model

### 2.1 Measure representation
- `w: double[M]`, M = 512 samples over [0,1), display range [0,1], clamped to `w_min = 0.02` before integration (measure must be strictly positive or the inverse CDF degenerates).
- Two buffers: `wDrawn` (edit target) and `wActive` (sounding). See §2.4.

### 2.2 Onset computation
```
CDF:      U[0] = 0;  U[i+1] = U[i] + max(w_min, w[i]);  normalize U[M] = 1
Inverse:  invU(u) by binary search over U + linear interp within the bin
Onsets:   for i in 0..k-1:
            u_i = wrap((i + phase) / k)        // phase = rotation in μ-space
            t_i = invU(u_i)
Quantize: t_i ← wrap(t_i + q · sdist(t_i, round(t_i·n)/n))   // blend 0..1
```
`sdist(a,b) = wrap(b−a+0.5) − 0.5` (shortest circular arc). Note quantization is a **blend**, not a snap: q is a continuous morph between expressive timing and warped-Euclidean grid patterns.

### 2.3 Trigger identity (required by offset layer)
The stable `sourceId` of a trigger is its **μ-space index i** (0..k−1). This survives measure edits, phase changes, quantize changes, and breathe morphs; it changes meaning only when k changes. Offsets keyed by sourceId therefore persist across all curve manipulation, which is the behavior a performer expects.

### 2.4 Latch semantics (validated design decision — do not "improve")
The measure and all distribution parameters are **latched at the bar boundary**. `wActive ← f(wDrawn)` and onsets recompute only at bar lines (or on manual TICK / while transport is stopped). Mid-bar edits render as a pending (dimmed) curve and take effect at the next downbeat. Rationale from prototyping: continuous recomputation makes edits inaudible smears; bar-latching makes every edit a discrete, audible compositional event. Same iterated-map philosophy as Elastic Euclid.

### 2.5 Breathe (measure modulation)
Per-bar morph between drawn curve and flat: at each bar boundary,
`m = 0.5 − 0.5·cos(2π · (bar mod period) / period)`,
`wActive[i] = wDrawn[i]·(1−m) + 0.5·m`.
Generalization for the engine version: **morph between two stored curves** (slot A / slot B), flat being just a default slot B. Morph position is itself automatable and latched per bar. Period in bars (2..64) or driven by host automation.

### 2.6 Energy (feeds velocity, per studio contract)
Each trigger reports `energy = wActive(t_i)` normalized to [0,1] over the current curve — onsets in dense regions can be accented (or de-accented) by the offset layer's contour generator. This is engine-provided signal, not policy; policy lives in the offset layer.

## 3. Parameters

| id | name | range | default | latch |
|---|---|---|---|---|
| `k` | onsets | 1..32 (int) | 7 | bar |
| `phase` | μ-rotation | 0..1 | 0 | bar |
| `n` | grid | 1..64 (int) | 16 | bar |
| `quantize` | grid blend | 0..1 | 0 | bar |
| `breathe` | morph on/off | bool | off | bar |
| `breathePeriod` | bars | 2..64 | 8 | bar |
| `morphPos` | curve A⇄B | 0..1 | 0 | bar (automatable; breathe overrides) |
| curve slots | A, B | drawn/preset | BEATS / FLAT | bar |

Presets shipped: FLAT, RAMP↗, RAMP↘, WAVES, BEATS (wrap-aware Gaussians at beat positions, downbeat weighted), RAND (seeded band-limited noise, ≤4 harmonics).

## 4. GUI (engine panel within studio shell)

Port the prototype layout: density lane (draw target) above onset lane.
- Sounding curve solid amber with area fill; pending edits dimmed overlay; beat gridlines through both lanes.
- Onset lane: grid ticks, amber onset dots, **flat-measure ghosts (cyan diamonds) with deformation lines** connecting ghost → actual position. This visualization tested well; keep it.
- IOI readout in step units.
- "edits pending" indicator when dirty.
- Curve drawing: pointer paint with segment interpolation between events (prototype implementation is correct; port it).

## 5. Determinism & trace

- No wall-clock dependence; RAND preset and any generative curve sources use the studio's seeded PCG32.
- Trace record per bar (via studio TraceWriter): `{bar, k, phase, n, q, morph, wActiveHash, onsets[], energies[]}`. Curve stored as hash + optional full dump every N bars to keep traces light.

## 6. Acceptance tests

1. **Euclid recovery:** flat measure, q = 1: for all k < n ≤ 32, output pattern is a rotation of Bjorklund E(k,n). (The rounding construction of Euclidean rhythms; ties must break consistently — round-half-up in phase order.)
2. **Inverse accuracy:** |U(invU(u)) − u| < 1e−6 for 10⁴ random u, random smooth curves.
3. **Latch invariant:** no onset recomputation between bar boundaries while transport runs (assert via instrumentation).
4. **Monotone deformation:** onsets are strictly increasing in μ-order for any valid measure (no crossings), including under quantize blend < 1. At q = 1 collisions onto the same cell are permitted and must be emitted as simultaneous triggers with distinct sourceIds.
5. **Determinism:** identical seed + edit-event sequence → bit-identical trace.

## 7. Roadmap

- **Audio-derived measures:** sidechain input → onset-strength envelope or spectral-flux curve → w(t). Rhythm section that distributes itself around another track.
- **Metric-weight measures via Tonality contract:** w(t) from time-signature metric hierarchy.
- **Composite with Elastic Euclid:** use w(t) to scale the elastic engine's lattice well depths — measure shapes the equilibrium, physics handles transitions. (Likely the flagship studio patch; see architecture doc §7.)
