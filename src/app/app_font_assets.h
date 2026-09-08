#ifndef APP_APP_FONT_ASSETS_H
#define APP_APP_FONT_ASSETS_H

/*
 * Locale -> subset font asset mapping, shared by the app (app_fonts.c) and
 * tests/font_locale_test.c so the test exercises the real selector instead
 * of a hand copy. Dependency-free on purpose: keep it that way.
 */

#include <string.h>

#define FONT_LATIN "assets/fonts/subset/NotoSans-App-Regular.ttf"
#define FONT_SC    "assets/fonts/subset/NotoSansSC-App-Regular.otf"
#define FONT_JP    "assets/fonts/subset/NotoSansJP-App-Regular.otf"
#define FONT_KR    "assets/fonts/subset/NotoSansKR-App-Regular.otf"
#define FONT_TC    "assets/fonts/subset/NotoSansTC-App-Regular.otf"

static inline const char *
ui_font_asset_for_locale(const char *code)
{
    if(code != NULL) {
        if(strcmp(code, "zh") == 0)
            return FONT_SC;
        if(strcmp(code, "ja") == 0)
            return FONT_JP;
        if(strcmp(code, "ko") == 0)
            return FONT_KR;
        if(strcmp(code, "zh-TW") == 0 || strcmp(code, "zh_Hant") == 0)
            return FONT_TC;
    }
    return FONT_LATIN;
}

#endif /* APP_APP_FONT_ASSETS_H */
