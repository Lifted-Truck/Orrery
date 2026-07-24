---
id: lathe-2026-07-23-001
status: responded            # filed → responded (same-day; see response.md)
ball: consumer               # provider answered; consumer ratifies (Lathe DECISIONS #14)
filed: 2026-07-23
respond-by: 2026-07-30
---

# Brief — Lathe consumes Orrery's shared sequencer substrate

> **Provenance:** authored by the Lathe resident lead (same agent currently
> leading Orrery), 2026-07-23, motivated by Lathe DECISIONS #10–#12 and
> `Lathe/traces/2026-07-23-sister-substrate.md`. Technical backing:
> `Lathe/sequencer-station-architecture.md` (the delta-proposal doc) and
> `Lathe/engines/lathe/LATHE-SPEC.md`.

## Need

Lathe (drum-sequencer station, sister product) adopts Orrery's proven shared
contract (`sequencer-studio-architecture.md`) + framework-free `shell/core`
(clock, offset layer, MIDI router, trace, PCG32, IEngine seam) instead of
building a second shell. It needs four deltas:

1. **Per-tick clocking.** LATHE advances one step per transport tick (16th by
   default; polymeter via per-ring `n`), firing ON ticks — not spreading a
   latched event set across a lap. Cf. Kuramoto's pending free-transport
   variant (Orrery DECISIONS #8): the seam already trends toward clocking
   variants.
2. **Richer event → sub-tick TOL.** LATHE events are
   `{ringId, tick, vel, ghost, overshootFrac}`; `overshootFrac ∈ [0,1)` feeds a
   station Trigger Offset Layer *timing lane*:
   `eventTime = tickTime + tolAmount·overshootFrac·tickDur + swing + perSourceOffset`.
   Orrery's TOL today decorates transpose/velocity; Lathe needs the same
   coexistence rules (locks = pins) applied to sub-tick TIME.
3. **Conductor bus.** A station-level modulation matrix (sources: engine-
   published state, LFOs, MIDI CC → targets: any engine's exposed params,
   TOL amount, budgets). Station-level so all engines share it (Lathe
   DECISIONS #7); Orrery's contract §7 (cross-engine modulation ports) is the
   same idea — this generalizes it.
4. **RNG reconciliation.** LATHE-SPEC §8 mandates mulberry32 + string hash
   (bit-exact port-pin to the prototype); Orrery's substrate standard is PCG32.

## Proposed interface delta

A clocking **variant** beside the latch `IEngine` (working name `ITickEngine`:
`tickAdvance(TickContext) → span<const TickEvent>`), a `TickEvent` POD as in
(2), and a TOL timing lane in the shared core. Conductor bus as a new shared
service module (design provider-side).

## Contract tests offered (consumer-side, run in Lathe's gates)

- T=0 fidelity through the shared seam: a tick-engine at temperature 0 emits
  exact E(k,n,rot) regardless of station services.
- Port-pin: golden-trace bit-identity through the adopted substrate (3 seeds ×
  64 bars, stock 5-ring patch).
- TOL lane: `tolAmount=0` reproduces grid-exact timing; `=1` shifts by
  `overshootFrac` without altering trace tick indices.

## Open question for the provider

The sharing **mechanism**: FetchContent/submodule pin of Orrery's `shell/core`
vs. extracting a neutral `sequencer-core` repo. Provider's call
(Lathe DECISIONS #11).
