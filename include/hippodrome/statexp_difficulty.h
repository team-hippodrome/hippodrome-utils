/* Stat Exp difficulty (docs/specs/STATEXP_DIFFICULTY.md): the player sets the lowest and highest stat experience
 * that rental Pokémon and opponent (COM) Pokémon can have, as two separate settings.
 *
 * Each setting is Vanilla (the game's own values, the default), one of five presets that split 0..65535 into fifths
 * (Casual 0-13107, Easy 13107-26214, Medium 26214-39321, Hard 39321-52428, Kaizo 52428-65535), or Custom (the
 * player's own lowest and highest value). A range moves each of the five stat experience values (HP, Attack,
 * Defense, Speed, Special: Gen 1 and 2 store one per stat, none is derived) that lies outside it to the nearer end
 * (a clamp); values inside it are left as the game made them. A custom range whose maximum is below its minimum is
 * an error (hu_se_errors): the port shows it and the setting acts as vanilla until it is fixed.
 *
 * DVs are left alone: in Gen 1/2 the HP DV is made of the low bits of the other four, so a DV range could not hold
 * for HP (descoped from the spec).
 *
 * Nothing here knows a game's memory layout: the port finds the rental and COM Pokémon, applies the range and
 * recalculates the stats with its game's own formula (or hu_gb_stat / hu_gb_hp, stats.h). */
#ifndef HIPPODROME_STATEXP_DIFFICULTY_H
#define HIPPODROME_STATEXP_DIFFICULTY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HU_SE_STAT_COUNT 5 /* HP, Attack, Defense, Speed, Special */

typedef enum hu_se_mode {
    HU_SE_VANILLA = 0, /* the game's own values (the default) */
    HU_SE_CASUAL,      /* 0 - 13107 */
    HU_SE_EASY,        /* 13107 - 26214 */
    HU_SE_MEDIUM,      /* 26214 - 39321 */
    HU_SE_HARD,        /* 39321 - 52428 */
    HU_SE_KAIZO,       /* 52428 - 65535 */
    HU_SE_CUSTOM,      /* custom_min - custom_max */
    HU_SE_MODE_COUNT
} hu_se_mode;

/* One setting (rental or opponent). The custom bounds are kept while a preset is picked, so switching back to
 * Custom finds them again. */
typedef struct hu_se_setting {
    uint8_t mode; /* hu_se_mode */
    uint16_t custom_min;
    uint16_t custom_max;
} hu_se_setting;

/* Vanilla, with custom bounds 0-65535. */
hu_se_setting hu_se_vanilla(void);
/* The mode's name for a UI ("Vanilla", "Casual", ..., "Custom"); "" for an unknown mode. */
const char* hu_se_mode_name(int mode);

/* The range the setting stands for, in *min / *max; false (and nothing written) for Vanilla, an unknown mode or a
 * custom range with errors. */
bool hu_se_range(const hu_se_setting* setting, uint16_t* min, uint16_t* max);

/* What is wrong with a custom range (0 when nothing is, and always 0 for the other modes). */
enum { HU_SE_ORDER = 1 << 0 /* maximum below the minimum */ };
unsigned hu_se_errors(const hu_se_setting* setting);
/* The message for one error bit (English), "" for 0 or an unknown bit. */
const char* hu_se_error_text(unsigned error_bit);

/* True when the setting changes anything (hu_se_range would return true). */
bool hu_se_active(const hu_se_setting* setting);

/* Clamps each of the five stat experience values into the setting's range in place (nothing when not active). */
void hu_se_apply(const hu_se_setting* setting, uint16_t stat_exp[HU_SE_STAT_COUNT]);

#ifdef __cplusplus
}
#endif
#endif
