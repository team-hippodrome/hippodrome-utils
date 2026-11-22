#include "hippodrome/hue_choice.h"

#include <stdlib.h>
#include <string.h>

#include "file_io.h"

#define FNV_OFFSET 0xCBF29CE484222325ull
#define FNV_PRIME 0x100000001B3ull

hu_hue_id hu_hue_id_add(hu_hue_id id, const void* data, size_t size) {
    const uint8_t* p = (const uint8_t*)data;
    uint64_t h = id == 0 ? FNV_OFFSET : id;
    for (size_t i = 0; i < size; i++) {
        h ^= p[i];
        h *= FNV_PRIME;
    }
    return h == 0 ? 1 : h;
}

hu_hue_id hu_hue_id_of(const void* data, size_t size) {
    return hu_hue_id_add(0, data, size);
}

int16_t hu_hue_nickname_shift(uint8_t sum, int16_t min_degrees, int16_t max_degrees) {
    /* as the games compute it: ((max - min) << 6) * sum / 255 + (min << 6), in s32 */
    int32_t v = (((int32_t)(max_degrees - min_degrees) << 6) * (int32_t)sum) / 255 + ((int32_t)min_degrees << 6);
    return (int16_t)v;
}

static int32_t wrap(int32_t v) {
    v %= HU_HUE_FULL_TURN;
    return v < 0 ? v + HU_HUE_FULL_TURN : v;
}

int16_t hu_hue_step(int16_t shift, int direction) {
    int32_t v = wrap(shift);
    int32_t step;
    if (direction >= 0) {
        step = v / HU_HUE_STEP + 1; /* the next step strictly above */
    } else {
        step = (v + HU_HUE_STEP - 1) / HU_HUE_STEP - 1; /* the next step strictly below */
    }
    return (int16_t)wrap(step * HU_HUE_STEP);
}

int hu_hue_degrees(int16_t shift) {
    return (int)(wrap(shift) / HU_HUE_UNITS_PER_DEGREE);
}

typedef struct hue_entry {
    hu_hue_id id;
    int16_t shift;
} hue_entry;

struct hu_hue_store {
    hue_entry* entries; /* sorted by id */
    size_t count;
    size_t capacity;
    bool dirty;
};

hu_hue_store* hu_hue_store_new(void) {
    return (hu_hue_store*)calloc(1, sizeof(hu_hue_store));
}

void hu_hue_store_free(hu_hue_store* s) {
    if (s != NULL) {
        free(s->entries);
        free(s);
    }
}

void hu_hue_store_clear(hu_hue_store* s) {
    s->count = 0;
    s->dirty = false;
}

/* index of id, or of where it would go (*found false) */
static size_t find(const hu_hue_store* s, hu_hue_id id, bool* found) {
    size_t lo = 0, hi = s->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (s->entries[mid].id < id) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    *found = lo < s->count && s->entries[lo].id == id;
    return lo;
}

bool hu_hue_get(const hu_hue_store* s, hu_hue_id id, int16_t* shift) {
    bool found;
    size_t i = find(s, id, &found);
    if (found && shift != NULL) {
        *shift = s->entries[i].shift;
    }
    return found;
}

static bool put(hu_hue_store* s, hu_hue_id id, int16_t shift) {
    bool found;
    size_t i = find(s, id, &found);
    shift = (int16_t)wrap(shift);
    if (found) {
        if (s->entries[i].shift != shift) {
            s->entries[i].shift = shift;
            s->dirty = true;
        }
        return true;
    }
    if (s->count == s->capacity) {
        size_t cap = s->capacity ? s->capacity * 2 : 32;
        hue_entry* e = (hue_entry*)realloc(s->entries, cap * sizeof(hue_entry));
        if (e == NULL) {
            return false;
        }
        s->entries = e;
        s->capacity = cap;
    }
    memmove(&s->entries[i + 1], &s->entries[i], (s->count - i) * sizeof(hue_entry));
    s->entries[i].id = id;
    s->entries[i].shift = shift;
    s->count++;
    s->dirty = true;
    return true;
}

void hu_hue_set(hu_hue_store* s, hu_hue_id id, int16_t shift) {
    if (id != 0) {
        put(s, id, shift);
    }
}

void hu_hue_unset(hu_hue_store* s, hu_hue_id id) {
    bool found;
    size_t i = find(s, id, &found);
    if (found) {
        memmove(&s->entries[i], &s->entries[i + 1], (s->count - i - 1) * sizeof(hue_entry));
        s->count--;
        s->dirty = true;
    }
}

int16_t hu_hue_resolve(const hu_hue_store* s, hu_hue_id id, int16_t game_default) {
    int16_t v;
    return (s != NULL && hu_hue_get(s, id, &v)) ? v : game_default;
}

size_t hu_hue_count(const hu_hue_store* s) {
    return s->count;
}

bool hu_hue_dirty(const hu_hue_store* s) {
    return s->dirty;
}

/* File: "HPHU", u16 version, u16 record size, u32 count, records (u64 id, s16 shift, 2 bytes free), u32 CRC-32 of
 * everything before it; little-endian. A reader takes records of the size the header gives, so later versions can
 * add fields at the end of a record. */
#define HUE_MAGIC "HPHU"
#define HUE_VERSION 1
#define HEADER_SIZE 12
#define RECORD_SIZE 12

size_t hu_hue_serialize(const hu_hue_store* s, uint8_t** out) {
    size_t size = HEADER_SIZE + s->count * RECORD_SIZE + 4;
    uint8_t* buf = (uint8_t*)calloc(1, size);
    uint8_t* p;
    if (buf == NULL) {
        *out = NULL;
        return 0;
    }
    memcpy(buf, HUE_MAGIC, 4);
    hu_put16(buf + 4, HUE_VERSION);
    hu_put16(buf + 6, RECORD_SIZE);
    hu_put32(buf + 8, (uint32_t)s->count);
    p = buf + HEADER_SIZE;
    for (size_t i = 0; i < s->count; i++, p += RECORD_SIZE) {
        hu_put32(p, (uint32_t)s->entries[i].id);
        hu_put32(p + 4, (uint32_t)(s->entries[i].id >> 32));
        hu_put16(p + 8, (uint16_t)s->entries[i].shift);
    }
    hu_put32(p, hu_crc32(buf, size - 4));
    *out = buf;
    return size;
}

hu_hue_io hu_hue_deserialize(hu_hue_store* s, const uint8_t* data, size_t size) {
    uint32_t count;
    unsigned record_size;
    const uint8_t* p;
    hu_hue_store_clear(s);
    if (size < HEADER_SIZE + 4 || memcmp(data, HUE_MAGIC, 4) != 0 || hu_get16(data + 4) != HUE_VERSION) {
        return HU_HUE_CORRUPT;
    }
    record_size = hu_get16(data + 6);
    count = hu_get32(data + 8);
    if (record_size < RECORD_SIZE || (size - HEADER_SIZE - 4) / record_size < count ||
        size != HEADER_SIZE + (size_t)count * record_size + 4 ||
        hu_get32(data + size - 4) != hu_crc32(data, size - 4)) {
        return HU_HUE_CORRUPT;
    }
    p = data + HEADER_SIZE;
    for (uint32_t i = 0; i < count; i++, p += record_size) {
        hu_hue_id id = hu_get32(p) | ((uint64_t)hu_get32(p + 4) << 32);
        if (id == 0) {
            continue;
        }
        if (!put(s, id, (int16_t)hu_get16(p + 8))) {
            hu_hue_store_clear(s);
            return HU_HUE_IO_ERROR;
        }
    }
    s->dirty = false;
    return HU_HUE_OK;
}

hu_hue_io hu_hue_load(hu_hue_store* s, const char* path) {
    uint8_t* data;
    size_t size;
    hu_hue_io r;
    int rc;
    hu_hue_store_clear(s);
    rc = hu_read_file(path, &data, &size);
    if (rc != 0) {
        return rc > 0 ? HU_HUE_MISSING : HU_HUE_IO_ERROR;
    }
    r = hu_hue_deserialize(s, data, size);
    free(data);
    return r;
}

hu_hue_io hu_hue_save(hu_hue_store* s, const char* path) {
    uint8_t* data;
    size_t size = hu_hue_serialize(s, &data);
    int rc;
    if (size == 0) {
        return HU_HUE_IO_ERROR;
    }
    rc = hu_write_file(path, data, size);
    free(data);
    if (rc != 0) {
        return HU_HUE_IO_ERROR;
    }
    s->dirty = false;
    return HU_HUE_OK;
}
