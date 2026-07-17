#include "settings_theme.h"

#include "app.h"
#include "app_settings.h"
#include "device_preferences.h"
#include "locale.h"
#include "settings_ui.h"
#include "theme.h"
#include "theme_meta.h"
#include "ui.h"

extern int view_width;
extern int view_height;

static const char *
settings_theme_label_key(ThemeId theme)
{
    switch(NormalizeTheme(theme)) {
    case THEME_SKY: return "theme_sky";
    case THEME_OCEAN: return "theme_ocean";
    case THEME_FOREST: return "theme_forest";
    case THEME_SUNSET: return "theme_sunset";
    case THEME_LAVENDER: return "theme_lavender";
    case THEME_CHERRY: return "theme_cherry";
    case THEME_DAWN: return "theme_dawn";
    case THEME_SAGE: return "theme_sage";
    case THEME_INK: return "theme_sepia";
    case THEME_MONO: return "theme_mono";
    case THEME_MINT: return "theme_mint";
    case THEME_COBALT: return "theme_cobalt";
    default: return "theme_sky";
    }
}

static const char *
settings_theme_label(ThemeId theme)
{
    return GetLocaleText(settings_theme_label_key(theme));
}

int
settings_theme_content_height(int content_w)
{
#if defined(PLATFORM_WEB)
    (void)content_w;
    return ScaleUIPx(260);
#else
    return ScaleUIPx(244) +
           settings_ui_toggle_row_height(GetLocaleText("transition_label"), content_w);
#endif
}

void
settings_theme_draw(InbeApp *app, int x, int w, int *y, SettingsThemeState *state)
{
    const char *theme_source_options[2];
    const char *theme_mode_options[3];
    const char *theme_options[THEME_COUNT];

    if(app == NULL || y == NULL || state == NULL)
        return;

    theme_source_options[APP_THEME_SOURCE_APP] = GetLocaleText("theme_inner_breeze");
    theme_source_options[APP_THEME_SOURCE_SYSTEM] = GetLocaleText("theme_system");
    app->theme_source = clampi(app->theme_source, APP_THEME_SOURCE_APP, APP_THEME_SOURCE_SYSTEM);
    DrawUIText(GetLocaleText("theme_label"), x, *y, GetUIFontSize(), GetThemeText());
    DrawUIDropdownButton(101, x, *y + ScaleUIPx(26), w, ScaleUIPx(36),
                            theme_source_options, 2, &app->theme_source);
    state->draw_theme_source_menu = 1;
    *y += ScaleUIPx(76);

    theme_mode_options[0] = GetLocaleText("theme_follow_device");
    theme_mode_options[1] = GetLocaleText("theme_light");
    theme_mode_options[2] = GetLocaleText("theme_dark");
    if(app->theme_source == APP_THEME_SOURCE_APP || ANDROID_BUILD) {
        app->theme_mode = clampi(app->theme_mode, APP_THEME_SYSTEM, APP_THEME_DARK);
        DrawUIText(GetLocaleText("theme_mode_label"), x, *y, GetUIFontSize(), GetThemeText());
        DrawUIDropdownButton(102, x, *y + ScaleUIPx(26), w, ScaleUIPx(36),
                                theme_mode_options, 3, &app->theme_mode);
        state->draw_theme_mode_menu = 1;
        *y += ScaleUIPx(76);
    } else {
        app->theme_mode = APP_THEME_SYSTEM;
        state->draw_theme_mode_menu = 0;
    }

    if(app->theme_source == APP_THEME_SOURCE_APP) {
        for(int i = 0; i < THEME_COUNT; i++)
            theme_options[i] = settings_theme_label((ThemeId)i);
        app->theme_id = clampi(app->theme_id, 0, THEME_COUNT - 1);
        DrawUIText(GetLocaleText("theme_palette_label"), x, *y, GetUIFontSize(), GetThemeText());
        DrawUIDropdownButton(103, x, *y + ScaleUIPx(26), w, ScaleUIPx(36),
                                theme_options, THEME_COUNT, &app->theme_id);
        state->draw_theme_palette_menu = 1;
        *y += ScaleUIPx(76);
    } else {
#if !ANDROID_BUILD
        Color muted = DarkenUIColor(GetThemeText(), 28);
        const char *system_name = GetSystemThemeNameCached();
        DrawUIText(system_name != NULL && system_name[0] != '\0'
                            ? system_name
                            : GetLocaleText("theme_system"),
                        x, *y, GetUISmallFontSize(), muted);
        *y += ScaleUIPx(38);
#endif
        state->draw_theme_palette_menu = 0;
    }

#if defined(PLATFORM_WEB)
    app->transition_mode = APP_TRANSITION_NONE;
#else
    {
        int transitions_enabled;
        app->transition_mode = clampi(app->transition_mode, APP_TRANSITION_NONE, APP_TRANSITION_FADE);
        transitions_enabled = app->transition_mode == APP_TRANSITION_FADE;
        if(settings_ui_draw_toggle_row(x, w, y, GetLocaleText("transition_label"),
                                       &transitions_enabled)) {
            app->transition_mode = transitions_enabled ? APP_TRANSITION_FADE
                                                       : APP_TRANSITION_NONE;
            if(app->transition_mode == APP_TRANSITION_NONE) {
                ResetUITransition(&app->screen_transition);
                app->content_transition.active = 0;
                app->route_transition_target = app_current_route(app);
            }
            app->settings_dirty = 1;
            app->settings_save_delay_ticks = 18;
        }
    }
#endif
}

void
settings_theme_handle_overlays(InbeApp *app, SettingsThemeState *state)
{
    int theme_source_changed = 0;
    int theme_mode_changed = 0;
    int theme_palette_changed = 0;

    if(app == NULL || state == NULL)
        return;
    if(state->draw_theme_source_menu && DrawUIDropdownMenu(101))
        theme_source_changed = 1;
    if(state->draw_theme_mode_menu && DrawUIDropdownMenu(102))
        theme_mode_changed = 1;
    if(state->draw_theme_palette_menu && DrawUIDropdownMenu(103))
        theme_palette_changed = 1;
    if(theme_source_changed || theme_mode_changed || theme_palette_changed) {
        app->theme_source = clampi(app->theme_source, APP_THEME_SOURCE_APP, APP_THEME_SOURCE_SYSTEM);
        app->theme_mode = clampi(app->theme_mode, APP_THEME_SYSTEM, APP_THEME_DARK);
        app->theme_id = clampi(app->theme_id, 0, THEME_COUNT - 1);
#if !ANDROID_BUILD
        if(app->theme_source == APP_THEME_SOURCE_SYSTEM)
            app->theme_mode = APP_THEME_SYSTEM;
#endif
        app_refresh_theme(app);
        app->settings_dirty = 1;
        app->settings_save_delay_ticks = 18;
    }

#if defined(PLATFORM_WEB)
    app->transition_mode = APP_TRANSITION_NONE;
#endif
}

void
settings_screen_draw_theme_picker_modal(InbeApp *app)
{
    int modal_w = ScaleUIPx(320);
    int modal_h = ScaleUIPx(360);
    const char *title = GetLocaleText("theme_picker_title");
    UIPanelFrame frame;
    UIScrollArea scroll_area;
    UIScrollView scroll_view;
    int picker_h;
    int draw_w;

    frame = DrawUIModalFrame(modal_w, modal_h, title,
                               (Texture2D){0},
                               app->icons[UI_ICON_TYPE_X]);

    if(frame.right_clicked) {
        app_close_modal(app);
        return;
    }

    draw_w = frame.content_w;
    picker_h = GetUIThemePickerHeight(draw_w);
    scroll_area = (UIScrollArea){
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
        .wheel_step = ScaleUIPx(42),
        .scrollbar_x = frame.content_x + frame.content_w - ScaleUIPx(8)
    };
    for(int i = 0; i < 3; i++) {
        UIScrollView measured = MeasureUIScrollContainer(scroll_area);
        if(measured.content_w == draw_w)
            break;
        draw_w = measured.content_w;
        scroll_area.content_height = GetUIThemePickerHeight(draw_w);
    }
    picker_h = GetUIThemePickerHeight(draw_w);
    scroll_area.content_height = picker_h;
    scroll_view = BeginUIScrollContainer(scroll_area);

    if(DrawUIThemePicker(scroll_view.content_x, scroll_view.content_y,
                            scroll_view.content_w, app->dark_mode, &app->theme_id)) {
        app->theme_id = clampi(app->theme_id, 0, THEME_COUNT - 1);
        app_refresh_theme(app);
        app->settings_dirty = 1;
        save_settings(app);
        app_close_modal(app);
    }
    EndUIScrollContainer(scroll_area, scroll_view);
}
