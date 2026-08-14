/*
 * Locale -> font asset mapping + font file presence.
 *
 * Links the REAL selector (ui_font_asset_for_locale from
 * src/app/app_font_assets.h) instead of a hand copy, so a change in the app
 * is actually what gets tested. Glyph coverage of the files themselves is
 * asserted separately by font_glyph_coverage_test.
 */

#include "../src/app/app_font_assets.h"

#include <stdio.h>
#include <string.h>

#ifndef KRYON_DIR
#define KRYON_DIR "vendor/kryon"
#endif

typedef struct LocaleFontCase {
    const char *locale;
    const char *font;
} LocaleFontCase;

static int
file_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");

    if(fp == NULL)
        return 0;
    fclose(fp);
    return 1;
}

int
main(void)
{
    static const LocaleFontCase cases[] = {
        {"en",      INBE_FONT_LATIN},
        {"cs",      INBE_FONT_LATIN},
        {"de",      INBE_FONT_LATIN},
        {"es",      INBE_FONT_LATIN},
        {"fr",      INBE_FONT_LATIN},
        {"id",      INBE_FONT_LATIN},
        {"it",      INBE_FONT_LATIN},
        {"pt",      INBE_FONT_LATIN},
        {"ru",      INBE_FONT_LATIN},
        {"zh",      INBE_FONT_SC},
        {"zh-TW",   INBE_FONT_TC},
        {"zh_Hant", INBE_FONT_TC},
        {"ja",      INBE_FONT_JP},
        {"ko",      INBE_FONT_KR},
        {NULL,      INBE_FONT_LATIN}
    };
    int failures = 0;

    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const char *actual = ui_font_asset_for_locale(cases[i].locale);

        if(actual == NULL || strcmp(actual, cases[i].font) != 0) {
            fprintf(stderr, "FAIL locale %s mapped to %s, expected %s\n",
                    cases[i].locale ? cases[i].locale : "(null)",
                    actual ? actual : "(null)", cases[i].font);
            failures++;
        }
        if(!file_exists(cases[i].font)) {
            fprintf(stderr, "FAIL missing font file %s\n", cases[i].font);
            failures++;
        }
    }

    if(failures != 0) {
        fprintf(stderr, "%d locale font failure(s)\n", failures);
        return 1;
    }

    printf("font locale tests passed\n");
    return 0;
}
