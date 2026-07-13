# CLAUDE.md — engines/torus-euclid/ (engine territory)

Sub-charter. Read root `../../CLAUDE.md` and contract
`../../sequencer-studio-architecture.md` first. Standard `IEngine` (tick/latch).

## ⚠ First pitch-generating engine — contract interaction to resolve
Orrery's first engine that emits PITCH, not just triggers. The contract's
output resolution (§2.4: pitch = engineBaseNote(sourceId) + transpose) assumes
the note-map assigns pitch; a pitch-native engine produces scale-degree rows
itself. How this composes with the note-map and the `scaleQuant` generator
needs a contract note (root DECISIONS #8) before the pitch path is wired.

## §Domain — source of truth
`torus-euclid-spec.md` (this dir). `sourceId`: pinned events get a stable id at
creation (survives drags); generated events use insertion rank (stable under
rotation + fixed pins/params, but the position under a rank shifts when pins/k/
A/n/m change) — offsets on generated events attach to structural rank, not
location. Document this legibly in GUI.

## The model in one line
k events with maximal evenness over a **time × pitch torus** (n time columns ×
m octave-folded pitch rows); 2D toroidal blue-noise = simultaneous rhythm and
melody coverage. Anisotropy A trades rhythm-evenness ⇄ pitch-coverage. Hand-
pinned events are constraints seeding the layout (the pins-and-flow model at
the pattern level).

## Reference oracle
`torus-euclid.html`. **Engine addition not in prototype:** a Lloyd-style
relaxation pass (L iterations, default 3) after greedy insertion — but
**L=0 must reproduce the prototype layout exactly** (regression anchor). Pins
never move during relaxation.

## Determinism obligations
Layout is deterministic (no RNG — greedy farthest-point, documented tie-breaks:
max min unweighted d², then lowest cell index). Nested by construction (length-k
layout is a prefix of length-(k+1)).
