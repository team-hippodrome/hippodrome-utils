/* Hue choice (docs/specs/HUE_CHANGE.md): a hue the player picks for each Pokémon, kept in a side file next to the
 * game's save, in place of the one the Stadium games derive from the bytes of its nickname.
 *
 * A Pokémon is identified by a fingerprint of the bytes that make it that Pokémon (hu_hue_id_of: the port hashes
 * species, original trainer, nickname, DVs, ... in its own game's encoding), so the choice follows it wherever the game
 * shows it (party screens, menus, battle) and survives being moved between boxes or carts. A Pokémon with no stored
 * choice keeps the hue its game would give it (hu_hue_nickname_shift), which is no change for one without a
 * nickname.
 *
 * Hues are in the games' own unit: the model renderer adds `shift` / 64 degrees to every colour's HSV hue
 * (Stadium Zero's Color_AdjustRGB5551; Stadium 1 has the same routine), and 0 leaves the colours untouched. */
#ifndef HIPPODROME_HUE_CHOICE_H
#define HIPPODROME_HUE_CHOICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HU_HUE_UNITS_PER_DEGREE 64
#define HU_HUE_FULL_TURN (360 * HU_HUE_UNITS_PER_DEGREE) /* 23040 */
/* The picker's steps: the original colours (0) and every 15 degrees round the colour wheel. */
#define HU_HUE_STEPS 24
#define HU_HUE_STEP (HU_HUE_FULL_TURN / HU_HUE_STEPS)

/* A Pokémon's fingerprint. 0 is never a valid id. */
typedef uint64_t hu_hue_id;

/* The fingerprint of `size` bytes (FNV-1a 64; never 0). Ports pass the fields that make a Pokémon itself, in a fixed
 * order, and include their game (enum hu_game, trainer_memory.h) so two games' Pokémon never share one. */
hu_hue_id hu_hue_id_of(const void* data, size_t size);
/* Adds more bytes to a fingerprint started with hu_hue_id_of (for fields that are not next to each other). */
hu_hue_id hu_hue_id_add(hu_hue_id id, const void* data, size_t size);

/* The hue the Stadium games give a nicknamed Pokémon: `sum` is the low byte of the sum the game makes of its OT id
 * and the bytes of its nickname and OT name, and [min_degrees, max_degrees] the species' range from the game's
 * species table. (Stadium Zero: Model_ComputeNicknameColorVariant.) A Pokémon without a nickname gets 0. */
int16_t hu_hue_nickname_shift(uint8_t sum, int16_t min_degrees, int16_t max_degrees);

/* The next hue on the picker from `shift` in `direction` (+1 / -1): the steps are 0 (the original colours) and every
 * HU_HUE_STEP, round the wheel; a hue between two steps (a nickname's) goes to the nearer one in that direction. */
int16_t hu_hue_step(int16_t shift, int direction);
/* `shift` in whole degrees, 0..359. */
int hu_hue_degrees(int16_t shift);

/* Restricted hues: only the ones the game itself can give the species, its range [min_degrees, max_degrees] from the
 * species table (the one hu_hue_nickname_shift takes; every species' range holds 0, the original colours). A range of
 * a full turn or more allows every hue. */
/* `shift` as a signed hue, -180 degrees (exclusive) to +180 (inclusive): 330 degrees is -30. */
int16_t hu_hue_signed(int16_t shift);
/* True when `shift` lies in [min_degrees, max_degrees] (going round the wheel the short way, as hu_hue_signed; a range
 * crossing the +-180 wrap point counts both parts, though the games' species tables have none). */
bool hu_hue_in_range(int16_t shift, int16_t min_degrees, int16_t max_degrees);
/* `shift` moved to the nearer end of the range when it lies outside it (unchanged inside it). */
int16_t hu_hue_clamp(int16_t shift, int16_t min_degrees, int16_t max_degrees);
/* hu_hue_step inside the range: the picker's steps (0 and every HU_HUE_STEP) that lie in it plus its two ends, without
 * going round the wheel; at an end it stays there. A hue outside the range starts from its hu_hue_clamp. */
int16_t hu_hue_step_in_range(int16_t shift, int direction, int16_t min_degrees, int16_t max_degrees);

typedef struct hu_hue_store hu_hue_store;

hu_hue_store* hu_hue_store_new(void);
void hu_hue_store_free(hu_hue_store* s);
void hu_hue_store_clear(hu_hue_store* s);

/* The stored choice for `id`: true and *shift, or false when the player has not picked one (use the game's own). */
bool hu_hue_get(const hu_hue_store* s, hu_hue_id id, int16_t* shift);
/* Stores the player's choice (any value; it is kept modulo a full turn, 0 = the original colours). */
void hu_hue_set(hu_hue_store* s, hu_hue_id id, int16_t shift);
/* Forgets the choice: the Pokémon goes back to the hue its game gives it. */
void hu_hue_unset(hu_hue_store* s, hu_hue_id id);
/* What to draw `id` with: the stored choice, else `game_default`. */
int16_t hu_hue_resolve(const hu_hue_store* s, hu_hue_id id, int16_t game_default);

size_t hu_hue_count(const hu_hue_store* s);
/* True after any change since the last load/save/clear. */
bool hu_hue_dirty(const hu_hue_store* s);

typedef enum hu_hue_io {
    HU_HUE_OK = 0,
    HU_HUE_MISSING = 1,   /* load: no file yet; the store is empty */
    HU_HUE_IO_ERROR = -1, /* could not read/write the file */
    HU_HUE_CORRUPT = -2,  /* load: not a hue file, or damaged; the store is empty */
} hu_hue_io;

/* Replaces the store with the file's contents. Paths are UTF-8 (on Windows too). */
hu_hue_io hu_hue_load(hu_hue_store* s, const char* path);
/* Writes the store through a temporary file, so a crash never leaves a half-written file. */
hu_hue_io hu_hue_save(hu_hue_store* s, const char* path);
/* The same as bytes. hu_hue_serialize returns the size and fills *out with malloc'd memory the caller frees. */
size_t hu_hue_serialize(const hu_hue_store* s, uint8_t** out);
hu_hue_io hu_hue_deserialize(hu_hue_store* s, const uint8_t* data, size_t size);

#ifdef __cplusplus
}
#endif
#endif
