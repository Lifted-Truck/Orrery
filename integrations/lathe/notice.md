---
id: lathe-2026-07-23-001
status: shipped
ball: consumer          # Lathe: integrate, bump the pin, verify against its own gates
shipped: 2026-07-29
contract-version: v1.1
tag: core-v1.1.0
---

# Notice — contract v1.1 shipped (tag `core-v1.1.0`)

> **Provenance:** Orrery resident lead, 2026-07-29. Implements the deltas
> accepted in `response.md` for brief `lathe-2026-07-23-001`. Provider record:
> DECISIONS #24, ROADMAP **O-share**.

## Shipped version

**Pin `core-v1.1.0`** (was `core-v0.1.0`). Contract doc bumped to **v1.1**;
`shell/core` unchanged in layout, so the FetchContent `SOURCE_SUBDIR shell/core`
mechanism is identical — only the `GIT_TAG` moves.

**Purely additive.** The latch `IEngine` seam is untouched and remains the
default; every v1.0 consumer compiles unchanged. Verified here: 15/15 core +
engine ctests green, the plugin's RT no-alloc gate still 0 allocs/4000 blocks.

## What landed

1. **`ITickEngine`** (`orrery/Contract.h`) — `tickAdvance(const TickEngineContext&)
   → std::span<const TickEvent>`, plus the same five obligations as the latch
   seam (gesture / midi-in / save / load / trace).
2. **`TickEvent`** (`orrery/Types.h`) —
   `{int32 sourceId; int32 tick; float vel; bool ghost; float overshootFrac;}`.
3. **`IFreeTransportEngine` + `FreeEvent`** — designed in the same event, as the
   response promised, because Orrery's own Kuramoto engine needed off the latch
   for the same reason (DECISIONS #8). Not something Lathe must consume; it is
   why the variants section is coherent rather than one-off.
4. **TOL timing lane** (`orrery/OffsetLayer.h`, contract §2.5) —
   `OffsetLayer::eventTime(id, pos, tickTime, tickDur, overshootFrac)` composing
   `tolAmount·overshootFrac·tickDur + swing(pos) + perSourceOffset·tickDur`,
   with `TimingCell{offset, lock}` under the existing pins-and-flow rule and
   `TimingParams{tolAmount, swing}`. **Default `tolAmount = 0` ⇒ grid-exact**,
   so behavior is unchanged until you opt in.
5. **Your contract tests now gate THIS repo** —
   `shell/core/tests/test_contract_v11.cpp` carries the three assertions your
   brief offered (tolAmount=0 grid-exact; tolAmount=1 shifts by
   `overshootFrac·tickDur` with the tick index untouched; the lane is
   presentation-only), landed per INTEGRATIONS §3. A provider-side regression
   against your expectations now fails Orrery's build.

## Migration notes (small — your boundary absorbs all of it)

- **Bump the pin:** `LATHE_ORRERY_TAG` → `core-v1.1.0`.
- **`ringId` → `sourceId`.** Your `Substrate.h` `TickEvent` names the field
  `ringId`; the shared contract keeps `sourceId` (the §1.2 identity concept —
  the seam does not adopt one engine's vocabulary). This is a one-line rename at
  your boundary, which is exactly what the boundary is for. LATHE-SPEC's
  *ringId* language stays valid engine-side.
- **`TickContext` → `TickEngineContext`.** The latch seam already owns the name
  `TickContext` (a latch/generation context despite its name); renaming that
  would break every shipped engine, so the variant took a distinct name.
  `TickEngineContext{tick, tempoBpm, rng*}`.
- **Delete your local definitions** of `ITickEngine` / `TickEvent` / the
  `tolEventTime` helper and include `orrery/Contract.h` + `orrery/OffsetLayer.h`
  instead. Your degraded banner (`kDegraded`, `status()`) can go with them —
  the degradation it announced is over.
- **RNG:** unchanged, as counter-designed. Keep mulberry32 inside LATHE; the
  substrate never touches it. Your port-pin bit-exactness is unaffected.
- Not shipped, still phased: the **conductor bus** (accepted, station-level,
  design doc before code). Nothing to migrate; it does not block L2.

## Closing the exchange

**Ball → consumer.** When Lathe has bumped the pin, adapted the boundary, and
`./verify fast` is green on your side, confirm in this thread and update both
ROADMAPs — that closes `lathe-2026-07-23-001`. Cite the brief ID in your PR so
the audit trail stays bidirectional.
