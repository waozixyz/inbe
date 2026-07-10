#include "whm_practice.h"

#include "app.h"
#include "locale.h"
#include "whm_session.h"
#include "theme.h"
#include "ui.h"
#include "raylib.h"
#include "screens/practice_screen.h"
#include "practices/meditation/meditation_music.h"

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
    int modal_w = ScaleUIPx(340);
    int modal_h = ScaleUIPx(360);
    int modal_x;
    int modal_y;
    int title_font = GetUIFontSize();
    int title_y;
    int close_size = ScaleUIPx(22);
    int close_padding = ScaleUIPx(8);
    int close_w = close_size + close_padding * 2;
    int close_hover = 0;
    int max_speed = app->inbe.speed_level;
    int start_speed = clampi(app->inbe.progressive_start_speed, SETTINGS_SPEED_MIN, max_speed);
    const char *title;
    int title_w;
    int title_max_w;

    if(modal_w > view_width - ScaleUIPx(24))
        modal_w = view_width - ScaleUIPx(24);
    if(modal_h > view_height - ScaleUIPx(24))
        modal_h = view_height - ScaleUIPx(24);
    modal_x = (view_width - modal_w) / 2;
    modal_y = (view_height - modal_h) / 2;
    title_y = modal_y + ScaleUIPx(14);

    SetUIModalCapture((Rectangle){
        (float)modal_x, (float)modal_y, (float)modal_w, (float)modal_h
    });
    DrawRectangle(0, 0, view_width, view_height, (Color){0, 0, 0, 180});
    DrawRectangle(modal_x, modal_y, modal_w, modal_h, GetThemeSurface());
    DrawUIBevel(modal_x, modal_y, modal_w, modal_h,
                  LightenUIColor(GetThemeSurface(), 40), DarkenUIColor(GetThemeSurface(), 40));

    title = GetLocaleText("progressive_start_speed_editor_title");
    title_w = MeasureUIText(title, title_font);
    title_max_w = modal_w - close_w * 2 - ScaleUIPx(24);
    while(title_font > ScaleUIPx(12) && title_w > title_max_w) {
        title_font--;
        title_w = MeasureUIText(title, title_font);
    }
    DrawUIText(title, modal_x + (modal_w - title_w) / 2, title_y, title_font, GetThemeText());

    if(DrawUIPaddedIconBtn(modal_x + modal_w - close_w - ScaleUIPx(6), modal_y + ScaleUIPx(6),
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
                                 modal_w - ScaleUIPx(48),
                                 ScaleUIPx(210),
                                 modal_x + modal_w / 2,
                                 modal_y + ScaleUIPx(150));

    if(DrawUISlider(12, modal_x + ScaleUIPx(24), modal_y + ScaleUIPx(250),
                      modal_w - ScaleUIPx(48), GetLocaleText("progressive_start_speed_label"),
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
    UISubtab tabs[WHM_SUBTAB_RENDER_MAX];

    if(tab_count <= 0 || tab_count > WHM_SUBTAB_RENDER_MAX)
        return -1;
    for(int i = 0; i < tab_count; i++) {
        tabs[i] = (UISubtab){0};
        tabs[i].label = tab_names[i];
        tabs[i].disabled = 0;
    }
    return DrawUISubtabBar((UISubtabBar){
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
    int preview_h = ScaleUIPx(240);
    int preview_radius;
    int preview_padding = ScaleUIPx(12);
    int speed = app->inbe.speed_level;
    int max_rounds = app->inbe.max_rounds;
    int max_breaths = int_from_count(app->inbe.maxbreaths);
    int pause_seconds = app->inbe.pause_seconds;
    int progressive_start_speed = app->inbe.progressive_start_speed;
    int progressive_speed = app->inbe.progressive_speed;
    int toggle_w = ScaleUIPx(56);
    int toggle_h = ScaleUIPx(30);

    animation_options[InbeBreathAnimationLinear] = GetLocaleText("breath_animation_linear");
    animation_options[InbeBreathAnimationInOut] = GetLocaleText("breath_animation_in_out");
    app->inbe.breath_animation = clampi(app->inbe.breath_animation,
                                        InbeBreathAnimationLinear,
                                        InbeBreathAnimationCount - 1);

    DrawUIText(GetLocaleText("breath_animation_label"), content_x, y, GetUIFontSize(), GetThemeText());
    DrawUIDropdownButton(104, content_x, y + ScaleUIPx(26), content_w, ScaleUIPx(36),
                            animation_options, InbeBreathAnimationCount,
                            &app->inbe.breath_animation);
    if(draw_breath_animation_menu != NULL)
        *draw_breath_animation_menu = 1;
    y += ScaleUIPx(76);

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
    if(DrawUISlider(1, content_x, y + preview_padding + preview_radius * 2 + ScaleUIPx(28),
                      content_w, GetLocaleText("speed_label"),
                      SETTINGS_SPEED_MIN, SETTINGS_SPEED_MAX, &speed, "")) {
        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        app->settings_preview.progressive_speed = 0;
        app->settings_dirty = 1;
    }
    y += preview_padding + preview_radius * 2 + ScaleUIPx(102);

    DrawUIText(GetLocaleText("progressive_speed_label"), content_x, y, GetUIFontSize(), GetThemeText());
    if(DrawUIToggleSwitch(content_x, y + ScaleUIPx(26), toggle_w, toggle_h, &progressive_speed,
                             GetLocaleText("toggle_off"), GetLocaleText("toggle_on"))) {
        app->inbe.progressive_speed = progressive_speed;
        app->settings_preview.progressive_speed = 0;
        app->settings_dirty = 1;
    }
    y += ScaleUIPx(66);

    if(app->inbe.progressive_speed) {
        int modify_w = MeasureUIText(GetLocaleText("modify_start_speed_button"), GetUIFontSize()) + ScaleUIPx(24);
        int modify_hover = 0;
        if(modify_w > content_w)
            modify_w = content_w;
        if(progressive_start_speed != clampi(progressive_start_speed, SETTINGS_SPEED_MIN, speed)) {
            app->inbe.progressive_start_speed = clampi(progressive_start_speed, SETTINGS_SPEED_MIN, speed);
            app->settings_preview.progressive_start_speed = app->inbe.progressive_start_speed;
            app->settings_dirty = 1;
        }
        if(DrawUIGenericButton(content_x, y, modify_w, ScaleUIPx(36),
                                  GetLocaleText("modify_start_speed_button"),
                                  UI_BUTTON_STYLE_SECONDARY, 0, &modify_hover)) {
            app_open_modal(app, UIModalEditProgressiveStartSpeed);
        }
        y += ScaleUIPx(58);
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
    int toggle_w = ScaleUIPx(56);
    int toggle_h = ScaleUIPx(30);
    int reset_w = MeasureUIText(GetLocaleText("reset_to_defaults_label"), GetUIFontSize()) + ScaleUIPx(24);
    int reset_h = ScaleUIPx(36);
    int reset_hover = 0;

    if(DrawUISlider(2, content_x, y, content_w, GetLocaleText("max_rounds_label"),
                      1, MaxRounds, &max_rounds, "")) {
        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        app->settings_dirty = 1;
    }
    y += ScaleUIPx(66);
    if(DrawUISlider(3, content_x, y, content_w, GetLocaleText("max_breaths_label"),
                      SETTINGS_BREATHS_MIN, SETTINGS_BREATHS_MAX, &max_breaths, "")) {
        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        app->settings_dirty = 1;
    }
    y += ScaleUIPx(66);
    if(DrawUISlider(4, content_x, y, content_w, GetLocaleText("pause_after_round_label"),
                      SETTINGS_PAUSE_MIN, SETTINGS_PAUSE_MAX, &pause_seconds, "s")) {
        apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
        apply_settings(&app->settings_preview, speed, max_rounds, max_breaths, pause_seconds);
        app->settings_dirty = 1;
    }
    y += ScaleUIPx(66);
    DrawUIText(GetLocaleText("hold_display_label"), content_x, y, GetUIFontSize(), GetThemeText());
    y += ScaleUIPx(26);
    draw_hold_display_mode_selector(app, content_x, y, content_w);
    y += ScaleUIPx(52);
    DrawUIText(GetLocaleText("double_tap_to_breathe_label"), content_x, y, GetUIFontSize(), GetThemeText());
    if(DrawUIToggleSwitch(content_x, y + ScaleUIPx(26), toggle_w, toggle_h, &double_tap_to_breathe,
                             GetLocaleText("toggle_off"), GetLocaleText("toggle_on"))) {
        app->double_tap_to_breathe = double_tap_to_breathe;
        app->settings_dirty = 1;
    }
    y += ScaleUIPx(76);
    DrawUIText(GetLocaleText("advanced_session_controls_label"), content_x, y, GetUIFontSize(), GetThemeText());
    if(DrawUIToggleSwitch(content_x, y + ScaleUIPx(26), toggle_w, toggle_h, &advanced_session_controls,
                             GetLocaleText("toggle_off"), GetLocaleText("toggle_on"))) {
        app->advanced_session_controls = advanced_session_controls;
        app->settings_dirty = 1;
    }
    y += ScaleUIPx(76);
    if(reset_w > content_w)
        reset_w = content_w;
    if(DrawUIGenericButton(content_x + content_w - reset_w, y, reset_w, reset_h,
                              GetLocaleText("reset_to_defaults_label"),
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
    y += reset_h + ScaleUIPx(28);

    return y;
}

static int
whm_config_content_height(InbeApp *app, int content_w)
{
    int integrated = app->inbe.screen == InbeScreenStart &&
                     app->modal.type != UIModalPracticeConfig;
    int bottom_padding = (integrated ? app_content_bottom_reserved(app) : 0) +
                         ScaleUIPx(24);

    if(app->practice_config_tab == 0) {
        int span = content_w < ScaleUIPx(240) ? content_w : ScaleUIPx(240);
        int preview_radius = span / 2;
        int h;
        if(preview_radius < ScaleUIPx(60))
            preview_radius = ScaleUIPx(60);
        if(preview_radius > ScaleUIPx(120))
            preview_radius = ScaleUIPx(120);
        preview_radius = (int)((float)preview_radius * 0.72f + 1.0f);
        h = ScaleUIPx(12) + preview_radius * 2 + ScaleUIPx(102);
        h += ScaleUIPx(66);
        if(app->inbe.progressive_speed)
            h += ScaleUIPx(58);
        h += ScaleUIPx(76);
        return h + bottom_padding;
    }

    if(app->practice_config_tab == 2)
        return meditation_music_measure_practice_settings(app, EXERCISE_WIM_HOF,
                                                          content_w, 1, 1) +
               bottom_padding;

    return ScaleUIPx(66) * 3 +
           ScaleUIPx(26) + ScaleUIPx(52) +
           ScaleUIPx(76) +
           ScaleUIPx(76) +
           ScaleUIPx(36) + ScaleUIPx(28) +
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
    PracticeSubscreenLayout layout;
    int config_tab_h = ScaleUIPx(40);
    int config_tab_gap = ScaleUIPx(14);
    int draw_breath_animation_menu = 0;
    int clicked_config_tab = -1;
    const char *config_tabs[] = {
        GetLocaleText("settings_section_breathing"),
        GetLocaleText("settings_section_session"),
        GetLocaleText("practice_music_title"),
    };

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        app->settings_drag_slider = 0;
    if(app->practice_config_tab < 0 || app->practice_config_tab > 2)
        app->practice_config_tab = 0;

    practice_screen_config_layout(app, UIModalPracticeConfig,
                                  config_tab_h + config_tab_gap, &layout);
    practice_screen_handle_config_title(app, GetLocaleText("practice_config_title"),
                                        UIModalPracticeConfig, NULL);

    clicked_config_tab = whm_draw_subtab_bar(layout.title_h, config_tab_h, config_tabs, 3,
                                             app->practice_config_tab);
    if(clicked_config_tab >= 0 && clicked_config_tab != app->practice_config_tab) {
        AppRoute route = app_current_route(app);
        route.practice_config_tab = clicked_config_tab;
        app->settings_scroll = 0;
        app_switch_route(app, route);
    }

    {
        WhmConfigScrollPageContext page_ctx = {app};
        UIScrollPage page = BeginUIScrollPage((UIScrollPageSpec){
            .y = layout.scroll_y,
            .height = layout.scroll_h,
            .max_content_width = ScaleUIPx(CONTENT_MAX_W),
            .min_content_width = ScaleUIPx(320),
            .scroll_offset = &app->settings_scroll,
            .content_height = whm_config_scroll_page_content_height,
            .user_data = &page_ctx
        });

        if(app->practice_config_tab == 0) {
            whm_config_draw_breathing_tab(app, page.content_x, page.content_w,
                                          page.content_y, &draw_breath_animation_menu);
        } else if(app->practice_config_tab == 1) {
            whm_config_draw_session_tab(app, page.content_x, page.content_w,
                                        page.content_y);
        } else {
            int y = page.content_y;
            meditation_music_draw_practice_settings(app, EXERCISE_WIM_HOF,
                                                    page.content_x, page.content_w,
                                                    &y, 1, 1);
        }
        EndUIScrollPage(page);
    }
    if(app->practice_config_tab == 2)
        meditation_music_draw_dropdown_menu(app);

    if(draw_breath_animation_menu && DrawUIDropdownMenu(104)) {
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
