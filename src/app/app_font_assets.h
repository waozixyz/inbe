#ifndef INBE_APP_FONT_ASSETS_H
#define INBE_APP_FONT_ASSETS_H

/*
 * Locale -> subset font asset mapping, shared by the app (app_fonts.c) and
 * tests/font_locale_test.c so the test exercises the real selector instead
 * of a hand copy. Dependency-free on purpose: keep it that way.
 */

#include <string.h>

#define INBE_FONT_LATIN "assets/fonts/subset/NotoSans-Inbe-Regular.ttf"
#define INBE_FONT_SC    "assets/fonts/subset/NotoSansSC-Inbe-Regular.otf"
#define INBE_FONT_JP    "assets/fonts/subset/NotoSansJP-Inbe-Regular.otf"
#define INBE_FONT_KR    "assets/fonts/subset/NotoSansKR-Inbe-Regular.otf"
#define INBE_FONT_TC    "assets/fonts/subset/NotoSansTC-Inbe-Regular.otf"

static inline const char *
ui_font_asset_for_locale(const char *code)
{
    if(code != NULL) {
        if(strcmp(code, "zh") == 0)
            return INBE_FONT_SC;
        if(strcmp(code, "ja") == 0)
            return INBE_FONT_JP;
        if(strcmp(code, "ko") == 0)
            return INBE_FONT_KR;
        if(strcmp(code, "zh-TW") == 0 || strcmp(code, "zh_Hant") == 0)
            return INBE_FONT_TC;
    }
    return INBE_FONT_LATIN;
}

#endif /* INBE_APP_FONT_ASSETS_H */
