#include "hippodrome/trainer_memory.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc32(const uint8_t* d, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

/* The file as a reader from before the Stat Exp clamps wrote it: records cut at the name. */
static size_t to_v1(const uint8_t* in, size_t size, uint8_t* out) {
    unsigned rs = in[6] | (in[7] << 8);
    unsigned v1 = rs - 6 * 4;
    uint32_t count = in[8] | (in[9] << 8) | (in[10] << 16) | ((uint32_t)in[11] << 24);
    (void)size;
    memcpy(out, in, 12);
    out[6] = (uint8_t)v1;
    out[7] = (uint8_t)(v1 >> 8);
    for (uint32_t i = 0; i < count; i++) memcpy(out + 12 + i * v1, in + 12 + i * rs, v1);
    size_t n = 12 + count * v1;
    uint32_t c = crc32(out, n);
    for (int b = 0; b < 4; b++) out[n + b] = (uint8_t)(c >> (8 * b));
    return n + 4;
}

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

    /* Stat Exp clamping: an estimate keeps the clamp it was learned under. Onix (base 70) L28. */
    {
        hu_tm* se = hu_tm_new();
        hu_tm_team_entry onix[1] = { { 95, 28, { 88, 0, 0, 0 } } };
        hu_tm_record_team(se, k, onix, 1);
        uint16_t lo, hi;
        hu_speed_range full = hu_speed_initial_se(70, 28, 65535, 65535);
        hu_speed_range r1 = { full.max, full.max }; /* the top: DV 15 */
        hu_tm_record_speed_se(se, k, 95, r1, 70, 65535, 65535);
        const hu_tm_mon* m = hu_tm_find_mon(hu_tm_find(se, k), 95);
        CHECK(m->flags & HU_TM_MON_SPEED_SE);
        hu_tm_mon_speed_se(m, &lo, &hi);
        CHECK_EQ(lo, 65535);
        CHECK_EQ(hi, 65535);
        /* Under its own clamp it is unchanged; under the game's own stat exp it widens but stays DV 15's span. */
        hu_speed_range u = hu_tm_speed_under(m, 70, 65535, 65535);
        CHECK_EQ(u.min, full.max);
        CHECK_EQ(u.max, full.max);
        u = hu_tm_speed_under(m, 70, 0, 65535);
        hu_speed_range dv15 = hu_speed_rescale(r1, 70, 28, 65535, 65535, 0, 65535);
        CHECK_EQ(u.min, dv15.min);
        CHECK_EQ(u.max, dv15.max);
        CHECK(u.min > hu_speed_initial(70, 28).min);
        /* The player switched back to Vanilla and learned more there: merged under vanilla, flag cleared. */
        hu_speed_range r2 = { (uint16_t)(u.max - 1), 999 };
        hu_tm_record_speed_se(se, k, 95, r2, 70, 0, 65535);
        m = hu_tm_find_mon(hu_tm_find(se, k), 95);
        CHECK(!(m->flags & HU_TM_MON_SPEED_SE));
        CHECK_EQ(m->speed.min, u.max - 1);
        CHECK_EQ(m->speed.max, u.max);
        hu_tm_mon_speed_se(m, &lo, &hi);
        CHECK_EQ(lo, 0);
        CHECK_EQ(hi, 65535);
        /* Record under a clamp again and round trip: the clamp survives the file. */
        hu_tm_record_speed_se(se, k, 95, full, 70, 16384, 36863);
        uint8_t* buf = NULL;
        size_t n = hu_tm_serialize(se, &buf);
        hu_tm* back = hu_tm_new();
        CHECK_EQ(hu_tm_deserialize(back, buf, n), HU_TM_OK);
        m = hu_tm_find_mon(hu_tm_find(back, k), 95);
        CHECK(m->flags & HU_TM_MON_SPEED_SE);
        hu_tm_mon_speed_se(m, &lo, &hi);
        CHECK_EQ(lo, 16384);
        CHECK_EQ(hi, 36863);
        /* A file from before the clamps (shorter records) loads, with every estimate taken as vanilla. */
        uint8_t* old = malloc(n);
        size_t on = to_v1(buf, n, old);
        CHECK_EQ(hu_tm_deserialize(back, old, on), HU_TM_OK);
        m = hu_tm_find_mon(hu_tm_find(back, k), 95);
        CHECK(m->flags & HU_TM_MON_HAS_SPEED);
        CHECK(!(m->flags & HU_TM_MON_SPEED_SE));
        CHECK_EQ(hu_tm_find_mon(hu_tm_find(back, k), 95)->moves[0], 88);
        free(old);
        free(buf);
        hu_tm_free(back);
        hu_tm_free(se);
    }

    hu_tm_free(tm2);
    hu_tm_free(tm);
    TEST_DONE();
}
