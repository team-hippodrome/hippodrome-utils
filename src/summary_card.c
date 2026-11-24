#include "hippodrome/summary_card.h"

#include <string.h>

static hu_card* card_at(hu_side* side, int i) {
    return (i >= 0 && i < side->count) ? &side->cards[i] : NULL;
}

void hu_side_init_order_select(hu_side* side, int count, int brought, hu_reveal_rule reveal) {
    memset(side, 0, sizeof(*side));
    if (count < 0) count = 0;
    if (count > HU_SIDE_MAX) count = HU_SIDE_MAX;
    if (brought <= 0 || brought > count) brought = count;
    side->count = (uint8_t)count;
    side->brought = (uint8_t)brought;
    side->reveal = (uint8_t)reveal;
}

void hu_side_init(hu_side* side, int count, int brought, hu_reveal_rule reveal) {
    hu_side_init_order_select(side, count, brought, reveal);
    if (side->brought == side->count) {
        /* the whole party fights: nothing to hide about picks */
        for (int i = 0; i < side->count; i++) {
            side->cards[i].flags |= HU_CARD_F_PICK_KNOWN | HU_CARD_F_PICKED;
        }
    }
}

void hu_side_set_mon(hu_side* side, int i, uint8_t species, uint8_t level, uint16_t hp, uint16_t max_hp,
                     const uint16_t moves[HU_CARD_MOVES], uint8_t base_speed, uint16_t known_speed) {
    hu_card* c = card_at(side, i);
    if (c == NULL) {
        return;
    }
    c->species = species;
    c->model_species = species;
    c->level = level;
    c->hp = hp;
    c->max_hp = max_hp;
    memcpy(c->moves, moves, sizeof(c->moves));
    c->speed = known_speed != 0 ? hu_speed_exact(known_speed) : hu_speed_initial(base_speed, level);
    c->transform_speed = c->speed;
    c->flags &= (uint8_t)~HU_CARD_F_TRANSFORMED;
    if (side->reveal == HU_REVEAL_ALL) {
        c->revealed = 0xF;
    }
    if (hp == 0 && max_hp != 0) {
        c->flags |= HU_CARD_F_FAINTED;
        c->faint_frames = 0xFFFF;
    }
}

static int count_known_picks(const hu_side* side) {
    int known_picked = 0;
    for (int i = 0; i < side->count; i++) {
        const hu_card* c = &side->cards[i];
        if ((c->flags & HU_CARD_F_PICK_KNOWN) && (c->flags & HU_CARD_F_PICKED)) {
            known_picked++;
        }
    }
    return known_picked;
}

static void mark_picked(hu_card* c) {
    c->flags |= HU_CARD_F_PICK_KNOWN | HU_CARD_F_PICKED;
    c->flags &= (uint8_t)~HU_CARD_F_PICK_INFERRED;
}

static void resolve_unknown(hu_side* side) {
    if (count_known_picks(side) < side->brought) {
        return;
    }
    for (int i = 0; i < side->count; i++) {
        hu_card* c = &side->cards[i];
        if (!(c->flags & HU_CARD_F_PICK_KNOWN)) {
            c->flags |= HU_CARD_F_PICK_KNOWN | HU_CARD_F_PICK_INFERRED;
            c->flags &= (uint8_t)~HU_CARD_F_PICKED;
        }
    }
}

void hu_side_set_picked(hu_side* side, uint8_t picked_mask) {
    for (int i = 0; i < side->count; i++) {
        hu_card* c = &side->cards[i];
        if ((picked_mask & (1u << i)) || (c->flags & HU_CARD_F_SENT_OUT)) {
            mark_picked(c);
        } else {
            c->flags |= HU_CARD_F_PICK_KNOWN;
            c->flags &= (uint8_t)~(HU_CARD_F_PICKED | HU_CARD_F_PICK_INFERRED);
        }
    }
}

void hu_side_pick(hu_side* side, int i) {
    hu_card* c = card_at(side, i);
    if (c != NULL && !(c->flags & HU_CARD_F_PICK_KNOWN) && count_known_picks(side) < side->brought) {
        mark_picked(c);
        resolve_unknown(side);
    }
}

void hu_side_unpick(hu_side* side, int i) {
    hu_card* c = card_at(side, i);
    if (c == NULL || !(c->flags & HU_CARD_F_PICKED) || (c->flags & HU_CARD_F_SENT_OUT)) {
        return;
    }
    c->flags &= (uint8_t)~(HU_CARD_F_PICK_KNOWN | HU_CARD_F_PICKED);
    for (int j = 0; j < side->count; j++) {
        if (side->cards[j].flags & HU_CARD_F_SENT_OUT) {
            return; /* keep what was already known when the battle began */
        }
    }
    for (int j = 0; j < side->count; j++) {
        if (side->cards[j].flags & HU_CARD_F_PICK_INFERRED) {
            side->cards[j].flags &= (uint8_t)~(HU_CARD_F_PICK_KNOWN | HU_CARD_F_PICK_INFERRED);
        }
    }
}

void hu_side_send_out(hu_side* side, int i) {
    hu_card* c = card_at(side, i);
    if (c == NULL) {
        return;
    }
    for (int j = 0; j < side->count; j++) {
        if (j != i && (side->cards[j].flags & HU_CARD_F_ACTIVE)) {
            hu_side_withdraw(side, j);
        }
    }
    mark_picked(c);
    c->flags |= HU_CARD_F_SENT_OUT | HU_CARD_F_ACTIVE;
    resolve_unknown(side);
}

void hu_side_move_used(hu_side* side, int i, int slot) {
    hu_card* c = card_at(side, i);
    if (c != NULL && slot >= 0 && slot < HU_CARD_MOVES) {
        c->revealed |= (uint8_t)(1u << slot);
    }
}

bool hu_side_reveal_move(hu_side* side, int i, uint16_t move) {
    hu_card* c = card_at(side, i);
    if (c == NULL || move == 0) {
        return false;
    }
    for (int s = 0; s < HU_CARD_MOVES; s++) {
        if (c->moves[s] == move) {
            c->revealed |= (uint8_t)(1u << s);
            return true;
        }
    }
    return false;
}

void hu_side_set_hp(hu_side* side, int i, uint16_t hp) {
    hu_card* c = card_at(side, i);
    if (c != NULL) {
        c->hp = hp;
    }
}

void hu_side_set_status(hu_side* side, int i, hu_status status) {
    hu_card* c = card_at(side, i);
    if (c != NULL) {
        c->status = (uint8_t)status;
    }
}

void hu_side_faint(hu_side* side, int i) {
    hu_card* c = card_at(side, i);
    if (c != NULL && !(c->flags & HU_CARD_F_FAINTED)) {
        c->flags |= HU_CARD_F_FAINTED;
        c->hp = 0;
        c->faint_frames = 0;
    }
}

void hu_side_transform(hu_side* side, int i, uint8_t species, hu_speed_range copied) {
    hu_card* c = card_at(side, i);
    if (c != NULL) {
        c->model_species = species;
        c->transform_speed = copied;
        c->flags |= HU_CARD_F_TRANSFORMED;
    }
}

void hu_side_set_substitute(hu_side* side, int i, bool on) {
    hu_card* c = card_at(side, i);
    if (c != NULL) {
        if (on) {
            c->flags |= HU_CARD_F_SUBSTITUTE;
        } else {
            c->flags &= (uint8_t)~HU_CARD_F_SUBSTITUTE;
        }
    }
}

void hu_side_withdraw(hu_side* side, int i) {
    hu_card* c = card_at(side, i);
    if (c != NULL) {
        c->flags &= (uint8_t)~(HU_CARD_F_ACTIVE | HU_CARD_F_SUBSTITUTE | HU_CARD_F_TRANSFORMED);
        c->model_species = c->species;
        c->transform_speed = c->speed;
    }
}

void hu_side_tick(hu_side* side) {
    for (int i = 0; i < side->count; i++) {
        hu_card* c = &side->cards[i];
        if ((c->flags & HU_CARD_F_FAINTED) && c->faint_frames < 0xFFFF) {
            c->faint_frames++;
        }
    }
}

hu_card_state hu_card_get_state(const hu_card* card) {
    if (card->flags & HU_CARD_F_FAINTED) {
        return HU_CARD_FAINTED;
    }
    if (!(card->flags & HU_CARD_F_PICK_KNOWN)) {
        return HU_CARD_UNKNOWN;
    }
    if (!(card->flags & HU_CARD_F_PICKED)) {
        return HU_CARD_NOT_PICKED;
    }
    if (card->status != HU_STATUS_NONE) {
        return HU_CARD_STATUSED;
    }
    return HU_CARD_PICKED;
}

bool hu_card_move_visible(const hu_card* card, int slot) {
    return slot >= 0 && slot < HU_CARD_MOVES && card->moves[slot] != 0 && (card->revealed & (1u << slot));
}

hu_speed_range hu_card_shown_speed(const hu_card* card) {
    return (card->flags & HU_CARD_F_TRANSFORMED) ? card->transform_speed : card->speed;
}

hu_rgb hu_status_color(hu_status status) {
    static const hu_rgb kColors[] = {
        { 255, 255, 255 }, /* none */
        { 150, 200, 255 }, /* sleep: light blue */
        { 160, 64, 200 },  /* poison: purple */
        { 90, 20, 140 },   /* toxic: deep purple */
        { 248, 216, 48 },  /* paralysis: yellow */
        { 64, 224, 208 },  /* freeze: aqua */
        { 224, 48, 32 },   /* burn: red */
    };
    if ((unsigned)status >= sizeof(kColors) / sizeof(kColors[0])) {
        status = HU_STATUS_NONE;
    }
    return kColors[status];
}

hu_model_look hu_card_model_look(const hu_card* card, uint16_t faint_length) {
    hu_model_look look;
    const hu_rgb black = { 0, 0, 0 }, grey = { 128, 128, 128 };
    look.anim = HU_ANIM_IDLE;
    look.anim_speed = 1.0f;
    look.tint = black;
    look.tint_amount = 0.0f;
    look.faint_done = false;
    switch (hu_card_get_state(card)) {
        case HU_CARD_PICKED:
            break;
        case HU_CARD_UNKNOWN:
            look.tint_amount = 0.25f; /* a slight black hue, still idling */
            break;
        case HU_CARD_NOT_PICKED:
            look.anim = HU_ANIM_HOLD;
            look.anim_speed = 0.0f;
            look.tint_amount = 0.6f; /* darker, frozen */
            break;
        case HU_CARD_STATUSED:
            look.tint = hu_status_color((hu_status)card->status);
            look.tint_amount = 0.45f;
            if (card->status == HU_STATUS_SLEEP) {
                look.anim = HU_ANIM_SLEEP;
            } else if (card->status == HU_STATUS_FREEZE) {
                look.anim = HU_ANIM_FROZEN;
            } else {
                look.anim_speed = 0.5f;
            }
            break;
        case HU_CARD_FAINTED:
            look.anim = HU_ANIM_FAINT;
            if (card->faint_frames >= faint_length) {
                look.faint_done = true;
                look.anim_speed = 0.0f;
                look.tint = grey;
                look.tint_amount = 0.7f;
            }
            break;
    }
    return look;
}

hu_hp_band hu_hp_band_of(uint16_t hp, uint16_t max_hp) {
    int px = hu_hp_fill(hp, max_hp, 48);
    if (px >= 27) return HU_HP_GREEN;
    if (px >= 10) return HU_HP_YELLOW;
    return HU_HP_RED;
}

int hu_hp_fill(uint16_t hp, uint16_t max_hp, int width) {
    long fill;
    if (max_hp == 0 || hp == 0 || width <= 0) {
        return 0;
    }
    if (hp >= max_hp) {
        return width;
    }
    fill = (long)hp * width / max_hp;
    return fill < 1 ? 1 : (int)fill;
}

hu_rgb hu_type_color(hu_type type) {
    static const hu_rgb kColors[HU_TYPE_COUNT] = {
        { 168, 168, 120 }, /* normal */
        { 192, 48, 40 },   /* fighting */
        { 168, 144, 240 }, /* flying */
        { 160, 64, 160 },  /* poison */
        { 224, 192, 104 }, /* ground */
        { 184, 160, 56 },  /* rock */
        { 168, 184, 32 },  /* bug */
        { 112, 88, 152 },  /* ghost */
        { 184, 184, 208 }, /* steel */
        { 240, 128, 48 },  /* fire */
        { 104, 144, 240 }, /* water */
        { 120, 200, 80 },  /* grass */
        { 248, 208, 48 },  /* electric */
        { 248, 88, 136 },  /* psychic */
        { 152, 216, 216 }, /* ice */
        { 112, 56, 248 },  /* dragon */
        { 112, 88, 72 },   /* dark */
        { 238, 153, 172 }, /* fairy */
        { 104, 160, 144 }, /* ??? */
    };
    if ((unsigned)type >= HU_TYPE_COUNT) {
        type = HU_TYPE_UNKNOWN;
    }
    return kColors[type];
}

hu_rgb hu_type_text_color(hu_type type) {
    hu_rgb c = hu_type_color(type);
    /* 40% toward white: the darker colours (ghost, dragon, dark, fighting) stay readable on a dark background */
    c.r = (uint8_t)(c.r + (255 - c.r) * 2 / 5);
    c.g = (uint8_t)(c.g + (255 - c.g) * 2 / 5);
    c.b = (uint8_t)(c.b + (255 - c.b) * 2 / 5);
    return c;
}
