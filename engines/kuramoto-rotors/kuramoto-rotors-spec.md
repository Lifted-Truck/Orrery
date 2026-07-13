# KURAMOTO ROTORS — Engine Specification

**Status:** Prototype validated (`kuramoto-rotors.html`), shipped.
**Scope:** Engine only; shell per `sequencer-studio-architecture.md`. **This engine requires the free-transport variant of the engine contract (§5) — it does not use tick/latch clocking.**

---

## 1. Concept

N rotors, each carrying its own Euclidean pattern E(k_i, n_i) and spinning at an individually detuned rate; onsets trigger on crossing a fixed strike line. Kuramoto phase coupling between rotors makes the phasing⇄locking transition a single continuous knob: K = 0 gives Reich-style process phasing; K above the critical value (≈ detune spread) locks the ensemble into a stable polyrhythm whose relative phases are *found by the dynamics*; the near-critical region gives intermittent phase-slipping (the playable zone).

## 2. Model

Phase φ_i in revolutions (unwrapped accumulator Φ_i). Base rate `f = bpm/60/beatsPerBar` rev/s; detune `ω_i = f·(1 + δ_i)` with δ_i spread symmetrically over ±spread (per-rotor override allowed).

```
dΦ_i/dt = ω_i + (K·f/N) · Σ_{j≠i} sin(2π(Φ_j − Φ_i)) · g
```
g = prototype gain constant (6/2π in prototype units; calibrate once so K is in "multiples of critical coupling" — K = 1.0 should sit approximately at the lock transition for the default spread. Expose raw K internally, display normalized K/K_c).

**Integration:** fixed internal timestep h = 1 ms regardless of audio block size (accumulate remainder), RK2 or semi-implicit Euler. Fixed h is required for cross-machine determinism of a continuous ODE.

**Retrograde (prototype divergence — fix in engine):** the prototype floors dΦ/dt at 5% of base to avoid double-triggering on momentary reversal under strong coupling. The engine must instead allow retrograde honestly and use **direction-aware crossing detection**: maintain per-onset unwrapped count `c = floor(Φ + o)`; emit a trigger only on *increment* (forward crossing); on decrement, update the count silently (crossing backward un-arms, re-crossing forward re-fires). Brief retrograde near lock is real dynamics and audibly interesting.

**Order parameter:** r·e^{iψ} = (1/N)Σe^{i2πΦ_j}; expose r (coherence) and ψ (mean phase) as output signals and GUI meter/arrow.

## 3. Transport: free-transport contract + phantom rotor

This engine ignores bar/tick latching — rotation *is* the transport, so continuous dynamics are the audible channel (the latch principle's intent, satisfied differently). Requires studio contract addition:

```cpp
class IFreeTransportEngine {
  // called every audio block; engine integrates internally at fixed h and
  // emits sample-accurate TriggerEvents within the block
  virtual void process(BlockContext&, TriggerSink&) = 0;
  // gestures, midi-in, state, trace as in IEngine
};
```

**Host sync as a phantom rotor:** optional mode where the host's bar phase participates as rotor 0 — infinite inertia (it is never pulled), coupling weight `hostPull` (0..1) on all real rotors. At hostPull = 0 the ensemble free-runs; raising it drags the whole detuned choir toward the DAW grid and at high values effectively quantizes the ensemble to the bar while preserving internal phase relationships. This replaces naive transport-chasing and is the flagship parameter of the engine version.

## 4. Parameters

| id | range | default |
|---|---|---|
| `N` rotors | 1..8 | 3 |
| per-rotor: `k`, `n`, `detuneOverride?`, `mute` | k 1..n, n 1..32 | E(3/4/5,16) |
| `K` coupling (normalized to K_c) | 0..4 | 0 |
| `spread` | 0..±8% | ±1.5% |
| `hostPull` | 0..1 | 0 |
| `bpm/beatsPerBar` | host or internal | host |
| `scatter` | trigger | — |

sourceId = `(rotor << 8) | onsetIndex`. Energy = normalized |instantaneous rate deviation| (|dΦ_i/dt − ω_i| / (K·f)) — accents when a rotor is being yanked by the field; near lock this decays to 0 (calm), during slips it spikes (audible struggle).

MIDI-in perturbation: note-on → phase kick Δφ to the mapped rotor (velocity-scaled, signed by note position relative to a center note).

## 5. GUI

Concentric rotors with per-rotor color, own-zero phase markers, onset dots, strike line at 12 o'clock with crossing flash, mean-field arrow (direction ψ, length r), coherence meter with "locked" indicator (r > 0.995 sustained), per-rotor rows (k±, detune, mute), SCATTER.

## 6. Determinism, trace, tests

- Determinism: fixed h, fixed rotor iteration order, seeded scatter. Trace: per-second snapshot `{t, Phis[], r, psi}` + every trigger.
- **Tests:** (1) K = 0 → pairwise drift rates match detunes to 1e-6 rev/s. (2) Lock: N = 3, default spread, K = 1.5·K_c from random phases → r > 0.99 within 30 s, sustained. (3) Critical calibration: measured K_c within 20% of displayed 1.0 across N ∈ {2,3,5}. (4) Retrograde crossing: synthetic Φ trajectory that reverses across an onset produces exactly one trigger per net forward crossing. (5) Phantom rotor: hostPull = 1 → mean phase ψ tracks host bar phase within 1% of a revolution at steady state. (6) Determinism bit-exact across block sizes 32..2048.

## 7. Roadmap

- **Frequency adaptation** (second-order Kuramoto / inertia): rotors slowly retune toward the ensemble — permanent consequences of transient perturbations, not just phase memory.
- Coupling topology: ring/chain/star instead of all-to-all (wave propagation of phase corrections).
- Per-rotor strike lines (rotated outputs), multiple strike lines per rotor.
- Cross-engine: r and ψ as modulation sources (e.g., coherence → filter cutoff of the internal voices; slips → probability boosts in Probable Euclid).
