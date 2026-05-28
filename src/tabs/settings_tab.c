#include "settings_tab.h"
#include "app.h"
#include "ui.h"
#include "theme.h"
#include "theme_meta.h"
#include "version.h"
#include "data.h"
#include "file_dialog.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

/* Theme colors - set by ui_set_colors */
extern Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

#define CONTENT_MAX_W 600
#define CONTENT_SIDE_PAD 20

static const char *settings_tab_names[] = {
    "Breathing",
    "Session",
    "Appearance",
    "Data",
    "About"
};

extern int view_width;
extern int view_height;

static void
draw_theme_selector(InbeApp *app, int x, int y, int w)
{
    int font = ui_clamp_px(14, 12, 16);
    int small_font = ui_clamp_px(12, 10, 14);
    const char *label = "Theme";

    DrawText(label, x, y, font, c_text);

    /* Light/Dark toggle */
    int toggle_w = ui_px(100);
    int toggle_h = ui_px(28);
    int toggle_x = x + w - toggle_w;
    int toggle_y = y - 2;

    if(ui_draw_toggle_switch(app, toggle_x, toggle_y, toggle_w, toggle_h, &app->dark_mode)) {
        refresh_theme_colors(app->theme_id, app->dark_mode);
        app->settings_dirty = 1;
    }

    int circle_size = ui_px(36);
    int circle_spacing = ui_px(24);
    int row_spacing = ui_px(36);
    int per_row = 3;
    int row_width = per_row * circle_size + (per_row - 1) * circle_spacing;
    int start_x = x + (w - row_width) / 2;
    int circle_y = y + ui_px(64);
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);

    for(int i = 0; i < THEME_COUNT; i++) {
        int row = i / per_row;
        int col = i % per_row;
        int cx = start_x + col * (circle_size + circle_spacing) + circle_size / 2;
        int cy = circle_y + row * (circle_size + row_spacing);

        /* Draw circle - get color from Lotus */
        const char *scope = app->dark_mode ? g_themes[i].dark_scope : g_themes[i].light_scope;
        Color theme_color = theme_get(scope, "circle");
        DrawCircle(cx, cy, circle_size / 2, theme_color);

        /* Draw selection ring */
        if(app->theme_id == i) {
            DrawCircleLines(cx, cy, circle_size / 2 + 2, c_text);
        } else {
            DrawCircleLines(cx, cy, circle_size / 2 + 1, ui_darken(c_bg, 30));
        }

        /* Check for click */
        Rectangle bounds = {cx - circle_size / 2 - 4, cy - circle_size / 2 - 4, circle_size + 8, circle_size + 8};
        if(CheckCollisionPointRec(mouse_world, bounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            app->theme_id = i;
            refresh_theme_colors(app->theme_id, app->dark_mode);
            app->settings_dirty = 1;
        }

        if(CheckCollisionPointRec(mouse_world, bounds))
            app->cursor_clickable = 1;

        /* Draw theme name below */
        const char *name = g_themes[i].name;
        int name_w = MeasureText(name, small_font);
        DrawText(name, cx - name_w / 2, cy + circle_size / 2 + ui_px(6), small_font, c_text);
    }
}

void
settings_tab_draw(InbeApp *app)
{
    int title_h = ui_screen_header_height();
    int viewport_h = view_height - title_h;
    int close_clicked = 0;
    int content_x;
    int content_w;
    static FileDialog export_dlg;
    static int export_dlg_initialized = 0;
    static char export_result[128] = "";  /* Store export result message */

    ui_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &content_x, &content_w);

    close_clicked = ui_draw_screen_header(app, "Settings", 1);
    if(close_clicked) {
        if(app->settings_dirty)
            save_settings(app);
        app->inbe.screen = InbeScreenStart;
    }

    /* Initialize export dialog on first call */
    if(!export_dlg_initialized) {
        file_dialog_init(&export_dlg);
        export_dlg_initialized = 1;
    }

    /* Dropdown for tab selection */
    int dropdown_h = ui_px(36);
    int dropdown_y = title_h + ui_px(8);

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        if(ui_draw_dropdown_button(app, 100, content_x, dropdown_y, content_w, dropdown_h, settings_tab_names, SETTINGS_TAB_COUNT, &app->settings_tab)) {
            reset_settings_preview(app);
        }

        int yoff = ui_px(16);
        int speed = app->inbe.speed_level;
        int max_rounds = app->inbe.max_rounds;
        int max_breaths = int_from_count(app->inbe.maxbreaths);
        int pause_seconds = app->inbe.pause_seconds;

        /* Update preview for breathing tab */
        update_preview_bounds(&app->settings_preview, content_w, ui_px(240));
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        inbestep(&app->settings_preview);
        if(app->settings_preview.phase != InbePhaseBreathe) {
            reset_settings_preview(app);
            update_preview_bounds(&app->settings_preview, content_w, ui_px(240));
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        }

        /* Content starts after dropdown */
        int content_start_y = dropdown_y + dropdown_h;

        switch(app->settings_tab) {
            case SETTINGS_TAB_BREATHING: {
                draw_preview_inbe(&app->settings_preview, content_x + content_w / 2, yoff + content_start_y + ui_px(100));

                if(ui_draw_slider(app, 1, content_x, yoff + content_start_y + ui_px(200), content_w, "Speed", SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX, &speed, "")) {
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }
                break;
            }
            case SETTINGS_TAB_SESSION: {
                int slider_y = yoff + content_start_y + ui_px(20);

                /* Max rounds */
                if(ui_draw_slider(app, 2, content_x, slider_y, content_w, "Max rounds", 1,
                               MaxRounds, &max_rounds, "")) {
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }

                /* Max breaths */
                if(ui_draw_slider(app, 3, content_x, slider_y + ui_px(66), content_w, "Max breaths", SETTINGS_BREATHS_MIN,
                               SETTINGS_BREATHS_MAX, &max_breaths, "")) {
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }

                /* Pause */
                if(ui_draw_slider(app, 4, content_x, slider_y + ui_px(132), content_w, "Pause after round", SETTINGS_PAUSE_MIN,
                               SETTINGS_PAUSE_MAX, &pause_seconds, "s")) {
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }

                /* Volume */
                int sound_volume = app->sound_volume;
                if(ui_draw_slider(app, 6, content_x, slider_y + ui_px(198), content_w, "Volume", SETTINGS_VOLUME_MIN,
                               SETTINGS_VOLUME_MAX, &sound_volume, "")) {
                    app->sound_volume = sound_volume;
                    app->settings_dirty = 1;
                }

                /* Reset to defaults button */
                int reset_y = slider_y + ui_px(265);
                int reset_w = MeasureText("Reset to defaults", ui_clamp_px(14, 12, 16)) + ui_px(24);
                int reset_h = ui_px(36);
                int reset_x = content_x + content_w - reset_w;
                int reset_hover = 0;
                Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);

                Rectangle reset_bounds = {reset_x, reset_y, reset_w, reset_h};
                if(CheckCollisionPointRec(mouse_world, reset_bounds)) {
                    DrawRectangle(reset_x, reset_y, reset_w, reset_h, c_button_hover);
                    ui_draw_bevel(reset_x, reset_y, reset_w, reset_h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
                    reset_hover = 1;
                    app->cursor_clickable = 1;
                } else {
                    DrawRectangle(reset_x, reset_y, reset_w, reset_h, c_button);
                    ui_draw_bevel(reset_x, reset_y, reset_w, reset_h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
                }

                int reset_font = ui_clamp_px(14, 12, 16);
                DrawText("Reset to defaults", reset_x + ui_px(12), reset_y + reset_h / 2 - reset_font / 2 - 1, reset_font, c_text);

                if(reset_hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    /* Reset to default values */
                    speed = 6;
                    max_rounds = DefaultMaxRounds;
                    max_breaths = DefaultMaxBreaths;
                    pause_seconds = DefaultPauseSeconds;
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }
                break;
            }
            case SETTINGS_TAB_APPEARANCE: {
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
                int checkbox_y = yoff + content_start_y;
                if(ui_draw_checkbox_toggle(app, content_x, checkbox_y, "Fullscreen", &app->fullscreen_enabled)) {
                    if(app->fullscreen_enabled && !IsWindowFullscreen())
                        ToggleFullscreen();
                    else if(!app->fullscreen_enabled && IsWindowFullscreen())
                        ToggleFullscreen();
                    app->settings_dirty = 1;
                }

                /* Theme selector - below fullscreen */
                int theme_y = yoff + content_start_y + ui_px(50);
#else
                int theme_y = yoff + content_start_y;
#endif
                draw_theme_selector(app, content_x, theme_y, content_w);

                int font = ui_clamp_px(14, 12, 16);
                int label_y = theme_y + ui_px(220);
                DrawText("Language", content_x, label_y, font, c_text);
                DrawText("Coming soon...", content_x, label_y + ui_px(30), font, ui_darken(c_text, 40));
                break;
            }
            case SETTINGS_TAB_DATA: {
                int font = ui_clamp_px(14, 12, 16);
                int text_y = yoff + content_start_y;
                int session_count = data_get_session_count();
                long long data_size = data_get_total_size();
                char size_str[32];
                int hover_export = 0;
                int hover_delete = 0;

                /* Format data size */
                if(data_size < 1024)
                    snprintf(size_str, sizeof(size_str), "%lld B", data_size);
                else if(data_size < 1024 * 1024)
                    snprintf(size_str, sizeof(size_str), "%.1f KB", (float)data_size / 1024);
                else
                    snprintf(size_str, sizeof(size_str), "%.1f MB", (float)data_size / (1024 * 1024));

                /* Title */
                DrawText("Data Management", content_x, text_y, font, c_text);
                text_y += ui_px(30);

                /* Statistics box */
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID)
                int stats_box_h = ui_px(90);
#else
                int stats_box_h = ui_px(66);  /* Smaller on Android (no storage line) */
#endif
                DrawRectangle(content_x, text_y, content_w, stats_box_h, ui_darken(c_bg, 8));
                ui_draw_bevel(content_x, text_y, content_w, stats_box_h, ui_lighten(c_bg, 35), ui_darken(c_bg, 45));

                int stat_x = content_x + ui_px(16);
                int stat_y = text_y + ui_px(16);
                char stat_text[64];

                snprintf(stat_text, sizeof(stat_text), "Total Sessions: %d", session_count);
                DrawText(stat_text, stat_x, stat_y, font, c_text);
                stat_y += ui_px(22);

                snprintf(stat_text, sizeof(stat_text), "Data Size: %s", size_str);
                DrawText(stat_text, stat_x, stat_y, font, c_text);
                stat_y += ui_px(22);

#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID)
                const char *storage_path = data_root();
                /* Replace /home/user/ with ~ for display */
                const char *display_path = storage_path;
                char home_buf[FS_PATH_MAX];
                if (strncmp(storage_path, "/home/", 6) == 0) {
                    const char *slash_after_user = strchr(storage_path + 6, '/');
                    if (slash_after_user != NULL) {
                        snprintf(home_buf, sizeof(home_buf), "~%s", slash_after_user);
                        display_path = home_buf;
                    }
                }
                DrawText(TextFormat("Storage: %s", display_path), stat_x, stat_y, ui_clamp_px(12, 10, 14), ui_darken(c_text, 40));
#endif

                text_y += stats_box_h + ui_px(24);

                /* Export button */
                int export_h = ui_px(36);
                int export_w = MeasureText("Export Data", font) + ui_px(24);
                int export_x = content_x;
                int export_y = text_y;
                Rectangle export_rect = {export_x, export_y, export_w, export_h};

                Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
                if(CheckCollisionPointRec(mouse_world, export_rect)) {
                    DrawRectangle(export_x, export_y, export_w, export_h, c_button_hover);
                    ui_draw_bevel(export_x, export_y, export_w, export_h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
                    hover_export = 1;
                    app->cursor_clickable = 1;
                } else {
                    DrawRectangle(export_x, export_y, export_w, export_h, c_button);
                    ui_draw_bevel(export_x, export_y, export_w, export_h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
                }
                DrawText("Export Data", export_x + ui_px(12), export_y + export_h / 2 - font / 2 - 1, font, c_text);

                /* Delete All button */
                int delete_w = MeasureText("Delete All Data", font) + ui_px(24);
                int delete_x = content_x + content_w - delete_w;
                Rectangle delete_rect = {delete_x, export_y, delete_w, export_h};

                if(CheckCollisionPointRec(mouse_world, delete_rect)) {
                    DrawRectangle(delete_x, export_y, delete_w, export_h, c_button_hover);
                    ui_draw_bevel(delete_x, export_y, delete_w, export_h, ui_darken(c_button_hover, 40), ui_lighten(c_button_hover, 40));
                    hover_delete = 1;
                    app->cursor_clickable = 1;
                } else {
                    DrawRectangle(delete_x, export_y, delete_w, export_h, c_button);
                    ui_draw_bevel(delete_x, export_y, delete_w, export_h, ui_lighten(c_button, 40), ui_darken(c_button, 40));
                }
                DrawText("Delete All Data", delete_x + ui_px(12), export_y + export_h / 2 - font / 2 - 1, font, c_text);

                /* Handle button clicks */
                if(hover_export && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    if(file_dialog_save(&export_dlg, "Export Data", "inbe-export.zip")) {
                        const char *path = file_dialog_get_path(&export_dlg);
                        if(path != NULL && data_export(path)) {
                            /* Extract just the filename for display */
                            const char *filename = GetFileName(path);
                            snprintf(export_result, sizeof(export_result), "Exported: %s", filename);
                            TraceLog(LOG_INFO, "DATA: Export successful to %s", path);
                        } else {
                            snprintf(export_result, sizeof(export_result), "Export failed - no data");
                            TraceLog(LOG_ERROR, "DATA: Export failed");
                        }
                    }
                }
                if(hover_delete && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    if(data_has_any()) {
                        app->modal.active = 1;
                        app->modal.type = UIModalConfirmDeleteData;
                        app->modal.selected_button = 0;
                    }
                }

                /* Draw export result message under Export button */
                if(export_result[0] != '\0') {
                    int result_font = ui_clamp_px(12, 10, 14);
                    int result_x = export_x;
                    int result_y = export_y + export_h + ui_px(8);
                    DrawText(export_result, result_x, result_y, result_font, c_text);
                }

                break;
            }

            case SETTINGS_TAB_ABOUT: {
                int font = ui_clamp_px(14, 12, 16);
                int small_font = ui_clamp_px(12, 10, 14);
                int text_y = yoff + content_start_y;

                /* App description */
                const char *desc_lines[] = {
                    "Inner Breeze is a simple breathing",
                    "meditation app to help you relax",
                    "and find your calm."
                };
                for(int i = 0; i < 3; i++) {
                    DrawText(desc_lines[i], content_x, text_y, font, c_text);
                    text_y += ui_px(22);
                }

                /* Version info */
                text_y += ui_px(20);
                char version_text[32];
                snprintf(version_text, sizeof(version_text), "Version %s", INBE_VERSION_STRING);
                DrawText(version_text, content_x, text_y, small_font, ui_darken(c_text, 40));

                /* Icon links */
                int links_y = text_y + ui_px(40);
                int icon_size = ui_clamp_px(ICON_SIZE_LARGE, ICON_SIZE_LARGE_MIN, ICON_SIZE_LARGE_MAX);
                int icon_padding = ui_px(4);
                int icon_spacing = ui_px(20);
                int icon_btn_w = icon_size + icon_padding * 2;
                int total_w = icon_btn_w * 4 + icon_spacing * 3;
                int links_start_x = content_x + (content_w - total_w) / 2;
                ui_draw_icon_link(app, links_start_x + icon_padding, links_y, icon_size, app->telegram_icon, "https://t.me/lotusinbe");
                ui_draw_icon_link(app, links_start_x + icon_btn_w + icon_spacing + icon_padding, links_y, icon_size, app->globe_icon, "https://inbe.waozi.xyz/");
                ui_draw_icon_link(app, links_start_x + (icon_btn_w + icon_spacing) * 2 + icon_padding, links_y, icon_size, app->monero_icon, "https://trocador.app/en/anonpay/?ticker_to=xmr&network_to=Mainnet&address=86CbC3d4a2GhT9auh6X99JhmhTMFKVVk8Q9cLrKTHkBu8LLkoNWgkBeAT3YZrvDM6NczYe8brUJNsTiFmwpWDZYnFG5kzSH&donation=True&simple_mode=True&amount=0.1&name=Inner+Breeze&email=waotzi@proton.me&ticker_from=xmr&network_from=Mainnet&buttonbgcolor=445588&textcolor=ffffff&bgcolor=eaeaffff");
                ui_draw_icon_link(app, links_start_x + (icon_btn_w + icon_spacing) * 3 + icon_padding, links_y, icon_size, app->stripe_icon, "https://donate.stripe.com/4gM3cv5boaR98HH9VvfAc04");
                break;
            }
        }
    EndScissorMode();

    /* Draw dropdown menu (floats above content) */
    ui_draw_dropdown_menu(app, 100);

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if(app->settings_dirty)
            save_settings(app);
    }
}
