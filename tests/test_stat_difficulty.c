#include "hippodrome/stat_difficulty.h"
#include "hippodrome/stats.h"
#include "test.h"

int main(void) {
    hu_sd_range r = hu_sd_vanilla();
    uint16_t se[5] = { 1, 1, 6889, 1, 65535 };

    /* vanilla changes nothing */
    CHECK(!r.custom);
    CHECK_EQ(hu_sd_errors(&r), 0);
    CHECK(!hu_sd_active(&r));
    CHECK_EQ(hu_sd_apply_dvs(&r, 0x3535), 0x3535);
    hu_sd_apply_stat_exp(&r, se);
    CHECK_EQ(se[0], 1);
    CHECK_EQ(se[4], 65535);

    /* the full range on custom is vanilla too */
    r.custom = true;
    CHECK(hu_sd_active(&r));
    CHECK_EQ(hu_sd_apply_dvs(&r, 0x3535), 0x3535);
    hu_sd_apply_stat_exp(&r, se);
    CHECK_EQ(se[2], 6889);

    /* DV word layout and the derived HP DV */
    CHECK_EQ(hu_dv_get(0x1234, HU_SD_DV_ATTACK), 1);
    CHECK_EQ(hu_dv_get(0x1234, HU_SD_DV_DEFENSE), 2);
    CHECK_EQ(hu_dv_get(0x1234, HU_SD_DV_SPEED), 3);
    CHECK_EQ(hu_dv_get(0x1234, HU_SD_DV_SPECIAL), 4);
    CHECK_EQ(hu_dv_hp(0xFFFF), 15);
    CHECK_EQ(hu_dv_hp(0x0000), 0);
    CHECK_EQ(hu_dv_hp(0x1234), 0xA); /* 1 0 1 0 */
    CHECK_EQ(hu_dv_hp(0x8888), 0);   /* all even */

    /* a clamp: values outside move to the nearer end, values inside stay */
    r.dv_min = 8;
    r.dv_max = 12;
    CHECK_EQ(hu_sd_apply_dvs(&r, 0x3F9C), 0x8C9C);
    r.stat_exp_min = 1000;
    r.stat_exp_max = 30000;
    {
        uint16_t s[5] = { 1, 999, 1000, 25600, 65535 };
        hu_sd_apply_stat_exp(&r, s);
        CHECK_EQ(s[0], 1000);
        CHECK_EQ(s[1], 1000);
        CHECK_EQ(s[2], 1000);
        CHECK_EQ(s[3], 25600);
        CHECK_EQ(s[4], 30000);
    }

    /* min == max gives that exact value: the Poké Ball Cup COM made as strong as a rental */
    r.dv_min = r.dv_max = 15;
    r.stat_exp_min = r.stat_exp_max = 65535;
    CHECK_EQ(hu_sd_apply_dvs(&r, 0x3535), 0xFFFF);
    {
        uint16_t s[5] = { 6889, 6889, 6889, 1, 1 };
        hu_sd_apply_stat_exp(&r, s);
        for (int i = 0; i < 5; i++) CHECK_EQ(s[i], 65535);
    }

    /* errors: max below min, DV over 15; an erroneous range acts as vanilla */
    r.dv_min = 10;
    r.dv_max = 5;
    CHECK_EQ(hu_sd_errors(&r), HU_SD_DV_ORDER);
    CHECK(!hu_sd_active(&r));
    CHECK_EQ(hu_sd_apply_dvs(&r, 0x3535), 0x3535);
    r.dv_max = 15;
    r.stat_exp_min = 2;
    r.stat_exp_max = 1;
    CHECK_EQ(hu_sd_errors(&r), HU_SD_STAT_EXP_ORDER);
    r.stat_exp_max = 2;
    r.dv_max = 16;
    CHECK_EQ(hu_sd_errors(&r), HU_SD_DV_TOO_HIGH);
    CHECK(hu_sd_error_text(HU_SD_DV_ORDER)[0] != '\0');
    CHECK(hu_sd_error_text(HU_SD_STAT_EXP_ORDER)[0] != '\0');
    CHECK(hu_sd_error_text(0)[0] == '\0');

    /* a stat from clamped values: level 30 Pikachu (base Speed 90), DV 15 and 65535 Speed stat exp */
    CHECK_EQ(hu_gb_stat(90, 15, 65535, 30), 86);
    TEST_DONE();
}
