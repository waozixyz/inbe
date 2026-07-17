#include "whm_session.h"
#include "platform.h"

#include "data.h"
#include "locale.h"
#include "theme.h"
#include "ui_dpi.h"
#include "ui.h"
#include "practices/meditation/meditation_music.h"
#include "practices/practice_registry.h"

#include <math.h>
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
    if(rmax < ScaleUIPx(60))
        rmax = ScaleUIPx(60);
    if(rmax > ScaleUIPx(120))
        rmax = ScaleUIPx(120);
    rmin = rmax / 2;
    if(rmin < ScaleUIPx(24))
        rmin = ScaleUIPx(24);
    if(rmin > rmax - ScaleUIPx(10))
        rmin = rmax - ScaleUIPx(10);
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
        FormatLocaleText(text, sizeof(text), "notification_results");
        goto update;
    }
    if(app->inbe.screen != InbeScreenSession)
        return;

    count = int_from_count(app->inbe.count);
    switch(app->inbe.phase) {
    case InbePhaseBreathe:
        FormatLocaleText(text, sizeof(text), "notification_breathing_count", count);
        break;
    case InbePhaseHold:
        FormatLocaleText(text, sizeof(text), "notification_hold_seconds", count);
        break;
    case InbePhaseRecover:
        FormatLocaleText(text, sizeof(text), "notification_breath_in_seconds", count);
        break;
    case InbePhaseStarting:
        FormatLocaleText(text, sizeof(text), "notification_starting_seconds",
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
            app_play_breath_cue(app, app->inbe.dir);
        }
        if(count_changed) {
            int count_value = int_from_count(app->inbe.count);
            int maxbreaths_value = int_from_count(app->inbe.maxbreaths);
            if(count_value == maxbreaths_value - 1)
                app_play_bell_cue(app, 0.8f);
        }
    } else if(phase_changed) {
        if(app->inbe.phase == InbePhaseNext && app->sound_last_phase == InbePhaseRecover)
            app_play_breath_cue(app, 1);
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
    session_update_circle_bounds_for_view(&app->inbe, GetUITitleBarHeight(), ScaleUIPx(56) + 80);
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
    app_play_breath_cue(app, 0);
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
clear_round_results_from(InbeApp *app, int first_round)
{
    if(app == NULL)
        return;

    if(first_round < 0)
        first_round = 0;
    if(first_round >= MaxRounds)
        return;

    for(int i = first_round; i < MaxRounds; i++)
        cpcount(app->inbe.results[i], "000");
    app->results_saved = 0;
    app->results_path[0] = '\0';
}

static int
session_round_has_starting_step(InbeApp *app)
{
    if(app == NULL)
        return 0;

    return app->inbe.round == 0 || app->inbe.pause_seconds > 0;
}

static void
session_enter_starting_step(InbeApp *app)
{
    app->inbe.phase = InbePhaseStarting;
    app->inbe.dir = 0;
    app->inbe.r = app->inbe.rmin;
    app->inbe.breath_frame = 0;
    app->inbe.breathtick = 0;
    app->inbe.sectick = 0;
    app->inbe.halftick = 0;
    cpcount(app->inbe.count, "000");
}

static void
session_enter_previous_round_last_step(InbeApp *app)
{
    if(app->inbe.round <= 0)
        return;

    clear_round_results_from(app, app->inbe.round);
    app->inbe.round--;
    app->inbe.phase = InbePhaseNext;
    app->inbe.dir = 0;
    app->inbe.r = app->inbe.rmax;
    app->inbe.breath_frame = 0;
    app->inbe.breathtick = 0;
    app->inbe.sectick = 0;
    app->inbe.halftick = 0;
    cpcount(app->inbe.count, "000");
}

static void
session_step_back(InbeApp *app)
{
    if(app == NULL)
        return;

    switch(app->inbe.phase) {
    case InbePhaseStarting:
        if(app->inbe.round > 0)
            session_enter_previous_round_last_step(app);
        else
            session_enter_starting_step(app);
        break;
    case InbePhaseBreathe:
        clear_round_results_from(app, app->inbe.round);
        if(session_round_has_starting_step(app))
            session_enter_starting_step(app);
        else if(app->inbe.round > 0)
            session_enter_previous_round_last_step(app);
        else
            session_reset_round_breathe(&app->inbe);
        break;
    case InbePhaseHold:
        clear_round_results_from(app, app->inbe.round);
        session_reset_round_breathe(&app->inbe);
        break;
    case InbePhaseRecover:
        app->inbe.phase = InbePhaseHold;
        app->inbe.dir = 0;
        app->inbe.r = app->inbe.rmin;
        app->inbe.breath_frame = 0;
        app->inbe.breathtick = 0;
        app->inbe.sectick = 0;
        app->inbe.halftick = 0;
        cpcount(app->inbe.count, app->inbe.results[app->inbe.round]);
        clear_round_results_from(app, app->inbe.round);
        break;
    case InbePhaseNext:
        app->inbe.phase = InbePhaseRecover;
        app->inbe.dir = 0;
        app->inbe.r = app->inbe.rmax;
        app->inbe.breath_frame = 0;
        app->inbe.breathtick = 0;
        app->inbe.sectick = 0;
        app->inbe.halftick = 0;
        count_from_int(app->inbe.count, 14);
        break;
    }

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
    int font = UI_TEXT_24;
    Color text_color = text_color_for_background(GetThemeCircle());

    if(app->inbe.phase == InbePhaseStarting) {
        int total_seconds = app->inbe.round == 0 ? 3 : app->inbe.pause_seconds;
        int remaining;

        if(total_seconds <= 0)
            remaining = 0;
        else {
            remaining = total_seconds - app->inbe.sectick / 60;
            if(remaining < 1)
                remaining = 1;
        }
        snprintf(text, sizeof(text), "%d", remaining);
        DrawCenteredUIControlText(text, center_x, center_y, font, text_color);
        return;
    }

    if(app->inbe.phase == InbePhaseRecover) {
        if(app->inbe.r < app->inbe.rmax) {
            DrawCenteredUIControlText("000", center_x, center_y, font, text_color);
            return;
        }

        count = int_from_count(app->inbe.count);
        if(count < 15) {
            count_from_int(text, 15 - count);
            DrawCenteredUIControlText(text, center_x, center_y, font, text_color);
            return;
        }
        DrawCenteredUIControlText("000", center_x, center_y, font, text_color);
        return;
    }

    if(app->inbe.phase == InbePhaseNext) {
        DrawCenteredUIControlText("000", center_x, center_y, font, text_color);
        return;
    }

    DrawCenteredUIControlText(app->inbe.count, center_x, center_y, font, text_color);
}

int
draw_hold_display_mode_selector(InbeApp *app, int x, int y, int w)
{
    const char *labels[2] = {GetLocaleText("hold_display_circle"), GetLocaleText("hold_display_stopwatch")};
    int selected = clampi(app->hold_display_mode, HOLD_DISPLAY_CIRCLE, HOLD_DISPLAY_STOPWATCH);
    int h = ScaleUIPx(36);
    int segment_w = w / 2;
    int clicked = 0;
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);

    for(int i = 0; i < 2; i++) {
        int segment_x = x + i * segment_w;
        int current_w = i == 1 ? x + w - segment_x : segment_w;
        Rectangle rect = {segment_x, y, current_w, h};
        int active_hit = CheckCollisionPointRec(mouse_world, rect) &&
                         !UIInputCapturesClick(mouse_world);
        int hovered = active_hit && UIHoverEffectsEnabled();
        int active = i == selected;
        Color fill = active ? GetThemeButton() : DarkenUIColor(GetThemeBackground(), 10);
        Color top = LightenUIColor(fill, 35);
        Color bottom = DarkenUIColor(fill, 45);
        int font = GetUIFontSize();
        int text_w;

        if(active_hit) {
            MarkUIClickable();
            if(hovered && !active) {
                fill = GetThemeButtonHover();
                top = DarkenUIColor(GetThemeButtonHover(), 40);
                bottom = LightenUIColor(GetThemeButtonHover(), 40);
            }
        }

        DrawRectangle(segment_x, y, current_w, h, fill);
        DrawUIBevel(segment_x, y, current_w, h, top, bottom);

        if(MeasureUIText(labels[i], font) > current_w - ScaleUIPx(12))
            font = UI_TEXT_12;
        text_w = MeasureUIText(labels[i], font);
        DrawUIText(labels[i], segment_x + (current_w - text_w) / 2,
                        GetUIControlTextY(labels[i], y, h, font), font, GetThemeText());

        if(active_hit && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
           !UIInputCapturesClick(mouse_world) && selected != i) {
            app->hold_display_mode = i == 0 ? HOLD_DISPLAY_CIRCLE : HOLD_DISPLAY_STOPWATCH;
            app->settings_dirty = 1;
            clicked = 1;
        }
    }

    return clicked;
}

static Color
color_with_alpha(Color color, unsigned char alpha)
{
    color.a = alpha;
    return color;
}

static void
session_circle_progress(InbeApp *app, int *total_ticks, float *progress,
                        int *countdown)
{
    int total = int_from_count(app->inbe.maxbreaths);
    float amount = 0.0f;
    int is_countdown = 0;

    if(total < 1)
        total = 1;

    if(total_ticks == NULL || progress == NULL || countdown == NULL)
        return;

    switch(app->inbe.phase) {
    case InbePhaseStarting: {
        int total_seconds = app->inbe.round == 0 ? 3 : app->inbe.pause_seconds;
        int total_frames = total_seconds * 60;

        total = total_seconds > 0 ? total_seconds : 1;
        if(total_frames > 0)
            amount = (float)app->inbe.sectick / (float)total_frames;
        is_countdown = 1;
        break;
    }
    case InbePhaseBreathe:
        amount = ((float)clampi(int_from_count(app->inbe.count), 0, total) +
                  current_breath_progress(&app->inbe)) / (float)total;
        break;
    case InbePhaseHold: {
        int total_frames = 60 * 60;
        int frame_in_minute = (int_from_count(app->inbe.count) * 60 + app->inbe.sectick) %
                              total_frames;

        total = 60;
        amount = (float)frame_in_minute / (float)total_frames;
        break;
    }
    case InbePhaseRecover: {
        int count = int_from_count(app->inbe.count);

        total = 15;
        if(app->inbe.r >= app->inbe.rmax) {
            amount = ((float)clampi(count, 0, total) +
                      (float)clampi(app->inbe.sectick, 0, 59) / 60.0f) /
                     (float)total;
        }
        is_countdown = 1;
        break;
    }
    case InbePhaseNext:
    default:
        amount = 0.0f;
        break;
    }

    if(amount < 0.0f)
        amount = 0.0f;
    if(amount > 1.0f)
        amount = 1.0f;

    *total_ticks = total;
    *progress = amount;
    *countdown = is_countdown;
}

static void
draw_session_progress_circle(InbeApp *app, int center_x, int center_y, float radius)
{
    int total_ticks = 1;
    int countdown = 0;
    float progress = 0.0f;
    int gap = ScaleUIPx(5);
    int band_width;
    int thickness;
    float inner_radius;
    float outer_radius;
    Color circle = GetThemeCircle();
    Color contrast = text_color_for_background(circle);
    Color band_bg;
    Color band_fill;
    Color tick_done;
    Color tick_pending;
    Color ring;

    if(app == NULL)
        return;

    session_circle_progress(app, &total_ticks, &progress, &countdown);

    if(total_ticks <= 20)
        band_width = ScaleUIPx(14);
    else if(total_ticks <= 40)
        band_width = ScaleUIPx(11);
    else
        band_width = ScaleUIPx(8);
    if(band_width < 5)
        band_width = 5;

    thickness = total_ticks <= 40 ? ScaleUIPx(3) : ScaleUIPx(2);
    if(thickness < 1)
        thickness = 1;

    outer_radius = radius - (float)gap;
    inner_radius = outer_radius - (float)band_width;
    if(inner_radius < radius * 0.66f)
        inner_radius = radius * 0.66f;
    if(outer_radius <= inner_radius)
        return;

    band_bg = color_with_alpha(contrast, 30);
    band_fill = color_with_alpha(contrast, 72);
    tick_pending = color_with_alpha(contrast, 96);
    tick_done = color_with_alpha(contrast, 235);
    ring = color_with_alpha(contrast, 175);

    DrawCircleV((Vector2){(float)center_x, (float)center_y}, radius, circle);
    DrawRing((Vector2){(float)center_x, (float)center_y}, inner_radius, outer_radius,
             0.0f, 360.0f, 128, band_bg);
    if(countdown) {
        if(progress < 1.0f) {
            DrawRing((Vector2){(float)center_x, (float)center_y}, inner_radius,
                     outer_radius, -450.0f, -90.0f - 360.0f * progress,
                     128, band_fill);
        }
    } else if(progress > 0.0f) {
        DrawRing((Vector2){(float)center_x, (float)center_y}, inner_radius, outer_radius,
                 -90.0f, -90.0f + 360.0f * progress, 128, band_fill);
    }
    DrawRing((Vector2){(float)center_x, (float)center_y}, inner_radius - 0.75f,
             inner_radius + 0.75f, 0.0f, 360.0f, 128, ring);
    DrawRing((Vector2){(float)center_x, (float)center_y}, outer_radius - 0.75f,
             outer_radius + 0.75f, 0.0f, 360.0f, 128, ring);

    for(int i = 0; i < total_ticks; i++) {
        float tick_progress = (float)i / (float)total_ticks;
        float direction = countdown ? -1.0f : 1.0f;
        float angle = -PI * 0.5f +
                      direction * ((float)i * 2.0f * PI) / (float)total_ticks;
        float s = sinf(angle);
        float c = cosf(angle);
        if(countdown && i == 0)
            tick_progress = 1.0f;
        int tick_colored = countdown ? tick_progress > progress + 0.0001f
                                     : tick_progress <= progress + 0.0001f;
        Vector2 start = {
            (float)center_x + c * inner_radius,
            (float)center_y + s * inner_radius
        };
        Vector2 end = {
            (float)center_x + c * outer_radius,
            (float)center_y + s * outer_radius
        };

        DrawLineEx(start, end, (float)thickness,
                   tick_colored ? tick_done : tick_pending);
    }
}

void
session_draw_inbe(InbeApp *app, int center_x, int center_y)
{
    float radius = draw_radius(&app->inbe);

    draw_session_progress_circle(app, center_x, center_y, radius);
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
    const char *play_text = GetLocaleText("play_button");
    const char *practice_text = practice_label(app->exercise_type);
    const char *warning_text = GetLocaleText("sun_salutation_work_in_progress");
    int font = UI_TEXT_16;
    int practice_font = GetUIFontSize();
    int warning_font = GetUISmallFontSize();
    int practice_w;
    int practice_y;
    int warning_w;
    int warning_y;
    float scale = 1.0f;

    int radius = ScaleUIPx(44);

    // Circular hover detection for all exercises
    float dx = mouse_world.x - center_x;
    float dy = mouse_world.y - center_y;
    float dist_sq = dx * dx + dy * dy;

    if(dist_sq <= radius * radius && !UIInputCapturesClick(mouse_world)) {
        active = 1;
        hovered = UIHoverEffectsEnabled();
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
    practice_w = MeasureUIText(practice_text, practice_font);
    if(practice_w > view_width - ScaleUIPx(48))
        practice_font = GetUISmallFontSize();
    practice_w = MeasureUIText(practice_text, practice_font);
    practice_y = center_y - scaled_radius - ScaleUIPx(42);
    if(practice_y < ScaleUIPx(8))
        practice_y = ScaleUIPx(8);
    DrawUIText(practice_text, center_x - practice_w / 2,
                    GetUIControlTextY(practice_text, practice_y, ScaleUIPx(32), practice_font),
                    practice_font, GetThemeText());

    DrawCircle(center_x, center_y, scaled_radius, GetThemeCircle());
    DrawCircleLines(center_x, center_y, scaled_radius, GetThemeText());

    // Draw PLAY text in center
    DrawCenteredUIControlText(play_text, center_x, center_y, font,  text_color_for_background(GetThemeCircle()));

    if(app->exercise_type == EXERCISE_SUN_SALUTATION) {
        warning_w = MeasureUIText(warning_text, warning_font);
        warning_y = center_y + scaled_radius + ScaleUIPx(14);
        DrawUIText(warning_text, center_x - warning_w / 2,
                        GetUIControlTextY(warning_text, warning_y, ScaleUIPx(28), warning_font),
                        warning_font, GetThemeText());
    }

    if(active) {
        MarkUIClickable();
    }

    return clicked;
}

static void
draw_session_status(InbeApp *app, int center_x, int center_y)
{
    (void)app;
    (void)center_x;
    (void)center_y;
}

static void
session_round_label(InbeApp *app, char *text, size_t text_size)
{
    if(text == NULL || text_size == 0)
        return;
    FormatLocaleText(text, text_size, "session_round_label", app->inbe.round + 1);
}

static int
session_draw_round_title_bar(InbeApp *app, const char *title, int height,
                             int show_close)
{
    int hover = 0;
    int close_icon = ScaleUIPx(18);
    int close_padding = ScaleUIPx(6);
    int close_total = close_icon + close_padding * 2;
    int close_x = view_width - close_total - ScaleUIPx(12);
    int close_y = (height - close_total) / 2;
    int side_reserved = view_width - close_x + ScaleUIPx(8);
    int font = UI_TEXT_24;
    int max_w = view_width - side_reserved * 2;
    int title_w;

    if(title == NULL)
        title = "";
    if(close_y < 0)
        close_y = 0;
    if(max_w < ScaleUIPx(48))
        max_w = view_width - ScaleUIPx(16);
    title_w = MeasureUIText(title, font);
    while(font > GetUIFontSize() && title_w > max_w) {
        font--;
        title_w = MeasureUIText(title, font);
    }

    DrawUITitleBar("", height);
    DrawUIText(title, (view_width - title_w) / 2,
               GetUIControlTextY(title, 0, height, font), font, GetThemeText());
    if(show_close && app != NULL && !app->modal.active &&
       DrawUIPaddedIconBtn(close_x, close_y, close_icon, close_padding,
                           app->icons[UI_ICON_TYPE_X], &hover))
        return 1;
    return 0;
}

void
draw_preview_inbe(Inbe *inbe, int center_x, int center_y)
{
    float r = draw_radius(inbe) * 0.72f;
    DrawCircleV((Vector2){(float)center_x, (float)center_y}, r, GetThemeCircle());
    DrawRing((Vector2){(float)center_x, (float)center_y}, r - 0.75f, r + 0.75f,
             0.0f, 360.0f, 96, GetThemeText());
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

static void
session_request_exit(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app->session_paused) {
        stop_android_background_session(app);
        app_init(app);
        return;
    }
    app_open_modal(app, UIModalConfirmExitSession);
}

static int
session_handle_exit_modal(InbeApp *app)
{
    SessionExitModalResult result;

    if(app == NULL || !app->modal.active ||
       app->modal.type != UIModalConfirmExitSession)
        return 0;

    result = app_draw_session_exit_modal(
        session_has_completed_rounds(app),
        GetLocaleText("save_completed_rounds_message"),
        GetLocaleText("all_progress_lost_message"));
    if(result == SessionExitModalCancel) {
        app_close_modal(app);
    } else if(result == SessionExitModalSave || result == SessionExitModalDiscard) {
        if(result == SessionExitModalSave)
            session_ensure_results_saved(app);
        stop_android_background_session(app);
        app_close_modal(app);
        app_init(app);
    }
    return 1;
}

void
session_update_screen(InbeApp *app, int center_x, int center_y, int *hover)
{
    int breath_max_y = view_height - ScaleUIPx(44);
    int title_h = GetUITitleBarHeight();
    char title[32];

    session_round_label(app, title, sizeof(title));
    if(session_draw_round_title_bar(app, title, title_h,
                                    app->show_session_return_button)) {
        session_request_exit(app);
        return;
    }

    if(session_handle_exit_modal(app))
        return;

    if(DrawUIIconSliderPopup((UIIconSliderPopup){
           .id = 500,
           .x = view_width - ScaleUIPx(76),
           .y = (title_h - (ScaleUIPx(18) + ScaleUIPx(5) * 2)) / 2,
           .icon_size = ScaleUIPx(18),
           .icon_padding = ScaleUIPx(5),
           .icon = sound_icon_for_volume(app),
           .open = &app->volume_popup_active,
           .value = &app->sound_volume,
           .min = SETTINGS_VOLUME_MIN,
           .max = SETTINGS_VOLUME_MAX,
           .popup_width = ScaleUIPx(40),
           .popup_height = ScaleUIPx(176)
       })) {
        app->settings_dirty = 1;
        update_session_sounds(app);
        save_settings(app);
    }

    if(app->advanced_session_controls) {
        int min_view_dim = view_width < view_height ? view_width : view_height;
        UIIconRowItem controls[] = {
            {app->icons[UI_ICON_TYPE_BACKWARD], 0},
            {app->session_paused ? app->icons[UI_ICON_TYPE_PLAY] : app->icons[UI_ICON_TYPE_PAUSE], 0},
            {app->icons[UI_ICON_TYPE_FORWARD], 0}
        };
        UIIconRowResult row = DrawUIBottomIconRow((UIBottomIconRow){
            .center_x = center_x,
            .view_width = view_width,
            .view_height = view_height,
            .count = 3,
            .items = controls,
            .icon_size = ScaleUIPx(24),
            .icon_padding = ScaleUIPx(10),
            .gap = ScaleUIPx(12),
            .side_margin = ScaleUIPx(24),
            .bottom_margin = ScaleUIPx(6),
            .max_button_width = min_view_dim / 6,
            .min_icon_size = ScaleUIPx(16),
            .min_icon_padding = ScaleUIPx(6),
            .min_gap = ScaleUIPx(8)
        });

        breath_max_y = row.y - ScaleUIPx(44);
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
        const char *breath_label = GetLocaleText("breath_button");
        int breath_font = UI_TEXT_16;
        int breath_w = MeasureUIText(breath_label, breath_font) + ScaleUIPx(72);
        int breath_h = GetUITextLineHeight(breath_font) + ScaleUIPx(28);
        int breath_y = center_y + app->inbe.rmax - ScaleUIPx(22);
        int breath_x;
        int breath_hover = 0;

        if(app->double_tap_to_breathe) {
            const char *hint = GetLocaleText("double_tap_to_breathe_hint");
            int hint_font = GetUIFontSize();
            int hint_w = MeasureUIText(hint, hint_font);
            int hint_y = center_y + app->inbe.rmax + ScaleUIPx(12);

            if(hint_y > breath_max_y)
                hint_y = breath_max_y;
            DrawUIText(hint, center_x - hint_w / 2, hint_y, hint_font,
                            GetThemeText());
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

        if(breath_w < ScaleUIPx(184))
            breath_w = ScaleUIPx(184);
        if(breath_w > view_width - ScaleUIPx(24))
            breath_w = view_width - ScaleUIPx(24);
        if(breath_y > breath_max_y)
            breath_y = breath_max_y;
        breath_x = center_x - breath_w / 2;
        if(DrawUIGenericButton(breath_x, breath_y, breath_w, breath_h,
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
    int box_y = ScaleUIPx(78);
    int box_w;
    int row_y = ScaleUIPx(215);
    int row_h = ScaleUIPx(32);
    int total = 0;
    int best = -1;
    int round_times[MaxRounds];
    int rounds = collect_result_rounds(app, round_times, MaxRounds);
    int discard_hover = 0;
    int save_hover = 0;
    int action_y = view_height - ScaleUIPx(40);
    int title_font;
    int title_w;

    (void)center_y;

    if(rounds <= 0) {
        app_init(app);
        return;
    }

    int responsive_max_w = (int)(view_width * 0.96f);
    int min_content_w = ScaleUIPx(320);
    if(responsive_max_w < min_content_w)
        responsive_max_w = min_content_w;
    GetUICenteredColumn(responsive_max_w, GetUIPageSidePadding(), &box_x, &box_w);

    title_font = GetUITitleFontSize(GetLocaleText("results_title"), view_width - ScaleUIPx(48));
    title_w = MeasureUIText(GetLocaleText("results_title"), title_font);
    DrawUIText(GetLocaleText("results_title"), center_x - title_w / 2, ScaleUIPx(34), title_font, GetThemeText());

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
        UIInfoRow rows[3];
        int summary_font = UI_TEXT_16;

        FormatLocaleText(lines[0], sizeof(lines[0]), "results_rounds", rounds);
        FormatLocaleText(lines[1], sizeof(lines[1]), "results_best", best);
        FormatLocaleText(lines[2], sizeof(lines[2]), "results_avg",
                      rounds > 0 ? total / rounds : 0);
        if(view_width < 420 && MeasureUIText(lines[2], summary_font) > box_w - ScaleUIPx(20))
            snprintf(lines[2], sizeof(lines[2]), "%ds", rounds > 0 ? total / rounds : 0);
        for(int i = 0; i < 3; i++)
            rows[i] = (UIInfoRow){lines[i], summary_font, GetThemeText()};
        DrawUIInfoRows((UIInfoRows){
            .x = box_x,
            .y = box_y,
            .width = box_w,
            .row_height = ScaleUIPx(29),
            .rows = rows,
            .row_count = 3,
            .background = DarkenUIColor(GetThemeBackground(), 6),
            .separator = DarkenUIColor(GetThemeBackground(), 30),
            .default_text = GetThemeText()
        });
    }

    DrawLeftUIControlTextInRect(GetLocaleText("round_times_title"),
                                    (Rectangle){(float)box_x, (float)ScaleUIPx(181),
                                                (float)box_w, (float)ScaleUIPx(28)},
                                    GetUIFontSize(), DarkenUIColor(GetThemeText(), 20));
    for(int i = 0; i < rounds; i++) {
        char row[48];
        UIInfoRow info_row;
        int row_font = GetUIFontSize();
        FormatLocaleText(row, sizeof(row), "round_result_label", i + 1, round_times[i]);
        info_row = (UIInfoRow){row, row_font, GetThemeText()};
        DrawUIInfoRows((UIInfoRows){
            .x = box_x,
            .y = row_y - 1,
            .width = box_w,
            .row_height = row_h,
            .rows = &info_row,
            .row_count = 1,
            .background = DarkenUIColor(GetThemeBackground(), 4),
            .separator = DarkenUIColor(GetThemeBackground(), 26),
            .default_text = GetThemeText()
        });
        row_y += row_h;
    }

    if(DrawUITextButton(center_x - box_w / 4, action_y, GetLocaleText("discard_button"), &discard_hover)) {
        session_discard_saved_results(app);
        app_init(app);
    }
    if(DrawUITextButton(center_x + box_w / 4, action_y, GetLocaleText("save_results_button"), &save_hover)) {
        if(session_ensure_results_saved(app))
            app_init(app);
    }
    if(discard_hover || save_hover)
        *hover = 1;
}
