# CLAUDE.md — Orrery (Sequencer Studio)

Root charter for a **composite project**: one VST plugin hosting multiple
generative sequencer *engines* behind shared contracts. Read this, then the
contract (`sequencer-studio-architecture.md`), then the sub-module CLAUDE.md
for the territory you're working in.

## What this is
A single JUCE/C++20 audio plugin (VST3 + AU + CLAP-if-trivial) — the "studio
shell" — that hosts N engines through one `IEngine` contract, one trigger-
offset decoration layer, one clock service, one MIDI router, and one trace
system. Engines are deterministic iterated maps clocked by musical time; a
trace + project state fully reconstructs a performance.

## The layer map (where things live)
- **`sequencer-studio-architecture.md` — THE CONTRACT.** `IEngine`,
  `TriggerEvent`, `OffsetCell`, clock, router, trace, determinism. Every
  engine imports this seam and nothing else of its neighbors. This is the
  organ boundary; treat a change to it as a contract-version event.
- **`shell/`** — the studio host (clock, offset layer, router, trace, plugin
  shell). Owns the contract's *implementation*.
- **`engines/<name>/`** — engine territories (all sequencer engines live under
  `engines/`). Each has its own spec (source of truth for its internals), a
  validated prototype (reference oracle), and its own acceptance tests. An
  engine may ONLY depend on the contract, never on another engine's internals.
  Current: **elastic-euclid (IMPLEMENTED — O2, `IEngine` + spec §8 tests green,
  in the plugin slot)**; measured-euclid, probable-euclid, torus-euclid
  (spec validated, not yet built); kuramoto-rotors (needs the free-transport
  contract variant); coupled-rings (spec gap). `engines/_template/` is the
  intake template.
- **`integrations/tonality/`** — the Tonality consumer boundary (scaleQuant +
  pitch via the Tonality JSON contract). One boundary module; pin the version;
  degrade visibly.

## Architecture: rung 2 → 3 (earned, not assumed)
Phase 1 is **single-threaded** (one lead builds shell + Elastic Euclid) to
PROVE the `IEngine` seam is real against a working engine. Once that gate is
green, remaining engines parallelize as **organs** (rung 3): each engine a
territory with its own verify gate, merged through the shell contract. Do not
start parallel engine work before the seam is proven — that is the whole point
of escalating only when the current rung is the demonstrated bottleneck.
See ROADMAP.md.

## Doctrine (this project is deterministic-core by nature)
- **AI/deterministic boundary.** The physics/measure/onset math is exact,
  seeded, wall-clock-free. AI may propose presets, curves, or parameter
  regimes for human review; AI never emits a trigger time or pitch by
  judgment. Enforced by: seeded PCG32 per engine slot, fixed substep counts,
  no `time.now()` / RNG-without-seed on any model path.
- **Oracle discipline.** Each engine's spec already defines acceptance tests
  (equilibrium/Euclid recovery, determinism bit-identity, ±1-sample timing,
  RT-safety no-alloc, latch invariants). Those are Layer-0. The validated
  HTML prototype is the behavioral reference (Layer-E). Gates are never
  weakened to pass; refreezing a golden without reviewing the diff is
  weakening.
- **Reduce, never invent.** Every computed value carries a trace (formula +
  inputs) that reproduces it; the trace system is not optional instrumentation,
  it is the correctness substrate.
- **RT safety by construction.** Engines never allocate/lock/log/read-wall-
  clock in `tick()`; fixed-capacity storage (≤32 sources/engine).

## Precedence (documentation coherence — do not trip this)
`elastic-euclid/elastic-euclid-spec.md` predates the contract. Its **model,
physics, clocking, determinism, and test sections stand**; its plugin-shell
sections (§3 format/threading, §4 MIDI mapping, per-particle note table) are
**superseded by `sequencer-studio-architecture.md`.** Build the shell from the
contract, not from the elastic spec. (DECISIONS.md #1.)

## Build on this Mac (machine-local gotchas)
This is a JUCE/CMake audio plugin on Apple Silicon. The hard-won build/install/
validate process (Unix Makefiles not Xcode/Ninja, absolute build paths,
codesign-seal-after-build, `auval`, Release-not-Debug for perf, sandbox
`dangerouslyDisableSandbox` for install/validate) lives in the global
`~/.claude/CLAUDE.md` §"Building & validating audio plugins on this Mac".
Follow it verbatim; do not re-derive it. Same repo conventions as AURICLE
(CMake, single repo, `Source/` split `dsp/`/`model/`/`gui/`).

## Adding an engine (intake — engines live under engines/)
New engines land as `engines/<name>/` (monorepo — DECISIONS #6). `./verify`
goes RED on any `engines/<name>/` not registered, so nothing is half-added.
To intake one:
1. Drop the engine into `engines/<name>/` (prototype `<name>.html`, and its
   spec `<name>-spec.md` when written).
2. Copy `engines/_template/CLAUDE.md` → `engines/<name>/CLAUDE.md`; fill the
   slots from the spec/prototype.
3. Register it in `project.manifest.json` → `composite.territories`
   (path `engines/<name>`, role, spec-or-null, prototype, contract, status).
4. Add it to `ROADMAP.md` (spec-before-build: a prototype-only engine is
   BLOCKED on its spec first — see Coupled Rings O4a; a non-standard-contract
   engine is blocked on the contract change — see Kuramoto).
5. `./verify fast` green.

An agent can do steps 2–5 from a landed dir; or notify the maintainer.
Verify's unregistered-engine check is the safety net either way.
**Watch for contract findings during intake:** an engine that needs a
transport variant (Kuramoto → free-transport) or introduces a new output kind
(Torus → native pitch) is a contract-change proposal, not just a new
territory — record it in DECISIONS and gate the build on the contract change.

## Human gates
Deleting files, changing the contract (`sequencer-studio-architecture.md`),
editing `./verify` or acceptance tests, adding a dependency, anything outward-
facing. And: **do not begin Coupled Rings implementation** — its spec is not
yet written (prototype only). Writing that spec is its own gated task.
