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
#define INBE_SYNC_ACCOUNT_ALIAS_KEY "sync_account_alias"

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

static int
settings_draw_public_id_field(const char *text, int x, int w, int *y, int font,
                              FlintUITextInputStyle style)
{
    int field_h;
    int clicked = 0;

    if(text == NULL || y == NULL)
        return 0;

    field_h = flint_ui_readonly_text_box_height(text, font, w, style, 0);
    clicked = flint_ui_readonly_text_box((FlintUIReadonlyTextBox){
        .bounds = {(float)x, (float)*y, (float)w, (float)field_h},
        .text = text,
        .font = font,
        .style = style
    });
    *y += field_h;
    return clicked;
}

static void
settings_compact_public_id(const char *public_id, char *out, size_t out_size)
{
    size_t len;

    if(out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    if(public_id == NULL)
        return;
    len = strlen(public_id);
    if(len <= 12) {
        snprintf(out, out_size, "%s", public_id);
        return;
    }
    snprintf(out, out_size, "%.*s...%.*s", 4, public_id, 4, public_id + len - 4);
}

static void
settings_backup_filename(char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "account.key");
}

static int
settings_start_sync_key_export(InbeApp *app, const InbeSyncAccount *account)
{
    char filename[64];

    settings_backup_filename(filename, sizeof(filename));
#if ANDROID_BUILD || defined(PLATFORM_WEB)
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
settings_sync_alias_filter(int codepoint, void *user_data)
{
    (void)user_data;
    return (codepoint >= 'a' && codepoint <= 'z') ||
           (codepoint >= 'A' && codepoint <= 'Z') ||
           (codepoint >= '0' && codepoint <= '9') ||
           codepoint == '_';
}

static void
settings_sync_alias_normalize(char *text)
{
    char out[40];
    int n = 0;

    if(text == NULL)
        return;
    for(int i = 0; text[i] != '\0' && n < (int)sizeof(out) - 1; i++) {
        unsigned char ch = (unsigned char)text[i];
        if(ch == '@' && n == 0)
            continue;
        if(ch >= 'A' && ch <= 'Z')
            ch = (unsigned char)(ch - 'A' + 'a');
        if((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_')
            out[n++] = (char)ch;
    }
    out[n] = '\0';
    snprintf(text, 40, "%s", out);
}

static int
settings_sync_alias_valid(const char *text)
{
    size_t len;

    if(text == NULL)
        return 0;
    len = strlen(text);
    if(len < 4 || len > 32)
        return 0;
    for(size_t i = 0; i < len; i++) {
        char ch = text[i];
        if((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_')
            continue;
        return 0;
    }
    return 1;
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

static void
settings_sync_open_alias_modal(InbeApp *app, int then_backup)
{
    const char *alias;

    if(app == NULL)
        return;
    alias = storage_get_setting_text(INBE_SYNC_ACCOUNT_ALIAS_KEY);
    snprintf(app->sync_alias_input, sizeof(app->sync_alias_input), "%s",
             alias != NULL ? alias : "");
    settings_sync_alias_normalize(app->sync_alias_input);
    app->sync_alias_cursor = (int)strlen(app->sync_alias_input);
    app->sync_alias_focused = 1;
    app->sync_alias_then_backup = then_backup;
    app_open_modal(app, UIModalSyncAlias);
}

static void
settings_sync_draw_account_id(InbeApp *app, const InbeSyncAccount *account,
                              int x, int w, int *y, int small_font,
                              FlintUITextInputStyle style)
{
    const char *alias = storage_get_setting_text(INBE_SYNC_ACCOUNT_ALIAS_KEY);
    char display[96];

    if(account == NULL)
        return;
    if(alias != NULL && alias[0] != '\0')
        snprintf(display, sizeof(display), "@%s", alias);
    else
        settings_compact_public_id(account->public_id, display, sizeof(display));
    if(settings_draw_public_id_field(display, x, w, y, small_font, style) && app != NULL)
        app_open_modal(app, UIModalSyncPublicId);
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
        if(storage_sync_review_clear_if_no_visible_diff()) {
            app_reload_after_import(app, 0);
            settings_screen_set_status_success(locale_get(settings_sync_result_key(result)), NULL);
        } else if(storage_sync_review_apply_remote_if_local_empty()) {
            app_reload_after_import(app, 0);
            settings_screen_set_status_success(locale_get(settings_sync_result_key(result)), NULL);
        } else if(storage_sync_review_pending()) {
            app_open_modal(app, UIModalSyncReview);
            settings_screen_set_status_error("Sync needs review");
        } else {
            app_reload_after_import(app, 0);
            settings_screen_set_status_success(locale_get(settings_sync_result_key(result)), NULL);
        }
    } else {
        settings_screen_set_status_error(locale_get(settings_sync_result_key(result)));
    }
}

static void
settings_sync_account_logout(InbeApp *app)
{
    if(app == NULL)
        return;
    sync_account_clear();
    settings_screen_set_status_success(locale_get("sync_logged_out"), NULL);
}

void
settings_sync_account_clear_remote_confirmed(InbeApp *app)
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
    settings_screen_set_status_success(locale_get("sync_remote_data_cleared"), NULL);
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

    if(!has_account) {
        if(ui_draw_generic_button(x, *y, w, btn_h, locale_get("sync_create_account_button"),
                                  UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
            if(sync_account_create(&account)) {
                settings_screen_set_status_success(locale_get("sync_account_created"), NULL);
                settings_sync_open_alias_modal(app, 1);
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
    settings_sync_draw_account_id(app, &account, x, w, y, small_font,
                                  settings_sync_text_style());
    *y += flint_px(4);

    {
        FlintUIButtonRowItem buttons[3] = {
            {locale_get("sync_copy_id_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {locale_get("sync_alias_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {locale_get("sync_backup_key_button"), UI_BUTTON_STYLE_PRIMARY, 0}
        };
        int clicked = ui_draw_button_row((FlintUIButtonRow){
            .x = x,
            .y = *y,
            .width = w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 3
        });

        if(clicked == 0) {
            SetClipboardText(account.public_id);
            settings_screen_set_status_success(locale_get("sync_public_id_copied"), NULL);
        } else if(clicked == 1) {
            settings_sync_open_alias_modal(app, 0);
        } else if(clicked == 2) {
            app_open_modal(app, UIModalSyncAccountBackup);
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
                settings_sync_open_alias_modal(app, 1);
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
    settings_sync_draw_account_id(app, &account, x, w, y, small_font, input_style);
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
        FlintUIButtonRowItem buttons[3] = {
            {locale_get("sync_copy_id_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {locale_get("sync_alias_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {locale_get("sync_backup_key_button"), UI_BUTTON_STYLE_SECONDARY, 0}
        };
        int clicked = ui_draw_button_row((FlintUIButtonRow){
            .x = x,
            .y = *y,
            .width = w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 3
        });

        if(clicked == 0) {
            SetClipboardText(account.public_id);
            settings_screen_set_status_success(locale_get("sync_public_id_copied"), NULL);
        } else if(clicked == 1) {
            settings_sync_open_alias_modal(app, 0);
        } else if(clicked == 2) {
            app_open_modal(app, UIModalSyncAccountBackup);
        }
    }
    *y += btn_h + flint_px(12);

    {
        FlintUIButtonRowItem buttons[2] = {
            {locale_get("sync_logout_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {locale_get("sync_clear_remote_data_button"), UI_BUTTON_STYLE_DANGER, 0}
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
            settings_sync_account_logout(app);
        } else if(clicked == 1) {
            app_open_modal(app, UIModalConfirmDeleteSyncAccount);
        }
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

int
settings_sync_account_draw_alias_modal(InbeApp *app)
{
    FlintUIPanelFrame frame;
    FlintUITextInputStyle input_style = settings_sync_text_style();
    int btn_h = flint_px(36);
    int gap = flint_px(10);
    int y;
    int at_w;
    int commit = 0;
    int result = 0;
    char url[256];
    char current_alias[40];
    const char *stored_alias;
    int has_alias;
    int unchanged_alias;

    if(app == NULL)
        return 3;
    stored_alias = storage_get_setting_text(INBE_SYNC_ACCOUNT_ALIAS_KEY);
    snprintf(current_alias, sizeof(current_alias), "%s", stored_alias != NULL ? stored_alias : "");
    settings_sync_alias_normalize(current_alias);
    has_alias = current_alias[0] != '\0';

    frame = ui_draw_modal_frame(flint_px(336), flint_px(238),
                                locale_get(has_alias ? "sync_alias_change_title" : "sync_alias_title"),
                                (Texture2D){0}, (Texture2D){0});
    y = frame.content_y;

    flint_text_draw(locale_get(has_alias ? "sync_alias_change_message" : "sync_alias_message"),
                    frame.content_x, y,
                    flint_ui_font_small(), flint_darken(flint_theme_get_text(), 20));
    y += flint_px(40);

    at_w = flint_text_measure("@", flint_ui_font()) + flint_px(12);
    flint_text_draw("@", frame.content_x, flint_ui_text_y("@", y, btn_h, flint_ui_font()),
                    flint_ui_font(), flint_theme_get_text());
    flint_ui_text_field((FlintUITextField){
        .bounds = {(float)(frame.content_x + at_w), (float)y,
                   (float)(frame.content_w - at_w), (float)btn_h},
        .text = app->sync_alias_input,
        .text_size = sizeof(app->sync_alias_input),
        .cursor_position = &app->sync_alias_cursor,
        .focused = &app->sync_alias_focused,
        .max_codepoints = 32,
        .font = flint_ui_font_small(),
        .style = input_style,
        .filter = settings_sync_alias_filter,
        .commit_pressed = &commit
    });
    settings_sync_alias_normalize(app->sync_alias_input);
    y += btn_h + flint_px(12);

    flint_text_draw(locale_get("sync_alias_rule"), frame.content_x, y,
                    flint_ui_font_small(), flint_darken(flint_theme_get_text(), 35));
    y += flint_px(34);
    unchanged_alias = has_alias && strcmp(app->sync_alias_input, current_alias) == 0;

    {
        FlintUIButtonRowItem buttons[2] = {
            {locale_get(app->sync_alias_then_backup ? "skip_button" : "close_button"),
             UI_BUTTON_STYLE_SECONDARY, 0},
            {locale_get(has_alias ? "sync_alias_save_button" : "sync_alias_register_button"),
             UI_BUTTON_STYLE_PRIMARY,
             !settings_sync_alias_valid(app->sync_alias_input) || unchanged_alias}
        };
        int clicked = ui_draw_button_row((FlintUIButtonRow){
            .x = frame.content_x,
            .y = y,
            .width = frame.content_w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 2
        });
        if(clicked == 0)
            return 3;
        if(clicked == 1 || (commit && settings_sync_alias_valid(app->sync_alias_input) && !unchanged_alias)) {
            if(settings_sync_server_normalize(app, url, sizeof(url))) {
                InbeSyncClientResult alias_result;
                TraceLog(LOG_INFO, "SYNC: saving alias @%s with %s",
                         app->sync_alias_input, url);
                alias_result = sync_client_register_alias(url, app->sync_alias_input);
                if(alias_result == INBE_SYNC_CLIENT_OK) {
                    TraceLog(LOG_INFO, "SYNC: alias saved @%s", app->sync_alias_input);
                    result = 1;
                } else {
                    TraceLog(LOG_WARNING, "SYNC: alias register failed result=%d alias=@%s",
                             (int)alias_result, app->sync_alias_input);
                    result = 2;
                }
            } else {
                TraceLog(LOG_WARNING, "SYNC: alias register failed invalid server url");
                result = 2;
            }
        }
    }
    return result;
}

int
settings_sync_account_draw_public_id_modal(InbeApp *app)
{
    FlintUIPanelFrame frame;
    InbeSyncAccount account;
    FlintUITextInputStyle input_style = settings_sync_text_style();
    int btn_h = flint_px(36);
    int gap = flint_px(10);
    int y;
    int result = 0;

    (void)app;
    memset(&account, 0, sizeof(account));
    if(!sync_account_load(&account))
        return 1;

    frame = ui_draw_modal_frame(flint_px(356), flint_px(218),
                                locale_get("sync_public_id_full_title"),
                                (Texture2D){0}, (Texture2D){0});
    y = frame.content_y;

    flint_text_draw(locale_get("sync_public_id_full_message"), frame.content_x, y,
                    flint_ui_font_small(), flint_darken(flint_theme_get_text(), 25));
    y += flint_px(34);

    settings_draw_public_id_field(account.public_id, frame.content_x, frame.content_w,
                                  &y, flint_ui_font_small(), input_style);
    y += flint_px(18);

    {
        FlintUIButtonRowItem buttons[2] = {
            {locale_get("sync_copy_id_button"), UI_BUTTON_STYLE_PRIMARY, 0},
            {locale_get("close_button"), UI_BUTTON_STYLE_SECONDARY, 0}
        };
        int clicked = ui_draw_button_row((FlintUIButtonRow){
            .x = frame.content_x,
            .y = y,
            .width = frame.content_w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 2
        });
        if(clicked == 0) {
            SetClipboardText(account.public_id);
            settings_screen_set_status_success(locale_get("sync_public_id_copied"), NULL);
        } else if(clicked == 1) {
            result = 1;
        }
    }

    return result;
}
