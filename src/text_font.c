/* Glyphs for translated text (hippodrome/text_font.h). Moved from Plethron's src/port/unicode_font.cpp. */
#include "hippodrome/text_font.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* a private copy (STBTT_STATIC), so it never clashes with a port's own stb_truetype or Dear ImGui's */
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include "../third_party/stb_truetype.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }
static double dmin(double a, double b) { return a < b ? a : b; }
static double dmax(double a, double b) { return a > b ? a : b; }
static int iround(double v) { return (int)lround(v); }

hu_glyph_style hu_glyph_style_for(int height, hu_glyph_style small_style) {
    if (height >= HU_GLYPH_OUTLINE_MIN_HEIGHT) {
        return HU_GLYPH_OUTLINE;
    }
    return (small_style >= HU_GLYPH_PLAIN && small_style <= HU_GLYPH_BOLD) ? small_style : HU_GLYPH_BOLD;
}

void hu_glyph_free(hu_glyph* glyph) {
    if (glyph != NULL) {
        free(glyph->tex);
        memset(glyph, 0, sizeof *glyph);
    }
}

/* ---- GNU Unifont ---- */

typedef struct {
    uint32_t cp;
    int width; /* 8 or 16 */
    uint16_t rows[16];
} hu_bitmap;

struct hu_unifont {
    hu_bitmap* glyphs; /* sorted by code point */
    int count;
};

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int by_cp(const void* a, const void* b) {
    uint32_t x = ((const hu_bitmap*)a)->cp, y = ((const hu_bitmap*)b)->cp;
    return (x > y) - (x < y);
}

hu_unifont* hu_unifont_parse(const char* hex, size_t len) {
    hu_unifont* font = calloc(1, sizeof *font);
    if (font == NULL) {
        return NULL;
    }
    int cap = 0;
    size_t pos = 0;
    while (hex != NULL && pos < len) {
        size_t end = pos;
        while (end < len && hex[end] != '\n') end++;
        const char* line = hex + pos;
        size_t n = end - pos;
        pos = end + 1;
        while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == ' ')) n--;
        if (n == 0 || line[0] == '#') continue;
        size_t colon = 0;
        while (colon < n && line[colon] != ':') colon++;
        if (colon == 0 || colon == n) continue;
        hu_bitmap b;
        memset(&b, 0, sizeof b);
        int ok = 1;
        for (size_t i = 0; i < colon && ok; i++) {
            int d = hex_digit(line[i]);
            ok = d >= 0;
            b.cp = (b.cp << 4) | (uint32_t)(d & 0xF);
        }
        size_t bits = n - colon - 1;
        if (!ok || (bits != 32 && bits != 64)) continue;
        b.width = bits == 32 ? 8 : 16;
        const int digits = b.width / 4;
        for (int y = 0; y < 16 && ok; y++) {
            uint16_t row = 0;
            for (int k = 0; k < digits && ok; k++) {
                int d = hex_digit(line[colon + 1 + y * digits + k]);
                ok = d >= 0;
                row = (uint16_t)((row << 4) | (d & 0xF));
            }
            b.rows[y] = row;
        }
        if (!ok) continue;
        if (font->count == cap) {
            cap = cap ? cap * 2 : 256;
            hu_bitmap* grown = realloc(font->glyphs, (size_t)cap * sizeof *grown);
            if (grown == NULL) {
                hu_unifont_free(font);
                return NULL;
            }
            font->glyphs = grown;
        }
        font->glyphs[font->count++] = b;
    }
    if (font->count > 1) {
        qsort(font->glyphs, (size_t)font->count, sizeof *font->glyphs, by_cp);
    }
    return font;
}

void hu_unifont_free(hu_unifont* font) {
    if (font != NULL) {
        free(font->glyphs);
        free(font);
    }
}

int hu_unifont_count(const hu_unifont* font) {
    return font ? font->count : 0;
}

static const hu_bitmap* find_bitmap(const hu_unifont* font, uint32_t cp) {
    if (font == NULL || font->count == 0) {
        return NULL;
    }
    hu_bitmap key;
    key.cp = cp;
    return bsearch(&key, font->glyphs, (size_t)font->count, sizeof *font->glyphs, by_cp);
}

int hu_unifont_has(const hu_unifont* font, uint32_t cp) {
    return find_bitmap(font, cp) != NULL;
}

static int pixel(const hu_bitmap* b, int x, int y) {
    if (x < 0 || y < 0 || x >= b->width || y >= 16) {
        return 0;
    }
    return (b->rows[y] >> (b->width - 1 - x)) & 1;
}

/* The outlined glyph at Unifont scale: (width + 2) x 18 cells, 2 = fill, 1 = outline, 0 = clear. Then an area
 * resample to `height` rows: each output texel's alpha is its covered fraction and its intensity the fill's share of
 * that coverage, so the outline reads as dark like the games' glyphs. The plain styles keep the same size and
 * advance (the layout does not move) and are all white, like the games' small sets. */
int hu_unifont_glyph(const hu_unifont* font, uint32_t cp, int height, hu_glyph_style style, hu_glyph* out) {
    const hu_bitmap* b = find_bitmap(font, cp);
    memset(out, 0, sizeof *out);
    if (b == NULL || height <= 0 || height > HU_GLYPH_MAX_HEIGHT) {
        return 0;
    }
    const int sw = b->width + 2, sh = 18;
    unsigned char src[18 * 18];
    memset(src, 0, sizeof src);
    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            unsigned char* cell = &src[y * sw + x];
            if (pixel(b, x - 1, y - 1)) {
                *cell = 2;
                continue;
            }
            if (style == HU_GLYPH_BOLD) {
                if (pixel(b, x - 2, y - 1) || pixel(b, x - 1, y - 2) || pixel(b, x - 2, y - 2)) {
                    *cell = 2;
                }
                continue;
            }
            if (style == HU_GLYPH_PLAIN) {
                continue;
            }
            for (int dy = -1; dy <= 1 && !*cell; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (pixel(b, x - 1 + dx, y - 1 + dy)) {
                        *cell = 1;
                        break;
                    }
                }
            }
        }
    }
    const double scale = (double)height / sh;
    out->height = height;
    out->draw_width = imax(1, iround(sw * scale));
    out->tex_width = (out->draw_width + 7) & ~7;
    /* Unifont glyphs carry their own side bearing; let neighbouring outlines touch. */
    out->advance = imax(1, out->draw_width - iround(scale));
    out->tex = calloc((size_t)out->tex_width * (size_t)height, 1);
    if (out->tex == NULL) {
        return 0;
    }
    for (int ty = 0; ty < height; ty++) {
        double y0 = ty / scale, y1 = (ty + 1) / scale;
        for (int tx = 0; tx < out->draw_width; tx++) {
            double x0 = tx / scale, x1 = (tx + 1) / scale;
            double fill = 0, line = 0, area = (x1 - x0) * (y1 - y0);
            for (int sy = (int)y0; sy < sh && sy < y1; sy++) {
                double oy = dmin(y1, sy + 1.0) - dmax(y0, (double)sy);
                for (int sx = (int)x0; sx < sw && sx < x1; sx++) {
                    double o = oy * (dmin(x1, sx + 1.0) - dmax(x0, (double)sx));
                    if (src[sy * sw + sx] == 2) {
                        fill += o;
                    } else if (src[sy * sw + sx] == 1) {
                        line += o;
                    }
                }
            }
            double a = (fill + line) / area;
            if (style == HU_GLYPH_PLAIN) {
                a *= 1.6;
            }
            if (a <= 0) {
                continue;
            }
            int alpha = imin(15, iround(a * 15));
            int intensity = imin(15, iround(fill / (fill + line) * 15));
            out->tex[ty * out->tex_width + tx] = (unsigned char)((intensity << 4) | alpha);
        }
    }
    return 1;
}

/* ---- Pop ---- */

struct hu_pop {
    unsigned char* data;
    stbtt_fontinfo info;
};

hu_pop* hu_pop_load(const void* ttf, size_t len) {
    if (ttf == NULL || len < 12) {
        return NULL;
    }
    hu_pop* font = calloc(1, sizeof *font);
    if (font == NULL) {
        return NULL;
    }
    font->data = malloc(len);
    if (font->data == NULL) {
        free(font);
        return NULL;
    }
    memcpy(font->data, ttf, len);
    int offset = stbtt_GetFontOffsetForIndex(font->data, 0);
    if (offset < 0 || !stbtt_InitFont(&font->info, font->data, offset)) {
        hu_pop_free(font);
        return NULL;
    }
    return font;
}

void hu_pop_free(hu_pop* font) {
    if (font != NULL) {
        free(font->data);
        free(font);
    }
}

int hu_pop_has(const hu_pop* font, uint32_t cp) {
    return font != NULL && stbtt_FindGlyphIndex(&font->info, (int)cp) != 0;
}

/* The outline is a grey dilation of the supersampled fill by a disc of radius rs: each texel takes the largest fill
 * within dx*dx + dy*dy <= rs*rs + rs. The disc is a stack of rows, row dy reaching `reach[dy]` either side, so the
 * dilation is the largest, over dy, of fill row y + dy's running maximum over [x - reach, x + reach]. The running
 * maxima for each reach are built one step wider at a time (wider[k] = max(wider[k-1], x - k, x + k)). Same pixels as
 * testing the whole disc at each texel, which took 25-70 ms a glyph at 64 pixels. */
int hu_pop_glyph(const hu_pop* font, uint32_t cp, int height, hu_glyph_style style, hu_glyph* out) {
    memset(out, 0, sizeof *out);
    if (font == NULL || height <= 0 || height > HU_GLYPH_MAX_HEIGHT) {
        return 0;
    }
    const stbtt_fontinfo* info = &font->info;
    const int index = stbtt_FindGlyphIndex(info, (int)cp);
    if (index == 0) {
        return 0;
    }
    enum { SS = 4 };
    const float scale = stbtt_ScaleForMappingEmToPixels(info, (float)(height * SS));
    int adv_units, lsb;
    stbtt_GetGlyphHMetrics(info, index, &adv_units, &lsb);
    const float r = height / 16.0f > 1.0f ? height / 16.0f : 1.0f;
    const int pad = (int)ceilf(r);
    const int adv = imax(1, iround(adv_units * scale / SS));
    out->height = height;
    out->draw_width = adv + 2 * pad;
    out->tex_width = (out->draw_width + 7) & ~7;
    out->advance = adv;
    const int cw = out->draw_width * SS, ch = height * SS;
    /* Pop is extra bold already, so both plain styles just leave the outline out (rs 0: nothing is dilated) */
    const int rs = (style == HU_GLYPH_OUTLINE) ? iround(r * SS) : 0;
    const size_t plane = (size_t)cw * (size_t)ch;
    unsigned char* fill = calloc(plane, 1);
    unsigned char* wider = calloc(plane * (size_t)(rs + 1), 1);
    unsigned char* line = calloc(plane, 1);
    int* reach = malloc(sizeof(int) * (size_t)(2 * rs + 1));
    out->tex = calloc((size_t)out->tex_width * (size_t)height, 1);
    int ok = fill && wider && line && reach && out->tex;
    if (ok) {
        int x0, y0, x1, y1;
        stbtt_GetGlyphBitmapBox(info, index, scale, scale, &x0, &y0, &x1, &y1);
        const int ox = pad * SS, base = iround(height * SS * 0.85f);
        if (x1 > x0 && y1 > y0) {
            const int bw = x1 - x0, bh = y1 - y0;
            unsigned char* bmp = malloc((size_t)bw * (size_t)bh);
            ok = bmp != NULL;
            if (ok) {
                stbtt_MakeGlyphBitmap(info, bmp, bw, bh, bw, scale, scale, index);
                for (int y = 0; y < bh; y++) {
                    int dy = base + y0 + y;
                    if (dy < 0 || dy >= ch) continue;
                    for (int x = 0; x < bw; x++) {
                        int dx = ox + x0 + x;
                        if (dx >= 0 && dx < cw) fill[(size_t)dy * cw + dx] = bmp[y * bw + x];
                    }
                }
                free(bmp);
            }
        }
    }
    if (ok) {
        for (int dy = -rs; dy <= rs; dy++) {
            int w = -1;
            while ((w + 1) * (w + 1) + dy * dy <= rs * rs + rs) w++;
            reach[dy + rs] = w; /* >= rs for dy = 0, so every row has at least its own texel */
        }
        memcpy(wider, fill, plane);
        for (int k = 1; k <= rs; k++) {
            const unsigned char* prev = &wider[(size_t)(k - 1) * plane];
            unsigned char* cur = &wider[(size_t)k * plane];
            for (int y = 0; y < ch; y++) {
                const unsigned char* f = &fill[(size_t)y * cw];
                for (int x = 0; x < cw; x++) {
                    unsigned char m = prev[(size_t)y * cw + x];
                    if (x - k >= 0 && f[x - k] > m) m = f[x - k];
                    if (x + k < cw && f[x + k] > m) m = f[x + k];
                    cur[(size_t)y * cw + x] = m;
                }
            }
        }
        for (int y = 0; y < ch; y++) {
            unsigned char* o = &line[(size_t)y * cw];
            for (int dy = -rs; dy <= rs; dy++) {
                const int sy = y + dy;
                if (sy < 0 || sy >= ch || reach[dy + rs] < 0) continue;
                const unsigned char* row = &wider[(size_t)reach[dy + rs] * plane + (size_t)sy * cw];
                for (int x = 0; x < cw; x++) {
                    if (row[x] > o[x]) o[x] = row[x];
                }
            }
        }
        for (int ty = 0; ty < height; ty++) {
            for (int tx = 0; tx < out->draw_width; tx++) {
                int f = 0, l = 0;
                for (int sy = 0; sy < SS; sy++) {
                    for (int sx = 0; sx < SS; sx++) {
                        size_t i = (size_t)(ty * SS + sy) * cw + (size_t)(tx * SS + sx);
                        f += fill[i];
                        l += line[i];
                    }
                }
                double fl = f / (255.0 * SS * SS), all = dmax(fl, l / (255.0 * SS * SS));
                if (all <= 0) continue;
                int alpha = imin(15, iround(all * 15));
                int intensity = imin(15, iround(fl / all * 15));
                out->tex[ty * out->tex_width + tx] = (unsigned char)((intensity << 4) | alpha);
            }
        }
    }
    free(fill);
    free(wider);
    free(line);
    free(reach);
    if (!ok) {
        hu_glyph_free(out);
    }
    return ok;
}

/* ---- UTF-8 ---- */

uint32_t hu_utf8_next(const char** s) {
    const unsigned char* p = (const unsigned char*)*s;
    uint32_t c = *p;
    int extra = 0;
    if (c == 0) {
        return 0;
    }
    if (c >= 0xF0) {
        c &= 0x07, extra = 3;
    } else if (c >= 0xE0) {
        c &= 0x0F, extra = 2;
    } else if (c >= 0xC0) {
        c &= 0x1F, extra = 1;
    } else if (c >= 0x80) {
        *s += 1;
        return 0xFFFD; /* stray continuation byte */
    }
    p++;
    for (int i = 0; i < extra; i++, p++) {
        if ((*p & 0xC0) != 0x80) {
            *s = (const char*)p;
            return 0xFFFD;
        }
        c = (c << 6) | (*p & 0x3F);
    }
    *s = (const char*)p;
    return c;
}
