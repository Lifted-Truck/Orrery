# SEQUENCER STUDIO — Shared Architecture

**Working title:** ORRERY (a mechanical model of multiple orbital systems — rename freely).
**Purpose:** A single VST hosting multiple generative sequencer *engines* (Elastic Euclid, Measured Euclid, Coupled Rings, and future engines) behind one transport, one trigger-decoration layer, one MIDI router, and one trace system. This document defines the contracts; engine specs (`elastic-euclid-spec.md`, `measured-euclid-spec.md`, …) define engine internals only.

**Note on precedence:** `elastic-euclid-spec.md` was written before this document as a standalone plugin spec. Its model, physics, clocking, determinism, and test sections stand; its plugin-shell sections (§3 format/threading, §4 MIDI mapping, per-particle note table) are superseded by this document. Elastic Euclid's `sourceId` = particle id.

---

## 1. Core abstractions

### 1.1 IEngine
```cpp
struct TriggerEvent {
  double  barPhase;    // [0,1) position within the lap
  int32   sourceId;    // stable identity within this engine (see 1.2)
  float   energy;      // [0,1] engine-defined intensity signal
};

class IEngine {
  // Called at latch boundaries only (bar / half / step per engine clock config).
  // Advances the engine one generation and returns the latched event set for
  // the next division. MUST be deterministic given state + seed. RT-safe.
  virtual void tick(TickContext&) = 0;
  virtual std::span<const TriggerEvent> latchedEvents() const = 0;

  virtual void handleGesture(const GestureEvent&) = 0;  // kick, drag, add/remove, curve edit
  virtual void handleMidiIn(const MidiPerturbation&) = 0;
  virtual void saveState(Chunk&) const = 0;  virtual void loadState(const Chunk&) = 0;
  virtual void writeTrace(TraceWriter&) const = 0;
};
```
Engines never emit MIDI, never read wall-clock, never allocate in `tick()`. Fixed-capacity storage (max 32 sources per engine).

### 1.2 Trigger identity (`sourceId`)
The contract that makes per-trigger offsets meaningful: each engine must define a sourceId that is **stable across generations and parameter changes short of changing k**.
- Elastic Euclid: particle id (already stable; LIFO removal preserves survivors).
- Measured Euclid: μ-space onset index.
- Coupled Rings: (ring, particle id) packed.
When k grows, new ids appear with default (zero) offsets; when k shrinks, offsets for vanished ids are retained in state (grayed in GUI) so re-adding restores them.

## 2. Trigger Offset Layer (the decoration stage)

Sits between every engine and the MIDI router. Per engine instance, per sourceId:

```
OffsetCell {
  int8   transpose;    // −24..+24 semitones
  int8   velOffset;    // −64..+64
  bool   lockT, lockV; // hand-edit locks (see 2.2)
}
```

### 2.1 Hand editing
GUI lane per engine: one row per sourceId, showing the cell's current transpose (vertical drag) and velocity offset (modifier-drag or second sub-row). Double-click resets. Editing a cell sets its lock flag.

### 2.2 Generative editing — coexistence rule
Generators write into offset cells **at bar boundaries** (same latch philosophy), but **skip locked cells**. This is the whole coexistence mechanism: hand edits are pins; generators flow around them. "Unlock all" and per-cell unlock gestures return cells to generator control. Lock state is saved.

### 2.3 Generators (v1 set, all seeded, all latched per bar)
| generator | targets | behavior |
|---|---|---|
| `walk` | T and/or V | bounded random walk per cell; params: step, range, rate (bars per move) |
| `contour` | V | map engine `energy` → velocity offset via depth/curve params (e.g., elastic kinetic accents, measured density accents) |
| `accent` | V | Euclidean accent pattern E(a, k) over sourceIds in id order — accents distributed maximally evenly across the *voices* |
| `arp` | T | cyclic interval sequence over sourceIds (e.g., +0,+3,+7,+12), rotate per bar option |
| `scaleQuant` | T (post) | quantize resulting pitch to scale via **Tonality JSON contract**; applied after all other T generators |

Generator stack per engine: ordered list, each with wet amount; deterministic evaluation order. Trace records generator outputs per bar.

### 2.4 Output resolution
```
pitch    = engineBaseNote(sourceId) + transpose      → scaleQuant if enabled
velocity = clamp(baseVel + velOffset + contour(energy))
```
`engineBaseNote`: per-engine editable note map (default: stacked spread as in prototypes).

## 3. Clock service

- Single transport authority derived from `AudioPlayHead` (ppq, time sig, isPlaying). One lap = `barsPerLap` bars (0.25..4, per engine).
- Emits latch callbacks at each engine's configured division (BAR / HALF / STEP / FREE-legacy) at **sample-accurate offsets within the audio block**; engines tick synchronously in a fixed engine order (slot index) for determinism.
- Manual TICK per engine while stopped.

## 4. MIDI router

- Per engine: output channel, note map, gate length, `quantizeOut` (0..1 blend of emitted timing to nearest 1/n — physics/measure untouched).
- MIDI input routing matrix: incoming channel/note-range → engine perturbation (each engine defines its `MidiPerturbation` semantics; e.g., elastic = kick impulse on matched particle).
- Ship as VST3 instrument with internal fallback drum voices (port prototype voices, one timbre preset per engine) + MIDI out; AU MIDI-processor build for Logic. (Rationale in elastic spec §3, still applies.)

## 5. Threading & RT safety

Identical to elastic spec §3.1, generalized: all engine `tick()`s on audio thread at boundary offsets (cheap by contract); lock-free snapshot per engine for GUI; SPSC gesture queue GUI→audio drained at block start; offset-cell edits travel the same queue.

## 6. Determinism & trace

- One PCG32 stream **per engine slot** plus one for the offset layer, all derived from a single saved project seed. Engine order fixed → global determinism.
- TraceWriter: JSONL, one record per (engine, generation) plus offset-layer records; drained off-thread from a ring buffer. A trace + project state fully reconstructs a performance (wend-compatible philosophy).

## 7. Project state & routing between engines (v2 horizon)

State chunk: version, seed, per-slot engine type + engine state + offset cells + generator stacks + routing. Versioned from day one.

Design the slot model so **engine→engine signals** are not a rewrite: engines may expose named output signals (elastic: particle positions/energies; measured: w(t)) and accept named modulation inputs (measured curve → elastic lattice well depths; ring A positions → ring B potential). v1 ships without cross-patching but with the port structure in place.

## 8. Build order recommendation

1. Studio shell: clock service, one engine slot, offset layer with `walk` + `accent` + hand editing, MIDI router, trace.
2. Elastic Euclid engine (spec exists, model validated).
3. Measured Euclid engine (spec exists, model validated).
4. Coupled Rings engine (prototype in progress).
5. Generator set completion (`contour`, `arp`, `scaleQuant` w/ Tonality contract).
6. Cross-engine modulation ports.
