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

**RT-safety in the core is by construction** (fixed arrays, no heap on the tick
path). The *enforceable* no-alloc gate now exists in the plugin build (below).

## Plugin map (O1b — `shell/plugin/`, JUCE lives here and ONLY here)
JUCE 8.0.14 (FetchContent-pinned). `tools/check_core_boundary.py` fails the
build if any `juce`/`JUCE_*` token appears under `shell/core/`.
- `PluginProcessor.{h,cpp}` — wraps `orrery_core`. AudioPlayHead→`TransportState`
  adapter; APVTS params (gain/sources/rate/gate/quantize/walk/accent) → core
  config; **`renderBlock(buffer, midi, ts)`** is the RT-critical seam, transport-
  injected so the headless RT test drives it. Chain: clock latches → engine
  `tick` → offset generators → mini-scheduler (lap-phase → sample-accurate note
  on/off) → MIDI out + voices.
  **Transport rule:** host transport requires an actual ppq — a playhead alone
  isn't enough. No ppq (the JUCE standalone, some bridges) → the **internal
  free-run clock** takes over, gated by the `run` param (header RUN chip,
  default on) at 120 BPM; otherwise the plugin could never play there.
  **Scheduler hygiene:** pending notes are cleared (offs flushed, no stuck
  notes) on stop and on transport discontinuities (loop wrap / relocate);
  manual TICK evolves the pattern WITHOUT scheduling into a stalled timeline.
- `Lockfree.h` — SPSC ring (atomics, power-of-two, POD) for GUI→audio gestures
  and audio→drain trace. No alloc/lock on the audio thread.
- `Voices.h` — fixed-capacity fallback drum voices (per-voice seeded noise).
- `MidiOut.{h,cpp}` — **CoreMIDI virtual source "Orrery"** + drain thread. The
  Ableton routing path: Live can't route plugin-API MIDI to other tracks, so
  Orrery opens its own virtual port and mirrors notes there (audio thread → SPSC
  ring → `sendMessageNow`, ~1 ms, RT-safe). Notes also go on the plugin-API bus
  for hosts that route it. `internalAudio` param gates the voices. See
  `ROUTING.md`.
- `TraceDrain.{h,cpp}` — background `juce::Thread` popping POD trace records and
  formatting JSONL off the audio thread. **Stop it in BOTH `releaseResources()`
  and the destructor** — a host may destroy the processor without releasing, and
  a running `juce::Thread` must be stopped before deletion (auval caught this).
- `tests/test_rt_noalloc.cpp` — the **RT gate**: thread-local allocation hook
  asserts 0 heap allocs across steady-state `renderBlock`s (warm-up excluded).

## GUI map (O-GUI — `shell/plugin/gui/`, DECISIONS #16)
The visual system mirrors the engine seam. Rules that keep it mess-free:
- **`Theme.h` is the ONLY source of color/type/metric** (prototype palette
  verbatim) + one `OrreryLookAndFeel`. No ad-hoc colors in any component.
- **`IEngineView.h` is the GUI seam** — each engine owns ONE view in
  `engines/<name>/gui/` (JUCE allowed there: it's the organ's adapter zone,
  exempted in `check_core_boundary.py`; engine cores stay fully scanned).
  A view depends on IEngineView/Theme/Snapshot + its OWN engine only. Views
  COMPILE in the plugin target (the only JUCE build); add one line per view
  in this dir's CMakeLists.
- **Queue-only data flow** (`Snapshot.h`): audio→GUI = `GuiSnapshot` POD via
  wait-free `TripleBuffer` (published per block); GUI→audio = `GestureEvent` +
  `OffsetEdit` SPSC rings. The GUI NEVER touches engine/offset state directly.
- Chrome (engine-agnostic): `Chrome.{h,cpp}` (HeaderBar w/ transport chip +
  manual TICK + RUN, EngineTabs, RoutingBar), `ParamRail` (APVTS rows from a
  spec, with section headers), `OffsetLane` (drag-to-pin cells, locks, chips),
  `PluginEditor` (assembly + engine swap).
- **`EngineSlot.{h,cpp}`** hosts all three engines behind one façade (the
  multi-engine slot, DECISIONS #17). The shell is ALLOWED to know the concrete
  engine set (it instantiates them); per-engine branching (param apply,
  snapshot fill, sourceCount) is localized here, not scattered through the
  processor. The `engine` choice param selects the active one; each keeps its
  own state. Adding an engine = a case here + a view + rail rows in
  `PluginEditor`; the processor is untouched. Shared-for-now (refinements):
  offset layer + router + note-map are one instance across engines.

**Two host-crash traps (DECISIONS #20 — enforce in every view):**
1. **Never build a `juce::String` from a `const char*` literal with bytes > 127**
   (`·`, `Δ`, `✓`, `—`, any UTF-8). The `const char*` ctor treats them as
   Latin-1 and `jassert`s (juce_String.cpp:327). Use ASCII separators; for a
   kept glyph, `juce::String::fromUTF8(...)`.
2. **Distribute RELEASE builds only.** A Debug plugin's fired `jassert`
   debug-traps (SIGTRAP) → the host kills it. Install from `build-release/`
   (`-DCMAKE_BUILD_TYPE=Release`); keep a Debug `build-plugin/` for pluginval's
   assertion coverage. Validate the shipping build with `pluginval`
   (`/Applications/pluginval.app/…/pluginval --strictness-level 8 --validate`),
   not just auval — it caught both of these.

Build/validate (machine-local, human-run — global CLAUDE.md gotchas): the
codesign re-seal is a CMake `POST_BUILD` (JUCE regenerates `moduleinfo.json`
after signing → broken seal → DAW silently skips it). `auval`/install to
`~/Library` run via `tools/validate_au.sh` in a real terminal. `./verify full`
builds the plugin + runs all ctests; set `ORRERY_JUCE_DIR` to reuse a cached
JUCE checkout offline.

## Do not
Reach into an engine's internals (only the `IEngine` interface); emit MIDI
from an engine; change the contract without a DECISIONS entry + human gate.
