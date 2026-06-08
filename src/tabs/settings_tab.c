#include "settings_tab.h"
#include "app.h"
#include "language_tab.h"
#include "locale.h"
#include "ui/ui.h"
#include "flint_text_layout.h"
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

extern int view_width;
extern int view_height;

static void
draw_category_card(InbeApp *app, const char *title, const char *description,
                   Rectangle rect, int *hover)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int title_font = flint_clamp_px(18, 16, 20);
    int desc_font = flint_clamp_px(14, 12, 16);
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

    while(title_font > flint_px(12) && MeasureText(title, title_font) > text_w) {
        title_font--;
    }
    int title_y = (int)rect.y + padding_y;
    DrawText(title, (int)rect.x + padding_x, title_y, title_font, c_text);

    FlintTextLayout desc_layout = flint_text_layout_parse(description, (Texture2D){0}, FLINT_ICON_TYPE_NONE, desc_font);
    flint_text_layout_reflow(&desc_layout, text_w, desc_font, flint_px(18));
    int desc_y = title_y + title_font + flint_px(6);
    flint_text_layout_draw(&desc_layout, (int)rect.x + padding_x, &desc_y, desc_font, flint_darken(c_text, 30));
    flint_text_layout_free(&desc_layout);
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
            DrawRectangle((int)tab_x, y, (int)tab_w, h, c_bg);
            ui_draw_bevel((int)tab_x, y, (int)tab_w, h,
                          flint_lighten(c_bg, 35), flint_darken(c_bg, 45));
        }

        int font = flint_clamp_px(14, 12, 16);
        int text_w = MeasureText(tab_names[i], font);
        DrawText(tab_names[i], tab_x + (tab_w - text_w) / 2, y + (h - font) / 2 - 1, font, c_text);

        if(is_hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !ui_dropdown_captures_click(mouse_world)) {
            clicked_tab = i;
        }
    }

    return clicked_tab;
}

#if !defined(LOTUS_BUILD)
static void
draw_theme_selector(InbeApp *app, int x, int y, int w)
{
    int font = flint_clamp_px(14, 12, 16);
    int small_font = flint_clamp_px(12, 10, 14);
    const char *label = locale_get("theme_label");

    DrawText(label, x, y, font, c_text);

    /* Light/Dark toggle */
    int toggle_w = flint_px(100);
    int toggle_h = flint_px(28);
    int toggle_x = x + w - toggle_w;
    int toggle_y = y - 2;

    if(ui_draw_toggle_switch(app, toggle_x, toggle_y, toggle_w, toggle_h, &app->dark_mode,
                             locale_get("toggle_off"), locale_get("toggle_on"))) {
        refresh_theme_colors(app->theme_id, app->dark_mode);
        app->settings_dirty = 1;
    }

    int circle_size = flint_px(36);
    int circle_spacing = flint_px(24);
    int row_spacing = flint_px(36);
    int per_row = 3;
    int row_width = per_row * circle_size + (per_row - 1) * circle_spacing;
    int start_x = x + (w - row_width) / 2;
    int circle_y = y + flint_px(64);
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);

    for(int i = 0; i < THEME_COUNT; i++) {
        int row = i / per_row;
        int col = i % per_row;
        int cx = start_x + col * (circle_size + circle_spacing) + circle_size / 2;
        int cy = circle_y + row * (circle_size + row_spacing);

        Color theme_color = c_circle;
        if(!flint_theme_catalog_color((FlintThemeId)i, app->dark_mode != 0, "circle", &theme_color)) {
            const char *scope = app->dark_mode ? g_themes[i].dark_scope : g_themes[i].light_scope;
            theme_color = theme_get(scope, "circle");
        }
        DrawCircle(cx, cy, circle_size / 2, theme_color);

        /* Draw selection ring */
        if(app->theme_id == i) {
            DrawCircleLines(cx, cy, circle_size / 2 + 2, c_text);
        } else {
            DrawCircleLines(cx, cy, circle_size / 2 + 1, flint_darken(c_bg, 30));
        }

        /* Check for click */
        Rectangle bounds = {cx - circle_size / 2 - 4, cy - circle_size / 2 - 4, circle_size + 8, circle_size + 8};
        if(CheckCollisionPointRec(mouse_world, bounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           !ui_dropdown_captures_click(mouse_world)) {
            app->theme_id = i;
            refresh_theme_colors(app->theme_id, app->dark_mode);
            app->settings_dirty = 1;
        }

        if(CheckCollisionPointRec(mouse_world, bounds))
            app->cursor_clickable = 1;

        /* Draw theme name below */
        const char *name = g_themes[i].name;
        int name_w = MeasureText(name, small_font);
        DrawText(name, cx - name_w / 2, cy + circle_size / 2 + flint_px(6), small_font, c_text);
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
    static FileDialog export_dlg;
    static int export_dlg_initialized = 0;
    static char export_result[128] = "";  /* Store export result message */

    /* Use percentage of screen width like tutorial, not DPI-scaled CONTENT_MAX_W */
    int responsive_max_w = (int)(view_width * 0.96f);
    int min_content_w = flint_px(320);
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
        int title_font = flint_clamp_px(16, 14, 18);
        int close_hover = 0;
        int back_hover = 0;

        /* Draw header background */
        DrawRectangle(0, 0, view_width, title_h, flint_darken(c_bg, 14));
        DrawLine(0, title_h - 1, view_width, title_h - 1, flint_darken(c_bg, 42));

        /* Draw back button */
        int back_btn_x = ui_icon_btn_padding(UI_ICON_SIZE_TINY);
        int back_clicked = ui_draw_icon_btn(app, back_btn_x, flint_px(8),
                                              UI_ICON_SIZE_TINY, app->return_icon, UI_ICON_TYPE_RETURN, &back_hover);

        /* Draw centered title (offset to account for back button) */
        const char *title = locale_get("settings_title");
        int title_w = MeasureText(title, title_font);
        int title_y = (title_h - title_font) / 2;
        DrawText(title, (view_width - title_w) / 2, title_y, title_font, c_text);

        /* Draw close button */
        close_clicked = ui_draw_icon_btn(app, view_width - flint_px(40) - ui_icon_btn_padding(UI_ICON_SIZE_TINY), flint_px(8),
                                         UI_ICON_SIZE_TINY, app->x_icon, UI_ICON_TYPE_X, &close_hover);

        /* Handle back button click */
        if(back_clicked) {
            app->settings_category = -1;  /* Return to category selection */
            app->settings_sub_tab = 0;
        }

        if(close_clicked) {
            if(app->settings_dirty)
                save_settings(app);
            app->settings_category = -1;
            app->settings_sub_tab = 0;
            app->settings_scroll = 0;
            app->inbe.screen = InbeScreenStart;  /* Close button always exits to homepage */
        }
    } else {
        /* Use standard header for category selection */
        close_clicked = ui_draw_screen_header(app, locale_get("settings_title"), 1);
        if(close_clicked) {
            if(app->settings_dirty)
                save_settings(app);
            app->settings_category = -1;  /* Reset navigation */
            app->settings_sub_tab = 0;
            app->inbe.screen = InbeScreenStart;
        }
    }

    /* Initialize export dialog on first call */
    if(!export_dlg_initialized) {
        file_dialog_init(&export_dlg);
        export_dlg_initialized = 1;
        app->settings_category = -1;  /* Initialize to category selection */
        app->settings_sub_tab = 0;
    }

    int content_start_y = title_h + flint_px(8);
    int language_menu_changed = 0;
    int draw_language_menu = 0;

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
                    draw_preview_inbe(&app->settings_preview, content_x + content_w / 2, tab_content_y + flint_px(100));

                    if(ui_draw_slider(app, 1, content_x, tab_content_y + flint_px(200), content_w, locale_get("speed_label"), SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX, &speed, "")) {
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
                    DrawText(locale_get("progressive_speed_label"), content_x, progressive_label_y, flint_clamp_px(14, 12, 16), c_text);
                    if(ui_draw_toggle_switch(app, toggle_x, progressive_toggle_y, toggle_w, toggle_h, &progressive_speed, locale_get("toggle_off"), locale_get("toggle_on"))) {
                        app->inbe.progressive_speed = progressive_speed;
                        app->settings_preview.progressive_speed = 0;
                        app->settings_dirty = 1;
                        TraceLog(LOG_INFO, "INBE: Settings toggled progressive_speed to %d", progressive_speed);
                    }

#ifdef __ANDROID__
                    /* Play in background (Android only) */
                    int play_in_background = app->inbe.play_in_background;
                    int play_bg_label_y = progressive_toggle_y + toggle_h + flint_px(18);
                    int play_bg_toggle_y = play_bg_label_y + flint_px(26);
                    DrawText(locale_get("play_in_background_label"), content_x, play_bg_label_y, flint_clamp_px(14, 12, 16), c_text);
                    if(ui_draw_toggle_switch(app, toggle_x, play_bg_toggle_y, toggle_w, toggle_h, &play_in_background, locale_get("toggle_off"), locale_get("toggle_on"))) {
                        app->inbe.play_in_background = play_in_background;
                        app->settings_dirty = 1;
                        TraceLog(LOG_INFO, "INBE: Settings toggled play_in_background to %d", play_in_background);
                    }
#endif
                } else if(app->settings_sub_tab == PRACTICE_SUBTAB_SESSION) {
                    int slider_y = tab_content_y + flint_px(20);

                    /* Max rounds */
                    if(ui_draw_slider(app, 2, content_x, slider_y, content_w, locale_get("max_rounds_label"), 1,
                                   MaxRounds, &max_rounds, "")) {
                        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                        app->settings_dirty = 1;
                    }

                    /* Max breaths */
                    if(ui_draw_slider(app, 3, content_x, slider_y + flint_px(66), content_w, locale_get("max_breaths_label"), SETTINGS_BREATHS_MIN,
                                   SETTINGS_BREATHS_MAX, &max_breaths, "")) {
                        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                        app->settings_dirty = 1;
                    }

                    /* Pause */
                    if(ui_draw_slider(app, 4, content_x, slider_y + flint_px(132), content_w, locale_get("pause_after_round_label"), SETTINGS_PAUSE_MIN,
                                   SETTINGS_PAUSE_MAX, &pause_seconds, "s")) {
                        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
                        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
                        app->settings_dirty = 1;
                    }

                    /* Advanced controls */
                    int advanced_session_controls = app->advanced_session_controls;
                    int toggle_w = flint_px(56);
                    int toggle_h = flint_px(30);
                    int advanced_label_y = slider_y + flint_px(198);
                    int advanced_toggle_y = advanced_label_y + flint_px(26);
                    DrawText(locale_get("advanced_session_controls_label"), content_x, advanced_label_y, flint_clamp_px(14, 12, 16), c_text);
                    if(ui_draw_toggle_switch(app, content_x, advanced_toggle_y, toggle_w, toggle_h, &advanced_session_controls, locale_get("toggle_off"), locale_get("toggle_on"))) {
                        app->advanced_session_controls = advanced_session_controls;
                        app->settings_dirty = 1;
                    }

                    /* Reset to defaults button */
                    int reset_y = advanced_toggle_y + toggle_h + flint_px(20);
                    int reset_w = MeasureText(locale_get("reset_to_defaults_label"), flint_clamp_px(14, 12, 16)) + flint_px(24);
                    int reset_h = flint_px(36);
                    int reset_x = content_x + content_w - reset_w;
                    int reset_hover = 0;
                    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);

                    Rectangle reset_bounds = {reset_x, reset_y, reset_w, reset_h};
                    if(CheckCollisionPointRec(mouse_world, reset_bounds)) {
                        DrawRectangle(reset_x, reset_y, reset_w, reset_h, c_button_hover);
                        ui_draw_bevel(reset_x, reset_y, reset_w, reset_h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
                        reset_hover = 1;
                        app->cursor_clickable = 1;
                    } else {
                        DrawRectangle(reset_x, reset_y, reset_w, reset_h, c_button);
                        ui_draw_bevel(reset_x, reset_y, reset_w, reset_h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
                    }

                    int reset_font = flint_clamp_px(14, 12, 16);
                    DrawText(locale_get("reset_to_defaults_label"), reset_x + flint_px(12), reset_y + reset_h / 2 - reset_font / 2 - 1, reset_font, c_text);

                    if(reset_hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                       !ui_dropdown_captures_click(mouse_world)) {
                        /* Reset to default values */
                        speed = 6;
                        max_rounds = DefaultMaxRounds;
                        max_breaths = DefaultMaxBreaths;
                        pause_seconds = DefaultPauseSeconds;
                        app->advanced_session_controls = 0;
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
                    if(ui_draw_slider(app, 6, content_x, slider_y, content_w, locale_get("volume_label"), SETTINGS_VOLUME_MIN,
                                   SETTINGS_VOLUME_MAX, &sound_volume, "")) {
                        app->sound_volume = sound_volume;
                        app->settings_dirty = 1;
                    }
                } else if(app->settings_sub_tab == APP_SUBTAB_VISUAL) {
                    int keyboard_toggle = app->on_screen_keyboard_enabled;
                    int toggle_w = flint_px(56);
                    int toggle_h = flint_px(30);
                    int keyboard_label_y;
                    int keyboard_toggle_y;
                    int theme_y;
#if !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID) && !defined(PLATFORM_WEB)
                    int checkbox_y = tab_content_y;
                    if(ui_draw_checkbox_toggle(app, content_x, checkbox_y, locale_get("fullscreen_label"), &app->fullscreen_enabled)) {
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
                    DrawText(locale_get("on_screen_keyboard_label"), content_x, keyboard_label_y, flint_clamp_px(14, 12, 16), c_text);
                    if(ui_draw_toggle_switch(app, content_x, keyboard_toggle_y, toggle_w, toggle_h,
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
                    int font = flint_clamp_px(14, 12, 16);
                    int label_y = tab_content_y;
#if defined(LOTUS_BUILD)
                    DrawText(locale_get("language_label"), content_x, label_y, font, c_text);
                    DrawText(locale_current_code(), content_x, label_y + flint_px(28), font, c_text);
#else
                    int dropdown_y = label_y + flint_px(28);
                    DrawText(locale_get("language_label"), content_x, label_y, font, c_text);
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
                    int font = flint_clamp_px(14, 12, 16);
                    int text_y = tab_content_y;
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
                    DrawText(locale_get("data_management_label"), content_x, text_y, font, c_text);
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
                    DrawText(stat_text, stat_x, stat_y, font, c_text);
                    stat_y += flint_px(22);

                    locale_format(stat_text, sizeof(stat_text), "data_size_label", size_str);
                    DrawText(stat_text, stat_x, stat_y, font, c_text);
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
                        DrawText(storage_text, stat_x, stat_y, flint_clamp_px(12, 10, 14), flint_darken(c_text, 40));
                    }
#endif

                    text_y += stats_box_h + flint_px(24);

                    /* Export button */
                    int export_h = flint_px(36);
                    int export_w = MeasureText(locale_get("export_data_button"), font) + flint_px(24);
                    int export_x = content_x;
                    int export_y = text_y;
                    Rectangle export_rect = {export_x, export_y, export_w, export_h};

                    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
                    if(CheckCollisionPointRec(mouse_world, export_rect)) {
                        DrawRectangle(export_x, export_y, export_w, export_h, c_button_hover);
                        ui_draw_bevel(export_x, export_y, export_w, export_h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
                        hover_export = 1;
                        app->cursor_clickable = 1;
                    } else {
                        DrawRectangle(export_x, export_y, export_w, export_h, c_button);
                        ui_draw_bevel(export_x, export_y, export_w, export_h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
                    }
                    DrawText(locale_get("export_data_button"), export_x + flint_px(12), export_y + export_h / 2 - font / 2 - 1, font, c_text);

                    /* Delete All button */
                    int delete_w = MeasureText(locale_get("delete_all_data_button"), font) + flint_px(24);
                    int delete_x = content_x;
                    int delete_y = export_y + export_h + flint_px(12);
                    Rectangle delete_rect = {delete_x, delete_y, delete_w, export_h};

                    if(CheckCollisionPointRec(mouse_world, delete_rect)) {
                        DrawRectangle(delete_x, delete_y, delete_w, export_h, c_button_hover);
                        ui_draw_bevel(delete_x, delete_y, delete_w, export_h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
                        hover_delete = 1;
                        app->cursor_clickable = 1;
                    } else {
                        DrawRectangle(delete_x, delete_y, delete_w, export_h, c_button);
                        ui_draw_bevel(delete_x, delete_y, delete_w, export_h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
                    }
                    DrawText(locale_get("delete_all_data_button"), delete_x + flint_px(12), delete_y + export_h / 2 - font / 2 - 1, font, c_text);

                    /* Handle button clicks */
                    if(hover_export && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                       !ui_dropdown_captures_click(mouse_world)) {
                        if(file_dialog_save(&export_dlg, locale_get("export_data_dialog_title"), "inbe-export.zip")) {
                            const char *path = file_dialog_get_path(&export_dlg);
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
                            /* On Android: don't show success message (share sheet is the feedback) */
                            if(path != NULL && data_export(path)) {
                                TraceLog(LOG_INFO, "DATA: Export successful (share sheet shown)");
                            } else {
                                locale_format(export_result, sizeof(export_result), "export_failed");
                                TraceLog(LOG_ERROR, "DATA: Export failed");
                            }
#else
                            /* Other platforms: show success message */
                            if(path != NULL && data_export(path)) {
                                const char *filename = GetFileName(path);
                                locale_format(export_result, sizeof(export_result), "exported_label", filename);
                                TraceLog(LOG_INFO, "DATA: Export successful to %s", path);
                            } else {
                                locale_format(export_result, sizeof(export_result), "export_failed");
                                TraceLog(LOG_ERROR, "DATA: Export failed");
                            }
#endif
                        }
                    }
                    if(hover_delete && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
                       !ui_dropdown_captures_click(mouse_world)) {
                        if(data_has_any()) {
                            app->modal.active = 1;
                            app->modal.type = UIModalConfirmDeleteData;
                            app->modal.selected_button = 0;
                        }
                    }

                    /* Draw export result message under Export button */
                    if(export_result[0] != '\0') {
                        int result_font = flint_clamp_px(12, 10, 14);
                        int result_x = export_x;
                        int result_y = export_y + export_h + flint_px(8);
                        DrawText(export_result, result_x, result_y, result_font, c_text);
                    }
                } else if(app->settings_sub_tab == ABOUT_DATA_SUBTAB_ABOUT) {
                    int font = flint_clamp_px(14, 12, 16);
                    int small_font = flint_clamp_px(12, 10, 14);
                    int text_y = tab_content_y;

                    /* App description */
                    const char *desc_text = locale_get("about_description");
                    FlintTextLayout desc_layout = flint_text_layout_parse(desc_text, (Texture2D){0}, FLINT_ICON_TYPE_NONE, font);
                    flint_text_layout_reflow(&desc_layout, content_w, font, flint_px(22));
                    flint_text_layout_draw(&desc_layout, content_x, &text_y, font, c_text);
                    flint_text_layout_free(&desc_layout);

                    /* Version info */
                    text_y += flint_px(20);
                    char version_text[32];
                    locale_format(version_text, sizeof(version_text), "version_label", INBE_VERSION_STRING);
                    DrawText(version_text, content_x, text_y, small_font, flint_darken(c_text, 40));

                    /* Icon links */
                    int links_y = text_y + flint_px(40);
                    int icon_size = flint_clamp_px(ICON_SIZE_LARGE, ICON_SIZE_LARGE_MIN, ICON_SIZE_LARGE_MAX);
                    int icon_padding = flint_px(4);
                    int icon_spacing = flint_px(20);
                    int icon_btn_w = icon_size + icon_padding * 2;
                    int total_w = icon_btn_w * 2 + icon_spacing * 1;
                    int columns = total_w <= content_w ? 2 : 2;
                    int grid_w = icon_btn_w * columns + icon_spacing * (columns - 1);
                    int links_start_x = content_x + (content_w - grid_w) / 2;
                    int row_spacing = flint_px(16);
                    Texture2D icons[2] = {app->telegram_icon, app->monero_icon};
                    UIIconType icon_types[2] = {UI_ICON_TYPE_TELEGRAM, UI_ICON_TYPE_MONERO};
                    const char *urls[2] = {
                        "https://t.me/lotusinbe",
                        "https://trocador.app/en/anonpay/?ticker_to=xmr&network_to=Mainnet&address=86CbC3d4a2GhT9auh6X99JhmhTMFKVVk8Q9cLrKTHkBu8LLkoNWgkBeAT3YZrvDM6NczYe8brUJNsTiFmwpWDZYnFG5kzSH&donation=True&simple_mode=True&amount=0.1&name=Inner+Breeze&email=waotzi@proton.me&ticker_from=xmr&network_from=Mainnet&buttonbgcolor=445588&textcolor=ffffff&bgcolor=eaeaffff"
                    };

                    for(int i = 0; i < 2; i++) {
                        int col = i % columns;
                        int row = i / columns;
                        int icon_x = links_start_x + col * (icon_btn_w + icon_spacing) + icon_padding;
                        int icon_y = links_y + row * (icon_btn_w + row_spacing);
                        ui_draw_icon_link(app, icon_x, icon_y, icon_size, icons[i], icon_types[i], urls[i]);
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

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if(app->settings_dirty)
            save_settings(app);
    }
}
