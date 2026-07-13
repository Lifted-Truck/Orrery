# TORUS EUCLID — Engine Specification

**Status:** Prototype validated (`torus-euclid.html`), shipped.
**Scope:** Engine only; shell per `sequencer-studio-architecture.md`. Standard `IEngine` (tick/latch) contract. This is ORRERY's first pitch-generating engine.

---

## 1. Concept

k events distributed with maximal evenness over a **time × pitch torus**: n time columns wrapping at the bar, m pitch rows wrapping at the octave (scale-degree rows, so the wrap is an octave-class fold). Evenness in the 2D toroidal metric = blue-noise coverage of rhythm and melody simultaneously; no two events close in both time and pitch. An anisotropy parameter weights the metric between the axes, making rhythm-evenness ⇄ pitch-coverage a single continuous trade.

Hand-placed **pinned events** act as constraints seeding the generative layout — the direct pattern-level realization of the studio's pins-and-flow model.

## 2. Core model

### 2.1 Metric
Cells (t, p), t ∈ [0,n), p ∈ [0,m). Circular per-axis distances normalized to [0, 0.5]:
```
dt = cdist(t₁,t₂,n)/n ;  dp = cdist(p₁,p₂,m)/m
d²(a,b) = (1−A)·dt² + A·dp²          A = anisotropy ∈ [0,1]
```

### 2.2 Layout (greedy farthest-point, pins first)
1. Seed the chosen set with all pinned cells (in pin order). If none and k > 0, seed (0, ⌊m/2⌋).
2. While |chosen| < k: add the unoccupied cell maximizing min weighted d² to the chosen set. Tie-breaks, in order: max min *unweighted* d², then lowest cell index. Deterministic; no RNG in the layout.
3. Rotations (rotT, rotP) applied as a global output transform after layout (pins stored in pre-rotation coordinates).

Nested by construction: the length-k layout is a prefix of the length-(k+1) layout (given fixed pins/params), so density sweeps add/remove events one at a time in structural order.

### 2.3 Relaxation pass (engine addition, not in prototype)
Greedy is visibly greedy in 2D — early auto-placements can wedge suboptimally, especially near clustered pins. After insertion, run L iterations (default 3, param 0..10) of Lloyd-style nudging in the toroidal metric: move each **unpinned** event to the toroidal centroid-of-farthest-region approximation (or gradient step away from nearest neighbors), then re-snap to the nearest unoccupied cell. Pins never move. L = 0 must reproduce the prototype layout exactly (regression anchor).

### 2.4 Latch
Full family discipline: parameter/pin edits mark dirty; layout recomputes at the bar boundary (or manual TICK / while stopped). Pending layout rendered dimmed. **Exception carve-out:** rotations may optionally apply immediately (`rotateLive` param, default off) since they are output transforms, not layout changes — cheap and performable.

### 2.5 Trigger identity (`sourceId`) — honest limitations
- Pinned events: stable id assigned at pin creation (survives drags — a moved pin is the same note).
- Generated events: sourceId = insertion rank. Stable under rotation and while pins/params are fixed; **positions under a given rank shift when pins, k, A, n, or m change.** Document in GUI: offsets on generated events attach to structural rank ("the 3rd most structural note"), not to a location. This is coherent but must be legible to the user; consider a GUI affordance to promote a generated event to pinned ("claim it") which freezes both position and identity.

### 2.6 Energy
`energy = 1 − rank/k` (structural importance). Velocity-follows-energy default ON: skeleton anchors, late insertions play as ornaments. Simultaneous events in one column are emitted as a chord with distinct sourceIds.

## 3. Pitch mapping

Row p → scale degree `p mod |S|`, octave `⌊p/|S|⌋`, over root note parameter. Ship scales: major/minor pentatonic, dorian, harmonic minor, major, natural minor, chromatic — **but the engine version should route through the Tonality contract** (scale = Tonality scale object; degree→MIDI resolution delegated) rather than hardcoding, with the built-ins as fallback when no Tonality endpoint is configured. Note: because the pitch axis wraps at the octave, `m` should default to a multiple of |S| (UI nudge, not a hard constraint); non-multiples are legal and produce mode-rotation at the wrap seam, which is a feature worth documenting rather than preventing.

## 4. Parameters

| id | range | default | latch |
|---|---|---|---|
| `k` | 1..64 (≥ pin count; adding pins can raise k) | 9 | bar |
| `anisotropy` | 0..1 | 0.5 | bar |
| `n` (time) | 4..64 | 16 | bar |
| `m` (rows) | 2..24 | 10 | bar |
| `rotT` | 0..n−1 | 0 | bar (or live) |
| `rotP` | 0..m−1 | 0 | bar (or live) |
| `relaxIters` | 0..10 | 3 | bar |
| `rotateLive` | bool | off | immediate |
| `scale/root` | Tonality ref or builtin | penta / A2 | bar |
| pin gestures | click add / drag / dbl-unpin / unpin-all | — | bar |

Resizing n or m: pins outside the new range are dropped with undo support (engine keeps a pin-edit undo stack, depth ≥ 32).

## 5. GUI

Piano-roll-torus: rectangle with dashed outer border signaling wrap on both axes, beat-emphasized column lines, octave-emphasized row lines with 8ve labels, sounding events (radius ∝ energy), cyan ring on pins, dimmed pending layout when dirty, playhead, blue-noise radius + onset-column-count readout. Interactions per prototype: click empty cell = pin (+k if needed), drag any event (generated events auto-pin on grab), double-click = unpin.

## 6. Determinism & trace

Layout is a pure function of (pins, k, A, n, m, relaxIters) — no RNG anywhere in this engine. Trace per latch: `{bar, params, pins[], layout[](t,p,rank,pinned), rotations}`.

## 7. Acceptance tests

1. **1D degeneracy:** m = 1 (or A = 0 with distinct-column tie-breaking): time positions of the k events form the greedy evenness prefix from Probable Euclid's `evenPriority(n)` — the engines share a spine; verify literally.
2. **Nesting:** layout(k) ⊂ layout(k+1) for all k < 32, fixed pins/params, L = 0.
3. **Pin invariance:** pinned cells appear in every layout unchanged, all parameter combinations.
4. **Wrap correctness:** min pairwise toroidal distance of a layout computed with wrap equals brute-force over all 9 unwrapped copies.
5. **Relaxation:** L > 0 never decreases min pairwise weighted distance vs L = 0 (monotone improvement or hold), and L = 0 is bit-identical to prototype layouts on a fixture set.
6. **Chord emission:** stacked events (A → 1 extreme) emit simultaneous triggers with distinct sourceIds at one sample offset.

## 8. Roadmap

- **Third axis:** time × pitch × velocity-or-timbre torus (m small) — evenness over expression space.
- **Toroidal Lloyd as a live physics mode:** run relaxation continuously with the elastic engine's clocked-tick discipline — Torus and Elastic converge into one engine family (2D elastic lattice).
- **Tonality-aware metric:** replace row-distance with voice-leading distance from the Tonality contract, so "evenness in pitch" means evenness in harmonic space rather than scale-step space.
- Per-column measure weighting (Measured Euclid's w(t) as a time-axis density → warped toroidal metric).
