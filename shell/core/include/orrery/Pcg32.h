// Pcg32.h — deterministic PRNG for the Orrery core (contract §6).
//
// One PCG32 stream per engine slot plus one for the offset layer, all derived
// from a single saved project seed. Header-only, seeded, wall-clock-free — the
// determinism substrate. This is the canonical minimal PCG32 (M.E. O'Neill,
// pcg-random.org; Apache-2.0 / MIT), used verbatim so streams are portable and
// bit-reproducible across platforms.
#pragma once

#include <cstdint>

namespace orrery {

struct Pcg32 {
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc   = 0xda3e39cb94b95bdbULL;

    // Seed a stream. `seq` selects the stream (use the slot index) so different
    // slots derived from the same project seed never share a sequence.
    void seed(uint64_t seed, uint64_t seq) {
        state = 0u;
        inc   = (seq << 1u) | 1u;
        nextU32();
        state += seed;
        nextU32();
    }

    uint32_t nextU32() {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31u));
    }

    // Uniform float in [0, 1). 24-bit mantissa resolution.
    float nextFloat() {
        return (nextU32() >> 8) * (1.0f / 16777216.0f);
    }

    // Uniform integer in [lo, hi] inclusive. Bounded, rejection-free
    // (negligible modulo bias for the small ranges used by generators).
    int nextInt(int lo, int hi) {
        if (hi <= lo) return lo;
        uint32_t span = static_cast<uint32_t>(hi - lo) + 1u;
        return lo + static_cast<int>(nextU32() % span);
    }
};

} // namespace orrery
