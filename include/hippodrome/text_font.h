/* Glyphs for translated text, drawn in the style of the Stadium games' own font: white fill, a one-pixel dark outline
 * from 16 pixels up, as IA8 textures (4-bit intensity, 4-bit alpha) the ports hand to the RDP like the game's glyphs.
 *
 * Two faces (assets/font, see TRANSLATION.md "Rendering"):
 *   - GNU Unifont (unifont_subset.hex, tools/font/make_unifont_subset.py): 16-pixel bitmaps resampled to the height
 *     asked for. Covers every character of every port's tables: the default, and the fallback for Pop.
 *   - Pop (pop_subset.ttf, tools/font/make_pop_font.py): the prototype rounded outline font, Latin drawn by Team
 *     Hippodrome, kana and kanji from M PLUS Rounded 1c, Hangul from Jua (SIL OFL 1.1).
 *
 * Nothing here reads files, settings or game memory: the port loads the files (from its archive or next to the
 * executable), picks the face and style from its own settings, and caches the glyphs it builds. A loaded font is
 * only read by the glyph builders, so several threads may build glyphs from it at once (a port may prewarm a
 * screen's glyphs on a worker).
 *
 * Moved from Plethron's src/port/unicode_font.cpp (team-hippodrome/plethron 0634dd6).
 */
#ifndef HIPPODROME_TEXT_FONT_H
#define HIPPODROME_TEXT_FONT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* How a glyph is drawn. The games' own 16x10 and 16x12 font sets have no outline (plain white); 16x16 and up are
 * outlined. Plain raises a bare bitmap's alpha (a one-pixel Unifont stroke covers ~60% of a texel at 10 pixels and
 * reads as grey); bold also fills the cell right of and below each pixel (two-pixel strokes, like the games' small
 * kana). Pop is extra bold already, so its plain and bold are the same: no outline. */
typedef enum hu_glyph_style {
    HU_GLYPH_PLAIN = 0,
    HU_GLYPH_OUTLINE = 1,
    HU_GLYPH_BOLD = 2
} hu_glyph_style;

/* Below this height the games' font sets have no outline. */
#define HU_GLYPH_OUTLINE_MIN_HEIGHT 16
/* The tallest glyph the builders make. */
#define HU_GLYPH_MAX_HEIGHT 64

/* The style for text `height` pixels tall: outlined from HU_GLYPH_OUTLINE_MIN_HEIGHT up, else `small_style` (the
 * port's setting; Plethron's "Small text" defaults to HU_GLYPH_BOLD). */
hu_glyph_style hu_glyph_style_for(int height, hu_glyph_style small_style);

/* One built glyph: `height` rows of `tex_width` IA8 texels (a multiple of 8), of which `draw_width` are drawn; the pen
 * then moves `advance` pixels. Free it with hu_glyph_free. */
typedef struct hu_glyph {
    unsigned char* tex;
    int tex_width;
    int draw_width;
    int advance;
    int height;
} hu_glyph;

void hu_glyph_free(hu_glyph* glyph);

/* ---- GNU Unifont ---- */

typedef struct hu_unifont hu_unifont;

/* Parses Unifont .hex text (CODEPOINT:HEXBITS lines, '#' comments). NULL when out of memory; an empty or unreadable
 * text gives a font with no glyphs. */
hu_unifont* hu_unifont_parse(const char* hex, size_t len);
void hu_unifont_free(hu_unifont* font);
int hu_unifont_count(const hu_unifont* font);
int hu_unifont_has(const hu_unifont* font, uint32_t cp);
/* Builds `cp` at `height` (1..HU_GLYPH_MAX_HEIGHT) into *out. 0 when the font has no such glyph or out of memory. */
int hu_unifont_glyph(const hu_unifont* font, uint32_t cp, int height, hu_glyph_style style, hu_glyph* out);

/* ---- Pop ---- */

typedef struct hu_pop hu_pop;

/* Loads a TrueType font (pop_subset.ttf); the bytes are copied. NULL when it is not a font or out of memory. The em
 * is the whole cell (ascent 850, descent 150 of 1000), so a glyph is drawn `height` pixels tall. */
hu_pop* hu_pop_load(const void* ttf, size_t len);
void hu_pop_free(hu_pop* font);
int hu_pop_has(const hu_pop* font, uint32_t cp);
/* Builds `cp` at `height` (1..HU_GLYPH_MAX_HEIGHT) into *out: rendered 4x supersampled, outlined by dilating the
 * fill with a disc (1 pixel per 16 of height, at least one), box-filtered down. 0 when the font has no such glyph. */
int hu_pop_glyph(const hu_pop* font, uint32_t cp, int height, hu_glyph_style style, hu_glyph* out);

/* ---- UTF-8 ---- */

/* Decodes the UTF-8 character at *s and advances *s past it. Returns 0 at the end and U+FFFD for a bad sequence. */
uint32_t hu_utf8_next(const char** s);

#ifdef __cplusplus
}
#endif

#endif
