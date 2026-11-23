#include "hippodrome/speed_estimate.h"

hu_speed_range hu_speed_initial(uint8_t base_speed, uint8_t level) {
    hu_speed_range r;
    r.min = hu_gb_stat(base_speed, 0, 0, level);
    r.max = hu_gb_stat(base_speed, HU_DV_MAX, HU_STAT_EXP_MAX, level);
    return r;
}

static uint16_t clamp16(unsigned v, uint16_t lo, uint16_t hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (uint16_t)v;
}

hu_speed_range hu_speed_initial_se(uint8_t base_speed, uint8_t level, uint16_t se_lo, uint16_t se_hi) {
    hu_speed_range r;
    r.min = hu_gb_stat(base_speed, 0, clamp16(0, se_lo, se_hi), level);
    r.max = hu_gb_stat(base_speed, HU_DV_MAX, clamp16(HU_STAT_EXP_MAX, se_lo, se_hi), level);
    return r;
}

/* The stat depends on the stat exp only through its bonus, which steps where the root (1..255) steps: between the
 * points below (and the clamp bounds) the Speed under either clamp is constant, so trying them covers every value. */
static int add_point(uint16_t* pts, int n, long v) {
    if (v >= 0 && v <= HU_STAT_EXP_MAX) pts[n++] = (uint16_t)v;
    return n;
}

hu_speed_range hu_speed_rescale(hu_speed_range r, uint8_t base_speed, uint8_t level, uint16_t old_lo, uint16_t old_hi,
                                uint16_t new_lo, uint16_t new_hi) {
    uint16_t pts[2 * 256 + 16];
    int n = 0;
    hu_speed_range out = { 0xFFFF, 0 };
    if (old_lo == new_lo && old_hi == new_hi) return r;
    n = add_point(pts, n, 0);
    n = add_point(pts, n, HU_STAT_EXP_MAX);
    for (long root = 1; root <= 255; root++) {
        n = add_point(pts, n, (root - 1) * (root - 1) + 1); /* first value with this root */
        n = add_point(pts, n, root * root);                 /* last one */
    }
    n = add_point(pts, n, old_lo);
    n = add_point(pts, n, old_hi);
    n = add_point(pts, n, new_lo);
    n = add_point(pts, n, new_hi);
    n = add_point(pts, n, (long)old_lo - 1);
    n = add_point(pts, n, (long)old_hi + 1);
    n = add_point(pts, n, (long)new_lo - 1);
    n = add_point(pts, n, (long)new_hi + 1);
    for (unsigned dv = 0; dv <= HU_DV_MAX; dv++) {
        for (int i = 0; i < n; i++) {
            uint16_t was = hu_gb_stat(base_speed, (uint8_t)dv, clamp16(pts[i], old_lo, old_hi), level);
            uint16_t now;
            if (was < r.min || was > r.max) continue;
            now = hu_gb_stat(base_speed, (uint8_t)dv, clamp16(pts[i], new_lo, new_hi), level);
            if (now < out.min) out.min = now;
            if (now > out.max) out.max = now;
        }
    }
    if (out.min > out.max) return hu_speed_initial_se(base_speed, level, new_lo, new_hi);
    return out;
}

hu_speed_range hu_speed_exact(uint16_t speed) {
    hu_speed_range r = { speed, speed };
    return r;
}

bool hu_speed_is_exact(hu_speed_range r) {
    return r.min == r.max;
}

/* The effective speed is monotone (non-decreasing) in the raw stat for any rules, so a linear scan over the (at most a
 * few hundred wide) range finds the bound. */
bool hu_speed_observe(hu_speed_range* r, hu_turn_result result, uint16_t opponent_speed, const hu_speed_rules* rules,
                      hu_speed_mods mods) {
    unsigned raw;
    if (r->min > r->max) {
        return false;
    }
    if (result == HU_MOVED_FIRST) {
        /* effective(raw) >= opponent_speed: the smallest such raw */
        for (raw = r->min; raw <= r->max; raw++) {
            if (hu_effective_speed(rules, (uint16_t)raw, mods) >= opponent_speed) {
                break;
            }
        }
        if (raw > r->max || raw == r->min) {
            return false;
        }
        r->min = (uint16_t)raw;
        return true;
    }
    /* effective(raw) <= opponent_speed: the largest such raw */
    raw = r->max;
    while (hu_effective_speed(rules, (uint16_t)raw, mods) > opponent_speed) {
        if (raw == r->min) {
            return false;
        }
        raw--;
    }
    if (raw == r->max) {
        return false;
    }
    r->max = (uint16_t)raw;
    return true;
}

hu_speed_range hu_speed_shown(hu_speed_range r, const hu_speed_rules* rules, hu_speed_mods mods) {
    hu_speed_range e;
    e.min = hu_effective_speed(rules, r.min, mods);
    e.max = hu_effective_speed(rules, r.max, mods);
    return e;
}

hu_speed_range hu_speed_intersect(hu_speed_range a, hu_speed_range b) {
    hu_speed_range r;
    r.min = a.min > b.min ? a.min : b.min;
    r.max = a.max < b.max ? a.max : b.max;
    if (r.min > r.max) {
        return b;
    }
    return r;
}
