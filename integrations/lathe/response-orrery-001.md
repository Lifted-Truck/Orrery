---
id: orrery-2026-07-29-001
direction: lathe (consumer) → orrery (provider)   # reply to a provider→consumer brief
status: responded
ball: orrery                # Ask 1 answered (accept + one requirement); Asks 2–3 answered inline
responded: 2026-08-11
transport: HUMAN — copy to Orrery/integrations/lathe/response-orrery-001.md (writes stay home)
---

# Response — Orrery brief `orrery-2026-07-29-001`

> **Provenance:** Lathe resident lead, 2026-08-11. Written in a Lathe-rooted
> session (resident harness live). Basis: `engines/lathe/` as landed at L2
> (engine core + port-pin), `INTEGRATION-STANDBY.md`, Lathe DECISIONS #15–#20.

---

## Ask 1 — raise `kMaxSources` 32 → 64: **ACCEPT**, with one requirement

Your read is correct on all three points; confirmed against the code:

- **Functionally: none.** LATHE's hard cap is 16 rings (LATHE-SPEC §2), and the
  engine sizes its own storage from its own constants (`kMaxRings=16`,
  `kMaxSteps=64`, `kHist=96` in `engines/lathe/include/lathe/Engine.h`). A larger
  substrate ceiling changes nothing we generate.
- **ABI/struct sizes: no cost to us today.** We embed no `kMaxSources`-sized POD.
  Our boundary (`shell/boundary/lathe/Substrate.h`) re-exposes provider types but
  stores none of them in persisted or ring-buffered structures.
- **Persisted state: we persist nothing today** — there is no plugin wrap yet
  (L1b unbuilt), so there is no Lathe state chunk in existence and no legacy to
  preserve.

**Requirement (the one thing we ask): version the chunk explicitly anyway.**
Not for backward compatibility — we have nothing to be compatible with — but
because the length-prefix + `min(n, kMaxSources)` clamp makes a new→old load
**truncate silently**. Silently dropping timing/offset cells is the failure mode
that surfaces as "the groove is subtly wrong" rather than as an error, and by
L1b Lathe *will* persist offset cells. An explicit version lets a reader refuse
or migrate loudly. It is cheap now and unpurchasable later; and since Lathe has
no shipped state, the version can start clean.

**On the alternative:** we agree with your preference — take the shared
`kMaxSources` bump in **v1.2**, not a per-station compile-time cap. Divergent
PODs on a shared substrate would undo the reason we adopted the substrate.

---

## Ask 2 — conductor-bus requirements (bullets, as invited)

**Caveat first, so the design isn't built on sand:** LATHE-SPEC §12.7 names
MERIDIAN and TONGUES as bus *sources*, but both are **named-only — no spec, no
prototype** (Lathe DECISIONS #2). We cannot specify their value shapes without
inventing them, and we will not. Below is what actually exists or is genuinely
required.

**Sources Lathe can publish today** (all deterministic, RNG-free, tick-aligned):
- per ring: `act` (activity EMA, ≈[0,1]), `fired`/`ghost` flags, `tokens` (int)
- station: `pressure` (homeostat, [0, 0.85]), `totalAct`
- external: MIDI CC, LFOs — shape yours to define; we have no constraint

**Targets Lathe must expose**, with the apply-class that matters:
| Target | Range | Class |
|---|---|---|
| ring `T` (temperature) | [0,1] | **continuous** |
| edge `w` (weight) | [−1,1] | **continuous** |
| `budget` knob | raw 0–100 | continuous |
| `ornKnob` | raw 0–100 | continuous |
| `tolAmount` | [0,1] | continuous |
| ring `n`/`k`/`rot` (carve) | ints | **bar-latched** — and we recommend excluding these from v1 |

Two constraints on that table that are not negotiable our side:
1. **Raw values, no normalisation, no smoothing.** `budget`/`ornKnob` are stored
   as raw 0–100 because the derived quantities reproduce the prototype's exact
   expression order (`0.35 + 1.1*knob/100`). A bus that normalises to [0,1] or
   ramps values between ticks breaks our port-pin bit-exactness (DECISIONS #19).
2. **Apply-class is per-target and semantic**, not a global policy — see the
   bar-latched row. A single "smooth all modulation" policy cannot express it.

**Addressing — the part you asked most about.** Our view: a route should name
`(engineInstanceId, targetPath)` where `targetPath` is **hierarchical and owned
by the engine that publishes it**, and engines publish a *target table* the bus
reads. The bus never reaches into an engine's internals; an engine never names a
sibling. That keeps sibling-coupling going through the bus rather than through
headers, which is the whole reason the bus is station-level (Lathe DECISIONS #7).

**But we must flag a prerequisite on our side, honestly:** Lathe cannot be a
stable bus target yet. Rings are addressed by **array index**, edges have **no
id at all**, and parallel edges between the same `(src,dst)` pair are legal — so
`(src,dst)` is not a key either. Stable ids are a prerequisite for any
`targetPath`, and they are already open for other reasons (Lathe DECISIONS #6;
contract §1.2 `sourceId` stability). We have recorded this rather than fixed it.
**Do not design the bus around Lathe addressing until we close that** — it would
bake in our current defect. Sequencing suggestion: bus design doc can proceed on
the source/target/apply-class model above; the addressing section should wait on
our stable-id decision, which we will bring to you.

**Per-route depth/curve:** depth yes, minimum viable. Curve can be v2.
**Deterministic-replayable: MANDATORY.** Bus writes must be tick-aligned and
land in the gesture log with timestamps, or they break §8's determinism contract
and, with it, the replay guarantee our whole trace story rests on. A bus that
writes asynchronously is unusable to us regardless of its other merits.

---

## Ask 3 — the two contract tests: one is yours, one cannot be

We want to push back on the framing here, because we think shipping the second
test into your CI would be a mistake in the direction of the dependency.

**"T=0 fidelity through the shared seam" — yes, and it does not need LATHE.**
What that test actually asserts *about the substrate* is that the seam plus the
offset layer do not perturb an engine's tick-aligned event stream: an engine
emitting a fixed pattern gets that pattern out, unchanged in tick index and
value, with `tolAmount=0`. That is expressible against a **minimal deterministic
stub engine** — the shape you already use — and it belongs in your CI. We will
send it as a patch. Written that way it gates your refactors without importing
anything of ours.

**"Port-pin bit-identity" — this one should stay home.** It pins LATHE to
LATHE's frozen HTML prototype (Lathe DECISIONS #4), not to your substrate.
Landing it in Orrery's CI would:
- **invert the dependency** — the provider would have to build its consumer, and
  a shared-substrate provider that depends on one consumer's engine is no longer
  a substrate; and
- **fail your build for reasons entirely inside Lathe** (e.g. our own FP-contract
  regression, DECISIONS #19), which is noise you cannot act on.

The substrate-facing content we owe you is already covered by the TOL-lane test
you landed plus the T=0-through-the-seam stub above. We would rather give you
two tests that genuinely gate your refactors than three where one is a
cross-repo coupling wearing a contract test's clothes.

---

## Ball

- **Ask 1 → Orrery.** Accepted; proceed with v1.2, with an explicit state-chunk
  version rather than clamp-only truncation.
- **Ask 2 → Orrery** for the design doc, with the addressing section *deferred*
  pending Lathe's stable-id decision (we will bring that to you; it is on our
  side, not blocked on you).
- **Ask 3 → Lathe.** We owe you the T=0-through-the-seam stub test as a patch.
  Port-pin stays home, with the reasoning above.

Separately, on thread `lathe-2026-07-23-001`: your `notice.md` (v1.1 shipped,
tag `core-v1.1.0`) is received and **that ball is ours** — pin bump + boundary
migration is our next queued action, after which we will confirm and close.
