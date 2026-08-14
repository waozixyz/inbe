#include "app.h"
#include "locale.h"
#include "app_internal.h"
#include "app_font_assets.h"
#include <stdlib.h>
#include <string.h>

int
app_running_in_kryon_preview(void)
{
    const char *inspect = getenv("KRYON_INSPECT");

    return IsUIInspectActive() ||
           (inspect != NULL && inspect[0] != '\0' && strcmp(inspect, "0") != 0);
}

/* Register a UI font from an embedded asset. Kryon seeds the standard
 * codepoint range (ASCII, Latin and Latin Extended, punctuation, currency,
 * Greek, Cyrillic) and grows the atlas on demand, so the font renders every
 * glyph its file contains in its own typeface instead of falling back to a
 * different registered font. */
static int
register_ui_font_source(const char *name, const char *path)
{
    const EmbeddedAsset *asset;

    if(name == NULL || path == NULL)
        return 0;

    asset = GetEmbeddedAsset(path);
    if(asset == NULL)
        return 0;

    return RegisterUIFontSource(name, GetEmbeddedAssetExtension(path),
                                asset->data, asset->size,
                                NULL, 0);
}

#if ANDROID_BUILD
/* Android system fonts act as cross-font fallback for scripts the bundled
 * subset lacks. They are dynamic source fonts: the atlas grows as glyphs are
 * needed. (.ttc collections are omitted -- raylib cannot load them.) */
static void
register_android_system_font_fallbacks(void)
{
    static const struct {
        const char *name;
        const char *path;
    } fonts[] = {
        {"sys-sc", "/system/fonts/NotoSansSC-Regular.otf"},
        {"sys-jp", "/system/fonts/NotoSansJP-Regular.otf"},
        {"sys-kr", "/system/fonts/NotoSansKR-Regular.otf"},
        {"sys-tc", "/system/fonts/NotoSansTC-Regular.otf"},
        {"sys-latin", "/system/fonts/NotoSans-Regular.ttf"},
        {"sys-roboto", "/system/fonts/Roboto-Regular.ttf"}
    };

    for(size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++)
        (void)RegisterUIFontFileSource(fonts[i].name, fonts[i].path, NULL, 0);
}
#endif

/* The language picker lists every locale in its native script, so CJK glyphs
 * must render even when the active UI font is Latin. Register the CJK subset
 * fonts as cross-font fallback; they grow on demand as their glyphs are drawn. */
static void
register_language_picker_fonts(void)
{
    (void)register_ui_font_source("ui-lang-latin", INBE_FONT_LATIN);
    (void)register_ui_font_source("ui-lang-ja", INBE_FONT_JP);
    (void)register_ui_font_source("ui-lang-ko", INBE_FONT_KR);
    (void)register_ui_font_source("ui-lang-zh", INBE_FONT_SC);
}

int
load_locale_font(InbeApp *app)
{
    Image white;
    const char *code;
    const char *font_path;
    const EmbeddedAsset *font_asset;
    int ok = 0;

    if(app == NULL)
        return 0;

    code = GetCurrentLocaleCode();
    if(code == NULL || code[0] == '\0')
        code = "en";
    font_path = ui_font_asset_for_locale(code);
    font_asset = GetEmbeddedAsset(font_path);
    if(font_asset == NULL)
        return 0;

    if(!IsUIInspectActive())
        ClearUIFonts();
    if(!RegisterUIFontSource("ui", GetEmbeddedAssetExtension(font_path),
                             font_asset->data, font_asset->size,
                             NULL, 0))
        goto done;
#if ANDROID_BUILD
    register_android_system_font_fallbacks();
#endif
    register_language_picker_fonts();
    if(!UseUIFont("ui"))
        goto done;

    white = GenImageColor(1, 1, WHITE);
    app->font_shapes_texture = LoadTextureFromImage(white);
    UnloadImage(white);
    if(app->font_shapes_texture.id == 0)
        goto done;
    SetTextureFilter(app->font_shapes_texture, TEXTURE_FILTER_POINT);

    SetShapesTexture(app->font_shapes_texture, (Rectangle){0, 0, 1, 1});
    ok = 1;

done:
    if(!ok && !IsUIInspectActive())
        ClearUIFonts();
    return ok;
}

void
unload_locale_font(InbeApp *app)
{
    if(app == NULL)
        return;
    if(!IsUIInspectActive())
        ClearUIFonts();
}

void
discard_locale_font_cpu(InbeApp *app)
{
    if(app == NULL)
        return;
    if(!IsUIInspectActive())
        ClearUIFonts();
}
