#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct UIChoppedGlyph {
    int32_t value;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t offsetX;
    int32_t offsetY;
    int32_t advanceX;
} UIChoppedGlyph;

typedef struct RequiredGlyph {
    int32_t codepoint;
    const char *label;
} RequiredGlyph;

static int
read_file(const char *path, unsigned char **out_data, size_t *out_size)
{
    FILE *fp;
    long size;
    unsigned char *data;

    fp = fopen(path, "rb");
    if(fp == NULL) {
        fprintf(stderr, "FAIL open %s\n", path);
        return 0;
    }
    if(fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        fprintf(stderr, "FAIL seek %s\n", path);
        return 0;
    }
    size = ftell(fp);
    if(size <= 0) {
        fclose(fp);
        fprintf(stderr, "FAIL empty %s\n", path);
        return 0;
    }
    if(fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        fprintf(stderr, "FAIL rewind %s\n", path);
        return 0;
    }
    data = (unsigned char *)malloc((size_t)size);
    if(data == NULL) {
        fclose(fp);
        fprintf(stderr, "FAIL allocate %ld bytes\n", size);
        return 0;
    }
    if(fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        fprintf(stderr, "FAIL read %s\n", path);
        return 0;
    }
    fclose(fp);
    *out_data = data;
    *out_size = (size_t)size;
    return 1;
}

static int
has_glyph(const UIChoppedGlyph *glyphs, int32_t glyph_count, int32_t codepoint)
{
    for(int32_t i = 0; i < glyph_count; i++) {
        if(glyphs[i].value == codepoint)
            return 1;
    }
    return 0;
}

int
main(void)
{
    static const RequiredGlyph required[] = {
        {0x8BBE, "Chinese 设"},
        {0x5B9A, "Chinese 定"},
        {0x8A2D, "Japanese 設"},
        {0xC124, "Korean 설"},
        {0xC815, "Korean 정"},
        {0x041D, "Cyrillic Н"},
        {0x0430, "Cyrillic а"},
        {0x010D, "Czech č"},
        {0x00E9, "Latin é"},
        {0x00E3, "Latin ã"}
    };
    unsigned char *data = NULL;
    size_t size = 0;
    int32_t glyph_count;
    size_t glyph_bytes;
    const UIChoppedGlyph *glyphs;
    int failures = 0;

    if(!read_file("assets/fonts/locales.dat", &data, &size))
        return 1;
    if(size < sizeof(glyph_count)) {
        free(data);
        fprintf(stderr, "FAIL locales.dat too small\n");
        return 1;
    }
    memcpy(&glyph_count, data, sizeof(glyph_count));
    if(glyph_count <= 0) {
        free(data);
        fprintf(stderr, "FAIL locales.dat has no glyphs\n");
        return 1;
    }
    glyph_bytes = (size_t)glyph_count * sizeof(UIChoppedGlyph);
    if(size - sizeof(glyph_count) < glyph_bytes) {
        free(data);
        fprintf(stderr, "FAIL locales.dat truncated glyph table\n");
        return 1;
    }
    glyphs = (const UIChoppedGlyph *)(const void *)(data + sizeof(glyph_count));

    for(size_t i = 0; i < sizeof(required) / sizeof(required[0]); i++) {
        if(!has_glyph(glyphs, glyph_count, required[i].codepoint)) {
            fprintf(stderr, "FAIL missing %s U+%04X in chopped locale font\n",
                    required[i].label, (unsigned int)required[i].codepoint);
            failures++;
        }
    }

    free(data);
    if(failures != 0) {
        fprintf(stderr, "%d chopped locale font failure(s)\n", failures);
        return 1;
    }
    printf("font locale tests passed\n");
    return 0;
}
