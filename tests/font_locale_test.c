#include <stdio.h>
#include <string.h>

#ifndef FLINT_DIR
#define FLINT_DIR "vendor/flint"
#endif

typedef struct LocaleFontCase {
    const char *locale;
    const char *font;
} LocaleFontCase;

static const char *
font_for_locale(const char *locale)
{
    if(locale != NULL) {
        if(strcmp(locale, "zh") == 0)
            return FLINT_DIR "/fonts/noto/NotoSansSC-Regular.otf";
        if(strcmp(locale, "ja") == 0)
            return FLINT_DIR "/fonts/noto/NotoSansJP-Regular.otf";
        if(strcmp(locale, "ko") == 0)
            return FLINT_DIR "/fonts/noto/NotoSansKR-Regular.otf";
        if(strcmp(locale, "zh-TW") == 0 || strcmp(locale, "zh_Hant") == 0)
            return FLINT_DIR "/fonts/noto/NotoSansTC-Regular.otf";
    }
    return FLINT_DIR "/fonts/noto/NotoSans-Regular.ttf";
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
        {"en", FLINT_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"cs", FLINT_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"de", FLINT_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"es", FLINT_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"fr", FLINT_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"id", FLINT_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"it", FLINT_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"pt", FLINT_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"ru", FLINT_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"zh", FLINT_DIR "/fonts/noto/NotoSansSC-Regular.otf"},
        {"ja", FLINT_DIR "/fonts/noto/NotoSansJP-Regular.otf"},
        {"ko", FLINT_DIR "/fonts/noto/NotoSansKR-Regular.otf"}
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
