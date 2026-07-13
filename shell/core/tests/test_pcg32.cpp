// test_pcg32.cpp — the determinism substrate must be reproducible & bounded.
#include "orrery/Pcg32.h"
#include "test_util.h"

using namespace orrery;

static void run() {
    // Same (seed, seq) → identical sequence.
    Pcg32 a, b;
    a.seed(12345, 1);
    b.seed(12345, 1);
    for (int i = 0; i < 1000; ++i) CHECK_EQ(a.nextU32(), b.nextU32());

    // Different sequence (slot) from the same project seed → diverges.
    Pcg32 c, d;
    c.seed(777, 0);
    d.seed(777, 1);
    bool differs = false;
    for (int i = 0; i < 8; ++i) if (c.nextU32() != d.nextU32()) differs = true;
    CHECK(differs);

    // nextFloat in [0,1); nextInt within inclusive bounds.
    Pcg32 e;
    e.seed(42, 2);
    for (int i = 0; i < 10000; ++i) {
        float f = e.nextFloat();
        CHECK(f >= 0.0f && f < 1.0f);
        int n = e.nextInt(-3, 5);
        CHECK(n >= -3 && n <= 5);
    }
    // Degenerate range returns lo.
    CHECK_EQ(e.nextInt(4, 4), 4);
}

RUN_MAIN()
