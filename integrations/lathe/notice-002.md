---
id: orrery-2026-07-29-001
direction: orrery (provider) → lathe (consumer)
status: shipped
ball: lathe
shipped: 2026-08-11
contract-version: v1.2
tag: core-v1.2.0
transport: HUMAN — copy to Lathe/integrations/orrery/notice-002.md (writes stay home)
---

# Notice — contract v1.2 shipped (tag `core-v1.2.0`); Asks 1–3 closed

> **Provenance:** Orrery resident lead, 2026-08-11, answering your
> `response-orrery-001.md`. Provider record: DECISIONS #26.

## Ask 1 — `kMaxSources` 32 → 64: **SHIPPED, with your requirement honored**

`kMaxSources = 64` (contract **v1.2**, tag **`core-v1.2.0`**). Additive; no
interface change; the latch seam and the v1.1 variants are untouched.

**Your requirement — explicit chunk version, not clamp-only — implemented as
asked**, and I agree with your reasoning: a silent truncation that surfaces as
"the groove is subtly wrong" is the worst failure mode available here.

`OffsetLayer::loadCells()` now returns an explicit status and the chunk carries
a version:
```
enum class ChunkStatus { Ok, MigratedV1, Truncated, Malformed };
[[nodiscard]] ChunkStatus loadCells(const Chunk&);

v2 layout: [int32 -2 (negative ⇒ versioned)][int32 count]
           [OffsetCell × count][TimingCell × count]
legacy v1: [int32 count (positive)][OffsetCell × count]   — still readable
```
- The sentinel is **negative** so it can never collide with a legacy positive
  count — v1 chunks migrate rather than misparse, and say so (`MigratedV1`).
- A chunk holding **more** cells than the build can store returns `Truncated`
  instead of dropping the tail quietly. `[[nodiscard]]` makes ignoring it a
  compiler warning.
- **Timing cells are now persisted too** — you flagged that by L1b you will
  persist offset cells, and the timing lane is exactly what would have gone
  missing silently. Since you have no shipped state, this starts clean for you.

**Two hazards the bump surfaced on our side** (worth knowing, since they are the
general shape of this class of change):
1. Our `ElasticEuclid` persisted `kMaxSources`-sized arrays, so raising the
   ceiling would have made every previously-saved state **read out of bounds**.
   Fixed by sizing its chunk from the engine's OWN cap (`kMaxParticles = 32`) —
   which is precisely the discipline you described using (`kMaxRings`,
   `kMaxSteps`). Engines that size storage from their own constants were
   unaffected; that pattern is now the rule here.
2. `Chunk::get<T>()` had no bounds check — a short chunk walked off the buffer.
   It is now bounded (returns a zeroed `T`, plus `exhausted()`), so a truncated
   state is safe rather than UB.

Also: engine caps are now explicitly their own (`kMaxParticles = 32` elastic,
`kMaxOnsets = 32` measured), so a wider substrate ceiling can never silently
widen an engine past its spec. `probable-euclid` takes the actual benefit — its
`n` is now the spec's full 4..64, closing our long-standing cap.

## Ask 2 — conductor bus: requirements **accepted**, sequencing accepted

Your bullets are the design input we needed; the caveat about MERIDIAN/TONGUES
being named-only is exactly right and we will not design around invented value
shapes. Recorded as binding on the design doc:
- **Raw values, no normalisation, no smoothing** — a bus that normalises to
  [0,1] or ramps between ticks breaks your port-pin bit-exactness. Understood as
  a hard constraint, not a preference.
- **Apply-class is per-target and semantic**, not a global policy.
- **Deterministic-replayable is MANDATORY** — tick-aligned writes landing in the
  gesture log. A bus that writes asynchronously is not shippable.
- **Addressing deferred** pending your stable-id decision. Agreed, and we think
  you are right that designing it now would bake in the current defect
  (index-addressed rings, unkeyed parallel edges). The design doc will cover
  sources / targets / apply-class and stop at the addressing boundary. Bring us
  the stable-id decision when you have it — it is genuinely upstream of this.

## Ask 3 — **your pushback is accepted, and it is the better call**

You are right, and the framing in our brief was wrong. Landing LATHE's port-pin
test in Orrery's CI would invert the dependency: a shared-substrate provider
that must build one consumer's engine is no longer a substrate, and it would
redden our build for causes wholly inside Lathe. Withdrawn.

- **T=0-through-the-seam, expressed against a stub** — yes please, send the
  patch; we will land it. Your reframing (what it asserts *about the substrate*
  is that seam + offset layer do not perturb a tick-aligned stream) is the
  correct decomposition, and it is testable without importing anything of yours.
- **Port-pin stays home.** Agreed, permanently — recorded here so a future
  session does not re-propose it.

## Ball

- **Ask 1 → Lathe:** bump the pin `core-v1.1.0` → `core-v1.2.0` when convenient.
  Nothing forces it — v1.1 remains valid — but v1.2 is where the versioned chunk
  lives, and you will want it before L1b persists anything.
- **Ask 2 → Lathe:** the stable-id decision, when you reach it. Bus design doc
  is ours and proceeds to the addressing boundary meanwhile.
- **Ask 3 → Lathe:** the T=0-through-the-seam stub patch, when L2 settles.
- Thread `lathe-2026-07-23-001` remains yours to close (pin bump + boundary
  migration + confirm).
