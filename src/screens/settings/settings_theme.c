#include "settings_theme.h"

#include "app.h"
#include "app_settings.h"
#include "device_preferences.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_theme_meta.h"
#include "flint_ui.h"

extern int view_width;
extern int view_height;

int
settings_theme_content_height(int content_w)
{
    (void)content_w;
#if defined(PLATFORM_WEB)
    return flint_px(260);
#else
    return flint_px(320);
#endif
}

void
settings_theme_draw(InbeApp *app, int x, int w, int *y, SettingsThemeState *state)
{
    const char *theme_source_options[2];
    const char *theme_mode_options[3];
    const char *theme_options[FLINT_THEME_COUNT];

    if(app == NULL || y == NULL || state == NULL)
        return;

    theme_source_options[APP_THEME_SOURCE_APP] = locale_get("theme_inner_breeze");
    theme_source_options[APP_THEME_SOURCE_SYSTEM] = locale_get("theme_system");
    app->theme_source = clampi(app->theme_source, APP_THEME_SOURCE_APP, APP_THEME_SOURCE_SYSTEM);
    flint_text_draw(locale_get("theme_label"), x, *y, flint_ui_font(), flint_theme_get_text());
    ui_draw_dropdown_button(101, x, *y + flint_px(26), w, flint_px(36),
                            theme_source_options, 2, &app->theme_source);
    state->draw_theme_source_menu = 1;
    *y += flint_px(76);

    theme_mode_options[0] = locale_get("theme_system");
    theme_mode_options[1] = locale_get("theme_light");
    theme_mode_options[2] = locale_get("theme_dark");
    app->theme_mode = clampi(app->theme_mode, APP_THEME_SYSTEM, APP_THEME_DARK);
    flint_text_draw(locale_get("theme_mode_label"), x, *y, flint_ui_font(), flint_theme_get_text());
    ui_draw_dropdown_button(102, x, *y + flint_px(26), w, flint_px(36),
                            theme_mode_options, 3, &app->theme_mode);
    state->draw_theme_mode_menu = 1;
    *y += flint_px(76);

    if(app->theme_source == APP_THEME_SOURCE_APP) {
        for(int i = 0; i < FLINT_THEME_COUNT; i++)
            theme_options[i] = flint_theme_label((FlintThemeId)i);
        app->theme_id = clampi(app->theme_id, 0, FLINT_THEME_COUNT - 1);
        flint_text_draw(locale_get("theme_palette_label"), x, *y, flint_ui_font(), flint_theme_get_text());
        ui_draw_dropdown_button(103, x, *y + flint_px(26), w, flint_px(36),
                                theme_options, FLINT_THEME_COUNT, &app->theme_id);
        state->draw_theme_palette_menu = 1;
        *y += flint_px(76);
    } else {
        const char *system_name = flint_theme_system_available()
                                      ? flint_theme_system_name()
                                      : locale_get("theme_system");
        Color muted = flint_darken(flint_theme_get_text(), 28);
        flint_text_draw(system_name, x, *y, flint_ui_font_small(), muted);
        state->draw_theme_palette_menu = 0;
        *y += flint_px(38);
    }

#if defined(PLATFORM_WEB)
    app->transition_mode = APP_TRANSITION_NONE;
    state->draw_transition_menu = 0;
#else
    {
        const char *transition_options[2];
        transition_options[APP_TRANSITION_NONE] = locale_get("transition_none");
        transition_options[APP_TRANSITION_FADE] = locale_get("transition_fade");
        app->transition_mode = clampi(app->transition_mode, APP_TRANSITION_NONE, APP_TRANSITION_FADE);
        flint_text_draw(locale_get("transition_label"), x, *y, flint_ui_font(), flint_theme_get_text());
        ui_draw_dropdown_button(104, x, *y + flint_px(26), w, flint_px(36),
                                transition_options, 2, &app->transition_mode);
    }
    state->draw_transition_menu = 1;
    *y += flint_px(76);
#endif
}

void
settings_theme_handle_overlays(InbeApp *app, SettingsThemeState *state)
{
    int theme_source_changed = 0;
    int theme_mode_changed = 0;
    int theme_palette_changed = 0;
    int transition_changed = 0;

    if(app == NULL || state == NULL)
        return;
    if(state->draw_theme_source_menu && ui_draw_dropdown_menu(101))
        theme_source_changed = 1;
    if(state->draw_theme_mode_menu && ui_draw_dropdown_menu(102))
        theme_mode_changed = 1;
    if(state->draw_theme_palette_menu && ui_draw_dropdown_menu(103))
        theme_palette_changed = 1;
    if(theme_source_changed || theme_mode_changed || theme_palette_changed) {
        app->theme_source = clampi(app->theme_source, APP_THEME_SOURCE_APP, APP_THEME_SOURCE_SYSTEM);
        app->theme_mode = clampi(app->theme_mode, APP_THEME_SYSTEM, APP_THEME_DARK);
        app->theme_id = clampi(app->theme_id, 0, FLINT_THEME_COUNT - 1);
        app_refresh_theme(app);
        app->settings_dirty = 1;
        save_settings(app);
    }

#if !defined(PLATFORM_WEB)
    if(state->draw_transition_menu && ui_draw_dropdown_menu(104))
        transition_changed = 1;
    if(transition_changed) {
        app->transition_mode = clampi(app->transition_mode, APP_TRANSITION_NONE, APP_TRANSITION_FADE);
        app->settings_dirty = 1;
        save_settings(app);
    }
#else
    (void)transition_changed;
    app->transition_mode = APP_TRANSITION_NONE;
#endif
}

void
settings_screen_draw_theme_picker_modal(InbeApp *app)
{
    int modal_w = flint_px(320);
    int modal_h = flint_px(360);
    const char *title = locale_get("theme_picker_title");
    FlintUIPanelFrame frame;
    FlintUIScrollArea scroll_area;
    FlintUIScrollView scroll_view;
    int picker_h;
    int draw_w;

    frame = ui_draw_modal_frame(modal_w, modal_h, title,
                               (Texture2D){0},
                               app->icons[UI_ICON_TYPE_X]);

    if(frame.right_clicked) {
        app_close_modal(app);
        return;
    }

    draw_w = frame.content_w;
    picker_h = ui_theme_picker_height(draw_w);
    scroll_area = (FlintUIScrollArea){
        .bounds = {
            (float)frame.content_x,
            (float)frame.content_y,
            (float)frame.content_w,
            (float)frame.content_h
        },
        .content_height = picker_h,
        .content_x = frame.content_x,
        .content_width = frame.content_w,
        .scroll_offset = &app->theme_state.theme_picker_scroll,
        .wheel_step = flint_px(42),
        .scrollbar_x = frame.content_x + frame.content_w - flint_px(8)
    };
    for(int i = 0; i < 3; i++) {
        FlintUIScrollView measured = ui_scroll_container_measure(scroll_area);
        if(measured.content_w == draw_w)
            break;
        draw_w = measured.content_w;
        scroll_area.content_height = ui_theme_picker_height(draw_w);
    }
    picker_h = ui_theme_picker_height(draw_w);
    scroll_area.content_height = picker_h;
    scroll_view = ui_scroll_container_begin(scroll_area);

    if(ui_draw_theme_picker(scroll_view.content_x, scroll_view.content_y,
                            scroll_view.content_w, app->dark_mode, &app->theme_id)) {
        app->theme_id = clampi(app->theme_id, 0, FLINT_THEME_COUNT - 1);
        app_refresh_theme(app);
        app->settings_dirty = 1;
        save_settings(app);
        app_close_modal(app);
    }
    ui_scroll_container_end(scroll_area, scroll_view);
}
