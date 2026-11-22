#include "hippodrome/window_anim.h"

static uint8_t total(const hu_window_anim* w) {
    return (uint8_t)(w->expand_frames + w->fill_frames);
}

void hu_window_anim_init(hu_window_anim* w, uint8_t expand_frames, uint8_t fill_frames) {
    w->expand_frames = expand_frames;
    w->fill_frames = fill_frames;
    w->t = 0;
    w->open = false;
}

void hu_window_anim_set(hu_window_anim* w, bool open) {
    w->open = open;
}

void hu_window_anim_reset(hu_window_anim* w) {
    w->t = 0;
    w->open = false;
}

void hu_window_anim_tick(hu_window_anim* w) {
    if (w->open && w->t < total(w)) {
        w->t++;
    } else if (!w->open && w->t > 0) {
        w->t--;
    }
}

float hu_window_anim_size(const hu_window_anim* w) {
    if (w->expand_frames == 0) {
        return w->t > 0 || w->open ? 1.0f : 0.0f;
    }
    return w->t >= w->expand_frames ? 1.0f : (float)w->t / (float)w->expand_frames;
}

float hu_window_anim_fill(const hu_window_anim* w) {
    if (w->t <= w->expand_frames) {
        return (w->fill_frames == 0 && w->t == w->expand_frames && w->t > 0) ? 1.0f : 0.0f;
    }
    return (float)(w->t - w->expand_frames) / (float)w->fill_frames;
}

bool hu_window_anim_visible(const hu_window_anim* w) {
    return w->t > 0;
}

bool hu_window_anim_is_open(const hu_window_anim* w) {
    return w->t == total(w) && w->open;
}
