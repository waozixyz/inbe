#include "trackers_screen.h"
#include "../app.h"
#include "../locale.h"
#include "flint_ui.h"
#include "flint_dpi.h"
#include "flint_text.h"
#include "flint_theme.h"
#include "../theme.h"
#include "../storage.h"
#include "../data.h"
#include "habits_screen.h"
#include <stdio.h>
#include <string.h>

#ifndef TRACKER_CATEGORY_COUNT
#define TRACKER_CATEGORY_COUNT 3
#endif

const char *g_tracker_category_labels[TRACKER_CATEGORY_COUNT] = {
    "Mind",
    "Yoga",
    "Fitness"
};

const int g_tracker_category_default_themes[TRACKER_CATEGORY_COUNT] = {
    FLINT_THEME_SKY,
    FLINT_THEME_SUNSET,
    FLINT_THEME_CHERRY
};

void
on_tracker_tab_click(void *user_data)
{
    InbeApp *app = user_data;
    extern void app_request_bottom_tab(InbeApp *app, int bottom_tab);
    enum { APP_BOTTOM_TAB_PRACTICE = 1 };
    app_request_bottom_tab(app, APP_BOTTOM_TAB_PRACTICE);
}

void
tracker_ensure_enabled_selection(InbeApp *app)
{
    if(app == NULL)
        return;

    enum {
        TRACKER_CATEGORY_MIND = 0,
        TRACKER_CATEGORY_YOGA,
        TRACKER_CATEGORY_FITNESS
    };

    if(app->practice_category_tab < 0 || app->practice_category_tab >= TRACKER_CATEGORY_COUNT)
        app->practice_category_tab = TRACKER_CATEGORY_MIND;

    if(app->practice_tab_enabled[app->practice_category_tab])
        return;

    for(int i = 0; i < TRACKER_CATEGORY_COUNT; i++) {
        if(app->practice_tab_enabled[i]) {
            app->practice_category_tab = i;
            return;
        }
    }

    app->practice_tab_enabled[TRACKER_CATEGORY_MIND] = 1;
    app->practice_category_tab = TRACKER_CATEGORY_MIND;
}

int
tracker_enabled_count(InbeApp *app)
{
    int count = 0;

    if(app == NULL)
        return 0;

    for(int i = 0; i < TRACKER_CATEGORY_COUNT; i++) {
        if(app->practice_tab_enabled[i])
            count++;
    }
    return count;
}

int
tracker_active_theme(InbeApp *app)
{
    if(app == NULL)
        return FLINT_THEME_SKY;

    extern int clampi(int value, int min, int max);

    tracker_ensure_enabled_selection(app);
    return clampi(app->practice_tab_theme[app->practice_category_tab], 0, FLINT_THEME_COUNT - 1);
}

void
tracker_sync_global_theme(InbeApp *app)
{
    if(app == NULL)
        return;
    extern void app_refresh_theme(InbeApp *app);
    app->theme_id = tracker_active_theme(app);
    app_refresh_theme(app);
}

int
tracker_activity_count_for_tab(int tab)
{
    enum {
        TRACKER_CATEGORY_MIND = 0
    };
    return tab == TRACKER_CATEGORY_MIND ? 2 : 1;
}

int
tracker_activity_for_tab(int tab, int index)
{
    enum {
        TRACKER_CATEGORY_MIND = 0,
        TRACKER_CATEGORY_YOGA,
        TRACKER_CATEGORY_FITNESS,
        EXERCISE_MEDITATION = 1,
        EXERCISE_WIM_HOF = 0,
        EXERCISE_SUN_SALUTATION = 2,
        EXERCISE_7_MINUTE_WORKOUT = 3
    };

    if(tab == TRACKER_CATEGORY_MIND) {
        return index == 1 ? EXERCISE_MEDITATION : EXERCISE_WIM_HOF;
    }
    if(tab == TRACKER_CATEGORY_YOGA)
        return EXERCISE_SUN_SALUTATION;
    if(tab == TRACKER_CATEGORY_FITNESS)
        return EXERCISE_7_MINUTE_WORKOUT;
    return EXERCISE_WIM_HOF;
}

const char *
tracker_activity_label(int exercise)
{
    enum {
        EXERCISE_MEDITATION = 1,
        EXERCISE_WIM_HOF = 0,
        EXERCISE_SUN_SALUTATION = 2,
        EXERCISE_7_MINUTE_WORKOUT = 3
    };

    switch(exercise) {
    case EXERCISE_MEDITATION:
        return locale_get("exercise_meditation");
    case EXERCISE_SUN_SALUTATION:
        return "Sun Salutation";
    case EXERCISE_7_MINUTE_WORKOUT:
        return "7-Minute Workout";
    case EXERCISE_WIM_HOF:
    default:
        return locale_get("exercise_wim_hof");
    }
}

int
tracker_activity_index_for_tab(int tab, int exercise)
{
    int count = tracker_activity_count_for_tab(tab);
    for(int i = 0; i < count; i++) {
        if(tracker_activity_for_tab(tab, i) == exercise)
            return i;
    }
    return 0;
}

void
tracker_clamp_activity_to_tab(InbeApp *app)
{
    int tab;
    int index;

    if(app == NULL)
        return;

    tracker_ensure_enabled_selection(app);
    tab = app->practice_category_tab;
    index = tracker_activity_index_for_tab(tab, app->exercise_type);
    app->exercise_type = tracker_activity_for_tab(tab, index);
}

Color
tracker_theme_color(InbeApp *app, int tab_index)
{
    extern int clampi(int value, int min, int max);

    int theme_id = FLINT_THEME_SKY;
    Color color = theme_get_circle();

    if(app != NULL && tab_index >= 0 && tab_index < TRACKER_CATEGORY_COUNT)
        theme_id = clampi(app->practice_tab_theme[tab_index], 0, FLINT_THEME_COUNT - 1);

    if(!flint_theme_catalog_color((FlintThemeId)theme_id, app != NULL && app->dark_mode != 0,
                                  "circle", &color)) {
        const char *scope = flint_theme_scope_for((FlintThemeId)theme_id,
                                                  app != NULL && app->dark_mode);
        color = flint_theme_get(scope, "circle");
    }

    return color;
}

int
tracker_category_bottom_y_for_app(InbeApp *app)
{
    return flint_px(58) +
           (tracker_enabled_count(app) > 1 ? flint_px(TRACKER_CATEGORY_TAB_H) : 0);
}

void
draw_tracker_coming_soon_popout(InbeApp *app)
{
    extern int view_width;

    const char *message = "coming soon...";
    int font = flint_ui_font();
    int pad_x = flint_px(14);
    int pad_y = flint_px(8);
    int tabs_y = flint_px(58);
    int tabs_h = flint_px(TRACKER_CATEGORY_TAB_H);
    int popout_w = flint_text_measure(message, font) + pad_x * 2;
    int popout_h = font + pad_y * 2;
    int popout_x = (view_width - popout_w) / 2;
    int popout_y = tabs_y + tabs_h + flint_px(8);

    if(app == NULL || app->practice_coming_soon_ticks <= 0)
        return;

    if(popout_x < flint_px(8))
        popout_x = flint_px(8);
    if(popout_x + popout_w > view_width - flint_px(8))
        popout_x = view_width - flint_px(8) - popout_w;

    DrawRectangle(popout_x, popout_y, popout_w, popout_h, theme_get_surface());
    ui_draw_bevel(popout_x, popout_y, popout_w, popout_h,
                  flint_lighten(theme_get_surface(), 40), flint_darken(theme_get_surface(), 40));
    flint_text_draw(message, popout_x + pad_x,
                    flint_ui_text_y(message, popout_y, popout_h, font),
                    font, theme_get_text());
}

void
draw_tracker_category_tabs(InbeApp *app)
{
    extern int view_width;
    extern void save_settings(InbeApp *app);

    FlintUISubtab tabs[TRACKER_CATEGORY_COUNT];
    int tab_indexes[TRACKER_CATEGORY_COUNT];
    int tabs_y = flint_px(58);
    int tabs_h = flint_px(TRACKER_CATEGORY_TAB_H);
    int enabled_count = 0;
    int selected_visible = 0;
    int clicked;

    if(app == NULL)
        return;

    tracker_ensure_enabled_selection(app);
    for(int i = 0; i < TRACKER_CATEGORY_COUNT; i++) {
        if(!app->practice_tab_enabled[i])
            continue;

        tab_indexes[enabled_count] = i;
        tabs[enabled_count] = (FlintUISubtab){
            .label = g_tracker_category_labels[i],
            .disabled = 0,
            .accent = tracker_theme_color(app, i)
        };
        if(i == app->practice_category_tab)
            selected_visible = enabled_count;
        enabled_count++;
    }

    if(enabled_count <= 1)
        return;

    clicked = ui_draw_subtab_bar((FlintUISubtabBar){
        .bounds = {0, (float)tabs_y, (float)view_width, (float)tabs_h},
        .tabs = tabs,
        .count = enabled_count,
        .selected_index = selected_visible,
        .font = flint_ui_font()
    });

    if(clicked >= 0 && clicked < enabled_count) {
        app->practice_category_tab = tab_indexes[clicked];
        app->practice_coming_soon_ticks = 0;
        tracker_clamp_activity_to_tab(app);
        tracker_sync_global_theme(app);
        save_settings(app);
    }
}

int
draw_theme_circle_button(InbeApp *app, int x, int y, int radius, int theme_id)
{

    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);
    Rectangle bounds = {
        (float)(x - radius - flint_px(5)),
        (float)(y - radius - flint_px(5)),
        (float)(radius * 2 + flint_px(10)),
        (float)(radius * 2 + flint_px(10))
    };
    int hovered = CheckCollisionPointRec(mouse, bounds);
    Color color = theme_get_circle();

    if(!flint_theme_catalog_color((FlintThemeId)theme_id, app->dark_mode != 0, "circle", &color)) {
        const char *scope = flint_theme_scope_for((FlintThemeId)theme_id, app->dark_mode != 0);
        color = flint_theme_get(scope, "circle");
    }

    DrawCircle(x, y, radius, color);
    DrawCircleLines(x, y, radius + flint_px(2), hovered ? theme_get_text() : flint_darken(theme_get_bg(), 42));

    if(hovered) {
        app->cursor_clickable = 1;
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !ui_input_captures_click(mouse))
            return 1;
    }

    return 0;
}

void
draw_tracker_config_button(InbeApp *app)
{
    extern int view_width;

    int icon_size = flint_px(20);
    int icon_padding = flint_px(8);
    int button_w = icon_size + icon_padding * 2;
    int button_x = view_width - button_w - flint_px(10);
    int button_y = (flint_px(58) - button_w) / 2;
    int hover = 0;

    if(app == NULL || app->modal.active)
        return;

    if(ui_draw_icon_btn_padded(button_x, button_y, icon_size, icon_padding,
                               app->icons[UI_ICON_TYPE_STACK], &hover)) {
        app->practice_config_theme_tab = TRACKER_CONFIG_TAB_LIST;
        app->inbe.screen = InbeScreenTrackerConfig;
    }
}

void
draw_tracker_config_page(InbeApp *app)
{
    extern int view_width;
    extern int view_height;
    extern void save_settings(InbeApp *app);
    extern int clampi(int value, int min, int max);

    int top_h = flint_px(58);
    int nav_h = flint_px(58);
    int content_x;
    int content_w;
    int max_w = flint_px(440);
    int y = top_h + flint_px(16);
    int row_h = flint_px(48);
    FlintUIHeader header;

    if(app == NULL)
        return;

    if(app->practice_config_theme_tab >= 0) {
        int tab = clampi(app->practice_config_theme_tab, 0, TRACKER_CATEGORY_COUNT - 1);
        char title[64];

        snprintf(title, sizeof(title), "%s Theme", g_tracker_category_labels[tab]);
        header = ui_draw_title_header(top_h, title,
                                      app->icons[UI_ICON_TYPE_RETURN], (Texture2D){0});
        if(header.left_clicked) {
            app->practice_config_theme_tab = TRACKER_CONFIG_TAB_LIST;
            save_settings(app);
            return;
        }

        flint_centered_column(max_w, flint_page_side_padding(), &content_x, &content_w);
        ui_begin_scissor((int)app->camera.offset.x,
                         (int)(app->camera.offset.y + top_h * app->camera.zoom),
                         (int)(view_width * app->camera.zoom),
                         (int)((view_height - top_h - nav_h) * app->camera.zoom));
        if(ui_draw_theme_picker(content_x, y, content_w, "Theme",
                                app->dark_mode, &app->practice_tab_theme[tab])) {
            app->practice_tab_theme[tab] = clampi(app->practice_tab_theme[tab], 0, FLINT_THEME_COUNT - 1);
            habits_sync_topic_theme_colors(app, tab, 1);
            if(tab == app->practice_category_tab)
                tracker_sync_global_theme(app);
            app->settings_dirty = 1;
            save_settings(app);
        }
        ui_end_scissor();
        return;
    }

    header = ui_draw_title_header(top_h, "Trackers",
                                  app->icons[UI_ICON_TYPE_RETURN], (Texture2D){0});
    if(header.left_clicked) {
        save_settings(app);
        app->inbe.screen = app->previous_screen;
        return;
    }

    flint_centered_column(max_w, flint_page_side_padding(), &content_x, &content_w);
    ui_begin_scissor((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + top_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)((view_height - top_h - nav_h) * app->camera.zoom));

    for(int i = 0; i < TRACKER_CATEGORY_COUNT; i++) {
        enum {
            TRACKER_CATEGORY_MIND = 0
        };
        int unavailable = i != TRACKER_CATEGORY_MIND;
        int label_x = content_x + flint_px(8);
        int circle_x = content_x + content_w - flint_px(24);
        int circle_y = y + row_h / 2;
        int theme_id = clampi(app->practice_tab_theme[i], 0, FLINT_THEME_COUNT - 1);
        Rectangle row_bounds = {
            (float)content_x,
            (float)y,
            (float)content_w,
            (float)row_h
        };
        Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);
        int row_hover = CheckCollisionPointRec(mouse, row_bounds);
        Color row_bg = unavailable ? flint_darken(theme_get_button(), 12) : flint_darken(theme_get_button(), 6);
        Color row_text = unavailable ? flint_darken(theme_get_text(), 72) : theme_get_text();

        if(unavailable && row_hover) {
            row_bg = flint_darken(theme_get_button(), 18);
            app->cursor_disabled = 1;
        }

        DrawRectangleRec(row_bounds, row_bg);
        DrawLine(content_x, y + row_h - 1,
                 content_x + content_w, y + row_h - 1,
                 flint_darken(theme_get_button(), 34));

        flint_text_draw(g_tracker_category_labels[i],
                        label_x,
                        flint_ui_text_y(g_tracker_category_labels[i],
                                        y, row_h, flint_ui_font()),
                        flint_ui_font(), row_text);

        if(unavailable) {
            Color color = tracker_theme_color(app, i);
            color.a = 120;
            DrawCircle(circle_x, circle_y, flint_px(13), color);
            DrawCircleLines(circle_x, circle_y, flint_px(15), flint_darken(theme_get_bg(), 45));
        } else if(draw_theme_circle_button(app, circle_x, circle_y, flint_px(13), theme_id)) {
            app->practice_config_theme_tab = i;
        }

        y += row_h;
    }

    ui_end_scissor();
    if(app->settings_dirty)
        save_settings(app);
}
