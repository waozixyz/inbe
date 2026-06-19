#include "settings_data.h"

#include "settings_screen.h"
#include "settings_sync_account.h"
#include "app.h"
#include "data.h"
#include "locale.h"
#include "storage.h"
#include "sync_account.h"
#include "theme.h"
#include "version.h"
#include "flint_theme_meta.h"
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
#include "raylib.h"
#include <stdio.h>
#include <string.h>

enum {
    SETTINGS_DATA_ACTION_NONE = 0,
    SETTINGS_DATA_ACTION_IMPORT,
    SETTINGS_DATA_ACTION_EXPORT,
    SETTINGS_DATA_ACTION_SYNC_KEY_EXPORT,
    SETTINGS_DATA_ACTION_SYNC_KEY_IMPORT
};

enum {
    SETTINGS_DATA_VIEW_MAIN = 0,
    SETTINGS_DATA_VIEW_SYNC_ACCOUNT = 1
};

#define INBE_SYNC_SERVER_URL_KEY "sync_server_url"
#define INBE_SYNC_SERVER_URL_DEFAULT "https://api.waozi.xyz"

#if defined(INBE_HAS_FLINT_FILE_DIALOG)
static FlintFileDialog export_dlg;
static FlintFileDialog import_dlg;
static int data_file_dialog_action = SETTINGS_DATA_ACTION_NONE;
#endif

static char pending_import_path[FS_PATH_MAX] = "";
static DataImportInfo pending_import_info;

static void settings_import_data(InbeApp *app);
static void settings_export_data(InbeApp *app);
static void settings_request_delete_all_data(InbeApp *app);
static void settings_sync_server_load(InbeApp *app);
static int settings_import_sync_key_path(InbeApp *app, const char *path);

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

static int
settings_link_icon_columns(int content_w)
{
    int max_columns = 5;
    int min_columns = 2;
    int icon_size = flint_px(32);
    int icon_padding = flint_px(4);
    int icon_spacing = flint_px(20);
    int icon_btn_w = icon_size + icon_padding * 2;

    for(int columns = max_columns; columns > min_columns; columns--) {
        int total_w = icon_btn_w * columns + icon_spacing * (columns - 1);
        if(total_w <= content_w)
            return columns;
    }

    return min_columns;
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

int
settings_data_content_height(int content_w)
{
    int data_button_h = flint_px(36);

    return data_button_h + flint_px(12) +
           data_button_h + flint_px(12) +
           data_button_h + flint_px(12) +
           data_button_h + flint_px(12) +
           flint_px(42) +
           settings_link_icons_height(content_w) +
           flint_px(8) + flint_px(22) +
           flint_px(40);
}

int
settings_data_is_configuring(const InbeApp *app)
{
    return app != NULL && app->settings_tab == SETTINGS_TAB_DATA &&
           app->settings_data_view == SETTINGS_DATA_VIEW_SYNC_ACCOUNT;
}

static void
settings_sync_server_load(InbeApp *app)
{
    const char *saved;

    if(app == NULL)
        return;
    saved = inbe_storage_get_setting_text(INBE_SYNC_SERVER_URL_KEY);
    snprintf(app->sync_server_url, sizeof(app->sync_server_url), "%s",
             saved != NULL && saved[0] != '\0'
                 ? saved
                 : INBE_SYNC_SERVER_URL_DEFAULT);
    app->sync_server_url_cursor = (int)strlen(app->sync_server_url);
}

static void
settings_open_sync_account_config(InbeApp *app)
{
    if(app == NULL)
        return;
    settings_sync_server_load(app);
    app->settings_data_view = SETTINGS_DATA_VIEW_SYNC_ACCOUNT;
    app->settings_scroll = 0;
    app->sync_server_url_focused = 0;
    settings_screen_clear_status();
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

static int
settings_start_sync_key_export_dialog(InbeApp *app, const char *filename)
{
#if defined(INBE_HAS_FLINT_FILE_DIALOG)
    settings_apply_file_dialog_theme(app);
    flint_file_dialog_begin_save(&export_dlg, locale_get("sync_save_key_dialog_title"), filename);
    data_file_dialog_action = SETTINGS_DATA_ACTION_SYNC_KEY_EXPORT;
    return 4;
#else
    (void)app;
    (void)filename;
    return 0;
#endif
}

static int
settings_start_sync_key_import_dialog(InbeApp *app)
{
#if defined(PLATFORM_WEB)
    (void)app;
    EM_ASM({
        const importPath = UTF8ToString($0);
        Module.__inbeSyncKeyImportResult = 0;

        const input = document.createElement("input");
        input.type = "file";
        input.accept = ".key,text/plain,application/octet-stream";
        input.style.display = "none";

        input.onchange = async function() {
            try {
                if(!input.files || input.files.length === 0) {
                    Module.__inbeSyncKeyImportResult = 2;
                    return;
                }

                const file = input.files[0];
                const bytes = new Uint8Array(await file.arrayBuffer());
                try {
                    FS.mkdir("/tmp");
                } catch(e) {}
                FS.writeFile("/tmp/inbe-sync-key-import.key", bytes);
                Module.__inbeSyncKeyImportResult = 1;
            } catch(e) {
                console.error("Inbe sync key import failed:", e);
                Module.__inbeSyncKeyImportResult = 3;
            } finally {
                input.remove();
            }
        };

        document.body.appendChild(input);
        input.click();
    }, "/tmp/inbe-sync-key-import.key");
    settings_screen_set_status_success(locale_get("sync_import_key_dialog_title"), NULL);
    return 1;
#elif defined(INBE_HAS_FLINT_FILE_DIALOG)
    settings_apply_file_dialog_theme(app);
    flint_file_dialog_begin_load(&import_dlg, locale_get("sync_import_key_dialog_title"));
    data_file_dialog_action = SETTINGS_DATA_ACTION_SYNC_KEY_IMPORT;
    return 1;
#else
    (void)app;
    return 0;
#endif
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

static void
settings_draw_version_centered(int x, int w, int *y)
{
    char version_text[32];
    int font = flint_ui_font_small();
    int text_w;

    snprintf(version_text, sizeof(version_text), "v%s", INBE_VERSION_STRING);
    text_w = flint_text_measure(version_text, font);
    flint_text_draw(version_text, x + (w - text_w) / 2, *y, font,
                    flint_darken(theme_get_text(), 40));
    *y += flint_px(22);
}

void
settings_data_draw(InbeApp *app, int x, int w, int *y)
{
    int data_button_h = flint_px(36);
    int hover_account = 0;
    int hover_import = 0;
    int hover_export = 0;
    int hover_delete = 0;

    settings_sync_account_set_save_dialog(settings_start_sync_key_export_dialog);
    settings_sync_account_set_import_dialog(settings_start_sync_key_import_dialog);
    if(app != NULL && app->settings_data_view == SETTINGS_DATA_VIEW_SYNC_ACCOUNT) {
        settings_sync_account_draw_config(app, x, w, y);
        return;
    }

    if(ui_draw_generic_button(x, *y, w, data_button_h,
                              locale_get("sync_configure_account_button"),
                              UI_BUTTON_STYLE_PRIMARY, 0, &hover_account))
        settings_open_sync_account_config(app);
    *y += data_button_h + flint_px(12);

    if(ui_draw_generic_button(x, *y, w, data_button_h,
                              locale_get("import_data_button"),
                              UI_BUTTON_STYLE_PRIMARY, 0, &hover_import))
        settings_import_data(app);
    *y += data_button_h + flint_px(12);

    if(ui_draw_generic_button(x, *y, w, data_button_h,
                              locale_get("export_data_button"),
                              UI_BUTTON_STYLE_PRIMARY, 0, &hover_export))
        settings_export_data(app);
    *y += data_button_h + flint_px(12);

    if(ui_draw_generic_button(x, *y, w, data_button_h,
                              locale_get("delete_all_data_button"),
                              UI_BUTTON_STYLE_DANGER, 0, &hover_delete))
        settings_request_delete_all_data(app);
    *y += data_button_h + flint_px(12);
    settings_screen_draw_status_reserved(x, y, flint_px(42));
    settings_draw_link_icons(app, x, w, y);
    *y += flint_px(8);
    settings_draw_version_centered(x, w, y);
}

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
void
settings_data_handle_android_import(InbeApp *app)
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
#else
void settings_data_handle_android_import(InbeApp *app) { (void)app; }
#endif

#if defined(PLATFORM_WEB)
#define SETTINGS_WEB_IMPORT_PATH "/tmp/inbe-web-import.zip"
#define SETTINGS_WEB_SYNC_KEY_IMPORT_PATH "/tmp/inbe-sync-key-import.key"

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

void
settings_data_handle_web_import(InbeApp *app)
{
    int import_result = EM_ASM_INT({
        const result = Module.__inbeImportResult || 0;
        if(result !== 0)
            Module.__inbeImportResult = 0;
        return result;
    });

    if(import_result == 2) {
        settings_screen_set_status_error(locale_get("import_cancelled"));
    } else if(import_result != 0 && import_result != 1) {
        settings_screen_set_status_error(locale_get("import_failed"));
    } else if(import_result == 1) {
        settings_begin_import_for_path(app, SETTINGS_WEB_IMPORT_PATH);
    }

    import_result = EM_ASM_INT({
        const result = Module.__inbeSyncKeyImportResult || 0;
        if(result !== 0)
            Module.__inbeSyncKeyImportResult = 0;
        return result;
    });

    if(import_result == 0)
        return;
    if(import_result == 2) {
        settings_screen_set_status_error(locale_get("sync_private_key_import_cancelled"));
        return;
    }
    if(import_result != 1) {
        settings_screen_set_status_error(locale_get("sync_private_key_import_failed"));
        return;
    }
    settings_import_sync_key_path(app, SETTINGS_WEB_SYNC_KEY_IMPORT_PATH);
}
#else
void settings_data_handle_web_import(InbeApp *app) { (void)app; }
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

static int
settings_import_sync_key_path(InbeApp *app, const char *path)
{
    InbeSyncAccount account;

    if(path != NULL && path[0] != '\0' &&
       inbe_sync_account_import_private_key(&account, path)) {
        settings_screen_set_status_success(locale_get("sync_private_key_imported"), NULL);
        TraceLog(LOG_INFO, "SYNC: Private key imported from %s", path);
        (void)app;
        return 1;
    }

    settings_screen_set_status_error(locale_get("sync_private_key_import_failed"));
    TraceLog(LOG_ERROR, "SYNC: Private key import failed");
    (void)app;
    return 0;
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
int
settings_data_draw_pending_file_dialog(InbeApp *app)
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
    } else if(data_file_dialog_action == SETTINGS_DATA_ACTION_EXPORT) {
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
    } else if(data_file_dialog_action == SETTINGS_DATA_ACTION_SYNC_KEY_EXPORT) {
        if(result == 1) {
            InbeSyncAccount account;
            const char *path = flint_file_dialog_get_path(&export_dlg);
            if(path != NULL && path[0] != '\0' &&
               inbe_sync_account_load(&account) &&
               inbe_sync_account_export_private_key(&account, path)) {
                settings_screen_set_status_success(locale_get("sync_private_key_backup_saved"), GetFileName(path));
                TraceLog(LOG_INFO, "SYNC: Private key backup saved to %s", path);
            } else {
                settings_screen_set_status_error(locale_get("sync_private_key_backup_failed"));
                TraceLog(LOG_ERROR, "SYNC: Private key backup failed");
            }
        } else {
            settings_screen_set_status_error(locale_get("sync_private_key_backup_cancelled"));
        }
    } else if(data_file_dialog_action == SETTINGS_DATA_ACTION_SYNC_KEY_IMPORT) {
        if(result == 1) {
            const char *path = flint_file_dialog_get_path(&import_dlg);
            settings_import_sync_key_path(app, path);
        } else {
            settings_screen_set_status_error(locale_get("sync_private_key_import_cancelled"));
        }
    }

    data_file_dialog_action = SETTINGS_DATA_ACTION_NONE;
    return 1;
}
#else
int settings_data_draw_pending_file_dialog(InbeApp *app) { (void)app; return 0; }
#endif

int
settings_data_draw_modals(InbeApp *app)
{
    if(app == NULL || !app->modal.active)
        return 0;

    if(app->modal.type == UIModalConfirmImportDataSettings) {
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
        return 1;
    }

    if(app->modal.type == UIModalConfirmDeleteData) {
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
                locale_format(deleted_message, sizeof(deleted_message),
                              "deleted_sessions", deleted);
                settings_screen_set_status_success(deleted_message, NULL);
            } else {
                int cleared = inbe_habits_clear_days(&app->habits);
                if(cleared > 0) {
                    char deleted_message[128];
                    inbe_habits_save(&app->habits);
                    locale_format(deleted_message, sizeof(deleted_message),
                                  "deleted_sessions", cleared);
                    settings_screen_set_status_success(deleted_message, NULL);
                } else {
                    settings_screen_set_status_error(locale_get("no_data_to_delete"));
                }
            }
        }
        return 1;
    }

    if(app->modal.type == UIModalConfirmDeleteSyncAccount) {
        int modal_result = ui_draw_modal(locale_get("sync_delete_account_title"),
                                         locale_get("sync_delete_account_message"),
                                         locale_get("cancel_button"),
                                         locale_get("delete_button"));
        if(modal_result == 1) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
            settings_screen_set_status_error(locale_get("sync_account_delete_cancelled"));
        } else if(modal_result == 2) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
            settings_sync_account_delete_confirmed(app);
        }
        return 1;
    }

    if(app->modal.type == UIModalSyncAccountBackup) {
        int modal_result = settings_sync_account_draw_backup_modal(app);
        if(modal_result == 1 || modal_result == 2 || modal_result == 3 || modal_result == 4) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
            if(modal_result == 2)
                settings_screen_set_status_success(locale_get("sync_private_key_backup_saved"), NULL);
            else if(modal_result == 3)
                settings_screen_set_status_error(locale_get("sync_private_key_backup_failed"));
        }
        return 1;
    }

    return 0;
}
