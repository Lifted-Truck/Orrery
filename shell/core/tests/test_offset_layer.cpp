// test_offset_layer.cpp — THE coexistence mechanism (§2.2): hand edits are pins,
// generators flow around them and NEVER touch a locked cell.
#include "orrery/OffsetLayer.h"
#include "test_util.h"

#include <cstdlib>
#include <vector>

using namespace orrery;

static void run() {
    // Euclidean accent E(3,8) → 3 hits, maximally even (positions 2,5,7).
    {
        bool hits[8];
        euclideanPattern(3, 8, hits);
        int count = 0;
        for (bool h : hits) count += h ? 1 : 0;
        CHECK_EQ(count, 3);
        CHECK(hits[2] && hits[5] && hits[7]);
        CHECK(!hits[0] && !hits[1]);
    }

    std::vector<int32_t> present8 = {0,1,2,3,4,5,6,7};

    // Coexistence: walk (T) + accent (V) running, with two hand-locked cells.
    {
        OffsetLayer ol;
        ol.seed(0xC0FFEE, 0xFFFF);
        ol.walk()   = WalkParams{true, /*T*/true, /*V*/false, /*step*/2, /*range*/12, /*rate*/1, 1.0f};
        ol.accent() = AccentParams{true, /*accents*/3, /*amount*/20, 1.0f};

        ol.setTranspose(3, +5);   // pin T on id 3 (a walk target)
        ol.setVelOffset(5, -30);  // pin V on id 5 (an accent HIT position)

        for (int g = 0; g < 40; ++g) ol.runGenerators(g, present8);

        // Pinned cells are exactly preserved — generators skipped them.
        CHECK_EQ((int)ol.cell(3).transpose, 5);
        CHECK(ol.cell(3).lockT);
        CHECK_EQ((int)ol.cell(5).velOffset, -30);
        CHECK(ol.cell(5).lockV);

        // Unlocked accent hit (id 7) received the accent; unlocked non-hit (id 0)
        // did not (walk targets T only, so V stays 0 there).
        CHECK_EQ((int)ol.cell(7).velOffset, 20);
        CHECK_EQ((int)ol.cell(0).velOffset, 0);

        // Walk actually moved unlocked T cells, all within the ±range bound.
        int tsum = 0;
        for (int32_t id : present8) {
            CHECK(ol.cell(id).transpose >= -12 && ol.cell(id).transpose <= 12);
            if (id != 3) tsum += std::abs((int)ol.cell(id).transpose);
        }
        CHECK(tsum > 0);

        // Unlock everything → the formerly pinned cells now fall to generators.
        ol.unlockAll();
        for (int g = 40; g < 44; ++g) ol.runGenerators(g, present8);
        CHECK(!ol.cell(5).lockV);
        CHECK_EQ((int)ol.cell(5).velOffset, 20);  // id 5 is an accent hit
    }

    // Offset retention across k changes (§1.2): a vanished sourceId keeps its
    // cell; re-appearing restores it.
    {
        OffsetLayer ol;
        ol.seed(1, 2);
        ol.setTranspose(6, +7);              // pin id 6
        std::vector<int32_t> present4 = {0,1,2,3};   // id 6 absent
        for (int g = 0; g < 10; ++g) ol.runGenerators(g, present4);
        CHECK_EQ((int)ol.cell(6).transpose, 7);      // untouched while absent
        for (int g = 10; g < 20; ++g) ol.runGenerators(g, present8);
        CHECK_EQ((int)ol.cell(6).transpose, 7);      // still pinned on return
    }

    // Determinism: same seed + same gesture history → identical cells.
    {
        OffsetLayer a, b;
        a.seed(99, 7); b.seed(99, 7);
        a.walk() = b.walk() = WalkParams{true, true, true, 3, 20, 1, 1.0f};
        for (int g = 0; g < 50; ++g) { a.runGenerators(g, present8); b.runGenerators(g, present8); }
        for (int32_t id : present8) {
            CHECK_EQ((int)a.cell(id).transpose, (int)b.cell(id).transpose);
            CHECK_EQ((int)a.cell(id).velOffset, (int)b.cell(id).velOffset);
        }
    }
}

RUN_MAIN()
