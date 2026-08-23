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
register_ui_font_source(const char *name, const char *path,
                        const int *codepoints, int codepoint_count)
{
    const EmbeddedAsset *asset;

    if(name == NULL || path == NULL)
        return 0;

    asset = GetEmbeddedAsset(path);
    if(asset == NULL) {
#if defined(KRYON_PLATFORM_PLAN9)
        return RegisterUIFontFileSource(name, path, codepoints, codepoint_count);
#else
        return 0;
#endif
    }

    return RegisterUIFontSource(name, GetEmbeddedAssetExtension(path),
                                asset->data, asset->size,
                                codepoints, codepoint_count);
}

#if !defined(PLATFORM_WEB) && !defined(_WIN32) && !ANDROID_BUILD
static int
register_desktop_system_ui_font(void)
{
    char font_path[512];

    if(!GetSystemUIFontFile(font_path, (int)sizeof(font_path)))
        return 0;
    if(!RegisterUIFontFileSource("ui", font_path, NULL, 0)) {
        TraceLog(LOG_WARNING, "INBE: failed to load system UI font: %s",
                 font_path);
        return 0;
    }
    TraceLog(LOG_INFO, "INBE: using system UI font: %s", font_path);
    return 1;
}
#endif

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

/* Fallback fonts only ever draw language names: ASCII plus the no-break
 * space covers their seeded set, and the native-script glyphs grow the
 * raster on demand with one batched re-rasterization per burst. This avoids
 * rasterizing the ~1000-glyph standard seed in every fallback font at
 * startup. */
static const int *
language_picker_seed_codepoints(int *out_count)
{
    static int seed[0x7E - 0x20 + 2];
    static int ready = 0;
    int count = 0;

    if(!ready) {
        for(int cp = 0x20; cp <= 0x7E; cp++)
            seed[count++] = cp;
        seed[count++] = 0x00A0;
        ready = 1;
    }
    if(out_count != NULL)
        *out_count = count;
    return seed;
}

/* The language picker lists every locale in its native script, so CJK glyphs
 * must render even when the active UI font is Latin. Register the CJK subset
 * fonts as cross-font fallback; they grow on demand as their glyphs are drawn. */
static void
register_language_picker_fonts(void)
{
    int seed_count = 0;
    const int *seed = language_picker_seed_codepoints(&seed_count);

    (void)register_ui_font_source("ui-lang-latin", INBE_FONT_LATIN, NULL, 0);
    (void)register_ui_font_source("ui-lang-ja", INBE_FONT_JP, seed, seed_count);
    (void)register_ui_font_source("ui-lang-ko", INBE_FONT_KR, seed, seed_count);
    (void)register_ui_font_source("ui-lang-zh", INBE_FONT_SC, seed, seed_count);
}

int
load_locale_font(InbeApp *app)
{
    Image white;
    const char *code;
    const char *font_path;
    const EmbeddedAsset *font_asset;
    int system_font_active = 0;
    int ok = 0;

    if(app == NULL)
        return 0;

    code = GetCurrentLocaleCode();
    if(code == NULL || code[0] == '\0')
        code = "en";
    font_path = ui_font_asset_for_locale(code);
    font_asset = GetEmbeddedAsset(font_path);
#if !defined(KRYON_PLATFORM_PLAN9)
    if(font_asset == NULL)
        return 0;
#endif

    if(!IsUIInspectActive())
        ClearUIFonts();
#if !defined(PLATFORM_WEB) && !defined(_WIN32) && !ANDROID_BUILD
    system_font_active = register_desktop_system_ui_font();
#endif
    if(system_font_active && font_asset != NULL) {
        (void)RegisterUIFontSource("ui-locale", GetEmbeddedAssetExtension(font_path),
                                   font_asset->data, font_asset->size,
                                   NULL, 0);
    } else {
        if(font_asset != NULL) {
            if(!RegisterUIFontSource("ui", GetEmbeddedAssetExtension(font_path),
                                     font_asset->data, font_asset->size,
                                     NULL, 0))
                goto done;
        } else {
#if defined(KRYON_PLATFORM_PLAN9)
            if(!RegisterUIFontFileSource("ui", font_path, NULL, 0))
                goto done;
#else
            goto done;
#endif
        }
    }
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
