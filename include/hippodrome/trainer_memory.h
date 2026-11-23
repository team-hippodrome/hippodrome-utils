/* Trainer memory (docs/specs/TRAINER_MEMORY.md): what the player has learned about each COM trainer's team, kept in an
 * auxiliary save file next to the game's own save.
 *
 * A trainer is identified by hu_tm_key (game, cup, division, round 1/2, trainer index). For each one the store keeps
 * whether the player has reached it, the team it showed (species, level, moveset), and for each of its Pokémon a bit
 * mask over its moveset of the moves the player has seen it use, plus its speed estimate. Move ids are 16-bit, so
 * custom and backported moves from mods fit.
 *
 * Which keys share knowledge (e.g. one game's cups having the same teams) and which modes have no memory at all are
 * game rules: the ports decide that, this store only records and answers. */
#ifndef HIPPODROME_TRAINER_MEMORY_H
#define HIPPODROME_TRAINER_MEMORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hippodrome/speed_estimate.h"

#ifdef __cplusplus
extern "C" {
#endif

enum hu_game {
    HU_GAME_STADIUM_ZERO = 0, /* Plethron */
    HU_GAME_STADIUM_1 = 1,    /* Stadion */
    HU_GAME_STADIUM_2 = 2,    /* Diaulos */
};

#define HU_TM_TEAM_MAX 6
#define HU_TM_MOVES 4
#define HU_TM_NAME_BYTES 32 /* the trainer's name as the game stores it (its own encoding), NUL-padded */

typedef struct hu_tm_key {
    uint8_t game;     /* enum hu_game */
    uint8_t cup;      /* the game's own cup / tournament number */
    uint8_t division; /* the game's own division (Zero: L1-30's ball cup), 0 where there is none */
    uint8_t round;    /* 0 = Round 1, 1 = Round 2 (Stadium 1 and 2); always 0 in Zero */
    uint8_t trainer;  /* the trainer's index within that cup/division (round of the bracket, template number, ...) */
} hu_tm_key;

enum hu_tm_mon_flags {
    HU_TM_MON_HAS_SPEED = 1 << 0, /* speed holds an estimate */
    HU_TM_MON_SENT_OUT = 1 << 1,  /* the player has seen it in battle */
};

typedef struct hu_tm_mon {
    uint8_t species; /* national dex number, 0 = empty slot */
    uint8_t level;
    uint8_t seen;  /* bit i: moves[i] has been seen used */
    uint8_t flags; /* enum hu_tm_mon_flags */
    uint16_t moves[HU_TM_MOVES]; /* the moveset (0 = none / not known yet) */
    hu_speed_range speed;
    /* The stat experience range the opponents' Pokémon were held in when `speed` was learned (Stat Exp difficulty;
     * 0 and 65535 under the base rules). SPEED_STAT_ESTIMATE: the estimate is adjusted when that setting changes. */
    uint16_t se_min, se_max;
} hu_tm_mon;

enum hu_tm_trainer_flags {
    HU_TM_REACHED = 1 << 0,   /* the player got to this trainer (saw the VS screen) */
    HU_TM_TEAM_KNOWN = 1 << 1 /* team[] holds the team it showed */
};

typedef struct hu_tm_trainer {
    hu_tm_key key;
    uint8_t flags; /* enum hu_tm_trainer_flags */
    uint8_t team_count;
    hu_tm_mon team[HU_TM_TEAM_MAX];
    uint8_t name[HU_TM_NAME_BYTES];
} hu_tm_trainer;

typedef struct hu_tm hu_tm;

hu_tm* hu_tm_new(void);
void hu_tm_free(hu_tm* tm);
void hu_tm_clear(hu_tm* tm);

/* Number of trainers stored, and the i-th one (for listing). */
size_t hu_tm_count(const hu_tm* tm);
const hu_tm_trainer* hu_tm_at(const hu_tm* tm, size_t i);

bool hu_tm_key_equal(hu_tm_key a, hu_tm_key b);

/* The trainer's record, or NULL if the player knows nothing about it. */
const hu_tm_trainer* hu_tm_find(const hu_tm* tm, hu_tm_key key);
/* Its Pokémon of `species`, or NULL. */
const hu_tm_mon* hu_tm_find_mon(const hu_tm_trainer* t, uint8_t species);

/* One Pokémon of a team as the game shows it, for hu_tm_record_team(). */
typedef struct hu_tm_team_entry {
    uint8_t species;
    uint8_t level;
    uint16_t moves[HU_TM_MOVES];
} hu_tm_team_entry;

/* The player reached the trainer and saw its team (count <= 6). Keeps what was already learned about Pokémon that are
 * still on the team (matched by species): seen moves that are still in the moveset stay seen, and a Pokémon whose
 * level changed loses its speed estimate. */
void hu_tm_record_team(hu_tm* tm, hu_tm_key key, const hu_tm_team_entry* team, int count);
/* Keeps the trainer's name (up to HU_TM_NAME_BYTES - 1 bytes of `name`, which ends at its first NUL), so a memory
 * screen can show it without loading the game's trainer tables. */
void hu_tm_record_name(hu_tm* tm, hu_tm_key key, const char* name);
/* Only marks the trainer as reached (for games that show no team before the battle). */
void hu_tm_record_reached(hu_tm* tm, hu_tm_key key);
/* The trainer's `species` was sent out / used `move`. Creates the trainer and the Pokémon if needed; a move not in the
 * known moveset takes a free slot (a game that shows no moveset learns it this way), or is dropped if all 4 are known. */
void hu_tm_record_sent_out(hu_tm* tm, hu_tm_key key, uint8_t species, uint8_t level);
void hu_tm_record_move(hu_tm* tm, hu_tm_key key, uint8_t species, uint16_t move);
/* Merges a speed range learned in battle with the stored one (hu_speed_intersect), under the base rules' stat
 * experience (0 to 65535). */
void hu_tm_record_speed(hu_tm* tm, hu_tm_key key, uint8_t species, hu_speed_range range);
/* The same, learned while the opponents' stat experience was held in [se_min, se_max] (Stat Exp difficulty). A stored
 * estimate from another range is first adjusted to this one (hu_speed_rescale), which needs the species' base Speed. */
void hu_tm_record_speed_se(hu_tm* tm, hu_tm_key key, uint8_t species, uint8_t base_speed, hu_speed_range range,
                           uint16_t se_min, uint16_t se_max);
/* The stored speed estimate as it is with the opponents' stat experience held in [se_min, se_max]: adjusted from the
 * range it was learned under. Returns false (and leaves *out) when there is no estimate. */
bool hu_tm_mon_speed(const hu_tm_mon* mon, uint8_t base_speed, uint16_t se_min, uint16_t se_max, hu_speed_range* out);

bool hu_tm_move_seen(const hu_tm_mon* mon, uint16_t move);

/* True after any change since the last load/save/clear. */
bool hu_tm_dirty(const hu_tm* tm);

typedef enum hu_tm_io {
    HU_TM_OK = 0,
    HU_TM_MISSING = 1,   /* load: no file yet; the store is empty */
    HU_TM_IO_ERROR = -1, /* could not read/write the file */
    HU_TM_CORRUPT = -2,  /* load: not a trainer memory file, or damaged; the store is empty */
} hu_tm_io;

/* Replaces the store with the file's contents. Paths are UTF-8 (on Windows too). */
hu_tm_io hu_tm_load(hu_tm* tm, const char* path);
/* Writes the store to `path` through a temporary file, so a crash never leaves a half-written file. */
hu_tm_io hu_tm_save(hu_tm* tm, const char* path);

/* The same as bytes, for ports that keep files in their own way (e.g. Android storage). hu_tm_serialize returns the
 * size and fills *out with malloc'd memory the caller frees. */
size_t hu_tm_serialize(const hu_tm* tm, uint8_t** out);
hu_tm_io hu_tm_deserialize(hu_tm* tm, const uint8_t* data, size_t size);

#ifdef __cplusplus
}
#endif
#endif
