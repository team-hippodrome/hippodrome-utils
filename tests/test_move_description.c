#include "hippodrome/move_description.h"
#include "test.h"

/* Stadium Zero: up = C-Up = slot 0, right 1, down 2, left 3 */
static const int8_t kSlots[4] = { 0, 1, 2, 3 };

int main(void) {
    hu_move_desc d;
    hu_move_desc_init(&d, 3, 2);
    CHECK_EQ(hu_move_desc_shown(&d), -1);
    CHECK(!hu_move_desc_update(&d, 0, kSlots, 0x0F));
    CHECK_EQ(hu_move_desc_shown(&d), -1);

    /* holding right shows slot 1 */
    CHECK(hu_move_desc_update(&d, HU_DPAD_RIGHT, kSlots, 0x0F));
    CHECK_EQ(hu_move_desc_shown(&d), 1);
    CHECK(!hu_move_desc_update(&d, HU_DPAD_RIGHT, kSlots, 0x0F));
    CHECK_EQ(hu_move_desc_shown(&d), 1);
    /* pressing down while right is still held: the last pressed wins */
    CHECK(hu_move_desc_update(&d, HU_DPAD_RIGHT | HU_DPAD_DOWN, kSlots, 0x0F));
    CHECK_EQ(hu_move_desc_shown(&d), 2);
    CHECK(!hu_move_desc_update(&d, HU_DPAD_RIGHT | HU_DPAD_DOWN, kSlots, 0x0F));
    CHECK_EQ(hu_move_desc_shown(&d), 2);
    /* letting go of down: back to right */
    CHECK(hu_move_desc_update(&d, HU_DPAD_RIGHT, kSlots, 0x0F));
    CHECK_EQ(hu_move_desc_shown(&d), 1);
    /* letting go: the window closes over a few frames, still showing slot 1 */
    for (int i = 0; i < 3; i++) hu_move_desc_update(&d, HU_DPAD_RIGHT, kSlots, 0x0F);
    CHECK(hu_window_anim_is_open(&d.anim));
    hu_move_desc_update(&d, 0, kSlots, 0x0F);
    CHECK_EQ(hu_move_desc_shown(&d), 1);
    for (int i = 0; i < 10; i++) hu_move_desc_update(&d, 0, kSlots, 0x0F);
    CHECK_EQ(hu_move_desc_shown(&d), -1);

    /* a direction with no move (a Pokémon that knows 2 moves) shows nothing */
    CHECK(!hu_move_desc_update(&d, HU_DPAD_LEFT, kSlots, 0x03));
    CHECK_EQ(hu_move_desc_shown(&d), -1);
    CHECK(hu_move_desc_update(&d, HU_DPAD_LEFT | HU_DPAD_UP, kSlots, 0x03));
    CHECK_EQ(hu_move_desc_shown(&d), 0);

    /* the menu goes away */
    for (int i = 0; i < 10; i++) hu_move_desc_close(&d);
    CHECK_EQ(hu_move_desc_shown(&d), -1);
    hu_move_desc_update(&d, HU_DPAD_UP, kSlots, 0x0F);
    hu_move_desc_reset(&d);
    CHECK_EQ(hu_move_desc_shown(&d), -1);
    TEST_DONE();
}
