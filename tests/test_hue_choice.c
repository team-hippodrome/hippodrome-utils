#include "hippodrome/hue_choice.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    /* the nickname formula: ((max - min) << 6) * sum / 255 + (min << 6) */
    CHECK_EQ(hu_hue_nickname_shift(0, 10, 40), 10 * 64);
    CHECK_EQ(hu_hue_nickname_shift(255, 10, 40), 40 * 64);
    CHECK_EQ(hu_hue_nickname_shift(128, 0, 60), (60 * 64 * 128) / 255);
    CHECK_EQ(hu_hue_nickname_shift(255, -30, 30), 30 * 64);
    CHECK_EQ(hu_hue_nickname_shift(0, -30, 30), -30 * 64);

    /* the picker's steps */
    CHECK_EQ(hu_hue_step(0, 1), HU_HUE_STEP);
    CHECK_EQ(hu_hue_step(0, -1), HU_HUE_FULL_TURN - HU_HUE_STEP);
    CHECK_EQ(hu_hue_step(HU_HUE_FULL_TURN - HU_HUE_STEP, 1), 0);
    CHECK_EQ(hu_hue_step(HU_HUE_STEP, -1), 0);
    CHECK_EQ(hu_hue_step(HU_HUE_STEP + 5, 1), 2 * HU_HUE_STEP);
    CHECK_EQ(hu_hue_step(HU_HUE_STEP + 5, -1), HU_HUE_STEP);
    CHECK_EQ(hu_hue_step(-1920, 1), HU_HUE_FULL_TURN - HU_HUE_STEP); /* -30 degrees = 330 */
    {
        int16_t v = 0;
        for (int i = 0; i < HU_HUE_STEPS; i++) v = hu_hue_step(v, 1);
        CHECK_EQ(v, 0); /* a full turn comes back to the original colours */
    }
    CHECK_EQ(hu_hue_degrees(-1920), 330);
    CHECK_EQ(hu_hue_degrees(64 * 90), 90);

    /* restricted to a species' range (Stadium Zero's Charizard: -20..15 degrees) */
    CHECK_EQ(hu_hue_signed(HU_HUE_FULL_TURN - 1920), -1920);
    CHECK_EQ(hu_hue_signed(180 * 64), 180 * 64);
    CHECK(hu_hue_in_range(0, -20, 15));
    CHECK(hu_hue_in_range(HU_HUE_FULL_TURN - 20 * 64, -20, 15)); /* 340 degrees = -20 */
    CHECK(!hu_hue_in_range(16 * 64, -20, 15));
    CHECK(hu_hue_in_range(200 * 64, -180, 180)); /* a full turn allows every hue */
    /* a range crossing the +-180 wrap point (no species has one; a modded table could): both parts count */
    CHECK(hu_hue_in_range(175 * 64, 170, -170));
    CHECK(hu_hue_in_range(-175 * 64, 170, -170));
    CHECK(!hu_hue_in_range(0, 170, -170));
    CHECK_EQ(hu_hue_clamp(10 * 64, -20, 15), 10 * 64);
    CHECK_EQ(hu_hue_clamp(60 * 64, -20, 15), 15 * 64);
    CHECK_EQ(hu_hue_clamp(HU_HUE_FULL_TURN - 90 * 64, -20, 15), -20 * 64); /* 270 = -90: nearer the bottom */
    CHECK_EQ(hu_hue_clamp(180 * 64, -20, 15), -20 * 64); /* 165 above the top, 160 below the bottom */
    CHECK_EQ(hu_hue_clamp(-175 * 64, -20, 15), -20 * 64);
    CHECK_EQ(hu_hue_step_in_range(0, 1, -20, 15), 15 * 64);
    CHECK_EQ(hu_hue_step_in_range(15 * 64, 1, -20, 15), 15 * 64); /* stays at the top */
    CHECK_EQ(hu_hue_step_in_range(0, -1, -20, 15), -15 * 64);
    CHECK_EQ(hu_hue_step_in_range(-15 * 64, -1, -20, 15), -20 * 64); /* the bottom end itself */
    CHECK_EQ(hu_hue_step_in_range(-20 * 64, -1, -20, 15), -20 * 64);
    CHECK_EQ(hu_hue_step_in_range(-20 * 64, 1, -20, 15), -15 * 64);
    CHECK_EQ(hu_hue_step_in_range(-1000, 1, -20, 15), -15 * 64); /* a nickname's -15.6 goes up to -15 */
    CHECK_EQ(hu_hue_step_in_range(120 * 64, -1, -20, 15), 0); /* outside: from the clamped 15 */
    CHECK_EQ(hu_hue_step_in_range(0, -1, -180, 180), HU_HUE_FULL_TURN - HU_HUE_STEP); /* full turn: as hu_hue_step */
    {
        /* Pikachu -15..25: 0, 15, 25, 25 going up */
        int16_t v = hu_hue_step_in_range(0, 1, -15, 25);
        CHECK_EQ(v, 15 * 64);
        v = hu_hue_step_in_range(v, 1, -15, 25);
        CHECK_EQ(v, 25 * 64);
        CHECK_EQ(hu_hue_step_in_range(v, -1, -15, 25), 15 * 64);
    }

    /* fingerprints */
    {
        const char a[] = "ピカチュウ", b[] = "ピカチュウ!";
        hu_hue_id ia = hu_hue_id_of(a, sizeof a), ib = hu_hue_id_of(b, sizeof b);
        CHECK(ia != 0 && ib != 0 && ia != ib);
        CHECK(hu_hue_id_of(a, sizeof a) == ia);
        CHECK(hu_hue_id_add(hu_hue_id_of(a, 3), a + 3, sizeof a - 3) == ia);
    }

    /* the store */
    {
        hu_hue_store* s = hu_hue_store_new();
        int16_t v = 123;
        uint8_t* bytes;
        size_t size;
        CHECK(!hu_hue_get(s, 42, &v));
        CHECK_EQ(v, 123);
        CHECK_EQ(hu_hue_resolve(s, 42, 777), 777);
        CHECK(!hu_hue_dirty(s));
        hu_hue_set(s, 42, 0); /* an explicit "original colours" is a choice too */
        CHECK(hu_hue_get(s, 42, &v));
        CHECK_EQ(v, 0);
        CHECK_EQ(hu_hue_resolve(s, 42, 777), 0);
        CHECK(hu_hue_dirty(s));
        hu_hue_set(s, 7, -960);
        CHECK(hu_hue_get(s, 7, &v));
        CHECK_EQ(v, HU_HUE_FULL_TURN - 960);
        for (int i = 0; i < 200; i++) hu_hue_set(s, 1000 + (uint64_t)i * 0x9E3779B97F4A7C15ull, (int16_t)(i * 10));
        CHECK_EQ(hu_hue_count(s), 202);
        hu_hue_set(s, 0, 5); /* 0 is no id */
        CHECK_EQ(hu_hue_count(s), 202);

        size = hu_hue_serialize(s, &bytes);
        CHECK(size > 0);
        {
            hu_hue_store* t = hu_hue_store_new();
            CHECK_EQ(hu_hue_deserialize(t, bytes, size), HU_HUE_OK);
            CHECK_EQ(hu_hue_count(t), 202);
            CHECK(!hu_hue_dirty(t));
            CHECK(hu_hue_get(t, 7, &v));
            CHECK_EQ(v, HU_HUE_FULL_TURN - 960);
            CHECK(hu_hue_get(t, 1000 + 150 * 0x9E3779B97F4A7C15ull, &v));
            CHECK_EQ(v, 1500);
            bytes[20] ^= 1; /* damaged: refused, the store comes back empty */
            CHECK_EQ(hu_hue_deserialize(t, bytes, size), HU_HUE_CORRUPT);
            CHECK_EQ(hu_hue_count(t), 0);
            CHECK_EQ(hu_hue_deserialize(t, (const uint8_t*)"HPTM", 4), HU_HUE_CORRUPT);
            hu_hue_store_free(t);
        }
        free(bytes);

        hu_hue_unset(s, 42);
        CHECK(!hu_hue_get(s, 42, NULL));
        CHECK_EQ(hu_hue_count(s), 201);

        /* files */
        {
            const char* path = "test_hue_choice.hue";
            hu_hue_store* t = hu_hue_store_new();
            remove(path);
            CHECK_EQ(hu_hue_load(t, path), HU_HUE_MISSING);
            CHECK_EQ(hu_hue_save(s, path), HU_HUE_OK);
            CHECK(!hu_hue_dirty(s));
            CHECK_EQ(hu_hue_load(t, path), HU_HUE_OK);
            CHECK_EQ(hu_hue_count(t), 201);
            CHECK(hu_hue_get(t, 7, &v));
            remove(path);
            hu_hue_store_free(t);
        }
        hu_hue_store_free(s);
    }
    TEST_DONE();
}
