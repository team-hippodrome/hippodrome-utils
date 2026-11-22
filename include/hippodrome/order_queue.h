/* Order queueing (docs/specs/ORDER_QUEUEING.md): while the turn's moves play out, the player presses Z to open the
 * command menu and picks their next order (a move, or a Pokémon to switch to). When the next command selection
 * starts, the queued order is entered for them on the COM's timing, unless they cancel it.
 *
 * The spec's display state machine:
 *
 *   Hidden --Z--> Showing --A--> Move_Select   --C--> Move_Queued
 *                 Showing --B--> Switch_Select --C--> Switch_Queued
 *   Move_Select / Switch_Select --L--> Showing;  Move_Queued --L--> Move_Select;  Switch_Queued --L--> Switch_Select
 *   any state --Z--> Hidden (and the queued order, if any, is dropped)
 *
 * "C" is the game's own pick button for a move or a party member (Stadium Zero picks moves with the C buttons and a
 * party member with the C buttons, A or B when six were brought): the port maps the button to a slot and calls
 * hu_oq_pick().
 *
 * A queued order is cancelled:
 *  - any time the player presses Z, including during the command selection it is entered in;
 *  - if the menu is in any state other than Move_Queued / Switch_Queued when the moves finish playing out;
 *  - if the order becomes illegal (the Pokémon fainted, is frozen or locked into a move, the move ran out of PP, the
 *    party member fainted, ...): the port checks that with its game's rules and calls hu_oq_cancel().
 *
 * Entering the order follows the COM's timing (HU_OQ_OPEN_FRAME, HU_OQ_PICK_FRAME): the menu opens on the open frame
 * and the order is picked on the pick frame, so the player has time to take back a last-second cancel. */
#ifndef HIPPODROME_ORDER_QUEUE_H
#define HIPPODROME_ORDER_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hu_oq_state {
    HU_OQ_HIDDEN = 0,
    HU_OQ_SHOWING,       /* the command menu: A fight, B Pokémon */
    HU_OQ_MOVE_SELECT,   /* picking a move */
    HU_OQ_SWITCH_SELECT, /* picking a party member */
    HU_OQ_MOVE_QUEUED,
    HU_OQ_SWITCH_QUEUED,
} hu_oq_state;

/* The frames after the command selection starts at which a queued order is entered: the Stadium games' COM opens its
 * menu at frame 30 of the command phase and picks at frame 90 (Stadium Zero: BattleScene_ApplyCpuChoice). */
#define HU_OQ_OPEN_FRAME 30
#define HU_OQ_PICK_FRAME 90

typedef enum hu_oq_action {
    HU_OQ_NONE = 0,
    HU_OQ_OPEN_FIGHT,   /* enter the fight menu now (the game's A on its command menu) */
    HU_OQ_OPEN_SWITCH,  /* enter the Pokémon menu now (the game's B) */
    HU_OQ_PICK,         /* pick hu_order_queue.slot now; the order is used up */
} hu_oq_action;

typedef struct hu_order_queue {
    uint8_t state;      /* hu_oq_state */
    int8_t slot;        /* Move_Queued: the move slot; Switch_Queued: the party slot; -1 otherwise */
    bool entering;      /* the command selection started and the order is being entered */
    uint16_t frame;     /* frames since the command selection started (while entering) */
    uint16_t open_frame, pick_frame;
} hu_order_queue;

/* Hidden, nothing queued, COM timing HU_OQ_OPEN_FRAME / HU_OQ_PICK_FRAME. */
void hu_oq_init(hu_order_queue* q);
/* Back to Hidden with nothing queued (a battle starts or ends). */
void hu_oq_reset(hu_order_queue* q);

/* Buttons while the moves play out (not during the command selection, where the game's own menu reads them; there
 * only Z counts, see hu_oq_z). Each returns true when the menu changed (the port may play its menu sound). */
bool hu_oq_z(hu_order_queue* q); /* toggles Hidden <-> Showing; from anywhere else: Hidden, the order dropped */
bool hu_oq_a(hu_order_queue* q); /* Showing -> Move_Select */
bool hu_oq_b(hu_order_queue* q); /* Showing -> Switch_Select */
bool hu_oq_l(hu_order_queue* q); /* one step back (see the state machine) */
/* The game's pick button for `slot` (Move_Select: a move slot; Switch_Select: a party slot). The port passes only
 * slots the player may pick now (a move with PP, a healthy party member that is not out already). */
bool hu_oq_pick(hu_order_queue* q, int slot);

/* The moves finished playing out and the command selection starts: an order that is not queued is dropped (the menu
 * hides); a queued one starts being entered. */
void hu_oq_resolution_end(hu_order_queue* q);
/* One frame of the command selection: what the port should do to its game's menu now. HU_OQ_PICK leaves the queue
 * Hidden. Returns HU_OQ_NONE when nothing is being entered. */
hu_oq_action hu_oq_command_frame(hu_order_queue* q);
/* The order became illegal, or the game's command selection could not take it: dropped, the menu hides. */
void hu_oq_cancel(hu_order_queue* q);

bool hu_oq_queued(const hu_order_queue* q);  /* Move_Queued or Switch_Queued */
bool hu_oq_visible(const hu_order_queue* q); /* the menu is up (any state but Hidden) and nothing is being entered */

#ifdef __cplusplus
}
#endif
#endif
