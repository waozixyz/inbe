#include "settings_screen.h"
#include "app.h"
#include "device_preferences.h"
#include "language_screen.h"
#include "locale.h"
#include "theme.h"
#include "flint_theme_meta.h"
#include "flint_clip.h"
#include "flint_ui.h"
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(_WIN32) && !defined(PLATFORM_WEB)
#define INBE_HAS_FLINT_FILE_DIALOG 1
#include "flint_file_dialog.h"
#endif
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include "android_import.h"
#endif
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif
#include "version.h"
#include "data.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

extern int view_width;
extern int view_height;

enum {
    SETTINGS_DATA_ACTION_NONE = 0,
    SETTINGS_DATA_ACTION_IMPORT,
    SETTINGS_DATA_ACTION_EXPORT
};

#if defined(INBE_HAS_FLINT_FILE_DIALOG)
static FlintFileDialog export_dlg;
static FlintFileDialog import_dlg;
static int data_file_dialog_action = SETTINGS_DATA_ACTION_NONE;
#endif

static int
settings_draw_subtab_bar(int y, int h, const char **tab_names, int tab_count,
                         int selected_tab)
{
    enum { SETTINGS_SUBTAB_RENDER_MAX = 8 };
    FlintUISubtab tabs[SETTINGS_SUBTAB_RENDER_MAX];

    if(tab_count <= 0 || tab_count > SETTINGS_SUBTAB_RENDER_MAX)
        return -1;

    for(int i = 0; i < tab_count; i++) {
        tabs[i].label = tab_names[i];
        tabs[i].disabled = 0;
    }

    return ui_draw_subtab_bar((FlintUISubtabBar){
        .bounds = {0, (float)y, (float)view_width, (float)h},
        .tabs = tabs,
        .count = tab_count,
        .selected_index = selected_tab,
        .font = flint_ui_font()
    });
}

/* Unified status system variables */
static char unified_status[256] = "";
static char unified_detail[256] = "";
static int unified_status_type = 0;
static char pending_import_path[FS_PATH_MAX] = "";
static DataImportInfo pending_import_info;

/* Helper functions for unified status system */
void settings_screen_set_status_success(const char *message, const char *detail) {
    if(message) {
        strncpy(unified_status, message, sizeof(unified_status) - 1);
        unified_status[sizeof(unified_status) - 1] = '\0';
    } else {
        unified_status[0] = '\0';
    }
    if(detail) {
        strncpy(unified_detail, detail, sizeof(unified_detail) - 1);
        unified_detail[sizeof(unified_detail) - 1] = '\0';
    } else {
        unified_detail[0] = '\0';
    }
    unified_status_type = 1;
}

void settings_screen_set_status_error(const char *message) {
    if(message) {
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
settings_clear_pending_import(void)
{
    pending_import_path[0] = '\0';
    memset(&pending_import_info, 0, sizeof(pending_import_info));
}

static void
settings_import_success(InbeApp *app, int imported_settings)
{
    app_reload_after_import(app, imported_settings);
    settings_screen_set_status_success(locale_get("imported_data"), NULL);
    TraceLog(LOG_INFO, "DATA: Import successful");
}

static int
settings_perform_import(InbeApp *app, const char *path, DataImportMode mode)
{
    if(app == NULL || path == NULL || path[0] == '\0') {
        settings_screen_set_status_error(locale_get("import_invalid_file"));
        return 0;
    }

    if(data_import_with_mode(path, mode)) {
        settings_import_success(app, mode == DATA_IMPORT_DATA_AND_SETTINGS);
        return 1;
    }

    settings_screen_set_status_error(locale_get("import_failed"));
    TraceLog(LOG_ERROR, "DATA: Import failed");
    return 0;
}

static void
settings_begin_import_for_path(InbeApp *app, const char *path)
{
    DataImportInfo info;

    if(app == NULL || path == NULL || path[0] == '\0') {
        settings_screen_set_status_error(locale_get("import_invalid_file"));
        return;
    }

    memset(&info, 0, sizeof(info));
    if(!data_inspect_import(path, &info) || !info.valid) {
        settings_screen_set_status_error(locale_get("import_invalid_file"));
        TraceLog(LOG_WARNING, "DATA: Invalid import file selected");
        return;
    }

    if((info.has_sessions || info.has_habits) && info.has_settings) {
        snprintf(pending_import_path, sizeof(pending_import_path), "%s", path);
        pending_import_info = info;
        app->modal.active = 1;
        app->modal.type = UIModalConfirmImportDataSettings;
        app->modal.selected_button = 0;
        settings_screen_clear_status();
        return;
    }

    settings_perform_import(app, path, DATA_IMPORT_DATA_ONLY);
}

static int
settings_draw_import_choice_modal(InbeApp *app)
{
    FlintUIPanelFrame frame;
    int font = flint_ui_font();
    int msg_y;
    int msg_x;
    const char *message = locale_get("import_choice_message");
    int row_gap = flint_px(10);
    int btn_h = flint_px(36);
    int btn_y;
    int btn_w;
    int hover = 0;

    (void)app;
    frame = ui_draw_modal_frame(flint_px(320), flint_px(232),
                                locale_get("import_choice_title"),
                                (Texture2D){0}, (Texture2D){0});
    msg_x = frame.content_x;
    msg_y = frame.content_y;
    flint_text_draw(message, msg_x, msg_y, font, theme_get_text());

    btn_y = frame.y + frame.h - flint_px(24) - btn_h * 2 - row_gap;
    btn_w = (frame.content_w - row_gap) / 2;
    if(ui_draw_generic_button(frame.content_x, btn_y, btn_w, btn_h,
                              locale_get("cancel_button"),
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover))
        return 1;
    if(ui_draw_generic_button(frame.content_x + btn_w + row_gap, btn_y, btn_w, btn_h,
                              locale_get("import_data_only_button"),
                              UI_BUTTON_STYLE_PRIMARY, 0, &hover))
        return 2;
    if(ui_draw_generic_button(frame.content_x, btn_y + btn_h + row_gap,
                              frame.content_w, btn_h,
                              locale_get("import_data_settings_button"),
                              UI_BUTTON_STYLE_PRIMARY, 0, &hover))
        return 3;
    return 0;
}

static void
settings_draw_status(int x, int *y)
{
    if(unified_status[0] == '\0')
        return;

    int status_font = flint_ui_font_small();
    Color status_color = (unified_status_type == 2) ? RED : theme_get_text();

    flint_text_draw(unified_status, x, *y, status_font, status_color);
    *y += flint_px(18);

    if(unified_detail[0] != '\0') {
        flint_text_draw(unified_detail, x, *y, status_font, flint_darken(theme_get_text(), 40));
        *y += flint_px(18);
    }
}

static void
settings_draw_status_reserved(int x, int *y, int reserved_h)
{
    int status_y = *y;

    settings_draw_status(x, &status_y);
    *y += reserved_h;
}

static void
settings_draw_version_centered(int x, int w, int *y)
{
    char version_text[32];
    int font = flint_ui_font_small();
    int text_w;

    snprintf(version_text, sizeof(version_text), "v%s", INBE_VERSION_STRING);
    text_w = flint_text_measure(version_text, font);
    flint_text_draw(version_text, x + (w - text_w) / 2, *y, font, flint_darken(theme_get_text(), 40));
    *y += flint_px(22);
}

static int
settings_link_icon_columns(int content_w)
{
    int max_columns = 5;
    int icon_size = flint_px(32);
    int icon_padding = flint_px(4);
    int icon_spacing = flint_px(20);
    int icon_btn_w = icon_size + icon_padding * 2;
    int total_w = icon_btn_w * max_columns + icon_spacing * (max_columns - 1);

    return total_w <= content_w ? max_columns : 2;
}

static int
settings_link_icons_height(int content_w)
{
    int link_count = 5;
    int icon_size = flint_px(32);
    int icon_padding = flint_px(4);
    int row_spacing = flint_px(16);
    int icon_btn_w = icon_size + icon_padding * 2;
    int columns = settings_link_icon_columns(content_w);
    int rows = (link_count + columns - 1) / columns;

    return flint_px(8) + rows * icon_btn_w + (rows - 1) * row_spacing;
}

static int
settings_data_content_height(int content_w)
{
    int data_button_h = flint_px(36);

    return flint_px(98) +
           data_button_h + flint_px(12) +
           data_button_h + flint_px(12) +
           data_button_h + flint_px(12) +
           flint_px(42) +
           settings_link_icons_height(content_w) +
           flint_px(8) + flint_px(22) +
           flint_px(40);
}

static void
settings_draw_link_icons(InbeApp *app, int content_x, int content_w, int *y)
{
    int link_count = 5;
    int icon_size = flint_px(32);
    int icon_padding = flint_px(4);
    int icon_spacing = flint_px(20);
    int icon_btn_w = icon_size + icon_padding * 2;
    int columns = settings_link_icon_columns(content_w);
    int grid_w = icon_btn_w * columns + icon_spacing * (columns - 1);
    int links_start_x = content_x + (content_w - grid_w) / 2;
    int row_spacing = flint_px(16);
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

    *y += flint_px(8);
    for(int i = 0; i < link_count; i++) {
        int col = i % columns;
        int row = i / columns;
        int icon_x = links_start_x + col * (icon_btn_w + icon_spacing) + icon_padding;
        int icon_y = *y + row * (icon_btn_w + row_spacing);
        ui_draw_icon_link(icon_x, icon_y, icon_size, icons[i], urls[i]);
    }
    *y += settings_link_icons_height(content_w) - flint_px(8);
}

#if defined(INBE_HAS_FLINT_FILE_DIALOG)
static void
settings_apply_file_dialog_theme(InbeApp *app)
{
    int theme_id = app != NULL ? app->theme_id : FLINT_THEME_SKY;
    int dark_mode = app != NULL && app->dark_mode != 0;

    if(theme_id < 0 || theme_id >= FLINT_THEME_COUNT)
        theme_id = FLINT_THEME_SKY;
    flint_file_dialog_set_theme_scope(flint_theme_scope_for((FlintThemeId)theme_id,
                                                            dark_mode != 0));
}
#endif

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
static void
settings_screen_handle_android_import(InbeApp *app)
{
    char import_path[FS_PATH_MAX];
    int import_result = android_import_poll_result(import_path, sizeof(import_path));

    if(import_result == ANDROID_IMPORT_RESULT_NONE)
        return;

    if(import_result == ANDROID_IMPORT_RESULT_CANCELLED) {
        settings_screen_set_status_error(locale_get("import_cancelled"));
        return;
    }

    if(import_path[0] == '\0') {
        settings_screen_set_status_error(locale_get("import_invalid_file"));
        return;
    }

    settings_begin_import_for_path(app, import_path);
}
#endif

#if defined(PLATFORM_WEB)
#define SETTINGS_WEB_IMPORT_PATH "/tmp/inbe-web-import.zip"

static void
settings_web_import_open_picker(void)
{
    EM_ASM({
        const importPath = UTF8ToString($0);
        Module.__inbeImportResult = 0;

        const input = document.createElement("input");
        input.type = "file";
        input.accept = ".zip,.db,application/zip,application/x-sqlite3,application/octet-stream";
        input.style.display = "none";

        input.onchange = async function() {
            try {
                if(!input.files || input.files.length === 0) {
                    Module.__inbeImportResult = 2;
                    return;
                }

                const file = input.files[0];
                const bytes = new Uint8Array(await file.arrayBuffer());
                try {
                    FS.mkdir("/tmp");
                } catch(e) {}
                FS.writeFile(importPath, bytes);
                Module.__inbeImportResult = 1;
            } catch(e) {
                console.error("Inbe web import failed:", e);
                Module.__inbeImportResult = 3;
            } finally {
                input.remove();
            }
        };

        document.body.appendChild(input);
        input.click();
    }, SETTINGS_WEB_IMPORT_PATH);
}

static void
settings_screen_handle_web_import(InbeApp *app)
{
    int import_result = EM_ASM_INT({
        const result = Module.__inbeImportResult || 0;
        if(result !== 0)
            Module.__inbeImportResult = 0;
        return result;
    });

    if(import_result == 0)
        return;
    if(import_result == 2) {
        settings_screen_set_status_error(locale_get("import_cancelled"));
        return;
    }
    if(import_result != 1) {
        settings_screen_set_status_error(locale_get("import_failed"));
        return;
    }

    settings_begin_import_for_path(app, SETTINGS_WEB_IMPORT_PATH);
}
#endif

static void
settings_import_data(InbeApp *app)
{
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    if(android_import_open_picker())
        settings_screen_set_status_success(locale_get("import_data_dialog_title"), NULL);
    else
        settings_screen_set_status_error(locale_get("import_failed"));
#elif defined(PLATFORM_WEB)
    (void)app;
    settings_web_import_open_picker();
    settings_screen_set_status_success(locale_get("import_data_dialog_title"), NULL);
#elif defined(INBE_HAS_FLINT_FILE_DIALOG)
    settings_apply_file_dialog_theme(app);
    flint_file_dialog_begin_load(&import_dlg, locale_get("import_data_dialog_title"));
    data_file_dialog_action = SETTINGS_DATA_ACTION_IMPORT;
#else
    (void)app;
    settings_screen_set_status_error(locale_get("import_failed"));
#endif
}

static void
settings_export_data(InbeApp *app)
{
    char export_filename[64];
    data_default_export_filename(export_filename, sizeof(export_filename));

    if(!data_has_any()) {
        settings_screen_set_status_error(locale_get("no_data_to_export"));
        return;
    }

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    (void)app;
    if(data_export(export_filename)) {
        settings_screen_set_status_success(locale_get("exported_label"), NULL);
        TraceLog(LOG_INFO, "DATA: Export successful (share sheet shown)");
    } else {
        settings_screen_set_status_error(locale_get("export_failed"));
        TraceLog(LOG_ERROR, "DATA: Export failed");
    }
#elif defined(INBE_HAS_FLINT_FILE_DIALOG)
    settings_apply_file_dialog_theme(app);
    flint_file_dialog_begin_save(&export_dlg, locale_get("export_data_dialog_title"), export_filename);
    data_file_dialog_action = SETTINGS_DATA_ACTION_EXPORT;
#else
    (void)app;
    settings_screen_set_status_error(locale_get("export_failed"));
#endif
}

static void
settings_request_delete_all_data(InbeApp *app)
{
    if(data_has_any()) {
        app->modal.active = 1;
        app->modal.type = UIModalConfirmDeleteData;
        app->modal.selected_button = 0;
    } else {
        settings_screen_set_status_error(locale_get("no_data_to_delete"));
    }
}

#if defined(INBE_HAS_FLINT_FILE_DIALOG)
static int
settings_draw_pending_file_dialog(InbeApp *app)
{
    FlintFileDialog *dlg;
    int result;

    if(data_file_dialog_action == SETTINGS_DATA_ACTION_NONE)
        return 0;

    settings_apply_file_dialog_theme(app);
    dlg = data_file_dialog_action == SETTINGS_DATA_ACTION_IMPORT ? &import_dlg : &export_dlg;
    result = flint_file_dialog_update(dlg);
    if(result < 0)
        return 1;

    if(data_file_dialog_action == SETTINGS_DATA_ACTION_IMPORT) {
        if(result == 1) {
            const char *path = flint_file_dialog_get_path(&import_dlg);
            if(path != NULL && path[0] != '\0') {
                settings_begin_import_for_path(app, path);
            } else {
                settings_screen_set_status_error(locale_get("import_invalid_file"));
                TraceLog(LOG_WARNING, "DATA: No file selected for import");
            }
        } else {
            settings_screen_set_status_error(locale_get("import_cancelled"));
        }
    } else {
        if(result == 1) {
            const char *path = flint_file_dialog_get_path(&export_dlg);
            if(path != NULL && data_export(path)) {
                const char *filename = GetFileName(path);
                settings_screen_set_status_success(locale_get("exported_label"), filename);
                TraceLog(LOG_INFO, "DATA: Export successful to %s", path);
            } else {
                settings_screen_set_status_error(locale_get("export_failed"));
                TraceLog(LOG_ERROR, "DATA: Export failed");
            }
        } else {
            settings_screen_set_status_error(locale_get("export_cancelled"));
        }
    }

    data_file_dialog_action = SETTINGS_DATA_ACTION_NONE;
    return 1;
}
#endif

int
settings_screen_draw(InbeApp *app)
{
    int top_margin = 0;
    int content_x;
    int content_w;

    int responsive_max_w = (int)(view_width * 0.96f);
    int max_content_w = flint_px(CONTENT_MAX_W);
    int min_content_w = flint_px(320);
    if(responsive_max_w > max_content_w)
        responsive_max_w = max_content_w;
    if(responsive_max_w < min_content_w)
        responsive_max_w = min_content_w;
    int side_padding = flint_page_side_padding();
    flint_centered_column(responsive_max_w, side_padding, &content_x, &content_w);

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        app->settings_drag_slider = 0;

    if(app->settings_tab < SETTINGS_TAB_DEVICE || app->settings_tab >= SETTINGS_TAB_COUNT)
        app->settings_tab = SETTINGS_TAB_DEVICE;

#if defined(INBE_HAS_FLINT_FILE_DIALOG)
    if(settings_draw_pending_file_dialog(app))
        return 1;
#endif

    int top_tab_h = flint_px(40);
    int top_tab_y = top_margin;
    int tab_gap = flint_px(14);
    int tab_content_start_y = top_tab_y + top_tab_h + tab_gap;
    int content_viewport_h = view_height - tab_content_start_y - flint_px(TAB_BAR_H);
    int clicked_top_tab = -1;
    const char *settings_tabs[] = {
        locale_get("settings_tab_device"),
        locale_get("settings_tab_theme"),
        locale_get("settings_tab_data"),
    };
    int language_menu_changed = 0;
    int draw_language_menu = 0;
    int draw_theme_mode_menu = 0;
    int draw_orientation_menu = 0;
    int theme_mode_changed = 0;
    int orientation_changed = 0;
    const char *theme_mode_options[] = {
        locale_get("theme_system"),
        locale_get("theme_light"),
        locale_get("theme_dark")
    };
    const char *orientation_options[] = {
        locale_get("orientation_system"),
        locale_get("orientation_portrait"),
        locale_get("orientation_landscape"),
        locale_get("orientation_sensor")
    };
    int orientation_option_count =
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
        4;
#else
        3;
#endif

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    settings_screen_handle_android_import(app);
#elif defined(PLATFORM_WEB)
    settings_screen_handle_web_import(app);
#endif

    if(content_viewport_h < 0)
        content_viewport_h = 0;

    clicked_top_tab = settings_draw_subtab_bar(top_tab_y, top_tab_h, settings_tabs,
                                               SETTINGS_TAB_COUNT, app->settings_tab);
    if(clicked_top_tab != -1) {
        app->settings_tab = clicked_top_tab;
        app->settings_scroll = 0;
        settings_screen_clear_status();
    }

    {
        int draw_w = content_w;
        int app_content_h;
        FlintUIScrollArea scroll_area;

        for(int pass = 0; pass < 3; pass++) {
            int planned_content_w = draw_w - flint_px(16);
            FlintUIScrollView measured;

            if(planned_content_w < flint_px(160))
                planned_content_w = draw_w;
            if(app->settings_tab == SETTINGS_TAB_THEME) {
                app_content_h = flint_px(76) + ui_theme_picker_height(planned_content_w) + flint_px(60);
            } else if(app->settings_tab == SETTINGS_TAB_DEVICE) {
                app_content_h = flint_px(74) + flint_px(74) + flint_px(76) + flint_px(40);
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
                app_content_h += flint_px(76);
#endif
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
                app_content_h += flint_px(50);
#endif
                app_content_h += flint_px(72);
            } else {
                app_content_h = settings_data_content_height(draw_w);
            }
            scroll_area = (FlintUIScrollArea){
                .bounds = {0.0f, (float)tab_content_start_y,
                           (float)view_width, (float)content_viewport_h},
                .content_height = app_content_h,
                .content_x = content_x,
                .content_width = content_w,
                .scroll_offset = &app->settings_scroll,
                .wheel_step = flint_px(42),
                .scrollbar_x = view_width - flint_px(8)
            };
            measured = ui_scroll_container_measure(scroll_area);
            if(measured.content_w == draw_w)
                break;
            draw_w = measured.content_w;
        }
        int y;
        FlintUIScrollView scroll_view = ui_scroll_container_begin(scroll_area);
        int draw_x = scroll_view.content_x;
        draw_w = scroll_view.content_w;
        y = scroll_view.content_y;

        if(app->settings_tab == SETTINGS_TAB_THEME) {

            app->theme_mode = clampi(app->theme_mode, APP_THEME_SYSTEM, APP_THEME_DARK);
            flint_text_draw(locale_get("theme_mode_label"), draw_x, y, flint_ui_font(), theme_get_text());
            ui_draw_dropdown_button(102, draw_x, y + flint_px(26), draw_w,
                                    flint_px(36), theme_mode_options, 3, &app->theme_mode);
            draw_theme_mode_menu = 1;
            y += flint_px(76);
            if(ui_draw_theme_picker(draw_x, y, draw_w, locale_get("theme_label"),
                                    app->dark_mode, &app->theme_id)) {
                app->theme_id = clampi(app->theme_id, 0, FLINT_THEME_COUNT - 1);
                app_refresh_theme(app);
                app->settings_dirty = 1;
                save_settings(app);
            }
            y += ui_theme_picker_height(draw_w) + flint_px(20);

        } else if(app->settings_tab == SETTINGS_TAB_DEVICE) {

            int sound_volume = app->sound_volume;
            int keyboard_toggle = app->on_screen_keyboard_enabled;
            int toggle_w = flint_px(56);
            int toggle_h = flint_px(30);

            if(language_dropdown_button(app, 101, draw_x, y, draw_w, flint_px(36), &app->language_index))
                language_menu_changed = 1;
            draw_language_menu = 1;
            y += flint_px(74);

            if(ui_draw_slider(6, draw_x, y, draw_w, locale_get("volume_label"),
                              SETTINGS_VOLUME_MIN, SETTINGS_VOLUME_MAX, &sound_volume, "")) {
                app->sound_volume = sound_volume;
                app->settings_dirty = 1;
                save_settings(app);
            }
            y += flint_px(74);
#ifdef __ANDROID__
            {
                int play_in_background = app->inbe.play_in_background;
                flint_text_draw(locale_get("play_in_background_label"), draw_x, y, flint_ui_font(), theme_get_text());
                if(ui_draw_toggle_switch(draw_x, y + flint_px(26), toggle_w, toggle_h,
                                         &play_in_background, locale_get("toggle_off"), locale_get("toggle_on"))) {
                    app->inbe.play_in_background = play_in_background;
                    app->settings_dirty = 1;
                }
                y += flint_px(76);
            }
#endif
            {
                int orientation_max = orientation_option_count - 1;
                app->orientation_mode = clampi(app->orientation_mode,
                                               APP_ORIENTATION_SYSTEM,
                                               orientation_max);
                flint_text_draw(locale_get("orientation_label"), draw_x, y,
                                flint_ui_font(), theme_get_text());
                ui_draw_dropdown_button(103, draw_x, y + flint_px(26), draw_w,
                                        flint_px(36), orientation_options, orientation_option_count,
                                        &app->orientation_mode);
                draw_orientation_menu = 1;
                y += flint_px(76);
            }
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
            if(ui_draw_checkbox_toggle(draw_x, y, locale_get("fullscreen_label"), &app->fullscreen_enabled)) {
                if(app->fullscreen_enabled && !IsWindowFullscreen())
                    ToggleFullscreen();
                else if(!app->fullscreen_enabled && IsWindowFullscreen())
                    ToggleFullscreen();
                app->settings_dirty = 1;
            }
            y += flint_px(50);
#endif
            flint_text_draw(locale_get("on_screen_keyboard_label"), draw_x, y, flint_ui_font(), theme_get_text());
            if(ui_draw_toggle_switch(draw_x, y + flint_px(26), toggle_w, toggle_h,
                                     &keyboard_toggle, locale_get("toggle_off"), locale_get("toggle_on"))) {
                app->on_screen_keyboard_enabled = keyboard_toggle;
                app->settings_dirty = 1;
            }
            y += flint_px(76);
        
        } else {

            int data_button_h = flint_px(36);
            int hover_import = 0;
            int hover_export = 0;
            int hover_delete = 0;

            if(ui_draw_generic_button(draw_x, y, draw_w, data_button_h,
                                      locale_get("import_data_button"),
                                      UI_BUTTON_STYLE_PRIMARY, 0, &hover_import))
                settings_import_data(app);
            y += data_button_h + flint_px(12);

            if(ui_draw_generic_button(draw_x, y, draw_w, data_button_h,
                                      locale_get("export_data_button"),
                                      UI_BUTTON_STYLE_PRIMARY, 0, &hover_export))
                settings_export_data(app);
            y += data_button_h + flint_px(12);

            if(ui_draw_generic_button(draw_x, y, draw_w, data_button_h,
                                      locale_get("delete_all_data_button"),
                                      UI_BUTTON_STYLE_DANGER, 0, &hover_delete))
                settings_request_delete_all_data(app);
            y += data_button_h + flint_px(12);
            settings_draw_status_reserved(draw_x, &y, flint_px(42));
            settings_draw_link_icons(app, draw_x, draw_w, &y);
            y += flint_px(8);
            settings_draw_version_centered(draw_x, draw_w, &y);
        }
        y += flint_px(40);
        ui_scroll_container_end(scroll_area, scroll_view);
    }

    ui_set_dropdown_clip_top(tab_content_start_y);
    if(draw_language_menu && language_dropdown_menu(app, 101))
        language_menu_changed = 1;
    if(draw_theme_mode_menu && ui_draw_dropdown_menu(102))
        theme_mode_changed = 1;
    if(draw_orientation_menu && ui_draw_dropdown_menu(103))
        orientation_changed = 1;
    if(language_menu_changed)
        apply_language_selection(app, app->language_index, 1);
    if(theme_mode_changed) {
        app->theme_mode = clampi(app->theme_mode, APP_THEME_SYSTEM, APP_THEME_DARK);
        app_refresh_theme(app);
        app->settings_dirty = 1;
        save_settings(app);
    }
    if(orientation_changed) {
        int orientation_max = orientation_option_count - 1;
        app->orientation_mode = clampi(app->orientation_mode, APP_ORIENTATION_SYSTEM,
                                       orientation_max);
        app_apply_orientation_preference(app);
        app->settings_dirty = 1;
        save_settings(app);
    }
    ui_set_dropdown_clip_top(0);

    if(app->modal.active && app->modal.type == UIModalConfirmImportDataSettings) {
        int modal_result = settings_draw_import_choice_modal(app);
        if(modal_result == 1) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
            settings_clear_pending_import();
            settings_screen_set_status_error(locale_get("import_cancelled"));
        } else if(modal_result == 2 || modal_result == 3) {
            char import_path[FS_PATH_MAX];
            DataImportMode mode = modal_result == 3
                                      ? DATA_IMPORT_DATA_AND_SETTINGS
                                      : DATA_IMPORT_DATA_ONLY;
            snprintf(import_path, sizeof(import_path), "%s", pending_import_path);
            app->modal.active = 0;
            app->modal.type = UIModalNone;
            settings_clear_pending_import();
            settings_perform_import(app, import_path, mode);
        }
    }

    if(app->modal.active && app->modal.type == UIModalConfirmDeleteData) {
        int modal_result = ui_draw_modal(locale_get("delete_all_data_title"),
                                         locale_get("delete_all_data_message"),
                                         locale_get("cancel_button"),
                                         locale_get("delete_button"));
        if(modal_result == 1) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
            settings_screen_set_status_error(locale_get("delete_cancelled"));
        } else if(modal_result == 2) {
            long long deleted = data_delete_all();
            app->modal.active = 0;
            app->modal.type = UIModalNone;
            if(deleted > 0) {
                char deleted_message[128];
                inbe_habits_add_default_set(&app->habits);
                locale_format(deleted_message, sizeof(deleted_message), "deleted_sessions", deleted);
                settings_screen_set_status_success(deleted_message, NULL);
            } else {
                int cleared = inbe_habits_clear_days(&app->habits);
                if(cleared > 0) {
                    char deleted_message[128];
                    inbe_habits_save(&app->habits);
                    locale_format(deleted_message, sizeof(deleted_message), "deleted_sessions", cleared);
                    settings_screen_set_status_success(deleted_message, NULL);
                } else {
                    settings_screen_set_status_error(locale_get("no_data_to_delete"));
                }
            }
        }
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if(app->settings_dirty)
            save_settings(app);
    }

    return 0;
}
