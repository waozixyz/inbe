#include "language_screen.h"

#include "app.h"
#include "locale.h"
#include "theme.h"
#include "ui.h"
#include "raylib.h"

extern int view_width;
extern int view_height;
static const char *g_language_options[64];
static int g_language_option_count = 0;

static void
build_language_options(const char **options, int max_options, int *count)
{
    int total = GetLocaleCount();

    if(total > max_options)
        total = max_options;
    if(total < 0)
        total = 0;

    for(int i = 0; i < total; i++)
        options[i] = GetLocaleLabel(i);

    *count = total;
}

int
language_dropdown_button(InbeApp *app, int id, int x, int y, int w, int h, int *selected_index)
{
    if(app == NULL || selected_index == NULL)
        return 0;

    build_language_options(g_language_options, 64, &g_language_option_count);
    if(g_language_option_count <= 0) {
        int font = GetUIFontSize();
        DrawRectangle(x, y, w, h, GetThemeButton());
        DrawUIBevel(x, y, w, h, DarkenUIColor(GetThemeBackground(), 30), LightenUIColor(GetThemeBackground(), 20));
        DrawUIText(GetLocaleText("language_label"), x + ScaleUIPx(12),
                 GetUIControlTextY(GetLocaleText("language_label"), y, h, font), font, GetThemeText());
        return 0;
    }

    if(*selected_index < 0 || *selected_index >= g_language_option_count)
        *selected_index = 0;

    return DrawUIDropdownButton(id, x, y, w, h, g_language_options, g_language_option_count, selected_index);
}

int
language_dropdown_menu(InbeApp *app, int id)
{
    (void)app;
    return DrawUIDropdownMenu(id);
}

void
language_screen_draw(InbeApp *app)
{
    int title_font;
    int label_font = GetUIFontSize();
    int title_w;
    int content_x;
    int content_w;
    int dropdown_h = ScaleUIPx(36);
    int dropdown_w;
    int dropdown_x;
    int dropdown_y;
    int button_x;
    int button_y;
    int next_hover = 0;
    int *selected_index = &app->language_index;
    int language_count = GetLocaleCount();
    int selection_changed = 0;

    if(language_count <= 0)
        language_count = 1;
    if(*selected_index < 0 || *selected_index >= language_count)
        *selected_index = 0;

    GetUICenteredColumn(ScaleUIPx(340), GetUIPageSidePadding(), &content_x, &content_w);
    dropdown_w = content_w;
    dropdown_x = content_x;
    dropdown_y = view_height / 2 - dropdown_h / 2;
    button_x = view_width / 2;
    button_y = dropdown_y + ScaleUIPx(64);

    if(language_dropdown_button(app, 200, dropdown_x, dropdown_y, dropdown_w, dropdown_h, selected_index))
        selection_changed = 1;

    title_font = GetUITitleFontSize(GetLocaleText("language_picker_title"), content_w);
    title_w = MeasureUIText(GetLocaleText("language_picker_title"), title_font);
    DrawUIText(GetLocaleText("language_picker_title"), view_width / 2 - title_w / 2, ScaleUIPx(28), title_font, GetThemeText());
    DrawUIText(GetLocaleText("language_label"), dropdown_x, dropdown_y - ScaleUIPx(24), label_font, GetThemeText());

    if(DrawUITextButton(button_x, button_y, GetLocaleText("next_button"), &next_hover)) {
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->practice_tab = PRACTICE_TAB_PLAY;
        app_switch_screen(app, InbeScreenStart);
    }

    if(language_dropdown_menu(app, 200))
        selection_changed = 1;

    if(selection_changed)
        apply_language_selection(app, *selected_index, 1);
}
