#include "hippodrome/text_font.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

static char* read_all(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc((size_t)n + 1);
    *len = fread(buf, 1, (size_t)n, f);
    fclose(f);
    return buf;
}

int main(void) {
    /* a half-width glyph: 'A' from Unifont, and a full-width box */
    const char* hex = "# comment\r\n0041:0000000018242442427E424242420000\n"
                      "3042:" "FFFF" "8001" "8001" "8001" "8001" "8001" "8001" "8001"
                      "8001" "8001" "8001" "8001" "8001" "8001" "8001" "FFFF\n"
                      "bad line\n0042:123\n";
    hu_unifont* u = hu_unifont_parse(hex, strlen(hex));
    CHECK(u != NULL);
    CHECK_EQ(hu_unifont_count(u), 2);
    CHECK(hu_unifont_has(u, 'A'));
    CHECK(hu_unifont_has(u, 0x3042));
    CHECK(!hu_unifont_has(u, 'B'));

    hu_glyph g;
    /* drawn 18 pixels tall, Unifont plus its outline is 1:1: 8 + 2 wide, advancing one less */
    CHECK(hu_unifont_glyph(u, 'A', 18, HU_GLYPH_OUTLINE, &g));
    CHECK_EQ(g.draw_width, 10);
    CHECK_EQ(g.tex_width, 16);
    CHECK_EQ(g.advance, 9);
    CHECK_EQ(g.height, 18);
    /* row 5 (Unifont row 4, 00011000): fill (white, opaque) with outline (dark, opaque) either side */
    CHECK_EQ(g.tex[5 * 16 + 4], 0xFF);
    CHECK_EQ(g.tex[5 * 16 + 3], 0x0F);
    CHECK_EQ(g.tex[0], 0x00);
    hu_glyph_free(&g);
    CHECK(g.tex == NULL);
    CHECK(hu_unifont_glyph(u, 0x3042, 9, HU_GLYPH_PLAIN, &g));
    CHECK_EQ(g.draw_width, 9);
    CHECK_EQ(g.tex_width, 16);
    hu_glyph_free(&g);
    CHECK(!hu_unifont_glyph(u, 'B', 16, HU_GLYPH_OUTLINE, &g));
    CHECK(!hu_unifont_glyph(u, 'A', 0, HU_GLYPH_OUTLINE, &g));
    CHECK(!hu_unifont_glyph(u, 'A', HU_GLYPH_MAX_HEIGHT + 1, HU_GLYPH_OUTLINE, &g));
    hu_unifont_free(u);

    CHECK_EQ(hu_glyph_style_for(16, HU_GLYPH_PLAIN), HU_GLYPH_OUTLINE);
    CHECK_EQ(hu_glyph_style_for(12, HU_GLYPH_PLAIN), HU_GLYPH_PLAIN);
    CHECK_EQ(hu_glyph_style_for(10, (hu_glyph_style)7), HU_GLYPH_BOLD);

    /* the committed fonts load and cover the languages */
    size_t len = 0;
    char* data = read_all(HU_FONT_DIR "/unifont_subset.hex", &len);
    CHECK(data != NULL);
    u = hu_unifont_parse(data, len);
    CHECK(hu_unifont_count(u) > 1000);
    CHECK(hu_unifont_has(u, 0x00E9) && hu_unifont_has(u, 0x65E5) && hu_unifont_has(u, 0xD55C));
    hu_unifont_free(u);
    free(data);
    data = read_all(HU_FONT_DIR "/pop_subset.ttf", &len);
    CHECK(data != NULL);
    hu_pop* p = hu_pop_load(data, len);
    CHECK(p != NULL);
    CHECK(hu_pop_has(p, 'A') && hu_pop_has(p, 0x3042));
    CHECK(hu_pop_glyph(p, 'A', 32, HU_GLYPH_OUTLINE, &g));
    CHECK_EQ(g.height, 32);
    CHECK(g.draw_width > g.advance);
    hu_glyph_free(&g);
    hu_pop_free(p);
    CHECK(hu_pop_load("not a font at all", 17) == NULL);
    free(data);

    const char* s = "a\xC3\xA9\xE6\x97\xA5\xF0\x9F\x98\x80\x80";
    CHECK_EQ(hu_utf8_next(&s), 'a');
    CHECK_EQ(hu_utf8_next(&s), 0xE9);
    CHECK_EQ(hu_utf8_next(&s), 0x65E5);
    CHECK_EQ(hu_utf8_next(&s), 0x1F600);
    CHECK_EQ(hu_utf8_next(&s), 0xFFFD);
    CHECK_EQ(hu_utf8_next(&s), 0);
    TEST_DONE();
}
