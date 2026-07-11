#include "settings_sync_account.h"

#include "settings_screen.h"
#include "app.h"
#include "locale.h"
#include "storage.h"
#include "sync_account.h"
#include "sync_client.h"
#include "theme.h"
#include "ui.h"
#include "flint.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define INBE_SYNC_SERVER_URL_KEY "sync_server_url"
#define INBE_SYNC_SERVER_URL_DEFAULT "https://api.waozi.xyz"
#define INBE_SYNC_ACCOUNT_ALIAS_KEY "sync_account_alias"

static SettingsSyncKeySaveDialog save_dialog_callback = NULL;
static SettingsSyncKeyImportDialog import_dialog_callback = NULL;

static void
settings_sync_ensure_default_server(void)
{
    const char *saved = storage_get_setting_text(INBE_SYNC_SERVER_URL_KEY);
    if(saved == NULL || saved[0] == '\0')
        storage_set_setting_text(INBE_SYNC_SERVER_URL_KEY, INBE_SYNC_SERVER_URL_DEFAULT);
}

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
settings_draw_public_id_link(const char *text, int x, int w, int *y, int font)
{
    int link_h;
    int clicked = 0;

    if(text == NULL || y == NULL)
        return 0;

    link_h = GetUITextHeight(text, font) + ScaleUIPx(8);
    if(link_h < ScaleUIPx(28))
        link_h = ScaleUIPx(28);
    clicked = DrawUIHref((UIHref){
        .bounds = {(float)x, (float)*y, (float)w, (float)link_h},
        .text = text,
        .font = font
    });
    *y += link_h;
    return clicked;
}

static void
settings_draw_public_id_field(const char *text, int x, int w, int *y, int font,
                              UITextInputStyle style)
{
    int field_h;

    if(text == NULL || y == NULL)
        return;

    field_h = GetUIReadonlyTextBoxHeight(text, font, w, style, 0);
    DrawUIReadonlyTextBox((UIReadonlyTextBox){
        .bounds = {(float)x, (float)*y, (float)w, (float)field_h},
        .text = text,
        .font = font,
        .style = style
    });
    *y += field_h;
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
        settings_screen_set_status_error(GetLocaleText("sync_server_url_invalid"));
        return;
    }
    storage_set_setting_text(INBE_SYNC_SERVER_URL_KEY, url);
    settings_screen_set_status_success(GetLocaleText("sync_server_saved"), url);
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

int
settings_sync_account_save_prepared(InbeApp *app, InbeSyncAccount *account,
                                    int action, int clear_local_data)
{
    InbeSyncAccountSaveResult result;

    if(app == NULL || account == NULL)
        return 0;

    result = sync_account_save(account, clear_local_data);
    if(result == INBE_SYNC_ACCOUNT_SAVE_NEEDS_CLEAR) {
        app->pending_sync_account = *account;
        app->pending_sync_account_action = action;
        app_open_modal(app, UIModalConfirmSyncAccountSwitch);
        return 0;
    }
    if(result != INBE_SYNC_ACCOUNT_SAVE_OK)
        return 0;

    app->pending_sync_account_action = InbePendingSyncAccountNone;
    memset(&app->pending_sync_account, 0, sizeof(app->pending_sync_account));
    settings_sync_ensure_default_server();

    if(action == InbePendingSyncAccountCreate) {
        settings_screen_set_status_success(GetLocaleText("sync_account_created"), NULL);
        app_auto_sync(app);
        settings_sync_open_alias_modal(app, 1);
    } else {
        settings_screen_set_status_success(GetLocaleText("sync_private_key_imported"), NULL);
        TraceLog(LOG_INFO, "SYNC: Private key imported");
        app_auto_sync(app);
    }
    return 1;
}

static void
settings_sync_draw_account_id(InbeApp *app, const InbeSyncAccount *account,
                              int x, int w, int *y, int small_font,
                              UITextInputStyle style)
{
    const char *alias = storage_get_setting_text(INBE_SYNC_ACCOUNT_ALIAS_KEY);
    char display[96];

    if(account == NULL)
        return;
    if(alias != NULL && alias[0] != '\0')
        snprintf(display, sizeof(display), "@%s", alias);
    else
        settings_compact_public_id(account->public_id, display, sizeof(display));
    (void)style;
    if(settings_draw_public_id_link(display, x, w, y, small_font) && app != NULL)
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
settings_sync_result_key(LyraSyncResult result)
{
    switch(result) {
        case LYRA_SYNC_OK:
            return "sync_connected";
        case LYRA_SYNC_INVALID_URL:
            return "sync_server_url_invalid";
        case LYRA_SYNC_NO_ACCOUNT:
            return "sync_no_account";
        case LYRA_SYNC_AUTH_FAILED:
            return "sync_auth_failed";
        case LYRA_SYNC_PAYLOAD_FAILED:
        case LYRA_SYNC_CHALLENGE_FAILED:
        case LYRA_SYNC_SIGN_FAILED:
        case LYRA_SYNC_REQUEST_FAILED:
        default:
            return "sync_failed";
    }
}

static int
settings_sync_create_account(InbeApp *app, InbeSyncAccount *account)
{
    if(account == NULL)
        return 0;
    if(!sync_account_generate(account)) {
        settings_screen_set_status_error(GetLocaleText("sync_account_create_failed"));
        return 0;
    }
    if(settings_sync_account_save_prepared(app, account, InbePendingSyncAccountCreate, 0))
        return 1;
    if(app == NULL || app->modal.type != UIModalConfirmSyncAccountSwitch)
        settings_screen_set_status_error(GetLocaleText("sync_account_create_failed"));
    return 0;
}

static void
settings_sync_run_connect(InbeApp *app)
{
    LyraSyncResult result;
    char url[sizeof(app->sync_server_url)];

    if(app == NULL)
        return;
    if(!settings_sync_server_normalize(app, url, sizeof(url))) {
        settings_screen_set_status_error(GetLocaleText("sync_server_url_invalid"));
        return;
    }
    storage_set_setting_text(INBE_SYNC_SERVER_URL_KEY, url);
    result = sync_client_sync(url);
    if(result == LYRA_SYNC_OK) {
        app_request_social_refresh(app);
        if(storage_sync_review_clear_if_no_visible_diff()) {
            app_reload_after_import(app, 0);
            settings_screen_set_status_success(GetLocaleText(settings_sync_result_key(result)), NULL);
        } else if(storage_sync_review_apply_remote_if_local_empty()) {
            app_reload_after_import(app, 0);
            settings_screen_set_status_success(GetLocaleText(settings_sync_result_key(result)), NULL);
        } else if(storage_sync_review_pending()) {
            app_open_modal(app, UIModalSyncReview);
            settings_screen_set_status_error(GetLocaleText("sync_review_needed"));
        } else {
            app_reload_after_import(app, 0);
            settings_screen_set_status_success(GetLocaleText(settings_sync_result_key(result)), NULL);
        }
    } else {
        settings_screen_set_status_error(GetLocaleText(settings_sync_result_key(result)));
    }
}

static void
settings_sync_account_logout(InbeApp *app)
{
    if(app == NULL)
        return;
    sync_account_clear();
    settings_screen_set_status_success(GetLocaleText("sync_logged_out_local_data_kept"), NULL);
}

void
settings_sync_account_clear_remote_confirmed(InbeApp *app)
{
    InbeSyncAccount account;
    LyraSyncResult result;
    char url[sizeof(app->sync_server_url)];

    if(app == NULL)
        return;
    if(!sync_account_load(&account)) {
        settings_screen_set_status_error(GetLocaleText("sync_no_account"));
        return;
    }

    if(settings_sync_server_normalize(app, url, sizeof(url))) {
        result = sync_client_delete_account(url);
        if(result != LYRA_SYNC_OK)
            TraceLog(LOG_WARNING, "SYNC: remote account delete failed: %d", result);
    } else {
        TraceLog(LOG_WARNING, "SYNC: remote account delete skipped due to invalid URL");
    }

    sync_account_clear();
    settings_screen_set_status_success(GetLocaleText("sync_remote_data_cleared"), NULL);
}

static UITextInputStyle
settings_sync_text_style(void)
{
    return (UITextInputStyle){
        .background = DarkenUIColor(GetThemeBackground(), 4),
        .border = GetThemeButton(),
        .focus_border = GetThemeButtonHover(),
        .text = GetThemeText(),
        .cursor = GetThemeText(),
        .radius = 0.08f,
        .padding_x = ScaleUIPx(10)
    };
}

static int
settings_sync_button_min_width(const char *label)
{
    return MeasureUIText(label != NULL ? label : "", GetUISmallFontSize()) +
           ScaleUIPx(20);
}

static int
settings_sync_button_group_fits(const UIButtonRowItem *items, int start, int count,
                                int width, int gap)
{
    int button_w;

    if(items == NULL || count <= 0 || width <= 0)
        return 0;
    button_w = (width - gap * (count - 1)) / count;
    if(button_w <= 0)
        return 0;
    for(int i = 0; i < count; i++) {
        if(settings_sync_button_min_width(items[start + i].label) > button_w)
            return 0;
    }
    return 1;
}

static int
settings_sync_button_group_count(const UIButtonRowItem *items, int start, int count,
                                 int width, int gap)
{
    for(int row_count = count; row_count > 1; row_count--) {
        if(settings_sync_button_group_fits(items, start, row_count, width, gap))
            return row_count;
    }
    return 1;
}

static int
settings_sync_draw_adaptive_button_row(UIButtonRow row, int *height_out)
{
    int clicked = -1;
    int gap = row.gap > 0 ? row.gap : ScaleUIPx(10);
    int draw_y = row.y;
    int index = 0;

    if(height_out != NULL)
        *height_out = 0;
    if(row.items == NULL || row.count <= 0 || row.width <= 0 || row.height <= 0)
        return -1;

    while(index < row.count) {
        int count = settings_sync_button_group_count(row.items, index,
                                                     row.count - index,
                                                     row.width, gap);
        int button_w = (row.width - gap * (count - 1)) / count;

        for(int i = 0; i < count; i++) {
            int hover = 0;
            int item_index = index + i;
            int button_x = row.x + i * (button_w + gap);
            int draw_w = i == count - 1 ? row.x + row.width - button_x : button_w;

            if(DrawUIGenericButton(button_x, draw_y, draw_w, row.height,
                                      row.items[item_index].label,
                                      row.items[item_index].style,
                                      row.items[item_index].disabled,
                                      &hover))
                clicked = item_index;
        }
        index += count;
        draw_y += row.height;
        if(index < row.count)
            draw_y += gap;
    }

    if(height_out != NULL)
        *height_out = draw_y - row.y;
    return clicked;
}

static int
settings_sync_adaptive_button_row_height(const UIButtonRowItem *items, int count,
                                         int width, int button_h, int gap)
{
    int height = 0;
    int index = 0;

    if(items == NULL || count <= 0 || width <= 0 || button_h <= 0)
        return 0;
    while(index < count) {
        int row_count = settings_sync_button_group_count(items, index, count - index,
                                                         width, gap);
        height += button_h;
        index += row_count;
        if(index < count)
            height += gap;
    }
    return height;
}

int
settings_sync_account_config_content_height(int content_w)
{
    InbeSyncAccount account;
    int has_account = sync_account_load(&account);
    int btn_h = ScaleUIPx(36);
    int gap = ScaleUIPx(10);
    int h = 0;
    char display[96];
    const char *alias;

    if(content_w <= 0)
        content_w = ScaleUIPx(320);

    if(!has_account) {
        UIButtonRowItem buttons[2] = {
            {GetLocaleText("sync_create_account_button"), UI_BUTTON_STYLE_PRIMARY, 0},
            {GetLocaleText("sync_import_key_button"), UI_BUTTON_STYLE_PRIMARY, 0}
        };
        h += ScaleUIPx(12);
        h += settings_sync_adaptive_button_row_height(buttons, 2, content_w, btn_h, gap);
        h += ScaleUIPx(18);
        h += ScaleUIPx(42);
        return h;
    }

    alias = storage_get_setting_text(INBE_SYNC_ACCOUNT_ALIAS_KEY);
    if(alias != NULL && alias[0] != '\0')
        snprintf(display, sizeof(display), "@%s", alias);
    else
        settings_compact_public_id(account.public_id, display, sizeof(display));

    h += ScaleUIPx(18);
    h += GetUIReadonlyTextBoxHeight(display, GetUISmallFontSize(),
                                           content_w, settings_sync_text_style(), 0);
    h += ScaleUIPx(8);
    h += ScaleUIPx(18) + btn_h + ScaleUIPx(12);
    h += btn_h + ScaleUIPx(12);
    {
        UIButtonRowItem buttons[3] = {
            {GetLocaleText("sync_copy_id_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {GetLocaleText("sync_alias_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {GetLocaleText("sync_backup_key_button"), UI_BUTTON_STYLE_SECONDARY, 0}
        };
        h += settings_sync_adaptive_button_row_height(buttons, 3, content_w, btn_h, gap);
        h += ScaleUIPx(12);
    }
    {
        UIButtonRowItem buttons[2] = {
            {GetLocaleText("sync_logout_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {GetLocaleText("sync_clear_remote_data_button"), UI_BUTTON_STYLE_DANGER, 0}
        };
        h += settings_sync_adaptive_button_row_height(buttons, 2, content_w, btn_h, gap);
        h += ScaleUIPx(12);
    }
    h += ScaleUIPx(42);
    return h;
}

void
settings_sync_account_draw(InbeApp *app, int x, int w, int *y)
{
    InbeSyncAccount account;
    int has_account = sync_account_load(&account);
    int font = GetUIFontSize();
    int small_font = GetUISmallFontSize();
    int btn_h = ScaleUIPx(36);
    int gap = ScaleUIPx(10);
    int hover = 0;

    DrawUIText(GetLocaleText("sync_account_title"), x, *y, font, GetThemeText());
    *y += ScaleUIPx(26);

    if(!has_account) {
        if(DrawUIGenericButton(x, *y, w, btn_h, GetLocaleText("sync_create_account_button"),
                                  UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
            settings_sync_create_account(app, &account);
        }
        *y += btn_h + ScaleUIPx(18);
        return;
    }

    DrawUIText(GetLocaleText("sync_public_id_label"), x, *y, small_font,
                    DarkenUIColor(GetThemeText(), 30));
    *y += ScaleUIPx(18);
    settings_sync_draw_account_id(app, &account, x, w, y, small_font,
                                  settings_sync_text_style());
    *y += ScaleUIPx(4);

    {
        UIButtonRowItem buttons[3] = {
            {GetLocaleText("sync_copy_id_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {GetLocaleText("sync_alias_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {GetLocaleText("sync_backup_key_button"), UI_BUTTON_STYLE_PRIMARY, 0}
        };
        int row_h = 0;
        int clicked = settings_sync_draw_adaptive_button_row((UIButtonRow){
            .x = x,
            .y = *y,
            .width = w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 3
        }, &row_h);

        if(clicked == 0) {
            SetClipboardText(account.public_id);
            settings_screen_set_status_success(GetLocaleText("sync_public_id_copied"), NULL);
        } else if(clicked == 1) {
            settings_sync_open_alias_modal(app, 0);
        } else if(clicked == 2) {
            app_open_modal(app, UIModalSyncAccountBackup);
        }
        *y += row_h;
    }
    *y += ScaleUIPx(16);
}

void
settings_sync_account_draw_config(InbeApp *app, int x, int w, int *y)
{
    InbeSyncAccount account;
    int has_account = sync_account_load(&account);
    int font = GetUIFontSize();
    int small_font = GetUISmallFontSize();
    int btn_h = ScaleUIPx(36);
    int gap = ScaleUIPx(10);
    int hover = 0;
    int commit = 0;
    UITextInputStyle input_style = settings_sync_text_style();

    if(app != NULL && app->sync_server_url[0] == '\0') {
        const char *saved;

        settings_sync_ensure_default_server();
        saved = storage_get_setting_text(INBE_SYNC_SERVER_URL_KEY);
        snprintf(app->sync_server_url, sizeof(app->sync_server_url), "%s",
                 saved != NULL && saved[0] != '\0'
                     ? saved
                     : INBE_SYNC_SERVER_URL_DEFAULT);
        app->sync_server_url_cursor = (int)strlen(app->sync_server_url);
    }

    if(!has_account) {
        UIButtonRowItem buttons[2] = {
            {GetLocaleText("sync_create_account_button"), UI_BUTTON_STYLE_PRIMARY, 0},
            {GetLocaleText("sync_import_key_button"), UI_BUTTON_STYLE_PRIMARY, 0}
        };
        int clicked;
        int row_h = 0;

        *y += ScaleUIPx(12);
        clicked = settings_sync_draw_adaptive_button_row((UIButtonRow){
            .x = x,
            .y = *y,
            .width = w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 2
        }, &row_h);
        if(clicked == 0) {
            has_account = settings_sync_create_account(app, &account);
        } else if(clicked == 1) {
            if(!settings_start_sync_key_import(app))
                settings_screen_set_status_error(GetLocaleText("sync_private_key_import_failed"));
        }
        *y += row_h + ScaleUIPx(18);
    }

    if(!has_account) {
        settings_screen_draw_status_reserved(x, y, ScaleUIPx(42));
        (void)font;
        return;
    }

    DrawUIText(GetLocaleText("sync_public_id_label"), x, *y, small_font,
                    DarkenUIColor(GetThemeText(), 30));
    *y += ScaleUIPx(18);
    settings_sync_draw_account_id(app, &account, x, w, y, small_font, input_style);
    *y += ScaleUIPx(8);

    DrawUIText(GetLocaleText("sync_remote_label"), x, *y, small_font,
                    DarkenUIColor(GetThemeText(), 30));
    *y += ScaleUIPx(18);
    DrawUITextField((UITextField){
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
    *y += btn_h + ScaleUIPx(12);

    if(DrawUIGenericButton(x, *y, w, btn_h,
                              settings_sync_server_connected(app)
                                  ? GetLocaleText("sync_connected_button")
                                  : GetLocaleText("sync_connect_button"),
                              UI_BUTTON_STYLE_PRIMARY, 0, &hover))
        settings_sync_run_connect(app);
    *y += btn_h + ScaleUIPx(12);

    {
        UIButtonRowItem buttons[3] = {
            {GetLocaleText("sync_copy_id_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {GetLocaleText("sync_alias_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {GetLocaleText("sync_backup_key_button"), UI_BUTTON_STYLE_SECONDARY, 0}
        };
        int row_h = 0;
        int clicked = settings_sync_draw_adaptive_button_row((UIButtonRow){
            .x = x,
            .y = *y,
            .width = w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 3
        }, &row_h);

        if(clicked == 0) {
            SetClipboardText(account.public_id);
            settings_screen_set_status_success(GetLocaleText("sync_public_id_copied"), NULL);
        } else if(clicked == 1) {
            settings_sync_open_alias_modal(app, 0);
        } else if(clicked == 2) {
            app_open_modal(app, UIModalSyncAccountBackup);
        }
        *y += row_h;
    }
    *y += ScaleUIPx(12);

    {
        UIButtonRowItem buttons[2] = {
            {GetLocaleText("sync_logout_button"), UI_BUTTON_STYLE_SECONDARY, 0},
            {GetLocaleText("sync_clear_remote_data_button"), UI_BUTTON_STYLE_DANGER, 0}
        };
        int row_h = 0;
        int clicked = settings_sync_draw_adaptive_button_row((UIButtonRow){
            .x = x,
            .y = *y,
            .width = w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 2
        }, &row_h);
        if(clicked == 0) {
            settings_sync_account_logout(app);
        } else if(clicked == 1) {
            app_open_modal(app, UIModalConfirmDeleteSyncAccount);
        }
        *y += row_h;
    }
    *y += ScaleUIPx(12);

    settings_screen_draw_status_reserved(x, y, ScaleUIPx(42));
    (void)font;
}

int
settings_sync_account_draw_backup_modal(InbeApp *app)
{
    UIPanelFrame frame;
    InbeSyncAccount account;
    int btn_h = ScaleUIPx(36);
    int gap = ScaleUIPx(10);
    int pad_bottom = ScaleUIPx(24);
    int text_button_gap = ScaleUIPx(40);
    int hover = 0;
    int y;
    int modal_h;
    int button_y;
    UIParagraph warning;

    memset(&account, 0, sizeof(account));
    sync_account_load(&account);

    warning = (UIParagraph){
        .text = GetLocaleText("sync_backup_warning"),
        .width = ScaleUIPx(336) - ScaleUIPx(36)
    };
    modal_h = ScaleUIPx(58) +
              GetUIParagraphHeight(warning) +
              text_button_gap +
              btn_h * 2 + gap +
              pad_bottom;
    if(modal_h < ScaleUIPx(224))
        modal_h = ScaleUIPx(224);

    frame = DrawUIModalFrame(ScaleUIPx(336), modal_h, GetLocaleText("sync_backup_title"),
                                (Texture2D){0}, (Texture2D){0});
    y = frame.content_y;
    warning.width = frame.content_w;
    DrawUIParagraph(warning, frame.content_x, &y);

    button_y = y + text_button_gap;
    if(button_y + btn_h * 2 + gap + pad_bottom > frame.y + frame.h)
        button_y = frame.y + frame.h - pad_bottom - btn_h * 2 - gap;
    if(DrawUIGenericButton(frame.content_x, button_y, frame.content_w, btn_h,
                              GetLocaleText("sync_save_key_file_button"), UI_BUTTON_STYLE_PRIMARY, 0, &hover))
        return settings_start_sync_key_export(app, &account);
    if(DrawUIGenericButton(frame.content_x, button_y + btn_h + gap, frame.content_w,
                              btn_h, GetLocaleText("close_button"), UI_BUTTON_STYLE_SECONDARY, 0, &hover))
        return 1;
    return 0;
}

int
settings_sync_account_draw_alias_modal(InbeApp *app)
{
    UIPanelFrame frame;
    UITextInputStyle input_style = settings_sync_text_style();
    int btn_h = ScaleUIPx(36);
    int gap = ScaleUIPx(10);
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

    frame = DrawUIModalFrame(ScaleUIPx(336), ScaleUIPx(238),
                                GetLocaleText(has_alias ? "sync_alias_change_title" : "sync_alias_title"),
                                (Texture2D){0}, (Texture2D){0});
    y = frame.content_y;

    DrawUIText(GetLocaleText(has_alias ? "sync_alias_change_message" : "sync_alias_message"),
                    frame.content_x, y,
                    GetUISmallFontSize(), DarkenUIColor(GetThemeText(), 20));
    y += ScaleUIPx(40);

    at_w = MeasureUIText("@", GetUIFontSize()) + ScaleUIPx(12);
    DrawUIText("@", frame.content_x, GetUIControlTextY("@", y, btn_h, GetUIFontSize()),
                    GetUIFontSize(), GetThemeText());
    DrawUITextField((UITextField){
        .bounds = {(float)(frame.content_x + at_w), (float)y,
                   (float)(frame.content_w - at_w), (float)btn_h},
        .text = app->sync_alias_input,
        .text_size = sizeof(app->sync_alias_input),
        .cursor_position = &app->sync_alias_cursor,
        .focused = &app->sync_alias_focused,
        .max_codepoints = 32,
        .font = GetUISmallFontSize(),
        .style = input_style,
        .filter = settings_sync_alias_filter,
        .commit_pressed = &commit
    });
    settings_sync_alias_normalize(app->sync_alias_input);
    y += btn_h + ScaleUIPx(12);

    DrawUIText(GetLocaleText("sync_alias_rule"), frame.content_x, y,
                    GetUISmallFontSize(), DarkenUIColor(GetThemeText(), 35));
    y += ScaleUIPx(34);
    unchanged_alias = has_alias && strcmp(app->sync_alias_input, current_alias) == 0;

    {
        UIButtonRowItem buttons[2] = {
            {GetLocaleText(app->sync_alias_then_backup ? "skip_button" : "close_button"),
             UI_BUTTON_STYLE_SECONDARY, 0},
            {GetLocaleText(has_alias ? "sync_alias_save_button" : "sync_alias_register_button"),
             UI_BUTTON_STYLE_PRIMARY,
             !settings_sync_alias_valid(app->sync_alias_input) || unchanged_alias}
        };
        int clicked = settings_sync_draw_adaptive_button_row((UIButtonRow){
            .x = frame.content_x,
            .y = y,
            .width = frame.content_w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 2
        }, NULL);
        if(clicked == 0)
            return 3;
        if(clicked == 1 || (commit && settings_sync_alias_valid(app->sync_alias_input) && !unchanged_alias)) {
            if(settings_sync_server_normalize(app, url, sizeof(url))) {
                LyraSyncResult alias_result;
                TraceLog(LOG_INFO, "SYNC: saving alias @%s with %s",
                         app->sync_alias_input, url);
                alias_result = sync_client_register_alias(url, app->sync_alias_input);
                if(alias_result == LYRA_SYNC_OK) {
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
    UIPanelFrame frame;
    InbeSyncAccount account;
    UITextInputStyle input_style = settings_sync_text_style();
    int btn_h = ScaleUIPx(36);
    int gap = ScaleUIPx(10);
    int y;
    int result = 0;

    (void)app;
    memset(&account, 0, sizeof(account));
    if(!sync_account_load(&account))
        return 1;

    frame = DrawUIModalFrame(ScaleUIPx(356), ScaleUIPx(218),
                                GetLocaleText("sync_public_id_full_title"),
                                (Texture2D){0}, (Texture2D){0});
    y = frame.content_y;

    DrawUIText(GetLocaleText("sync_public_id_full_message"), frame.content_x, y,
                    GetUISmallFontSize(), DarkenUIColor(GetThemeText(), 25));
    y += ScaleUIPx(34);

    settings_draw_public_id_field(account.public_id, frame.content_x, frame.content_w,
                                  &y, GetUISmallFontSize(), input_style);
    y += ScaleUIPx(18);

    {
        UIButtonRowItem buttons[2] = {
            {GetLocaleText("sync_copy_id_button"), UI_BUTTON_STYLE_PRIMARY, 0},
            {GetLocaleText("close_button"), UI_BUTTON_STYLE_SECONDARY, 0}
        };
        int clicked = settings_sync_draw_adaptive_button_row((UIButtonRow){
            .x = frame.content_x,
            .y = y,
            .width = frame.content_w,
            .height = btn_h,
            .gap = gap,
            .items = buttons,
            .count = 2
        }, NULL);
        if(clicked == 0) {
            SetClipboardText(account.public_id);
            settings_screen_set_status_success(GetLocaleText("sync_public_id_copied"), NULL);
        } else if(clicked == 1) {
            result = 1;
        }
    }

    return result;
}
