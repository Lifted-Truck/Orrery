# INTEGRATION-STANDBY — Orrery ↔ plugin-skeleton library

*Passive standby (notice 2026-07-29, DECISIONS #23). No refactoring toward the
library, no imagined interfaces. This file is the raw material for the first
brief when the mediator calls. Cheap + current > polished.*

Last updated: 2026-07-29 (state: O0–O3 + GUI + multi-engine slot shipped;
Orrery is also the shared-substrate **provider** for Lathe at tag `core-v0.1.0`).

---

## (a) Friction list — infrastructure problems actually hit here

**Parameters**
- APVTS is one **flat namespace** (~40 IDs). Hierarchy is faked with string
  prefixes: bare shell params (`gain`, `rate`, `gateMs`, `quantizeOut`,
  `transpose`), engine-scoped by convention only (`m_*` measured, `p_*`
  probable, Elastic's are *unprefixed* — an inconsistency), `voice*` for the
  onboard synth. No per-engine namespace, no addressing scheme.
- **Every engine's params exist always**, even for the 2 inactive engines —
  they are automatable, saved, and shown in the host's flat param list.
  Adding the 3rd engine added ~17 more IDs to the same flat space.
- Param→engine dispatch is a **hand-written `switch` per engine**
  (`EngineSlot::applyParams`) pulling that engine's subset by literal string.
  Adding an engine = editing that switch. No registry, no declaration.
- Applied **every block** for the active engine (cheap, but it is a poll, not a
  change-notification; `noteSpread` needed a manual "changed?" guard).
- Choice params must be read by `getIndex()`, not the raw normalized value — a
  real bug we shipped and fixed (DECISIONS: engine select).

**Modulation**
- **There is no modulation system.** Contract §7 (cross-engine modulation ports)
  is still "v2 horizon"; the **conductor bus** was accepted as a phased shared
  service only because Lathe asked (DECISIONS #22). Nothing routes today.
- The closest thing that exists — and the piece most worth generalizing — is the
  **offset layer's pins-and-flow rule**: generators write per-source cells at
  bar boundaries and **skip hand-locked cells**. Hand edit = pin; generators
  flow around it. That coexistence rule is engine-agnostic and reusable.
- Generators (`walk`, `accent`) are hardcoded types with hardcoded targets
  (transpose/velocity), not a source→target matrix. `contour`/`arp`/`scaleQuant`
  are specified but unbuilt.
- **No sub-tick timing lane** — TOL decorates pitch/velocity only. Lathe needs
  `overshootFrac`-driven micro-timing; accepted as contract v1.1, not built.

**Presets / state**
- **No preset system at all.** No format, no browser, no scoping, no morphing.
- State = one `ValueTree` chunk: seed + APVTS XML + 3 engine chunks + offset
  cells. Versioned only by an engine-internal `int32` at the head of each chunk.
- Known trap already recorded globally: never embed large binary in the state
  chunk (hosts silently fail to save) — path references instead.

**Scope / ownership (the notice's point 2 — pre-existing, documented)**
- `OffsetLayer`, `MidiRouter`, and the **note map are ONE shared instance across
  all three engines**. Switching engines silently reinterprets the same 32
  offset cells. Documented as a deliberate simplification (DECISIONS #17), but
  it is exactly the "should plausibly be per-module" case: **per-engine offset
  state is the known next refinement.**
- `sourceId` is engine-defined and capped at `kMaxSources = 32` — Probable
  wants a 64-step grid and is **capped at 32 today** pending an offset-layer
  capacity change (DECISIONS #14). A hard shared constant is the constraint.

**Voices**
- `VoiceBank` = fixed 32-voice array, one timbre, steal-quietest allocation,
  no per-voice modulation, no voice-level addressing. Params are global to the
  bank (`voiceTune/Decay/Transient/Drop`).

**MIDI / host**
- Plugin-API MIDI-out is not routable in Ableton at all → we ship a **CoreMIDI
  virtual source** as a second path (`VirtualMidiOut`). ~1 ms, off-thread.
- A `IS_MIDI_EFFECT` build **cannot be instantiated by Live** (no third-party
  MIDI-device role; verified in Live's log — DECISIONS #21 closure). Any library
  assumption that "MIDI effect" is a portable plugin role is wrong for Live.
- Note map is `36 + 3*(id%5)` + a global transpose; no per-source editor yet.

**Event pipeline**
- Fixed chain, hardcoded: clock latch → `engine.tick()` → offset generators →
  router mapping → mini-scheduler (lap-phase → sample-accurate on/off) → MIDI
  out + voices. No insert points, no reordering, no typed stages.
- Trace = POD ring → off-thread JSONL. Works well; the POD/ring split is the
  reusable part.

**Transport**
- Host transport requires an actual **ppq** (a playhead alone is not enough);
  standalone has none, so an internal free-run clock is mandatory. Any shared
  transport abstraction must model "no host transport" as a first-class case.

---

## (b) Inventory — components here others might want

**`shell/core/` — framework-free C++20, zero JUCE (already proven portable:
Lathe consumes it at tag `core-v0.1.0`)**
- `Contract.h` — the `IEngine` seam (tick/latch), `TickContext`, `GestureEvent`,
  `MidiPerturbation`, `Chunk` (POD save/load), `TraceWriter`.
- `Clock` — pure latch math: per-block boundaries, sample-accurate offsets,
  BAR/HALF/STEP, `barsPerLap`, lap-phase→ppq→sample. No wall clock. **±1-sample
  verified across 4 rates × 4 tempos × 3 divisions.**
- `OffsetLayer` — per-source cells + **hand-edit locks + generator coexistence**
  (the pins-and-flow rule). The single most reusable idea here.
- `MidiRouter` — note map + velocity + gate + `quantizeOut` blend + global
  transpose. Pure mapping, no I/O.
- `Trace` — JSONL writer **+ parser** (lossless round-trip, tested).
- `Pcg32` — seeded, per-slot streams from one project seed.

**Lock-free primitives (`shell/plugin/Lockfree.h` — framework-free, misfiled
under plugin/)**
- `SpscRing<T,N>` (POD, power-of-two) and `TripleBuffer<T>` (wait-free
  single-writer snapshot). Used for gestures, offset edits, MIDI out, and the
  GUI snapshot. Would move cleanly into a shared library.

**GUI system (`shell/plugin/gui/`, JUCE)**
- `Theme.h` — one token file (color/type/metric); `OrreryLookAndFeel`.
- **`IEngineView` seam** — the GUI analog of the engine seam: each module owns
  its view in its own territory, depends only on tokens + a POD snapshot.
- `GuiSnapshot` POD + triple-buffer publish; `OffsetEdit` SPSC — **GUI never
  touches model state.** This isolation pattern is the reusable part.
- `ParamRail` — rows built from a declarative spec + APVTS attachments (the
  seed of a param-registry-driven UI).
- `OffsetLane` — drag-to-edit cells with lock pins (the pins-and-flow UI).

**Plugin-shell pieces**
- `VirtualMidiOut` — CoreMIDI virtual source + drain thread (**the general
  answer to hosts that won't route plugin MIDI**).
- `TraceDrain` — POD ring → off-thread JSONL file.
- `VoiceBank` — pitched percussion voice (sine w/ pitch drop + noise click).

**Harness / gates (arguably the most portable thing here)**
- **Core-boundary gate** (`tools/check_core_boundary.py`) — fails the build if
  the framework leaks into a framework-free core; exempts `*/gui/` adapters.
- **RT no-alloc gate** — thread-local allocation hook asserting 0 heap allocs
  across steady-state `renderBlock`s, driven headlessly via an injected
  transport seam.
- Codesign re-seal POST_BUILD (JUCE breaks its own seal); pluginval-in-CI
  lesson; ship-Release-not-Debug lesson.

---

## (c) Sketch — current parameter + modulation architecture

**Parameters.** One `AudioProcessorValueTreeState` per plugin instance, flat.
IDs are strings with convention-only scoping (`m_`/`p_`/`voice`/bare). At
construction the processor caches `std::atomic<float>*` for every param into a
`Params` struct (no map lookups on the audio thread). Each `renderBlock`:
`applyParams()` → reads the atomics → writes shell config (clock division,
router quantize/transpose, offset generator settings, voice params) → then
`EngineSlot::applyParams(apvts)` switches on the active `engine` choice and
pulls **that engine's** subset by literal ID. GUI side: `ParamRail` is
constructed from a per-engine row spec (`{paramId, label, suffix, decimals}`)
with standard APVTS slider/button attachments; switching engines rebuilds the
rail. **No renames to date — additions only** (per standby rule 1, any future
rename gets an old→new line here).

**Addressing.** The only structured address is `sourceId` — an engine-defined,
stable-across-parameter-changes integer in `[0, 32)` (Elastic: particle index;
Measured: μ-space onset index; Probable: grid step). It keys the offset cells.
It is **not** namespaced by engine, so the 32 cells are shared/reinterpreted
across engines — the main known scope defect.

**"Modulation" (such as it is).** No matrix, no sources/targets, no depth. What
exists is the **Trigger Offset Layer**: per-`sourceId`
`OffsetCell{transpose, velOffset, lockT, lockV}`. At each bar boundary,
generators (`walk` = bounded seeded random walk; `accent` = Euclidean pattern
over sources) recompute cells **but skip any cell whose lock is set** — hand
edits are pins, generators flow around them; locks persist in state and cells
survive source-count changes. Output resolution is
`pitch = noteMap[sourceId] + transpose + globalTranspose`,
`velocity = clamp(baseVel + velOffset)`. Everything is seeded (PCG32, one stream
per engine slot + one for the offset layer, all derived from a single project
seed) and evaluated in fixed order, so the whole chain is bit-reproducible.
Modulation *of* parameters (LFOs, CC, cross-engine state) **does not exist**;
the conductor bus is accepted-but-unbuilt.

**Signal/event flow.** `clock latch → engine.tick() → offset generators →
router → mini-scheduler (lap-phase → sample-accurate note on/off) → MIDI out
(plugin bus + virtual port) + internal voices`. Hardcoded stages, no insert
points. GUI observes via a wait-free `GuiSnapshot` triple buffer and sends
gestures/offset edits back through SPSC rings — never touching model state.
