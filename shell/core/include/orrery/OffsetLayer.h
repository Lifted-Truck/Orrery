// OffsetLayer.h — the decoration stage (sequencer-studio-architecture.md §2).
//
// THE coexistence mechanism: hand edits are pins (locked cells); generators
// flow around them. Generators write cells at bar boundaries and SKIP locked
// cells. Every offset cell persists across k changes so a vanished sourceId's
// offset is restored when it re-appears (§1.2, §2.2). Deterministic: the layer
// owns one PCG32 stream from the project seed.
//
// v1 generator set for O1 is `walk` + `accent` (ROADMAP O1). `contour`, `arp`,
// and `scaleQuant` are O5 and intentionally absent here.
#pragma once

#include <span>

#include "orrery/Contract.h"
#include "orrery/Pcg32.h"
#include "orrery/Types.h"

namespace orrery {

// Bounded random walk (§2.3). Advances internal per-cell state every rateBars;
// the cell value is recomposed from that state each bar for unlocked cells.
struct WalkParams {
    bool  enabled  = false;
    bool  targetT  = true;   // write transpose
    bool  targetV  = false;  // write velocity offset
    int   step     = 1;      // max +/- move per advance
    int   range    = 12;     // symmetric bound on the walk value
    int   rateBars = 1;      // advance once every this many bars
    float wet      = 1.0f;   // contribution scale [0,1]
};

// Euclidean accent E(accents, k) over present sourceIds in id order (§2.3):
// accents distributed maximally evenly across the voices; hit voices get a
// velocity bump.
struct AccentParams {
    bool  enabled = false;
    int   accents = 3;
    int   amount  = 20;      // velocity added to accented voices
    float wet     = 1.0f;
};

class OffsetLayer {
public:
    void seed(uint64_t projectSeed, uint64_t stream = 0xFFFF);

    // ── Hand editing (§2.1) — every hand edit sets the cell's lock ────────────
    void setTranspose(int32_t id, int value);   // sets lockT
    void setVelOffset(int32_t id, int value);   // sets lockV
    void resetCell(int32_t id);                  // clears value + locks (double-click)
    void unlockT(int32_t id);
    void unlockV(int32_t id);
    void unlockAll();

    // ── Generative editing (§2.2) — bar-boundary write, locked cells skipped ──
    // Compose the generator stack into cells for the k present sourceIds (given
    // sorted ascending). Advances walk state on rateBars multiples of `gen`.
    void runGenerators(int64_t gen, std::span<const int32_t> presentIds);

    // ── Output resolution (§2.4) ──────────────────────────────────────────────
    // pitch = baseNote + transpose ; velocity = clamp(baseVel + velOffset).
    // (contour(energy) and scaleQuant compose here at O5.)
    static void resolve(const OffsetCell& cell, int baseNote, int baseVel,
                        int& pitchOut, int& velOut);

    const OffsetCell& cell(int32_t id) const { return cells_[id]; }
    OffsetCell&       cell(int32_t id)       { return cells_[id]; }

    WalkParams&   walk()   { return walk_; }
    AccentParams& accent() { return accent_; }

    // State persistence (shell-level; not on the tick path).
    void saveCells(Chunk& c) const;
    void loadCells(const Chunk& c);

    // A one-line JSONL trace record of the current cell state for generation g.
    void writeTrace(int64_t gen, TraceWriter& tw) const;

private:
    OffsetCell cells_[kMaxSources];
    int        walkT_[kMaxSources] = {0};  // internal walk state (transpose)
    int        walkV_[kMaxSources] = {0};  // internal walk state (velocity)
    WalkParams   walk_;
    AccentParams accent_;
    Pcg32        rng_;
};

// Euclidean hit pattern via Bresenham distribution: exactly clamp(a,0,k) hits
// spread maximally evenly across k positions. out must hold k bools.
void euclideanPattern(int accents, int k, bool* out);

} // namespace orrery
