#include "settings_tab.h"
#include "app.h"
#include "app_session.h"
#include "language_tab.h"
#include "locale.h"
#include "flint_ui.h"
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(_WIN32) && !defined(PLATFORM_WEB)
#define INBE_HAS_FLINT_FILE_DIALOG 1
#include "flint_file_dialog.h"
#endif
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include "android_import.h"
#endif
#include "theme.h"
#include "theme_meta.h"
#include "version.h"
#include "data.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

/* Theme colors - set by ui_set_colors */
extern Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

extern int view_width;
extern int view_height;

static const char *
settings_current_title(InbeApp *app)
{
    if(app == NULL || app->settings_category == -1)
        return locale_get("settings_title");

    switch(app->settings_category) {
    case SETTINGS_CATEGORY_PRACTICE:
        return app->settings_sub_tab == PRACTICE_SUBTAB_SESSION
                   ? locale_get("settings_tab_session")
                   : locale_get("settings_tab_breathing");
    case SETTINGS_CATEGORY_APP:
        if(app->settings_sub_tab == APP_SUBTAB_VISUAL)
            return locale_get("settings_tab_appearance");
        if(app->settings_sub_tab == APP_SUBTAB_LANGUAGE)
            return locale_get("settings_tab_language");
        return locale_get("settings_tab_sound");
    case SETTINGS_CATEGORY_ABOUT_DATA:
        return app->settings_sub_tab == ABOUT_DATA_SUBTAB_ABOUT
                   ? locale_get("settings_tab_about")
                   : locale_get("settings_tab_data");
    default:
        return locale_get("settings_title");
    }
}

static void
draw_category_card(InbeApp *app, const char *title, const char *description,
                   Rectangle rect, int *hover)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int title_font = flint_ui_font();
    int desc_font = flint_ui_font();
    int padding_x = flint_px(16);
    int padding_y = flint_px(10);
    int text_w = (int)rect.width - padding_x * 2;

    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;

    if(mx >= (int)rect.x && mx <= (int)(rect.x + rect.width) &&
       my >= (int)rect.y && my <= (int)(rect.y + rect.height)) {
        DrawRectangle((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, c_button_hover);
        ui_draw_bevel((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height,
                      flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
    } else {
        DrawRectangle((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, c_button);
        ui_draw_bevel((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height,
                      flint_lighten(c_button, 40), flint_darken(c_button, 40));
    }

    while(title_font > flint_px(12) && flint_text_measure(title, title_font) > text_w) {
        title_font--;
    }
    int title_y = flint_ui_text_y(title, (int)rect.y + padding_y, title_font, title_font);
    flint_text_draw(title, (int)rect.x + padding_x, title_y, title_font, c_text);

    int desc_y = title_y + title_font + flint_px(8);
    flint_ui_paragraph_draw((FlintUIParagraph){
        .text = description,
        .width = text_w,
        .font = desc_font,
        .line_gap = flint_px(8),
        .color = flint_darken(c_text, 30),
    }, (int)rect.x + padding_x, &desc_y);
}

static void
settings_draw_category_selection(InbeApp *app, int content_x, int content_w, int start_y)
{
    int card_height = flint_px(120);  /* Increased height for multi-line text */
    int card_spacing = flint_px(12);
    int current_y = start_y;

    int hover_practice = 0, hover_app = 0, hover_about_data = 0;

    /* Practice Settings Card */
    Rectangle practice_rect = {content_x, current_y, content_w, card_height};
    draw_category_card(app, locale_get("settings_category_practice"),
                      locale_get("settings_category_practice_desc"),
                      practice_rect, &hover_practice);
    current_y += card_height + card_spacing;

    /* App Preferences Card */
    Rectangle app_rect = {content_x, current_y, content_w, card_height};
    draw_category_card(app, locale_get("settings_category_app"),
                      locale_get("settings_category_app_desc"),
                      app_rect, &hover_app);
    current_y += card_height + card_spacing;

    /* About & Data Card */
    Rectangle about_data_rect = {content_x, current_y, content_w, card_height};
    draw_category_card(app, locale_get("settings_category_about_data"),
                      locale_get("settings_category_about_data_desc"),
                      about_data_rect, &hover_about_data);

    /* Handle card clicks */
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if(hover_practice) {
            app->settings_category = SETTINGS_CATEGORY_PRACTICE;
            app->settings_sub_tab = PRACTICE_SUBTAB_BREATHING;
        } else if(hover_app) {
            app->settings_category = SETTINGS_CATEGORY_APP;
            app->settings_sub_tab = APP_SUBTAB_SOUND;
        } else if(hover_about_data) {
            app->settings_category = SETTINGS_CATEGORY_ABOUT_DATA;
            app->settings_sub_tab = ABOUT_DATA_SUBTAB_DATA;
        }
    }
}

static int
settings_draw_subtab_bar(InbeApp *app, int x, int y, int w, int h,
                        const char **tab_names, int tab_count, int selected_tab)
{
    int tab_w = w / tab_count;
    int clicked_tab = -1;

    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);

    for(int i = 0; i < tab_count; i++) {
        int tab_x = x + i * tab_w;
        Rectangle tab_rect = {tab_x, y, tab_w, h};

        int is_hovered = CheckCollisionPointRec(mouse_world, tab_rect);
        int is_selected = (i == selected_tab);

        if(is_selected) {
            DrawRectangle((int)tab_x, y, (int)tab_w, h, c_button);
            ui_draw_bevel((int)tab_x, y, (int)tab_w, h,
                          flint_lighten(c_button, 40), flint_darken(c_button, 40));
        } else if(is_hovered) {
            DrawRectangle((int)tab_x, y, (int)tab_w, h, c_button_hover);
            ui_draw_bevel((int)tab_x, y, (int)tab_w, h,
                          flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
            app->cursor_clickable = 1;
        } else {
            Color inactive_bg = flint_darken(c_bg, 10);
            DrawRectangle((int)tab_x, y, (int)tab_w, h, inactive_bg);
            ui_draw_bevel((int)tab_x, y, (int)tab_w, h,
                          flint_lighten(inactive_bg, 35), flint_darken(inactive_bg, 45));
        }

        int font = flint_ui_font();
        int text_w = flint_text_measure(tab_names[i], font);
        flint_text_draw(tab_names[i], tab_x + (tab_w - text_w) / 2,
                 flint_ui_text_y(tab_names[i], y, h, font), font, c_text);

        if(is_hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !ui_dropdown_captures_click(mouse_world)) {
            clicked_tab = i;
        }
    }

    return clicked_tab;
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
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, c_button);
    ui_draw_bevel(modal_x, modal_y, modal_w, modal_h,
                  flint_lighten(c_button, 40), flint_darken(c_button, 40));

    const char *title = locale_get("progressive_start_speed_editor_title");
    int title_w = flint_text_measure(title, title_font);
    int title_max_w = modal_w - close_w * 2 - flint_px(24);
    while(title_font > flint_px(12) && title_w > title_max_w) {
        title_font--;
        title_w = flint_text_measure(title, title_font);
    }
    flint_text_draw(title, modal_x + (modal_w - title_w) / 2, title_y, title_font, c_text);

    if(ui_draw_icon_btn_padded(modal_x + modal_w - close_w - flint_px(6), modal_y + flint_px(6),
                               close_size, close_padding, app->x_icon, UI_ICON_TYPE_X, &close_hover)) {
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

#if !defined(LOTUS_BUILD)
static void
draw_theme_selector(InbeApp *app, int x, int y, int w)
{
    if(ui_draw_theme_switcher(x, y, w, locale_get("theme_label"),
                              locale_get("theme_light"), locale_get("theme_dark"),
                              &app->theme_id, &app->dark_mode)) {
        refresh_theme_colors(app->theme_id, app->dark_mode);
        app->settings_dirty = 1;
    }
}
#endif

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
    int viewport_h = view_height - title_h;
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

        /* Draw back button */
        int back_btn_x = ui_icon_btn_padding(UI_ICON_SIZE_TINY);
        int back_clicked = ui_draw_icon_btn(back_btn_x, flint_px(8),
                                              UI_ICON_SIZE_TINY, app->return_icon, UI_ICON_TYPE_RETURN, &back_hover);

        /* Draw centered title (offset to account for back button) */
        const char *title = settings_current_title(app);
        int title_w = flint_text_measure(title, title_font);
        int title_y = flint_ui_text_y(title, 0, title_h, title_font);
        flint_text_draw(title, (view_width - title_w) / 2, title_y, title_font, c_text);

        /* Draw close button */
        close_clicked = ui_draw_icon_btn(view_width - flint_px(40) - ui_icon_btn_padding(UI_ICON_SIZE_TINY), flint_px(8),
                                         UI_ICON_SIZE_TINY, app->x_icon, UI_ICON_TYPE_X, &close_hover);

        /* Handle back button click */
        if(back_clicked) {
            settings_tab_clear_status();
            app->settings_category = -1;  /* Return to category selection */
            app->settings_sub_tab = 0;
        }

        if(close_clicked) {
            if(app->settings_dirty)
                save_settings(app);
            settings_tab_clear_status();
            app->settings_category = -1;
            app->settings_sub_tab = 0;
            app->settings_scroll = 0;
            app->inbe.screen = InbeScreenStart;  /* Close button always exits to homepage */
        }
    } else {
        /* Use standard header for category selection */
        close_clicked = ui_draw_screen_header(locale_get("settings_title"), 1);
        if(close_clicked) {
            if(app->settings_dirty)
                save_settings(app);
            settings_tab_clear_status();
            app->settings_category = -1;  /* Reset navigation */
            app->settings_sub_tab = 0;
            app->inbe.screen = InbeScreenStart;
        }
    }

    /* Initialize export dialog on first call */
#if defined(INBE_HAS_FLINT_FILE_DIALOG)
    if(!export_dlg_initialized) {
        flint_file_dialog_init(&export_dlg);
        export_dlg_initialized = 1;
        app->settings_category = -1;  /* Initialize to category selection */
        app->settings_sub_tab = 0;
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

    int content_start_y = title_h + flint_px(8);
    int language_menu_changed = 0;
    int draw_language_menu = 0;

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    settings_tab_handle_android_import();
#endif

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));

    /* Show category selection or sub-tabs based on state */
    if(app->settings_category == -1) {
        /* Category selection screen */
        settings_draw_category_selection(app, content_x, content_w, content_start_y);
    } else {
        /* Show sub-tabs for selected category */
        int yoff = flint_px(16);
        int speed = app->inbe.speed_level;
        int max_rounds = app->inbe.max_rounds;
        int max_breaths = int_from_count(app->inbe.maxbreaths);
        int pause_seconds = app->inbe.pause_seconds;

        /* Update preview for breathing tab */
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

        /* Draw sub-tab bar and handle content based on category */
        int subtab_h = flint_px(40);
        int subtab_y = content_start_y;
        int clicked_subtab = -1;

        switch(app->settings_category) {
            case SETTINGS_CATEGORY_PRACTICE: {
                const char *practice_tabs[] = {
                    locale_get("settings_tab_breathing"),
                    locale_get("settings_tab_session")
                };
                clicked_subtab = settings_draw_subtab_bar(app, content_x, subtab_y, content_w, subtab_h,
                                                          practice_tabs, PRACTICE_SUBTAB_COUNT,
                                                          app->settings_sub_tab);
                if(clicked_subtab != -1) {
                    app->settings_sub_tab = clicked_subtab;
                    reset_settings_preview(app);
                }

                int tab_content_y = subtab_y + subtab_h + yoff;
                if(app->settings_sub_tab == PRACTICE_SUBTAB_BREATHING) {
                    int progressive_start_speed = app->inbe.progressive_start_speed;

                    draw_preview_inbe(&app->settings_preview, content_x + content_w / 2, tab_content_y + flint_px(100));

                    if(ui_draw_slider(1, content_x, tab_content_y + flint_px(200), content_w, locale_get("speed_label"), SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX, &speed, "")) {
                        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                        app->settings_preview.progressive_speed = 0;
                        app->settings_dirty = 1;
                    }

                    /* Progressive speed - under Speed slider */
                    int progressive_speed = app->inbe.progressive_speed;
                    int toggle_w = flint_px(56);
                    int toggle_h = flint_px(30);
                    int toggle_x = content_x;
                    int progressive_label_y = tab_content_y + flint_px(275);
                    int progressive_toggle_y = progressive_label_y + flint_px(26);
                    flint_text_draw(locale_get("progressive_speed_label"), content_x, progressive_label_y, flint_ui_font(), c_text);
                    if(ui_draw_toggle_switch(toggle_x, progressive_toggle_y, toggle_w, toggle_h, &progressive_speed, locale_get("toggle_off"), locale_get("toggle_on"))) {
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
                                                  UI_BUTTON_STYLE_SECONDARY, &modify_hover)) {
                            app->modal.active = 1;
                            app->modal.type = UIModalEditProgressiveStartSpeed;
                            app->modal.selected_button = 0;
                        }
                    }

                } else if(app->settings_sub_tab == PRACTICE_SUBTAB_SESSION) {
                    int slider_y = tab_content_y + flint_px(20);

                    /* Max rounds */
                    if(ui_draw_slider(2, content_x, slider_y, content_w, locale_get("max_rounds_label"), 1,
                                   MaxRounds, &max_rounds, "")) {
                        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                        app->settings_dirty = 1;
                    }

                    /* Max breaths */
                    if(ui_draw_slider(3, content_x, slider_y + flint_px(66), content_w, locale_get("max_breaths_label"), SETTINGS_BREATHS_MIN,
                                   SETTINGS_BREATHS_MAX, &max_breaths, "")) {
                        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                        app->settings_dirty = 1;
                    }

                    /* Pause */
                    if(ui_draw_slider(4, content_x, slider_y + flint_px(132), content_w, locale_get("pause_after_round_label"), SETTINGS_PAUSE_MIN,
                                   SETTINGS_PAUSE_MAX, &pause_seconds, "s")) {
                        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                        app->settings_dirty = 1;
                    }

                    /* Advanced controls */
                    int advanced_session_controls = app->advanced_session_controls;
                    int toggle_w = flint_px(56);
                    int toggle_h = flint_px(30);
                    int hold_display_label_y = slider_y + flint_px(198);
                    int hold_display_y = hold_display_label_y + flint_px(26);
                    int advanced_label_y = hold_display_y + flint_px(52);
                    int advanced_toggle_y = advanced_label_y + flint_px(26);

                    flint_text_draw(locale_get("hold_display_label"), content_x, hold_display_label_y, flint_ui_font(), c_text);
                    draw_hold_display_mode_selector(app, content_x, hold_display_y, content_w);

                    flint_text_draw(locale_get("advanced_session_controls_label"), content_x, advanced_label_y, flint_ui_font(), c_text);
                    if(ui_draw_toggle_switch(content_x, advanced_toggle_y, toggle_w, toggle_h, &advanced_session_controls, locale_get("toggle_off"), locale_get("toggle_on"))) {
                        app->advanced_session_controls = advanced_session_controls;
                        app->settings_dirty = 1;
                    }

                    /* Reset to defaults button using Flint utility */
                    int reset_y = advanced_toggle_y + toggle_h + flint_px(20);
                    int reset_w = flint_text_measure(locale_get("reset_to_defaults_label"), flint_ui_font()) + flint_px(24);
                    int reset_h = flint_px(36);
                    int reset_x = content_x + content_w - reset_w;
                    int reset_hover = 0;

                    if(ui_draw_generic_button(reset_x, reset_y, reset_w, reset_h,
                                           locale_get("reset_to_defaults_label"), UI_BUTTON_STYLE_SECONDARY, &reset_hover)) {
                        /* Reset to default values */
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
                break;
            }
            case SETTINGS_CATEGORY_APP: {
                const char *app_tabs[] = {
                    locale_get("settings_tab_sound"),
                    locale_get("settings_tab_appearance"),
                    locale_get("settings_tab_language")
                };
                clicked_subtab = settings_draw_subtab_bar(app, content_x, subtab_y, content_w, subtab_h,
                                                          app_tabs, APP_SUBTAB_COUNT,
                                                          app->settings_sub_tab);
                if(clicked_subtab != -1) {
                    app->settings_sub_tab = clicked_subtab;
                }

                int tab_content_y = subtab_y + subtab_h + yoff;
                if(app->settings_sub_tab == APP_SUBTAB_SOUND) {
                    int slider_y = tab_content_y + flint_px(20);
                    int sound_volume = app->sound_volume;
                    if(ui_draw_slider(6, content_x, slider_y, content_w, locale_get("volume_label"), SETTINGS_VOLUME_MIN,
                                   SETTINGS_VOLUME_MAX, &sound_volume, "")) {
                        app->sound_volume = sound_volume;
                        app->settings_dirty = 1;
                        /* IMMEDIATE SAVE: Persist volume change right away */
                        save_settings(app);
                    }

#ifdef __ANDROID__
                    int play_in_background = app->inbe.play_in_background;
                    int toggle_w = flint_px(56);
                    int toggle_h = flint_px(30);
                    int play_bg_label_y = slider_y + flint_px(66);
                    int play_bg_toggle_y = play_bg_label_y + flint_px(26);
                    flint_text_draw(locale_get("play_in_background_label"), content_x, play_bg_label_y,
                             flint_ui_font(), c_text);
                    if(ui_draw_toggle_switch(content_x, play_bg_toggle_y, toggle_w, toggle_h,
                                             &play_in_background, locale_get("toggle_off"), locale_get("toggle_on"))) {
                        app->inbe.play_in_background = play_in_background;
                        app->settings_dirty = 1;
                        TraceLog(LOG_INFO, "INBE: Settings toggled play_in_background to %d", play_in_background);
                    }
#endif

                } else if(app->settings_sub_tab == APP_SUBTAB_VISUAL) {
                    int keyboard_toggle = app->on_screen_keyboard_enabled;
                    int toggle_w = flint_px(56);
                    int toggle_h = flint_px(30);
                    int keyboard_label_y;
                    int keyboard_toggle_y;
                    int theme_y;
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
                    int checkbox_y = tab_content_y;
                    if(ui_draw_checkbox_toggle(content_x, checkbox_y, locale_get("fullscreen_label"), &app->fullscreen_enabled)) {
                        if(app->fullscreen_enabled && !IsWindowFullscreen())
                            ToggleFullscreen();
                        else if(!app->fullscreen_enabled && IsWindowFullscreen())
                            ToggleFullscreen();
                        app->settings_dirty = 1;
                    }

                    keyboard_label_y = tab_content_y + flint_px(50);
#else
                    keyboard_label_y = tab_content_y;
#endif
                    keyboard_toggle_y = keyboard_label_y + flint_px(26);
                    flint_text_draw(locale_get("on_screen_keyboard_label"), content_x, keyboard_label_y, flint_ui_font(), c_text);
                    if(ui_draw_toggle_switch(content_x, keyboard_toggle_y, toggle_w, toggle_h,
                                             &keyboard_toggle, locale_get("toggle_off"), locale_get("toggle_on"))) {
                        app->on_screen_keyboard_enabled = keyboard_toggle;
                        app->settings_dirty = 1;
                    }

#if !defined(LOTUS_BUILD)
                    theme_y = keyboard_toggle_y + toggle_h + flint_px(24);
                    draw_theme_selector(app, content_x, theme_y, content_w);
#else
                    (void)theme_y;
#endif
                } else if(app->settings_sub_tab == APP_SUBTAB_LANGUAGE) {
                    int font = flint_ui_font();
                    int label_y = tab_content_y;
#if defined(LOTUS_BUILD)
                    flint_text_draw(locale_get("language_label"), content_x, label_y, font, c_text);
                    flint_text_draw(locale_current_code(), content_x, label_y + flint_px(28), font, c_text);
#else
                    int dropdown_y = label_y + flint_px(28);
                    flint_text_draw(locale_get("language_label"), content_x, label_y, font, c_text);
                    if(language_dropdown_button(app, 101, content_x, dropdown_y, content_w, flint_px(36), &app->language_index))
                        language_menu_changed = 1;
                    draw_language_menu = 1;
#endif
                }
                break;
            }
            case SETTINGS_CATEGORY_ABOUT_DATA: {
                const char *about_data_tabs[] = {
                    locale_get("settings_tab_data"),
                    locale_get("settings_tab_about")
                };
                clicked_subtab = settings_draw_subtab_bar(app, content_x, subtab_y, content_w, subtab_h,
                                                          about_data_tabs, ABOUT_DATA_SUBTAB_COUNT,
                                                          app->settings_sub_tab);
                if(clicked_subtab != -1) {
                    app->settings_sub_tab = clicked_subtab;
                }

                int tab_content_y = subtab_y + subtab_h + yoff;
                if(app->settings_sub_tab == ABOUT_DATA_SUBTAB_DATA) {
                    int font = flint_ui_font();
                    int text_y = tab_content_y;
                    int session_count = data_get_session_count();
                    long long data_size = data_get_total_size();
                    char size_str[32];
                    int hover_delete = 0;

                    /* Format data size */
                    if(data_size < 1024)
                        snprintf(size_str, sizeof(size_str), "%lld B", data_size);
                    else if(data_size < 1024 * 1024)
                        snprintf(size_str, sizeof(size_str), "%.1f KB", (float)data_size / 1024);
                    else
                        snprintf(size_str, sizeof(size_str), "%.1f MB", (float)data_size / (1024 * 1024));

                    /* Title */
                    flint_text_draw(locale_get("data_management_label"), content_x, text_y, font, c_text);
                    text_y += flint_px(30);

                    /* Statistics box */
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID)
                    int stats_box_h = flint_px(90);
#else
                    int stats_box_h = flint_px(66);  /* Smaller on Android (no storage line) */
#endif
                    DrawRectangle(content_x, text_y, content_w, stats_box_h, flint_darken(c_bg, 8));
                    ui_draw_bevel(content_x, text_y, content_w, stats_box_h, flint_lighten(c_bg, 35), flint_darken(c_bg, 45));

                    int stat_x = content_x + flint_px(16);
                    int stat_y = text_y + flint_px(16);
                    char stat_text[64];

                    locale_format(stat_text, sizeof(stat_text), "total_sessions_label", session_count);
                    flint_text_draw(stat_text, stat_x, stat_y, font, c_text);
                    stat_y += flint_px(22);

                    locale_format(stat_text, sizeof(stat_text), "data_size_label", size_str);
                    flint_text_draw(stat_text, stat_x, stat_y, font, c_text);
                    stat_y += flint_px(22);

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
                    {
                        char storage_text[FS_PATH_MAX + 32];
                        locale_format(storage_text, sizeof(storage_text), "storage_label", display_path);
                        flint_text_draw(storage_text, stat_x, stat_y, flint_ui_font_small(), flint_darken(c_text, 40));
                    }
#endif

                    text_y += stats_box_h + flint_px(24);

                    /* Import button */
                    int import_h = flint_px(36);
                    int import_w = flint_text_measure(locale_get("import_data_button"), font) + flint_px(24);
                    int import_x = content_x;
                    int import_y = text_y;
                    int hover_import = 0;
                    int hover_export = 0;

                    /* Export button */
                    int export_h = flint_px(36);
                    int export_w = flint_text_measure(locale_get("export_data_button"), font) + flint_px(24);
                    int export_x = content_x;
                    int export_y = import_y + import_h + flint_px(12);

                    /* Import button using Flint utility */
                    if(ui_draw_generic_button(import_x, import_y, import_w, import_h,
                                           locale_get("import_data_button"), UI_BUTTON_STYLE_PRIMARY, &hover_import)) {
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
                        if(android_import_open_picker()) {
                            settings_tab_set_status_success(locale_get("import_data_dialog_title"), NULL);
                        } else {
                            settings_tab_set_status_error(locale_get("import_failed"));
                        }
#elif defined(INBE_HAS_FLINT_FILE_DIALOG)
                        if(flint_file_dialog_load(&import_dlg, locale_get("import_data_dialog_title"))) {
                            const char *path = flint_file_dialog_get_path(&import_dlg);
                            if(path != NULL && path[0] != '\0') {
                                if(data_import(path)) {
                                    char import_message[128];
                                    locale_format(import_message, sizeof(import_message), "imported_sessions", data_get_session_count());
                                    settings_tab_set_status_success(import_message, NULL);
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

                    /* Export button using Flint utility */
                    if(ui_draw_generic_button(export_x, export_y, export_w, export_h,
                                           locale_get("export_data_button"), UI_BUTTON_STYLE_PRIMARY, &hover_export)) {
                        if(!data_has_any()) {
                            settings_tab_set_status_error(locale_get("no_data_to_export"));
                        }
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
                        else if(data_export("inbe-export.zip")) {
                            settings_tab_set_status_success(locale_get("exported_label"), NULL);
                            TraceLog(LOG_INFO, "DATA: Export successful (share sheet shown)");
                        } else {
                            settings_tab_set_status_error(locale_get("export_failed"));
                            TraceLog(LOG_ERROR, "DATA: Export failed");
                        }
#elif defined(INBE_HAS_FLINT_FILE_DIALOG)
                        else if(flint_file_dialog_save(&export_dlg, locale_get("export_data_dialog_title"), "inbe-export.zip")) {
                            const char *path = flint_file_dialog_get_path(&export_dlg);
                            if(path != NULL && data_export(path)) {
                                const char *filename = GetFileName(path);
                                settings_tab_set_status_success(locale_get("exported_label"), filename);
                                TraceLog(LOG_INFO, "DATA: Export successful to %s", path);
                            } else {
                                settings_tab_set_status_error(locale_get("export_failed"));
                                TraceLog(LOG_ERROR, "DATA: Export failed");
                            }
                        } else {
                            settings_tab_set_status_error(locale_get("export_cancelled"));
                        }
#else
                        else {
                            settings_tab_set_status_error(locale_get("export_failed"));
                        }
#endif
                    }

                    /* Delete All button */
                    int delete_w = flint_text_measure(locale_get("delete_all_data_button"), font) + flint_px(24);
                    int delete_x = content_x;
                    int delete_y = export_y + export_h + flint_px(12);

                    if(ui_draw_generic_button(delete_x, delete_y, delete_w, export_h,
                                              locale_get("delete_all_data_button"),
                                              UI_BUTTON_STYLE_DANGER, &hover_delete)) {
                        if(data_has_any()) {
                            app->modal.active = 1;
                            app->modal.type = UIModalConfirmDeleteData;
                            app->modal.selected_button = 0;
                        } else {
                            settings_tab_set_status_error(locale_get("no_data_to_delete"));
                        }
                    }

                    /* Draw unified status area at bottom */
                    if(unified_status[0] != '\0') {
                        int status_font = flint_ui_font_small();
                        int status_x = content_x;
                        int status_y = delete_y + export_h + flint_px(16);

                        /* Main status message with color coding */
                        Color status_color = (unified_status_type == 2) ? RED : c_text;
                        flint_text_draw(unified_status, status_x, status_y, status_font, status_color);

                        /* Detail line (filename, count, etc.) */
                        if(unified_detail[0] != '\0') {
                            int detail_y = status_y + flint_px(16);
                            flint_text_draw(unified_detail, status_x, detail_y, status_font,
                                    flint_darken(c_text, 40));
                        }
                    }
                } else if(app->settings_sub_tab == ABOUT_DATA_SUBTAB_ABOUT) {
                    int font = flint_ui_font();
                    int small_font = flint_ui_font_small();
                    int text_y = tab_content_y;

                    /* App description */
                    const char *desc_text = locale_get("about_description");
                    flint_ui_paragraph_draw((FlintUIParagraph){
                        .text = desc_text,
                        .width = content_w,
                        .font = font,
                        .line_gap = flint_px(10),
                        .color = c_text,
                    }, content_x, &text_y);

                    /* Version info */
                    text_y += flint_px(20);
                    char version_text[32];
                    locale_format(version_text, sizeof(version_text), "version_label", INBE_VERSION_STRING);
                    flint_text_draw(version_text, content_x, text_y, small_font, flint_darken(c_text, 40));

                    /* Icon links */
                    int links_y = text_y + flint_px(40);
                    int icon_size = flint_px(32);
                    int icon_padding = flint_px(4);
                    int icon_spacing = flint_px(20);
                    int icon_btn_w = icon_size + icon_padding * 2;
                    int link_count = 4;
                    int max_columns = 4;
                    int total_w = icon_btn_w * max_columns + icon_spacing * (max_columns - 1);
                    int columns = total_w <= content_w ? max_columns : 2;
                    int grid_w = icon_btn_w * columns + icon_spacing * (columns - 1);
                    int links_start_x = content_x + (content_w - grid_w) / 2;
                    int row_spacing = flint_px(16);
                    Texture2D icons[4] = {app->discord_icon, app->telegram_icon, app->btc_icon, app->monero_icon};
                    UIIconType icon_types[4] = {UI_ICON_TYPE_NONE, UI_ICON_TYPE_TELEGRAM, UI_ICON_TYPE_BTC, UI_ICON_TYPE_MONERO};
                    const char *urls[4] = {
                        "https://discord.com/invite/JbGZ4yENDt",
                        "https://t.me/lotusinbe",
                        "https://trocador.app/en/anonpay/?ticker_to=btc&network_to=Mainnet&address=bc1qxzcetg50f6epgddc09n82xqn3zswlmk44235y5&donation=True&simple_mode=True&amount=0.001&name=Inner+Breeze&email=waotzi@proton.me&ticker_from=btc&network_from=Mainnet&buttonbgcolor=445588&textcolor=ffffff&bgcolor=eaeaffff",
                        "https://trocador.app/en/anonpay/?ticker_to=xmr&network_to=Mainnet&address=86CbC3d4a2GhT9auh6X99JhmhTMFKVVk8Q9cLrKTHkBu8LLkoNWgkBeAT3YZrvDM6NczYe8brUJNsTiFmwpWDZYnFG5kzSH&donation=True&simple_mode=True&amount=0.1&name=Inner+Breeze&email=waotzi@proton.me&ticker_from=xmr&network_from=Mainnet&buttonbgcolor=445588&textcolor=ffffff&bgcolor=eaeaffff"
                    };

                    for(int i = 0; i < link_count; i++) {
                        int col = i % columns;
                        int row = i / columns;
                        int icon_x = links_start_x + col * (icon_btn_w + icon_spacing) + icon_padding;
                        int icon_y = links_y + row * (icon_btn_w + row_spacing);
                        ui_draw_icon_link(icon_x, icon_y, icon_size, icons[i], icon_types[i], urls[i]);
                    }
                }
                break;
            }
        }
    }
    EndScissorMode();

    /* Draw dropdown menu (floats above content) */
    if(draw_language_menu && language_dropdown_menu(app, 101))
        language_menu_changed = 1;
    if(language_menu_changed)
        apply_language_selection(app, app->language_index, 1);

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
