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
