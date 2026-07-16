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

12. **O2: Elastic Euclid implemented; rung-2→3 escalation fired; equilibrium
    gate characterized honestly** (2026-07-13). `engines/elastic-euclid/`
    implements `IEngine` with physics matched to `elastic-euclid-2.html`
    (K=rep·0.004, A=lat·3, c=damp·8, ε=4e-4, semi-implicit Euler, SUB=24,
    double). Two contract-shaping choices: (a) **sourceId = particle array
    index** (dense, LIFO), not the prototype's monotonic display id — the
    offset layer needs ids in [0,32) and array identity preserves the LIFO-
    survivor invariant (§1.2); (b) the engine **owns its PCG32** (seeded per
    slot by the shell via `seed()`) because gesture-time randomness (kick/add-
    jitter) has no `TickContext`. With O2's gate green, the **rung-2→3
    escalation trigger fires** (DECISIONS #2): the seam is proven by a real
    engine; remaining engines parallelize as organs.
    **Equilibrium gate — epistemic-discipline note.** First cut of §8.1 passed
    18/18 and I was suspicious of the comfortable result; it was TRIVIAL (jitter
    < half a lattice cell → the start already quantized correctly). Five build-
    time probe sweeps then established the real behavior: E(k,n) is a stable
    *fixed point* (exact starts hold it, 18/18) but only a *local* attractor —
    within-basin perturbations (≲0.4 cell, no kick) recover (72/72), but beyond
    ~0.5 cell / with kicks the system falls into metastable non-Euclid minima
    (recovery ~50–90%); convergence completes by ~200 ticks or never; the
    settled state sits ~0.016 RMS off exact cells (E(k,n) is maximally, not
    perfectly, even). The gate asserts the two GUARANTEED properties (stability
    + within-basin recovery) non-trivially, and the basin limit is documented
    in the engine CLAUDE.md — NOT weakened away. §8.1's word "adjacent" is
    load-bearing; global convergence from arbitrary starts is not a property of
    this model. Rejected: keeping the trivial 18/18 test (would have "passed"
    while testing nothing); asserting global convergence (false — the traps are
    real); tuning params to force convergence (would change the validated
    prototype's behavior).

13. **O3: Measured Euclid implemented (organ); tie-robust round-half-up fixes
    Euclid recovery** (2026-07-13). `engines/measured-euclid/` implements
    `IEngine`: measure w[512], piecewise-linear CDF + binary-search invCDF
    (matched to `measured-euclid.html`), onsets at equal accumulated-measure
    intervals, quantize-blend, breathe/morph latched per bar. sourceId=μ-index.
    Spec §6 acceptance tests green (Euclid recovery 496/496, inverse accuracy
    <1e-6, latch invariant, monotone deformation, determinism). **Correctness
    fix:** the naive `round(t·n)` quantize failed Euclid recovery for all 9
    gcd(k,n)>1 cases because floating-point sends exact half-integer ties DOWN
    (0.3·15 = 4.4999…982 → 4, not the spec-mandated round-half-UP to 5). A +1e-9
    rounding bias (≫ fp error, ≪ any real gap) restores it → 496/496. This is a
    real engine fix (the plugin would emit wrong Euclidean patterns), grounded
    in spec §6.1's explicit "round-half-up ties" requirement — not test-tuning.
    Rejected: weakening the recovery test to skip gcd>1 (would ship the bug).

14. **O3: Probable Euclid implemented (organ); two honest recalibrations + a
    filed contract gap** (2026-07-13). `engines/probable-euclid/` implements
    `IEngine`: greedy farthest-point evenness, clump blend, logistic field,
    per-bar PCG32 Bernoulli (spec §2.4 — PCG32, not the prototype's mulberry32;
    only determinism is contractual). §6 gates green: backbone recovery 522/522,
    determinism, freeze invariant. Two acceptance tests were RECALIBRATED to the
    measured truth (the spec §6.2 explicitly says "calibrate; a regression
    floor," and §6.3's blanket claim over-reaches): (a) prefix-evenness floor
    0.97→**0.79** — the greedy nested family degrades to 0.80× Bjorklund at small
    n (E(3,5): clustered {0,1,2} vs even {0,2,4}, the documented §2.1
    nestedness trade-off, matches the prototype); (b) "Σp_i≈d for all τ∈[0,1],
    c∈[0,1]" is FALSE — it holds EXACTLY (1e-16) only at τ→0 & pure evenness;
    clump and high temperature intentionally decouple count from density
    (envelope ≈4.5 at τ=1). The gate now asserts the exact calibration + a
    non-decreasing density response, and characterizes the decoupling.
    **CONTRACT GAP (filed):** Probable's `sourceId` = grid step index needs
    n∈[4,64] (spec §3), but the offset layer's fixed capacity is
    `kMaxSources`=32. **n is capped at 32 for Phase 1**; reaching 64 requires
    widening the offset-layer capacity to 64 — a contract-version event (human-
    gated), deferred. Degrade-visibly, don't silently violate (INTEGRATIONS
    rule 2 / make-gaps-visible). Rejected: silently truncating n>32 (data loss);
    compacting sourceId to onset-rank (breaks "step 7 is always +5", spec §2.5);
    unilaterally bumping kMaxSources (frozen contract, affects every engine).

    Both engines land as ORGANS (own territory + acceptance-test verify gate,
    merged via the contract) — the rung-3 model in practice. The plugin slot
    stays Elastic; per-engine param routing + engine selection in the shell is
    a later phase (the generic-param plumbing doesn't yet cover 3 param sets).

15. **MIDI-out routing: dual path (plugin-API bus + CoreMIDI virtual port) with
    an internal-audio toggle** (2026-07-13, human — "needs to output MIDI to
    other tracks, but I like the audio too"; primary host Ableton). Orrery emits
    notes on BOTH the plugin-API event bus (sample-accurate; Reaper/Bitwig/
    Cubase route it) AND a self-opened CoreMIDI virtual source named "Orrery"
    (the ONLY way to feed other **Ableton Live** tracks — Live's MIDI From taps
    before the instrument and cannot capture plugin-generated MIDI; documented
    Ableton workaround is a virtual MIDI bus). Audio-thread → SPSC ring → drain
    thread → `MidiOutput::sendMessageNow` (RT-safe, ~1 ms; reuses the trace-drain
    pattern). An `internalAudio` APVTS bool (default on) gates the fallback
    voices so Orrery can be a silent MIDI generator OR also sound. See
    `shell/plugin/ROUTING.md`. RT no-alloc gate still green (0 allocs/4000
    blocks — the ring push is alloc-free); AU VALIDATION SUCCEEDED. Rejected:
    plugin-API MIDI-out only (invisible to Live, the primary host); sending
    CoreMIDI from the audio thread (RT risk); a Logic `aumi` build now (deferred
    — roadmap). Known: auval prints a benign debug-only `MessageManager` leak
    (the CoreMIDI subsystem instantiates JUCE's message-thread singleton, which
    auval's minimal host doesn't tear down; real DAWs own it — no leak there).

16. **Visual system: design-token file + IEngineView seam; engine views live in
    their territory's gui/ (sanctioned boundary exemption)** (2026-07-13, human
    re-prioritization: "modular, cleanly structured, aesthetic visual system…
    jumped up in the roadmap"; aesthetic approved via mockup = match the
    prototypes). Architecture mirrors the proven engine seam:
    - `shell/plugin/gui/Theme.h` — ALL color/type/metric tokens (prototype
      palette verbatim); one `OrreryLookAndFeel`; no ad-hoc colors anywhere.
    - `IEngineView` (shell/plugin/gui/) — the GUI analog of `IEngine`: each
      engine territory owns ONE view in `engines/<name>/gui/`, depending only on
      the seam headers (IEngineView/Theme/Snapshot) + its own engine; never on
      the shell's internals or a sibling view. First instance:
      `engines/elastic-euclid/gui/ElasticView` (ring/ghosts/whiskers/playhead).
    - Data flow is queue-only: audio→GUI via a wait-free `TripleBuffer`
      publishing a POD `GuiSnapshot` per block; GUI→audio via SPSC rings
      (`GestureEvent` for engine gestures, new `OffsetEdit` for offset-lane hand
      edits — contract §5's queue philosophy). The GUI never touches model
      state; RT no-alloc gate stayed green (0 allocs/4000 blocks).
    - **Boundary-gate change (gated file, flagged):** `check_core_boundary.py`
      now exempts `engines/<name>/gui/` — the view is JUCE by nature and is the
      organ's ADAPTER zone; engine cores (src/include/tests) remain fully
      scanned, shell/core keeps zero exemptions. Views compile in the plugin
      target (only place JUCE exists); ownership stays in the territory.
    Rejected: views inside shell/plugin (breaks organ ownership — every engine
    PR would touch the shell); a per-engine JUCE dependency in engine CMake
    (drags JUCE into the core build path); GUI reading engine state directly
    (races; violates the snapshot isolation that keeps this maintainable).
