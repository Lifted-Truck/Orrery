---
id: lathe-2026-07-23-001
status: shipped
ball: consumer               # ratified (Lathe #14); v1.1 now shipped — see notice.md
responded: 2026-07-23
---

# Response — accept with one counter-design (Orrery provider)

> **Provenance:** authored by the Orrery resident lead (same agent leading
> Lathe), 2026-07-23. Recorded provider-side as Orrery DECISIONS #22 +
> ROADMAP item **O-share**.

## Mechanism (the open question): FetchContent pin to a tagged Orrery release

Lathe consumes `Orrery/shell/core` via **CMake FetchContent pinned to a git
tag** of `github.com/Lifted-Truck/Orrery` (public), adding only
`shell/core` as a subdirectory — the same pinning pattern both stations
already use for JUCE. Provider commitment: tag **`core-v0.1.0`** on current
main (done with this response). One Lathe boundary file knows the mechanism
(INTEGRATIONS rule 1); version bumps are explicit pin changes.
*Deferred alternative:* extracting a neutral `sequencer-core` repo — right
shape at ≥3 consumers, pure overhead at 2 (both on this machine). Revisit if a
third station appears.

## Delta dispositions

| # | Delta | Disposition |
|---|---|---|
| 1 | Per-tick clocking | **Accept — contract v1.1 variant.** `ITickEngine` lands beside the latch `IEngine`, designed TOGETHER with Kuramoto's free-transport variant (Orrery DECISIONS #8) as one clocking-variants section — two pending engines now want off the latch, so this is a single design event, not two patches. Note: the shared clock already computes Step-division boundaries; the variant's real payload is the event shape + firing-on-tick semantics. |
| 2 | `TickEvent` + TOL timing lane | **Accept — v1.1.** `TickEvent {ringId:int32, tick:int32, vel:float, ghost:bool, overshootFrac:float}` POD in the shared core; the offset layer gains a TIMING lane (per-source static offset + swing + `tolAmount·overshootFrac`) under the existing coexistence rule (hand-set = pinned; generators flow around). Presentation-only: never feeds back into engine cores. |
| 3 | Conductor bus | **Accept — station-level shared service, phased after v1.1.** It generalizes contract §7 (cross-engine modulation ports, already "v2 horizon" provider-side). Engines publish named state; the bus drives named targets; bar-latched or continuous per route. Design doc lands provider-side before code; Lathe's LATHE-SPEC §12.7 is the reference target surface. |
| 4 | RNG reconciliation | **Counter-design: no contract change needed.** Engines already own their randomness streams behind the seam (Orrery precedent: each engine seeds/derives its own; the substrate never dictates the generator inside an engine). LATHE carries mulberry32 + string hash internally — its port-pin stays bit-exact — while station services keep PCG32. Determinism contract binds at the seam (identical inputs ⇒ identical event stream), not at the generator choice. |

## Sequencing

Provider-side v1.1 work (`ITickEngine` + `TickEvent` + TOL timing lane) is
scheduled as Orrery **O-share** and unblocks Lathe **L1**. Lathe is not
blocked meanwhile (INTEGRATIONS rule 2): it proceeds against the pinned
`core-v0.1.0` with a local, visibly-degraded tick-adapter stub and swaps to
the shared variant when v1.1 tags.

**Ball → consumer:** ratify (fold into Lathe ROADMAP/DECISIONS) or refine.
