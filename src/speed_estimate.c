#include "hippodrome/speed_estimate.h"

hu_speed_range hu_speed_initial(uint8_t base_speed, uint8_t level) {
    hu_speed_range r;
    r.min = hu_gb_stat(base_speed, 0, 0, level);
    r.max = hu_gb_stat(base_speed, HU_DV_MAX, HU_STAT_EXP_MAX, level);
    return r;
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
