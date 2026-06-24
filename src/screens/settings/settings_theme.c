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
    return flint_px(304) + flint_px(60);
}

void
settings_theme_draw(InbeApp *app, int x, int w, int *y, SettingsThemeState *state)
{
    const char *theme_mode_options[3];

    if(app == NULL || y == NULL || state == NULL)
        return;

    theme_mode_options[0] = locale_get("theme_system");
    theme_mode_options[1] = locale_get("theme_light");
    theme_mode_options[2] = locale_get("theme_dark");
    app->theme_mode = clampi(app->theme_mode, APP_THEME_SYSTEM, APP_THEME_DARK);
    flint_text_draw(locale_get("theme_mode_label"), x, *y, flint_ui_font(), flint_theme_get_text());
    ui_draw_dropdown_button(102, x, *y + flint_px(26), w, flint_px(36),
                            theme_mode_options, 3, &app->theme_mode);
    state->draw_theme_mode_menu = 1;
    *y += flint_px(76);

    {
        const char *nav_mode_options[2];
        nav_mode_options[NAV_MODE_TABBAR] = locale_get("nav_mode_tabbar");
        nav_mode_options[NAV_MODE_DROPDOWN] = locale_get("nav_mode_dropdown");
        if(app->navigation_mode != NAV_MODE_DROPDOWN)
            app->navigation_mode = NAV_MODE_TABBAR;

        flint_text_draw(locale_get("nav_mode_label"), x, *y, flint_ui_font(),
                        flint_theme_get_text());
        ui_draw_dropdown_button(103, x, *y + flint_px(26), w, flint_px(36),
                                nav_mode_options, 2,
                                &app->navigation_mode);
    }
    state->draw_nav_mode_menu = 1;
    *y += flint_px(76);

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

    // Draw single theme circle button
    flint_text_draw(locale_get("theme_label"), x, *y, flint_ui_font(), flint_theme_get_text());
    *y += flint_px(26);

    int circle_size = flint_px(32);
    Color theme_color = flint_theme_get_circle();
    int circle_x = x + w / 2;
    int circle_y = *y + circle_size / 2;

    // Check for hover
    Rectangle circle_bounds = {
        (float)(circle_x - circle_size),
        (float)(circle_y - circle_size),
        (float)(circle_size * 2),
        (float)(circle_size * 2)
    };
    int is_hovered = CheckCollisionPointRec(GetMousePosition(), circle_bounds);

    // Draw circle with hover effect
    int draw_size = is_hovered ? circle_size + flint_px(4) : circle_size;
    DrawCircle(circle_x, circle_y, draw_size / 2, theme_color);
    DrawCircleLines(circle_x, circle_y, draw_size / 2 + flint_px(2),
                    flint_theme_get_text());

    // Check for click to open modal (skip if modal is active or just closed)
    if(is_hovered && !app->modal.active &&
       !ui_input_captures_click(GetMousePosition()) &&
       app->inbe.frame - app->modal_open_frame > 1) {
        ui_mark_clickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            app_open_modal(app, UIModalThemePicker);
        }
    }
    *y += circle_size + flint_px(20);
}

void
settings_theme_handle_overlays(InbeApp *app, SettingsThemeState *state)
{
    int theme_mode_changed = 0;
    int nav_mode_changed = 0;
    int transition_changed = 0;

    if(app == NULL || state == NULL)
        return;
    if(state->draw_theme_mode_menu && ui_draw_dropdown_menu(102))
        theme_mode_changed = 1;
    if(theme_mode_changed) {
        app->theme_mode = clampi(app->theme_mode, APP_THEME_SYSTEM, APP_THEME_DARK);
        app_refresh_theme(app);
        app->settings_dirty = 1;
        save_settings(app);
    }

    if(state->draw_nav_mode_menu && ui_draw_dropdown_menu(103))
        nav_mode_changed = 1;
    if(nav_mode_changed) {
        if(app->navigation_mode != NAV_MODE_DROPDOWN)
            app->navigation_mode = NAV_MODE_TABBAR;
        app->settings_dirty = 1;
        save_settings(app);
    }

    if(state->draw_transition_menu && ui_draw_dropdown_menu(104))
        transition_changed = 1;
    if(transition_changed) {
        app->transition_mode = clampi(app->transition_mode, APP_TRANSITION_NONE, APP_TRANSITION_FADE);
        app->settings_dirty = 1;
        save_settings(app);
    }
}

void
settings_screen_draw_theme_picker_modal(InbeApp *app)
{
    int modal_w = flint_px(320);
    int modal_h = flint_px(360);
    const char *title = locale_get("theme_picker_title");
    FlintUIPanelFrame frame;

    frame = ui_draw_modal_frame(modal_w, modal_h, title,
                               (Texture2D){0},
                               app->icons[UI_ICON_TYPE_X]);

    if(frame.right_clicked) {
        app_close_modal(app);
        return;
    }

    // Draw theme picker in modal content area
    if(ui_draw_theme_picker(frame.content_x, frame.content_y,
                            frame.content_w, app->dark_mode, &app->theme_id)) {
        app->theme_id = clampi(app->theme_id, 0, FLINT_THEME_COUNT - 1);
        app_refresh_theme(app);
        app->settings_dirty = 1;
        save_settings(app);
        app_close_modal(app);
    }
}
