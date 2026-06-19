#include "settings_sync_account.h"

#include "settings_screen.h"
#include "app.h"
#include "locale.h"
#include "sync_account.h"
#include "theme.h"
#include "flint_ui.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

static SettingsSyncKeySaveDialog save_dialog_callback = NULL;

void
settings_sync_account_set_save_dialog(SettingsSyncKeySaveDialog callback)
{
    save_dialog_callback = callback;
}

static void
settings_draw_hex_groups(const char *text, int x, int *y, int font, Color color,
                         int groups, int group_chars)
{
    char line[128];
    int offset = 0;

    if(text == NULL || y == NULL)
        return;
    for(int i = 0; i < groups && text[offset] != '\0'; i++) {
        int len = group_chars;
        if((int)strlen(text + offset) < len)
            len = (int)strlen(text + offset);
        snprintf(line, sizeof(line), "%.*s", len, text + offset);
        flint_text_draw(line, x, *y, font, color);
        *y += flint_px(17);
        offset += len;
    }
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
    return inbe_sync_account_export_private_key(account, filename) ? 2 : 3;
#else
    if(save_dialog_callback != NULL)
        return save_dialog_callback(app, filename);
    return inbe_sync_account_export_private_key(account, filename) ? 2 : 3;
#endif
}

void
settings_sync_account_draw(InbeApp *app, int x, int w, int *y)
{
    InbeSyncAccount account;
    int has_account = inbe_sync_account_load(&account);
    int font = flint_ui_font();
    int small_font = flint_ui_font_small();
    int btn_h = flint_px(36);
    int gap = flint_px(10);
    int half_w = (w - gap) / 2;
    int hover = 0;

    flint_text_draw("Sync Account", x, *y, font, theme_get_text());
    *y += flint_px(26);

    if(!inbe_sync_account_available()) {
        flint_text_draw("liboqs is not built for this target.", x, *y, small_font,
                        flint_darken(theme_get_text(), 35));
        *y += flint_px(32);
        return;
    }

    if(!has_account) {
        if(ui_draw_generic_button(x, *y, w, btn_h, "Create Account",
                                  UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
            if(inbe_sync_account_create(&account)) {
                settings_screen_set_status_success("Sync account created", NULL);
                app->modal.active = 1;
                app->modal.type = UIModalSyncAccountBackup;
                app->modal.selected_button = 0;
            } else {
                settings_screen_set_status_error("Could not create sync account");
            }
        }
        *y += btn_h + flint_px(18);
        return;
    }

    flint_text_draw("Public ID", x, *y, small_font, flint_darken(theme_get_text(), 30));
    *y += flint_px(18);
    settings_draw_hex_groups(account.public_id, x, y, small_font, theme_get_text(), 2, 32);
    *y += flint_px(4);

    if(ui_draw_generic_button(x, *y, half_w, btn_h, "Copy ID",
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
        SetClipboardText(account.public_id);
        settings_screen_set_status_success("Public ID copied", NULL);
    }
    if(ui_draw_generic_button(x + half_w + gap, *y, half_w, btn_h, "Backup Key",
                              UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
        app->modal.active = 1;
        app->modal.type = UIModalSyncAccountBackup;
        app->modal.selected_button = 0;
    }
    *y += btn_h + flint_px(16);
}

int
settings_sync_account_draw_backup_modal(InbeApp *app)
{
    FlintUIPanelFrame frame;
    InbeSyncAccount account;
    int font = flint_ui_font_small();
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
    inbe_sync_account_load(&account);

    warning = (FlintUIParagraph){
        .text = "Save this private key somewhere you control. Anyone with it can claim this sync account.",
        .width = flint_px(336) - flint_px(36),
        .font = font,
        .line_gap = flint_px(4),
        .color = theme_get_text()
    };
    modal_h = flint_px(58) +
              flint_ui_paragraph_height(warning) +
              text_button_gap +
              btn_h * 2 + gap +
              pad_bottom;
    if(modal_h < flint_px(224))
        modal_h = flint_px(224);

    frame = ui_draw_modal_frame(flint_px(336), modal_h, "Backup Sync Key",
                                (Texture2D){0}, (Texture2D){0});
    y = frame.content_y;
    warning.width = frame.content_w;
    flint_ui_paragraph_draw(warning, frame.content_x, &y);

    button_y = y + text_button_gap;
    if(button_y + btn_h * 2 + gap + pad_bottom > frame.y + frame.h)
        button_y = frame.y + frame.h - pad_bottom - btn_h * 2 - gap;
    if(ui_draw_generic_button(frame.content_x, button_y, frame.content_w, btn_h,
                              "Save .key File", UI_BUTTON_STYLE_PRIMARY, 0, &hover))
        return settings_start_sync_key_export(app, &account);
    if(ui_draw_generic_button(frame.content_x, button_y + btn_h + gap, frame.content_w,
                              btn_h, "Close", UI_BUTTON_STYLE_SECONDARY, 0, &hover))
        return 1;
    return 0;
}
