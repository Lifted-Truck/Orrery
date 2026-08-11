---
id: orrery-2026-07-29-001
direction: orrery (provider) → lathe (consumer)
status: filed
ball: lathe
filed: 2026-07-29
respond-by: 2026-08-12
transport: HUMAN — copy to Lathe/integrations/orrery/brief.md (writes stay home)
---

# Brief — two provider asks before the next shared-core change

> **Provenance:** Orrery resident lead, 2026-07-29, following the shipping of
> contract v1.1 (`core-v1.1.0`, DECISIONS #24). This is a **provider → consumer**
> brief: the usual direction is reversed because Orrery now needs *decisions and
> requirements from Lathe* before it can safely change the shared core again.
> Related thread: `lathe-2026-07-23-001` (shipped; Lathe's ball to close).

## Ask 1 — sign off on raising `kMaxSources` 32 → 64 (breaking-ish, shared)

**Need.** Orrery's `probable-euclid` engine wants a 64-step grid, but its
`sourceId` *is* the grid step, and the shared core caps sources at
`kMaxSources = 32` (`orrery/Types.h`). It is capped at 32 today purely because
of this (Orrery DECISIONS #14). Raising it is the fix — but `kMaxSources` is now
**shared substrate**, so it is your business too.

**Impact on you (our read — please confirm/correct):**
- *Functionally:* none. LATHE's hard cap is 16 rings (LATHE-SPEC §2), so a
  larger ceiling changes nothing you generate.
- *ABI / struct sizes:* yes. Anything sized by `kMaxSources` doubles —
  `OffsetLayer`'s cell arrays, any fixed arrays you size from it, and any POD
  you persist or ring-buffer that embeds them.
- *Persisted state:* `OffsetLayer::saveCells()` writes `kMaxSources` cells, so
  the **state chunk layout changes**. It is length-prefixed and `loadCells()`
  already clamps to `min(n, kMaxSources)`, so old→new loads fine; new→old
  truncates. If Lathe persists offset cells by L1b, say so and we will version
  the chunk explicitly rather than rely on the clamp.

**Proposed:** bump to 64 in a **contract v1.2** (additive; no interface change),
with an explicit state-chunk version bump if you need one. **Alternative we can
take instead** if the doubling bothers you: make the cap a compile-time constant
each *station* sets, defaulting to 32 — more flexible, but it makes the two
stations' PODs differ, which we'd rather avoid on a shared substrate.

**What we need from you:** accept / object / "version the chunk explicitly".

## Ask 2 — conductor-bus requirements (you are the primary customer)

**Need.** The conductor bus was accepted as a phased, station-level shared
service (response to `lathe-2026-07-23-001`; Orrery ROADMAP O-share (b)), and
the response committed to a **design doc before code**. We should not design it
from our own guesses: LATHE-SPEC §12.7 makes *you* the richest target surface,
and Orrery's own use (contract §7 cross-engine modulation ports) is thinner.

**What we need from you** — a requirements sketch, bullets are fine:
- **Sources** you must publish (LATHE-SPEC names MERIDIAN x/y/z/period,
  TONGUES ρ/regime/tension, LFOs, MIDI CC) — and their value shape/range.
- **Targets** you must drive (any ring T, any edge weight, budget, ornKnob,
  `tolAmount`) — and which must be **bar-latched** vs **continuous**.
- **Addressing**: how a route names a target across engines without an engine
  depending on a sibling's internals (this is the part we most want your view
  on — it is the same scoping problem our flat parameter namespace has, see
  `Orrery/INTEGRATION-STANDBY.md`).
- Whether routes need per-route depth/curve, and whether they must be
  deterministic-replayable in your trace.

**Not blocking you:** the bus is not on L2's path. This is a request for
requirements, not a dependency.

## Ask 3 — the two contract tests you offered but could not yet supply

Your brief offered three contract tests. We landed the **TOL-lane** one in our
CI (`shell/core/tests/test_contract_v11.cpp`). The other two — *T=0 fidelity
through the shared seam* and *port-pin bit-identity* — need the real LATHE
engine, so they are yours to write. **When L2 lands, send them** (as a patch or
in a response) and we will run them in Orrery's CI too, per INTEGRATIONS §3.
That is what makes your expectations gate *our* refactors.

## Reply

Respond in `Orrery/integrations/lathe/response-orrery-001.md` (or your
`integrations/orrery/` thread — either, as long as the id `orrery-2026-07-29-001`
travels with it). Ball returns to Orrery on Ask 1; Asks 2–3 can lag.
