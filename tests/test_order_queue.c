#include "hippodrome/order_queue.h"
#include "test.h"

enum { OPEN_FRAME = 2, PICK_FRAME = 10 };

static int run_command_phase(hu_order_queue* q, hu_oq_action* opened, int* pick_frame) {
    *opened = HU_OQ_NONE;
    *pick_frame = -1;
    for (int f = 1; f <= 200; f++) {
        hu_oq_action a = hu_oq_command_frame(q);
        if (a == HU_OQ_OPEN_FIGHT || a == HU_OQ_OPEN_SWITCH) {
            *opened = a;
            CHECK_EQ(f, OPEN_FRAME);
        } else if (a == HU_OQ_PICK) {
            *pick_frame = f;
            return q->slot;
        }
    }
    return -1;
}

int main(void) {
    hu_order_queue q;
    hu_oq_action opened;
    int pick_frame;
    hu_oq_init(&q, OPEN_FRAME, PICK_FRAME);
    CHECK(!hu_oq_visible(&q));

    /* the diagram's edges */
    CHECK(!hu_oq_a(&q)); /* Hidden ignores A, B, L, picks */
    CHECK(!hu_oq_l(&q));
    CHECK(!hu_oq_pick(&q, 0));
    CHECK(hu_oq_z(&q));
    CHECK_EQ(q.state, HU_OQ_SHOWING);
    CHECK(hu_oq_visible(&q));
    CHECK(!hu_oq_l(&q)); /* no L edge out of Showing */
    CHECK(!hu_oq_pick(&q, 1));
    CHECK(hu_oq_z(&q));
    CHECK_EQ(q.state, HU_OQ_HIDDEN);
    hu_oq_z(&q);
    CHECK(hu_oq_a(&q));
    CHECK_EQ(q.state, HU_OQ_MOVE_SELECT);
    CHECK(!hu_oq_b(&q));
    CHECK(hu_oq_l(&q));
    CHECK_EQ(q.state, HU_OQ_SHOWING);
    CHECK(hu_oq_b(&q));
    CHECK_EQ(q.state, HU_OQ_SWITCH_SELECT);
    CHECK(hu_oq_pick(&q, 4));
    CHECK_EQ(q.state, HU_OQ_SWITCH_QUEUED);
    CHECK_EQ(q.slot, 4);
    CHECK(hu_oq_queued(&q));
    CHECK(hu_oq_l(&q));
    CHECK_EQ(q.state, HU_OQ_SWITCH_SELECT);
    CHECK_EQ(q.slot, -1);
    CHECK(hu_oq_l(&q));
    CHECK(hu_oq_a(&q));
    CHECK(hu_oq_pick(&q, 2));
    CHECK_EQ(q.state, HU_OQ_MOVE_QUEUED);
    CHECK(hu_oq_l(&q));
    CHECK_EQ(q.state, HU_OQ_MOVE_SELECT);
    CHECK(hu_oq_pick(&q, 3));
    CHECK(hu_oq_z(&q)); /* Z from Move_Queued: Hidden, dropped */
    CHECK_EQ(q.state, HU_OQ_HIDDEN);
    CHECK(!hu_oq_queued(&q));

    /* a queued move is entered on the frames supplied by the port */
    hu_oq_z(&q);
    hu_oq_a(&q);
    hu_oq_pick(&q, 1);
    hu_oq_resolution_end(&q);
    CHECK(q.entering);
    CHECK(!hu_oq_visible(&q));
    CHECK(!hu_oq_a(&q)); /* the game's menu has the buttons now */
    CHECK_EQ(run_command_phase(&q, &opened, &pick_frame), 1);
    CHECK_EQ(opened, HU_OQ_OPEN_FIGHT);
    CHECK_EQ(pick_frame, PICK_FRAME);
    CHECK_EQ(q.state, HU_OQ_HIDDEN);
    CHECK(!q.entering);
    CHECK_EQ(hu_oq_command_frame(&q), HU_OQ_NONE);

    /* a queued switch opens the Pokémon menu */
    hu_oq_z(&q);
    hu_oq_b(&q);
    hu_oq_pick(&q, 5);
    hu_oq_resolution_end(&q);
    CHECK_EQ(run_command_phase(&q, &opened, &pick_frame), 5);
    CHECK_EQ(opened, HU_OQ_OPEN_SWITCH);
    CHECK_EQ(pick_frame, PICK_FRAME);

    /* not queued when the moves finish: dropped */
    hu_oq_z(&q);
    hu_oq_a(&q);
    hu_oq_resolution_end(&q);
    CHECK_EQ(q.state, HU_OQ_HIDDEN);
    CHECK(!q.entering);
    CHECK_EQ(hu_oq_command_frame(&q), HU_OQ_NONE);

    /* Z during the command selection takes it back, even after the menu opened */
    hu_oq_z(&q);
    hu_oq_a(&q);
    hu_oq_pick(&q, 0);
    hu_oq_resolution_end(&q);
    for (int f = 0; f < OPEN_FRAME; f++) hu_oq_command_frame(&q);
    CHECK(hu_oq_z(&q));
    CHECK(!q.entering);
    CHECK_EQ(q.state, HU_OQ_HIDDEN);
    for (int f = 0; f < 100; f++) CHECK_EQ(hu_oq_command_frame(&q), HU_OQ_NONE);

    /* illegal: cancelled */
    hu_oq_z(&q);
    hu_oq_b(&q);
    hu_oq_pick(&q, 3);
    hu_oq_cancel(&q);
    CHECK(!hu_oq_queued(&q));
    CHECK(!hu_oq_visible(&q));

    /* Another port can use different frames; resetting a battle keeps its timing. */
    hu_oq_init(&q, 1, 4);
    hu_oq_reset(&q);
    hu_oq_z(&q);
    hu_oq_a(&q);
    hu_oq_pick(&q, 2);
    hu_oq_resolution_end(&q);
    CHECK_EQ(hu_oq_command_frame(&q), HU_OQ_OPEN_FIGHT);
    CHECK_EQ(hu_oq_command_frame(&q), HU_OQ_NONE);
    CHECK_EQ(hu_oq_command_frame(&q), HU_OQ_NONE);
    CHECK_EQ(hu_oq_command_frame(&q), HU_OQ_PICK);
    CHECK_EQ(q.slot, 2);
    TEST_DONE();
}
