# CLAUDE.md — <engine-name>/ (engine territory)

<!-- TEMPLATE. Copy this dir to <engine-name>/ when a new engine lands, fill
     the <angle-bracket> slots from its spec/prototype, then register it in
     ../project.manifest.json (composite.territories) and add its phase to
     ../ROADMAP.md. `./verify` stays RED until the territory is registered. -->

Sub-charter. Read root `../CLAUDE.md` and contract
`../sequencer-studio-architecture.md` first. Implements the `IEngine`
contract; depends on the contract ONLY — never on the shell's internals or
another engine.

## §Domain — source of truth
`<engine-name>-spec.md` (this dir). `sourceId` = <what makes trigger identity
stable across generations and param changes short of changing k>.

## The model in one line
<the engine's generative principle, one or two sentences>

## Reference oracle
`<engine-name>.html` (validated prototype) — the behavioral reference the C++
must match.

## Acceptance tests (Layer-0 — from the spec, do not weaken)
<list the spec's acceptance tests: determinism bit-identity is mandatory for
every engine; add the engine-specific correctness properties>

## Determinism obligations
Fixed integration/computation (no wall-clock, no unseeded RNG); all randomness
from the shell's seeded PCG32 for this slot; trace one record per generation.
