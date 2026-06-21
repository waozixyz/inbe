#include "settings_sync_account.h"

#include "settings_screen.h"
#include "app.h"
#include "flint_locale.h"
#include "storage.h"
#include "sync_account.h"
#include "sync_client.h"
#include "flint_theme.h"
#include "flint_ui.h"
#include "raylib.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define INBE_SYNC_SERVER_URL_KEY "sync_server_url"
#define INBE_SYNC_SERVER_URL_DEFAULT "https://api.waozi.xyz"

static SettingsSyncKeySaveDialog save_dialog_callback = NULL;
static SettingsSyncKeyImportDialog import_dialog_callback = NULL;

void
settings_sync_account_set_save_dialog(SettingsSyncKeySaveDialog callback)
{
    save_dialog_callback = callback;
}

void
settings_sync_account_set_import_dialog(SettingsSyncKeyImportDialog callback)
{
    import_dialog_callback = callback;
}

static void
settings_draw_public_id_field(const char *text, int x, int w, int *y, int font,
                              FlintUITextInputStyle style)
{
    char line[128];
    int text_len;
    int pad_x;
    int pad_y = flint_px(8);
    int line_h = flint_text_line_height(font);
    int content_w;
    int line_count = 0;
    int offset = 0;
    int field_h;
    int draw_y;

    if(text == NULL || y == NULL)
        return;

    text_len = (int)strlen(text);
    pad_x = style.padding_x > 0 ? style.padding_x : flint_px(10);
    content_w = w - pad_x * 2;
    if(content_w < flint_px(24))
        content_w = flint_px(24);

    while(offset < text_len) {
        int len;

        if(flint_text_measure(text + offset, font) <= content_w) {
            offset = text_len;
            line_count++;
            break;
        }

        len = 1;
        while(offset + len < text_len && len + 1 < (int)sizeof(line)) {
            snprintf(line, sizeof(line), "%.*s", len + 1, text + offset);
            if(flint_text_measure(line, font) > content_w)
                break;
            len++;
        }
        offset += len;
        line_count++;
    }
    if(line_count < 1)
        line_count = 1;

    field_h = line_count * line_h + pad_y * 2;
    DrawRectangleRounded((Rectangle){(float)x, (float)*y, (float)w, (float)field_h},
                         style.radius > 0.0f ? style.radius : 0.08f, 8,
                         style.background);
    DrawRectangleRoundedLines((Rectangle){(float)x, (float)*y, (float)w, (float)field_h},
                              style.radius > 0.0f ? style.radius : 0.08f, 8,
                              style.border);

    offset = 0;
    draw_y = *y + pad_y;
    while(offset < text_len) {
        int len;

        if(flint_text_measure(text + offset, font) <= content_w) {
            snprintf(line, sizeof(line), "%s", text + offset);
            flint_text_draw(line, x + pad_x, draw_y, font, style.text);
            break;
        }

        len = 1;
        while(offset + len < text_len && len + 1 < (int)sizeof(line)) {
            snprintf(line, sizeof(line), "%.*s", len + 1, text + offset);
            if(flint_text_measure(line, font) > content_w)
                break;
            len++;
        }
        snprintf(line, sizeof(line), "%.*s", len, text + offset);
        flint_text_draw(line, x + pad_x, draw_y, font, style.text);
        draw_y += line_h;
        offset += len;
    }

    *y += field_h;
}

static void
settings_backup_filename(char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "inbe-sync.key");
}

static int
settings_start_sync_key_export(InbeApp *app, const InbeSyncAccount *account)
{
    char filename[64];

    settings_backup_filename(filename, sizeof(filename));
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID) || defined(PLATFORM_WEB)
    (void)app;
    return sync_account_export_private_key(account, filename) ? 2 : 3;
#else
    if(save_dialog_callback != NULL)
        return save_dialog_callback(app, filename);
    return sync_account_export_private_key(account, filename) ? 2 : 3;
#endif
}

static int
settings_start_sync_key_import(InbeApp *app)
{
    if(import_dialog_callback != NULL)
        return import_dialog_callback(app);
    return 0;
}

static int
settings_sync_url_filter(int codepoint, void *user_data)
{
    (void)user_data;
    return codepoint >= 33 && codepoint <= 126 && !isspace(codepoint);
}

static int
settings_sync_server_normalize(InbeApp *app, char *out, size_t out_size)
{
    if(app == NULL || out == NULL || out_size == 0)
        return 0;
    if(!sync_client_normalize_url(app->sync_server_url, out, out_size))
        return 0;
    snprintf(app->sync_server_url, sizeof(app->sync_server_url), "%s", out);
    app->sync_server_url_cursor = (int)strlen(app->sync_server_url);
    return 1;
}

static void
settings_sync_server_save(InbeApp *app)
{
    char url[sizeof(app->sync_server_url)];

    if(app == NULL)
        return;
    if(!settings_sync_server_normalize(app, url, sizeof(url))) {
        settings_screen_set_status_error(locale_get("sync_server_url_invalid"));
        return;
    }
    storage_set_setting_text(INBE_SYNC_SERVER_URL_KEY, url);
    settings_screen_set_status_success(locale_get("sync_server_saved"), url);
}

static int
settings_sync_server_connected(InbeApp *app)
{
    char url[sizeof(app->sync_server_url)];
    const char *saved_url;
    char saved_normalized[sizeof(app->sync_server_url)];
    InbeSyncAccount account;

    if(app == NULL || !sync_account_load(&account))
        return 0;
    if(!sync_client_normalize_url(app->sync_server_url, url, sizeof(url)))
        return 0;
    saved_url = storage_get_setting_text(INBE_SYNC_SERVER_URL_KEY);
    if(!sync_client_normalize_url(saved_url, saved_normalized, sizeof(saved_normalized)))
        return 0;
    return strcmp(saved_normalized, url) == 0;
}

static const char *
settings_sync_result_key(InbeSyncClientResult result)
{
    switch(result) {
        case INBE_SYNC_CLIENT_OK:
            return "sync_connected";
        case INBE_SYNC_CLIENT_INVALID_URL:
            return "sync_server_url_invalid";
        case INBE_SYNC_CLIENT_NO_ACCOUNT:
            return "sync_no_account";
        case INBE_SYNC_CLIENT_AUTH_FAILED:
            return "sync_auth_failed";
        case INBE_SYNC_CLIENT_PAYLOAD_FAILED:
        case INBE_SYNC_CLIENT_CHALLENGE_FAILED:
        case INBE_SYNC_CLIENT_SIGN_FAILED:
        case INBE_SYNC_CLIENT_REQUEST_FAILED:
        default:
            return "sync_failed";
    }
}

static void
settings_sync_run_connect(InbeApp *app)
{
    InbeSyncClientResult result;
    char url[sizeof(app->sync_server_url)];

    if(app == NULL)
        return;
    if(!settings_sync_server_normalize(app, url, sizeof(url))) {
        settings_screen_set_status_error(locale_get("sync_server_url_invalid"));
        return;
    }
    storage_set_setting_text(INBE_SYNC_SERVER_URL_KEY, url);
    result = sync_client_sync(url);
    if(result == INBE_SYNC_CLIENT_OK) {
        app_reload_after_import(app, 0);
        settings_screen_set_status_success(locale_get(settings_sync_result_key(result)), NULL);
    } else {
        settings_screen_set_status_error(locale_get(settings_sync_result_key(result)));
    }
}

void
settings_sync_account_delete_confirmed(InbeApp *app)
{
    InbeSyncAccount account;
    InbeSyncClientResult result;
    char url[sizeof(app->sync_server_url)];

    if(app == NULL)
        return;
    if(!sync_account_load(&account)) {
        settings_screen_set_status_error(locale_get("sync_no_account"));
        return;
    }

    if(settings_sync_server_normalize(app, url, sizeof(url))) {
        result = sync_client_delete_account(url);
        if(result != INBE_SYNC_CLIENT_OK)
            TraceLog(LOG_WARNING, "SYNC: remote account delete failed: %d", result);
    } else {
        TraceLog(LOG_WARNING, "SYNC: remote account delete skipped due to invalid URL");
    }

    sync_account_clear();
    settings_screen_set_status_success(locale_get("sync_account_deleted"), NULL);
}

static FlintUITextInputStyle
settings_sync_text_style(void)
{
    return (FlintUITextInputStyle){
        .background = flint_darken(flint_theme_get_bg(), 4),
        .border = flint_theme_get_button(),
        .focus_border = flint_theme_get_button_hover(),
        .text = flint_theme_get_text(),
        .cursor = flint_theme_get_text(),
        .radius = 0.08f,
        .padding_x = flint_px(10)
    };
}

void
settings_sync_account_draw(InbeApp *app, int x, int w, int *y)
{
    InbeSyncAccount account;
    int has_account = sync_account_load(&account);
    int font = flint_ui_font();
    int small_font = flint_ui_font_small();
    int btn_h = flint_px(36);
    int gap = flint_px(10);
    int hover = 0;

    flint_text_draw(locale_get("sync_account_title"), x, *y, font, flint_theme_get_text());
    *y += flint_px(26);

    if(!sync_account_available()) {
        flint_text_draw(locale_get("sync_liboqs_unavailable"), x, *y, small_font,
                        flint_darken(flint_theme_get_text(), 35));
        *y += flint_px(32);
        return;
    }

    if(!has_account) {
        if(ui_draw_generic_button(x, *y, w, btn_h, locale_get("sync_create_account_button"),
                                  UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
            if(sync_account_create(&account)) {
                settings_screen_set_status_success(locale_get("sync_account_created"), NULL);
                app->modal.active = 1;
                app->modal.type = UIModalSyncAccountBackup;
                app->modal.selected_button = 0;
            } else {
                settings_screen_set_status_error(locale_get("sync_account_create_failed"));
            }
        }
        *y += btn_h + flint_px(18);
        return;
    }

    flint_text_draw(locale_get("sync_public_id_label"), x, *y, small_font,
                    flint_darken(flint_theme_get_text(), 30));
    *y += flint_px(18);
    settings_draw_public_id_field(account.public_id, x, w, y, small_font,
                                  settings_sync_text_style());
    *y += flint_px(4);

    {
        FlintUIButtonRowItem buttons[2] = {
            {locale_get("sync_copy_id_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {locale_get("sync_backup_key_button"), UI_BUTTON_STYLE_PRIMARY, 0}
        };
        int clicked = ui_draw_button_row((FlintUIButtonRow){
            .x = x,
            .y = *y,
            .width = w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 2
        });

        if(clicked == 0) {
            SetClipboardText(account.public_id);
            settings_screen_set_status_success(locale_get("sync_public_id_copied"), NULL);
        } else if(clicked == 1) {
            app->modal.active = 1;
            app->modal.type = UIModalSyncAccountBackup;
            app->modal.selected_button = 0;
        }
    }
    *y += btn_h + flint_px(16);
}

void
settings_sync_account_draw_config(InbeApp *app, int x, int w, int *y)
{
    InbeSyncAccount account;
    int has_account = sync_account_load(&account);
    int font = flint_ui_font();
    int small_font = flint_ui_font_small();
    int btn_h = flint_px(36);
    int gap = flint_px(10);
    int hover = 0;
    int commit = 0;
    FlintUITextInputStyle input_style = settings_sync_text_style();

    if(!sync_account_available()) {
        flint_text_draw(locale_get("sync_liboqs_unavailable"), x, *y, small_font,
                        flint_darken(flint_theme_get_text(), 35));
        *y += flint_px(32);
        return;
    }

    if(!has_account) {
        FlintUIButtonRowItem buttons[2] = {
            {locale_get("sync_create_account_button"), UI_BUTTON_STYLE_PRIMARY, 0},
            {locale_get("sync_import_key_button"), UI_BUTTON_STYLE_PRIMARY, 0}
        };
        int clicked;

        *y += flint_px(12);
        clicked = ui_draw_button_row((FlintUIButtonRow){
            .x = x,
            .y = *y,
            .width = w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 2
        });
        if(clicked == 0) {
            if(sync_account_create(&account)) {
                has_account = 1;
                settings_screen_set_status_success(locale_get("sync_account_created"), NULL);
                app->modal.active = 1;
                app->modal.type = UIModalSyncAccountBackup;
                app->modal.selected_button = 0;
            } else {
                settings_screen_set_status_error(locale_get("sync_account_create_failed"));
            }
        } else if(clicked == 1) {
            if(!settings_start_sync_key_import(app))
                settings_screen_set_status_error(locale_get("sync_private_key_import_failed"));
        }
        *y += btn_h + flint_px(18);
    }

    if(!has_account) {
        settings_screen_draw_status_reserved(x, y, flint_px(42));
        (void)font;
        return;
    }

    flint_text_draw(locale_get("sync_public_id_label"), x, *y, small_font,
                    flint_darken(flint_theme_get_text(), 30));
    *y += flint_px(18);
    settings_draw_public_id_field(account.public_id, x, w, y, small_font, input_style);
    *y += flint_px(8);

    flint_text_draw(locale_get("sync_remote_label"), x, *y, small_font,
                    flint_darken(flint_theme_get_text(), 30));
    *y += flint_px(18);
    flint_ui_text_field((FlintUITextField){
        .bounds = {(float)x, (float)*y, (float)w, (float)btn_h},
        .text = app->sync_server_url,
        .text_size = sizeof(app->sync_server_url),
        .cursor_position = &app->sync_server_url_cursor,
        .focused = &app->sync_server_url_focused,
        .max_codepoints = 255,
        .font = small_font,
        .style = input_style,
        .filter = settings_sync_url_filter,
        .commit_pressed = &commit
    });
    if(commit)
        settings_sync_server_save(app);
    *y += btn_h + flint_px(12);

    if(ui_draw_generic_button(x, *y, w, btn_h,
                              settings_sync_server_connected(app)
                                  ? locale_get("sync_connected_button")
                                  : locale_get("sync_connect_button"),
                              UI_BUTTON_STYLE_PRIMARY, 0, &hover))
        settings_sync_run_connect(app);
    *y += btn_h + flint_px(12);

    {
        FlintUIButtonRowItem buttons[2] = {
            {locale_get("sync_copy_id_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {locale_get("sync_backup_key_button"), UI_BUTTON_STYLE_SECONDARY, 0}
        };
        int clicked = ui_draw_button_row((FlintUIButtonRow){
            .x = x,
            .y = *y,
            .width = w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 2
        });

        if(clicked == 0) {
            SetClipboardText(account.public_id);
            settings_screen_set_status_success(locale_get("sync_public_id_copied"), NULL);
        } else if(clicked == 1) {
            app->modal.active = 1;
            app->modal.type = UIModalSyncAccountBackup;
            app->modal.selected_button = 0;
        }
    }
    *y += btn_h + flint_px(12);

    if(ui_draw_generic_button(x, *y, w, btn_h, locale_get("sync_delete_account_button"),
                              UI_BUTTON_STYLE_DANGER, 0, &hover)) {
        app->modal.active = 1;
        app->modal.type = UIModalConfirmDeleteSyncAccount;
        app->modal.selected_button = 0;
    }
    *y += btn_h + flint_px(12);

    settings_screen_draw_status_reserved(x, y, flint_px(42));
    (void)font;
}

int
settings_sync_account_draw_backup_modal(InbeApp *app)
{
    FlintUIPanelFrame frame;
    InbeSyncAccount account;
    int btn_h = flint_px(36);
    int gap = flint_px(10);
    int pad_bottom = flint_px(24);
    int text_button_gap = flint_px(40);
    int hover = 0;
    int y;
    int modal_h;
    int button_y;
    FlintUIParagraph warning;

    memset(&account, 0, sizeof(account));
    sync_account_load(&account);

    warning = (FlintUIParagraph){
        .text = locale_get("sync_backup_warning"),
        .width = flint_px(336) - flint_px(36)
    };
    modal_h = flint_px(58) +
              flint_ui_paragraph_height(warning) +
              text_button_gap +
              btn_h * 2 + gap +
              pad_bottom;
    if(modal_h < flint_px(224))
        modal_h = flint_px(224);

    frame = ui_draw_modal_frame(flint_px(336), modal_h, locale_get("sync_backup_title"),
                                (Texture2D){0}, (Texture2D){0});
    y = frame.content_y;
    warning.width = frame.content_w;
    flint_ui_paragraph_draw(warning, frame.content_x, &y);

    button_y = y + text_button_gap;
    if(button_y + btn_h * 2 + gap + pad_bottom > frame.y + frame.h)
        button_y = frame.y + frame.h - pad_bottom - btn_h * 2 - gap;
    if(ui_draw_generic_button(frame.content_x, button_y, frame.content_w, btn_h,
                              locale_get("sync_save_key_file_button"), UI_BUTTON_STYLE_PRIMARY, 0, &hover))
        return settings_start_sync_key_export(app, &account);
    if(ui_draw_generic_button(frame.content_x, button_y + btn_h + gap, frame.content_w,
                              btn_h, locale_get("close_button"), UI_BUTTON_STYLE_SECONDARY, 0, &hover))
        return 1;
    return 0;
}
