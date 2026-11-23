#include "hippodrome/statexp_difficulty.h"

#include "hippodrome/stats.h"

#define FIFTH (HU_STAT_EXP_MAX / 5) /* 13107 */

hu_se_setting hu_se_vanilla(void) {
    hu_se_setting s = { HU_SE_VANILLA, 0, HU_STAT_EXP_MAX };
    return s;
}

const char* hu_se_mode_name(int mode) {
    static const char* const kNames[HU_SE_MODE_COUNT] = { "Vanilla", "Casual", "Easy",  "Medium",
                                                          "Hard",    "Kaizo",  "Custom" };
    return (mode >= 0 && mode < HU_SE_MODE_COUNT) ? kNames[mode] : "";
}

unsigned hu_se_errors(const hu_se_setting* setting) {
    if (setting->mode != HU_SE_CUSTOM) return 0;
    return setting->custom_max < setting->custom_min ? HU_SE_ORDER : 0;
}

const char* hu_se_error_text(unsigned error_bit) {
    return error_bit == HU_SE_ORDER ? "The highest Stat Exp is below the lowest." : "";
}

bool hu_se_range(const hu_se_setting* setting, hu_se_side side, uint16_t* min, uint16_t* max) {
    if (setting->mode >= HU_SE_CASUAL && setting->mode <= HU_SE_KAIZO) {
        unsigned k = setting->mode - HU_SE_CASUAL;
        if (side == HU_SE_RENTAL) k = 4 - k; /* the labels are difficulty: Kaizo rentals are the weakest */
        *min = (uint16_t)(k * FIFTH);
        *max = (uint16_t)((k + 1) * FIFTH);
        return true;
    }
    if (setting->mode == HU_SE_CUSTOM && hu_se_errors(setting) == 0) {
        *min = setting->custom_min;
        *max = setting->custom_max;
        return true;
    }
    return false;
}

bool hu_se_active(const hu_se_setting* setting) {
    uint16_t lo, hi;
    return hu_se_range(setting, HU_SE_OPPONENT, &lo, &hi);
}

void hu_se_apply(const hu_se_setting* setting, hu_se_side side, uint16_t stat_exp[HU_SE_STAT_COUNT]) {
    uint16_t lo, hi;
    int i;
    if (!hu_se_range(setting, side, &lo, &hi)) return;
    for (i = 0; i < HU_SE_STAT_COUNT; i++) {
        if (stat_exp[i] < lo) stat_exp[i] = lo;
        if (stat_exp[i] > hi) stat_exp[i] = hi;
    }
}

/* ---- four bounds ---- */

hu_se_bounds hu_se_bounds_vanilla(void) {
    hu_se_bounds b = { { HU_SE_VANILLA, 0 }, { HU_SE_VANILLA, HU_STAT_EXP_MAX } };
    return b;
}

uint16_t hu_se_preset_value(int mode, hu_se_side side) {
    /* round((k / 4)^2 * 65535), k = 0..4 */
    static const uint16_t kCurve[5] = { 0, 4096, 16384, 36863, 65535 };
    int k;
    if (mode < HU_SE_CASUAL || mode > HU_SE_KAIZO) return 0;
    k = mode - HU_SE_CASUAL;
    if (side == HU_SE_RENTAL) k = 4 - k; /* the labels are difficulty: Kaizo rentals are the weakest */
    return kCurve[k];
}

bool hu_se_bound_value(const hu_se_bound* bound, hu_se_side side, uint16_t* out) {
    if (bound->mode >= HU_SE_CASUAL && bound->mode <= HU_SE_KAIZO) {
        *out = hu_se_preset_value(bound->mode, side);
        return true;
    }
    if (bound->mode == HU_SE_CUSTOM) {
        *out = bound->custom;
        return true;
    }
    return false;
}

bool hu_se_bounds_active(const hu_se_bounds* bounds) {
    uint16_t v;
    return hu_se_bound_value(&bounds->min, HU_SE_OPPONENT, &v) || hu_se_bound_value(&bounds->max, HU_SE_OPPONENT, &v);
}

bool hu_se_bounds_fix(hu_se_bounds* bounds, hu_se_side side, bool changed_min) {
    uint16_t lo, hi;
    if (!hu_se_bound_value(&bounds->min, side, &lo) || !hu_se_bound_value(&bounds->max, side, &hi) || lo <= hi) {
        return false;
    }
    if (changed_min) {
        bounds->max = bounds->min;
    } else {
        bounds->min = bounds->max;
    }
    return true;
}

void hu_se_bounds_apply(const hu_se_bounds* bounds, hu_se_side side, uint16_t stat_exp[HU_SE_STAT_COUNT]) {
    uint16_t lo, hi;
    const bool has_lo = hu_se_bound_value(&bounds->min, side, &lo);
    const bool has_hi = hu_se_bound_value(&bounds->max, side, &hi);
    int i;
    for (i = 0; i < HU_SE_STAT_COUNT; i++) {
        if (has_lo && stat_exp[i] < lo) stat_exp[i] = lo;
        if (has_hi && stat_exp[i] > hi) stat_exp[i] = hi;
    }
}
