#include "settings_about.h"

#include "app.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_ui.h"
#include "version.h"
#include "raylib.h"

#include <stdio.h>

static int
about_link_icon_columns(int content_w)
{
    int link_count = 5;
    int icon_size = flint_px(32);
    int icon_padding = flint_px(4);
    int icon_spacing = flint_px(20);
    int icon_btn_w = icon_size + icon_padding * 2;
    int max_columns = link_count;
    int min_columns = content_w >= flint_px(240) ? 3 : 2;

    for(int columns = max_columns; columns >= min_columns; columns--) {
        int total_w = icon_btn_w * columns + icon_spacing * (columns - 1);
        if(total_w <= content_w)
            return columns;
    }

    return min_columns;
}

static int
about_link_icons_height(int content_w)
{
    int link_count = 5;
    int icon_size = flint_px(32);
    int icon_padding = flint_px(4);
    int row_spacing = flint_px(16);
    int icon_btn_w = icon_size + icon_padding * 2;
    int columns = about_link_icon_columns(content_w);
    int rows = (link_count + columns - 1) / columns;

    return rows * icon_btn_w + (rows - 1) * row_spacing;
}

int
settings_about_content_height(int content_w)
{
    FlintUIParagraph description = {
        .text = locale_get("about_text"),
        .width = content_w,
        .font = flint_ui_font(),
        .line_gap = flint_px(4),
        .color = flint_darken(flint_theme_get_text(), 35)
    };
    return flint_px(194) + flint_ui_paragraph_height(description) +
           about_link_icons_height(content_w);
}

static void
about_draw_link_icons(InbeApp *app, int content_x, int content_w, int *y)
{
    int link_count = 5;
    int icon_size = flint_px(32);
    int icon_padding = flint_px(4);
    int icon_spacing = flint_px(20);
    int icon_btn_w = icon_size + icon_padding * 2;
    int columns = about_link_icon_columns(content_w);
    int grid_w = icon_btn_w * columns + icon_spacing * (columns - 1);
    int links_start_x = content_x + (content_w - grid_w) / 2;
    int row_spacing = flint_px(16);
    Texture2D icons[5] = {
        app->icons[UI_ICON_TYPE_DISCORD],
        app->icons[UI_ICON_TYPE_TELEGRAM],
        app->icons[UI_ICON_TYPE_GITHUB],
        app->icons[UI_ICON_TYPE_BTC],
        app->icons[UI_ICON_TYPE_MONERO]
    };
    const char *urls[5] = {
        "https://discord.com/invite/JbGZ4yENDt",
        "https://t.me/lotusinbe",
        "https://github.com/waozixyz/inbe",
        "https://trocador.app/en/anonpay/?ticker_to=btc&network_to=Mainnet&address=bc1qxzcetg50f6epgddc09n82xqn3zswlmk44235y5&donation=True&simple_mode=True&amount=0.001&name=Inner+Breeze&email=waotzi@proton.me&ticker_from=btc&network_from=Mainnet&buttonbgcolor=445588&textcolor=ffffff&bgcolor=eaeaffff",
        "https://trocador.app/en/anonpay/?ticker_to=xmr&network_to=Mainnet&address=86CbC3d4a2GhT9auh6X99JhmhTMFKVVk8Q9cLrKTHkBu8LLkoNWgkBeAT3YZrvDM6NczYe8brUJNsTiFmwpWDZYnFG5kzSH&donation=True&simple_mode=True&amount=0.1&name=Inner+Breeze&email=waotzi@proton.me&ticker_from=xmr&network_from=Mainnet&buttonbgcolor=445588&textcolor=ffffff&bgcolor=eaeaffff"
    };

    for(int i = 0; i < link_count; i++) {
        int col = i % columns;
        int row = i / columns;
        int icon_x = links_start_x + col * (icon_btn_w + icon_spacing) + icon_padding;
        int icon_y = *y + row * (icon_btn_w + row_spacing);
        ui_draw_icon_link(icon_x, icon_y, icon_size, icons[i], urls[i]);
    }
    *y += about_link_icons_height(content_w);
}

void
settings_about_draw(InbeApp *app, int x, int w, int *y)
{
    char version_text[32];
    int title_font = flint_ui_title_font(locale_get("app_title"), w);
    int font = flint_ui_font();
    int small_font = flint_ui_font_small();
    int logo_size = flint_px(56);
    int text_w;
    Color muted = flint_darken(flint_theme_get_text(), 35);
    FlintUIParagraph description;

    *y += flint_px(16);
    ui_draw_icon_texture(x + (w - logo_size) / 2, *y, logo_size,
                         app->icons[UI_ICON_TYPE_INBE], WHITE);
    *y += logo_size + flint_px(12);

    text_w = flint_text_measure(locale_get("app_title"), title_font);
    flint_text_draw(locale_get("app_title"), x + (w - text_w) / 2, *y, title_font,
                    flint_theme_get_text());
    *y += flint_px(34);

    snprintf(version_text, sizeof(version_text), "v%s", INBE_VERSION_STRING);
    text_w = flint_text_measure(version_text, small_font);
    flint_text_draw(version_text, x + (w - text_w) / 2, *y, small_font, muted);
    *y += flint_px(28);

    description = (FlintUIParagraph){
        .text = locale_get("about_text"),
        .width = w,
        .font = font,
        .line_gap = flint_px(4),
        .color = muted
    };
    flint_ui_paragraph_draw(description, x, y);
    *y += flint_px(18);

    about_draw_link_icons(app, x, w, y);
    *y += flint_px(32);
}
