#include "app_session.h"

#include "data.h"
#include "locale.h"
#include "theme.h"
#include "flint_dpi.h"
#include "flint_ui.h"

#include <stdio.h>

#ifdef __ANDROID__
#include "android_timer.h"
#include "android_wakelock.h"
void set_global_inbe_app(InbeApp *app);
#endif

extern int view_width;
extern int view_height;

static void
set_circle_bounds(Inbe *inbe, int rmin, int rmax)
{
    int old_rmin;
    int old_rmax;
    int old_span;
    int new_span;

    if(inbe == NULL)
        return;

    if(rmin < 8)
        rmin = 8;
    if(rmax < rmin + 8)
        rmax = rmin + 8;

    old_rmin = inbe->rmin;
    old_rmax = inbe->rmax;
    old_span = old_rmax - old_rmin;
    new_span = rmax - rmin;

    if(old_span > 0) {
        float t = (float)(inbe->r - old_rmin) / (float)old_span;

        if(t < 0.0f)
            t = 0.0f;
        if(t > 1.0f)
            t = 1.0f;

        inbe->r = rmin + t * (float)new_span;
    } else {
        inbe->r = rmin;
    }

    inbe->rmin = rmin;
    inbe->rmax = rmax;
    if(inbe->r < rmin)
        inbe->r = rmin;
    if(inbe->r > rmax)
        inbe->r = rmax;
}

void
session_update_circle_bounds_for_view(Inbe *inbe, int top_reserve, int bottom_reserve)
{
    int available_w = view_width - CIRCLE_SIDE_PAD * 2;
    int available_h = view_height - top_reserve - bottom_reserve - CIRCLE_SIDE_PAD * 2;
    int rmax;
    int rmin;

    if(available_w < 0)
        available_w = 0;
    if(available_h < 0)
        available_h = 0;

    rmax = available_w / 2;
    if(available_h / 2 < rmax)
        rmax = available_h / 2;
    if(rmax < 24)
        rmax = 24;

    rmin = rmax / 2;
    if(rmin < 16)
        rmin = 16;
    if(rmin > rmax - 8)
        rmin = rmax - 8;
    set_circle_bounds(inbe, rmin, rmax);
}

void
update_preview_bounds(Inbe *inbe, int content_w, int content_h)
{
    int span = content_w;
    int rmax;
    int rmin;

    if(content_h > 0 && content_h < span)
        span = content_h;

    rmax = span / 2;
    if(rmax < flint_px(60))
        rmax = flint_px(60);
    if(rmax > flint_px(120))
        rmax = flint_px(120);
    rmin = rmax / 2;
    if(rmin < flint_px(24))
        rmin = flint_px(24);
    if(rmin > rmax - flint_px(10))
        rmin = rmax - flint_px(10);
    set_circle_bounds(inbe, rmin, rmax);
}

void
session_reset_round_breathe(Inbe *inbe)
{
    inbe->phase = InbePhaseBreathe;
    inbe->dir = 0;
    inbe->r = inbe->rmin;
    inbe->breath_frame = 0;
    inbe->breathtick = 0;
    inbe->sectick = 0;
    inbe->halftick = 0;
    cpcount(inbe->count, "000");
}

static void
reset_round_start(Inbe *inbe)
{
    session_reset_round_breathe(inbe);
    if(inbe->pause_seconds > 0)
        inbe->phase = InbePhaseStarting;
}

static void
remember_sound_state(InbeApp *app)
{
    if(app == NULL)
        return;

    app->sound_last_screen = app->inbe.screen;
    app->sound_last_phase = app->inbe.phase;
    app->sound_last_dir = app->inbe.dir;
    cpcount(app->sound_last_count, app->inbe.count);
}

static void
play_app_sound(InbeApp *app, Sound sound, float scale)
{
    float volume;

    if(app == NULL || !app->audio_ready || sound.frameCount == 0 || app->sound_volume <= 0)
        return;

    volume = ((float)app->sound_volume / 100.0f) * scale;
    if(volume < 0.0f)
        volume = 0.0f;
    if(volume > 1.0f)
        volume = 1.0f;

    StopSound(sound);
    SetSoundVolume(sound, volume);
    PlaySound(sound);
}

void
update_session_sounds(InbeApp *app)
{
    if(app == NULL)
        return;

    if(app->inbe.screen != InbeScreenSession ||
       (app->session_paused && !(app->backgrounded && app->inbe.play_in_background))) {
        remember_sound_state(app);
        return;
    }

    int screen_changed = app->sound_last_screen != InbeScreenSession;
    int phase_changed = app->sound_last_phase != app->inbe.phase;
    int dir_changed = app->sound_last_dir != app->inbe.dir;
    int count_changed = !(app->sound_last_count[0] == app->inbe.count[0] &&
                          app->sound_last_count[1] == app->inbe.count[1] &&
                          app->sound_last_count[2] == app->inbe.count[2]);

    if(app->inbe.phase == InbePhaseBreathe) {
        if(screen_changed || phase_changed || dir_changed) {
            Sound breath_snd = app->inbe.dir == 0 ? app->breath_in_sound : app->breath_out_sound;
            play_app_sound(app, breath_snd, 1.0f);
        }
        if(count_changed) {
            int count_value = int_from_count(app->inbe.count);
            int maxbreaths_value = int_from_count(app->inbe.maxbreaths);
            if(count_value == maxbreaths_value - 1)
                play_app_sound(app, app->bell_sound, 0.8f);
        }
    } else if(phase_changed) {
        if(app->inbe.phase == InbePhaseNext && app->sound_last_phase == InbePhaseRecover)
            play_app_sound(app, app->breath_out_sound, 1.0f);
    }

    remember_sound_state(app);
}

void
session_start(InbeApp *app)
{
    int speed = app->inbe.speed_level;
    int max_rounds = app->inbe.max_rounds;
    int max_breaths = int_from_count(app->inbe.maxbreaths);
    int pause_seconds = app->inbe.pause_seconds;
    int progressive_speed = app->inbe.progressive_speed;
    int progressive_start_speed = app->inbe.progressive_start_speed;
    int play_in_background = app->inbe.play_in_background;

    inbeinit(&app->inbe);
    app->inbe.progressive_speed = progressive_speed;
    app->inbe.progressive_start_speed = progressive_start_speed;
    app->inbe.play_in_background = play_in_background;
    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
    app->saved_pause_seconds = app->inbe.pause_seconds;
    app->inbe.pause_seconds = 3;
    session_update_circle_bounds_for_view(&app->inbe, 0, flint_px(56) + 80);
    app->inbe.screen = InbeScreenSession;
    app->session_paused = 0;
    app->results_saved = 0;
    app->results_path[0] = '\0';
    remember_sound_state(app);

#ifdef __ANDROID__
    android_keep_screen_on();
    TraceLog(LOG_INFO, "INBE: Starting session - play_in_background = %d", app->inbe.play_in_background);
    if(app->inbe.play_in_background) {
        android_wakelock_acquire();
        android_timer_set_app(app);
        set_global_inbe_app(app);
        android_timer_start();
    } else {
        set_global_inbe_app(app);
    }
#endif
}

static int
collect_result_rounds(InbeApp *app, int *round_times, int max_rounds)
{
    int rounds;
    int count = 0;

    if(app == NULL || round_times == NULL || max_rounds <= 0)
        return 0;

    rounds = app->inbe.round + 1;
    if(rounds < 1)
        rounds = 1;
    if(rounds > app->inbe.max_rounds)
        rounds = app->inbe.max_rounds;
    if(rounds > max_rounds)
        rounds = max_rounds;

    for(int i = 0; i < rounds; i++) {
        int seconds = int_from_count(app->inbe.results[i]);
        if(seconds > 0)
            round_times[count++] = seconds;
    }

    return count;
}

int
session_ensure_results_saved(InbeApp *app)
{
    int round_times[MaxRounds];
    int rounds;

    if(app == NULL)
        return 0;
    if(app->results_saved)
        return 1;

    rounds = collect_result_rounds(app, round_times, MaxRounds);
    if(rounds <= 0)
        return 0;

    if(data_save_session_path_for_activity(round_times, rounds,
                                           0,
                                           app != NULL ? app->exercise_type : 0,
                                           app->results_path, sizeof(app->results_path))) {
        app->results_saved = 1;
        sync_habits_for_activity(app, app->exercise_type);
        TraceLog(LOG_INFO, "INBE: session saved successfully");
        return 1;
    }

    TraceLog(LOG_WARNING, "INBE: session save failed");
    return 0;
}

void
session_discard_saved_results(InbeApp *app)
{
    if(app == NULL)
        return;

    if(app->results_saved && app->results_path[0] != '\0') {
        if(data_delete_session(app->results_path)) {
            app->results_saved = 0;
            app->results_path[0] = '\0';
        }
    }
}

int
session_has_completed_rounds(InbeApp *app)
{
    int round_times[MaxRounds];
    return collect_result_rounds(app, round_times, MaxRounds) > 0;
}

static void
finish_hold(InbeApp *app)
{
    cpcount(app->inbe.results[app->inbe.round], app->inbe.count);
    cpcount(app->inbe.count, "000");
    app->inbe.phase = InbePhaseRecover;
    app->inbe.r = app->inbe.rmin;
    app->inbe.breath_frame = 0;
    app->inbe.breathtick = 0;
    app->inbe.sectick = 0;
    play_app_sound(app, app->breath_in_sound, 1.0f);
}

static void
finish_round(InbeApp *app)
{
    app->inbe.breathtick = 0;
    app->inbe.sectick = 0;
    cpcount(app->inbe.count, "000");

    if(app->inbe.round < app->inbe.max_rounds - 1) {
        app->inbe.round++;
        if(app->inbe.round == 1)
            app->inbe.pause_seconds = app->saved_pause_seconds;
        reset_round_start(&app->inbe);
    } else {
        if(session_ensure_results_saved(app)) {
#ifdef __ANDROID__
            android_allow_screen_off();
#endif
            app->inbe.screen = InbeScreenResults;
        } else {
            inbe_app_init(app);
        }
    }
}

static void
session_step_back(InbeApp *app)
{
    int speed = app->inbe.speed_level;
    int max_rounds = app->inbe.max_rounds;
    int max_breaths = int_from_count(app->inbe.maxbreaths);
    int pause_seconds = app->saved_pause_seconds;
    int progressive_speed = app->inbe.progressive_speed;
    int progressive_start_speed = app->inbe.progressive_start_speed;
    int play_in_background = app->inbe.play_in_background;

    inbeinit(&app->inbe);
    app->inbe.progressive_speed = progressive_speed;
    app->inbe.progressive_start_speed = progressive_start_speed;
    app->inbe.play_in_background = play_in_background;
    apply_settings(&app->inbe, speed, max_rounds, max_breaths, pause_seconds);
    app->saved_pause_seconds = pause_seconds;
    app->inbe.pause_seconds = 3;
    session_update_circle_bounds_for_view(&app->inbe, 0, flint_px(56) + 80);
    app->inbe.screen = InbeScreenSession;
    app->session_paused = 0;
    app->results_saved = 0;
    app->results_path[0] = '\0';
    remember_sound_state(app);
}

static void
session_step_forward(InbeApp *app)
{
    switch(app->inbe.phase) {
    case InbePhaseStarting:
        session_reset_round_breathe(&app->inbe);
        break;
    case InbePhaseBreathe:
        app->inbe.phase = InbePhaseHold;
        app->inbe.r = app->inbe.rmin;
        app->inbe.breath_frame = 0;
        app->inbe.breathtick = 0;
        app->inbe.sectick = 0;
        cpcount(app->inbe.count, "000");
        break;
    case InbePhaseHold:
        finish_hold(app);
        break;
    case InbePhaseRecover:
        app->inbe.phase = InbePhaseNext;
        app->inbe.breath_frame = 0;
        app->inbe.sectick = 0;
        cpcount(app->inbe.count, "000");
        break;
    case InbePhaseNext:
        finish_round(app);
        break;
    }
}

static void
draw_session_counter(InbeApp *app, int center_x, int center_y)
{
    char text[CountSize];
    int count;
    int font = flint_px(16);

    if(app->inbe.phase == InbePhaseRecover) {
        if(app->inbe.r < app->inbe.rmax) {
            flint_ui_draw_text_centered("000", center_x, center_y, font, theme_get_text());
            return;
        }

        count = int_from_count(app->inbe.count);
        if(count < 15) {
            count_from_int(text, 15 - count);
            flint_ui_draw_text_centered(text, center_x, center_y, font, theme_get_text());
            return;
        }
        flint_ui_draw_text_centered("000", center_x, center_y, font, theme_get_text());
        return;
    }

    if(app->inbe.phase == InbePhaseNext) {
        flint_ui_draw_text_centered("000", center_x, center_y, font, theme_get_text());
        return;
    }

    flint_ui_draw_text_centered(app->inbe.count, center_x, center_y, font, theme_get_text());
}

int
draw_hold_display_mode_selector(InbeApp *app, int x, int y, int w)
{
    const char *labels[2] = {locale_get("hold_display_circle"), locale_get("hold_display_stopwatch")};
    int selected = clampi(app->hold_display_mode, HOLD_DISPLAY_CIRCLE, HOLD_DISPLAY_STOPWATCH);
    int h = flint_px(36);
    int segment_w = w / 2;
    int clicked = 0;
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);

    for(int i = 0; i < 2; i++) {
        int segment_x = x + i * segment_w;
        int current_w = i == 1 ? x + w - segment_x : segment_w;
        Rectangle rect = {segment_x, y, current_w, h};
        int hovered = CheckCollisionPointRec(mouse_world, rect) &&
                      !ui_input_captures_click(mouse_world);
        int active = i == selected;
        Color fill = active ? theme_get_button() : flint_darken(theme_get_bg(), 10);
        Color top = flint_lighten(fill, 35);
        Color bottom = flint_darken(fill, 45);
        int font = flint_ui_font();
        int text_w;

        if(hovered) {
            app->cursor_clickable = 1;
            if(!active) {
                fill = theme_get_button_hover();
                top = flint_darken(theme_get_button_hover(), 40);
                bottom = flint_lighten(theme_get_button_hover(), 40);
            }
        }

        DrawRectangle(segment_x, y, current_w, h, fill);
        ui_draw_bevel(segment_x, y, current_w, h, top, bottom);

        while(font > flint_px(11) && flint_text_measure(labels[i], font) > current_w - flint_px(12))
            font--;
        text_w = flint_text_measure(labels[i], font);
        flint_text_draw(labels[i], segment_x + (current_w - text_w) / 2,
                        flint_ui_text_y(labels[i], y, h, font), font, theme_get_text());

        if(hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
           !ui_input_captures_click(mouse_world) && selected != i) {
            app->hold_display_mode = i == 0 ? HOLD_DISPLAY_CIRCLE : HOLD_DISPLAY_STOPWATCH;
            app->settings_dirty = 1;
            clicked = 1;
        }
    }

    return clicked;
}

static void
draw_hold_progress_outline(InbeApp *app, int center_x, int center_y)
{
    int seconds = int_from_count(app->inbe.count);
    int total_frames = seconds * 60 + app->inbe.sectick;
    int minute_frames = 60 * 60;
    int completed_minutes = total_frames / minute_frames;
    int frame_in_minute = total_frames % minute_frames;
    float sweep = 360.0f * (float)frame_in_minute / (float)minute_frames;
    int thickness = flint_px(5);
    int gap = flint_px(18);
    int radius = app->inbe.r + flint_px(12);
    int min_radius = app->inbe.r / 2;

    if(thickness < 3)
        thickness = 3;
    if(min_radius < flint_px(16))
        min_radius = flint_px(16);

    for(int i = 0; i <= completed_minutes; i++) {
        int ring_radius = radius - i * gap;
        if(ring_radius <= min_radius)
            break;

        if(i < completed_minutes) {
            DrawRing((Vector2){center_x, center_y}, (float)(ring_radius - thickness / 2),
                     (float)(ring_radius + thickness / 2), -90.0f, 270.0f, 96, theme_get_text());
        } else if(sweep > 0.0f) {
            DrawRing((Vector2){center_x, center_y}, (float)(ring_radius - thickness / 2),
                     (float)(ring_radius + thickness / 2), -90.0f, -90.0f + sweep, 96, theme_get_text());
        }
    }
}

void
session_draw_inbe(InbeApp *app, int center_x, int center_y)
{
    if(app->inbe.phase == InbePhaseHold && app->hold_display_mode == HOLD_DISPLAY_CIRCLE) {
        draw_hold_progress_outline(app, center_x, center_y);
    } else {
        DrawCircle(center_x, center_y, app->inbe.r, theme_get_circle());
        DrawCircleLines(center_x, center_y, app->inbe.r, theme_get_text());
    }
    draw_session_counter(app, center_x, center_y);
}

int
session_draw_start_preview(InbeApp *app, int center_x, int center_y)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int hovered = 0;
    int clicked = 0;
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    const char *play_text = locale_get("play_button");
    int font = flint_px(16);
    float scale = 1.0f;

    // Use smaller, consistent circle for all practice types
    int radius = flint_px(32);

    // Circular hover detection for all exercises
    float dx = mouse_world.x - center_x;
    float dy = mouse_world.y - center_y;
    float dist_sq = dx * dx + dy * dy;

    if(dist_sq <= radius * radius && !ui_input_captures_click(mouse_world)) {
        hovered = 1;
        app->play_circle_hover = 1;
    } else {
        app->play_circle_hover = 0;
    }

    // Smooth scale animation
    float target_scale = hovered ? 1.08f : 1.0f;
    app->play_circle_scale += (target_scale - app->play_circle_scale) * 0.2f;
    if(app->play_circle_scale < 1.0f) app->play_circle_scale = 1.0f;
    if(app->play_circle_scale > 1.08f) app->play_circle_scale = 1.08f;

    scale = app->play_circle_scale;

    // Handle click
    if(hovered && released) {
        clicked = 1;
    }

    // Draw circle with hover scale for all exercises
    int scaled_radius = (int)(radius * scale);
    DrawCircle(center_x, center_y, scaled_radius, theme_get_circle());
    DrawCircleLines(center_x, center_y, scaled_radius, theme_get_text());

    // Draw PLAY text in center
    flint_ui_draw_text_centered(play_text, center_x, center_y, font, theme_get_text());

    if(hovered) {
        app->cursor_clickable = 1;
    }

    return clicked;
}

static void
draw_session_status(InbeApp *app, int center_x, int center_y)
{
    char text[32];
    char max_text[32];
    int total_seconds;
    int remaining;
    int max_text_w;
    int text_y;
    int font = flint_px(16);

    if(app->inbe.phase != InbePhaseStarting)
        return;

    total_seconds = app->inbe.round == 0 ? 3 : app->inbe.pause_seconds;
    if(total_seconds <= 0)
        return;

    remaining = total_seconds - app->inbe.sectick / 60;
    if(remaining < 1)
        remaining = 1;

    locale_format(max_text, sizeof(max_text), "starting_in", 30);
    max_text_w = flint_text_measure(max_text, font);
    locale_format(text, sizeof(text), "starting_in", remaining);
    text_y = center_y - (int)(app->inbe.rmax * 0.72f) - flint_px(40);
    if(text_y < flint_px(20))
        text_y = flint_px(20);
    flint_text_draw(text, center_x - max_text_w / 2, text_y, font, theme_get_text());
}

static void
draw_session_round_label(InbeApp *app)
{
    char text[32];
    char max_text[32];
    int max_text_w;
    int top_bar_left;
    int top_bar_right;
    int text_x;
    int text_y;
    int font = flint_px(16);

    if(app->inbe.phase != InbePhaseBreathe)
        return;

    locale_format(max_text, sizeof(max_text), "session_round_label", MaxRounds);
    max_text_w = flint_text_measure(max_text, font);
    locale_format(text, sizeof(text), "session_round_label", app->inbe.round + 1);

    top_bar_left = flint_px(12) + flint_px(24) + flint_px(10) * 2;
    top_bar_right = view_width - flint_px(56);
    text_x = top_bar_left + (top_bar_right - top_bar_left - max_text_w) / 2;
    text_y = flint_ui_text_y(text, flint_px(12), flint_px(24) + flint_px(10) * 2, font);

    flint_text_draw(text, text_x, text_y, font, theme_get_text());
}

void
draw_preview_inbe(Inbe *inbe, int center_x, int center_y)
{
    int r = (int)((float)inbe->r * 0.72f);
    DrawCircle(center_x, center_y, r, theme_get_circle());
    DrawCircleLines(center_x, center_y, r, theme_get_text());
}

static Texture2D
sound_icon_for_volume(InbeApp *app)
{
    int vol = app->sound_volume;
    if(vol <= 0) return app->icons[UI_ICON_TYPE_SOUND0];
    if(vol <= 25) return app->icons[UI_ICON_TYPE_SOUND1];
    if(vol <= 75) return app->icons[UI_ICON_TYPE_SOUND2];
    return app->icons[UI_ICON_TYPE_SOUND3];
}

static void
stop_android_background_session(InbeApp *app)
{
#ifdef __ANDROID__
    if(app->inbe.play_in_background) {
        android_wakelock_release();
        android_timer_stop();
    }
#else
    (void)app;
#endif
}

void
session_update_screen(InbeApp *app, int center_x, int center_y, int *hover)
{
    int return_hover = 0;
    int modal_result = 0;
    int breath_max_y = view_height - flint_px(44);

    if(ui_draw_icon_btn_padded(flint_px(12), flint_px(12), flint_px(24),
                               flint_px(10), app->icons[UI_ICON_TYPE_RETURN], &return_hover)) {
        if(app->session_paused) {
            stop_android_background_session(app);
            inbe_app_init(app);
        } else {
            app->modal.active = 1;
            app->modal.type = UIModalConfirmExitSession;
            app->modal.selected_button = 0;
        }
    }

    int sound_btn_x = view_width - flint_px(56);
    int sound_btn_y = flint_px(12);
    int sound_btn_size = flint_px(24);
    int sound_btn_padding = flint_px(10);
    int sound_hover = 0;
    if(ui_draw_icon_btn_padded(sound_btn_x, sound_btn_y, sound_btn_size, sound_btn_padding,
                               sound_icon_for_volume(app), &sound_hover)) {
        app->volume_popup_active = !app->volume_popup_active;
    }

    if(app->volume_popup_active) {
        int popup_w = flint_px(44);
        int popup_x = sound_btn_x;
        int popup_y = sound_btn_y + sound_btn_size + sound_btn_padding * 2;
        int popup_h = flint_px(200);
        Vector2 mouse = GetMousePosition();

        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
           (mouse.x < popup_x || mouse.x > popup_x + popup_w ||
            mouse.y < popup_y || mouse.y > popup_y + popup_h)) {
            app->volume_popup_active = 0;
        }

        DrawRectangle(popup_x, popup_y, popup_w, popup_h, theme_get_surface());
        ui_draw_bevel(popup_x, popup_y, popup_w, popup_h,
                      flint_lighten(theme_get_surface(), 40), flint_darken(theme_get_surface(), 40));

        if(ui_draw_slider_vertical(500, popup_x + popup_w / 2, popup_y + flint_px(10),
                                   popup_h - flint_px(20), SETTINGS_VOLUME_MIN,
                                   SETTINGS_VOLUME_MAX, &app->sound_volume)) {
            app->settings_dirty = 1;
            update_session_sounds(app);
            save_settings(app);
        }
    }

    if(app->modal.active && app->modal.type == UIModalConfirmExitSession) {
        if(session_has_completed_rounds(app)) {
            modal_result = ui_draw_modal_3btn(locale_get("exit_session_title"),
                                              locale_get("save_completed_rounds_message"),
                                              locale_get("cancel_button"),
                                              locale_get("save_button"),
                                              locale_get("discard_button"));
            if(modal_result == 1) {
                app->modal.active = 0;
                app->modal.type = UIModalNone;
            } else if(modal_result == 2) {
                session_ensure_results_saved(app);
                stop_android_background_session(app);
                app->modal.active = 0;
                app->modal.type = UIModalNone;
                inbe_app_init(app);
            } else if(modal_result == 3) {
                stop_android_background_session(app);
                app->modal.active = 0;
                app->modal.type = UIModalNone;
                inbe_app_init(app);
            }
        } else {
            modal_result = ui_draw_modal(locale_get("exit_session_title"),
                                         locale_get("all_progress_lost_message"),
                                         locale_get("cancel_button"),
                                         locale_get("exit_button"));
            if(modal_result == 1) {
                app->modal.active = 0;
                app->modal.type = UIModalNone;
            } else if(modal_result == 2) {
                stop_android_background_session(app);
                app->modal.active = 0;
                app->modal.type = UIModalNone;
                inbe_app_init(app);
            }
        }
        return;
    }

    if(app->advanced_session_controls) {
        int back_hover = 0;
        int pause_hover = 0;
        int forward_hover = 0;
        int control_size = flint_px(24);
        int control_padding = flint_px(10);
        int control_gap = flint_px(12);
        int min_view_dim = view_width < view_height ? view_width : view_height;
        int available_row_w = view_width - flint_px(48);
        int max_btn_w = min_view_dim / 6;
        int max_btn_w_by_row;
        int control_btn_w;
        int control_y;
        int pause_x;
        int back_x;
        int forward_x;

        if(available_row_w < flint_px(120))
            available_row_w = flint_px(120);

        max_btn_w_by_row = (available_row_w - control_gap * 2) / 3;
        if(max_btn_w <= 0 || max_btn_w > max_btn_w_by_row)
            max_btn_w = max_btn_w_by_row;

        control_btn_w = control_size + control_padding * 2;
        if(control_btn_w > max_btn_w) {
            control_btn_w = max_btn_w;
            control_padding = control_btn_w / 4;
            control_size = control_btn_w - control_padding * 2;
        }

        if(control_padding < flint_px(6))
            control_padding = flint_px(6);
        if(control_size < flint_px(16))
            control_size = flint_px(16);

        control_btn_w = control_size + control_padding * 2;
        control_gap = control_btn_w / 4;
        if(control_gap < flint_px(8))
            control_gap = flint_px(8);
        control_y = view_height - flint_px(6) - control_btn_w;
        breath_max_y = control_y - flint_px(44);
        pause_x = center_x - control_btn_w / 2;
        back_x = pause_x - control_btn_w - control_gap;
        forward_x = pause_x + control_btn_w + control_gap;

        if(ui_draw_icon_btn_padded(back_x, control_y, control_size, control_padding,
                                   app->icons[UI_ICON_TYPE_BACKWARD], &back_hover))
            session_step_back(app);
        if(ui_draw_icon_btn_padded(pause_x, control_y, control_size, control_padding,
                                   app->session_paused ? app->icons[UI_ICON_TYPE_PLAY] : app->icons[UI_ICON_TYPE_PAUSE],
                                   &pause_hover))
            app->session_paused = !app->session_paused;
        if(ui_draw_icon_btn_padded(forward_x, control_y, control_size, control_padding,
                                   app->icons[UI_ICON_TYPE_FORWARD], &forward_hover))
            session_step_forward(app);
    }

    draw_session_status(app, center_x, center_y);
    draw_session_round_label(app);

    if(!app->session_paused) {
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
        pthread_mutex_t *timer_mutex = android_timer_get_mutex();
        if(timer_mutex) {
            pthread_mutex_lock(timer_mutex);
            inbestep(&app->inbe);
            update_session_sounds(app);
            pthread_mutex_unlock(timer_mutex);
        } else
#endif
        {
            inbestep(&app->inbe);
            update_session_sounds(app);
        }
    }

    if(app->inbe.phase == InbePhaseHold) {
        int breath_y = center_y + (int)(app->inbe.rmax * flint_dpi_scale() + 0.5f) + flint_px(24);
        if(breath_y > breath_max_y)
            breath_y = breath_max_y;
        if(ui_draw_text_btn(center_x, breath_y, locale_get("breath_button"), hover))
            finish_hold(app);
    }
}

void
session_draw_results_screen(InbeApp *app, int center_x, int center_y, int *hover)
{
    int box_x;
    int box_y = flint_px(78);
    int box_w;
    int row_y = flint_px(215);
    int row_h = flint_px(32);
    int total = 0;
    int best = -1;
    int round_times[MaxRounds];
    int rounds = collect_result_rounds(app, round_times, MaxRounds);
    int discard_hover = 0;
    int save_hover = 0;
    int action_y = view_height - flint_px(40);
    int title_font = flint_px(32);
    int title_w;

    (void)center_y;

    if(rounds <= 0) {
        inbe_app_init(app);
        return;
    }

    int responsive_max_w = (int)(view_width * 0.96f);
    int min_content_w = flint_px(320);
    if(responsive_max_w < min_content_w)
        responsive_max_w = min_content_w;
    flint_centered_column(responsive_max_w, flint_page_side_padding(), &box_x, &box_w);

    title_w = flint_text_measure(locale_get("results_title"), title_font);
    flint_text_draw(locale_get("results_title"), center_x - title_w / 2, flint_px(34), title_font, theme_get_text());

    for(int i = 0; i < rounds; i++) {
        int seconds = round_times[i];
        total += seconds;
        if(best < 0 || seconds > best)
            best = seconds;
    }
    if(best < 0)
        best = 0;

    DrawRectangle(box_x, box_y, box_w, flint_px(88), flint_darken(theme_get_bg(), 6));
    DrawLine(box_x, box_y + flint_px(29), box_x + box_w, box_y + flint_px(29), flint_darken(theme_get_bg(), 30));
    DrawLine(box_x, box_y + flint_px(58), box_x + box_w, box_y + flint_px(58), flint_darken(theme_get_bg(), 30));
    {
        char line[64];
        locale_format(line, sizeof(line), "results_rounds", rounds);
        flint_text_draw(line, box_x + flint_px(10), box_y + flint_px(10), flint_px(16), theme_get_text());
        locale_format(line, sizeof(line), "results_best", best);
        flint_text_draw(line, box_x + flint_px(10), box_y + flint_px(39), flint_px(16), theme_get_text());
        locale_format(line, sizeof(line), "results_avg", rounds > 0 ? total / rounds : 0);
        if(view_width < 420 && flint_text_measure(line, flint_px(16)) > box_w - flint_px(20))
            snprintf(line, sizeof(line), "%ds", rounds > 0 ? total / rounds : 0);
        flint_text_draw(line, box_x + flint_px(10), box_y + flint_px(68), flint_px(16), theme_get_text());
    }

    flint_text_draw(locale_get("round_times_title"), box_x, flint_px(188),
                    flint_ui_font(), flint_darken(theme_get_text(), 20));
    for(int i = 0; i < rounds; i++) {
        char row[48];
        int row_font = flint_ui_font();
        locale_format(row, sizeof(row), "round_result_label", i + 1, round_times[i]);
        DrawRectangle(box_x, row_y - 1, box_w, row_h, flint_darken(theme_get_bg(), 4));
        DrawLine(box_x, row_y + row_h - 2, box_x + box_w, row_y + row_h - 2, flint_darken(theme_get_bg(), 26));
        flint_text_draw(row, box_x + flint_px(10), flint_ui_text_y(row, row_y, row_h, row_font), row_font, theme_get_text());
        row_y += row_h;
    }

    if(ui_draw_text_btn(center_x - box_w / 4, action_y, locale_get("discard_button"), &discard_hover)) {
        session_discard_saved_results(app);
        inbe_app_init(app);
    }
    if(ui_draw_text_btn(center_x + box_w / 4, action_y, locale_get("save_results_button"), &save_hover)) {
        if(session_ensure_results_saved(app))
            inbe_app_init(app);
    }
    if(discard_hover || save_hover)
        *hover = 1;
}
