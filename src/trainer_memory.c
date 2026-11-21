#include "hippodrome/trainer_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

struct hu_tm {
    hu_tm_trainer* trainers;
    size_t count;
    size_t capacity;
    bool dirty;
};

hu_tm* hu_tm_new(void) {
    return (hu_tm*)calloc(1, sizeof(hu_tm));
}

void hu_tm_free(hu_tm* tm) {
    if (tm != NULL) {
        free(tm->trainers);
        free(tm);
    }
}

void hu_tm_clear(hu_tm* tm) {
    tm->count = 0;
    tm->dirty = false;
}

size_t hu_tm_count(const hu_tm* tm) {
    return tm->count;
}

const hu_tm_trainer* hu_tm_at(const hu_tm* tm, size_t i) {
    return i < tm->count ? &tm->trainers[i] : NULL;
}

bool hu_tm_key_equal(hu_tm_key a, hu_tm_key b) {
    return a.game == b.game && a.cup == b.cup && a.division == b.division && a.round == b.round &&
           a.trainer == b.trainer;
}

static hu_tm_trainer* find(const hu_tm* tm, hu_tm_key key) {
    for (size_t i = 0; i < tm->count; i++) {
        if (hu_tm_key_equal(tm->trainers[i].key, key)) {
            return &tm->trainers[i];
        }
    }
    return NULL;
}

const hu_tm_trainer* hu_tm_find(const hu_tm* tm, hu_tm_key key) {
    return find(tm, key);
}

static hu_tm_trainer* get(hu_tm* tm, hu_tm_key key) {
    hu_tm_trainer* t = find(tm, key);
    if (t != NULL) {
        return t;
    }
    if (tm->count == tm->capacity) {
        size_t cap = tm->capacity ? tm->capacity * 2 : 32;
        hu_tm_trainer* grown = (hu_tm_trainer*)realloc(tm->trainers, cap * sizeof(*grown));
        if (grown == NULL) {
            return NULL;
        }
        tm->trainers = grown;
        tm->capacity = cap;
    }
    t = &tm->trainers[tm->count++];
    memset(t, 0, sizeof(*t));
    t->key = key;
    tm->dirty = true;
    return t;
}

const hu_tm_mon* hu_tm_find_mon(const hu_tm_trainer* t, uint8_t species) {
    if (t == NULL || species == 0) {
        return NULL;
    }
    for (int i = 0; i < t->team_count; i++) {
        if (t->team[i].species == species) {
            return &t->team[i];
        }
    }
    return NULL;
}

static hu_tm_mon* get_mon(hu_tm* tm, hu_tm_trainer* t, uint8_t species) {
    hu_tm_mon* m = (hu_tm_mon*)hu_tm_find_mon(t, species);
    if (m != NULL || species == 0) {
        return m;
    }
    if (t->team_count >= HU_TM_TEAM_MAX) {
        return NULL;
    }
    m = &t->team[t->team_count++];
    memset(m, 0, sizeof(*m));
    m->species = species;
    tm->dirty = true;
    return m;
}

void hu_tm_record_team(hu_tm* tm, hu_tm_key key, const hu_tm_team_entry* team, int count) {
    hu_tm_trainer* t = get(tm, key);
    hu_tm_mon old[HU_TM_TEAM_MAX];
    int old_count;
    if (t == NULL) {
        return;
    }
    if (count > HU_TM_TEAM_MAX) count = HU_TM_TEAM_MAX;
    if (count < 0) count = 0;
    memcpy(old, t->team, sizeof(old));
    old_count = t->team_count;
    t->team_count = 0;
    for (int i = 0; i < count; i++) {
        hu_tm_mon* m;
        const hu_tm_mon* prev = NULL;
        if (team[i].species == 0) {
            continue;
        }
        m = &t->team[t->team_count++];
        memset(m, 0, sizeof(*m));
        for (int j = 0; j < old_count; j++) {
            if (old[j].species == team[i].species) {
                prev = &old[j];
                break;
            }
        }
        m->species = team[i].species;
        m->level = team[i].level;
        memcpy(m->moves, team[i].moves, sizeof(m->moves));
        if (prev != NULL) {
            m->flags = prev->flags;
            m->speed = prev->speed;
            if (prev->level != 0 && prev->level != team[i].level) {
                m->flags &= (uint8_t)~HU_TM_MON_HAS_SPEED;
            }
            for (int s = 0; s < HU_TM_MOVES; s++) {
                if (hu_tm_move_seen(prev, m->moves[s])) {
                    m->seen |= (uint8_t)(1u << s);
                }
            }
        }
    }
    t->flags |= HU_TM_REACHED | HU_TM_TEAM_KNOWN;
    tm->dirty = true;
}

void hu_tm_record_name(hu_tm* tm, hu_tm_key key, const char* name) {
    hu_tm_trainer* t = get(tm, key);
    uint8_t buf[HU_TM_NAME_BYTES];
    size_t n = 0;
    if (t == NULL || name == NULL) {
        return;
    }
    memset(buf, 0, sizeof(buf));
    while (n < HU_TM_NAME_BYTES - 1 && name[n] != '\0') {
        buf[n] = (uint8_t)name[n];
        n++;
    }
    if (memcmp(buf, t->name, sizeof(buf)) != 0) {
        memcpy(t->name, buf, sizeof(buf));
        tm->dirty = true;
    }
}

void hu_tm_record_reached(hu_tm* tm, hu_tm_key key) {
    hu_tm_trainer* t = get(tm, key);
    if (t != NULL && !(t->flags & HU_TM_REACHED)) {
        t->flags |= HU_TM_REACHED;
        tm->dirty = true;
    }
}

void hu_tm_record_sent_out(hu_tm* tm, hu_tm_key key, uint8_t species, uint8_t level) {
    hu_tm_trainer* t = get(tm, key);
    hu_tm_mon* m = t ? get_mon(tm, t, species) : NULL;
    if (m == NULL) {
        return;
    }
    if (m->level == 0) {
        m->level = level;
        tm->dirty = true;
    }
    if (!(m->flags & HU_TM_MON_SENT_OUT)) {
        m->flags |= HU_TM_MON_SENT_OUT;
        tm->dirty = true;
    }
}

void hu_tm_record_move(hu_tm* tm, hu_tm_key key, uint8_t species, uint16_t move) {
    hu_tm_trainer* t = get(tm, key);
    hu_tm_mon* m = t ? get_mon(tm, t, species) : NULL;
    int slot = -1;
    if (m == NULL || move == 0) {
        return;
    }
    for (int s = 0; s < HU_TM_MOVES && slot < 0; s++) {
        if (m->moves[s] == move) slot = s;
    }
    for (int s = 0; s < HU_TM_MOVES && slot < 0; s++) {
        if (m->moves[s] == 0) {
            m->moves[s] = move;
            slot = s;
        }
    }
    if (slot >= 0 && !(m->seen & (1u << slot))) {
        m->seen |= (uint8_t)(1u << slot);
        tm->dirty = true;
    }
}

void hu_tm_record_speed(hu_tm* tm, hu_tm_key key, uint8_t species, hu_speed_range range) {
    hu_tm_trainer* t = get(tm, key);
    hu_tm_mon* m = t ? get_mon(tm, t, species) : NULL;
    hu_speed_range merged;
    if (m == NULL) {
        return;
    }
    merged = (m->flags & HU_TM_MON_HAS_SPEED) ? hu_speed_intersect(m->speed, range) : range;
    if (!(m->flags & HU_TM_MON_HAS_SPEED) || merged.min != m->speed.min || merged.max != m->speed.max) {
        m->speed = merged;
        m->flags |= HU_TM_MON_HAS_SPEED;
        tm->dirty = true;
    }
}

bool hu_tm_move_seen(const hu_tm_mon* mon, uint16_t move) {
    if (mon == NULL || move == 0) {
        return false;
    }
    for (int s = 0; s < HU_TM_MOVES; s++) {
        if (mon->moves[s] == move && (mon->seen & (1u << s))) {
            return true;
        }
    }
    return false;
}

bool hu_tm_dirty(const hu_tm* tm) {
    return tm->dirty;
}

/* ---- file format ----
 * All little-endian.
 *   0  "HPTM"            magic
 *   4  u16 version (1)
 *   6  u16 record size (RECORD_SIZE)
 *   8  u32 trainer count
 *  12  records
 * end  u32 CRC-32 (zlib polynomial) of everything before it
 * Record: key (game, cup, division, round, trainer), flags, team_count, pad, then 6 mons of MON_SIZE bytes:
 *   species, level, seen, flags, moves u16[4], speed min u16, speed max u16,
 * then the name (HU_TM_NAME_BYTES). A reader takes records of the size the header gives, so later versions can add
 * fields at the end of a record. */
#define TM_MAGIC "HPTM"
#define TM_VERSION 1
#define HEADER_SIZE 12
#define MON_SIZE (4 + 2 * HU_TM_MOVES + 4)
#define RECORD_SIZE (8 + HU_TM_TEAM_MAX * MON_SIZE + HU_TM_NAME_BYTES)

static uint32_t crc32_bytes(const uint8_t* p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return c ^ 0xFFFFFFFFu;
}

static void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t* p, uint32_t v) {
    put16(p, (uint16_t)v);
    put16(p + 2, (uint16_t)(v >> 16));
}
static uint16_t get16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t get32(const uint8_t* p) {
    return get16(p) | ((uint32_t)get16(p + 2) << 16);
}

size_t hu_tm_serialize(const hu_tm* tm, uint8_t** out) {
    size_t size = HEADER_SIZE + tm->count * RECORD_SIZE + 4;
    uint8_t* buf = (uint8_t*)calloc(1, size);
    uint8_t* p;
    if (buf == NULL) {
        *out = NULL;
        return 0;
    }
    memcpy(buf, TM_MAGIC, 4);
    put16(buf + 4, TM_VERSION);
    put16(buf + 6, RECORD_SIZE);
    put32(buf + 8, (uint32_t)tm->count);
    p = buf + HEADER_SIZE;
    for (size_t i = 0; i < tm->count; i++) {
        const hu_tm_trainer* t = &tm->trainers[i];
        p[0] = t->key.game;
        p[1] = t->key.cup;
        p[2] = t->key.division;
        p[3] = t->key.round;
        p[4] = t->key.trainer;
        p[5] = t->flags;
        p[6] = t->team_count;
        for (int j = 0; j < HU_TM_TEAM_MAX; j++) {
            const hu_tm_mon* m = &t->team[j];
            uint8_t* q = p + 8 + j * MON_SIZE;
            if (j >= t->team_count) {
                continue;
            }
            q[0] = m->species;
            q[1] = m->level;
            q[2] = m->seen;
            q[3] = m->flags;
            for (int s = 0; s < HU_TM_MOVES; s++) {
                put16(q + 4 + 2 * s, m->moves[s]);
            }
            put16(q + 4 + 2 * HU_TM_MOVES, m->speed.min);
            put16(q + 6 + 2 * HU_TM_MOVES, m->speed.max);
        }
        memcpy(p + 8 + HU_TM_TEAM_MAX * MON_SIZE, t->name, HU_TM_NAME_BYTES);
        p += RECORD_SIZE;
    }
    put32(p, crc32_bytes(buf, size - 4));
    *out = buf;
    return size;
}

hu_tm_io hu_tm_deserialize(hu_tm* tm, const uint8_t* data, size_t size) {
    uint32_t count;
    unsigned record_size;
    const uint8_t* p;
    hu_tm_clear(tm);
    if (size < HEADER_SIZE + 4 || memcmp(data, TM_MAGIC, 4) != 0 || get16(data + 4) != TM_VERSION) {
        return HU_TM_CORRUPT;
    }
    record_size = get16(data + 6);
    count = get32(data + 8);
    if (record_size < RECORD_SIZE || (size - HEADER_SIZE - 4) / record_size < count ||
        size != HEADER_SIZE + (size_t)count * record_size + 4 || get32(data + size - 4) != crc32_bytes(data, size - 4)) {
        return HU_TM_CORRUPT;
    }
    p = data + HEADER_SIZE;
    for (uint32_t i = 0; i < count; i++, p += record_size) {
        hu_tm_key key = { p[0], p[1], p[2], p[3], p[4] };
        hu_tm_trainer* t = get(tm, key);
        if (t == NULL) {
            hu_tm_clear(tm);
            return HU_TM_IO_ERROR;
        }
        t->flags = p[5];
        t->team_count = p[6] > HU_TM_TEAM_MAX ? HU_TM_TEAM_MAX : p[6];
        for (int j = 0; j < t->team_count; j++) {
            const uint8_t* q = p + 8 + j * MON_SIZE;
            hu_tm_mon* m = &t->team[j];
            m->species = q[0];
            m->level = q[1];
            m->seen = q[2];
            m->flags = q[3];
            for (int s = 0; s < HU_TM_MOVES; s++) {
                m->moves[s] = get16(q + 4 + 2 * s);
            }
            m->speed.min = get16(q + 4 + 2 * HU_TM_MOVES);
            m->speed.max = get16(q + 6 + 2 * HU_TM_MOVES);
        }
        memcpy(t->name, p + 8 + HU_TM_TEAM_MAX * MON_SIZE, HU_TM_NAME_BYTES);
        t->name[HU_TM_NAME_BYTES - 1] = 0;
    }
    tm->dirty = false;
    return HU_TM_OK;
}

static FILE* open_utf8(const char* path, const char* mode) {
#ifdef _WIN32
    wchar_t wpath[1024], wmode[8];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 1024) == 0 ||
        MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, 8) == 0) {
        return NULL;
    }
    return _wfopen(wpath, wmode);
#else
    return fopen(path, mode);
#endif
}

static int replace_file(const char* from, const char* to) {
#ifdef _WIN32
    wchar_t wfrom[1024], wto[1024];
    if (MultiByteToWideChar(CP_UTF8, 0, from, -1, wfrom, 1024) == 0 ||
        MultiByteToWideChar(CP_UTF8, 0, to, -1, wto, 1024) == 0) {
        return -1;
    }
    return MoveFileExW(wfrom, wto, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1;
#else
    return rename(from, to);
#endif
}

hu_tm_io hu_tm_load(hu_tm* tm, const char* path) {
    FILE* f = open_utf8(path, "rb");
    uint8_t* data;
    long size;
    hu_tm_io r;
    hu_tm_clear(tm);
    if (f == NULL) {
        return HU_TM_MISSING;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return HU_TM_IO_ERROR;
    }
    data = (uint8_t*)malloc(size > 0 ? (size_t)size : 1);
    if (data == NULL || fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return HU_TM_IO_ERROR;
    }
    fclose(f);
    r = hu_tm_deserialize(tm, data, (size_t)size);
    free(data);
    return r;
}

hu_tm_io hu_tm_save(hu_tm* tm, const char* path) {
    uint8_t* data;
    size_t size = hu_tm_serialize(tm, &data);
    size_t len = strlen(path);
    char* tmp;
    FILE* f;
    bool ok;
    if (size == 0) {
        return HU_TM_IO_ERROR;
    }
    tmp = (char*)malloc(len + 5);
    if (tmp == NULL) {
        free(data);
        return HU_TM_IO_ERROR;
    }
    memcpy(tmp, path, len);
    memcpy(tmp + len, ".tmp", 5);
    f = open_utf8(tmp, "wb");
    ok = f != NULL && fwrite(data, 1, size, f) == size;
    if (f != NULL) {
        ok = (fflush(f) == 0) && ok;
        ok = (fclose(f) == 0) && ok;
    }
    free(data);
    if (!ok || replace_file(tmp, path) != 0) {
        remove(tmp);
        free(tmp);
        return HU_TM_IO_ERROR;
    }
    free(tmp);
    tm->dirty = false;
    return HU_TM_OK;
}
