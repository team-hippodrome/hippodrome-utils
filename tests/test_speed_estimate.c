#include "hippodrome/speed_estimate.h"
#include "test.h"

static hu_speed_mods mods(int stage, int cuts) {
    hu_speed_mods m = hu_speed_mods_none();
    m.stage = (int8_t)stage;
    m.paralysis_cuts = (uint8_t)cuts;
    return m;
}

int main(void) {
    const hu_speed_rules* gb = &hu_gb_speed_rules;
    /* Pikachu L50: 95..141 */
    hu_speed_range r = hu_speed_initial(90, 50);
    CHECK_EQ(r.min, 95);
    CHECK_EQ(r.max, 141);
    CHECK(!hu_speed_is_exact(r));

    /* It moved first against a Pokémon at 120: at least 120. */
    CHECK(hu_speed_observe(&r, HU_MOVED_FIRST, 120, gb, mods(0, 0)));
    CHECK_EQ(r.min, 120);
    CHECK_EQ(r.max, 141);
    /* Moving first against something slower teaches nothing. */
    CHECK(!hu_speed_observe(&r, HU_MOVED_FIRST, 100, gb, mods(0, 0)));
    /* Moved second against 130: at most 130. */
    CHECK(hu_speed_observe(&r, HU_MOVED_SECOND, 130, gb, mods(0, 0)));
    CHECK_EQ(r.max, 130);
    /* A contradicting observation is ignored. */
    CHECK(!hu_speed_observe(&r, HU_MOVED_FIRST, 200, gb, mods(0, 0)));
    CHECK_EQ(r.min, 120);
    CHECK_EQ(r.max, 130);

    /* Normalisation: at +2 (x2) it moved first against 250, so raw >= 125. */
    r = hu_speed_initial(90, 50);
    CHECK(hu_speed_observe(&r, HU_MOVED_FIRST, 250, gb, mods(2, 0)));
    CHECK_EQ(r.min, 125);
    /* Paralysed (x1/4) and moved second against 30: raw/4 <= 30, so raw <= 123. */
    r = hu_speed_initial(90, 50);
    CHECK(hu_speed_observe(&r, HU_MOVED_SECOND, 30, gb, mods(0, 1)));
    CHECK_EQ(r.max, 123);
    /* Paralysis cut twice (Gen 1 quirk) and moved first against 7: raw/4/4 >= 7 -> raw >= 112. */
    r = hu_speed_initial(90, 50);
    CHECK(hu_speed_observe(&r, HU_MOVED_FIRST, 7, gb, mods(0, 2)));
    CHECK_EQ(r.min, 112);
    /* At -1 (x66/100) it moved second against 70: raw*66/100 <= 70 -> raw <= 107 (107*66/100 = 70). */
    r = hu_speed_initial(90, 50);
    CHECK(hu_speed_observe(&r, HU_MOVED_SECOND, 70, gb, mods(-1, 0)));
    CHECK_EQ(r.max, 107);
    /* Halving paralysis under other rules: moved second against 60 -> raw <= 121. */
    hu_speed_rules half = *gb;
    half.paralysis.den = 2;
    r = hu_speed_initial(90, 50);
    CHECK(hu_speed_observe(&r, HU_MOVED_SECOND, 60, &half, mods(0, 1)));
    CHECK_EQ(r.max, 121);
    /* Below the range entirely: moving second against 10 can't happen -> ignored. */
    r = hu_speed_initial(90, 50);
    CHECK(!hu_speed_observe(&r, HU_MOVED_SECOND, 10, gb, mods(0, 0)));
    CHECK_EQ(r.max, 141);
    CHECK_EQ(r.min, 95);
    /* Exactly at the bottom of the range: max becomes min. */
    CHECK(hu_speed_observe(&r, HU_MOVED_SECOND, 95, gb, mods(0, 0)));
    CHECK(hu_speed_is_exact(r));

    r = hu_speed_initial(90, 50);
    hu_speed_range e = hu_speed_shown(r, gb, mods(1, 0));
    CHECK_EQ(e.min, 142);
    CHECK_EQ(e.max, 211);

    hu_speed_range a = { 100, 140 }, b = { 120, 160 }, c = { 150, 160 };
    hu_speed_range i = hu_speed_intersect(a, b);
    CHECK_EQ(i.min, 120);
    CHECK_EQ(i.max, 140);
    i = hu_speed_intersect(a, c);
    CHECK_EQ(i.min, 150);
        /* Stat Exp clamping. Beedrill (base 75) L12: the game's own stat exp gives hu_speed_initial. */
    hu_speed_range v = hu_speed_initial(75, 12);
    hu_speed_range s = hu_speed_initial_se(75, 12, 0, 65535);
    CHECK_EQ(s.min, v.min);
    CHECK_EQ(s.max, v.max);
    /* Stat exp fixed at 65535: only the DV is left, 30..34. */
    s = hu_speed_initial_se(75, 12, 65535, 65535);
    CHECK_EQ(s.min, 30);
    CHECK_EQ(s.max, 34);
    /* Seen at exactly 26 with the game's own stat exp: every DV fits, so under 65535..65535 it is 30..34 again. */
    hu_speed_range x = { 26, 26 };
    s = hu_speed_rescale(x, 75, 12, 0, 65535, 65535, 65535);
    CHECK_EQ(s.min, 30);
    CHECK_EQ(s.max, 34);
    /* Exactly 31 under 65535..65535 pins the DV to 2..5; back with the game's own stat exp that is 23..31. */
    x.min = x.max = 31;
    s = hu_speed_rescale(x, 75, 12, 65535, 65535, 0, 65535);
    CHECK_EQ(s.min, 23);
    CHECK_EQ(s.max, 31);
    /* Same clamp: unchanged. */
    s = hu_speed_rescale(x, 75, 12, 100, 200, 100, 200);
    CHECK_EQ(s.min, 31);
    CHECK_EQ(s.max, 31);
    /* A range keeps no DVs, so there and back loses what the clamp pinned (23..31 fits every DV): that is why
     * trainer memory keeps the stored estimate and only rescales it to show it. */
    s = hu_speed_rescale(hu_speed_rescale(x, 75, 12, 65535, 65535, 0, 65535), 75, 12, 0, 65535, 65535, 65535);
    CHECK_EQ(s.min, 30);
    CHECK_EQ(s.max, 34);
    /* A range no stat could have (corrupt file) falls back to the plain initial range under the new clamp. */
    x.min = x.max = 200;
    s = hu_speed_rescale(x, 75, 12, 0, 65535, 65535, 65535);
    CHECK_EQ(s.min, 30);
    CHECK_EQ(s.max, 34);

    TEST_DONE();
}
