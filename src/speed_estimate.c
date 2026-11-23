#include "hippodrome/speed_estimate.h"

hu_speed_range hu_speed_initial(uint8_t base_speed, uint8_t level) {
    hu_speed_range r;
    r.min = hu_gb_stat(base_speed, 0, 0, level);
    r.max = hu_gb_stat(base_speed, HU_DV_MAX, HU_STAT_EXP_MAX, level);
    return r;
}

/* As hu_se_bounds_apply holds a value: raised to the minimum, then lowered to the maximum. */
static uint16_t clamp_se(uint32_t e, uint16_t lo, uint16_t hi) {
    if (e < lo) e = lo;
    if (e > hi) e = hi;
    return (uint16_t)e;
}

hu_speed_range hu_speed_initial_se(uint8_t base_speed, uint8_t level, uint16_t se_min, uint16_t se_max) {
    hu_speed_range r;
    r.min = hu_gb_stat(base_speed, 0, clamp_se(0, se_min, se_max), level);
    r.max = hu_gb_stat(base_speed, HU_DV_MAX, clamp_se(HU_STAT_EXP_MAX, se_min, se_max), level);
    return r;
}

/* The stat is non-decreasing in the (unclamped) stat experience, so each DV's allowed stat experience is one interval,
 * found by binary search. */
hu_speed_range hu_speed_rescale(hu_speed_range r, uint8_t base_speed, uint8_t level, uint16_t from_min,
                                uint16_t from_max, uint16_t to_min, uint16_t to_max) {
    hu_speed_range out = { 0xFFFF, 0 };
    for (unsigned dv = 0; dv <= HU_DV_MAX; dv++) {
        uint32_t lo = 0, hi = HU_STAT_EXP_MAX + 1u, e_min, e_max;
        /* the smallest e whose stat reaches r.min */
        while (lo < hi) {
            uint32_t mid = (lo + hi) / 2;
            if (hu_gb_stat(base_speed, (uint8_t)dv, clamp_se(mid, from_min, from_max), level) >= r.min) hi = mid;
            else lo = mid + 1;
        }
        e_min = lo;
        /* the largest e whose stat stays at most r.max */
        lo = 0;
        hi = HU_STAT_EXP_MAX + 1u;
        while (lo < hi) {
            uint32_t mid = (lo + hi) / 2;
            if (hu_gb_stat(base_speed, (uint8_t)dv, clamp_se(mid, from_min, from_max), level) > r.max) hi = mid;
            else lo = mid + 1;
        }
        if (lo == 0 || e_min > HU_STAT_EXP_MAX || e_min > lo - 1) {
            continue; /* no stat experience fits this DV */
        }
        e_max = lo - 1;
        {
            uint16_t a = hu_gb_stat(base_speed, (uint8_t)dv, clamp_se(e_min, to_min, to_max), level);
            uint16_t b = hu_gb_stat(base_speed, (uint8_t)dv, clamp_se(e_max, to_min, to_max), level);
            if (a < out.min) out.min = a;
            if (b > out.max) out.max = b;
        }
    }
    if (out.min > out.max) {
        return hu_speed_initial_se(base_speed, level, to_min, to_max);
    }
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
