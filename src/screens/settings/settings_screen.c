#include "settings_screen.h"

#include "app.h"
#include "app_settings.h"
#include "locale.h"
#include "settings_about.h"
#include "settings_data.h"
#include "settings_device.h"
#include "settings_session.h"
#include "settings_theme.h"
#include "theme.h"
#include "ui.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

#define SETTINGS_CONTENT_TOP_PADDING 8

extern int view_width;
extern int view_height;

static char unified_status[256] = "";
static char unified_detail[256] = "";
static int unified_status_type = 0;

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

    status_font = GetUISmallFontSize();
    status_color = (unified_status_type == 2) ? RED : GetThemeText();

    DrawUIText(unified_status, x, *y, status_font, status_color);
    *y += ScaleUIPx(18);

    if(unified_detail[0] != '\0') {
        DrawUIText(unified_detail, x, *y, status_font,
                        DarkenUIColor(GetThemeText(), 40));
        *y += ScaleUIPx(18);
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

static int
settings_overview_content_height(int content_w)
{
    (void)content_w;
    return ScaleUIPx(250);
}

typedef struct SettingsScrollPageContext {
    InbeApp *app;
    int tab;
} SettingsScrollPageContext;

static int
settings_scroll_page_content_height(int content_w, void *user_data)
{
    SettingsScrollPageContext *ctx = user_data;
    int planned_content_w = content_w - ScaleUIPx(16);

    if(planned_content_w < ScaleUIPx(160))
        planned_content_w = content_w;
    if(ctx->tab == SETTINGS_TAB_OVERVIEW)
        return settings_overview_content_height(planned_content_w);
    return settings_tab_content_height(ctx->app, ctx->tab, planned_content_w);
}

static const char *
settings_tab_label(int tab)
{
    switch(tab) {
    case SETTINGS_TAB_SESSION:
        return GetLocaleText("settings_section_session");
    case SETTINGS_TAB_DEVICE:
        return GetLocaleText("settings_tab_device");
    case SETTINGS_TAB_THEME:
        return GetLocaleText("settings_section_appearance");
    case SETTINGS_TAB_ABOUT:
        return GetLocaleText("settings_tab_about");
    default:
        return GetLocaleText("settings_title");
    }
}

static void
settings_draw_section_header(InbeApp *app, int tab)
{
    int header_h = GetUITabBarHeight();
    const char *title = settings_tab_label(tab);

    if(DrawUIReturnTitleBar(app->icons[UI_ICON_TYPE_RETURN], title, header_h)) {
        app->settings_tab = SETTINGS_TAB_OVERVIEW;
        app->settings_scroll = 0;
        if(app->modal.type == UIModalThemePicker)
            app_close_modal(app);
        settings_screen_clear_status();
    }
}

static void
settings_draw_overview(InbeApp *app, int x, int w, int *y)
{
    const int tabs[] = {
        SETTINGS_TAB_SESSION,
        SETTINGS_TAB_DEVICE,
        SETTINGS_TAB_THEME,
        SETTINGS_TAB_ABOUT
    };
    int btn_h = ScaleUIPx(40);
    int gap = ScaleUIPx(10);
    int hover = 0;
    for(int i = 0; i < 4; i++) {
        if(DrawUIGenericButton(x, *y, w, btn_h, settings_tab_label(tabs[i]),
                                  UI_BUTTON_STYLE_SECONDARY, app->modal.active,
                                  &hover)) {
            app->settings_tab = tabs[i];
            app->settings_scroll = 0;
            settings_screen_clear_status();
        }
        *y += btn_h + gap;
    }
}

int
settings_screen_draw(InbeApp *app)
{
    int detail_header_h;
    int tab_content_start_y;
    int content_viewport_h;
    SettingsDeviceState device_state = {0};

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        app->settings_drag_slider = 0;

    if(app->settings_tab < SETTINGS_TAB_OVERVIEW || app->settings_tab >= SETTINGS_TAB_COUNT)
        app->settings_tab = SETTINGS_TAB_OVERVIEW;

    if(settings_data_draw_pending_file_dialog(app))
        return 1;

    detail_header_h = GetUITabBarHeight();
    tab_content_start_y = detail_header_h;
    tab_content_start_y += ScaleUIPx(SETTINGS_CONTENT_TOP_PADDING);
    content_viewport_h = view_height - tab_content_start_y - app_content_bottom_reserved(app);

#if ANDROID_BUILD
    settings_data_handle_android_import(app);
#elif defined(PLATFORM_WEB)
    settings_data_handle_web_import(app);
#endif

    if(content_viewport_h < 0)
        content_viewport_h = 0;

    if(app->settings_tab == SETTINGS_TAB_OVERVIEW)
        DrawUITitleBar(GetLocaleText("settings_title"), detail_header_h);
    else
        settings_draw_section_header(app, app->settings_tab);

    {
        SettingsScrollPageContext page_ctx = {app, app->settings_tab};
        UIScrollPage page = BeginUIScrollPage((UIScrollPageSpec){
            .y = tab_content_start_y,
            .height = content_viewport_h,
            .max_content_width = ScaleUIPx(CONTENT_MAX_W),
            .min_content_width = ScaleUIPx(320),
            .scroll_offset = &app->settings_scroll,
            .content_height = settings_scroll_page_content_height,
            .user_data = &page_ctx
        });
        int y = page.content_y;

        if(app->settings_tab == SETTINGS_TAB_OVERVIEW)
            settings_draw_overview(app, page.content_x, page.content_w, &y);
        else if(app->settings_tab == SETTINGS_TAB_THEME)
            settings_theme_draw(app, page.content_x, page.content_w, &y, &app->theme_state);
        else if(app->settings_tab == SETTINGS_TAB_SESSION)
            settings_session_draw(app, page.content_x, page.content_w, &y);
        else if(app->settings_tab == SETTINGS_TAB_DEVICE)
            settings_device_draw(app, page.content_x, page.content_w, &y, &device_state);
        else
            settings_about_draw(app, page.content_x, page.content_w, &y);

        EndUIScrollPage(page);
    }

    SetUIDropdownClipTop(tab_content_start_y);
    SetUIDropdownClipBottom(view_height - app_content_bottom_reserved(app));
    if(app->settings_tab == SETTINGS_TAB_THEME)
        settings_theme_handle_overlays(app, &app->theme_state);
    else if(app->settings_tab == SETTINGS_TAB_DEVICE)
        settings_device_handle_overlays(app, &device_state);
    SetUIDropdownClipTop(0);
    SetUIDropdownClipBottom(0);

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && app->settings_dirty &&
       app->settings_save_delay_ticks <= 0)
        save_settings(app);

    return 0;
}
