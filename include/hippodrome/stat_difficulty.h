/* DV and Stat Exp difficulty (docs/specs/DV_STATEXP_DIFFICULTY.md): the player sets the lowest and highest DVs and
 * stat experience that rental Pokémon and opponent (COM) Pokémon can have, as two separate settings.
 *
 * Each setting is either vanilla (the game's own values, the default) or a custom range. A custom range moves every
 * DV and stat experience value that lies outside it to the nearer end (a clamp): values inside the range are left as
 * the game made them, so the full range (DV 0-15, stat exp 0-65535) plays as vanilla and min == max gives every
 * Pokémon that exact value. A range whose maximum is below its minimum is an error (hu_sd_errors): the port shows it
 * and the setting acts as vanilla until it is fixed.
 *
 * The DV word is the Game Boy one that all three Stadium games use (Gen 1 and 2): 4 bits each, Attack in bits 12-15,
 * Defense 8-11, Speed 4-7, Special 0-3. The HP DV is not stored: it is made of the low bit of each of the other four
 * (hu_dv_hp), so it follows the clamped DVs as on the Game Boy and can end up outside the range (for example 8-15 can
 * give an HP DV of 0). Stat experience is the five words HP, Attack, Defense, Speed, Special.
 *
 * Nothing here knows a game's memory layout: the port finds the rental and COM Pokémon, applies the range and
 * recalculates the stats with its game's own formula (or hu_gb_stat / hu_gb_hp, stats.h). In Gen 2 the DVs also
 * decide gender, shininess and Unown's letter, which a clamp can change: Stadium 2's port decides what to do there. */
#ifndef HIPPODROME_STAT_DIFFICULTY_H
#define HIPPODROME_STAT_DIFFICULTY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HU_SD_STAT_EXP_COUNT 5 /* HP, Attack, Defense, Speed, Special */

/* One setting (rental or opponent). */
typedef struct hu_sd_range {
    bool custom;           /* false: vanilla, the game's own values (the default) */
    uint8_t dv_min;        /* 0..15 */
    uint8_t dv_max;        /* 0..15 */
    uint16_t stat_exp_min; /* 0..65535 */
    uint16_t stat_exp_max; /* 0..65535 */
} hu_sd_range;

/* Vanilla, with the full range ready for when the player turns it on (DV 0-15, stat exp 0-65535). */
hu_sd_range hu_sd_vanilla(void);

/* What is wrong with a range (0 when nothing is): a bit set of these. */
enum {
    HU_SD_DV_ORDER = 1 << 0,       /* DV maximum below the minimum */
    HU_SD_STAT_EXP_ORDER = 1 << 1, /* stat exp maximum below the minimum */
    HU_SD_DV_TOO_HIGH = 1 << 2,    /* a DV bound over 15 */
};
unsigned hu_sd_errors(const hu_sd_range* range);
/* The message for one error bit (English; the ports' UIs are English), "" for 0 or an unknown bit. */
const char* hu_sd_error_text(unsigned error_bit);

/* True when the range changes anything: custom and without errors. */
bool hu_sd_active(const hu_sd_range* range);

/* One DV out of the DV word: HU_SD_DV_ATTACK etc. */
enum { HU_SD_DV_ATTACK = 0, HU_SD_DV_DEFENSE = 1, HU_SD_DV_SPEED = 2, HU_SD_DV_SPECIAL = 3 };
uint8_t hu_dv_get(uint16_t dvs, int which);
/* The HP DV the Game Boy games derive: Attack's low bit << 3 | Defense's << 2 | Speed's << 1 | Special's. */
uint8_t hu_dv_hp(uint16_t dvs);

/* The DV word with each of its four DVs clamped into the range (unchanged when the range is not active). */
uint16_t hu_sd_apply_dvs(const hu_sd_range* range, uint16_t dvs);
/* Clamps each of the five stat experience values into the range in place (nothing when the range is not active). */
void hu_sd_apply_stat_exp(const hu_sd_range* range, uint16_t stat_exp[HU_SD_STAT_EXP_COUNT]);

#ifdef __cplusplus
}
#endif
#endif
