# ELASTIC EUCLID — VST Specification

**Status:** Prototype validated (HTML/Web Audio, v1 + v2). Ready for plugin implementation.
**Prototype artifacts:** `elastic-euclid.html` (continuous flow, deprecated model), `elastic-euclid-2.html` (clocked iterated map, canonical model).
**Core design decision (validated in prototyping):** the physics is an **iterated map clocked by musical time**, not a continuous flow. `pattern[t+1] = relax(pattern[t])`. Continuous wall-clock evolution was tried first and rejected: motion between playhead crossings is inaudible by construction, so it reads as decorative. All evolution happens at clock-division boundaries; the pattern is latched (frozen) within each division.

---

## 1. Concept

A rhythm sequencer where the Euclidean pattern E(k,n) is the **equilibrium state of a dynamical system** rather than the output of an algorithm.

- k particles live on a circle (phase θ ∈ [0,1), one lap = one bar).
- Pairwise repulsion drives maximal evenness.
- A lattice potential with n wells drives grid quantization.
- At equilibrium with both forces active, particle positions coincide exactly with a Bjorklund pattern (some rotation of E(k,n)).
- Perturbations (adding/removing a particle, kicks, dragging, incoming MIDI) displace the system, which then **relaxes back over audible bar-to-bar generations** instead of snapping.

The musical value is entirely in the transitions: k-changes renegotiate rather than jump; low damping produces ringing (period-2/limit-cycle orbits = automatic pattern variation); lattice strength morphs between free continuous phasing and grid-locked Euclid.

## 2. Core model

### 2.1 State
```
particle_i = { theta: double in [0,1), omega: double, id: int, mass: double (v2: fixed 1.0) }
k = particles.length (1..32)
n = lattice wells (1..64)
generation = int counter
```

### 2.2 Forces (per integrator substep)
Signed circular distance: `sdist(a,b) = wrap(b - a + 0.5) - 0.5`, range (−0.5, 0.5].

1. **Repulsion** (pairwise, symmetric):
   `F_rep(i←j) = −K · sign(d) / (d² + ε)`, with `d = sdist(θ_i, θ_j)`, `ε = 4e-4`.
   Prototype scaling: `K = repulsion_param × 0.004`.
2. **Lattice** (wells at multiples of 1/n):
   `F_lat(i) = −A · sin(2π n θ_i) / n`. Prototype scaling: `A = lattice_param × 3.0`.
3. **Damping:** `F_damp(i) = −c · ω_i`. Prototype scaling: `c = damping_param × 8.0`.

### 2.3 Integration
Semi-implicit Euler, fixed substeps:
```
ω += F·h;  ω = clamp(ω, −6, 6);  θ = wrap(θ + ω·h)
```
One **tick** applies `T = relax_per_tick` seconds of simulated time in exactly `SUB = 24` substeps (`h = T/24`). Fixed substep count is mandatory for determinism (see §6).

### 2.4 Clocking
Ticks fire at boundaries of the selected clock division:
- `BAR` (default): once per bar → one generation per bar.
- `HALF`: twice per bar.
- `STEP`: every 1/n of a bar.
- `FREE`: continuous evolution (kept only as a comparison/legacy mode; document as "usually not what you want").

**Latch semantics:** between ticks, θ values are constants. Triggering, GUI pattern readout, and MIDI output all read the latched state. Stored velocity (from kicks/MIDI/drag release) is visible state but has no effect until the next tick.

### 2.5 Perturbation inputs
- **Kick:** `ω_i += uniform(−1.5, 1.5)` for all i (seeded RNG, see §6).
- **Add particle:** insert at midpoint of the largest angular gap ± small seeded noise; ω = 0.
- **Remove particle:** LIFO (remove most recently added).
- **Drag (GUI):** while held, θ pinned to pointer, ω forced to 0; takes effect immediately (a drag is a performance gesture, not a physics event).
- **MIDI in (new for VST):** incoming note-on on mapped channel applies a kick impulse to the particle whose note it matches; velocity scales impulse magnitude. This makes the sequencer *playable* — drumming into it perturbs the rhythm.

## 3. Plugin architecture

- **Framework:** JUCE (current LTS), C++20. Same repo conventions as AURICLE (CMake, single repo, `Source/` split into `dsp/`, `model/`, `gui/`).
- **Format:** VST3 + AU + CLAP if the JUCE CLAP wrapper is in the toolchain. **Important VST3 caveat:** VST3 has no first-class "MIDI FX" category; ship as an *instrument* (`kInstrumentSynth`) that both emits MIDI (for routing to other instruments in hosts that support VST3 MIDI-out, e.g. Ableton Live 11+, Bitwig, Reaper) **and** contains a minimal internal drum voice (port the prototype's sine-drop + noise-click voice) so it is audible standalone in hosts with poor VST3 MIDI routing. AU on macOS can additionally ship a `kAudioUnitType_MIDIProcessor` build for Logic.
- **Host sync:** derive bar phase from `AudioPlayHead::PositionInfo` (ppqPosition, timeSigNumerator/Denominator, isPlaying). One ring lap = one bar of the host time signature by default; expose a `bars-per-lap` parameter (0.25..4) for longer/shorter cycles. When transport stopped, free-running internal clock is OFF; ⏭ TICK remains available.

### 3.1 Threading & real-time safety
- All physics runs on the **audio thread**. Cost is O(k²·SUB) per tick with k ≤ 32, SUB = 24 → trivially cheap and, critically, only at tick boundaries, not per-block.
- No allocation, locks, or logging on the audio thread. Particle array is fixed-capacity (32), plain structs.
- GUI reads state via a lock-free snapshot (double-buffered POD copy + atomic sequence counter).
- Drag/kick/add/remove from GUI → lock-free SPSC command queue drained at block start.

### 3.2 Sample-accurate scheduling
Within each `processBlock`:
1. Compute bar-phase at block start and end from host ppq.
2. If a tick boundary falls inside the block: run `tick()` at that exact sample offset, then continue trigger scanning with post-tick positions.
3. For each particle, if its latched θ falls inside the swept phase interval, emit note-on at the exact sample offset `= (θ − phase_blockstart) / phase_per_sample`. This kills the prototype's frame-rate jitter.
4. Note-offs: fixed gate time parameter (ms or fraction of a step).

## 4. Parameters

| id | name | range | default | notes |
|---|---|---|---|---|
| `k` | particles | 1..32 (int) | 5 | add/remove via gesture; automatable stepped |
| `n` | lattice wells | 1..64 (int) | 16 | |
| `repulsion` | evenness | 0..2 | 1.0 | |
| `lattice` | grid pull | 0..2 | 0.6 | 0 = free phasing |
| `damping` | settle | 0.02..2 | 0.35 | |
| `relax` | relaxation/tick | 0.01..0.6 s (sim time) | 0.08 | log taper |
| `clockdiv` | clock | BAR / HALF / STEP / FREE | BAR | |
| `barsPerLap` | lap length | 0.25..4 | 1 | quantized to musical values |
| `kick` | kick | trigger | — | automatable trigger param |
| `quantizeOut` | output quantize | 0..1 | 0 | 0 = emit continuous micro-timing; 1 = snap emitted notes to nearest 1/n (physics untouched) |
| `gate` | gate length | 1..500 ms | 80 | |
| `midiInPerturb` | MIDI-in kick amount | 0..1 | 0.5 | |
| per-particle | note number, channel | GUI table | stacked map | default: note = 36 + 3·(id mod 5) style spread |

Velocity of emitted notes: base velocity ± contribution from |ω| at trigger time (moving particles hit harder — kinetic accent). Expose `velEnergy` 0..1.

## 5. GUI

Port the v2 prototype visual language (it works):
- Ring, lattice ticks (length ∝ lattice param), playhead, latched particles (solid), pending-energy whiskers (dashed), tick pulse flash at generation boundaries.
- **Bjorklund equilibrium ghosts:** hollow diamonds at nearest-rotation E(k,n). Algorithm: compute Bjorklund onsets; for each of n rotations, best cyclic alignment of the two sorted position lists by summed squared sdist; display argmin. Also drive a "Δ from equilibrium" meter from the RMS.
- Linear latched-pattern strip with ghost markers.
- Generation counter.
- Drag interaction with pointer capture; hit radius ~0.06 of the circle.

## 6. Determinism & decision trace (wend compatibility)

- Fixed substep count per tick; double precision; no wall-clock dependence anywhere in the model → identical input sequence produces identical output on any machine.
- All randomness (kick impulses, add-particle jitter) from a **seeded PCG32**; seed is a saved parameter.
- Optional **trace mode** (off audio thread, drained from a ring buffer): one JSON line per generation `{gen, k, n, params, thetas[], omegas[], events[]}` — same philosophy as wend's decision traces. This is the bridge to Tonality-ecosystem tooling: a trace fully reconstructs a performance.

## 7. State

Save/restore: all parameters + particle array (θ, ω, id, note map) + generation + RNG state. Chunk versioned from day one.

## 8. Acceptance tests

1. **Equilibrium correctness:** for all (k, n) with k < n ≤ 32, default physics, from Bjorklund-adjacent random starts: after 200 ticks the quantized pattern equals some rotation of E(k,n). (Property test.)
2. **Determinism:** identical seed + identical event sequence → bit-identical trace across two runs.
3. **Timing:** emitted note-on offsets within ±1 sample of analytically expected positions at 44.1/48/96 kHz, block sizes 32..2048, including ticks landing mid-block.
4. **RT safety:** no allocations in `processBlock` (assert via allocation hooks in debug).
5. **k-change behavior:** adding a particle mid-playback never produces a discontinuous jump of *other* particles within the same bar (latch invariant).

## 9. Roadmap (post-v1)

- Per-particle **mass** (heavy anchor downbeat, light scattering ghost notes).
- **Asymmetric repulsion / pinned particles** (a particle that pushes but cannot be pushed = fixed anchor).
- **Coupled rings:** ring B's particles feel potential wells created by ring A's positions → interlocking parts for free. (This is the strongest post-v1 feature; design the model layer so multiple rings + coupling matrix is not a rewrite.)
- **Tonality integration:** pitch assignment via the shared JSON contract instead of the static note map (particle id → scale-degree policy).
- Bifurcation-aware presets: named (relax, damping) regimes — "settle", "ring", "period-2", "wander".

## 10. Open questions for build agent

- CLAP wrapper availability in current JUCE toolchain — include if trivial, don't block on it.
- Whether `barsPerLap` should follow host time-sig changes mid-playback (proposal: recompute lap boundaries lazily at next bar).
- GUI framework: raw JUCE `Component` painting is sufficient (prototype canvas ports 1:1); no need for OpenGL unless profiling says otherwise.
