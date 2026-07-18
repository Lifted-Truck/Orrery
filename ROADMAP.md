# Orrery — ROADMAP

> **Single source of truth for direction AND phase status** (DECISIONS #18:
> the manifest carries structure only — status is never duplicated there).
> Phase gates are never weakened to pass. Derived from
> `sequencer-studio-architecture.md` §8 build order, with the rung-2→3
> escalation made explicit. The contract doc defines *what*; this defines
> *in what order, gated by what, and where we are*.

## Re-prioritization (2026-07-13, human)
Two items jumped up ahead of further engines — establish structure before it
sprawls:
- **O-GUI — Modular visual system.** ✅ **DONE 2026-07-13** (DECISIONS #16, #17):
  mockup approved → `shell/plugin/gui/` (Theme tokens, LookAndFeel,
  `IEngineView` seam, `GuiSnapshot` triple-buffer, `OffsetEdit` ring, chrome) +
  all three engine-owned views (Elastic ring, Measured density+onset lanes,
  Probable radial field). **Multi-engine slot (`EngineSlot`) — all three
  engines now playable + visible**, tabs switch the live engine + swap view +
  param rail. Voices are the prototype's pitched percussion with a VOICE
  control section; internal transport lets the standalone free-run. RT no-alloc
  green; auval SUCCEEDED. Since landed: playheads on all views, Measured curve
  painting + bezier/steps draw modes + parameterized presets (slope/cycles/
  subdiv), global FREEZE (Elastic physics-hold + Probable realization-hold),
  note SPREAD toggle, global MIDI-out transpose. *Remaining polish: per-engine
  offset state (currently shared), m_breathe bool in the rail, lattice-pull-
  scaled ticks, resize persistence, note-map editor.*
- **O-sync — Tempo/transport tightening.** Host-clock generation already follows
  Ableton (bpm/ppq/time-sig/isPlaying → bar-grid latches). Remaining: timestamp
  the CoreMIDI virtual-port sends so Ableton-routed notes land beat-tight (not
  ~1 ms drain-jittered), + transport loop/reposition/count-in verification.

## Build sequence (phase-gated)

### Rung 2 — prove the seam (single-threaded lead)

- **O0 — Scaffold + contract ratification.** Repo, charter, sub-territory
  CLAUDE.md, manifest, Tonality brief filed. *Gate: human ratifies the
  manifest, the rung path, and the contract as frozen-for-Phase-1.*
  **✅ RATIFIED 2026-07-13 (DECISIONS #10).**
- **O1 — Studio shell (core).** ✅ **DONE 2026-07-13** — built core-first
  (DECISIONS #10): `shell/core/` framework-free C++20 — clock service
  (host-synced latch math, sample-accurate offsets, BAR/HALF/STEP, `barsPerLap`),
  one engine slot proven by `StubEngine`, offset layer with `walk` + `accent` +
  hand-edit locks, MIDI-router mapping, JSONL trace (writer + parser). *Gate met
  (`./verify fast`): offset lock/generator coexistence unit-tested, trace
  round-trips, whole-pipeline determinism bit-identical, clock ±1-sample across
  a rate/tempo/division sweep, stub sourceId stability (§1.2). 6/6 ctests green.*
- **O1c — MIDI-out routing + audio toggle.** ✅ **DONE 2026-07-13.** Dual MIDI
  out — plugin-API event bus (Reaper/Bitwig/…) + a self-opened CoreMIDI virtual
  source "Orrery" (the Ableton path; Live can't route plugin-API MIDI). RT-safe
  ring→drain send. `internalAudio` toggle makes it a silent MIDI generator or an
  audible instrument. `shell/plugin/ROUTING.md`; DECISIONS #15. *Follow-up: a
  Logic `aumi` MIDI-processor build; per-instance port naming.*
- **O1b — Plugin wrapper + RT gate.** ✅ **DONE 2026-07-13.** `shell/plugin/`
  wraps `orrery_core` in a JUCE 8.0.14 VST3/AU/Standalone instrument:
  AudioPlayHead→TransportState adapter, `renderBlock` seam, mini-scheduler
  (lap-phase → sample-accurate note on/off), SPSC gesture queue, POD trace ring
  + off-thread JSONL drain, MIDI in→perturbation / out, APVTS params (generic
  editor), fallback drum voices, small-chunk state. *Gate met: VST3 + AU +
  Standalone build with codesign seal re-sealed after JUCE's moduleinfo
  regeneration (`codesign --verify --deep` VALID); **`auval -v aumu Orry Lftk`
  → AU VALIDATION SUCCEEDED**; RT no-alloc gate green (0 heap allocs across
  4000 `renderBlock`s under a thread-local allocation hook, `test_rt_noalloc`);
  core-boundary gate green (no JUCE in `shell/core`). `./verify full` runs the
  whole plugin build + ctests; `auval`/install are human-run via
  `tools/validate_au.sh`.* **← here; O2 is next.**
- **O2 — Elastic Euclid engine.** ✅ **DONE 2026-07-13.** `engines/elastic-euclid/`
  implements the validated spec against `IEngine` (physics matched to
  `elastic-euclid-2.html`). *Gate met: §8.1 equilibrium — E(k,n) is a stable
  fixed point (18/18) and within-basin perturbations recover to it (72/72),
  non-trivially (distinct off-equilibrium starts → same Euclidean attractor +
  demonstrated migration); §8.2 determinism bit-identity (seed+gesture script,
  + save/load); §8.5 k-change latch invariant; §8.3/§8.4 timing + no-alloc are
  the shell gates (clock ±1-sample ctest, RT no-alloc harness now drives Elastic
  — 0 allocs/4000 blocks). Basin is finite — recovery beyond ~0.5-cell
  displacement / with kicks is NOT claimed (measured + documented in the engine
  CLAUDE.md; §8.1's "adjacent" is load-bearing). AU VALIDATION SUCCEEDED with
  Elastic in the slot.* **← RUNG-2→3 ESCALATION TRIGGER FIRED: the IEngine seam
  is proven by a real engine. Remaining engines may now parallelize as organs.**

### Rung 3 — parallelize engines as organs (✅ EARNED at O2, 2026-07-13)

> The seam is proven. Each remaining engine is now an independent organ:
> `engines/<name>/` implementing `IEngine`, its own acceptance-test ctests as
> the verify gate, merged through the shell contract. The elastic engine is the
> template (physics core + spec §8 tests + CLAUDE.md basin notes).

- **O3 — Measured Euclid engine** (organ; territory `measured-euclid/`).
  ✅ **DONE 2026-07-13.** *Gate met: §6 tests green — Euclid recovery 496/496
  (all k<n≤32, via a tie-robust round-half-up fix, DECISIONS #13), inverse
  accuracy <1e-6, latch invariant, monotone deformation, determinism.*
- **O4 — Additional engines** (organs, parallelizable once O2 proves the seam;
  each in `engines/<name>/` with its own verify gate). Build order among these
  is TBD after O2 — the user has landed a pool of validated engines; prioritize
  at O2. Each gate = that engine's spec acceptance tests green + matches its
  prototype (some carry explicit divergence anchors: torus L=0 reproduces the
  prototype layout; kuramoto honest-retrograde replaces the prototype floor).
  - **probable-euclid** — ✅ **DONE 2026-07-13** (organ). §6 gates green:
    backbone recovery 522/522, determinism, freeze invariant; evenness floor +
    expected-count claims recalibrated to measured truth (DECISIONS #14).
    **n capped at 32** (Phase-1: sourceId=step must fit the 32-cell offset
    layer; n=64 needs an offset-capacity contract change — filed).
  - **torus-euclid** — standard IEngine; ready, BUT its pitch output waits on
    the pitch/note-map contract note (DECISIONS #9). Rhythm layout unblocked.
  - **kuramoto-rotors** — **BLOCKED on a contract change**: needs
    `IFreeTransportEngine` added to the contract §5 first (DECISIONS #8).
  - **coupled-rings** — **BLOCKED on spec**: O4a writes
    `engines/coupled-rings/coupled-rings-spec.md` from the prototype
    (human-reviewed) before build.
- **O5 — Generator set completion.** `contour`, `arp`, `scaleQuant`
  (scaleQuant requires the Tonality boundary — O-int below). *Gate: each
  generator seeded, latched per bar, deterministic order, trace-recorded;
  locked-cell skip verified.*
- **O6 — Cross-engine modulation ports (v2 horizon).** Named output signals /
  modulation inputs per the contract §7; v1 ships the port structure, v2 wires
  patches (measured w(t) → elastic well depths; ring A → ring B). *Gate:
  design-only in v1 — the slot/port model exists and is state-versioned.*

### Cross-cutting

- **O-int — Tonality integration.** Boundary module for the scaleQuant/pitch
  JSON contract; pin the version; degrade visibly (static note map fallback
  when Tonality absent). Blocks the `scaleQuant` generator only. See
  `integrations/tonality/brief.md`.

## Decisions on record
See DECISIONS.md (precedence ruling, rung path, Tonality consumer, Coupled
Rings spec gap).

## Target consumers / applications
The plugin itself (the musician). Trace output is wend-compatible (a trace +
state reconstructs a performance) — potential future consumer of Orrery traces.

## Deferred / demoted
- FREE clock mode (continuous evolution) — kept only as a legacy/comparison
  mode; "usually not what you want" (elastic spec §2.4).
- Per-particle mass, asymmetric/pinned particles, audio-derived measures,
  bifurcation presets — engine roadmaps, post-v1.
