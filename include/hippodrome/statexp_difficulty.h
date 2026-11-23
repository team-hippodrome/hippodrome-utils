/* Stat Exp clamping (docs/specs/STATEXP_DIFFICULTY.md): a lowest and a highest stat experience for the rental Pokémon
 * and for the opponent (COM) Pokémon, four bounds in all, each Vanilla (the default: that bound leaves the game's
 * values alone), one of five presets or Custom.
 *
 * The presets follow the stat exp bonus curve (floor(min(255, ceil(sqrt(stat exp))) / 4)): round((k / 4)^2 * 65535)
 * for k = 0..4 adds about a quarter of the full bonus per step. The labels are difficulty, seen from the player:
 *   opponents: Casual 0, Easy 4096, Medium 16384, Hard 36863, Kaizo 65535
 *   rentals:   Casual 65535, Easy 36863, Medium 16384, Hard 4096, Kaizo 0
 *
 * Applying a group: each of the five values (HP, Attack, Defense, Speed, Special) starts from the game's own, is
 * raised to the lowest bound (if not Vanilla), then lowered to the highest bound (if not Vanilla). The port works the
 * stats out again with its game's formula. hu_se_bound_changed keeps a group's lowest at or below its highest, as the
 * launcher's validation asks. DVs are never changed (the HP DV is made of the other four's parity bits). */
#ifndef HIPPODROME_STATEXP_DIFFICULTY_H
#define HIPPODROME_STATEXP_DIFFICULTY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HU_SE_STAT_COUNT 5 /* HP, Attack, Defense, Speed, Special */

typedef enum hu_se_preset {
    HU_SE_VANILLA = 0, /* this bound leaves the game's values alone (the default) */
    HU_SE_CASUAL,
    HU_SE_EASY,
    HU_SE_MEDIUM,
    HU_SE_HARD,
    HU_SE_KAIZO,
    HU_SE_CUSTOM, /* the player's own value */
    HU_SE_PRESET_COUNT
} hu_se_preset;

/* Whose Pokémon a group is for: harder means more stat exp for opponents and less for rentals. */
typedef enum hu_se_side { HU_SE_OPPONENT = 0, HU_SE_RENTAL = 1 } hu_se_side;

typedef struct hu_se_bound {
    uint8_t preset;  /* hu_se_preset */
    uint16_t custom; /* the value while preset is HU_SE_CUSTOM (kept while another preset is picked) */
} hu_se_bound;

/* One group (rental or opponent). */
typedef struct hu_se_setting {
    hu_se_bound min;
    hu_se_bound max;
} hu_se_setting;

/* All four bounds Vanilla (custom values 0 for a lowest bound, 65535 for a highest). */
hu_se_setting hu_se_vanilla(void);
/* The preset's name for a UI ("Vanilla", "Casual", ..., "Custom"); "" for an unknown one. */
const char* hu_se_preset_name(int preset);
/* A preset's stat exp on `side` (the tables above); 0 for Vanilla, Custom or an unknown preset. */
uint16_t hu_se_preset_value(int preset, hu_se_side side);

/* The bound's value in *value; false (nothing written) when it is Vanilla or unknown. */
bool hu_se_bound_value(const hu_se_bound* bound, hu_se_side side, uint16_t* value);

/* The group as one clamp: *lo = its lowest bound (0 when Vanilla), *hi = its highest (65535 when Vanilla). With
 * hu_se_bound_changed's rule kept, lo <= hi. A Pokémon whose game value is v ends up with hu_se_clamp(v, lo, hi). */
void hu_se_bounds(const hu_se_setting* setting, hu_se_side side, uint16_t* lo, uint16_t* hi);
/* max(v, lo), then min(that, hi): the order the spec gives (a lowest bound above the highest gives hi). */
uint16_t hu_se_clamp(uint16_t v, uint16_t lo, uint16_t hi);

/* True when any of the group's bounds is not Vanilla. */
bool hu_se_active(const hu_se_setting* setting);

/* Applies the group to the five values in place (nothing while both bounds are Vanilla). */
void hu_se_apply(const hu_se_setting* setting, hu_se_side side, uint16_t stat_exp[HU_SE_STAT_COUNT]);

/* After the player changed one bound (`changed_min`: the lowest, else the highest): when neither is Vanilla and the
 * lowest is now above the highest, the other bound takes the changed one's value (a raised lowest bumps the highest
 * up, a lowered highest pulls the lowest down). Returns true when it changed the other bound. */
bool hu_se_bound_changed(hu_se_setting* setting, hu_se_side side, bool changed_min);

#ifdef __cplusplus
}
#endif
#endif
