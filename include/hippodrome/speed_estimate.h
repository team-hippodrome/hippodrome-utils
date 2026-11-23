/* Speed stat estimates (docs/specs/SPEED_STAT_ESTIMATE.md): a min/max range for the raw Speed stat of a Pokémon whose
 * stats are hidden, narrowed by the turn order.
 *
 * Derived from the Stadium decomps: Zero's Battle_DetermineMoveOrder (turn_order.c) lets Quick Attack go first and
 * Counter last, otherwise compares the two `speed` values (stage and paralysis applied) and breaks a tie with
 * Battle_Random() < 0x80. Which moves share a priority bracket is the port's call (it knows its move table, custom
 * moves included); this module only needs to hear that Speed decided the order.
 *
 * The range is kept as the raw stat (no stage, no paralysis), so stat changes never lose information:
 * hu_speed_observe() converts what the turn order showed back to the raw stat with the Pokémon's visible modifiers,
 * and hu_speed_shown() applies them again for display. */
#ifndef HIPPODROME_SPEED_ESTIMATE_H
#define HIPPODROME_SPEED_ESTIMATE_H

#include <stdbool.h>
#include <stdint.h>

#include "hippodrome/stats.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hu_speed_range {
    uint16_t min;
    uint16_t max;
} hu_speed_range;

/* The species' widest range at `level` under the base rules: 0 DVs and 0 stat experience to 15 DVs and 65535. */
hu_speed_range hu_speed_initial(uint8_t base_speed, uint8_t level);
/* The widest range when every stat experience value is held in [se_min, se_max] (Stat Exp difficulty,
 * docs/specs/STATEXP_DIFFICULTY.md): 0 DVs at se_min to 15 DVs at se_max. hu_speed_initial is (0, 65535). */
hu_speed_range hu_speed_initial_se(uint8_t base_speed, uint8_t level, uint16_t se_min, uint16_t se_max);

/* A range learned while stat experience was held in [from_min, from_max], as it is when it is held in
 * [to_min, to_max] instead (the spec: the estimate follows a changed Stat Exp setting). The Pokémon's own DVs and
 * stat experience are unknown; each value the game clamps into the bounds. The result keeps exactly the DV / stat
 * experience pairs the old range allowed. A range no pair fits gives hu_speed_initial_se(to). */
hu_speed_range hu_speed_rescale(hu_speed_range r, uint8_t base_speed, uint8_t level, uint16_t from_min,
                                uint16_t from_max, uint16_t to_min, uint16_t to_max);

/* An exact value (min == max), for a Pokémon whose stats the viewer knows. */
hu_speed_range hu_speed_exact(uint16_t speed);

bool hu_speed_is_exact(hu_speed_range r);

typedef enum hu_turn_result {
    HU_MOVED_FIRST = 0,  /* the subject acted before the opponent */
    HU_MOVED_SECOND = 1, /* the subject acted after the opponent */
} hu_turn_result;

/* One turn where both sides used moves of the same priority bracket, so Speed decided the order (a tie is broken at
 * random, so moving first means "at least as fast", moving second "at most as fast").
 * `opponent_speed` is the opponent's speed exactly as the turn order compared it; `mods` are the subject's own visible
 * modifiers under `rules`. Narrows *r and returns true when it changed. An observation that contradicts the range (it
 * would leave it empty) is ignored and returns false. */
bool hu_speed_observe(hu_speed_range* r, hu_turn_result result, uint16_t opponent_speed, const hu_speed_rules* rules,
                      hu_speed_mods mods);

/* The range with the modifiers applied, for display next to the opponent's live speed. */
hu_speed_range hu_speed_shown(hu_speed_range r, const hu_speed_rules* rules, hu_speed_mods mods);

/* Intersection of two ranges (e.g. what this battle saw with what the save remembers). If they do not overlap, `b`
 * wins (the newer observation). */
hu_speed_range hu_speed_intersect(hu_speed_range a, hu_speed_range b);

#ifdef __cplusplus
}
#endif
#endif
