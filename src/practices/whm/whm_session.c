#include "whm_session.h"
#include "platform.h"

#include "data.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_dpi.h"
#include "flint_ui.h"
#include "practices/meditation/meditation_music.h"
#include "practices/practice_registry.h"

#include <stdio.h>
#include <string.h>

#if ANDROID_BUILD
#include "android_timer.h"
#include "android_wakelock.h"
#endif

extern int view_width;
extern int view_height;

static int whm_background_remainder_ms = 0;
#if ANDROID_BUILD
static char whm_last_notification_text[96] = "";
static void stop_android_background_session(InbeApp *app);
#endif

static Color
text_color_for_background(Color background)
{
    int luma = background.r * 299 + background.g * 587 + background.b * 114;

    return luma >= 128000 ? BLACK : WHITE;
}

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

#if ANDROID_BUILD
static void
session_update_notification(InbeApp *app)
{
    char text[96] = "";
    int count;

    if(app == NULL)
        return;

    if(app->inbe.screen == InbeScreenResults) {
        locale_format(text, sizeof(text), "notification_results");
        goto update;
    }
    if(app->inbe.screen != InbeScreenSession)
        return;

    count = int_from_count(app->inbe.count);
    switch(app->inbe.phase) {
    case InbePhaseBreathe:
        locale_format(text, sizeof(text), "notification_breathing_count", count);
        break;
    case InbePhaseHold:
        locale_format(text, sizeof(text), "notification_hold_seconds", count);
        break;
    case InbePhaseRecover:
        locale_format(text, sizeof(text), "notification_breath_in_seconds", count);
        break;
    case InbePhaseStarting:
        locale_format(text, sizeof(text), "notification_starting_seconds",
                      app->inbe.pause_seconds > 0
                          ? app->inbe.pause_seconds - app->inbe.sectick / 60
                          : 0);
        break;
    case InbePhaseNext:
    default:
        break;
    }

update:
    if(strcmp(text, whm_last_notification_text) == 0)
        return;

    snprintf(whm_last_notification_text, sizeof(whm_last_notification_text), "%s", text);
    android_wakelock_update_session_notification(text);
}
#endif

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
            app_play_sound(app, breath_snd, 1.0f);
        }
        if(count_changed) {
            int count_value = int_from_count(app->inbe.count);
            int maxbreaths_value = int_from_count(app->inbe.maxbreaths);
            if(count_value == maxbreaths_value - 1)
                app_play_sound(app, app->bell_sound, 0.8f);
        }
    } else if(phase_changed) {
        if(app->inbe.phase == InbePhaseNext && app->sound_last_phase == InbePhaseRecover)
            app_play_sound(app, app->breath_out_sound, 1.0f);
    }

    remember_sound_state(app);
}

void
session_background_start(InbeApp *app)
{
    if(app == NULL)
        return;
    whm_background_remainder_ms = 0;
#if ANDROID_BUILD
    whm_last_notification_text[0] = '\0';
#endif
    remember_sound_state(app);
}

void
session_advance_elapsed(InbeApp *app, int elapsed_ms)
{
    int elapsed_total;
    int frame_count;

    if(app == NULL || elapsed_ms <= 0)
        return;
    if(app->inbe.screen != InbeScreenSession || app->session_paused)
        return;

    elapsed_total = elapsed_ms + whm_background_remainder_ms;
    if(elapsed_total > 5 * 60 * 1000)
        elapsed_total = 5 * 60 * 1000;

    frame_count = (elapsed_total * 60) / 1000;
    whm_background_remainder_ms = elapsed_total - (frame_count * 1000) / 60;

    for(int i = 0; i < frame_count && app->inbe.screen == InbeScreenSession; i++) {
        inbestep(&app->inbe);
        practice_update_session_sounds(app);
#if ANDROID_BUILD
        session_update_notification(app);
#endif
    }
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
    session_update_circle_bounds_for_view(&app->inbe, flint_ui_title_bar_height(), flint_px(56) + 80);
    app_switch_screen(app, InbeScreenSession);
    app->session_paused = 0;
    app->results_saved = 0;
    app->results_path[0] = '\0';
    remember_sound_state(app);

#if ANDROID_BUILD
    android_keep_screen_on();
#endif
    TraceLog(LOG_INFO, "INBE: Starting session - play_in_background = %d", app->inbe.play_in_background);
    meditation_music_start_session(app);
    practice_background_start(app, PRACTICE_WHM);
#if ANDROID_BUILD
    whm_last_notification_text[0] = '\0';
    session_update_notification(app);
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
        app_auto_sync(app);
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
        if(data_discard_session(app->results_path)) {
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
    app_play_sound(app, app->breath_in_sound, 1.0f);
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
            meditation_music_stop(app);
#if ANDROID_BUILD
            android_allow_screen_off();
            stop_android_background_session(app);
#endif
            app_switch_screen(app, InbeScreenResults);
        } else {
            app_init(app);
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
    session_update_circle_bounds_for_view(&app->inbe, flint_ui_title_bar_height(), flint_px(56) + 80);
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
    int font = FLINT_TEXT_16;
    Color text_color = app->inbe.phase == InbePhaseHold &&
                       app->hold_display_mode == HOLD_DISPLAY_CIRCLE
                           ? text_color_for_background(flint_theme_get_bg())
                           :  text_color_for_background(flint_theme_get_circle());

    if(app->inbe.phase == InbePhaseRecover) {
        if(app->inbe.r < app->inbe.rmax) {
            flint_ui_draw_text_centered("000", center_x, center_y, font, text_color);
            return;
        }

        count = int_from_count(app->inbe.count);
        if(count < 15) {
            count_from_int(text, 15 - count);
            flint_ui_draw_text_centered(text, center_x, center_y, font, text_color);
            return;
        }
        flint_ui_draw_text_centered("000", center_x, center_y, font, text_color);
        return;
    }

    if(app->inbe.phase == InbePhaseNext) {
        flint_ui_draw_text_centered("000", center_x, center_y, font, text_color);
        return;
    }

    flint_ui_draw_text_centered(app->inbe.count, center_x, center_y, font, text_color);
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
        int active_hit = CheckCollisionPointRec(mouse_world, rect) &&
                         !ui_input_captures_click(mouse_world);
        int hovered = active_hit && ui_hover_effects_enabled();
        int active = i == selected;
        Color fill = active ? flint_theme_get_button() : flint_darken(flint_theme_get_bg(), 10);
        Color top = flint_lighten(fill, 35);
        Color bottom = flint_darken(fill, 45);
        int font = flint_ui_font();
        int text_w;

        if(active_hit) {
            app->cursor_clickable = 1;
            if(hovered && !active) {
                fill = flint_theme_get_button_hover();
                top = flint_darken(flint_theme_get_button_hover(), 40);
                bottom = flint_lighten(flint_theme_get_button_hover(), 40);
            }
        }

        DrawRectangle(segment_x, y, current_w, h, fill);
        ui_draw_bevel(segment_x, y, current_w, h, top, bottom);

        if(flint_text_measure(labels[i], font) > current_w - flint_px(12))
            font = FLINT_TEXT_12;
        text_w = flint_text_measure(labels[i], font);
        flint_text_draw(labels[i], segment_x + (current_w - text_w) / 2,
                        flint_ui_text_y(labels[i], y, h, font), font, flint_theme_get_text());

        if(active_hit && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
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
                     (float)(ring_radius + thickness / 2), -90.0f, 270.0f, 96, flint_theme_get_text());
        } else if(sweep > 0.0f) {
            DrawRing((Vector2){center_x, center_y}, (float)(ring_radius - thickness / 2),
                     (float)(ring_radius + thickness / 2), -90.0f, -90.0f + sweep, 96, flint_theme_get_text());
        }
    }
}

void
session_draw_inbe(InbeApp *app, int center_x, int center_y)
{
    float radius = draw_radius(&app->inbe);

    if(app->inbe.phase == InbePhaseHold && app->hold_display_mode == HOLD_DISPLAY_CIRCLE) {
        draw_hold_progress_outline(app, center_x, center_y);
    } else {
        DrawCircleV((Vector2){(float)center_x, (float)center_y}, radius, flint_theme_get_circle());
        DrawRing((Vector2){(float)center_x, (float)center_y}, radius - 0.75f,
                 radius + 0.75f, 0.0f, 360.0f, 96, flint_theme_get_text());
    }
    draw_session_counter(app, center_x, center_y);
}

int
session_draw_start_preview(InbeApp *app, int center_x, int center_y)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int active = 0;
    int hovered = 0;
    int clicked = 0;
    int released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    const char *play_text = locale_get("play_button");
    const char *practice_text = practice_label(app->exercise_type);
    const char *warning_text = locale_get("sun_salutation_work_in_progress");
    int font = FLINT_TEXT_16;
    int practice_font = flint_ui_font();
    int warning_font = flint_ui_font_small();
    int practice_w;
    int practice_y;
    int warning_w;
    int warning_y;
    float scale = 1.0f;

    int radius = flint_px(44);

    // Circular hover detection for all exercises
    float dx = mouse_world.x - center_x;
    float dy = mouse_world.y - center_y;
    float dist_sq = dx * dx + dy * dy;

    if(dist_sq <= radius * radius && !ui_input_captures_click(mouse_world)) {
        active = 1;
        hovered = ui_hover_effects_enabled();
        app->play_circle_hover = hovered;
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
    if(active && released) {
        clicked = 1;
    }

    int scaled_radius = (int)(radius * scale);
    practice_w = flint_text_measure(practice_text, practice_font);
    if(practice_w > view_width - flint_px(48))
        practice_font = flint_ui_font_small();
    practice_w = flint_text_measure(practice_text, practice_font);
    practice_y = center_y - scaled_radius - flint_px(42);
    if(practice_y < flint_px(8))
        practice_y = flint_px(8);
    flint_text_draw(practice_text, center_x - practice_w / 2,
                    flint_ui_text_y(practice_text, practice_y, flint_px(32), practice_font),
                    practice_font, flint_theme_get_text());

    DrawCircle(center_x, center_y, scaled_radius, flint_theme_get_circle());
    DrawCircleLines(center_x, center_y, scaled_radius, flint_theme_get_text());

    // Draw PLAY text in center
    flint_ui_draw_text_centered(play_text, center_x, center_y, font,  text_color_for_background(flint_theme_get_circle()));

    if(app->exercise_type == EXERCISE_SUN_SALUTATION) {
        warning_w = flint_text_measure(warning_text, warning_font);
        warning_y = center_y + scaled_radius + flint_px(14);
        flint_text_draw(warning_text, center_x - warning_w / 2,
                        flint_ui_text_y(warning_text, warning_y, flint_px(28), warning_font),
                        warning_font, flint_theme_get_text());
    }

    if(active) {
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
    int font = FLINT_TEXT_16;

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
    flint_text_draw(text, center_x - max_text_w / 2, text_y, font, flint_theme_get_text());
}

static void
session_round_label(InbeApp *app, char *text, size_t text_size)
{
    if(text == NULL || text_size == 0)
        return;
    locale_format(text, text_size, "session_round_label", app->inbe.round + 1);
}

void
draw_preview_inbe(Inbe *inbe, int center_x, int center_y)
{
    float r = draw_radius(inbe) * 0.72f;
    DrawCircleV((Vector2){(float)center_x, (float)center_y}, r, flint_theme_get_circle());
    DrawRing((Vector2){(float)center_x, (float)center_y}, r - 0.75f, r + 0.75f,
             0.0f, 360.0f, 96, flint_theme_get_text());
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
    meditation_music_stop(app);
    practice_active_background_stop(app);
}

void
session_update_screen(InbeApp *app, int center_x, int center_y, int *hover)
{
    int breath_max_y = view_height - flint_px(44);
    int title_h = flint_ui_title_bar_height();
    char title[32];

    session_round_label(app, title, sizeof(title));
    if(app->show_session_return_button &&
       flint_ui_return_title_bar(app->icons[UI_ICON_TYPE_RETURN], title, title_h)) {
        if(app->session_paused) {
            stop_android_background_session(app);
            app_init(app);
        } else {
            app_open_modal(app, UIModalConfirmExitSession);
        }
    } else if(!app->show_session_return_button) {
        flint_ui_title_bar(title, title_h);
    }

    if(app->show_session_volume_control) {
        if(ui_draw_icon_slider_popup((FlintUIIconSliderPopup){
               .id = 500,
               .x = view_width - flint_px(56),
               .y = (title_h - (flint_px(24) + flint_px(10) * 2)) / 2,
               .icon_size = flint_px(24),
               .icon_padding = flint_px(10),
               .icon = sound_icon_for_volume(app),
               .open = &app->volume_popup_active,
               .value = &app->sound_volume,
               .min = SETTINGS_VOLUME_MIN,
               .max = SETTINGS_VOLUME_MAX,
               .popup_width = flint_px(44),
               .popup_height = flint_px(200)
           })) {
            app->settings_dirty = 1;
            update_session_sounds(app);
            save_settings(app);
        }
    } else {
        app->volume_popup_active = 0;
    }

    if(app->modal.active && app->modal.type == UIModalConfirmExitSession) {
        SessionExitModalResult result =
            app_draw_session_exit_modal(session_has_completed_rounds(app),
                                        locale_get("save_completed_rounds_message"),
                                        locale_get("all_progress_lost_message"));
        if(result == SessionExitModalCancel) {
            app_close_modal(app);
        } else if(result == SessionExitModalSave || result == SessionExitModalDiscard) {
            if(result == SessionExitModalSave)
                session_ensure_results_saved(app);
            stop_android_background_session(app);
            app_close_modal(app);
            app_init(app);
        }
        return;
    }

    if(app->advanced_session_controls) {
        int min_view_dim = view_width < view_height ? view_width : view_height;
        FlintUIIconRowItem controls[] = {
            {app->icons[UI_ICON_TYPE_BACKWARD], 0},
            {app->session_paused ? app->icons[UI_ICON_TYPE_PLAY] : app->icons[UI_ICON_TYPE_PAUSE], 0},
            {app->icons[UI_ICON_TYPE_FORWARD], 0}
        };
        FlintUIIconRowResult row = ui_draw_bottom_icon_row((FlintUIBottomIconRow){
            .center_x = center_x,
            .view_width = view_width,
            .view_height = view_height,
            .count = 3,
            .items = controls,
            .icon_size = flint_px(24),
            .icon_padding = flint_px(10),
            .gap = flint_px(12),
            .side_margin = flint_px(24),
            .bottom_margin = flint_px(6),
            .max_button_width = min_view_dim / 6,
            .min_icon_size = flint_px(16),
            .min_icon_padding = flint_px(6),
            .min_gap = flint_px(8)
        });

        breath_max_y = row.y - flint_px(44);
        if(row.clicked_index == 0)
            session_step_back(app);
        else if(row.clicked_index == 1)
            app->session_paused = !app->session_paused;
        else if(row.clicked_index == 2)
            session_step_forward(app);
    }

    draw_session_status(app, center_x, center_y);

    if(!app->session_paused && !app->backgrounded) {
#if ANDROID_BUILD
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
#if ANDROID_BUILD
        session_update_notification(app);
#endif
    }

    if(app->inbe.phase != InbePhaseHold)
        app->breath_tap_last_time = 0.0;

    if(app->inbe.phase == InbePhaseHold) {
        const char *breath_label = locale_get("breath_button");
        int breath_font = FLINT_TEXT_16;
        int breath_w = flint_text_measure(breath_label, breath_font) + flint_px(72);
        int breath_h = flint_text_line_height(breath_font) + flint_px(28);
        int breath_y = center_y + app->inbe.rmax - flint_px(2);
        int breath_x;
        int breath_hover = 0;

        if(app->double_tap_to_breathe) {
            const char *hint = locale_get("double_tap_to_breathe_hint");
            int hint_font = flint_ui_font();
            int hint_w = flint_text_measure(hint, hint_font);
            int hint_y = center_y + app->inbe.rmax + flint_px(12);

            if(hint_y > breath_max_y)
                hint_y = breath_max_y;
            flint_text_draw(hint, center_x - hint_w / 2, hint_y, hint_font,
                            flint_theme_get_text());
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                double now = GetTime();
                if(app->breath_tap_last_time > 0.0 &&
                   now - app->breath_tap_last_time <= 0.35) {
                    app->breath_tap_last_time = 0.0;
                    finish_hold(app);
                } else {
                    app->breath_tap_last_time = now;
                }
            }
            return;
        }

        if(breath_w < flint_px(184))
            breath_w = flint_px(184);
        if(breath_w > view_width - flint_px(24))
            breath_w = view_width - flint_px(24);
        if(breath_y > breath_max_y)
            breath_y = breath_max_y;
        breath_x = center_x - breath_w / 2;
        if(ui_draw_generic_button(breath_x, breath_y, breath_w, breath_h,
                                  breath_label, UI_BUTTON_STYLE_PRIMARY, 0,
                                  &breath_hover))
            finish_hold(app);
        if(hover != NULL)
            *hover = breath_hover;
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
    int title_font;
    int title_w;

    (void)center_y;

    if(rounds <= 0) {
        app_init(app);
        return;
    }

    int responsive_max_w = (int)(view_width * 0.96f);
    int min_content_w = flint_px(320);
    if(responsive_max_w < min_content_w)
        responsive_max_w = min_content_w;
    flint_centered_column(responsive_max_w, flint_page_side_padding(), &box_x, &box_w);

    title_font = flint_ui_title_font(locale_get("results_title"), view_width - flint_px(48));
    title_w = flint_text_measure(locale_get("results_title"), title_font);
    flint_text_draw(locale_get("results_title"), center_x - title_w / 2, flint_px(34), title_font, flint_theme_get_text());

    for(int i = 0; i < rounds; i++) {
        int seconds = round_times[i];
        total += seconds;
        if(best < 0 || seconds > best)
            best = seconds;
    }
    if(best < 0)
        best = 0;

    {
        char lines[3][64];
        FlintUIInfoRow rows[3];
        int summary_font = FLINT_TEXT_16;

        locale_format(lines[0], sizeof(lines[0]), "results_rounds", rounds);
        locale_format(lines[1], sizeof(lines[1]), "results_best", best);
        locale_format(lines[2], sizeof(lines[2]), "results_avg",
                      rounds > 0 ? total / rounds : 0);
        if(view_width < 420 && flint_text_measure(lines[2], summary_font) > box_w - flint_px(20))
            snprintf(lines[2], sizeof(lines[2]), "%ds", rounds > 0 ? total / rounds : 0);
        for(int i = 0; i < 3; i++)
            rows[i] = (FlintUIInfoRow){lines[i], summary_font, flint_theme_get_text()};
        ui_draw_info_rows((FlintUIInfoRows){
            .x = box_x,
            .y = box_y,
            .width = box_w,
            .row_height = flint_px(29),
            .rows = rows,
            .row_count = 3,
            .background = flint_darken(flint_theme_get_bg(), 6),
            .separator = flint_darken(flint_theme_get_bg(), 30),
            .default_text = flint_theme_get_text()
        });
    }

    flint_ui_draw_text_left_in_rect(locale_get("round_times_title"),
                                    (Rectangle){(float)box_x, (float)flint_px(181),
                                                (float)box_w, (float)flint_px(28)},
                                    flint_ui_font(), flint_darken(flint_theme_get_text(), 20));
    for(int i = 0; i < rounds; i++) {
        char row[48];
        FlintUIInfoRow info_row;
        int row_font = flint_ui_font();
        locale_format(row, sizeof(row), "round_result_label", i + 1, round_times[i]);
        info_row = (FlintUIInfoRow){row, row_font, flint_theme_get_text()};
        ui_draw_info_rows((FlintUIInfoRows){
            .x = box_x,
            .y = row_y - 1,
            .width = box_w,
            .row_height = row_h,
            .rows = &info_row,
            .row_count = 1,
            .background = flint_darken(flint_theme_get_bg(), 4),
            .separator = flint_darken(flint_theme_get_bg(), 26),
            .default_text = flint_theme_get_text()
        });
        row_y += row_h;
    }

    if(ui_draw_text_btn(center_x - box_w / 4, action_y, locale_get("discard_button"), &discard_hover)) {
        session_discard_saved_results(app);
        app_init(app);
    }
    if(ui_draw_text_btn(center_x + box_w / 4, action_y, locale_get("save_results_button"), &save_hover)) {
        if(session_ensure_results_saved(app))
            app_init(app);
    }
    if(discard_hover || save_hover)
        *hover = 1;
}
