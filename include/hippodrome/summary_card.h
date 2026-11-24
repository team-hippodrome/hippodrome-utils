/* Side summary bars (docs/specs/PARTY_SUMMARY_BARS.md): one card per Pokémon of each side of a battle, showing its
 * state, its model, its HP, the moves the viewer knows and its speed estimate.
 *
 * This is the part every port shares: what a card knows, which state it is in, how its model should look (tint,
 * animation, speed) and which moves it may show. Ports feed it from their battle engine and draw it themselves. */
#ifndef HIPPODROME_SUMMARY_CARD_H
#define HIPPODROME_SUMMARY_CARD_H

#include <stdbool.h>
#include <stdint.h>

#include "hippodrome/speed_estimate.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HU_SIDE_MAX 6
#define HU_CARD_MOVES 4

typedef enum hu_card_state {
    HU_CARD_PICKED = 0,     /* brought to the battle (picked on the order screen), healthy */
    HU_CARD_NOT_PICKED = 1, /* left out of a battle where fewer than the full party are brought */
    HU_CARD_STATUSED = 2,   /* brought, with a status condition */
    HU_CARD_FAINTED = 3,
    HU_CARD_UNKNOWN = 4, /* the opponent has not shown whether it brought this one yet */
} hu_card_state;

typedef enum hu_status {
    HU_STATUS_NONE = 0,
    HU_STATUS_SLEEP,
    HU_STATUS_POISON,
    HU_STATUS_TOXIC,
    HU_STATUS_PARALYSIS,
    HU_STATUS_FREEZE,
    HU_STATUS_BURN,
} hu_status;

enum hu_card_flags {
    HU_CARD_F_PICK_KNOWN = 1 << 0, /* the viewer knows whether it was picked */
    HU_CARD_F_PICKED = 1 << 1,
    HU_CARD_F_SENT_OUT = 1 << 2, /* has been in battle */
    HU_CARD_F_ACTIVE = 1 << 3,   /* in battle now */
    HU_CARD_F_FAINTED = 1 << 4,
    HU_CARD_F_SUBSTITUTE = 1 << 5, /* behind a substitute: the model does not change for it */
};

typedef struct hu_card {
    uint8_t species;       /* national dex number; 0 = empty card */
    uint8_t model_species; /* what the model shows: the Transform target while transformed, else species */
    uint8_t level;
    uint8_t status; /* hu_status */
    uint16_t hp;
    uint16_t max_hp;
    uint16_t moves[HU_CARD_MOVES]; /* 16-bit ids: custom moves from mods fit */
    uint8_t revealed; /* bit per move slot the viewer may see */
    uint8_t flags;    /* enum hu_card_flags */
    /* Raw Speed range (stages and paralysis not applied). Exact on the viewer's own side. */
    hu_speed_range speed;
    /* While transformed, the range of the Speed it copied (shown instead of `speed`, never saved). */
    hu_speed_range transform_speed;
    uint16_t faint_frames; /* frames since it fainted, for the faint animation */
} hu_card;

typedef enum hu_reveal_rule {
    HU_REVEAL_ALL = 0,     /* the viewer's own side in single player: every move is shown */
    HU_REVEAL_AS_USED = 1, /* moves appear once used (COM side; both sides in multiplayer) */
} hu_reveal_rule;

typedef struct hu_side {
    hu_card cards[HU_SIDE_MAX];
    uint8_t count;   /* cards in use (the party size) */
    uint8_t brought; /* how many this battle brings (3 in a 3-of-6 battle, = count when the whole party fights) */
    uint8_t reveal;  /* hu_reveal_rule */
} hu_side;

/* ---- feeding the cards ---- */

/* Starts a side for a new battle: `count` party Pokémon, of which `brought` fight. Clears everything. */
void hu_side_init(hu_side* side, int count, int brought, hu_reveal_rule reveal);
/* Fills card `i` from the party (species, level, HP, the moveset). The speed range starts as the species' widest
 * (hu_speed_initial) on a hidden side and exact on the viewer's own (`known_speed` != 0). */
void hu_side_set_mon(hu_side* side, int i, uint8_t species, uint8_t level, uint16_t hp, uint16_t max_hp,
                     const uint16_t moves[HU_CARD_MOVES], uint8_t base_speed, uint16_t known_speed);
/* The viewer knows which ones were picked (its own side, or a game that shows both picks): `picked_mask` bit i. */
void hu_side_set_picked(hu_side* side, uint8_t picked_mask);
/* Card `i` came into battle (marks it picked, active, and the others inactive). Once as many have been seen as the side
 * brings, the cards still unknown become Not Picked. */
void hu_side_send_out(hu_side* side, int i);
/* Card `i` attempted the move in `slot` (0-3): reveals that slot even on a miss or failure. Do not call if it could
 * not act. */
void hu_side_move_used(hu_side* side, int i, int slot);
/* Reveals the move id `move` on card `i` if it is in its moveset (for trainer memory); returns whether it was. */
bool hu_side_reveal_move(hu_side* side, int i, uint16_t move);
void hu_side_set_hp(hu_side* side, int i, uint16_t hp);
void hu_side_set_status(hu_side* side, int i, hu_status status);
/* Fainted (hp 0): starts the faint animation. */
void hu_side_faint(hu_side* side, int i);
/* Transform: the model shows `species` and the speed shows `copied` until the Pokémon leaves battle. */
void hu_side_transform(hu_side* side, int i, uint8_t species, hu_speed_range copied);
void hu_side_set_substitute(hu_side* side, int i, bool on);
/* Card `i` left battle (switched out): transform and substitute end. */
void hu_side_withdraw(hu_side* side, int i);
/* Advances the cards' timers by one frame. */
void hu_side_tick(hu_side* side);

/* ---- reading them ---- */

hu_card_state hu_card_get_state(const hu_card* card);
bool hu_card_move_visible(const hu_card* card, int slot);
/* The Speed range to show: the copied one while transformed, else `speed`. */
hu_speed_range hu_card_shown_speed(const hu_card* card);

typedef enum hu_model_anim {
    HU_ANIM_IDLE = 0,   /* the idle loop */
    HU_ANIM_SLEEP = 1,  /* the sleeping pose/loop */
    HU_ANIM_FROZEN = 2, /* the frozen pose */
    HU_ANIM_FAINT = 3,  /* the faint animation, then held on its last frame */
    HU_ANIM_HOLD = 4,   /* stopped on its current (or first) idle frame */
} hu_model_anim;

typedef struct hu_rgb {
    uint8_t r, g, b;
} hu_rgb;

/* How a card's model should look. The port multiplies the model's colours by (1 - tint_amount) and adds
 * tint * tint_amount (a fog-like blend toward `tint`). */
typedef struct hu_model_look {
    hu_model_anim anim;
    float anim_speed; /* 1 normal, 0.5 for most status conditions, 0 when held */
    hu_rgb tint;
    float tint_amount; /* 0 = no tint */
    bool faint_done;   /* the faint animation has finished: hold and show grey */
} hu_model_look;

/* `faint_length` is how many frames the port's faint animation takes. */
hu_model_look hu_card_model_look(const hu_card* card, uint16_t faint_length);

/* The colour of a status condition (sleep light blue, poison purple, toxic deep purple, paralysis yellow, freeze
 * aqua, burn red). */
hu_rgb hu_status_color(hu_status status);

/* Move and Pokémon types, in a fixed order of the library's own (each port maps its game's type ids onto it). Room for
 * the types of later generations and mods. */
typedef enum hu_type {
    HU_TYPE_NORMAL = 0,
    HU_TYPE_FIGHTING,
    HU_TYPE_FLYING,
    HU_TYPE_POISON,
    HU_TYPE_GROUND,
    HU_TYPE_ROCK,
    HU_TYPE_BUG,
    HU_TYPE_GHOST,
    HU_TYPE_STEEL,
    HU_TYPE_FIRE,
    HU_TYPE_WATER,
    HU_TYPE_GRASS,
    HU_TYPE_ELECTRIC,
    HU_TYPE_PSYCHIC,
    HU_TYPE_ICE,
    HU_TYPE_DRAGON,
    HU_TYPE_DARK,
    HU_TYPE_FAIRY,
    HU_TYPE_UNKNOWN, /* ??? (Curse in Gen 2), or an id the port does not know */
    HU_TYPE_COUNT
} hu_type;

/* The type's usual colour (Normal tan, Fire orange, Water blue, ...). */
hu_rgb hu_type_color(hu_type type);
/* The same colour lightened so text in it reads on a dark card. */
hu_rgb hu_type_text_color(hu_type type);

typedef enum hu_hp_band { HU_HP_GREEN = 0, HU_HP_YELLOW = 1, HU_HP_RED = 2 } hu_hp_band;
/* The Game Boy's HP bar colours: green above about 56% (27 of 48 pixels), yellow down to about 21% (10 of 48), then
 * red. */
hu_hp_band hu_hp_band_of(uint16_t hp, uint16_t max_hp);
/* Width of the bar's fill for a bar `width` units wide (at least 1 while hp > 0). */
int hu_hp_fill(uint16_t hp, uint16_t max_hp, int width);

#ifdef __cplusplus
}
#endif
#endif
