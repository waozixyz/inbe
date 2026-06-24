#include "settings_screen.h"

#include "app.h"
#include "app_settings.h"
#include "flint_locale.h"
#include "settings_data.h"
#include "settings_device.h"
#include "settings_theme.h"
#include "flint_theme.h"
#include "flint_ui.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

extern int view_width;
extern int view_height;

static char unified_status[256] = "";
static char unified_detail[256] = "";
static int unified_status_type = 0;

static int
settings_draw_subtab_bar(int y, int h, const char **tab_names, int tab_count,
                         int selected_tab)
{
    enum { SETTINGS_SUBTAB_RENDER_MAX = 8 };
    FlintUISubtab tabs[SETTINGS_SUBTAB_RENDER_MAX];

    if(tab_count <= 0 || tab_count > SETTINGS_SUBTAB_RENDER_MAX)
        return -1;

    for(int i = 0; i < tab_count; i++) {
        tabs[i] = (FlintUISubtab){0};
        tabs[i].label = tab_names[i];
        tabs[i].disabled = 0;
    }

    return ui_draw_subtab_bar((FlintUISubtabBar){
        .bounds = {0, (float)y, (float)view_width, (float)h},
        .tabs = tabs,
        .count = tab_count,
        .selected_index = selected_tab
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
    if(tab == SETTINGS_TAB_DEVICE)
        return settings_device_content_height(content_w);
    if(settings_data_is_configuring(app))
        return flint_px(430);
    return settings_data_content_height(content_w);
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
    int top_margin = 0;
    int main_header_h;
    int detail_header_h;
    int top_tab_h;
    int top_tab_y;
    int tab_gap;
    int tab_content_start_y;
    int content_viewport_h;
    int clicked_top_tab = -1;
    const char *settings_tabs[] = {
        locale_get("settings_tab_device"),
        locale_get("settings_tab_theme"),
        locale_get("settings_tab_data"),
    };
    SettingsDeviceState device_state = {0};

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        app->settings_drag_slider = 0;

    if(app->settings_tab < SETTINGS_TAB_DEVICE || app->settings_tab >= SETTINGS_TAB_COUNT)
        app->settings_tab = SETTINGS_TAB_DEVICE;

    if(settings_data_draw_pending_file_dialog(app))
        return 1;

    main_header_h = settings_data_is_configuring(app) ? 0 : flint_px(58);
    if(main_header_h > 0) {
        Texture2D left_icon = (Texture2D){0};
        FlintUIHeader header = ui_draw_title_header(main_header_h,
                                                    locale_get("settings_title"),
                                                    left_icon,
                                                    (Texture2D){0});
        (void)header.left_clicked;
    }

    detail_header_h = settings_data_is_configuring(app) ? flint_px(58) : 0;
    if(detail_header_h > 0) {
        FlintUIHeader header = ui_draw_title_header(detail_header_h,
                                                    locale_get("sync_configure_account_button"),
                                                    app->icons[UI_ICON_TYPE_RETURN],
                                                    (Texture2D){0});
        if(header.left_clicked) {
            app->settings_data_view = 0;
            app->settings_scroll = 0;
            app->sync_server_url_focused = 0;
            settings_screen_clear_status();
        }
    }

    top_tab_h = detail_header_h > 0 ? 0 : flint_px(40);
    top_tab_y = top_margin + main_header_h + detail_header_h;
    tab_gap = top_tab_h > 0 ? flint_px(14) : 0;
    tab_content_start_y = top_tab_y + top_tab_h + tab_gap;
    content_viewport_h = view_height - tab_content_start_y - app_content_bottom_reserved(app);

#if ANDROID_BUILD
    settings_data_handle_android_import(app);
#elif defined(PLATFORM_WEB)
    settings_data_handle_web_import(app);
#endif

    if(content_viewport_h < 0)
        content_viewport_h = 0;

    if(top_tab_h > 0)
        clicked_top_tab = settings_draw_subtab_bar(top_tab_y, top_tab_h, settings_tabs,
                                                   SETTINGS_TAB_COUNT, app->settings_tab);
    if(clicked_top_tab != -1) {
        app->settings_tab = clicked_top_tab;
        app->settings_scroll = 0;
        app->settings_data_view = 0;
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
        else if(app->settings_tab == SETTINGS_TAB_DEVICE)
            settings_device_draw(app, page.content_x, page.content_w, &y, &device_state);
        else
            settings_data_draw(app, page.content_x, page.content_w, &y);

        ui_scroll_page_end(page);
    }

    ui_set_dropdown_clip_top(tab_content_start_y);
    if(app->settings_tab == SETTINGS_TAB_THEME)
        settings_theme_handle_overlays(app, &app->theme_state);
    else if(app->settings_tab == SETTINGS_TAB_DEVICE)
        settings_device_handle_overlays(app, &device_state);
    ui_set_dropdown_clip_top(0);

    settings_data_draw_modals(app);

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && app->settings_dirty)
        save_settings(app);

    return 0;
}
