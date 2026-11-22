#include "hippodrome/stat_difficulty.h"

#include "hippodrome/stats.h"

hu_sd_range hu_sd_vanilla(void) {
    hu_sd_range r = { false, 0, HU_DV_MAX, 0, HU_STAT_EXP_MAX };
    return r;
}

unsigned hu_sd_errors(const hu_sd_range* range) {
    unsigned e = 0;
    if (range->dv_max < range->dv_min) e |= HU_SD_DV_ORDER;
    if (range->stat_exp_max < range->stat_exp_min) e |= HU_SD_STAT_EXP_ORDER;
    if (range->dv_min > HU_DV_MAX || range->dv_max > HU_DV_MAX) e |= HU_SD_DV_TOO_HIGH;
    return e;
}

const char* hu_sd_error_text(unsigned error_bit) {
    switch (error_bit) {
    case HU_SD_DV_ORDER:
        return "The highest DV is below the lowest.";
    case HU_SD_STAT_EXP_ORDER:
        return "The highest Stat Exp is below the lowest.";
    case HU_SD_DV_TOO_HIGH:
        return "DVs go from 0 to 15.";
    default:
        return "";
    }
}

bool hu_sd_active(const hu_sd_range* range) {
    return range->custom && hu_sd_errors(range) == 0;
}

uint8_t hu_dv_get(uint16_t dvs, int which) {
    return (uint8_t)((dvs >> (12 - 4 * which)) & 0xF);
}

uint8_t hu_dv_hp(uint16_t dvs) {
    return (uint8_t)(((hu_dv_get(dvs, HU_SD_DV_ATTACK) & 1) << 3) | ((hu_dv_get(dvs, HU_SD_DV_DEFENSE) & 1) << 2) |
                     ((hu_dv_get(dvs, HU_SD_DV_SPEED) & 1) << 1) | (hu_dv_get(dvs, HU_SD_DV_SPECIAL) & 1));
}

uint16_t hu_sd_apply_dvs(const hu_sd_range* range, uint16_t dvs) {
    uint16_t out = 0;
    int i;
    if (!hu_sd_active(range)) return dvs;
    for (i = 0; i < 4; i++) {
        uint8_t dv = hu_dv_get(dvs, i);
        if (dv < range->dv_min) dv = range->dv_min;
        if (dv > range->dv_max) dv = range->dv_max;
        out |= (uint16_t)(dv << (12 - 4 * i));
    }
    return out;
}

void hu_sd_apply_stat_exp(const hu_sd_range* range, uint16_t stat_exp[HU_SD_STAT_EXP_COUNT]) {
    int i;
    if (!hu_sd_active(range)) return;
    for (i = 0; i < HU_SD_STAT_EXP_COUNT; i++) {
        if (stat_exp[i] < range->stat_exp_min) stat_exp[i] = range->stat_exp_min;
        if (stat_exp[i] > range->stat_exp_max) stat_exp[i] = range->stat_exp_max;
    }
}
