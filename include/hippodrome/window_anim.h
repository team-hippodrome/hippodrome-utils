/* How an enhancement's window opens and closes (AGENTS.md, design principle 2): it expands from its middle, then its
 * contents fill in; closing empties it first, then it contracts. This is the timing only; ports draw the window.
 *
 * Used by the mid-battle move description (MIDBATTLE_MOVE_DESCRIPTION) and the order queue menu (ORDER_QUEUEING). */
#ifndef HIPPODROME_WINDOW_ANIM_H
#define HIPPODROME_WINDOW_ANIM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hu_window_anim {
    uint8_t expand_frames; /* frames the frame takes to grow to full size */
    uint8_t fill_frames;   /* then the frames its contents take to fade in */
    uint8_t t;             /* 0 = closed .. expand_frames + fill_frames = fully open */
    bool open;             /* where it is heading */
} hu_window_anim;

void hu_window_anim_init(hu_window_anim* w, uint8_t expand_frames, uint8_t fill_frames);
/* Open (true) or close (false) from wherever it is now; a window that is closing reopens from that point. */
void hu_window_anim_set(hu_window_anim* w, bool open);
/* Snaps it shut with no animation (e.g. the battle ended under it). */
void hu_window_anim_reset(hu_window_anim* w);
/* One frame. */
void hu_window_anim_tick(hu_window_anim* w);

/* How big the frame is, 0..1 (scale it about its middle). */
float hu_window_anim_size(const hu_window_anim* w);
/* How far in its contents are, 0..1 (their opacity). */
float hu_window_anim_fill(const hu_window_anim* w);
/* Anything to draw (open, opening or closing). */
bool hu_window_anim_visible(const hu_window_anim* w);
/* Fully open. */
bool hu_window_anim_is_open(const hu_window_anim* w);

#ifdef __cplusplus
}
#endif
#endif
