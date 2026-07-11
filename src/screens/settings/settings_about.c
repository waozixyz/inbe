#include "settings_about.h"

#include "app.h"
#include "locale.h"
#include "theme.h"
#include "ui.h"
#include "version.h"
#include "flint.h"

#include <stdio.h>

static int
about_link_icon_columns(int content_w)
{
    int link_count = 5;
    int icon_size = ScaleUIPx(32);
    int icon_padding = ScaleUIPx(4);
    int icon_spacing = ScaleUIPx(20);
    int icon_btn_w = icon_size + icon_padding * 2;
    int max_columns = link_count;
    int min_columns = content_w >= ScaleUIPx(240) ? 3 : 2;

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
    int icon_size = ScaleUIPx(32);
    int icon_padding = ScaleUIPx(4);
    int row_spacing = ScaleUIPx(16);
    int icon_btn_w = icon_size + icon_padding * 2;
    int columns = about_link_icon_columns(content_w);
    int rows = (link_count + columns - 1) / columns;

    return rows * icon_btn_w + (rows - 1) * row_spacing;
}

int
settings_about_content_height(int content_w)
{
    UIParagraph description = {
        .text = GetLocaleText("about_text"),
        .width = content_w,
        .font = GetUIFontSize(),
        .line_gap = ScaleUIPx(4),
        .color = DarkenUIColor(GetThemeText(), 35)
    };
    return ScaleUIPx(194) + GetUIParagraphHeight(description) +
           about_link_icons_height(content_w);
}

static void
about_draw_link_icons(InbeApp *app, int content_x, int content_w, int *y)
{
    int link_count = 5;
    int icon_size = ScaleUIPx(32);
    int icon_padding = ScaleUIPx(4);
    int icon_spacing = ScaleUIPx(20);
    int icon_btn_w = icon_size + icon_padding * 2;
    int columns = about_link_icon_columns(content_w);
    int grid_w = icon_btn_w * columns + icon_spacing * (columns - 1);
    int links_start_x = content_x + (content_w - grid_w) / 2;
    int row_spacing = ScaleUIPx(16);
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
        DrawUIIconLink(icon_x, icon_y, icon_size, icons[i], urls[i]);
    }
    *y += about_link_icons_height(content_w);
}

void
settings_about_draw(InbeApp *app, int x, int w, int *y)
{
    char version_text[32];
    int title_font = GetUITitleFontSize(GetLocaleText("app_title"), w);
    int font = GetUIFontSize();
    int small_font = GetUISmallFontSize();
    int logo_size = ScaleUIPx(56);
    int text_w;
    Color muted = DarkenUIColor(GetThemeText(), 35);
    UIParagraph description;

    *y += ScaleUIPx(16);
    DrawUIIconTexture(x + (w - logo_size) / 2, *y, logo_size,
                         app->icons[UI_ICON_TYPE_INBE], WHITE);
    *y += logo_size + ScaleUIPx(12);

    text_w = MeasureUIText(GetLocaleText("app_title"), title_font);
    DrawUIText(GetLocaleText("app_title"), x + (w - text_w) / 2, *y, title_font,
                    GetThemeText());
    *y += ScaleUIPx(34);

    snprintf(version_text, sizeof(version_text), "v%s", INBE_VERSION_STRING);
    text_w = MeasureUIText(version_text, small_font);
    DrawUIText(version_text, x + (w - text_w) / 2, *y, small_font, muted);
    *y += ScaleUIPx(28);

    description = (UIParagraph){
        .text = GetLocaleText("about_text"),
        .width = w,
        .font = font,
        .line_gap = ScaleUIPx(4),
        .color = muted
    };
    DrawUIParagraph(description, x, y);
    *y += ScaleUIPx(18);

    about_draw_link_icons(app, x, w, y);
    *y += ScaleUIPx(32);
}
