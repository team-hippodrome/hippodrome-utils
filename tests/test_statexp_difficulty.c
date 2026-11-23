#include "hippodrome/statexp_difficulty.h"
#include "test.h"

#include <string.h>

int main(void) {
    hu_se_setting s = hu_se_vanilla();
    uint16_t lo = 1, hi = 1;

    /* vanilla changes nothing */
    CHECK_EQ(s.mode, HU_SE_VANILLA);
    CHECK(!hu_se_active(&s));
    CHECK(!hu_se_range(&s, HU_SE_OPPONENT, &lo, &hi));
    {
        uint16_t se[5] = { 1, 1, 6889, 25600, 65535 };
        hu_se_apply(&s, HU_SE_OPPONENT, se);
        CHECK_EQ(se[0], 1);
        CHECK_EQ(se[2], 6889);
        CHECK_EQ(se[4], 65535);
    }

    /* the presets: fifths of 65535 */
    static const uint16_t kEnds[6] = { 0, 13107, 26214, 39321, 52428, 65535 };
    for (int m = HU_SE_CASUAL; m <= HU_SE_KAIZO; m++) {
        s.mode = (uint8_t)m;
        CHECK(hu_se_range(&s, HU_SE_OPPONENT, &lo, &hi));
        CHECK_EQ(lo, kEnds[m - HU_SE_CASUAL]);
        CHECK_EQ(hi, kEnds[m - HU_SE_CASUAL + 1]);
        CHECK_EQ(hu_se_errors(&s), 0);
    }
    /* rentals: the same labels the other way round (Casual = strongest, Kaizo = weakest) */
    for (int m = HU_SE_CASUAL; m <= HU_SE_KAIZO; m++) {
        s.mode = (uint8_t)m;
        CHECK(hu_se_range(&s, HU_SE_RENTAL, &lo, &hi));
        CHECK_EQ(lo, kEnds[4 - (m - HU_SE_CASUAL)]);
        CHECK_EQ(hi, kEnds[5 - (m - HU_SE_CASUAL)]);
    }
    s.mode = HU_SE_KAIZO;
    {
        uint16_t se[5] = { 65535, 25600, 13107, 1, 0 };
        hu_se_apply(&s, HU_SE_RENTAL, se);
        CHECK_EQ(se[0], 13107);
        CHECK_EQ(se[1], 13107);
        CHECK_EQ(se[2], 13107);
        CHECK_EQ(se[3], 1);
        CHECK_EQ(se[4], 0);
    }
    /* custom bounds are the same for both sides */
    s.mode = HU_SE_CUSTOM;
    s.custom_min = 100;
    s.custom_max = 200;
    CHECK(hu_se_range(&s, HU_SE_RENTAL, &lo, &hi));
    CHECK_EQ(lo, 100);
    CHECK_EQ(hi, 200);
    CHECK(strcmp(hu_se_mode_name(HU_SE_KAIZO), "Kaizo") == 0);
    CHECK(strcmp(hu_se_mode_name(HU_SE_EASY), "Easy") == 0);
    CHECK(strcmp(hu_se_mode_name(99), "") == 0);

    /* Medium on the Poké Ball Cup round 1 COM (25600 HP, 6889 elsewhere, one 65535) */
    s.mode = HU_SE_MEDIUM;
    {
        uint16_t se[5] = { 25600, 6889, 65535, 6889, 30000 };
        hu_se_apply(&s, HU_SE_OPPONENT, se);
        CHECK_EQ(se[0], 26214);
        CHECK_EQ(se[1], 26214);
        CHECK_EQ(se[2], 39321);
        CHECK_EQ(se[3], 26214);
        CHECK_EQ(se[4], 30000); /* inside: kept */
    }
    /* Kaizo maxes a rental's stat exp of 1 */
    s.mode = HU_SE_KAIZO;
    {
        uint16_t se[5] = { 1, 1, 1, 1, 1 };
        hu_se_apply(&s, HU_SE_OPPONENT, se);
        for (int i = 0; i < 5; i++) CHECK_EQ(se[i], 52428);
    }

    /* custom: own bounds, min == max forces a value, max below min is an error and acts as vanilla */
    s.mode = HU_SE_CUSTOM;
    s.custom_min = s.custom_max = 65535;
    {
        uint16_t se[5] = { 1, 6889, 25600, 1, 65535 };
        hu_se_apply(&s, HU_SE_OPPONENT, se);
        for (int i = 0; i < 5; i++) CHECK_EQ(se[i], 65535);
    }
    s.custom_min = 2;
    s.custom_max = 1;
    CHECK_EQ(hu_se_errors(&s), HU_SE_ORDER);
    CHECK(!hu_se_active(&s));
    {
        uint16_t se[5] = { 0, 0, 0, 0, 0 };
        hu_se_apply(&s, HU_SE_OPPONENT, se);
        CHECK_EQ(se[0], 0);
    }
    CHECK(hu_se_error_text(HU_SE_ORDER)[0] != '\0');
    CHECK(hu_se_error_text(0)[0] == '\0');
    /* an unknown mode is vanilla */
    s.mode = 42;
    CHECK(!hu_se_active(&s));
    TEST_DONE();
}
