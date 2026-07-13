# CLAUDE.md — measured-euclid/ (engine territory)

Sub-charter. Read root `../CLAUDE.md` and contract
`../sequencer-studio-architecture.md` first. Implements the `IEngine`
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

## Latch obligation (validated — do NOT "improve")
Measure + all distribution params latch at the bar boundary; mid-bar edits
render dimmed (pending) and take effect next downbeat. Continuous recomputation
makes edits inaudible smears — bar-latching makes each edit a discrete audible
event. Same iterated-map philosophy as Elastic.
