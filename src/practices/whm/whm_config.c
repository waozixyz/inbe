#include "whm_practice.h"

#include "app.h"
#include "flint_locale.h"
#include "whm_session.h"
#include "flint_theme.h"
#include "flint_ui.h"
#include "raylib.h"

extern int view_width;
extern int view_height;

static void
whm_draw_exact_speed_preview(Inbe *preview, int *preview_speed,
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

    if(*preview_speed != speed)
        *preview_speed = speed;

    update_preview_bounds(preview, content_w, content_h);
    inbestep(preview);
    draw_preview_inbe(preview, center_x, center_y);
}

static void
whm_draw_progressive_start_speed_editor(InbeApp *app)
{
    int modal_w = flint_px(340);
    int modal_h = flint_px(360);
    int modal_x;
    int modal_y;
    int title_font = flint_ui_font();
    int title_y;
    int close_size = flint_px(22);
    int close_padding = flint_px(8);
    int close_w = close_size + close_padding * 2;
    int close_hover = 0;
    int max_speed = app->inbe.speed_level;
    int start_speed = clampi(app->inbe.progressive_start_speed, SETTINGS_SPEED_MIN, max_speed);
    const char *title;
    int title_w;
    int title_max_w;

    if(modal_w > view_width - flint_px(24))
        modal_w = view_width - flint_px(24);
    if(modal_h > view_height - flint_px(24))
        modal_h = view_height - flint_px(24);
    modal_x = (view_width - modal_w) / 2;
    modal_y = (view_height - modal_h) / 2;
    title_y = modal_y + flint_px(14);

    ui_set_modal_capture((Rectangle){
        (float)modal_x, (float)modal_y, (float)modal_w, (float)modal_h
    });
    DrawRectangle(0, 0, view_width, view_height, (Color){0, 0, 0, 180});
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, flint_theme_get_surface());
    ui_draw_bevel(modal_x, modal_y, modal_w, modal_h,
                  flint_lighten(flint_theme_get_surface(), 40), flint_darken(flint_theme_get_surface(), 40));

    title = locale_get("progressive_start_speed_editor_title");
    title_w = flint_text_measure(title, title_font);
    title_max_w = modal_w - close_w * 2 - flint_px(24);
    while(title_font > flint_px(12) && title_w > title_max_w) {
        title_font--;
        title_w = flint_text_measure(title, title_font);
    }
    flint_text_draw(title, modal_x + (modal_w - title_w) / 2, title_y, title_font, flint_theme_get_text());

    if(ui_draw_icon_btn_padded(modal_x + modal_w - close_w - flint_px(6), modal_y + flint_px(6),
                               close_size, close_padding, app->icons[UI_ICON_TYPE_X], &close_hover)) {
        app_close_modal(app);
        return;
    }

    whm_draw_exact_speed_preview(&app->start_speed_preview,
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

static int
whm_draw_subtab_bar(int y, int h, const char **tab_names, int tab_count,
                    int selected_tab)
{
    enum { WHM_SUBTAB_RENDER_MAX = 8 };
    FlintUISubtab tabs[WHM_SUBTAB_RENDER_MAX];

    if(tab_count <= 0 || tab_count > WHM_SUBTAB_RENDER_MAX)
        return -1;
    for(int i = 0; i < tab_count; i++) {
        tabs[i] = (FlintUISubtab){0};
        tabs[i].label = tab_names[i];
        tabs[i].disabled = 0;
    }
    return ui_draw_subtab_bar((FlintUISubtabBar){
        .bounds = {0, (float)y, (float)view_width, (float)h},
        .tabs = tabs,
        .count = tab_count,
        .selected_index = selected_tab
    });
}

static int
whm_config_draw_breathing_tab(InbeApp *app, int content_x, int content_w, int y,
                              int *draw_breath_animation_menu)
{
    static const char *animation_options[InbeBreathAnimationCount];
    int preview_h = flint_px(240);
    int preview_radius;
    int preview_padding = flint_px(12);
    int speed = app->inbe.speed_level;
    int max_rounds = app->inbe.max_rounds;
    int max_breaths = int_from_count(app->inbe.maxbreaths);
    int pause_seconds = app->inbe.pause_seconds;
    int progressive_start_speed = app->inbe.progressive_start_speed;
    int progressive_speed = app->inbe.progressive_speed;
    int toggle_w = flint_px(56);
    int toggle_h = flint_px(30);

    animation_options[InbeBreathAnimationLinear] = locale_get("breath_animation_linear");
    animation_options[InbeBreathAnimationInOut] = locale_get("breath_animation_in_out");
    app->inbe.breath_animation = clampi(app->inbe.breath_animation,
                                        InbeBreathAnimationLinear,
                                        InbeBreathAnimationCount - 1);

    flint_text_draw(locale_get("breath_animation_label"), content_x, y, flint_ui_font(), flint_theme_get_text());
    ui_draw_dropdown_button(104, content_x, y + flint_px(26), content_w, flint_px(36),
                            animation_options, InbeBreathAnimationCount,
                            &app->inbe.breath_animation);
    if(draw_breath_animation_menu != NULL)
        *draw_breath_animation_menu = 1;
    y += flint_px(76);

    update_preview_bounds(&app->settings_preview, content_w, preview_h);
    apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
    app->settings_preview.progressive_speed = 0;
    inbestep(&app->settings_preview);
    if(app->settings_preview.phase != InbePhaseBreathe) {
        reset_settings_preview(app);
        update_preview_bounds(&app->settings_preview, content_w, preview_h);
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        app->settings_preview.progressive_speed = 0;
    }

    preview_radius = (int)((float)app->settings_preview.rmax * 0.72f + 1.0f);
    draw_preview_inbe(&app->settings_preview, content_x + content_w / 2,
                      y + preview_padding + preview_radius);
    if(ui_draw_slider(1, content_x, y + preview_padding + preview_radius * 2 + flint_px(28),
                      content_w, locale_get("speed_label"),
                      SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX, &speed, "")) {
        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        app->settings_preview.progressive_speed = 0;
        app->settings_dirty = 1;
    }
    y += preview_padding + preview_radius * 2 + flint_px(102);

    flint_text_draw(locale_get("progressive_speed_label"), content_x, y, flint_ui_font(), flint_theme_get_text());
    if(ui_draw_toggle_switch(content_x, y + flint_px(26), toggle_w, toggle_h, &progressive_speed,
                             locale_get("toggle_off"), locale_get("toggle_on"))) {
        app->inbe.progressive_speed = progressive_speed;
        app->settings_preview.progressive_speed = 0;
        app->settings_dirty = 1;
    }
    y += flint_px(66);

    if(app->inbe.progressive_speed) {
        int modify_w = flint_text_measure(locale_get("modify_start_speed_button"), flint_ui_font()) + flint_px(24);
        int modify_hover = 0;
        if(modify_w > content_w)
            modify_w = content_w;
        if(progressive_start_speed != clampi(progressive_start_speed, SETTINGS_SPEED_MIN, speed)) {
            app->inbe.progressive_start_speed = clampi(progressive_start_speed, SETTINGS_SPEED_MIN, speed);
            app->settings_preview.progressive_start_speed = app->inbe.progressive_start_speed;
            app->settings_dirty = 1;
        }
        if(ui_draw_generic_button(content_x, y, modify_w, flint_px(36),
                                  locale_get("modify_start_speed_button"),
                                  UI_BUTTON_STYLE_SECONDARY, 0, &modify_hover)) {
            app_open_modal(app, UIModalEditProgressiveStartSpeed);
        }
        y += flint_px(58);
    }

    return y;
}

static int
whm_config_draw_session_tab(InbeApp *app, int content_x, int content_w, int y)
{
    int speed = app->inbe.speed_level;
    int max_rounds = app->inbe.max_rounds;
    int max_breaths = int_from_count(app->inbe.maxbreaths);
    int pause_seconds = app->inbe.pause_seconds;
    int double_tap_to_breathe = app->double_tap_to_breathe;
    int advanced_session_controls = app->advanced_session_controls;
    int toggle_w = flint_px(56);
    int toggle_h = flint_px(30);
    int reset_w = flint_text_measure(locale_get("reset_to_defaults_label"), flint_ui_font()) + flint_px(24);
    int reset_h = flint_px(36);
    int reset_hover = 0;

    if(ui_draw_slider(2, content_x, y, content_w, locale_get("max_rounds_label"),
                      1, MaxRounds, &max_rounds, "")) {
        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        app->settings_dirty = 1;
    }
    y += flint_px(66);
    if(ui_draw_slider(3, content_x, y, content_w, locale_get("max_breaths_label"),
                      SETTINGS_BREATHS_MIN, SETTINGS_BREATHS_MAX, &max_breaths, "")) {
        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        app->settings_dirty = 1;
    }
    y += flint_px(66);
    if(ui_draw_slider(4, content_x, y, content_w, locale_get("pause_after_round_label"),
                      SETTINGS_PAUSE_MIN, SETTINGS_PAUSE_MAX, &pause_seconds, "s")) {
        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        app->settings_dirty = 1;
    }
    y += flint_px(66);
    flint_text_draw(locale_get("hold_display_label"), content_x, y, flint_ui_font(), flint_theme_get_text());
    y += flint_px(26);
    draw_hold_display_mode_selector(app, content_x, y, content_w);
    y += flint_px(52);
    flint_text_draw(locale_get("double_tap_to_breathe_label"), content_x, y, flint_ui_font(), flint_theme_get_text());
    if(ui_draw_toggle_switch(content_x, y + flint_px(26), toggle_w, toggle_h, &double_tap_to_breathe,
                             locale_get("toggle_off"), locale_get("toggle_on"))) {
        app->double_tap_to_breathe = double_tap_to_breathe;
        app->settings_dirty = 1;
    }
    y += flint_px(76);
    flint_text_draw(locale_get("advanced_session_controls_label"), content_x, y, flint_ui_font(), flint_theme_get_text());
    if(ui_draw_toggle_switch(content_x, y + flint_px(26), toggle_w, toggle_h, &advanced_session_controls,
                             locale_get("toggle_off"), locale_get("toggle_on"))) {
        app->advanced_session_controls = advanced_session_controls;
        app->settings_dirty = 1;
    }
    y += flint_px(76);
    if(reset_w > content_w)
        reset_w = content_w;
    if(ui_draw_generic_button(content_x + content_w - reset_w, y, reset_w, reset_h,
                              locale_get("reset_to_defaults_label"),
                              UI_BUTTON_STYLE_SECONDARY, 0, &reset_hover)) {
        speed = DefaultSpeedLevel;
        max_rounds = DefaultMaxRounds;
        max_breaths = DefaultMaxBreaths;
        pause_seconds = DefaultPauseSeconds;
        app->inbe.progressive_start_speed = DefaultProgressiveStartSpeed;
        app->settings_preview.progressive_start_speed = DefaultProgressiveStartSpeed;
        app->inbe.breath_animation = InbeBreathAnimationLinear;
        app->settings_preview.breath_animation = InbeBreathAnimationLinear;
        app->advanced_session_controls = 0;
        app->double_tap_to_breathe = 0;
        app->hold_display_mode = HOLD_DISPLAY_CIRCLE;
        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        app->settings_dirty = 1;
    }
    y += reset_h + flint_px(28);

    return y;
}

static int
whm_config_content_height(InbeApp *app, int content_w)
{
    int integrated = app->inbe.screen == InbeScreenStart &&
                     app->modal.type != UIModalPracticeConfig;
    int bottom_padding = (integrated ? app_content_bottom_reserved(app) : 0) +
                         flint_px(24);

    if(app->practice_config_tab == 0) {
        int span = content_w < flint_px(240) ? content_w : flint_px(240);
        int preview_radius = span / 2;
        int h;
        if(preview_radius < flint_px(60))
            preview_radius = flint_px(60);
        if(preview_radius > flint_px(120))
            preview_radius = flint_px(120);
        preview_radius = (int)((float)preview_radius * 0.72f + 1.0f);
        h = flint_px(12) + preview_radius * 2 + flint_px(102);
        h += flint_px(66);
        if(app->inbe.progressive_speed)
            h += flint_px(58);
        h += flint_px(76);
        return h + bottom_padding;
    }

    return flint_px(66) * 3 +
           flint_px(26) + flint_px(52) +
           flint_px(76) +
           flint_px(76) +
           flint_px(36) + flint_px(28) +
           bottom_padding;
}

typedef struct WhmConfigScrollPageContext {
    InbeApp *app;
} WhmConfigScrollPageContext;

static int
whm_config_scroll_page_content_height(int content_w, void *user_data)
{
    WhmConfigScrollPageContext *ctx = user_data;

    return whm_config_content_height(ctx->app, content_w);
}

void
whm_config_screen_draw(InbeApp *app)
{
    int integrated = app->inbe.screen == InbeScreenStart &&
                     app->modal.type != UIModalPracticeConfig;
    int title_h = integrated ? app_content_top_reserved(app) : flint_ui_title_bar_height();
    int config_tab_h = flint_px(40);
    int config_tab_gap = flint_px(14);
    int scroll_y;
    int scroll_h;
    int bottom_reserved = integrated ? app_content_bottom_reserved(app) : 0;
    int draw_breath_animation_menu = 0;
    int clicked_config_tab = -1;
    const char *config_tabs[] = {
        locale_get("settings_section_breathing"),
        locale_get("settings_section_session"),
    };

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        app->settings_drag_slider = 0;
    if(app->practice_config_tab < 0 || app->practice_config_tab > 1)
        app->practice_config_tab = 0;

    if(!integrated) {
        if(flint_ui_return_title_bar(app->icons[UI_ICON_TYPE_RETURN], locale_get("practice_config_title"), title_h)) {
            if(app->modal.active && app->modal.type == UIModalPracticeConfig) {
                app_close_modal(app);
            } else {
                if(app->settings_dirty)
                    save_settings(app);
                app->settings_scroll = 0;
                app->practice_tab = PRACTICE_TAB_PLAY;
                app_switch_screen(app, InbeScreenStart);
            }
        }
    }

    clicked_config_tab = whm_draw_subtab_bar(title_h, config_tab_h, config_tabs, 2,
                                             app->practice_config_tab);
    if(clicked_config_tab >= 0 && clicked_config_tab != app->practice_config_tab) {
        app->practice_config_tab = clicked_config_tab;
        app->settings_scroll = 0;
    }
    scroll_y = title_h + config_tab_h + config_tab_gap;
    scroll_h = view_height - scroll_y - bottom_reserved -
               (integrated ? flint_px(8) : 0);
    if(scroll_h < 0)
        scroll_h = 0;

    {
        WhmConfigScrollPageContext page_ctx = {app};
        FlintUIScrollPage page = ui_scroll_page_begin((FlintUIScrollPageSpec){
            .y = scroll_y,
            .height = scroll_h,
            .max_content_width = flint_px(CONTENT_MAX_W),
            .min_content_width = flint_px(320),
            .scroll_offset = &app->settings_scroll,
            .content_height = whm_config_scroll_page_content_height,
            .user_data = &page_ctx
        });

        if(app->practice_config_tab == 0) {
            whm_config_draw_breathing_tab(app, page.content_x, page.content_w,
                                          page.content_y, &draw_breath_animation_menu);
        } else {
            whm_config_draw_session_tab(app, page.content_x, page.content_w,
                                        page.content_y);
        }
        ui_scroll_page_end(page);
    }

    if(draw_breath_animation_menu && ui_draw_dropdown_menu(104)) {
        app->inbe.breath_animation = clampi(app->inbe.breath_animation,
                                            InbeBreathAnimationLinear,
                                            InbeBreathAnimationCount - 1);
        app->settings_preview.breath_animation = app->inbe.breath_animation;
        app->settings_preview.progressive_speed = 0;
        app->settings_dirty = 1;
    }

    if(app->modal.active && app->modal.type == UIModalEditProgressiveStartSpeed)
        whm_draw_progressive_start_speed_editor(app);
}
