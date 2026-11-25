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
    /* Stat Exp difficulty: the widest range with stat experience held in a range. */
    {
        hu_speed_range full = hu_speed_initial(90, 50);
        hu_speed_range same = hu_speed_initial_se(90, 50, 0, 65535);
        CHECK_EQ(same.min, full.min);
        CHECK_EQ(same.max, full.max);
        hu_speed_range kaizo = hu_speed_initial_se(90, 50, 65535, 65535);
        CHECK_EQ(kaizo.min, hu_gb_stat(90, 0, 65535, 50));
        CHECK_EQ(kaizo.max, full.max);
        /* Rescaling to the same range changes nothing; from the base rules to a fixed 65535 keeps the DV spread. */
        hu_speed_range k = { 120, 130 };
        hu_speed_range back = hu_speed_rescale(k, 90, 50, 0, 65535, 0, 65535);
        CHECK_EQ(back.min, 120);
        CHECK_EQ(back.max, 130);
        hu_speed_range up = hu_speed_rescale(k, 90, 50, 0, 65535, 65535, 65535);
        CHECK(up.min >= kaizo.min && up.max <= kaizo.max);
        CHECK_EQ(up.max, full.max); /* 130 allows DV 15 at little stat exp: at 65535 that is the top */
        /* An exact speed learned at a fixed 0 stat exp is one DV: at a fixed 65535 it is exact again. */
        hu_speed_range exact = { hu_gb_stat(90, 7, 0, 50), hu_gb_stat(90, 7, 0, 50) };
        hu_speed_range moved = hu_speed_rescale(exact, 90, 50, 0, 0, 65535, 65535);
        CHECK(moved.min == moved.max || moved.max - moved.min <= 1); /* (neighbouring DVs can share a stat) */
        CHECK(moved.min >= hu_gb_stat(90, 6, 65535, 50) && moved.max <= hu_gb_stat(90, 8, 65535, 50));
        /* A range no DV and stat exp fit gives the widest range of the new setting. */
        hu_speed_range none = { 10, 11 };
        hu_speed_range w = hu_speed_rescale(none, 90, 50, 0, 65535, 0, 65535);
        CHECK_EQ(w.min, full.min);
        CHECK_EQ(w.max, full.max);
    }

    /* Failsafe: hu_speed_contains, hu_speed_validate, and hu_speed_observe_checked. */
    {
        hu_speed_range fallback = hu_speed_initial(80, 55); /* Dragonite L55: 93-144 */
        hu_speed_range r = fallback;
        bool was_reset = true;

        CHECK(hu_speed_contains(fallback, 127));
        CHECK(!hu_speed_contains(hu_speed_exact(117), 127));

        /* Valid range passes through unchanged. */
        hu_speed_range v = hu_speed_validate(r, 127, fallback, &was_reset);
        CHECK(!was_reset);
        CHECK_EQ(v.min, r.min);
        CHECK_EQ(v.max, r.max);

        /* Corrupted range (e.g. stale 117-117 when true Speed is 127) resets to fallback. */
        v = hu_speed_validate(hu_speed_exact(117), 127, fallback, &was_reset);
        CHECK(was_reset);
        CHECK_EQ(v.min, fallback.min);
        CHECK_EQ(v.max, fallback.max);

        /* Normal observation that still contains true Speed narrows without resetting. */
        CHECK(hu_speed_observe_checked(&r, HU_MOVED_FIRST, 117, &hu_gb_speed_rules, hu_speed_mods_none(), 127,
                                       fallback, &was_reset));
        CHECK(!was_reset);
        CHECK_EQ(r.min, 117);
        CHECK_EQ(r.max, fallback.max);

        /* Bad observation (e.g. missing paralysis flag) that would exclude true Speed resets to fallback. */
        CHECK(!hu_speed_observe_checked(&r, HU_MOVED_SECOND, 117, &hu_gb_speed_rules, hu_speed_mods_none(), 127,
                                        fallback, &was_reset));
        CHECK(was_reset);
        CHECK_EQ(r.min, fallback.min);
        CHECK_EQ(r.max, fallback.max);
    }

    TEST_DONE();
}
