#include "hippodrome/move_description.h"

void hu_move_desc_init(hu_move_desc* d, uint8_t expand_frames, uint8_t fill_frames) {
    d->slot = -1;
    d->held = 0;
    hu_window_anim_init(&d->anim, expand_frames, fill_frames);
}

bool hu_move_desc_update(hu_move_desc* d, uint8_t held, const int8_t slot_of[4], uint8_t usable) {
    uint8_t pressed = (uint8_t)(held & ~d->held);
    int8_t want = -1;
    bool changed = false;
    held &= 0x0F;
    /* a direction pressed this frame wins; otherwise the shown one stays while its direction is held */
    for (int i = 0; i < 4; i++) {
        if ((pressed & (1 << i)) && slot_of[i] >= 0 && (usable & (1 << slot_of[i]))) {
            want = slot_of[i];
            break;
        }
    }
    if (want < 0 && d->slot >= 0 && d->anim.open) {
        for (int i = 0; i < 4; i++) {
            if ((held & (1 << i)) && slot_of[i] == d->slot) {
                want = d->slot;
                break;
            }
        }
    }
    if (want < 0) {
        /* the shown direction was let go while another is still held: show that one */
        for (int i = 0; i < 4; i++) {
            if ((held & (1 << i)) && slot_of[i] >= 0 && (usable & (1 << slot_of[i]))) {
                want = slot_of[i];
                break;
            }
        }
    }
    d->held = held;
    if (want >= 0) {
        changed = want != d->slot || !d->anim.open;
        d->slot = want;
        hu_window_anim_set(&d->anim, true);
    } else {
        hu_window_anim_set(&d->anim, false);
    }
    hu_window_anim_tick(&d->anim);
    if (!hu_window_anim_visible(&d->anim)) {
        d->slot = -1;
    }
    return changed;
}

void hu_move_desc_close(hu_move_desc* d) {
    d->held = 0;
    hu_window_anim_set(&d->anim, false);
    hu_window_anim_tick(&d->anim);
    if (!hu_window_anim_visible(&d->anim)) {
        d->slot = -1;
    }
}

void hu_move_desc_reset(hu_move_desc* d) {
    d->slot = -1;
    d->held = 0;
    hu_window_anim_reset(&d->anim);
}

int hu_move_desc_shown(const hu_move_desc* d) {
    return hu_window_anim_visible(&d->anim) ? d->slot : -1;
}
