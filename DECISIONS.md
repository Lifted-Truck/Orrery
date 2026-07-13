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
5. **Composite-project scaffold shape** (2026-07-12). Orrery is scaffolded as
   an umbrella (root charter + contract + integrations + knowledge loop) with
   contract-bound sub-territories (shell + engines), each a mini-project with
   its own CLAUDE.md whose §Domain is that module's spec. This is the first
   composite scaffold in the ecosystem; if it holds, abstract it into a
   reusable procedure (autonomous kit — the "compose" variant of spinup).
