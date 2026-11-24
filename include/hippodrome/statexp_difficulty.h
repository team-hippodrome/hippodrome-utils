/* Stat Exp difficulty (docs/specs/STATEXP_DIFFICULTY.md): use hu_se_bounds for each side, with independent Minimum
 * and Maximum settings for rental Pokémon and opponent (COM) Pokémon. Each bound is Vanilla, a preset or Custom.
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
    HU_SE_CASUAL,
    HU_SE_EASY,
    HU_SE_MEDIUM,
    HU_SE_HARD,
    HU_SE_KAIZO,
    HU_SE_CUSTOM,
    HU_SE_MODE_COUNT
} hu_se_mode;

/* Whose Pokémon a setting is for: harder means more Stat Exp for opponents and less for the player's rentals. */
typedef enum hu_se_side { HU_SE_OPPONENT = 0, HU_SE_RENTAL = 1 } hu_se_side;

/* The mode's name for a UI ("Vanilla", "Casual", ..., "Custom"); "" for an unknown mode. */
const char* hu_se_mode_name(int mode);

/* ---- Deprecated single-setting API ----
 *
 * hu_se_setting and its operations are kept for compatibility with existing callers. They predate the current
 * spec: each preset selects a range in fifths of 65535, from 0..13107 (Casual) to 52428..65535 (Kaizo) for
 * opponents and the reverse for rentals. New callers should use hu_se_bounds and the quadratic presets below. */

/* One setting (rental or opponent). The custom bounds are kept while a preset is picked, so switching back to
 * Custom finds them again. */
typedef struct hu_se_setting {
    uint8_t mode; /* hu_se_mode */
    uint16_t custom_min;
    uint16_t custom_max;
} hu_se_setting;

/* Vanilla, with custom bounds 0-65535. */
hu_se_setting hu_se_vanilla(void);

/* The range the setting stands for, in *min / *max; false (and nothing written) for Vanilla, an unknown mode or a
 * custom range with errors. */
bool hu_se_range(const hu_se_setting* setting, hu_se_side side, uint16_t* min, uint16_t* max);

/* What is wrong with a custom range (0 when nothing is, and always 0 for the other modes). */
enum { HU_SE_ORDER = 1 << 0 /* maximum below the minimum */ };
unsigned hu_se_errors(const hu_se_setting* setting);
/* The message for one error bit (English), "" for 0 or an unknown bit. */
const char* hu_se_error_text(unsigned error_bit);

/* True when the setting changes anything (hu_se_range would return true). */
bool hu_se_active(const hu_se_setting* setting); /* (the same for both sides) */

/* Clamps each of the five stat experience values into the setting's range in place (nothing when not active). */
void hu_se_apply(const hu_se_setting* setting, hu_se_side side, uint16_t stat_exp[HU_SE_STAT_COUNT]);

/* ---- Four bounds (the reworked spec, 2026-11) ----
 *
 * Each group (rental or opponent) has a Minimum and a Maximum, set separately: Vanilla (that bound leaves the game's
 * values alone: no floor for the Minimum, no ceiling for the Maximum), one of the five presets or Custom (its own
 * value). The presets sit on a quadratic curve, round((k / 4)^2 * 65535) for k = 0..4 (0, 4096, 16384, 36863, 65535),
 * so each step adds about a quarter of the most Stat Exp can add to a stat (its bonus grows with the square root);
 * for opponents Casual is 0 and Kaizo 65535, for rentals the other way round. A Minimum above the Maximum is never
 * left in place: hu_se_bounds_fix moves the bound the player did not just change to the other one. The settings are
 * not applied in Free Battle (the port's call: it knows its modes). */

/* One bound: `mode` is a hu_se_mode (HU_SE_VANILLA ... HU_SE_CUSTOM); `custom` is kept while a preset is picked. */
typedef struct hu_se_bound {
    uint8_t mode;
    uint16_t custom;
} hu_se_bound;

typedef struct hu_se_bounds {
    hu_se_bound min;
    hu_se_bound max;
} hu_se_bounds;

/* Both Vanilla (custom values 0 and 65535). */
hu_se_bounds hu_se_bounds_vanilla(void);
/* A preset's Stat Exp for `side` (HU_SE_CASUAL ... HU_SE_KAIZO); 0 for any other mode. */
uint16_t hu_se_preset_value(int mode, hu_se_side side);
/* The bound's value in *out; false (nothing written) for Vanilla or an unknown mode. */
bool hu_se_bound_value(const hu_se_bound* bound, hu_se_side side, uint16_t* out);
/* Writes the numeric bounds for hu_speed_initial_se and hu_tm_record_speed_se. Vanilla or an unknown mode uses
 * 0 for the Minimum and 65535 for the Maximum. The values are not reordered: validate edits with hu_se_bounds_fix. */
void hu_se_bounds_se_range(const hu_se_bounds* bounds, hu_se_side side, uint16_t* se_min, uint16_t* se_max);
/* True when either bound is not Vanilla. */
bool hu_se_bounds_active(const hu_se_bounds* bounds);
/* After the player changed one bound (`changed_min`: the Minimum, else the Maximum): when neither is Vanilla and the
 * Minimum's value is above the Maximum's, the other bound takes the changed one's setting (so their values match).
 * Returns true when it changed something. */
bool hu_se_bounds_fix(hu_se_bounds* bounds, hu_se_side side, bool changed_min);
/* Raises each of the five values to the Minimum (unless Vanilla), then lowers it to the Maximum (unless Vanilla). */
void hu_se_bounds_apply(const hu_se_bounds* bounds, hu_se_side side, uint16_t stat_exp[HU_SE_STAT_COUNT]);

#ifdef __cplusplus
}
#endif
#endif
