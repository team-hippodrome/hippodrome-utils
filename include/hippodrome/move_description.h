/* Mid-battle move description (docs/specs/MIDBATTLE_MOVE_DESCRIPTION.md): in the battle's move select menu, holding a
 * D-pad direction shows the description of the move on the C button of the same direction, as Stadium 2 does.
 *
 * This keeps which move's description is up and its window's animation (window_anim.h); the port says which
 * directions are held each frame and draws the window with its own game's move data and font. */
#ifndef HIPPODROME_MOVE_DESCRIPTION_H
#define HIPPODROME_MOVE_DESCRIPTION_H

#include <stdbool.h>
#include <stdint.h>

#include "hippodrome/window_anim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The D-pad directions, as bits. Each game maps a direction to the move slot of the C button of that direction
 * (Stadium Zero: C-Up 0, C-Right 1, C-Down 2, C-Left 3). */
enum hu_dpad {
    HU_DPAD_UP = 1 << 0,
    HU_DPAD_RIGHT = 1 << 1,
    HU_DPAD_DOWN = 1 << 2,
    HU_DPAD_LEFT = 1 << 3,
};

typedef struct hu_move_desc {
    int8_t slot;         /* the move slot whose description is up (or closing), -1 none */
    uint8_t held;        /* the directions held last frame (enum hu_dpad) */
    hu_window_anim anim; /* the window */
} hu_move_desc;

/* `expand_frames` / `fill_frames`: the window's opening (hu_window_anim_init). */
void hu_move_desc_init(hu_move_desc* d, uint8_t expand_frames, uint8_t fill_frames);
/* One frame of the move menu. `held`: the D-pad directions held now (enum hu_dpad); `slot_of[i]`: the move slot of
 * direction bit i (up, right, down, left); `usable`: bit per move slot that has a move to describe. The direction
 * pressed last wins while several are held; letting go of all of them closes the window. Returns true when the
 * shown move changed this frame (the port may play its cursor sound). */
bool hu_move_desc_update(hu_move_desc* d, uint8_t held, const int8_t slot_of[4], uint8_t usable);
/* The menu is not up (a move was picked, L went back, the battle moved on): the window closes. Call it every frame
 * the menu is not up, so the closing animation runs. */
void hu_move_desc_close(hu_move_desc* d);
/* Gone at once, no animation. */
void hu_move_desc_reset(hu_move_desc* d);
/* The slot to draw, or -1 when there is nothing to draw. */
int hu_move_desc_shown(const hu_move_desc* d);

#ifdef __cplusplus
}
#endif
#endif
