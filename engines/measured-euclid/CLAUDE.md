# CLAUDE.md — measured-euclid/ (engine territory)

Sub-charter. Read root `../../CLAUDE.md` and contract
`../../sequencer-studio-architecture.md` first. Implements the `IEngine`
contract; depends on the contract only.

## §Domain — source of truth
`measured-euclid-spec.md` (this dir) — engine only; shared concerns live in
the contract and are not duplicated. `sourceId` = μ-space onset index (0..k−1),
stable across measure edits / phase / quantize / breathe; changes meaning only
when k changes.

## The model in one line
Onsets distributed with **maximal evenness under a measure** dμ = w(t)dt: a
density curve over the bar; onsets at equal intervals of accumulated measure
(crowd where high, spread where low). Flat measure + full quantize recovers
classic Euclid — E(k,n) is a special case, not a mode.

## Reference oracle
`measured-euclid.html` (validated, no revisions requested).

## Acceptance tests (Layer-0 — spec §6, do not weaken)
Euclid recovery (flat measure, q=1, round-half-up ties), inverse accuracy
(|U(invU(u))−u| < 1e−6), latch invariant (no recompute between bars), monotone
deformation (no onset crossings; q=1 collisions emit simultaneous triggers with
distinct sourceIds), determinism bit-identity.

## Implementation notes (O3 — landed, `src/MeasuredEuclid.cpp`)
- CDF/invCDF matched to `measured-euclid.html` (piecewise-linear, binary
  search). `forwardU` exposed so the inverse-accuracy test checks U(invU(u))≈u.
- **Tie-robust round-half-up** in quantize (spec §6.1 correctness fix): naive
  `round(t·n)` sends exact half-integer ties DOWN because fp makes them land
  ~1e-16 below x.5 (e.g. 0.3·15 = 4.4999…982). This broke Euclid recovery for
  every gcd(k,n)>1 case (9/496). A `+1e-9` bias (≫ fp error, ≪ any real gap)
  restores round-half-up → E(k,n) recovers for ALL k<n≤32 (496/496). The
  plugin would otherwise emit wrong Euclidean patterns; this is a real fix, not
  test-tuning. (DECISIONS #13.)
- `sourceId` = μ-index (0..k−1); Add/Remove change k (LIFO on the highest
  index); CurveEdit(value) loads a preset; Drag sets μ-phase.

## Latch obligation (validated — do NOT "improve")
Measure + all distribution params latch at the bar boundary; mid-bar edits
render dimmed (pending) and take effect next downbeat. Continuous recomputation
makes edits inaudible smears — bar-latching makes each edit a discrete audible
event. Same iterated-map philosophy as Elastic.
