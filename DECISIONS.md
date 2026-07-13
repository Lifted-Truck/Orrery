# Decisions on record (append-only)

Each entry: the decision, the alternative rejected, and why. Never rewrite
history; supersede with a new numbered entry.

1. **Contract precedence: architecture doc over elastic spec's shell sections**
   (2026-07-12, from the specs themselves). `sequencer-studio-architecture.md`
   is THE contract. `elastic-euclid-spec.md`'s model/physics/clocking/
   determinism/test sections stand; its plugin-shell sections (§3 format/
   threading, §4 MIDI mapping, per-particle note table) are superseded. Build
   the shell from the contract. Rejected: treating the older elastic spec as
   authoritative for the shell — it would produce a per-engine plugin, not the
   studio.
2. **Architecture rung 2 → 3, escalated at the O2 gate** (2026-07-12, human
   at O0). Phase 1 single-threaded: one lead builds shell + Elastic to PROVE
   the `IEngine` seam against a real engine. At O2 green, remaining engines
   parallelize as organs (own territory + verify gate, merged via the shell
   contract). Rejected: rung 3 from day one (bets the contract is right before
   any engine exercises it; one shared build/audio-thread makes isolation
   partial) and rung 2 only (undersells the independently-verifiable engine
   structure the specs already have). Escalate only when the current rung is
   the demonstrated bottleneck.
3. **Orrery is a Tonality CONSUMER** (2026-07-12). scaleQuant + pitch
   assignment go through the Tonality JSON contract via ONE boundary module;
   pin the contract version; degrade visibly to a static note map when
   Tonality is absent. Brief filed at `integrations/tonality/brief.md`.
   Rejected: reimplementing scale/pitch logic locally (violates
   never-reimplement-the-provider's-core).
4. **Coupled Rings is spec-gapped; build is blocked on writing the spec**
   (2026-07-12). Only a prototype exists (`coupled-rings.html`). O4a (write
   `coupled-rings-spec.md` from the prototype, human-reviewed) precedes any
   implementation. Rejected: reverse-engineering the C++ straight from the
   prototype — the other engines earned their validated specs first; this one
   should too.
6. **Engines are territories in ONE repo (monorepo), not separate repos**
   (2026-07-12, human question). The engines compile into one plugin binary,
   share the audio thread, and share the `IEngine` contract as source (not a
   versioned package) — they are not independently deployable, so there is
   nothing to release or version separately. A territory is a subdirectory
   with an enforced write boundary (the intra-repo organ model, DESIGN §2-3),
   which is exactly this. Rejected: repo-per-engine — would force the freely-
   changing contract into a semver'd cross-repo dependency (the full
   INTEGRATIONS linked-PR protocol) for zero benefit. Separate repos are for
   independent products with cross-repo contracts (Tonality ↔ consumers), not
   modules of one build. New engines land as new top-level dirs + a manifest
   territory entry + a sub-charter; `./verify` flags any unregistered dir.

7. **Engines collected under `engines/`** (2026-07-13, human). All sequencer
   engine territories moved to `engines/<name>/`; `shell/` (the host) and
   umbrella infra stay at root. Makes ingest of new engines mechanical (drop
   into `engines/`, verify flags it until registered) and separates engine
   territories from infrastructure cleanly. Reorg only — no engine internals
   changed. Manifest gains `composite.engines_dir`.
8. **Kuramoto Rotors requires a free-transport contract variant — build
   blocked** (2026-07-13, ingest finding). Its spec needs
   `IFreeTransportEngine` (integrate at fixed h, emit sample-accurate triggers
   per block; no tick/latch), which the contract §5 does not define. This is a
   contract-change proposal: add the variant to
   `sequencer-studio-architecture.md` §5 (human-gated) BEFORE building the
   engine. Rejected: forcing Kuramoto into the tick/latch IEngine — its
   musical value is continuous rotation, which latching destroys.
9. **Torus Euclid is the first pitch-generating engine — needs a contract
   note** (2026-07-13, ingest finding). The contract's output resolution
   (§2.4: note-map assigns pitch to sourceId) assumes trigger-only engines;
   a pitch-native engine emits scale-degree rows. Resolve how native pitch
   composes with the note-map and `scaleQuant` (Tonality boundary) in the
   contract before wiring Torus's pitch path. Not a blocker for its rhythm
   layout, only its pitch output.

5. **Composite-project scaffold shape** (2026-07-12). Orrery is scaffolded as
   an umbrella (root charter + contract + integrations + knowledge loop) with
   contract-bound sub-territories (shell + engines), each a mini-project with
   its own CLAUDE.md whose §Domain is that module's spec. This is the first
   composite scaffold in the ecosystem; if it holds, abstract it into a
   reusable procedure (autonomous kit — the "compose" variant of spinup).

10. **O0 ratified; O1 shell built core-first (framework-free + ctest before the
    JUCE/auval dance)** (2026-07-13, human at O0). The manifest, the rung-2→3
    path (DECISIONS #2), and `sequencer-studio-architecture.md` are ratified and
    frozen-for-Phase-1 — no §5 free-transport variant and no §2.4 pitch-native
    resolution until each is separately human-gated (both booked post-O2).
    Fixed two off-by-one manifest cross-refs found at ratification (torus
    #8→#9, kuramoto #7→#8). O1 builds the shell as a **pure C++20 core** (clock
    math, offset layer, MIDI-router mapping, trace, PCG32) gated by deterministic
    `ctest`; the JUCE VST3/AU wrapper + codesign-seal + `auval` land as a
    follow-on (O1b) per the machine-local build gotchas. Rejected: building the
    JUCE plugin shell up front — the contract logic (latch timing, lock/generator
    coexistence, trace round-trip, bit-identical determinism) is verifiable
    without a plugin host, and the framework-free-core doctrine keeps UI/IO/time
    in thin adapters. Time/IO/threading (AudioPlayHead read, MIDI bytes, ring-
    buffer drain) stay out of the core, behind adapter seams O1b fills in.

11. **O1b plugin shape: JUCE 8.0.14, generic editor, renderBlock seam,
    tick-per-lap scheduling** (2026-07-13). The `shell/plugin/` wrapper pins
    JUCE 8.0.14 (same commit AURICLE pins — a sibling's cache reuses offline via
    `ORRERY_JUCE_DIR`). Three deliberate O1b scopings: (a) **generic editor**
    (`GenericAudioProcessorEditor` over APVTS params) — a custom GUI is a later
    phase; APVTS also gives automatable params + host state for free. (b) The
    RT-critical work lives in **`renderBlock(buffer, midi, ts)`**, transport-
    injected, so a headless console app drives the exact audio path under an
    allocation hook — the enforceable no-alloc gate, not a claim. (c) The plugin
    **latches once per scheduling lap** (lap = the core clock's latch interval;
    engine `barPhase` tiles one interval, consecutive latches abut) to avoid
    lap/division double-scheduling with the whole-lap-emitting stub; finer per-
    engine divisions wire in when a real engine needs them. Rejected: a custom
    GUI now (premature — no engine to visualize); testing RT-safety by
    inspection only (the core's by-construction claim needs an instrumented
    audio callback to be a gate). Contract UNCHANGED — all of this is adapter
    work in `shell/plugin/`; the frozen `IEngine` seam held against a real host.
    Two bugs the gates caught: JUCE's post-sign `moduleinfo.json` regeneration
    breaks the seal (fixed by a `POST_BUILD` `codesign --force -s -`, else the
    DAW silently skips the plugin), and a `juce::Thread` destroyed without a
    prior `releaseResources()` asserts (fixed by stopping the drain in the
    destructor too — surfaced by `auval`).
