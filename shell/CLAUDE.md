# CLAUDE.md — shell/ (studio host territory)

Sub-charter. Read the root `../CLAUDE.md` and the contract
`../sequencer-studio-architecture.md` first. This territory OWNS the
contract's implementation; engines depend on it, not the reverse.

## §Domain — what shell/ owns
The plugin shell and every shared service from the contract:
- **Clock service** (contract §3): single transport authority from
  `AudioPlayHead`; sample-accurate latch callbacks at each engine's division;
  fixed engine order for determinism; manual TICK while stopped.
- **Trigger Offset Layer** (contract §2): per-engine, per-sourceId offset
  cells; hand-edit locks; generator stack (`walk`, `accent`, `contour`, `arp`,
  `scaleQuant`) writing at bar boundaries, skipping locked cells. This
  coexistence rule (hand edits are pins, generators flow around them) is THE
  mechanism — test it hard.
- **MIDI router** (contract §4): per-engine channel/note-map/gate/quantizeOut;
  input routing matrix → engine `MidiPerturbation`.
- **Trace system** (contract §6): PCG32 streams (one per engine slot + one for
  the offset layer) from a single project seed; JSONL TraceWriter drained
  off-thread from a ring buffer.
- **Plugin shell**: VST3 instrument (`kInstrumentSynth`) with internal fallback
  drum voices + MIDI out; AU MIDI-processor build for Logic.

## Invariants (the shell enforces the contract for every engine)
- Engines are called at latch boundaries only; `tick()` is RT-safe by
  contract — the shell must not create conditions that violate that (no
  locking the audio thread, SPSC queues for GUI→audio).
- Fixed engine order (slot index) → global determinism. Never reorder slots
  at runtime in a way that changes the RNG derivation.
- The offset layer never mutates engine state; it decorates the engine's
  `TriggerEvent`s downstream. Keep the engine→offset→router direction one-way.

## Core map (O1 — `shell/core/`, framework-free, ctest-gated)
Built core-first (DECISIONS #10): pure C++20, no JUCE, so the contract logic is
verifiable without a plugin host. The JUCE VST3/AU wrapper + `auval` land at O1b.
- `include/orrery/Contract.h` — **the IEngine seam** (§1): `IEngine`,
  `TickContext`, `GestureEvent`, `MidiPerturbation`, `Chunk`, `TraceWriter`.
  Frozen-for-Phase-1: no free-transport variant (#8), no pitch-native path (#9).
- `include/orrery/Types.h` — POD types crossing the seam (`TriggerEvent`,
  `OffsetCell`, clock/transport/latch/router structs, `kMaxSources = 32`).
- `include/orrery/Pcg32.h` — deterministic PCG32 (§6); one stream per slot +
  one for the offset layer, from a single project seed.
- `Clock.{h,cpp}` (§3) — pure latch math: per-block boundaries, sample-accurate
  offsets, BAR/HALF/STEP, `barsPerLap`, lap-phase→ppq→sample. No wall clock.
- `OffsetLayer.{h,cpp}` (§2) — **the coexistence mechanism**: cells + locks +
  `walk`/`accent` generators writing at bar boundaries, skipping locked cells;
  output resolution. `contour`/`arp`/`scaleQuant` are O5.
- `MidiRouter.{h,cpp}` (§4) — pure mapping (channel/note-map/gate/quantizeOut);
  MIDI byte emission + input matrix are O1b adapters.
- `Trace.{h,cpp}` (§6) — JSONL writer + parser; lossless round-trip.
- `StubEngine.{h,cpp}` — smallest real `IEngine`; proves the slot. Elastic
  Euclid replaces it at O2.
- `tests/` — the O1 Layer-0 gate (`./verify fast`): PCG32 reproducibility,
  clock ±1-sample timing sweep, offset lock/generator coexistence, trace
  round-trip, stub sourceId stability, whole-pipeline bit-identity + save/load.

**RT-safety is by construction here** (fixed arrays, no heap on the tick path).
The *enforceable* no-alloc gate (allocation hook / ASAN on the real audio
callback) is an O1b/O2 item — there is no `processBlock` to instrument yet.
Don't claim that gate green until it runs.

## Do not
Reach into an engine's internals (only the `IEngine` interface); emit MIDI
from an engine; change the contract without a DECISIONS entry + human gate.
