// Clock.h — the clock service (sequencer-studio-architecture.md §3).
//
// Pure, stateless latch math. Given a host transport snapshot for one audio
// block and an engine's ClockConfig, it locates the latch boundaries inside the
// block and their sample-accurate offsets, and maps an engine's lap-phase
// events to absolute ppq / block-relative sample offsets. No wall clock, no
// state — the shell adapter supplies TransportState from the AudioPlayHead.
#pragma once

#include "orrery/Types.h"

namespace orrery::clockmath {

// Quarter notes per bar for the transport's time signature (4/4 → 4, 6/8 → 3).
double ppqPerBar(const TransportState& t);

// Samples per quarter note at the transport's rate/tempo.
double samplesPerQuarter(const TransportState& t);

// Quarter notes between successive latches for this division.
double latchIntervalQuarters(const ClockConfig& c, const TransportState& t);

// Length of one lap (barsPerLap bars) in quarter notes.
double lapLengthQuarters(const ClockConfig& c, const TransportState& t);

// Block-relative sample offset of an absolute ppq position. Returns the offset
// in [0, blockSize) if the position falls inside this block, else -1.
int sampleOffsetForPpq(double targetPpq, const TransportState& t);

// Absolute ppq of an event at `barPhase` within lap `lapIndex`.
double eventPpq(long long lapIndex, double barPhase,
                const ClockConfig& c, const TransportState& t);

// Fill `out` with the latch boundaries whose ppq lies in this block's window
// [ppqAtBlockStart, ppqAtBlockStart + blockQuarters). Returns the count written
// (capped at maxOut). Each LatchPoint carries its within-block sampleOffset, its
// monotonic index from ppq 0 (the generation), and its absolute ppq. When the
// transport is stopped, returns 0 (manual TICK is a separate shell affordance).
int computeLatches(const ClockConfig& c, const TransportState& t,
                   LatchPoint* out, int maxOut);

} // namespace orrery::clockmath
