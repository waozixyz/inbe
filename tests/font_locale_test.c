#include <stdio.h>
#include <string.h>

#ifndef KRYON_DIR
#define KRYON_DIR "vendor/kryon"
#endif

#define FONT_SUBSET_DIR "assets/fonts/subset"

typedef struct LocaleFontCase {
    const char *locale;
    const char *font;
} LocaleFontCase;

static const char *
font_for_locale(const char *locale)
{
    if(locale != NULL) {
        if(strcmp(locale, "zh") == 0)
            return FONT_SUBSET_DIR "/NotoSansSC-Inbe-Regular.otf";
        if(strcmp(locale, "ja") == 0)
            return FONT_SUBSET_DIR "/NotoSansJP-Inbe-Regular.otf";
        if(strcmp(locale, "ko") == 0)
            return FONT_SUBSET_DIR "/NotoSansKR-Inbe-Regular.otf";
        if(strcmp(locale, "zh-TW") == 0 || strcmp(locale, "zh_Hant") == 0)
            return FONT_SUBSET_DIR "/NotoSansTC-Inbe-Regular.otf";
    }
    return FONT_SUBSET_DIR "/NotoSans-Inbe-Regular.ttf";
}

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
        {"en", FONT_SUBSET_DIR "/NotoSans-Inbe-Regular.ttf"},
        {"cs", FONT_SUBSET_DIR "/NotoSans-Inbe-Regular.ttf"},
        {"de", FONT_SUBSET_DIR "/NotoSans-Inbe-Regular.ttf"},
        {"es", FONT_SUBSET_DIR "/NotoSans-Inbe-Regular.ttf"},
        {"fr", FONT_SUBSET_DIR "/NotoSans-Inbe-Regular.ttf"},
        {"id", FONT_SUBSET_DIR "/NotoSans-Inbe-Regular.ttf"},
        {"it", FONT_SUBSET_DIR "/NotoSans-Inbe-Regular.ttf"},
        {"pt", FONT_SUBSET_DIR "/NotoSans-Inbe-Regular.ttf"},
        {"ru", FONT_SUBSET_DIR "/NotoSans-Inbe-Regular.ttf"},
        {"zh", FONT_SUBSET_DIR "/NotoSansSC-Inbe-Regular.otf"},
        {"ja", FONT_SUBSET_DIR "/NotoSansJP-Inbe-Regular.otf"},
        {"ko", FONT_SUBSET_DIR "/NotoSansKR-Inbe-Regular.otf"}
    };
    int failures = 0;

    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const char *actual = font_for_locale(cases[i].locale);

        if(strcmp(actual, cases[i].font) != 0) {
            fprintf(stderr, "FAIL locale %s mapped to %s, expected %s\n",
                    cases[i].locale, actual, cases[i].font);
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
