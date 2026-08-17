/*
 * font_glyph_coverage_test.c
 *
 * Asserts the shipped subset font files actually contain glyphs for the
 * characters the UI renders and users can type. Fonts were previously only
 * checked as files on disk (existence), never as glyph coverage -- which is
 * how a subset regeneration that dropped ae oe ue ss / e' n~ c~ shipped
 * silently and typed text fell back to a broken font.
 *
 * Invariants:
 *  1. The Latin subset font covers every non-ASCII codepoint of the
 *     Latin/Cyrillic locale files plus assets/fonts/input_common.txt (the
 *     typed-input charset). Those locales all render with the Latin font
 *     (see ui_font_asset_for_locale in src/app/app_fonts.c).
 *  2. Every non-ASCII codepoint of the CJK locale files and the language
 *     index is covered by at least one shipped font, so the language picker
 *     and the cross-font fallback can render it.
 *
 * Pure C: parses the sfnt cmap tables (format 4 BMP + format 12) directly,
 * no GPU and no kryon dependency. Run from the repository root.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_LATIN "assets/fonts/subset/NotoSans-Inbe-Regular.ttf"
#define FONT_SC    "assets/fonts/subset/NotoSansSC-Inbe-Regular.otf"
#define FONT_JP    "assets/fonts/subset/NotoSansJP-Inbe-Regular.otf"
#define FONT_KR    "assets/fonts/subset/NotoSansKR-Inbe-Regular.otf"
#define FONT_TC    "assets/fonts/subset/NotoSansTC-Inbe-Regular.otf"

#define MAX_CODEPOINTS 4096

typedef struct {
    unsigned int values[MAX_CODEPOINTS];
    size_t count;
} CodepointSet;

static void
cpset_add(CodepointSet *set, unsigned int cp)
{
    if(cp <= 0x7F || cp == 0xFEFF)
        return; /* ASCII is always covered; skip the BOM */
    for(size_t i = 0; i < set->count; i++) {
        if(set->values[i] == cp)
            return;
    }
    if(set->count >= MAX_CODEPOINTS) {
        fprintf(stderr, "font_glyph_coverage_test: codepoint set overflow\n");
        exit(1);
    }
    set->values[set->count++] = cp;
}

static void
cpset_add_utf8(CodepointSet *set, const unsigned char *text, size_t size)
{
    size_t i = 0;

    while(i < size) {
        unsigned int cp;
        unsigned char c = text[i];

        if(c < 0x80) {
            cp = c;
            i++;
        } else if((c & 0xE0) == 0xC0 && i + 1 < size) {
            cp = ((unsigned)(c & 0x1F) << 6) | (text[i + 1] & 0x3F);
            i += 2;
        } else if((c & 0xF0) == 0xE0 && i + 2 < size) {
            cp = ((unsigned)(c & 0x0F) << 12) |
                 ((unsigned)(text[i + 1] & 0x3F) << 6) |
                 (text[i + 2] & 0x3F);
            i += 3;
        } else if((c & 0xF8) == 0xF0 && i + 3 < size) {
            cp = ((unsigned)(c & 0x07) << 18) |
                 ((unsigned)(text[i + 1] & 0x3F) << 12) |
                 ((unsigned)(text[i + 2] & 0x3F) << 6) |
                 (text[i + 3] & 0x3F);
            i += 4;
        } else {
            i++;
            continue;
        }
        cpset_add(set, cp);
    }
}

static void
cpset_add_file(CodepointSet *set, const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char buf[8192];
    size_t n;

    if(f == NULL) {
        fprintf(stderr, "font_glyph_coverage_test: cannot open %s\n", path);
        exit(1);
    }
    while((n = fread(buf, 1, sizeof(buf), f)) > 0)
        cpset_add_utf8(set, buf, n);
    fclose(f);
}

/* --- sfnt cmap parsing ------------------------------------------------- */

static unsigned int
rd_u16(const unsigned char *p)
{
    return ((unsigned)p[0] << 8) | p[1];
}

static unsigned int
rd_u32(const unsigned char *p)
{
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) |
           ((unsigned)p[2] << 8) | p[3];
}

typedef struct {
    const unsigned char *fmt4;  /* BMP segment subtable, or NULL */
    const unsigned char *fmt12; /* UCS-4 grouped subtable, or NULL */
} CmapView;

typedef struct {
    unsigned char *data;
    long size;
    CmapView cmap;
} FontFile;

static void
font_close(FontFile *font)
{
    free(font->data);
    font->data = NULL;
    font->size = 0;
    memset(&font->cmap, 0, sizeof(font->cmap));
}

static int
font_open(FontFile *font, const char *path)
{
    FILE *f;
    long size;
    unsigned int num_tables, i, cmap_off = 0, cmap_len = 0;

    memset(font, 0, sizeof(*font));
    f = fopen(path, "rb");
    if(f == NULL)
        return 0;
    if(fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 ||
       fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    font->data = malloc((size_t)size);
    if(font->data == NULL || fread(font->data, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        font_close(font);
        return 0;
    }
    fclose(f);
    font->size = size;

    /* sfnt header: u32 flavor, u16 numTables, u16 searchRange, ... then
     * 16-byte table records (tag, checksum, offset, length). */
    if(size < 12)
        goto fail;
    num_tables = rd_u16(font->data + 4);
    for(i = 0; i < num_tables; i++) {
        long rec = 12 + (long)i * 16;
        if(rec + 16 > size)
            goto fail;
        if(memcmp(font->data + rec, "cmap", 4) == 0) {
            cmap_off = rd_u32(font->data + rec + 8);
            cmap_len = rd_u32(font->data + rec + 12);
            break;
        }
    }
    if(cmap_off == 0 || cmap_len < 4 || (long)cmap_off + (long)cmap_len > size)
        goto fail;

    /* cmap table: u16 version, u16 numTables, then 8-byte encoding records
     * (platformID, encodingID, subtable offset). Keep the best Unicode
     * BMP (format 4) and UCS-4 (format 12) subtables. */
    {
        unsigned int n = rd_u16(font->data + cmap_off + 2);
        unsigned int fmt4_score = 0, fmt12_score = 0;

        for(i = 0; i < n; i++) {
            long rec = (long)cmap_off + 4 + (long)i * 8;
            unsigned int platform, encoding, sub_off, format, score;
            long sub;

            if(rec + 8 > size)
                continue;
            platform = rd_u16(font->data + rec);
            encoding = rd_u16(font->data + rec + 2);
            sub_off = rd_u32(font->data + rec + 4);
            sub = (long)cmap_off + sub_off;
            if(sub + 2 > size)
                continue;
            format = rd_u16(font->data + sub);

            /* Unicode platform (0) or Windows Unicode (3,1/3,10). */
            if(platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10)))
                score = (format == 12) ? 2 : 1;
            else
                continue;

            if(format == 4 && score > fmt4_score) {
                font->cmap.fmt4 = font->data + sub;
                fmt4_score = score;
            } else if(format == 12 && score > fmt12_score) {
                font->cmap.fmt12 = font->data + sub;
                fmt12_score = score;
            }
        }
    }
    if(font->cmap.fmt4 == NULL && font->cmap.fmt12 == NULL)
        goto fail;
    return 1;

fail:
    font_close(font);
    return 0;
}

static int
cmap4_has(const unsigned char *t, unsigned int cp)
{
    unsigned int seg_count = rd_u16(t + 6) / 2;
    const unsigned char *end_code = t + 14;
    const unsigned char *start_code = t + 16 + (size_t)seg_count * 2;
    const unsigned char *id_delta = t + 16 + (size_t)seg_count * 4;
    const unsigned char *id_range_offset = t + 16 + (size_t)seg_count * 6;

    for(unsigned int seg = 0; seg < seg_count; seg++) {
        unsigned int start = rd_u16(start_code + (size_t)seg * 2);
        unsigned int end = rd_u16(end_code + (size_t)seg * 2);

        if(cp < start || cp > end)
            continue;
        if(end == 0xFFFF)
            return 0; /* sentinel segment maps nothing */
        {
            unsigned int ro = rd_u16(id_range_offset + (size_t)seg * 2);
            unsigned int glyph;

            if(ro == 0) {
                glyph = (cp + rd_u16(id_delta + (size_t)seg * 2)) & 0xFFFF;
            } else {
                const unsigned char *p = id_range_offset + (size_t)seg * 2 + ro +
                                         (size_t)(cp - start) * 2;
                /* p must stay inside the table; subsets are small so the
                 * segment bounds above already keep this in range. */
                glyph = rd_u16(p);
                if(glyph != 0)
                    glyph = (glyph + rd_u16(id_delta + (size_t)seg * 2)) & 0xFFFF;
            }
            return glyph != 0;
        }
    }
    return 0;
}

static int
cmap12_has(const unsigned char *t, unsigned int cp)
{
    unsigned int n_groups = rd_u32(t + 12);
    const unsigned char *groups = t + 16;

    for(unsigned int g = 0; g < n_groups; g++) {
        const unsigned char *rec = groups + (size_t)g * 12;
        unsigned int start = rd_u32(rec);
        unsigned int end = rd_u32(rec + 4);

        if(cp >= start && cp <= end)
            return 1;
    }
    return 0;
}

static int
font_has_glyph(const FontFile *font, unsigned int cp)
{
    if(font->cmap.fmt12 != NULL && cmap12_has(font->cmap.fmt12, cp))
        return 1;
    if(font->cmap.fmt4 != NULL && cmap4_has(font->cmap.fmt4, cp))
        return 1;
    return 0;
}

/* --- the test ----------------------------------------------------------- */

/* Codepoints rendered by the CJK subset fonts, never the Latin font: the
 * language index lists every locale in its native endonym (日本語, 한국어,
 * 中文...), so the corpus mixes scripts. The Latin-font check skips these;
 * invariant 2 covers them across the shipped fonts. */
static int
cp_is_cjk(unsigned int cp)
{
    return (cp >= 0x2E80 && cp <= 0x303E) ||  /* CJK radicals, punctuation */
           (cp >= 0x3040 && cp <= 0x30FF) ||  /* kana */
           (cp >= 0x3130 && cp <= 0x318F) ||  /* hangul compatibility jamo */
           (cp >= 0x3400 && cp <= 0x4DBF) ||  /* CJK unified ext A */
           (cp >= 0x4E00 && cp <= 0x9FFF) ||  /* CJK unified */
           (cp >= 0xAC00 && cp <= 0xD7AF) ||  /* hangul syllables */
           (cp >= 0xF900 && cp <= 0xFAFF) ||  /* CJK compat ideographs */
           (cp >= 0xFF00 && cp <= 0xFFEF);    /* fullwidth forms */
}

static const char *latin_locales[] = {
    "en", "es", "cs", "de", "fr", "id", "it", "pt", "ru"
};
static const char *cjk_locales[] = { "ja", "ko", "zh" };

int
main(int argc, char **argv)
{
    static const char *all_fonts[] = { FONT_LATIN, FONT_SC, FONT_JP, FONT_KR, FONT_TC };

    /* Debug probe: font_glyph_coverage_test --probe <font> <cphex>...
     * prints whether each codepoint has a glyph. */
    if(argc >= 3 && strcmp(argv[1], "--probe") == 0) {
        FontFile font;

        if(!font_open(&font, argv[2])) {
            fprintf(stderr, "cannot parse %s\n", argv[2]);
            return 1;
        }
        for(int i = 3; i < argc; i++) {
            unsigned int cp = (unsigned int)strtoul(argv[i], NULL, 16);

            printf("U+%04X %s\n", cp, font_has_glyph(&font, cp) ? "yes" : "no");
        }
        font_close(&font);
        return 0;
    }
    CodepointSet latin_set = {0}, cjk_set = {0};
    FontFile latin;
    int failures = 0;
    char path[64];

    /* 1) The Latin font must cover every Latin/Cyrillic locale char and the
     *    whole typed-input charset -- these are the scripts that render with
     *    it as the active UI font. */
    cpset_add_file(&latin_set, "assets/fonts/input_common.txt");
    cpset_add_file(&latin_set, "locales/index.txt");
    for(size_t i = 0; i < sizeof(latin_locales) / sizeof(latin_locales[0]); i++) {
        snprintf(path, sizeof(path), "locales/%s.txt", latin_locales[i]);
        cpset_add_file(&latin_set, path);
    }
    if(!font_open(&latin, FONT_LATIN)) {
        fprintf(stderr, "FAIL: cannot parse cmap of %s\n", FONT_LATIN);
        return 1;
    }
    for(size_t i = 0; i < latin_set.count; i++) {
        unsigned int cp = latin_set.values[i];

        if(cp_is_cjk(cp))
            continue; /* CJK scripts render via the CJK subset fonts */
        if(!font_has_glyph(&latin, cp)) {
            printf("FAIL: U+%04X has no glyph in %s (Latin font)\n", cp, FONT_LATIN);
            failures++;
        }
    }
    font_close(&latin);
    printf("Latin font: %zu required codepoints, %d missing\n",
           latin_set.count, failures);

    /* 2) Every CJK locale char (plus the index endonyms) must exist in at
     *    least one shipped font so the picker and cross-font fallback can
     *    render it. */
    cpset_add_file(&cjk_set, "locales/index.txt");
    for(size_t i = 0; i < sizeof(cjk_locales) / sizeof(cjk_locales[0]); i++) {
        snprintf(path, sizeof(path), "locales/%s.txt", cjk_locales[i]);
        cpset_add_file(&cjk_set, path);
    }
    {
        FontFile fonts[5];
        int opened = 0;

        for(size_t f = 0; f < 5; f++) {
            if(font_open(&fonts[f], all_fonts[f]))
                opened++;
        }
        if(opened == 0) {
            fprintf(stderr, "FAIL: cannot parse any shipped font cmap\n");
            return 1;
        }
        for(size_t i = 0; i < cjk_set.count; i++) {
            unsigned int cp = cjk_set.values[i];
            int covered = 0;

            for(size_t f = 0; f < 5 && !covered; f++) {
                if(fonts[f].data != NULL && font_has_glyph(&fonts[f], cp))
                    covered = 1;
            }
            if(!covered) {
                printf("FAIL: U+%04X has no glyph in ANY shipped font\n", cp);
                failures++;
            }
        }
        for(size_t f = 0; f < 5; f++)
            font_close(&fonts[f]);
        printf("CJK/picker: %zu required codepoints checked against %d fonts\n",
               cjk_set.count, opened);
    }

    if(failures > 0) {
        printf("font_glyph_coverage tests FAILED (%d missing glyphs)\n", failures);
        return 1;
    }
    printf("font_glyph_coverage tests passed\n");
    return 0;
}
