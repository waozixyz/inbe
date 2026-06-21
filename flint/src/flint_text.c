#include "flint_text.h"
#include "flint_clip.h"
#include "flint_scaling.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

static Font g_flint_text_font = {0};
static Font g_flint_text_small_font = {0};

static int
font_valid(Font font)
{
    return font.texture.id != 0 && font.glyphs != NULL && font.recs != NULL &&
           font.glyphCount > 0 && font.baseSize > 0;
}

typedef struct FlintChoppedGlyph {
    int32_t value;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t offsetX;
    int32_t offsetY;
    int32_t advanceX;
} FlintChoppedGlyph;

static Font
active_font(void)
{
    if(font_valid(g_flint_text_font))
        return g_flint_text_font;

    return GetFontDefault();
}

static Font
active_font_for_size(int font_size)
{
    if(font_size == FLINT_TEXT_8 && font_valid(g_flint_text_small_font))
        return g_flint_text_small_font;

    return active_font();
}

static int
font_integer_scale(int font_size)
{
    int target_size = flint_px(font_size);
    int base_size = FLINT_TEXT_BASE_SIZE;
    int scale;

    if(target_size < base_size)
        target_size = base_size;
    scale = (target_size + base_size / 2) / base_size;
    return scale > 0 ? scale : 1;
}


void
flint_text_set_font(Font font)
{
    g_flint_text_font = font;
}

void
flint_text_set_small_font(Font font)
{
    g_flint_text_small_font = font;
}

Font
flint_text_font(void)
{
    return active_font();
}

Font
flint_text_load_chopped_font_from_memory(const unsigned char *png_data, unsigned int png_size,
                                         const unsigned char *dat_data, unsigned int dat_size,
                                         int base_size)
{
    Font font = {0};
    FlintChoppedGlyph *glyphs = NULL;
    GlyphInfo *glyph_infos = NULL;
    Rectangle *recs = NULL;
    int32_t glyph_count = 0;
    Image image = {0};
    Texture2D texture = {0};
    const unsigned char *cursor;
    size_t glyph_bytes;

    if(png_data == NULL || png_size == 0 || dat_data == NULL || dat_size < sizeof(glyph_count))
        return font;

    cursor = dat_data;
    memcpy(&glyph_count, cursor, sizeof(glyph_count));
    cursor += sizeof(glyph_count);
    if(glyph_count <= 0)
        return font;

    glyph_bytes = (size_t)glyph_count * sizeof(*glyphs);
    if((size_t)dat_size - sizeof(glyph_count) < glyph_bytes)
        return font;

    glyphs = (FlintChoppedGlyph *)calloc((size_t)glyph_count, sizeof(*glyphs));
    glyph_infos = (GlyphInfo *)calloc((size_t)glyph_count, sizeof(*glyph_infos));
    recs = (Rectangle *)calloc((size_t)glyph_count, sizeof(*recs));
    if(glyphs == NULL || glyph_infos == NULL || recs == NULL)
        goto cleanup;

    memcpy(glyphs, cursor, glyph_bytes);

    image = LoadImageFromMemory(".png", png_data, (int)png_size);
    if(image.data == NULL)
        goto cleanup;

#if defined(_WIN32)
    {
        int pot_w = 1;
        int pot_h = 1;
        while(pot_w < image.width)
            pot_w <<= 1;
        while(pot_h < image.height)
            pot_h <<= 1;
        if(pot_w != image.width || pot_h != image.height)
            ImageResizeCanvas(&image, pot_w, pot_h, 0, 0, BLANK);
    }
#endif

    texture = LoadTextureFromImage(image);
    UnloadImage(image);
    image = (Image){0};
    if(texture.id == 0)
        goto cleanup;
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);

    for(int i = 0; i < glyph_count; i++) {
        glyph_infos[i].value = glyphs[i].value;
        glyph_infos[i].offsetX = glyphs[i].offsetX;
        glyph_infos[i].offsetY = glyphs[i].offsetY;
        glyph_infos[i].advanceX = glyphs[i].advanceX;
        glyph_infos[i].image = (Image){0};
        recs[i] = (Rectangle){(float)glyphs[i].x, (float)glyphs[i].y,
                              (float)glyphs[i].w, (float)glyphs[i].h};
    }

    font.texture = texture;
    font.glyphs = glyph_infos;
    font.recs = recs;
    font.glyphCount = glyph_count;
    font.baseSize = base_size > 0 ? base_size : FLINT_TEXT_BASE_SIZE;
    font.glyphPadding = 0;

    free(glyphs);
    return font;

cleanup:
    if(image.data != NULL)
        UnloadImage(image);
    if(texture.id != 0)
        UnloadTexture(texture);
    free(glyphs);
    free(glyph_infos);
    free(recs);
    return (Font){0};
}

Font
flint_text_load_chopped_font(const char *png_path, const char *dat_path, int base_size)
{
    Font font = {0};
    FILE *file = NULL;
    unsigned char *dat_data = NULL;
    unsigned char *png_data = NULL;
    long dat_size;
    int png_size;

    if(png_path == NULL || dat_path == NULL)
        return font;

    file = fopen(dat_path, "rb");
    if(file == NULL)
        return font;

    if(fseek(file, 0, SEEK_END) != 0)
        goto cleanup;
    dat_size = ftell(file);
    if(dat_size <= 0)
        goto cleanup;
    rewind(file);

    dat_data = (unsigned char *)malloc((size_t)dat_size);
    if(dat_data == NULL)
        goto cleanup;
    if(fread(dat_data, 1, (size_t)dat_size, file) != (size_t)dat_size)
        goto cleanup;
    fclose(file);
    file = NULL;

    png_data = LoadFileData(png_path, &png_size);
    if(png_data == NULL || png_size <= 0)
        goto cleanup;

    font = flint_text_load_chopped_font_from_memory(png_data, (unsigned int)png_size,
                                                    dat_data, (unsigned int)dat_size,
                                                    base_size);

cleanup:
    if(file != NULL)
        fclose(file);
    if(png_data != NULL)
        UnloadFileData(png_data);
    free(dat_data);
    return font;
}

void
flint_text_unload_font(Font *font)
{
    if(font == NULL || font->texture.id == 0)
        return;

    UnloadTexture(font->texture);
    free(font->glyphs);
    free(font->recs);
    *font = (Font){0};
}

int
flint_text_measure(const char *text, int font_size)
{
    Font font = active_font_for_size(font_size);

    if(text == NULL || font.texture.id == 0)
        return 0;

    return flint_text_measure_scaled(text, font_integer_scale(font_size));
}

int
flint_text_height(const char *text, int font_size)
{
    Font font = active_font_for_size(font_size);
    float scale;
    float min_top = 0.0f;
    float max_bottom = 0.0f;
    int seen_glyph = 0;

    if(text == NULL || text[0] == '\0' || font.texture.id == 0 || font.baseSize <= 0)
        return font_size;

    scale = (float)font_integer_scale(font_size);
    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;
        if(codepoint != ' ' && codepoint != '\t') {
            int index = GetGlyphIndex(font, codepoint);
            GlyphInfo glyph = font.glyphs[index];
            Rectangle rec = font.recs[index];
            float glyph_top = (float)glyph.offsetY * scale - (float)font.glyphPadding * scale;
            float glyph_bottom = glyph_top + ((float)rec.height + 2.0f * (float)font.glyphPadding) * scale;

            if(!seen_glyph) {
                min_top = glyph_top;
                max_bottom = glyph_bottom;
                seen_glyph = 1;
            } else {
                if(glyph_top < min_top)
                    min_top = glyph_top;
                if(glyph_bottom > max_bottom)
                    max_bottom = glyph_bottom;
            }
        }
        i += codepoint_byte_count;
    }

    if(!seen_glyph)
        return font.baseSize * font_integer_scale(font_size);
    return (int)(max_bottom - min_top + 0.5f);
}

int
flint_text_line_height(int font_size)
{
    Font font = active_font_for_size(font_size);
    int scale = font_integer_scale(font_size);

    return font.baseSize > 0 ? font.baseSize * scale : FLINT_TEXT_BASE_SIZE * scale;
}

int
flint_text_measure_scaled(const char *text, int scale)
{
    Font font = active_font();
    int width = 0;

    if(text == NULL || font.texture.id == 0 || font.glyphs == NULL || font.baseSize <= 0)
        return 0;
    if(scale < 1)
        scale = 1;

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;

        int index = GetGlyphIndex(font, codepoint);
        width += font.glyphs[index].advanceX * scale;
        i += codepoint_byte_count;
    }

    return width;
}

void
flint_text_draw(const char *text, int x, int y, int font_size, Color color)
{
    flint_text_draw_scaled(text, x, y, font_integer_scale(font_size), color);
}

void
flint_text_draw_scaled(const char *text, int x, int y, int scale, Color color)
{
    Font font = active_font();
    int cursor_x = x;

    if(text == NULL || font.texture.id == 0 || font.glyphs == NULL || font.recs == NULL)
        return;
    if(scale < 1)
        scale = 1;

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;

        int index = GetGlyphIndex(font, codepoint);
        GlyphInfo glyph = font.glyphs[index];
        Rectangle src = font.recs[index];

        if(src.width > 0.0f && src.height > 0.0f) {
            Rectangle dst = {
                .x = (float)(cursor_x + glyph.offsetX * scale),
                .y = (float)(y + glyph.offsetY * scale),
                .width = src.width * (float)scale,
                .height = src.height * (float)scale
            };
            DrawTexturePro(font.texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, color);
        }

        cursor_x += glyph.advanceX * scale;
        i += codepoint_byte_count;
    }
}

void
flint_text_draw_centered(const char *text, int center_x, int center_y, int font_size, Color color)
{
    Font font = active_font_for_size(font_size);
    int actual_size = font.baseSize > 0 ? font.baseSize * font_integer_scale(font_size) : font_size;
    int text_w = flint_text_measure(text, font_size);
    int y = flint_text_y(text, center_y - actual_size / 2, actual_size, font_size);

    flint_text_draw(text, center_x - text_w / 2, y, font_size, color);
}

void
flint_text_draw_in_rect(const char *text, Rectangle rect, int font_size, Color color)
{
    const char *value = text != NULL ? text : "";
    int text_w = flint_text_measure(value, font_size);
    int x = (int)(rect.x + (rect.width - (float)text_w) * 0.5f);
    int y = flint_text_y(value, (int)rect.y, (int)rect.height, font_size);
    int clip_guard = 1;

    flint_clip_begin((int)rect.x, (int)rect.y - clip_guard,
                     (int)rect.width, (int)rect.height + clip_guard * 2);
    flint_text_draw(value, x, y, font_size, color);
    flint_clip_end();
}

int
flint_text_y_scaled(const char *text, int box_y, int box_h, int scale)
{
    Font font = active_font();
    int font_size;

    if(scale < 1)
        scale = 1;
    font_size = font.baseSize > 0 ? font.baseSize * scale : 16 * scale;
    return flint_text_y(text, box_y, box_h, font_size);
}

int
flint_text_y(const char *text, int box_y, int box_h, int font_size)
{
    Font font = active_font_for_size(font_size);
    float scale;
    float min_top = 0.0f;
    float max_bottom = 0.0f;
    int seen_glyph = 0;

    if(text == NULL || text[0] == '\0' || font.texture.id == 0 || font.baseSize <= 0)
        return box_y + (int)(((float)box_h - (float)font_size) * 0.5f + 0.5f);

    scale = (float)font_integer_scale(font_size);

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;

        if(codepoint != ' ' && codepoint != '\t') {
            int index = GetGlyphIndex(font, codepoint);
            GlyphInfo glyph = font.glyphs[index];
            Rectangle rec = font.recs[index];
            float glyph_top = (float)glyph.offsetY * scale - (float)font.glyphPadding * scale;
            float glyph_bottom = glyph_top + ((float)rec.height + 2.0f * (float)font.glyphPadding) * scale;

            if(!seen_glyph) {
                min_top = glyph_top;
                max_bottom = glyph_bottom;
                seen_glyph = 1;
            } else {
                if(glyph_top < min_top)
                    min_top = glyph_top;
                if(glyph_bottom > max_bottom)
                    max_bottom = glyph_bottom;
            }
        }

        i += codepoint_byte_count;
    }

    if(!seen_glyph)
        return box_y + (int)(((float)box_h - (float)font_size) * 0.5f + 0.5f);

    return box_y + (int)(((float)box_h - (max_bottom - min_top)) * 0.5f - min_top + 0.5f);
}
