/* Stat formulas and Speed modifiers.
 *
 * The stat formula is the Game Boy one that all three Stadium games use (Gen 1 and 2), checked against Stadium Zero's
 * Battle_ComputePokemonStat in pokestadium0. Speed modifiers are data (hu_speed_rules) rather than constants, so a
 * port or a mod with other rules (custom or backported moves, a paralysis that halves instead of quarters, other
 * stage ratios) passes its own table. */
#ifndef HIPPODROME_STATS_H
#define HIPPODROME_STATS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HU_STAT_EXP_MAX 65535u
#define HU_DV_MAX 15u
#define HU_STAGE_MIN (-6)
#define HU_STAGE_MAX 6

/* The stat-experience term: the smallest root 1..255 whose square reaches stat_exp (Battle_ComputePokemonStat's loop),
 * over 4. */
unsigned hu_stat_exp_bonus(uint16_t stat_exp);

/* A non-HP stat: ((base + dv) * 2 + bonus) * level / 100 + 5, capped at 999. */
uint16_t hu_gb_stat(uint8_t base, uint8_t dv, uint16_t stat_exp, uint8_t level);
/* HP: ((base + dv) * 2 + bonus) * level / 100 + level + 10, capped at 999. */
uint16_t hu_gb_hp(uint8_t base, uint8_t dv, uint16_t stat_exp, uint8_t level);

typedef struct hu_ratio {
    uint16_t num;
    uint16_t den;
} hu_ratio;

/* How a battle engine turns a raw Speed stat into the speed the turn order compares. */
typedef struct hu_speed_rules {
    hu_ratio stages[13]; /* stage -6..+6 */
    hu_ratio paralysis;  /* applied once per paralysis cut, with a floor of 1 */
    uint16_t cap;        /* highest stat after a stage (999 on the Game Boy) */
} hu_speed_rules;

/* Gen 1 and 2 (all three Stadium games): pokered's StatModifierRatios, the same bytes as Zero's two tables
 * D_8218ADF4 and gStatStageMultipliers; paralysis quarters Speed (Zero's Battle_ApplyParalysisSpeedCut: speed >>= 2,
 * at least 1). */
extern const hu_speed_rules hu_gb_speed_rules;

/* What the player can see that changes a Pokémon's speed. */
typedef struct hu_speed_mods {
    int8_t stage;           /* -6..+6 */
    uint8_t paralysis_cuts; /* how many times the paralysis cut has been applied since its Speed was last recomputed
                             * from the raw stat: 1 while paralysed, more with Gen 1's quirk of cutting again whenever
                             * the opponent's stats change (Zero calls Battle_ApplyParalysisSpeedCut on the other side
                             * after every stat change). Every one of those events is shown to the player. */
    hu_ratio extra;         /* any other multiplier a port or mod applies ({0, 0} or {1, 1} for none) */
} hu_speed_mods;

/* No modifiers. */
hu_speed_mods hu_speed_mods_none(void);

/* A stat with a stage applied: stat * num / den, between 1 and rules->cap. */
uint16_t hu_apply_stage(const hu_speed_rules* rules, uint16_t stat, int stage);

/* Speed as the turn order compares it: stage, then the paralysis cuts, then `extra`. */
uint16_t hu_effective_speed(const hu_speed_rules* rules, uint16_t raw, hu_speed_mods mods);

#ifdef __cplusplus
}
#endif
#endif
