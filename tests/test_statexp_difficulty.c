#include "hippodrome/statexp_difficulty.h"
#include "test.h"

#include <string.h>

int main(void) {
    hu_se_setting s = hu_se_vanilla();
    uint16_t lo, hi;

    /* vanilla changes nothing */
    CHECK(!hu_se_active(&s));
    hu_se_bounds(&s, HU_SE_OPPONENT, &lo, &hi);
    CHECK_EQ(lo, 0);
    CHECK_EQ(hi, 65535);
    {
        uint16_t se[5] = { 1, 1, 6889, 25600, 65535 };
        hu_se_apply(&s, HU_SE_OPPONENT, se);
        CHECK_EQ(se[0], 1);
        CHECK_EQ(se[4], 65535);
    }

    /* the preset tables: the quadratic curve, reversed for rentals */
    static const uint16_t kOpp[5] = { 0, 4096, 16384, 36863, 65535 };
    for (int p = HU_SE_CASUAL; p <= HU_SE_KAIZO; p++) {
        CHECK_EQ(hu_se_preset_value(p, HU_SE_OPPONENT), kOpp[p - HU_SE_CASUAL]);
        CHECK_EQ(hu_se_preset_value(p, HU_SE_RENTAL), kOpp[4 - (p - HU_SE_CASUAL)]);
    }
    CHECK_EQ(hu_se_preset_value(HU_SE_VANILLA, HU_SE_OPPONENT), 0);
    CHECK(strcmp(hu_se_preset_name(HU_SE_KAIZO), "Kaizo") == 0);
    CHECK(strcmp(hu_se_preset_name(99), "") == 0);

    /* only a minimum: Poké Ball Cup round 1 COM with Opponent Minimum = Medium (16384) */
    s.min.preset = HU_SE_MEDIUM;
    CHECK(hu_se_active(&s));
    {
        uint16_t se[5] = { 25600, 6889, 65535, 6889, 6889 };
        hu_se_apply(&s, HU_SE_OPPONENT, se);
        CHECK_EQ(se[0], 25600);
        CHECK_EQ(se[1], 16384);
        CHECK_EQ(se[2], 65535); /* no maximum: kept */
        CHECK_EQ(se[3], 16384);
    }
    /* only a maximum: Rental Maximum = Kaizo (0) strips a rental's stat exp */
    s = hu_se_vanilla();
    s.max.preset = HU_SE_KAIZO;
    {
        uint16_t se[5] = { 1, 1, 13107, 1, 65535 };
        hu_se_apply(&s, HU_SE_RENTAL, se);
        for (int i = 0; i < 5; i++) CHECK_EQ(se[i], 0);
    }

    /* custom values */
    s = hu_se_vanilla();
    s.min.preset = HU_SE_CUSTOM;
    s.min.custom = 1000;
    s.max.preset = HU_SE_CUSTOM;
    s.max.custom = 2000;
    hu_se_bounds(&s, HU_SE_RENTAL, &lo, &hi);
    CHECK_EQ(lo, 1000);
    CHECK_EQ(hi, 2000);

    /* validation: a raised minimum bumps the maximum, a lowered maximum pulls the minimum */
    s = hu_se_vanilla();
    s.min.preset = HU_SE_HARD;  /* opponent 36863 */
    s.max.preset = HU_SE_EASY;  /* opponent 4096 */
    CHECK(hu_se_bound_changed(&s, HU_SE_OPPONENT, true));
    CHECK_EQ(s.max.preset, HU_SE_HARD);
    s.min.preset = HU_SE_KAIZO;
    s.max.preset = HU_SE_CASUAL;
    CHECK(hu_se_bound_changed(&s, HU_SE_OPPONENT, false));
    CHECK_EQ(s.min.preset, HU_SE_CASUAL);
    /* rentals: Casual (65535) as the minimum above a Kaizo (0) maximum */
    s.min.preset = HU_SE_CASUAL;
    s.max.preset = HU_SE_KAIZO;
    CHECK(hu_se_bound_changed(&s, HU_SE_RENTAL, true));
    CHECK_EQ(s.max.preset, HU_SE_CASUAL);
    /* custom against a preset */
    s.min.preset = HU_SE_CUSTOM;
    s.min.custom = 50000;
    s.max.preset = HU_SE_HARD; /* opponent 36863 */
    CHECK(hu_se_bound_changed(&s, HU_SE_OPPONENT, true));
    CHECK_EQ(s.max.preset, HU_SE_CUSTOM);
    CHECK_EQ(s.max.custom, 50000);
    /* nothing to do: in order, or one bound Vanilla */
    s.min.preset = HU_SE_EASY;
    s.max.preset = HU_SE_HARD;
    CHECK(!hu_se_bound_changed(&s, HU_SE_OPPONENT, true));
    s.max.preset = HU_SE_VANILLA;
    s.min.preset = HU_SE_KAIZO;
    CHECK(!hu_se_bound_changed(&s, HU_SE_OPPONENT, true));

    CHECK_EQ(hu_se_clamp(5, 10, 20), 10);
    CHECK_EQ(hu_se_clamp(25, 10, 20), 20);
    CHECK_EQ(hu_se_clamp(15, 10, 20), 15);
    TEST_DONE();
}
