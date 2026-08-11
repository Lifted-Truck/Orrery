// OffsetLayer.cpp — implementation of the §2 decoration stage.
#include "orrery/OffsetLayer.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace orrery {

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

void euclideanPattern(int accents, int k, bool* out) {
    if (k <= 0) return;
    const int a = clampi(accents, 0, k);
    for (int i = 0; i < k; ++i) {
        const int prev = (i * a) / k;
        const int cur  = ((i + 1) * a) / k;
        out[i] = (cur - prev) > 0;
    }
}

void OffsetLayer::seed(uint64_t projectSeed, uint64_t stream) {
    rng_.seed(projectSeed, stream);
}

void OffsetLayer::setTranspose(int32_t id, int value) {
    cells_[id].transpose = static_cast<int8_t>(clampi(value, -24, 24));
    cells_[id].lockT = true;
}

void OffsetLayer::setVelOffset(int32_t id, int value) {
    cells_[id].velOffset = static_cast<int8_t>(clampi(value, -64, 64));
    cells_[id].lockV = true;
}

void OffsetLayer::resetCell(int32_t id) {
    cells_[id] = OffsetCell{};
    walkT_[id] = 0;
    walkV_[id] = 0;
}

void OffsetLayer::unlockT(int32_t id)  { cells_[id].lockT = false; }
void OffsetLayer::unlockV(int32_t id)  { cells_[id].lockV = false; }
void OffsetLayer::unlockAll() {
    for (auto& c : cells_) { c.lockT = false; c.lockV = false; }
}

void OffsetLayer::runGenerators(int64_t gen, std::span<const int32_t> presentIds) {
    const int k = static_cast<int>(presentIds.size());
    if (k <= 0) return;

    // 1) Advance walk internal state on rateBars boundaries — for EVERY present
    //    id (locked or not), so unlocking later resumes a coherent walk. The
    //    lock only gates whether the value is written into the cell (step 3).
    if (walk_.enabled && walk_.rateBars > 0 && (gen % walk_.rateBars) == 0) {
        for (int32_t id : presentIds) {
            if (walk_.targetT) {
                walkT_[id] = clampi(walkT_[id] + rng_.nextInt(-walk_.step, walk_.step),
                                    -walk_.range, walk_.range);
            }
            if (walk_.targetV) {
                walkV_[id] = clampi(walkV_[id] + rng_.nextInt(-walk_.step, walk_.step),
                                    -walk_.range, walk_.range);
            }
        }
    }

    // 2) Euclidean accent pattern over the present voices in id order.
    bool hits[kMaxSources] = {false};
    if (accent_.enabled) euclideanPattern(accent_.accents, k, hits);

    // 3) Compose the stack into cells — locked cells are never touched.
    for (int i = 0; i < k; ++i) {
        const int32_t id = presentIds[i];
        OffsetCell& c = cells_[id];

        if (!c.lockT) {
            int t = 0;
            if (walk_.enabled && walk_.targetT)
                t += static_cast<int>(std::lround(walkT_[id] * walk_.wet));
            c.transpose = static_cast<int8_t>(clampi(t, -24, 24));
        }
        if (!c.lockV) {
            int v = 0;
            if (walk_.enabled && walk_.targetV)
                v += static_cast<int>(std::lround(walkV_[id] * walk_.wet));
            if (accent_.enabled && hits[i])
                v += static_cast<int>(std::lround(accent_.amount * accent_.wet));
            c.velOffset = static_cast<int8_t>(clampi(v, -64, 64));
        }
    }
}

// ── §2.5 TIMING lane (contract v1.1) ────────────────────────────────────────
double swingOffset(int64_t pos, double swing, double tickDur) {
    // Odd positions ride later; even ones are untouched. Guard the modulo for
    // negative positions so a pre-roll tick doesn't invert the feel.
    const bool odd = ((pos % 2) + 2) % 2 == 1;
    return odd ? clampd(swing, 0.0, 1.0) * tickDur * 0.5 : 0.0;
}

void OffsetLayer::setTimingOffset(int32_t id, float fracOfTick) {
    timing_[id].offset = static_cast<float>(clampd(fracOfTick, -1.0, 1.0));
    timing_[id].lock = true;   // hand edit = pin (same rule as the other cells)
}

void OffsetLayer::unlockAllTiming() {
    for (auto& t : timing_) t.lock = false;
}

double OffsetLayer::eventTime(int32_t id, int64_t pos, double tickTime,
                              double tickDur, float overshootFrac) const {
    const double tol = clampd(timingParams_.tolAmount, 0.0, 1.0);
    const double os  = clampd(static_cast<double>(overshootFrac), 0.0, 1.0);
    return tickTime
         + tol * os * tickDur
         + swingOffset(pos, timingParams_.swing, tickDur)
         + static_cast<double>(timing_[id].offset) * tickDur;
}

void OffsetLayer::resolve(const OffsetCell& cell, int baseNote, int baseVel,
                          int& pitchOut, int& velOut) {
    pitchOut = clampi(baseNote + cell.transpose, 0, 127);
    velOut   = clampi(baseVel + cell.velOffset, 1, 127);
}

// v2 layout: [-2][count][OffsetCell × count][TimingCell × count].
// The version sentinel is NEGATIVE so it can never collide with a legacy v1
// chunk, whose first field was a positive cell count.
static constexpr int32_t kCellChunkV2 = -2;

void OffsetLayer::saveCells(Chunk& c) const {
    c.put<int32_t>(kCellChunkV2);
    c.put<int32_t>(kMaxSources);
    for (const auto& cell : cells_)  c.put(cell);
    for (const auto& t : timing_)    c.put(t);
}

OffsetLayer::ChunkStatus OffsetLayer::loadCells(const Chunk& c) {
    c.rewind();
    const int32_t head = c.get<int32_t>();

    if (head >= 0) {
        // Legacy v1: head IS the count, and there are no timing cells.
        if (head > kMaxSources) return ChunkStatus::Truncated;
        for (int i = 0; i < head; ++i) cells_[i] = c.get<OffsetCell>();
        for (auto& t : timing_) t = TimingCell{};
        return ChunkStatus::MigratedV1;
    }
    if (head != kCellChunkV2) return ChunkStatus::Malformed;   // unknown version

    const int32_t n = c.get<int32_t>();
    if (n < 0) return ChunkStatus::Malformed;
    // MORE cells than we can hold: report rather than quietly drop the tail.
    const bool overflow = n > kMaxSources;
    const int  keep = overflow ? kMaxSources : n;
    for (int i = 0; i < keep; ++i) cells_[i] = c.get<OffsetCell>();
    for (int i = keep; i < n; ++i) (void)c.get<OffsetCell>();   // skip the tail
    for (int i = 0; i < keep; ++i) timing_[i] = c.get<TimingCell>();
    return overflow ? ChunkStatus::Truncated : ChunkStatus::Ok;
}

void OffsetLayer::writeTrace(int64_t gen, TraceWriter& tw) const {
    std::string s = "{\"kind\":\"offset\",\"gen\":" + std::to_string(gen) + ",\"cells\":[";
    for (int i = 0; i < kMaxSources; ++i) {
        const auto& c = cells_[i];
        if (i) s += ',';
        s += "{\"t\":" + std::to_string(c.transpose)
           + ",\"v\":" + std::to_string(c.velOffset)
           + ",\"lt\":" + (c.lockT ? "1" : "0")
           + ",\"lv\":" + (c.lockV ? "1" : "0") + "}";
    }
    s += "]}";
    tw.writeLine(s);
}

} // namespace orrery
