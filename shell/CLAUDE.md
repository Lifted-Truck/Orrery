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

## Do not
Reach into an engine's internals (only the `IEngine` interface); emit MIDI
from an engine; change the contract without a DECISIONS entry + human gate.
