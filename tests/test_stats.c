#include "hippodrome/stats.h"
#include "test.h"

int main(void) {
    const hu_speed_rules* gb = &hu_gb_speed_rules;
    hu_speed_mods m = hu_speed_mods_none();

    CHECK_EQ(hu_stat_exp_bonus(0), 0);
    CHECK_EQ(hu_stat_exp_bonus(16), 1);
    CHECK_EQ(hu_stat_exp_bonus(17), 1);     /* root 5 */
    CHECK_EQ(hu_stat_exp_bonus(65535), 63); /* root capped at 255 */

    /* Pikachu (base Speed 90) at level 50: 95 to 141. */
    CHECK_EQ(hu_gb_stat(90, 0, 0, 50), 95);
    CHECK_EQ(hu_gb_stat(90, 15, 65535, 50), 141);
    /* Mewtwo (base Speed 130) at level 100: 265 to 358. */
    CHECK_EQ(hu_gb_stat(130, 0, 0, 100), 265);
    CHECK_EQ(hu_gb_stat(130, 15, 65535, 100), 358);
    /* Chansey (base HP 250) at level 100 with max: 703. */
    CHECK_EQ(hu_gb_hp(250, 15, 65535, 100), 703);

    CHECK_EQ(hu_apply_stage(gb, 100, 0), 100);
    CHECK_EQ(hu_apply_stage(gb, 100, 1), 150);
    CHECK_EQ(hu_apply_stage(gb, 100, -1), 66);
    CHECK_EQ(hu_apply_stage(gb, 100, -6), 25);
    CHECK_EQ(hu_apply_stage(gb, 400, 6), 999);
    CHECK_EQ(hu_apply_stage(gb, 1, -6), 1);
    CHECK_EQ(hu_apply_stage(gb, 100, 9), 400); /* clamped to +6 */

    CHECK_EQ(hu_effective_speed(gb, 100, m), 100);
    m.paralysis_cuts = 1;
    CHECK_EQ(hu_effective_speed(gb, 100, m), 25);
    CHECK_EQ(hu_effective_speed(gb, 3, m), 1);
    m.stage = 2;
    CHECK_EQ(hu_effective_speed(gb, 100, m), 50);
    m.stage = 0;
    m.paralysis_cuts = 2; /* Gen 1: cut again after the opponent's stat change */
    CHECK_EQ(hu_effective_speed(gb, 100, m), 6);

    /* Other rules: a halving paralysis (later generations) and an extra multiplier. */
    hu_speed_rules half = *gb;
    half.paralysis.num = 1;
    half.paralysis.den = 2;
    m = hu_speed_mods_none();
    m.paralysis_cuts = 1;
    CHECK_EQ(hu_effective_speed(&half, 100, m), 50);
    m.extra.num = 3;
    m.extra.den = 2;
    CHECK_EQ(hu_effective_speed(&half, 100, m), 75);
    TEST_DONE();
}
