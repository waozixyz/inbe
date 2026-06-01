#include "language_tab.h"

#include "app.h"
#include "locale.h"
#include "ui/ui.h"
#include "raylib.h"

extern int view_width;
extern int view_height;
extern Color c_text, c_bg, c_button;
static const char *g_language_options[64];
static int g_language_option_count = 0;

static void
build_language_options(const char **options, int max_options, int *count)
{
    int total = locale_count();

    if(total > max_options)
        total = max_options;
    if(total < 0)
        total = 0;

    for(int i = 0; i < total; i++)
        options[i] = locale_label_at(i);

    *count = total;
}

int
language_dropdown_button(InbeApp *app, int id, int x, int y, int w, int h, int *selected_index)
{
    if(app == NULL || selected_index == NULL)
        return 0;

    build_language_options(g_language_options, 64, &g_language_option_count);
    if(g_language_option_count <= 0) {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, ui_darken(c_bg, 30), ui_lighten(c_bg, 20));
        DrawText(locale_get("language_label"), x + ui_px(12), y + h / 2 - ui_clamp_px(14, 12, 16) / 2 - 1,
                 ui_clamp_px(14, 12, 16), c_text);
        return 0;
    }

    if(*selected_index < 0 || *selected_index >= g_language_option_count)
        *selected_index = 0;

    return ui_draw_dropdown_button(app, id, x, y, w, h, g_language_options, g_language_option_count, selected_index);
}

int
language_dropdown_menu(InbeApp *app, int id)
{
    return ui_draw_dropdown_menu(app, id);
}

void
language_tab_draw(InbeApp *app)
{
    int title_font = ui_clamp_px(22, 18, 26);
    int label_font = ui_clamp_px(14, 12, 16);
    int title_w;
    int content_x;
    int content_w;
    int dropdown_h = ui_px(36);
    int dropdown_w;
    int dropdown_x;
    int dropdown_y;
    int button_x;
    int button_y;
    int next_hover = 0;
    int *selected_index = &app->language_index;
    int language_count = locale_count();
    int selection_changed = 0;

    if(language_count <= 0)
        language_count = 1;
    if(*selected_index < 0 || *selected_index >= language_count)
        *selected_index = 0;

    ui_centered_column(ui_px(340), ui_page_side_padding(), &content_x, &content_w);
    dropdown_w = content_w;
    dropdown_x = content_x;
    dropdown_y = view_height / 2 - dropdown_h / 2;
    button_x = view_width / 2;
    button_y = dropdown_y + ui_px(64);

    if(language_dropdown_button(app, 200, dropdown_x, dropdown_y, dropdown_w, dropdown_h, selected_index))
        selection_changed = 1;

    title_w = MeasureText(locale_get("language_picker_title"), title_font);
    DrawText(locale_get("language_picker_title"), view_width / 2 - title_w / 2, ui_px(28), title_font, c_text);
    DrawText(locale_get("language_label"), dropdown_x, dropdown_y - ui_px(24), label_font, c_text);

    if(ui_draw_text_btn(app, button_x, button_y, locale_get("next_button"), &next_hover)) {
        if(!app->tutorial_seen)
            app->inbe.screen = InbeScreenManual;
        else
            app->inbe.screen = InbeScreenStart;
    }

    if(language_dropdown_menu(app, 200))
        selection_changed = 1;

    if(selection_changed)
        apply_language_selection(app, *selected_index, 1);
}
