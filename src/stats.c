#include "hippodrome/stats.h"

unsigned hu_stat_exp_bonus(uint16_t stat_exp) {
    unsigned root = 1;
    while (root < 255 && root * root < stat_exp) {
        root++;
    }
    return root / 4;
}

static unsigned core(uint8_t base, uint8_t dv, uint16_t stat_exp, uint8_t level) {
    return (((unsigned)base + dv) * 2 + hu_stat_exp_bonus(stat_exp)) * level / 100;
}

static uint16_t cap999(unsigned v) {
    return (uint16_t)(v > 999 ? 999 : v);
}

uint16_t hu_gb_stat(uint8_t base, uint8_t dv, uint16_t stat_exp, uint8_t level) {
    return cap999(core(base, dv, stat_exp, level) + 5);
}

uint16_t hu_gb_hp(uint8_t base, uint8_t dv, uint16_t stat_exp, uint8_t level) {
    return cap999(core(base, dv, stat_exp, level) + level + 10);
}

const hu_speed_rules hu_gb_speed_rules = {
    {
        { 25, 100 }, { 28, 100 }, { 33, 100 }, { 40, 100 }, { 50, 100 }, { 66, 100 }, { 1, 1 },
        { 15, 10 },  { 2, 1 },    { 25, 10 },  { 3, 1 },    { 35, 10 },  { 4, 1 },
    },
    { 1, 4 },
    999,
};

hu_speed_mods hu_speed_mods_none(void) {
    hu_speed_mods m = { 0, 0, { 1, 1 } };
    return m;
}

static unsigned apply_ratio(unsigned v, hu_ratio r) {
    if (r.num == 0 || r.den == 0) {
        return v;
    }
    return v * r.num / r.den;
}

uint16_t hu_apply_stage(const hu_speed_rules* rules, uint16_t stat, int stage) {
    unsigned v;
    if (stage < HU_STAGE_MIN) stage = HU_STAGE_MIN;
    if (stage > HU_STAGE_MAX) stage = HU_STAGE_MAX;
    v = apply_ratio(stat, rules->stages[stage + 6]);
    if (v < 1) v = 1;
    if (rules->cap != 0 && v > rules->cap) v = rules->cap;
    return (uint16_t)v;
}

uint16_t hu_effective_speed(const hu_speed_rules* rules, uint16_t raw, hu_speed_mods mods) {
    unsigned v = hu_apply_stage(rules, raw, mods.stage);
    for (unsigned i = 0; i < mods.paralysis_cuts && v > 1; i++) {
        v = apply_ratio(v, rules->paralysis);
        if (v < 1) v = 1;
    }
    v = apply_ratio(v, mods.extra);
    if (v < 1) v = 1;
    return (uint16_t)(v > 0xFFFF ? 0xFFFF : v);
}
