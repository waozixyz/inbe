#include <stdio.h>
#include <string.h>

#ifndef KRYON_DIR
#define KRYON_DIR "vendor/kryon"
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
            return KRYON_DIR "/fonts/noto/NotoSansSC-Regular.otf";
        if(strcmp(locale, "ja") == 0)
            return KRYON_DIR "/fonts/noto/NotoSansJP-Regular.otf";
        if(strcmp(locale, "ko") == 0)
            return KRYON_DIR "/fonts/noto/NotoSansKR-Regular.otf";
        if(strcmp(locale, "zh-TW") == 0 || strcmp(locale, "zh_Hant") == 0)
            return KRYON_DIR "/fonts/noto/NotoSansTC-Regular.otf";
    }
    return KRYON_DIR "/fonts/noto/NotoSans-Regular.ttf";
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
        {"en", KRYON_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"cs", KRYON_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"de", KRYON_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"es", KRYON_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"fr", KRYON_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"id", KRYON_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"it", KRYON_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"pt", KRYON_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"ru", KRYON_DIR "/fonts/noto/NotoSans-Regular.ttf"},
        {"zh", KRYON_DIR "/fonts/noto/NotoSansSC-Regular.otf"},
        {"ja", KRYON_DIR "/fonts/noto/NotoSansJP-Regular.otf"},
        {"ko", KRYON_DIR "/fonts/noto/NotoSansKR-Regular.otf"}
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
