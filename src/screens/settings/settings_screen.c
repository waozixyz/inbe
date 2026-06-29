#include "settings_screen.h"

#include "app.h"
#include "app_settings.h"
#include "flint_locale.h"
#include "settings_about.h"
#include "settings_data.h"
#include "settings_device.h"
#include "settings_session.h"
#include "settings_theme.h"
#include "flint_theme.h"
#include "flint_ui.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

#define SETTINGS_CONTENT_TOP_PADDING 8

extern int view_width;
extern int view_height;

static char unified_status[256] = "";
static char unified_detail[256] = "";
static int unified_status_type = 0;

static int
settings_draw_desktop_tab_bar(InbeApp *app, int y, const char **tab_names)
{
    FlintUITab tabs[SETTINGS_TAB_COUNT];

    if(app == NULL)
        return -1;

    for(int i = 0; i < SETTINGS_TAB_COUNT; i++)
        tabs[i] = (FlintUITab){
            .label = tab_names[i],
            .icon = (Texture2D){0},
            .icon_size = 0,
            .disabled = app->modal.active
        };

    return ui_draw_tab_bar((FlintUITabBar){
        .bounds = {0, (float)y, (float)view_width, (float)ui_tab_bar_height()},
        .tabs = tabs,
        .count = SETTINGS_TAB_COUNT,
        .selected_index = app->settings_tab,
        .min_tab_width = flint_px(110),
        .max_tab_width = flint_px(170)
    });
}

void
settings_screen_set_status_success(const char *message, const char *detail)
{
    if(message != NULL) {
        strncpy(unified_status, message, sizeof(unified_status) - 1);
        unified_status[sizeof(unified_status) - 1] = '\0';
    } else {
        unified_status[0] = '\0';
    }
    if(detail != NULL) {
        strncpy(unified_detail, detail, sizeof(unified_detail) - 1);
        unified_detail[sizeof(unified_detail) - 1] = '\0';
    } else {
        unified_detail[0] = '\0';
    }
    unified_status_type = 1;
}

void
settings_screen_set_status_error(const char *message)
{
    if(message != NULL) {
        strncpy(unified_status, message, sizeof(unified_status) - 1);
        unified_status[sizeof(unified_status) - 1] = '\0';
    } else {
        unified_status[0] = '\0';
    }
    unified_detail[0] = '\0';
    unified_status_type = 2;
}

void
settings_screen_clear_status(void)
{
    unified_status[0] = '\0';
    unified_detail[0] = '\0';
    unified_status_type = 0;
}

static void
settings_draw_status(int x, int *y)
{
    int status_font;
    Color status_color;

    if(unified_status[0] == '\0')
        return;

    status_font = flint_ui_font_small();
    status_color = (unified_status_type == 2) ? RED : flint_theme_get_text();

    flint_text_draw(unified_status, x, *y, status_font, status_color);
    *y += flint_px(18);

    if(unified_detail[0] != '\0') {
        flint_text_draw(unified_detail, x, *y, status_font,
                        flint_darken(flint_theme_get_text(), 40));
        *y += flint_px(18);
    }
}

void
settings_screen_draw_status_reserved(int x, int *y, int reserved_h)
{
    int status_y = *y;

    settings_draw_status(x, &status_y);
    *y += reserved_h;
}

static int
settings_tab_content_height(InbeApp *app, int tab, int content_w)
{
    if(tab == SETTINGS_TAB_THEME)
        return settings_theme_content_height(content_w);
    if(tab == SETTINGS_TAB_SESSION)
        return settings_session_content_height(content_w);
    if(tab == SETTINGS_TAB_DEVICE)
        return settings_device_content_height(content_w);
    (void)app;
    return settings_about_content_height(content_w);
}

typedef struct SettingsScrollPageContext {
    InbeApp *app;
    int tab;
} SettingsScrollPageContext;

static int
settings_scroll_page_content_height(int content_w, void *user_data)
{
    SettingsScrollPageContext *ctx = user_data;
    int planned_content_w = content_w - flint_px(16);

    if(planned_content_w < flint_px(160))
        planned_content_w = content_w;
    return settings_tab_content_height(ctx->app, ctx->tab, planned_content_w);
}

int
settings_screen_draw(InbeApp *app)
{
    int detail_header_h;
    int selector_h;
    int tab_content_start_y;
    int content_viewport_h;
    int selected_tab;
    int clicked_tab = -1;
    int draw_settings_menu = 0;
    const char *settings_tabs[] = {
        locale_get("settings_section_session"),
        locale_get("settings_tab_device"),
        locale_get("settings_section_appearance"),
        locale_get("settings_tab_about"),
    };
    SettingsDeviceState device_state = {0};

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        app->settings_drag_slider = 0;

    if(app->settings_tab < SETTINGS_TAB_SESSION || app->settings_tab >= SETTINGS_TAB_COUNT)
        app->settings_tab = SETTINGS_TAB_SESSION;
    selected_tab = app->settings_tab;

    if(settings_data_draw_pending_file_dialog(app))
        return 1;

    detail_header_h = 0;

    selector_h = detail_header_h > 0 ? 0 : ui_tab_bar_height();
    tab_content_start_y = detail_header_h + selector_h;
    if(selector_h > 0)
        tab_content_start_y += flint_px(SETTINGS_CONTENT_TOP_PADDING);
    content_viewport_h = view_height - tab_content_start_y - app_content_bottom_reserved(app);

#if ANDROID_BUILD
    settings_data_handle_android_import(app);
#elif defined(PLATFORM_WEB)
    settings_data_handle_web_import(app);
#endif

    if(content_viewport_h < 0)
        content_viewport_h = 0;

    if(selector_h > 0) {
        selected_tab = app->settings_tab;
        if(app_should_use_tab_bar(app)) {
            clicked_tab = settings_draw_desktop_tab_bar(app, 0, settings_tabs);
        } else {
            (void)ui_draw_toolbar_header((FlintUIToolbarHeader){
                .leading_icon = (Texture2D){0},
                .toolbar = (FlintUIToolbar){
                    .id = 302,
                    .height = selector_h,
                    .options = app->modal.active ? NULL : settings_tabs,
                    .option_count = app->modal.active ? 0 : SETTINGS_TAB_COUNT,
                    .selected_index = &selected_tab,
                    .dropdown_min_width = flint_px(150),
                    .dropdown_height = flint_px(36),
                    .side_padding = -1
                }
            });
            draw_settings_menu = !app->modal.active;
        }
    }

    if(clicked_tab >= 0 && clicked_tab < SETTINGS_TAB_COUNT)
        selected_tab = clicked_tab;

    if(selected_tab != app->settings_tab) {
        app->settings_tab = selected_tab;
        app->settings_scroll = 0;
        if(app->modal.type == UIModalThemePicker) {
            app_close_modal(app);
        }
        settings_screen_clear_status();
    }

    {
        SettingsScrollPageContext page_ctx = {app, app->settings_tab};
        FlintUIScrollPage page = ui_scroll_page_begin((FlintUIScrollPageSpec){
            .y = tab_content_start_y,
            .height = content_viewport_h,
            .max_content_width = flint_px(CONTENT_MAX_W),
            .min_content_width = flint_px(320),
            .scroll_offset = &app->settings_scroll,
            .content_height = settings_scroll_page_content_height,
            .user_data = &page_ctx
        });
        int y = page.content_y;

        if(app->settings_tab == SETTINGS_TAB_THEME)
            settings_theme_draw(app, page.content_x, page.content_w, &y, &app->theme_state);
        else if(app->settings_tab == SETTINGS_TAB_SESSION)
            settings_session_draw(app, page.content_x, page.content_w, &y);
        else if(app->settings_tab == SETTINGS_TAB_DEVICE)
            settings_device_draw(app, page.content_x, page.content_w, &y, &device_state);
        else
            settings_about_draw(app, page.content_x, page.content_w, &y);

        ui_scroll_page_end(page);
    }

    if(draw_settings_menu) {
        ui_set_dropdown_clip_top(tab_content_start_y);
        if(ui_draw_dropdown_menu(302) && selected_tab != app->settings_tab) {
            app->settings_tab = selected_tab;
            app->settings_scroll = 0;
            if(app->modal.type == UIModalThemePicker) {
                app_close_modal(app);
            }
            settings_screen_clear_status();
        }
        ui_set_dropdown_clip_top(0);
    }

    ui_set_dropdown_clip_top(tab_content_start_y);
    if(app->settings_tab == SETTINGS_TAB_THEME)
        settings_theme_handle_overlays(app, &app->theme_state);
    else if(app->settings_tab == SETTINGS_TAB_DEVICE)
        settings_device_handle_overlays(app, &device_state);
    ui_set_dropdown_clip_top(0);

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && app->settings_dirty)
        save_settings(app);

    return 0;
}
