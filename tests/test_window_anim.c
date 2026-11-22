#include "hippodrome/window_anim.h"
#include "test.h"

int main(void) {
    hu_window_anim w;
    hu_window_anim_init(&w, 4, 2);
    CHECK(!hu_window_anim_visible(&w));
    hu_window_anim_set(&w, true);
    hu_window_anim_tick(&w);
    CHECK(hu_window_anim_visible(&w));
    CHECK(hu_window_anim_size(&w) > 0.24f && hu_window_anim_size(&w) < 0.26f);
    CHECK(hu_window_anim_fill(&w) == 0.0f);
    for (int i = 0; i < 3; i++) hu_window_anim_tick(&w);
    CHECK(hu_window_anim_size(&w) == 1.0f); /* expanded, not filled yet */
    CHECK(hu_window_anim_fill(&w) == 0.0f);
    CHECK(!hu_window_anim_is_open(&w));
    hu_window_anim_tick(&w);
    CHECK(hu_window_anim_fill(&w) == 0.5f);
    hu_window_anim_tick(&w);
    CHECK(hu_window_anim_is_open(&w));
    hu_window_anim_tick(&w); /* stays open */
    CHECK(hu_window_anim_is_open(&w));
    /* closing: the contents empty first, then the frame contracts */
    hu_window_anim_set(&w, false);
    hu_window_anim_tick(&w);
    CHECK(hu_window_anim_fill(&w) == 0.5f);
    CHECK(hu_window_anim_size(&w) == 1.0f);
    hu_window_anim_tick(&w);
    CHECK(hu_window_anim_fill(&w) == 0.0f);
    CHECK(hu_window_anim_size(&w) == 1.0f);
    hu_window_anim_tick(&w);
    CHECK(hu_window_anim_size(&w) < 1.0f);
    /* reopening mid-close goes on from there */
    hu_window_anim_set(&w, true);
    hu_window_anim_tick(&w);
    CHECK(hu_window_anim_size(&w) == 1.0f);
    for (int i = 0; i < 10; i++) hu_window_anim_tick(&w);
    CHECK(hu_window_anim_is_open(&w));
    hu_window_anim_reset(&w);
    CHECK(!hu_window_anim_visible(&w));
    TEST_DONE();
}
