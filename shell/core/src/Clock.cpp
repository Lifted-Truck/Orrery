// Clock.cpp — implementation of the §3 clock latch math.
#include "orrery/Clock.h"

#include <cmath>

namespace orrery::clockmath {

// A latch that lands within a hair of a boundary counts as on the boundary.
// One nanosecond of ppq is far below any musical resolution, so this only
// absorbs floating-point drift, never a real event.
static constexpr double kEps = 1e-9;

double ppqPerBar(const TransportState& t) {
    return static_cast<double>(t.timeSigNum) * 4.0 / static_cast<double>(t.timeSigDen);
}

double samplesPerQuarter(const TransportState& t) {
    return t.sampleRate * 60.0 / t.bpm;
}

double latchIntervalQuarters(const ClockConfig& c, const TransportState& t) {
    const double perBar = ppqPerBar(t);
    switch (c.division) {
        case Division::Bar:  return perBar;
        case Division::Half: return perBar / 2.0;
        case Division::Step: return perBar / static_cast<double>(c.stepsPerBar);
    }
    return perBar;
}

double lapLengthQuarters(const ClockConfig& c, const TransportState& t) {
    return c.barsPerLap * ppqPerBar(t);
}

int sampleOffsetForPpq(double targetPpq, const TransportState& t) {
    const double rel = (targetPpq - t.ppqAtBlockStart) * samplesPerQuarter(t);
    const int off = static_cast<int>(std::llround(rel));
    if (off < 0 || off >= t.blockSize) return -1;
    return off;
}

double eventPpq(long long lapIndex, double barPhase,
                const ClockConfig& c, const TransportState& t) {
    return static_cast<double>(lapIndex) * lapLengthQuarters(c, t)
         + barPhase * lapLengthQuarters(c, t);
}

int computeLatches(const ClockConfig& c, const TransportState& t,
                   LatchPoint* out, int maxOut) {
    if (!t.isPlaying || maxOut <= 0) return 0;

    const double interval = latchIntervalQuarters(c, t);
    if (interval <= 0.0) return 0;

    const double spq        = samplesPerQuarter(t);
    const double blockQ     = static_cast<double>(t.blockSize) / spq;
    const double startPpq   = t.ppqAtBlockStart;
    const double endPpq     = startPpq + blockQ;

    // First latch index at or after the block start (epsilon so a boundary
    // sitting exactly on the first sample is included, not skipped).
    long long n = static_cast<long long>(std::ceil((startPpq - kEps) / interval));
    if (n < 0) n = 0;

    int count = 0;
    for (; count < maxOut; ++n) {
        const double L = static_cast<double>(n) * interval;
        if (L >= endPpq - kEps) break;      // past this block
        if (L < startPpq - kEps) continue;  // defensive; ceil() should prevent

        int off = static_cast<int>(std::llround((L - startPpq) * spq));
        if (off < 0) off = 0;
        if (off >= t.blockSize) off = t.blockSize - 1;

        out[count].sampleOffset = off;
        out[count].index        = n;
        out[count].ppq          = L;
        ++count;
    }
    return count;
}

} // namespace orrery::clockmath
