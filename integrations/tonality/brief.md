---
id: orrery-001
from: Orrery
to: Tonality
status: draft
ball: consumer
filed: 2026-07-12
respond-by: (unset — draft; file when O5/scaleQuant is scheduled)
---

# Brief (DRAFT): pitch/scale JSON contract for scaleQuant + pitch assignment

## Need
Orrery's `scaleQuant` generator (offset layer, contract §2.3) and per-engine
pitch assignment need to quantize a pitch to a scale and map particle/onset
`sourceId` → scale-degree → note, via Tonality's shared JSON contract rather
than a local reimplementation (INTEGRATIONS rule 3).

## Proposed interface (consumer-side expectation — to be reconciled with Tonality's spec)
A pure, offline-callable contract (hot paths never call the provider —
INTEGRATIONS rule 6): given {scale/key identifier, pitch or scale-degree
index}, return the quantized MIDI pitch (+ optionally spelling for display).
Consumed at bar-latch boundaries or frozen into a lookup at load, never on the
audio thread.

## Boundary + degradation
ONE boundary module is the only code that knows Tonality's wire format; the
rest of Orrery consumes normalized MIDI integers. Absent Tonality, degrade
visibly to the static per-engine note map (contract §2.4 default) with a
surfaced "degraded: local scale" flag.

## Status
DRAFT — not yet filed with Tonality. File when O5 (generator completion) is
scheduled; until then scaleQuant is unimplemented and the static note map is
the shipped behavior. Pin the Tonality contract version at file time
(INTEGRATIONS rule 4).

## Contract tests offered (at file time)
A fixture set: (scale, degree) → expected MIDI, incl. edge cases (chromatic
passthrough, out-of-scale input policy), for Tonality's resident to land in
its CI.
