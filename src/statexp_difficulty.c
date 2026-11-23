#include "hippodrome/statexp_difficulty.h"

#include "hippodrome/stats.h"

/* round((k / 4)^2 * 65535), k = 0..4 */
static const uint16_t kCurve[5] = { 0, 4096, 16384, 36863, 65535 };

hu_se_setting hu_se_vanilla(void) {
    hu_se_setting s = { { HU_SE_VANILLA, 0 }, { HU_SE_VANILLA, HU_STAT_EXP_MAX } };
    return s;
}

const char* hu_se_preset_name(int preset) {
    static const char* const kNames[HU_SE_PRESET_COUNT] = { "Vanilla", "Casual", "Easy",  "Medium",
                                                            "Hard",    "Kaizo",  "Custom" };
    return (preset >= 0 && preset < HU_SE_PRESET_COUNT) ? kNames[preset] : "";
}

uint16_t hu_se_preset_value(int preset, hu_se_side side) {
    int k;
    if (preset < HU_SE_CASUAL || preset > HU_SE_KAIZO) return 0;
    k = preset - HU_SE_CASUAL;
    return kCurve[side == HU_SE_RENTAL ? 4 - k : k];
}

bool hu_se_bound_value(const hu_se_bound* bound, hu_se_side side, uint16_t* value) {
    if (bound->preset >= HU_SE_CASUAL && bound->preset <= HU_SE_KAIZO) {
        *value = hu_se_preset_value(bound->preset, side);
        return true;
    }
    if (bound->preset == HU_SE_CUSTOM) {
        *value = bound->custom;
        return true;
    }
    return false;
}

void hu_se_bounds(const hu_se_setting* setting, hu_se_side side, uint16_t* lo, uint16_t* hi) {
    if (!hu_se_bound_value(&setting->min, side, lo)) *lo = 0;
    if (!hu_se_bound_value(&setting->max, side, hi)) *hi = HU_STAT_EXP_MAX;
}

uint16_t hu_se_clamp(uint16_t v, uint16_t lo, uint16_t hi) {
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

bool hu_se_active(const hu_se_setting* setting) {
    uint16_t v;
    return hu_se_bound_value(&setting->min, HU_SE_OPPONENT, &v) || hu_se_bound_value(&setting->max, HU_SE_OPPONENT, &v);
}

void hu_se_apply(const hu_se_setting* setting, hu_se_side side, uint16_t stat_exp[HU_SE_STAT_COUNT]) {
    uint16_t lo, hi;
    int i;
    if (!hu_se_active(setting)) return;
    hu_se_bounds(setting, side, &lo, &hi);
    for (i = 0; i < HU_SE_STAT_COUNT; i++) {
        stat_exp[i] = hu_se_clamp(stat_exp[i], lo, hi);
    }
}

bool hu_se_bound_changed(hu_se_setting* setting, hu_se_side side, bool changed_min) {
    uint16_t lo, hi;
    if (!hu_se_bound_value(&setting->min, side, &lo) || !hu_se_bound_value(&setting->max, side, &hi) || lo <= hi) {
        return false;
    }
    /* the same side's tables, so copying the bound copies its value */
    if (changed_min) {
        setting->max = setting->min;
    } else {
        setting->min = setting->max;
    }
    return true;
}
