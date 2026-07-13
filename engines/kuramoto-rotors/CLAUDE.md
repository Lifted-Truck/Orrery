# CLAUDE.md — engines/kuramoto-rotors/ (engine territory)

Sub-charter. Read root `../../CLAUDE.md` and contract
`../../sequencer-studio-architecture.md` first.

## ⚠ Requires a contract variant — BUILD-BLOCKED on a contract change
This engine does **not** use tick/latch clocking. It needs the free-transport
interface `IFreeTransportEngine` (integrate internally at fixed h, emit
sample-accurate triggers per block), which the contract §5 does not yet
define. Adding it is a contract-change proposal (root DECISIONS #7) — human-
gated, done in the shell/contract before this engine builds.

## §Domain — source of truth
`kuramoto-rotors-spec.md` (this dir). `sourceId` per the contract §1.2 once
the free-transport variant is specified (per-rotor onset identity).

## The model in one line
N rotors, each carrying its own E(k_i,n_i) and detuned rate, coupled via
Kuramoto phase coupling; K is one continuous knob from Reich-style process
phasing (K=0) through intermittent slipping to locked polyrhythm (K>K_c).
Onsets fire on forward strike-line crossings.

## Reference oracle
`kuramoto-rotors.html`. **Two deliberate engine divergences from the
prototype (do not "match the prototype" here):** (1) allow honest retrograde
with direction-aware crossing detection instead of the prototype's 5% rate
floor; (2) the host-sync "phantom rotor" (rotor 0, infinite inertia, hostPull
coupling) replaces naive transport-chasing — this is the flagship parameter.

## Determinism obligations
Fixed internal timestep h = 1 ms regardless of block size (accumulate
remainder), RK2 / semi-implicit Euler — mandatory for cross-machine
determinism of a continuous ODE. Order parameter r·e^{iψ} exposed as output
signal.
