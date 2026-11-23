#include "hippodrome/trainer_memory.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t hu_crc32(const uint8_t* p, size_t n); /* src/file_io.h */

int main(void) {
    hu_tm* tm = hu_tm_new();
    hu_tm_key k = { HU_GAME_STADIUM_ZERO, 2, 0, 0, 3 };
    hu_tm_key other = { HU_GAME_STADIUM_ZERO, 2, 1, 0, 3 };
    hu_tm_team_entry team[3] = {
        { 25, 30, { 85, 86, 98, 0 } }, /* Pikachu */
        { 95, 28, { 88, 157, 0, 0 } }, /* Onix */
        { 0, 0, { 0 } },               /* empty: skipped */
    };

    CHECK(hu_tm_find(tm, k) == NULL);
    CHECK(!hu_tm_dirty(tm));
    hu_tm_record_team(tm, k, team, 3);
    CHECK(hu_tm_dirty(tm));
    const hu_tm_trainer* t = hu_tm_find(tm, k);
    CHECK(t != NULL);
    CHECK(hu_tm_find(tm, other) == NULL);
    CHECK_EQ(t->team_count, 2);
    CHECK(t->flags & HU_TM_REACHED);
    CHECK(t->flags & HU_TM_TEAM_KNOWN);

    hu_tm_record_move(tm, k, 25, 85);
    hu_tm_record_move(tm, k, 25, 98);
    hu_tm_record_move(tm, k, 25, 0); /* ignored */
    const hu_tm_mon* pika = hu_tm_find_mon(hu_tm_find(tm, k), 25);
    CHECK(pika != NULL);
    CHECK(hu_tm_move_seen(pika, 85));
    CHECK(hu_tm_move_seen(pika, 98));
    CHECK(!hu_tm_move_seen(pika, 86));
    CHECK(!hu_tm_move_seen(pika, 0));

    hu_speed_range a = { 60, 80 }, b = { 70, 90 };
    hu_tm_record_speed(tm, k, 25, a);
    hu_tm_record_speed(tm, k, 25, b);
    pika = hu_tm_find_mon(hu_tm_find(tm, k), 25);
    CHECK(pika->flags & HU_TM_MON_HAS_SPEED);
    CHECK_EQ(pika->speed.min, 70);
    CHECK_EQ(pika->speed.max, 80);

    /* A fresh estimate is under the base rules' stat exp. Under Stat Exp difficulty, the stored one is adjusted. */
    CHECK_EQ(pika->se_min, 0);
    CHECK_EQ(pika->se_max, 65535);
    {
        hu_speed_range got;
        CHECK(hu_tm_mon_speed(pika, 90, 0, 65535, &got));
        CHECK_EQ(got.min, 70);
        CHECK_EQ(got.max, 80);
        CHECK(hu_tm_mon_speed(pika, 90, 65535, 65535, &got));
        CHECK(got.min >= hu_gb_stat(90, 0, 65535, 30));
        CHECK(!hu_tm_mon_speed(hu_tm_find_mon(hu_tm_find(tm, k), 95), 35, 0, 65535, &got));
        /* recording under another setting moves the stored range to it first, then keeps the new setting */
        hu_speed_range hard = hu_speed_initial_se(90, 30, 36863, 65535);
        hu_tm_record_speed_se(tm, k, 25, 90, hard, 36863, 65535);
        pika = hu_tm_find_mon(hu_tm_find(tm, k), 25);
        CHECK_EQ(pika->se_min, 36863);
        CHECK_EQ(pika->se_max, 65535);
        hu_speed_range was = hu_speed_rescale(b, 90, 30, 0, 65535, 36863, 65535);
        hu_speed_range was2 = hu_speed_rescale(a, 90, 30, 0, 65535, 36863, 65535);
        CHECK(pika->speed.min >= was.min && pika->speed.min >= was2.min);
        CHECK(pika->speed.max <= was.max && pika->speed.max <= was2.max);
        hu_tm_record_speed(tm, k, 25, a); /* (back to the base rules for the checks below) */
        pika = hu_tm_find_mon(hu_tm_find(tm, k), 25);
        CHECK_EQ(pika->se_min, 0);
    }

    /* Seeing the team again keeps what was learned; a level change drops the speed estimate. */
    hu_tm_record_team(tm, k, team, 2);
    pika = hu_tm_find_mon(hu_tm_find(tm, k), 25);
    CHECK(hu_tm_move_seen(pika, 85));
    CHECK(pika->flags & HU_TM_MON_HAS_SPEED);
    team[0].level = 31;
    hu_tm_record_team(tm, k, team, 2);
    pika = hu_tm_find_mon(hu_tm_find(tm, k), 25);
    CHECK(!(pika->flags & HU_TM_MON_HAS_SPEED));
    CHECK(hu_tm_move_seen(pika, 85));

    /* A move that left the moveset stops counting as seen; a 16-bit (modded) id works. */
    team[0].moves[0] = 400;
    hu_tm_record_team(tm, k, team, 2);
    pika = hu_tm_find_mon(hu_tm_find(tm, k), 25);
    CHECK(!hu_tm_move_seen(pika, 85));
    CHECK(hu_tm_move_seen(pika, 98));
    hu_tm_record_move(tm, k, 25, 400);
    CHECK(hu_tm_move_seen(hu_tm_find_mon(hu_tm_find(tm, k), 25), 400));
    /* A move outside the known moveset takes the free 4th slot; once all 4 are known, others are dropped. */
    hu_tm_record_move(tm, k, 25, 99);
    CHECK_EQ(hu_tm_find_mon(hu_tm_find(tm, k), 25)->moves[3], 99);
    hu_tm_record_move(tm, k, 25, 1);
    CHECK(!hu_tm_move_seen(hu_tm_find_mon(hu_tm_find(tm, k), 25), 1));

    /* A move from a trainer we never reached creates the record (sent out first). */
    hu_tm_record_sent_out(tm, other, 130, 30);
    hu_tm_record_move(tm, other, 130, 56);
    const hu_tm_trainer* o = hu_tm_find(tm, other);
    CHECK(o != NULL && !(o->flags & HU_TM_REACHED));
    CHECK(hu_tm_move_seen(hu_tm_find_mon(o, 130), 56));
    CHECK(hu_tm_find_mon(o, 130)->flags & HU_TM_MON_SENT_OUT);
    CHECK_EQ(hu_tm_count(tm), 2);

    hu_tm_record_name(tm, k, "\xA4\xE0\xA4\xB7 BUG CATCHER");
    CHECK(strcmp((const char*)hu_tm_find(tm, k)->name, "\xA4\xE0\xA4\xB7 BUG CATCHER") == 0);

    /* Round trip through a file. */
    const char* path = "test_trainer_memory.bin";
    CHECK_EQ(hu_tm_save(tm, path), HU_TM_OK);
    CHECK(!hu_tm_dirty(tm));
    hu_tm* tm2 = hu_tm_new();
    CHECK_EQ(hu_tm_load(tm2, path), HU_TM_OK);
    CHECK_EQ(hu_tm_count(tm2), 2);
    const hu_tm_trainer* t2 = hu_tm_find(tm2, k);
    CHECK(t2 != NULL && t2->team_count == 2);
    CHECK(hu_tm_move_seen(hu_tm_find_mon(t2, 25), 98));
    CHECK(hu_tm_move_seen(hu_tm_find_mon(t2, 25), 400));
    CHECK_EQ(hu_tm_find_mon(t2, 25)->level, 31);
    CHECK_EQ(hu_tm_find_mon(t2, 95)->moves[1], 157);
    CHECK(strcmp((const char*)t2->name, "\xA4\xE0\xA4\xB7 BUG CATCHER") == 0);

    /* The stat exp range survives the file; a file from before it (shorter records) loads under the base rules. */
    hu_tm_record_speed_se(tm, k, 95, 70, (hu_speed_range){ 60, 70 }, 4096, 65535);
    CHECK_EQ(hu_tm_save(tm, path), HU_TM_OK);
    CHECK_EQ(hu_tm_load(tm2, path), HU_TM_OK);
    CHECK_EQ(hu_tm_find_mon(hu_tm_find(tm2, k), 95)->se_min, 4096);
    {
        uint8_t* data;
        size_t size = hu_tm_serialize(tm, &data);
        unsigned rec = data[6] | (data[7] << 8), count = data[8], old_rec = rec - 24;
        size_t old_size = 12 + (size_t)count * old_rec + 4;
        uint8_t* old = (uint8_t*)calloc(1, old_size);
        uint32_t crc;
        memcpy(old, data, 12);
        old[6] = (uint8_t)old_rec;
        old[7] = (uint8_t)(old_rec >> 8);
        for (unsigned i = 0; i < count; i++) memcpy(old + 12 + i * old_rec, data + 12 + i * rec, old_rec);
        crc = hu_crc32(old, old_size - 4);
        for (int i = 0; i < 4; i++) old[old_size - 4 + i] = (uint8_t)(crc >> (8 * i));
        CHECK_EQ(hu_tm_deserialize(tm2, old, old_size), HU_TM_OK);
        const hu_tm_mon* onix = hu_tm_find_mon(hu_tm_find(tm2, k), 95);
        CHECK_EQ(onix->se_min, 0);
        CHECK_EQ(onix->se_max, 65535);
        CHECK_EQ(onix->speed.min, 60);
        free(old);
        free(data);
    }

    /* A damaged file is refused and leaves the store empty. */
    FILE* f = fopen(path, "r+b");
    fseek(f, 20, SEEK_SET);
    fputc(0x5A, f);
    fclose(f);
    CHECK_EQ(hu_tm_load(tm2, path), HU_TM_CORRUPT);
    CHECK_EQ(hu_tm_count(tm2), 0);
    remove(path);
    CHECK_EQ(hu_tm_load(tm2, path), HU_TM_MISSING);

    uint8_t junk[8] = "garbage";
    CHECK_EQ(hu_tm_deserialize(tm2, junk, sizeof junk), HU_TM_CORRUPT);

    hu_tm_free(tm2);
    hu_tm_free(tm);
    TEST_DONE();
}
