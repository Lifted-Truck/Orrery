// test_clock.cpp — latch boundaries and sample-accurate timing (§3, ±1 sample).
#include "orrery/Clock.h"
#include "test_util.h"

#include <cmath>

using namespace orrery;
using namespace orrery::clockmath;

static TransportState T(double sr, double bpm, double ppq, int block) {
    TransportState t;
    t.sampleRate = sr; t.bpm = bpm; t.ppqAtBlockStart = ppq; t.blockSize = block;
    return t;
}

static void run() {
    // Time signature → quarters per bar.
    { TransportState t = T(48000, 120, 0, 512); t.timeSigNum = 4; t.timeSigDen = 4;
      CHECK_NEAR(ppqPerBar(t), 4.0, 1e-12); }
    { TransportState t = T(48000, 120, 0, 512); t.timeSigNum = 6; t.timeSigDen = 8;
      CHECK_NEAR(ppqPerBar(t), 3.0, 1e-12); }

    // Bar latch at ppq 0 lands on sample 0, generation index 0.
    {
        ClockConfig c; c.division = Division::Bar;
        TransportState t = T(48000, 120, 0.0, 512);
        LatchPoint lp[8];
        int n = computeLatches(c, t, lp, 8);
        CHECK_EQ(n, 1);
        CHECK_EQ(lp[0].sampleOffset, 0);
        CHECK_EQ(lp[0].index, (int64_t)0);
    }

    // Step latches: 16 steps/bar in 4/4 → 0.25q apart = 6000 samples @120/48k.
    // A 48000-sample block starting at ppq 0 spans 2 quarters = 8 steps.
    {
        ClockConfig c; c.division = Division::Step; c.stepsPerBar = 16;
        TransportState t = T(48000, 120, 0.0, 48000);
        LatchPoint lp[32];
        int n = computeLatches(c, t, lp, 32);
        CHECK_EQ(n, 8);
        for (int i = 0; i < n; ++i) {
            CHECK_EQ(lp[i].sampleOffset, i * 6000);
            CHECK_EQ(lp[i].index, (int64_t)i);
        }
    }

    // A latch mid-block (not on a block boundary) is placed to ±1 sample.
    // ppqStart = 3.9, Bar latch at ppq 4.0 → 0.1q * 24000 = 2400 samples in.
    {
        ClockConfig c; c.division = Division::Bar;
        TransportState t = T(48000, 120, 3.9, 4096);
        LatchPoint lp[8];
        int n = computeLatches(c, t, lp, 8);
        CHECK_EQ(n, 1);
        CHECK_EQ(lp[0].index, (int64_t)1);
        CHECK(std::abs(lp[0].sampleOffset - 2400) <= 1);
    }

    // ±1-sample sweep across rates/tempos/divisions: every latch's absolute
    // sample position must be within 1 sample of the ideal (ppq * spq).
    {
        const double rates[] = {44100.0, 48000.0, 88200.0, 96000.0};
        const double bpms[]  = {90.0, 120.0, 128.0, 137.5};
        const Division divs[] = {Division::Bar, Division::Half, Division::Step};
        for (double sr : rates)
        for (double bpm : bpms)
        for (Division d : divs) {
            ClockConfig c; c.division = d; c.stepsPerBar = 16;
            const double spq = samplesPerQuarter(T(sr, bpm, 0, 0));
            // Walk several blocks of 512 samples, tracking ppq exactly.
            double ppq = 0.0;
            const int block = 512;
            const double blockQ = block / spq;
            for (int b = 0; b < 200; ++b) {
                TransportState t = T(sr, bpm, ppq, block);
                LatchPoint lp[64];
                int n = computeLatches(c, t, lp, 64);
                const double blockStartSample = ppq * spq;
                for (int i = 0; i < n; ++i) {
                    double ideal = lp[i].ppq * spq;          // absolute ideal sample
                    double got   = blockStartSample + lp[i].sampleOffset;
                    CHECK(std::fabs(ideal - got) <= 1.0);
                }
                ppq += blockQ;
            }
        }
    }

    // Stopped transport yields no latches.
    {
        ClockConfig c;
        TransportState t = T(48000, 120, 0, 4096); t.isPlaying = false;
        LatchPoint lp[8];
        CHECK_EQ(computeLatches(c, t, lp, 8), 0);
    }
}

RUN_MAIN()
