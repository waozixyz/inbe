#include "settings_tab.h"
#include "app.h"
#include "app_preferences.h"
#include "app_session.h"
#include "language_tab.h"
#include "meditation_music.h"
#include "locale.h"
#include "theme_meta.h"
#include "flint_ui.h"
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(_WIN32) && !defined(PLATFORM_WEB)
#define INBE_HAS_FLINT_FILE_DIALOG 1
#include "flint_file_dialog.h"
#endif
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include "android_import.h"
#endif
#include "version.h"
#include "data.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

/* Theme colors - set by ui_set_colors */
extern Color c_text, c_bg, c_surface, c_circle, c_button, c_button_hover, c_icon;

extern int view_width;
extern int view_height;

static const char *
settings_current_title(InbeApp *app)
{
    if(app != NULL && app->settings_category == SETTINGS_CATEGORY_PRACTICE) {
        return app->settings_sub_tab == PRACTICE_SUBTAB_SESSION
                   ? locale_get("settings_tab_session")
                   : locale_get("settings_tab_breathing");
    }
    if(app != NULL && app->settings_category == SETTINGS_CATEGORY_MEDITATION)
        return locale_get("meditation_music_settings_title");

    return locale_get("settings_title");
}

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

static void
settings_draw_exact_speed_preview(Inbe *preview, int *preview_speed,
                                  int speed, int max_rounds, int max_breaths,
                                  int pause_seconds, int content_w, int content_h,
                                  int center_x, int center_y)
{
    if(preview == NULL || preview_speed == NULL)
        return;

    if(preview->phase != InbePhaseBreathe) {
        inbeinit(preview);
        apply_settings(preview, speed, max_rounds, max_breaths, pause_seconds);
        preview->progressive_speed = 0;
        session_reset_round_breathe(preview);
    } else if(*preview_speed != speed) {
        apply_settings(preview, speed, max_rounds, max_breaths, pause_seconds);
        preview->progressive_speed = 0;
    }

    if(*preview_speed != speed) {
        *preview_speed = speed;
    }

    update_preview_bounds(preview, content_w, content_h);
    inbestep(preview);
    draw_preview_inbe(preview, center_x, center_y);
}

void
settings_draw_progressive_start_speed_editor(InbeApp *app)
{
    int modal_w = flint_px(340);
    int modal_h = flint_px(360);
    if(modal_w > view_width - flint_px(24))
        modal_w = view_width - flint_px(24);
    if(modal_h > view_height - flint_px(24))
        modal_h = view_height - flint_px(24);
    int modal_x = (view_width - modal_w) / 2;
    int modal_y = (view_height - modal_h) / 2;
    int title_font = flint_ui_font();
    int title_y = modal_y + flint_px(14);
    int close_size = flint_px(22);
    int close_padding = flint_px(8);
    int close_w = close_size + close_padding * 2;
    int close_hover = 0;
    int max_speed = app->inbe.speed_level;
    int start_speed = clampi(app->inbe.progressive_start_speed, SETTINGS_SPEED_MIN, max_speed);

    DrawRectangle(0, 0, view_width, view_height, (Color){0, 0, 0, 180});
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_surface);
    ui_draw_bevel(modal_x, modal_y, modal_w, modal_h,
                  flint_lighten(c_surface, 40), flint_darken(c_surface, 40));

    const char *title = locale_get("progressive_start_speed_editor_title");
    int title_w = flint_text_measure(title, title_font);
    int title_max_w = modal_w - close_w * 2 - flint_px(24);
    while(title_font > flint_px(12) && title_w > title_max_w) {
        title_font--;
        title_w = flint_text_measure(title, title_font);
    }
    flint_text_draw(title, modal_x + (modal_w - title_w) / 2, title_y, title_font, c_text);

    if(ui_draw_icon_btn_padded(modal_x + modal_w - close_w - flint_px(6), modal_y + flint_px(6),
                               close_size, close_padding, app->icons[UI_ICON_TYPE_X], &close_hover)) {
        app->modal.active = 0;
        app->modal.type = UIModalNone;
        return;
    }

    settings_draw_exact_speed_preview(&app->start_speed_preview,
                                      &app->start_speed_preview_speed,
                                      start_speed,
                                      app->inbe.max_rounds,
                                      int_from_count(app->inbe.maxbreaths),
                                      app->inbe.pause_seconds,
                                      modal_w - flint_px(48),
                                      flint_px(210),
                                      modal_x + modal_w / 2,
                                      modal_y + flint_px(150));

    if(ui_draw_slider(12, modal_x + flint_px(24), modal_y + flint_px(250),
                      modal_w - flint_px(48), locale_get("progressive_start_speed_label"),
                      SETTINGS_SPEED_MIN, max_speed, &start_speed, "")) {
        app->inbe.progressive_start_speed = start_speed;
        app->settings_dirty = 1;
    }
}

static void
settings_draw_section_title(const char *title, int x, int *y)
{
    int font = flint_ui_font();
    flint_text_draw(title, x, *y, font, c_text);
    *y += flint_px(34);
}

/* Unified status system variables */
static char unified_status[256] = "";
static char unified_detail[256] = "";
static int unified_status_type = 0;

/* Helper functions for unified status system */
void settings_tab_set_status_success(const char *message, const char *detail) {
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

void settings_tab_set_status_error(const char *message) {
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
settings_tab_clear_status(void)
{
    unified_status[0] = '\0';
    unified_detail[0] = '\0';
    unified_status_type = 0;
}

static void
settings_draw_status(int x, int *y)
{
    if(unified_status[0] == '\0')
        return;

    int status_font = flint_ui_font_small();
    Color status_color = (unified_status_type == 2) ? RED : c_text;

    flint_text_draw(unified_status, x, *y, status_font, status_color);
    *y += flint_px(18);

    if(unified_detail[0] != '\0') {
        flint_text_draw(unified_detail, x, *y, status_font, flint_darken(c_text, 40));
        *y += flint_px(18);
    }
}

static void
settings_format_data_size(char *dst, size_t dst_size, long long data_size)
{
    if(data_size < 1024)
        snprintf(dst, dst_size, "%lld B", data_size);
    else if(data_size < 1024 * 1024)
        snprintf(dst, dst_size, "%.1f KB", (float)data_size / 1024);
    else
        snprintf(dst, dst_size, "%.1f MB", (float)data_size / (1024 * 1024));
}

static void
settings_draw_data_stats(int x, int y, int w)
{
    int font = flint_ui_font();
    int session_count = data_get_session_count();
    long long data_size = data_get_total_size();
    char size_str[32];
    char stat_text[64];
    int stats_box_h = flint_px(66);
    int stat_x = x + flint_px(16);
    int stat_y = y + flint_px(16);

    settings_format_data_size(size_str, sizeof(size_str), data_size);

    DrawRectangle(x, y, w, stats_box_h, flint_darken(c_bg, 8));
    ui_draw_bevel(x, y, w, stats_box_h, flint_lighten(c_bg, 35), flint_darken(c_bg, 45));

    locale_format(stat_text, sizeof(stat_text), "total_sessions_label", session_count);
    flint_text_draw(stat_text, stat_x, stat_y, font, c_text);
    stat_y += flint_px(22);

    locale_format(stat_text, sizeof(stat_text), "data_size_label", size_str);
    flint_text_draw(stat_text, stat_x, stat_y, font, c_text);
}

#if defined(INBE_HAS_FLINT_FILE_DIALOG)
static void
settings_apply_file_dialog_theme(InbeApp *app)
{
    int theme_id = app != NULL ? app->theme_id : ThemeSky;
    int dark_mode = app != NULL && app->dark_mode != 0;

    if(theme_id < 0 || theme_id >= THEME_COUNT)
        theme_id = ThemeSky;
    flint_file_dialog_set_theme_scope(flint_theme_scope_for((FlintThemeId)theme_id,
                                                            dark_mode != 0));
}
#endif

static void
settings_draw_about_block(InbeApp *app, int content_x, int content_w, int *y)
{
    int font = flint_ui_font();
    int small_font = flint_ui_font_small();
    char version_text[32];
    int links_y;
    int icon_size;
    int icon_padding;
    int icon_spacing;
    int icon_btn_w;
    int link_count = 5;
    int max_columns = 5;
    int total_w;
    int columns;
    int grid_w;
    int links_start_x;
    int row_spacing;
    Texture2D icons[5] = {app->icons[UI_ICON_TYPE_DISCORD], app->icons[UI_ICON_TYPE_TELEGRAM], app->icons[UI_ICON_TYPE_GITHUB], app->icons[UI_ICON_TYPE_BTC], app->icons[UI_ICON_TYPE_MONERO]};
    const char *urls[5] = {
        "https://discord.com/invite/JbGZ4yENDt",
        "https://t.me/lotusinbe",
        "https://github.com/waozixyz/inbe",
        "https://trocador.app/en/anonpay/?ticker_to=btc&network_to=Mainnet&address=bc1qxzcetg50f6epgddc09n82xqn3zswlmk44235y5&donation=True&simple_mode=True&amount=0.001&name=Inner+Breeze&email=waotzi@proton.me&ticker_from=btc&network_from=Mainnet&buttonbgcolor=445588&textcolor=ffffff&bgcolor=eaeaffff",
        "https://trocador.app/en/anonpay/?ticker_to=xmr&network_to=Mainnet&address=86CbC3d4a2GhT9auh6X99JhmhTMFKVVk8Q9cLrKTHkBu8LLkoNWgkBeAT3YZrvDM6NczYe8brUJNsTiFmwpWDZYnFG5kzSH&donation=True&simple_mode=True&amount=0.1&name=Inner+Breeze&email=waotzi@proton.me&ticker_from=xmr&network_from=Mainnet&buttonbgcolor=445588&textcolor=ffffff&bgcolor=eaeaffff"
    };

    settings_draw_section_title(locale_get("settings_tab_about"), content_x, y);

    flint_ui_paragraph_draw((FlintUIParagraph){
        .text = locale_get("about_description"),
        .width = content_w,
        .font = font,
        .line_gap = flint_px(10),
        .color = c_text,
    }, content_x, y);

    *y += flint_px(20);
    locale_format(version_text, sizeof(version_text), "version_label", INBE_VERSION_STRING);
    flint_text_draw(version_text, content_x, *y, small_font, flint_darken(c_text, 40));

    links_y = *y + flint_px(40);
    icon_size = flint_px(32);
    icon_padding = flint_px(4);
    icon_spacing = flint_px(20);
    icon_btn_w = icon_size + icon_padding * 2;
    total_w = icon_btn_w * max_columns + icon_spacing * (max_columns - 1);
    columns = total_w <= content_w ? max_columns : 2;
    grid_w = icon_btn_w * columns + icon_spacing * (columns - 1);
    links_start_x = content_x + (content_w - grid_w) / 2;
    row_spacing = flint_px(16);

    for(int i = 0; i < link_count; i++) {
        int col = i % columns;
        int row = i / columns;
        int icon_x = links_start_x + col * (icon_btn_w + icon_spacing) + icon_padding;
        int icon_y = links_y + row * (icon_btn_w + row_spacing);
        ui_draw_icon_link(icon_x, icon_y, icon_size, icons[i], urls[i]);
    }

    *y = links_y + ((link_count + columns - 1) / columns) * (icon_btn_w + row_spacing);
}

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
static void
settings_tab_handle_android_import(void)
{
    char import_path[FS_PATH_MAX];
    int import_result = android_import_poll_result(import_path, sizeof(import_path));

    if(import_result == ANDROID_IMPORT_RESULT_NONE)
        return;

    if(import_result == ANDROID_IMPORT_RESULT_CANCELLED) {
        settings_tab_set_status_error(locale_get("import_cancelled"));
        return;
    }

    if(import_path[0] == '\0') {
        settings_tab_set_status_error(locale_get("import_invalid_file"));
        return;
    }

    if(data_import(import_path)) {
        char import_message[128];
        locale_format(import_message, sizeof(import_message), "imported_sessions", data_get_session_count());
        settings_tab_set_status_success(import_message, NULL);
        TraceLog(LOG_INFO, "DATA: Android import successful");
    } else {
        settings_tab_set_status_error(locale_get("import_failed"));
        TraceLog(LOG_ERROR, "DATA: Android import failed");
    }
}
#endif

void
settings_tab_draw(InbeApp *app)
{
    int title_h = ui_screen_header_height();
    int viewport_h = view_height - title_h - flint_px(TAB_BAR_H);
    int close_clicked = 0;
    int content_x;
    int content_w;
#if defined(INBE_HAS_FLINT_FILE_DIALOG)
    static FlintFileDialog export_dlg;
    static int export_dlg_initialized = 0;
    static FlintFileDialog import_dlg;
    static int import_dlg_initialized = 0;
#endif

    /* Use percentage of screen width, then cap it to the shared DPI-aware max. */
    int responsive_max_w = (int)(view_width * 0.96f);
    int max_content_w = flint_px(CONTENT_MAX_W);
    int min_content_w = flint_px(320);
    if(responsive_max_w > max_content_w)
        responsive_max_w = max_content_w;
    if(responsive_max_w < min_content_w)
        responsive_max_w = min_content_w;
    int side_padding = flint_page_side_padding();
    flint_centered_column(responsive_max_w, side_padding, &content_x, &content_w);

    /* Reset slider drag state when mouse is released */
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        app->settings_drag_slider = 0;
    }

    /* Draw custom header with back button when in a category */
    if(app->settings_category != -1) {
        int title_h = ui_screen_header_height();
        int title_font = flint_ui_font();
        int close_hover = 0;
        int back_hover = 0;

        /* Draw header background */
        DrawRectangle(0, 0, view_width, title_h, flint_darken(c_bg, 14));
        DrawLine(0, title_h - 1, view_width, title_h - 1, flint_darken(c_bg, 42));

        int back_clicked = 0;
        if(!app->settings_from_exercise_selector) {
            int back_btn_x = ui_icon_btn_padding(UI_ICON_SIZE_TINY);
            back_clicked = ui_draw_icon_btn(back_btn_x, flint_px(8),
                                            UI_ICON_SIZE_TINY, app->icons[UI_ICON_TYPE_RETURN],
                                            &back_hover);
        }

        /* Draw centered title (offset to account for back button) */
        const char *title = settings_current_title(app);
        int title_w = flint_text_measure(title, title_font);
        int title_y = flint_ui_text_y(title, 0, title_h, title_font);
        flint_text_draw(title, (view_width - title_w) / 2, title_y, title_font, c_text);

        /* Draw close button */
        close_clicked = ui_draw_icon_btn(view_width - flint_px(40) - ui_icon_btn_padding(UI_ICON_SIZE_TINY), flint_px(8),
                                         UI_ICON_SIZE_TINY, app->icons[UI_ICON_TYPE_X], &close_hover);

        /* Handle back button click */
        if(back_clicked) {
            settings_tab_clear_status();
            if(app->settings_from_exercise_selector) {
                if(app->settings_dirty)
                    save_settings(app);
                app->settings_from_exercise_selector = 0;
                app->settings_category = -1;
                app->settings_sub_tab = 0;
                app->settings_scroll = 0;
                app->inbe.screen = InbeScreenStart;
            } else {
                app->settings_category = -1;
                app->settings_sub_tab = 0;
            }
        }

        if(close_clicked) {
            if(app->settings_dirty)
                save_settings(app);
            settings_tab_clear_status();
            app->settings_from_exercise_selector = 0;
            app->settings_category = -1;
            app->settings_sub_tab = 0;
            app->settings_scroll = 0;
            app->inbe.screen = app->main_tab == APP_MAIN_TAB_HABITS
                                   ? InbeScreenHabits
                                   : InbeScreenStart;
        }
    } else {
        FlintUIHeader header = ui_draw_title_header(title_h, locale_get("settings_title"),
                                                    (Texture2D){0},
                                                    app->icons[UI_ICON_TYPE_X]);
        close_clicked = header.right_clicked;
        if(close_clicked) {
            if(app->settings_dirty)
                save_settings(app);
            settings_tab_clear_status();
            app->settings_from_exercise_selector = 0;
            app->settings_category = -1;
            app->settings_sub_tab = 0;
            app->inbe.screen = app->main_tab == APP_MAIN_TAB_HABITS
                                   ? InbeScreenHabits
                                   : InbeScreenStart;
        }
    }

    /* Initialize export dialog on first call */
#if defined(INBE_HAS_FLINT_FILE_DIALOG)
    if(!export_dlg_initialized) {
        flint_file_dialog_init(&export_dlg);
        export_dlg_initialized = 1;
    }

    /* Initialize import dialog on first call */
    if(!import_dlg_initialized) {
        flint_file_dialog_init(&import_dlg);
        import_dlg_initialized = 1;
    }
#else
    if(app->settings_category == -1) {
        app->settings_sub_tab = 0;
    }
#endif

    int content_start_y = title_h;
    int language_menu_changed = 0;
    int draw_language_menu = 0;
    int draw_meditation_music_menu = 0;
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
    settings_tab_handle_android_import();
#endif

    ui_begin_scissor((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));

    if(app->settings_category == SETTINGS_CATEGORY_PRACTICE) {
        int yoff = flint_px(16);
        int speed = app->inbe.speed_level;
        int max_rounds = app->inbe.max_rounds;
        int max_breaths = int_from_count(app->inbe.maxbreaths);
        int pause_seconds = app->inbe.pause_seconds;
        int subtab_h = flint_px(40);
        int subtab_y = content_start_y;
        int clicked_subtab = -1;
        const char *practice_tabs[] = {
            locale_get("settings_tab_breathing"),
            locale_get("settings_tab_session")
        };

        update_preview_bounds(&app->settings_preview, content_w, flint_px(240));
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        app->settings_preview.progressive_speed = 0;
        inbestep(&app->settings_preview);
        if(app->settings_preview.phase != InbePhaseBreathe) {
            reset_settings_preview(app);
            update_preview_bounds(&app->settings_preview, content_w, flint_px(240));
            apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
            app->settings_preview.progressive_speed = 0;
        }

        clicked_subtab = settings_draw_subtab_bar(subtab_y, subtab_h, practice_tabs,
                                                  PRACTICE_SUBTAB_COUNT,
                                                  app->settings_sub_tab);
        if(clicked_subtab != -1) {
            app->settings_sub_tab = clicked_subtab;
            app->settings_scroll = 0;
            reset_settings_preview(app);
        }

        {
            int tab_view_y = subtab_y + subtab_h + yoff;
            int tab_view_h = view_height - tab_view_y - flint_px(TAB_BAR_H);
            int tab_content_h = app->settings_sub_tab == PRACTICE_SUBTAB_BREATHING
                                    ? flint_px(app->inbe.progressive_speed ? 410 : 360)
                                    : flint_px(365);
            FlintUIScrollArea scroll_area;
            FlintUIScrollView scroll_view;
            int tab_content_y;

            if(tab_view_h < flint_px(80))
                tab_view_h = flint_px(80);
            scroll_area = (FlintUIScrollArea){
                .bounds = {(float)content_x, (float)tab_view_y,
                           (float)content_w, (float)tab_view_h},
                .content_height = tab_content_h,
                .scroll_offset = &app->settings_scroll,
                .wheel_step = flint_px(42)
            };
            scroll_view = ui_scroll_container_begin(scroll_area);
            tab_content_y = scroll_view.content_y;

            if(app->settings_sub_tab == PRACTICE_SUBTAB_BREATHING) {
                int progressive_start_speed = app->inbe.progressive_start_speed;
                int progressive_speed = app->inbe.progressive_speed;
                int toggle_w = flint_px(56);
                int toggle_h = flint_px(30);
                int progressive_label_y = tab_content_y + flint_px(275);
                int progressive_toggle_y = progressive_label_y + flint_px(26);

                draw_preview_inbe(&app->settings_preview, content_x + content_w / 2, tab_content_y + flint_px(100));

                if(ui_draw_slider(1, content_x, tab_content_y + flint_px(200), content_w, locale_get("speed_label"),
                                  SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX, &speed, "")) {
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_preview.progressive_speed = 0;
                    app->settings_dirty = 1;
                }

                flint_text_draw(locale_get("progressive_speed_label"), content_x, progressive_label_y, flint_ui_font(), c_text);
                if(ui_draw_toggle_switch(content_x, progressive_toggle_y, toggle_w, toggle_h, &progressive_speed,
                                         locale_get("toggle_off"), locale_get("toggle_on"))) {
                    app->inbe.progressive_speed = progressive_speed;
                    app->settings_preview.progressive_speed = 0;
                    app->settings_dirty = 1;
                    TraceLog(LOG_INFO, "INBE: Settings toggled progressive_speed to %d", progressive_speed);
                }

                if(app->inbe.progressive_speed) {
                    int modify_y = progressive_toggle_y + toggle_h + flint_px(20);
                    int modify_w = flint_text_measure(locale_get("modify_start_speed_button"), flint_ui_font()) + flint_px(24);
                    int modify_h = flint_px(36);
                    int modify_hover = 0;
                    if(modify_w > content_w)
                        modify_w = content_w;

                    if(progressive_start_speed != clampi(progressive_start_speed, SETTINGS_SPEED_MIN, speed)) {
                        app->inbe.progressive_start_speed = clampi(progressive_start_speed, SETTINGS_SPEED_MIN, speed);
                        app->settings_preview.progressive_start_speed = app->inbe.progressive_start_speed;
                        app->settings_dirty = 1;
                    }

                    if(ui_draw_generic_button(content_x, modify_y, modify_w, modify_h,
                                              locale_get("modify_start_speed_button"),
                                              UI_BUTTON_STYLE_SECONDARY, 0, &modify_hover)) {
                        app->modal.active = 1;
                        app->modal.type = UIModalEditProgressiveStartSpeed;
                        app->modal.selected_button = 0;
                    }
                }
            } else {
                int slider_y = tab_content_y + flint_px(20);
                int advanced_session_controls = app->advanced_session_controls;
                int toggle_w = flint_px(56);
                int toggle_h = flint_px(30);
                int hold_display_label_y = slider_y + flint_px(198);
                int hold_display_y = hold_display_label_y + flint_px(26);
                int advanced_label_y = hold_display_y + flint_px(52);
                int advanced_toggle_y = advanced_label_y + flint_px(26);
                int reset_y = advanced_toggle_y + toggle_h + flint_px(20);
                int reset_w = flint_text_measure(locale_get("reset_to_defaults_label"), flint_ui_font()) + flint_px(24);
                int reset_h = flint_px(36);
                int reset_x = content_x + content_w - reset_w;
                int reset_hover = 0;

                if(ui_draw_slider(2, content_x, slider_y, content_w, locale_get("max_rounds_label"),
                                  1, MaxRounds, &max_rounds, "")) {
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }

                if(ui_draw_slider(3, content_x, slider_y + flint_px(66), content_w, locale_get("max_breaths_label"),
                                  SETTINGS_BREATHS_MIN, SETTINGS_BREATHS_MAX, &max_breaths, "")) {
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }

                if(ui_draw_slider(4, content_x, slider_y + flint_px(132), content_w, locale_get("pause_after_round_label"),
                                  SETTINGS_PAUSE_MIN, SETTINGS_PAUSE_MAX, &pause_seconds, "s")) {
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }

                flint_text_draw(locale_get("hold_display_label"), content_x, hold_display_label_y, flint_ui_font(), c_text);
                draw_hold_display_mode_selector(app, content_x, hold_display_y, content_w);

                flint_text_draw(locale_get("advanced_session_controls_label"), content_x, advanced_label_y, flint_ui_font(), c_text);
                if(ui_draw_toggle_switch(content_x, advanced_toggle_y, toggle_w, toggle_h, &advanced_session_controls,
                                         locale_get("toggle_off"), locale_get("toggle_on"))) {
                    app->advanced_session_controls = advanced_session_controls;
                    app->settings_dirty = 1;
                }

                if(ui_draw_generic_button(reset_x, reset_y, reset_w, reset_h,
                                          locale_get("reset_to_defaults_label"),
                                          UI_BUTTON_STYLE_SECONDARY, 0, &reset_hover)) {
                    speed = DefaultSpeedLevel;
                    max_rounds = DefaultMaxRounds;
                    max_breaths = DefaultMaxBreaths;
                    pause_seconds = DefaultPauseSeconds;
                    app->inbe.progressive_start_speed = DefaultProgressiveStartSpeed;
                    app->settings_preview.progressive_start_speed = DefaultProgressiveStartSpeed;
                    app->advanced_session_controls = 0;
                    app->hold_display_mode = HOLD_DISPLAY_CIRCLE;
                    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                    app->settings_dirty = 1;
                }
            }
            ui_scroll_container_end(scroll_area, scroll_view);
        }
    } else if(app->settings_category == SETTINGS_CATEGORY_MEDITATION) {
        int scroll_y = content_start_y + flint_px(16);
        int scroll_h = view_height - scroll_y - flint_px(TAB_BAR_H);
        int scroll_content_h = flint_px(300);
        int y;
        FlintUIScrollArea scroll_area;
        FlintUIScrollView scroll_view;

        if(scroll_h < flint_px(80))
            scroll_h = flint_px(80);
        scroll_area = (FlintUIScrollArea){
            .bounds = {(float)content_x, (float)scroll_y,
                       (float)content_w, (float)scroll_h},
            .content_height = scroll_content_h,
            .scroll_offset = &app->settings_scroll,
            .wheel_step = flint_px(42)
        };
        scroll_view = ui_scroll_container_begin(scroll_area);
        y = scroll_view.content_y;

        settings_draw_section_title(locale_get("meditation_music_section_title"), content_x, &y);
        meditation_music_draw_settings(app, content_x, content_w, &y);
        draw_meditation_music_menu = 1;
        ui_scroll_container_end(scroll_area, scroll_view);
    } else {
        int subtab_h = flint_px(40);
        int subtab_gap = flint_px(16);
        int subtab_y = content_start_y;
        int tab_content_start_y = subtab_y + subtab_h + subtab_gap;
        int content_viewport_h = view_height - tab_content_start_y - flint_px(TAB_BAR_H);
        int clicked_subtab = -1;
        const char *app_tabs[] = {
            locale_get("settings_tab_app"),
            locale_get("settings_tab_data_about")
        };
        int y = tab_content_start_y - app->settings_scroll;
        int content_top = y;
        int sound_volume = app->sound_volume;
        int keyboard_toggle = app->on_screen_keyboard_enabled;
        int toggle_w = flint_px(56);
        int toggle_h = flint_px(30);
        int data_button_w;
        int data_button_h = flint_px(36);
        int data_hover = 0;

        if(content_viewport_h < 0)
            content_viewport_h = 0;

        clicked_subtab = settings_draw_subtab_bar(subtab_y, subtab_h, app_tabs,
                                                  APP_SETTINGS_TAB_COUNT,
                                                  app->app_settings_tab);
        if(clicked_subtab != -1) {
            app->app_settings_tab = clicked_subtab;
            app->settings_scroll = 0;
            settings_tab_clear_status();
            y = tab_content_start_y;
            content_top = y;
        }

        ui_end_scissor();
        ui_begin_scissor((int)app->camera.offset.x,
                         (int)(app->camera.offset.y + tab_content_start_y * app->camera.zoom),
                         (int)(view_width * app->camera.zoom),
                         (int)(content_viewport_h * app->camera.zoom));

        if(app->app_settings_tab == APP_SETTINGS_TAB_APP) {
            settings_draw_section_title(locale_get("settings_tab_sound"), content_x, &y);
            if(ui_draw_slider(6, content_x, y, content_w, locale_get("volume_label"),
                              SETTINGS_VOLUME_MIN, SETTINGS_VOLUME_MAX, &sound_volume, "")) {
                app->sound_volume = sound_volume;
                app->settings_dirty = 1;
                save_settings(app);
            }
            y += flint_px(74);

#ifdef __ANDROID__
            {
                int play_in_background = app->inbe.play_in_background;
                flint_text_draw(locale_get("play_in_background_label"), content_x, y, flint_ui_font(), c_text);
                if(ui_draw_toggle_switch(content_x, y + flint_px(26), toggle_w, toggle_h,
                                         &play_in_background, locale_get("toggle_off"), locale_get("toggle_on"))) {
                    app->inbe.play_in_background = play_in_background;
                    app->settings_dirty = 1;
                }
                y += flint_px(76);
            }
#endif

            settings_draw_section_title(locale_get("settings_tab_appearance"), content_x, &y);
            {
                app->theme_mode = clampi(app->theme_mode, APP_THEME_SYSTEM, APP_THEME_DARK);
                flint_text_draw(locale_get("theme_mode_label"), content_x, y, flint_ui_font(), c_text);
                ui_draw_dropdown_button(102, content_x, y + flint_px(26), content_w,
                                        flint_px(36), theme_mode_options, 3, &app->theme_mode);
                draw_theme_mode_menu = 1;
                y += flint_px(76);
            }
            {
                int orientation_max = orientation_option_count - 1;
                app->orientation_mode = clampi(app->orientation_mode,
                                               APP_ORIENTATION_SYSTEM,
                                               orientation_max);
                flint_text_draw(locale_get("orientation_label"), content_x, y,
                                flint_ui_font(), c_text);
                ui_draw_dropdown_button(103, content_x, y + flint_px(26), content_w,
                                        flint_px(36), orientation_options, orientation_option_count,
                                        &app->orientation_mode);
                draw_orientation_menu = 1;
                y += flint_px(76);
            }
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
            if(ui_draw_checkbox_toggle(content_x, y, locale_get("fullscreen_label"), &app->fullscreen_enabled)) {
                if(app->fullscreen_enabled && !IsWindowFullscreen())
                    ToggleFullscreen();
                else if(!app->fullscreen_enabled && IsWindowFullscreen())
                    ToggleFullscreen();
                app->settings_dirty = 1;
            }
            y += flint_px(50);
#endif
            flint_text_draw(locale_get("on_screen_keyboard_label"), content_x, y, flint_ui_font(), c_text);
            if(ui_draw_toggle_switch(content_x, y + flint_px(26), toggle_w, toggle_h,
                                     &keyboard_toggle, locale_get("toggle_off"), locale_get("toggle_on"))) {
                app->on_screen_keyboard_enabled = keyboard_toggle;
                app->settings_dirty = 1;
            }
            y += flint_px(76);
            settings_draw_section_title(locale_get("settings_tab_language"), content_x, &y);
#if defined(LOTUS_BUILD)
            flint_text_draw(locale_current_code(), content_x, y, flint_ui_font(), c_text);
#else
            if(language_dropdown_button(app, 101, content_x, y, content_w, flint_px(36), &app->language_index))
                language_menu_changed = 1;
            draw_language_menu = 1;
#endif
            y += flint_px(62);
        } else {
            settings_draw_section_title(locale_get("data_management_label"), content_x, &y);
            data_button_w = flint_text_measure(locale_get("data_management_button"), flint_ui_font()) + flint_px(24);
            if(data_button_w > content_w)
                data_button_w = content_w;
            if(ui_draw_generic_button(content_x, y, data_button_w, data_button_h,
                                      locale_get("data_management_button"),
                                      UI_BUTTON_STYLE_PRIMARY, 0, &data_hover)) {
                app->modal.active = 1;
                app->modal.type = UIModalDataManagement;
                app->modal.selected_button = 0;
            }
            y += data_button_h + flint_px(16);
            settings_draw_status(content_x, &y);
            y += flint_px(26);

            settings_draw_about_block(app, content_x, content_w, &y);
        }
        y += flint_px(40);

        {
            int total_content_h = y - content_top;
            int max_scroll = total_content_h - content_viewport_h;
            if(max_scroll < 0)
                max_scroll = 0;
            app->settings_scroll -= (int)(GetMouseWheelMove() * 24.0f);
            app->settings_scroll = clampi(app->settings_scroll, 0, max_scroll);
            if(max_scroll > 0) {
                int scrollbar_x = (int)(app->camera.offset.x + (content_x + content_w + flint_px(4)) * app->camera.zoom);
                int scrollbar_y = (int)(app->camera.offset.y + tab_content_start_y * app->camera.zoom);
                int scrollbar_viewport = (int)(content_viewport_h * app->camera.zoom);
                int total_content_screen_h = (int)(total_content_h * app->camera.zoom);
                ui_draw_scrollbar(scrollbar_x, scrollbar_y, scrollbar_viewport, total_content_screen_h,
                                  &app->settings_scroll, max_scroll);
            }
        }
    }
    ui_end_scissor();

    /* Draw dropdown menu (floats above content) */
    ui_set_dropdown_clip_top(title_h);
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
    if(draw_meditation_music_menu)
        meditation_music_draw_dropdown_menu(app);
    ui_set_dropdown_clip_top(0);

    if(app->modal.active && app->modal.type == UIModalDataManagement) {
        int modal_w = flint_px(360);
        int modal_h = flint_px(360);
        int modal_x;
        int modal_y;
        int title_font = flint_ui_font();
        int title_w;
        int close_size = flint_px(22);
        int close_padding = flint_px(8);
        int close_w = close_size + close_padding * 2;
        int close_hover = 0;
        int y;
        int button_h = flint_px(36);
        int button_w;
        int hover_import = 0;
        int hover_export = 0;
        int hover_delete = 0;

        if(modal_w > view_width - flint_px(24))
            modal_w = view_width - flint_px(24);
        if(modal_h > view_height - flint_px(24))
            modal_h = view_height - flint_px(24);
        modal_x = (view_width - modal_w) / 2;
        modal_y = (view_height - modal_h) / 2;

        DrawRectangle(0, 0, view_width, view_height, (Color){0, 0, 0, 180});
        DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_surface);
        ui_draw_bevel(modal_x, modal_y, modal_w, modal_h,
                      flint_lighten(c_surface, 40), flint_darken(c_surface, 40));

        title_w = flint_text_measure(locale_get("data_management_label"), title_font);
        flint_text_draw(locale_get("data_management_label"),
                        modal_x + (modal_w - title_w) / 2,
                        modal_y + flint_px(14), title_font, c_text);

        if(ui_draw_icon_btn_padded(modal_x + modal_w - close_w - flint_px(6),
                                   modal_y + flint_px(6), close_size, close_padding,
                                   app->icons[UI_ICON_TYPE_X], &close_hover)) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
            return;
        }

        y = modal_y + flint_px(54);
        settings_draw_data_stats(modal_x + flint_px(18), y, modal_w - flint_px(36));
        y += flint_px(84);

        button_w = modal_w - flint_px(36);
        if(ui_draw_generic_button(modal_x + flint_px(18), y, button_w, button_h,
                                  locale_get("import_data_button"), UI_BUTTON_STYLE_PRIMARY,
                                  0, &hover_import)) {
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
            if(android_import_open_picker()) {
                settings_tab_set_status_success(locale_get("import_data_dialog_title"), NULL);
                app->modal.active = 0;
                app->modal.type = UIModalNone;
            } else {
                settings_tab_set_status_error(locale_get("import_failed"));
            }
#elif defined(INBE_HAS_FLINT_FILE_DIALOG)
            settings_apply_file_dialog_theme(app);
            if(flint_file_dialog_load(&import_dlg, locale_get("import_data_dialog_title"))) {
                const char *path = flint_file_dialog_get_path(&import_dlg);
                if(path != NULL && path[0] != '\0') {
                    if(data_import(path)) {
                        char import_message[128];
                        locale_format(import_message, sizeof(import_message), "imported_sessions", data_get_session_count());
                        settings_tab_set_status_success(import_message, NULL);
                        app->modal.active = 0;
                        app->modal.type = UIModalNone;
                        TraceLog(LOG_INFO, "DATA: Import successful");
                    } else {
                        settings_tab_set_status_error(locale_get("import_failed"));
                        TraceLog(LOG_ERROR, "DATA: Import failed");
                    }
                } else {
                    settings_tab_set_status_error(locale_get("import_invalid_file"));
                    TraceLog(LOG_WARNING, "DATA: No file selected for import");
                }
            } else {
                settings_tab_set_status_error(locale_get("import_cancelled"));
            }
#else
            settings_tab_set_status_error(locale_get("import_failed"));
#endif
        }

        y += button_h + flint_px(12);
        if(ui_draw_generic_button(modal_x + flint_px(18), y, button_w, button_h,
                                  locale_get("export_data_button"), UI_BUTTON_STYLE_PRIMARY,
                                  0, &hover_export)) {
            char export_filename[64];
            data_default_export_filename(export_filename, sizeof(export_filename));

            if(!data_has_any()) {
                settings_tab_set_status_error(locale_get("no_data_to_export"));
            }
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
            else if(data_export(export_filename)) {
                settings_tab_set_status_success(locale_get("exported_label"), NULL);
                app->modal.active = 0;
                app->modal.type = UIModalNone;
                TraceLog(LOG_INFO, "DATA: Export successful (share sheet shown)");
            } else {
                settings_tab_set_status_error(locale_get("export_failed"));
                TraceLog(LOG_ERROR, "DATA: Export failed");
            }
#elif defined(INBE_HAS_FLINT_FILE_DIALOG)
            else {
                settings_apply_file_dialog_theme(app);
                if(flint_file_dialog_save(&export_dlg, locale_get("export_data_dialog_title"), export_filename)) {
                    const char *path = flint_file_dialog_get_path(&export_dlg);
                    if(path != NULL && data_export(path)) {
                        const char *filename = GetFileName(path);
                        settings_tab_set_status_success(locale_get("exported_label"), filename);
                        app->modal.active = 0;
                        app->modal.type = UIModalNone;
                        TraceLog(LOG_INFO, "DATA: Export successful to %s", path);
                    } else {
                        settings_tab_set_status_error(locale_get("export_failed"));
                        TraceLog(LOG_ERROR, "DATA: Export failed");
                    }
                } else {
                    settings_tab_set_status_error(locale_get("export_cancelled"));
                }
            }
#else
            else {
                settings_tab_set_status_error(locale_get("export_failed"));
            }
#endif
        }

        y += button_h + flint_px(12);
        if(ui_draw_generic_button(modal_x + flint_px(18), y, button_w, button_h,
                                  locale_get("delete_all_data_button"),
                                  UI_BUTTON_STYLE_DANGER, 0, &hover_delete)) {
            if(data_has_any()) {
                app->modal.type = UIModalConfirmDeleteData;
                app->modal.selected_button = 0;
            } else {
                settings_tab_set_status_error(locale_get("no_data_to_delete"));
            }
        }

        y += button_h + flint_px(16);
        settings_draw_status(modal_x + flint_px(18), &y);
    }

    if(app->modal.active && app->modal.type == UIModalConfirmDeleteData) {
        int modal_result = ui_draw_modal(locale_get("delete_all_data_title"),
                                         locale_get("delete_all_data_message"),
                                         locale_get("cancel_button"),
                                         locale_get("delete_button"));
        if(modal_result == 1) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
            settings_tab_set_status_error(locale_get("delete_cancelled"));
        } else if(modal_result == 2) {
            long long deleted = data_delete_all();
            app->modal.active = 0;
            app->modal.type = UIModalNone;
            if(deleted > 0) {
                char deleted_message[128];
                locale_format(deleted_message, sizeof(deleted_message), "deleted_sessions", deleted);
                settings_tab_set_status_success(deleted_message, NULL);
            } else {
                settings_tab_set_status_error(locale_get("no_data_to_delete"));
            }
        }
    }

    if(app->modal.active && app->modal.type == UIModalEditProgressiveStartSpeed) {
        settings_draw_progressive_start_speed_editor(app);
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if(app->settings_dirty)
            save_settings(app);
    }
}
