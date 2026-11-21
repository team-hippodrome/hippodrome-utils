#include "hippodrome/summary_card.h"
#include "test.h"

int main(void) {
    hu_side s;
    const uint16_t moves[4] = { 85, 86, 98, 0 };

    /* A COM side: 6 shown, 3 brought, moves revealed as used. */
    hu_side_init(&s, 6, 3, HU_REVEAL_AS_USED);
    for (int i = 0; i < 6; i++) {
        hu_side_set_mon(&s, i, (uint8_t)(25 + i), 50, 100, 100, moves, 90, 0);
    }
    CHECK_EQ(hu_card_get_state(&s.cards[0]), HU_CARD_UNKNOWN);
    CHECK_EQ(s.cards[0].speed.min, 95);
    CHECK_EQ(s.cards[0].speed.max, 141);
    CHECK(!hu_card_move_visible(&s.cards[0], 0));

    hu_side_send_out(&s, 2);
    CHECK_EQ(hu_card_get_state(&s.cards[2]), HU_CARD_PICKED);
    CHECK(s.cards[2].flags & HU_CARD_F_ACTIVE);
    hu_side_move_used(&s, 2, 1);
    CHECK(hu_card_move_visible(&s.cards[2], 1));
    CHECK(!hu_card_move_visible(&s.cards[2], 0));
    CHECK(!hu_card_move_visible(&s.cards[2], 3)); /* empty slot never shows */
    CHECK(hu_side_reveal_move(&s, 2, 98));
    CHECK(hu_card_move_visible(&s.cards[2], 2));
    CHECK(!hu_side_reveal_move(&s, 2, 1)); /* not in the moveset */
    const uint16_t custom[4] = { 300, 0, 0, 0 }; /* a modded move id past 255 */
    hu_side_set_mon(&s, 1, 25, 50, 100, 100, custom, 90, 0);
    CHECK(hu_side_reveal_move(&s, 1, 300));
    CHECK(hu_card_move_visible(&s.cards[1], 0));

    hu_side_send_out(&s, 4);
    CHECK(!(s.cards[2].flags & HU_CARD_F_ACTIVE));
    CHECK_EQ(hu_card_get_state(&s.cards[0]), HU_CARD_UNKNOWN);
    hu_side_send_out(&s, 5);
    /* all three brought are known: the rest are Not Picked */
    CHECK_EQ(hu_card_get_state(&s.cards[0]), HU_CARD_NOT_PICKED);
    CHECK_EQ(hu_card_get_state(&s.cards[1]), HU_CARD_NOT_PICKED);
    CHECK_EQ(hu_card_get_state(&s.cards[3]), HU_CARD_NOT_PICKED);

    hu_side_set_status(&s, 5, HU_STATUS_PARALYSIS);
    CHECK_EQ(hu_card_get_state(&s.cards[5]), HU_CARD_STATUSED);
    hu_model_look look = hu_card_model_look(&s.cards[5], 30);
    CHECK_EQ(look.anim, HU_ANIM_IDLE);
    CHECK(look.anim_speed == 0.5f);
    CHECK_EQ(look.tint.r, 248);
    hu_side_set_status(&s, 5, HU_STATUS_SLEEP);
    CHECK_EQ(hu_card_model_look(&s.cards[5], 30).anim, HU_ANIM_SLEEP);
    hu_side_set_status(&s, 5, HU_STATUS_FREEZE);
    CHECK_EQ(hu_card_model_look(&s.cards[5], 30).anim, HU_ANIM_FROZEN);

    look = hu_card_model_look(&s.cards[0], 30);
    CHECK_EQ(look.anim, HU_ANIM_HOLD);
    CHECK(look.tint_amount > 0.5f);
    hu_side tmp;
    hu_side_init(&tmp, 6, 3, HU_REVEAL_AS_USED);
    hu_side_set_mon(&tmp, 0, 25, 50, 100, 100, moves, 90, 0);
    look = hu_card_model_look(&tmp.cards[0], 30);
    CHECK_EQ(look.anim, HU_ANIM_IDLE);
    CHECK(look.tint_amount > 0.0f && look.tint_amount < 0.5f);

    /* Fainting: animation for 30 frames, then held grey; status no longer matters. */
    hu_side_faint(&s, 5);
    CHECK_EQ(hu_card_get_state(&s.cards[5]), HU_CARD_FAINTED);
    CHECK_EQ(s.cards[5].hp, 0);
    look = hu_card_model_look(&s.cards[5], 30);
    CHECK_EQ(look.anim, HU_ANIM_FAINT);
    CHECK(!look.faint_done);
    for (int f = 0; f < 30; f++) hu_side_tick(&s);
    look = hu_card_model_look(&s.cards[5], 30);
    CHECK(look.faint_done);
    CHECK(look.anim_speed == 0.0f);

    /* Transform: model and speed follow the target until withdrawn; substitute changes nothing on the model. */
    hu_speed_range copied = { 120, 120 };
    hu_side_transform(&s, 4, 150, copied);
    CHECK_EQ(s.cards[4].model_species, 150);
    CHECK_EQ(hu_card_shown_speed(&s.cards[4]).min, 120);
    CHECK_EQ(s.cards[4].speed.min, 95); /* the saved estimate is untouched */
    hu_side_set_substitute(&s, 4, true);
    CHECK_EQ(s.cards[4].model_species, 150);
    hu_side_withdraw(&s, 4);
    CHECK_EQ(s.cards[4].model_species, 29);
    CHECK(!(s.cards[4].flags & HU_CARD_F_SUBSTITUTE));
    CHECK_EQ(hu_card_shown_speed(&s.cards[4]).min, 95);

    /* The viewer's own side: everything shown, exact speed, picks known. */
    hu_side me;
    hu_side_init(&me, 6, 3, HU_REVEAL_ALL);
    hu_side_set_mon(&me, 0, 25, 50, 100, 100, moves, 90, 120);
    CHECK(hu_card_move_visible(&me.cards[0], 0));
    CHECK(hu_speed_is_exact(me.cards[0].speed));
    hu_side_set_picked(&me, 0x5);
    CHECK_EQ(hu_card_get_state(&me.cards[0]), HU_CARD_PICKED);
    CHECK_EQ(hu_card_get_state(&me.cards[1]), HU_CARD_NOT_PICKED);

    /* Whole party fights: no unknowns. */
    hu_side all;
    hu_side_init(&all, 3, 3, HU_REVEAL_AS_USED);
    hu_side_set_mon(&all, 0, 25, 50, 100, 100, moves, 90, 0);
    CHECK_EQ(hu_card_get_state(&all.cards[0]), HU_CARD_PICKED);

    /* HP bar: GB colour bands. */
    CHECK_EQ(hu_hp_band_of(100, 100), HU_HP_GREEN);
    CHECK_EQ(hu_hp_band_of(57, 100), HU_HP_GREEN);  /* 27/48 */
    CHECK_EQ(hu_hp_band_of(55, 100), HU_HP_YELLOW); /* 26/48 */
    CHECK_EQ(hu_hp_band_of(21, 100), HU_HP_YELLOW); /* 10/48 */
    CHECK_EQ(hu_hp_band_of(20, 100), HU_HP_RED);
    CHECK_EQ(hu_hp_fill(1, 999, 48), 1);
    CHECK_EQ(hu_hp_fill(0, 999, 48), 0);
    CHECK_EQ(hu_hp_fill(50, 100, 48), 24);

    /* Type colours: unknown ids fall back to ???, text colours are lighter. */
    CHECK_EQ(hu_type_color(HU_TYPE_FIRE).r, 240);
    CHECK_EQ(hu_type_color((hu_type)200).g, hu_type_color(HU_TYPE_UNKNOWN).g);
    CHECK(hu_type_text_color(HU_TYPE_DRAGON).r > hu_type_color(HU_TYPE_DRAGON).r);
    TEST_DONE();
}
