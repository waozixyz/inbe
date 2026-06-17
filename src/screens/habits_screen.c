#include "habits_screen.h"

#include "practice_screen.h"
#include "data.h"
#include "storage.h"
#include "app.h"
#include "theme.h"
#include "flint_runtime_assets.h"
#include "locale.h"
#include "breath_engine.h"
#include "flint_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* External declarations from app.c */
extern int view_width;
extern int view_height;

/* Helper functions */
static void inbe_habits_add_seed(InbeHabits *habits, const char *id, const char *name,
                                 Color color, int activity_mask);

static void
copy_text(char *dst, size_t dst_size, const char *src)
{
    if(dst == NULL || dst_size == 0)
        return;
    if(src == NULL)
        src = "";
    snprintf(dst, dst_size, "%s", src);
}

static int
habit_find_day(const InbeHabit *habit, int day_index)
{
    if(habit == NULL)
        return -1;
    for(int i = 0; i < habit->day_count; i++) {
        if(habit->days[i].day_index == day_index)
            return i;
    }
    return -1;
}

/* Core habits functions from habits.c */
int
inbe_habits_today_index(void)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if(tm == NULL)
        return 0;
    return (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday;
}

int
inbe_habit_completed_day(const InbeHabit *habit, int day_index)
{
    int index = habit_find_day(habit, day_index);
    return index >= 0 && habit->days[index].completed;
}

int
inbe_habit_completed_today(const InbeHabit *habit)
{
    return inbe_habit_completed_day(habit, inbe_habits_today_index());
}

void
inbe_habits_save(const InbeHabits *habits)
{
    inbe_storage_habits_save(habits);
    return;
}

int
inbe_habits_clear_days(InbeHabits *habits)
{
    int cleared = 0;

    if(habits == NULL)
        return 0;
    for(int i = 0; i < habits->count; i++) {
        InbeHabit *habit = &habits->items[i];
        for(int d = 0; d < habit->day_count; d++) {
            if(habit->days[d].day_index > 0 || habit->days[d].completed)
                cleared++;
        }
        memset(habit->days, 0, sizeof(habit->days));
        habit->day_count = 0;
    }
    return cleared;
}

void
inbe_habits_add_default(InbeHabits *habits)
{
    int number;
    char name[INBE_HABIT_NAME_SIZE];

    if(habits == NULL || habits->count >= INBE_HABIT_MAX)
        return;

    number = habits->count + 1;
    snprintf(name, sizeof(name), "Habit %d", number);
    inbe_habits_add_custom(habits, name, (Color){99, 196, 165, 255},
                           INBE_HABIT_SYNC_NONE, 0);
}

void
inbe_habits_add_default_set(InbeHabits *habits)
{
    if(habits == NULL)
        return;

    memset(habits, 0, sizeof(*habits));
    inbe_habits_add_seed(habits, "meditation", "Meditation", (Color){126, 183, 230, 255},
                         habit_activity_mask_for(EXERCISE_WIM_HOF) |
                         habit_activity_mask_for(EXERCISE_MEDITATION));
    habits->selected = 0;
    habits->loaded = 1;
    inbe_habits_save(habits);
}

void
inbe_habits_delete(InbeHabits *habits, int index)
{
    if(habits == NULL || index < 0 || index >= habits->count)
        return;

    for(int i = index; i < habits->count - 1; i++)
        habits->items[i] = habits->items[i + 1];
    habits->count--;
    memset(&habits->items[habits->count], 0, sizeof(habits->items[habits->count]));

    if(habits->selected > index)
        habits->selected--;
    else if(habits->selected >= habits->count)
        habits->selected = habits->count - 1;
    if(habits->selected < 0 && habits->count > 0)
        habits->selected = 0;

    inbe_habits_save(habits);
}

int
inbe_habits_add_custom(InbeHabits *habits, const char *name, Color color,
                       int sync_mode, int sync_activity)
{
    InbeHabit *habit;
    int number;

    if(habits == NULL || habits->count >= INBE_HABIT_MAX)
        return -1;

    number = habits->count + 1;
    habit = &habits->items[habits->count];
    memset(habit, 0, sizeof(*habit));
    snprintf(habit->id, sizeof(habit->id), "habit-%d", number);
    copy_text(habit->name, sizeof(habit->name),
              name != NULL && name[0] != '\0' ? name : "Habit");
    habit->color = color;
    habit->color.a = 255;
    habit->sync_mode = sync_mode;
    habit->sync_activity = sync_activity;
    habits->selected = habits->count;
    habits->count++;
    inbe_habits_save(habits);
    return habits->selected;
}

static void
inbe_habits_add_seed(InbeHabits *habits, const char *id, const char *name,
                     Color color, int activity_mask)
{
    InbeHabit *habit;

    if(habits == NULL || habits->count >= INBE_HABIT_MAX)
        return;

    habit = &habits->items[habits->count++];
    memset(habit, 0, sizeof(*habit));
    copy_text(habit->id, sizeof(habit->id), id);
    copy_text(habit->name, sizeof(habit->name), name);
    habit->color = color;
    habit->sync_mode = INBE_HABIT_SYNC_ACTIVITIES;
    habit->sync_activity = activity_mask;
}

void
inbe_habits_init(InbeHabits *habits)
{
    if(habits == NULL)
        return;
    data_init();
    if(inbe_storage_habits_load(habits)) {
        if(habits->count == 3 &&
           strcmp(habits->items[0].id, "mind") == 0 &&
           strcmp(habits->items[1].id, "yoga") == 0 &&
           strcmp(habits->items[2].id, "fitness") == 0) {
            inbe_habits_add_default_set(habits);
            return;
        }
        if(habits->selected < 0 || habits->selected >= habits->count)
            habits->selected = 0;
        habits->loaded = 1;
        return;
    }
    inbe_habits_add_default_set(habits);
}

void
inbe_habit_set_day(InbeHabits *habits, int index, int day_index, int completed)
{
    InbeHabit *habit;
    int existing_index;

    if(habits == NULL || index < 0 || index >= habits->count || day_index <= 0)
        return;

    habit = &habits->items[index];
    existing_index = habit_find_day(habit, day_index);
    if(existing_index >= 0) {
        habit->days[existing_index].completed = completed != 0;
    } else if(completed && habit->day_count < INBE_HABIT_MAX_DAYS) {
        habit->days[habit->day_count].day_index = day_index;
        habit->days[habit->day_count].completed = 1;
        habit->day_count++;
    }
    habits->selected = index;
    inbe_habits_save(habits);
}

void
inbe_habit_toggle_day(InbeHabits *habits, int index, int day_index)
{
    InbeHabit *habit;
    int existing_index;

    if(habits == NULL || index < 0 || index >= habits->count || day_index <= 0)
        return;

    habit = &habits->items[index];
    existing_index = habit_find_day(habit, day_index);
    if(existing_index >= 0) {
        habit->days[existing_index].completed = !habit->days[existing_index].completed;
    } else if(habit->day_count < INBE_HABIT_MAX_DAYS) {
        habit->days[habit->day_count].day_index = day_index;
        habit->days[habit->day_count].completed = 1;
        habit->day_count++;
    }
    habits->selected = index;
    inbe_habits_save(habits);
}

void
inbe_habit_toggle_today(InbeHabits *habits, int index)
{
    inbe_habit_toggle_day(habits, index, inbe_habits_today_index());
}

int
habit_activity_mask_for(int exercise)
{
    if(exercise < 0 || exercise >= EXERCISE_COUNT)
        return 0;
    return 1 << exercise;
}

int
habit_matches_activity(const InbeHabit *habit, int exercise_type)
{
    if(habit == NULL)
        return 0;
    if(habit->sync_mode == INBE_HABIT_SYNC_ACTIVITIES)
        return (habit->sync_activity & habit_activity_mask_for(exercise_type)) != 0;
    return 0;
}

void
sync_habits_for_activity(InbeApp *app, int exercise_type)
{
    int today;
    int selected;
    int changed = 0;

    if(app == NULL)
        return;

    today = inbe_habits_today_index();
    selected = app->habits.selected;
    for(int i = 0; i < app->habits.count; i++) {
        InbeHabit *habit = &app->habits.items[i];
        if(habit_matches_activity(habit, exercise_type)) {
            if(!inbe_habit_completed_day(habit, today)) {
                inbe_habit_set_day(&app->habits, i, today, 1);
                changed = 1;
            }
        }
    }
    if(changed) {
        app->habits.selected = selected;
        inbe_habits_save(&app->habits);
    }
}

/* Habit utility helpers */
int
habit_is_linked(const InbeHabit *habit)
{
    return habit != NULL && habit->sync_mode != INBE_HABIT_SYNC_NONE;
}

static void
habit_format_date(int day_index, char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "%04d-%02d-%02d",
             day_index / 10000, (day_index / 100) % 100, day_index % 100);
}

/* Habit edit functions */
void
habit_edit_begin_new(InbeApp *app)
{
    if(app == NULL)
        return;

    snprintf(app->habit_edit_text, sizeof(app->habit_edit_text), "%s", "New Habit");
    app->habit_edit_active = 1;
    app->habit_edit_is_new = 1;
    app->habit_edit_index = -1;
    app->habit_edit_cursor = (int)strlen(app->habit_edit_text);
    app->habit_edit_color = (Color){99, 196, 165, 255};
    app->habit_edit_sync_mode = INBE_HABIT_SYNC_NONE;
    app->habit_edit_sync_activity = 0;
    app->inbe.screen = InbeScreenHabitEdit;
}

void
habit_edit_begin(InbeApp *app, int index)
{
    if(app == NULL || index < 0 || index >= app->habits.count)
        return;

    snprintf(app->habit_edit_text, sizeof(app->habit_edit_text), "%s",
             app->habits.items[index].name);
    app->habit_edit_active = 1;
    app->habit_edit_is_new = 0;
    app->habit_edit_index = index;
    app->habit_edit_cursor = (int)strlen(app->habit_edit_text);
    app->habit_edit_color = app->habits.items[index].color;
    app->habit_edit_sync_mode = app->habits.items[index].sync_mode;
    app->habit_edit_sync_activity = app->habits.items[index].sync_activity;
    app->inbe.screen = InbeScreenHabitEdit;
}

void
habit_edit_cancel(InbeApp *app)
{
    if(app == NULL)
        return;

    app->habit_edit_active = 0;
    app->habit_edit_is_new = 0;
    app->habit_edit_index = -1;
    app->habit_edit_cursor = 0;
    app->habit_edit_text[0] = '\0';
    ui_focus_set_text_input_active(0);
}

static const char *
habit_edit_trimmed_text(InbeApp *app)
{
    char *start;
    char *end;

    if(app == NULL)
        return "";

    start = app->habit_edit_text;
    while(*start == ' ' || *start == '\t')
        start++;
    end = start + strlen(start);
    while(end > start && (end[-1] == ' ' || end[-1] == '\t'))
        end--;
    *end = '\0';
    return start;
}

void
habit_edit_commit(InbeApp *app)
{
    const char *text;
    int index;

    if(app == NULL || !app->habit_edit_active)
        return;

    index = app->habit_edit_index;
    if(!app->habit_edit_is_new && (index < 0 || index >= app->habits.count)) {
        habit_edit_cancel(app);
        return;
    }

    text = habit_edit_trimmed_text(app);
    if(text[0] != '\0') {
        if(app->habit_edit_sync_activity != 0)
            app->habit_edit_sync_mode = INBE_HABIT_SYNC_ACTIVITIES;
        else
            app->habit_edit_sync_mode = INBE_HABIT_SYNC_NONE;
        if(app->habit_edit_is_new) {
            inbe_habits_add_custom(&app->habits, text, app->habit_edit_color,
                                   app->habit_edit_sync_mode,
                                   app->habit_edit_sync_activity);
        } else {
            snprintf(app->habits.items[index].name,
                     sizeof(app->habits.items[index].name), "%s", text);
            app->habits.items[index].color = app->habit_edit_color;
            app->habits.items[index].color.a = 255;
            app->habits.items[index].sync_mode = app->habit_edit_sync_mode;
            app->habits.items[index].sync_activity = app->habit_edit_sync_activity;
            app->habits.selected = index;
            inbe_habits_save(&app->habits);
        }
    }
    habit_edit_cancel(app);
    app->inbe.screen = InbeScreenHabits;
}

/* Habit linked session functions */
static void
habit_linked_session_callback(const char *path, int year, int month, int day,
                              int hour, int minute, int second,
                              int topic, int activity,
                              const int *round_times, int round_count, void *user)
{
    HabitLinkedContext *ctx = user;
    HabitLinkedEntry *entry;
    int day_index = year * 10000 + month * 100 + day;

    (void)path;
    if(ctx == NULL || round_times == NULL || round_count <= 0)
        return;
    if(ctx->day_filter > 0 && ctx->day_filter != day_index)
        return;
    if(activity < 0 || activity >= EXERCISE_COUNT)
        activity = EXERCISE_WIM_HOF;
    if(ctx->sync_mode == INBE_HABIT_SYNC_ACTIVITIES &&
       (ctx->sync_activity & habit_activity_mask_for(activity)) == 0)
        return;
    (void)topic;
    if(ctx->count >= HABIT_LINKED_ENTRY_MAX)
        return;

    entry = &ctx->entries[ctx->count++];
    memset(entry, 0, sizeof(*entry));
    snprintf(entry->path, sizeof(entry->path), "%s", path != NULL ? path : "");
    entry->year = year;
    entry->month = month;
    entry->day = day;
    entry->hour = hour;
    entry->minute = minute;
    entry->second = second;
    entry->activity = activity;
    entry->round_count = round_count > MaxRounds ? MaxRounds : round_count;
    for(int i = 0; i < entry->round_count; i++) {
        entry->rounds[i] = round_times[i];
        entry->total_seconds += round_times[i];
        if(round_times[i] > entry->best_seconds)
            entry->best_seconds = round_times[i];
    }
    ctx->total_seconds += entry->total_seconds;
    if(entry->best_seconds > ctx->best_seconds)
        ctx->best_seconds = entry->best_seconds;
}

static void
habit_collect_linked_entries(const InbeHabit *habit, int day_filter, HabitLinkedContext *ctx)
{
    if(ctx == NULL)
        return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->day_filter = day_filter;
    ctx->sync_mode = habit != NULL ? habit->sync_mode : INBE_HABIT_SYNC_NONE;
    ctx->sync_activity = habit != NULL ? habit->sync_activity : EXERCISE_WIM_HOF;
    data_list_session_records(habit_linked_session_callback, ctx);
}

void
habit_session_cancel_edit(InbeApp *app)
{
    if(app == NULL)
        return;
    app->habit_session_edit_active = 0;
    app->habit_session_edit_kind = 0;
    app->habit_session_edit_round = -1;
    app->habit_session_edit_cursor = 0;
    app->habit_session_edit_path[0] = '\0';
    app->habit_session_edit_text[0] = '\0';
    ui_focus_set_text_input_active(0);
}

static void
habit_session_begin_time_edit(InbeApp *app, const HabitLinkedEntry *entry)
{
    if(app == NULL || entry == NULL)
        return;
    app->habit_session_edit_active = 1;
    app->habit_session_edit_kind = HABIT_SESSION_EDIT_TIME;
    app->habit_session_edit_round = -1;
    snprintf(app->habit_session_edit_path, sizeof(app->habit_session_edit_path),
             "%s", entry->path);
    snprintf(app->habit_session_edit_text, sizeof(app->habit_session_edit_text),
             "%02d:%02d", entry->hour, entry->minute);
    app->habit_session_edit_cursor = (int)strlen(app->habit_session_edit_text);
}

static void
habit_session_begin_round_edit(InbeApp *app, const HabitLinkedEntry *entry, int round)
{
    if(app == NULL || entry == NULL || round < 0 || round >= entry->round_count)
        return;
    app->habit_session_edit_active = 1;
    app->habit_session_edit_kind = HABIT_SESSION_EDIT_ROUND;
    app->habit_session_edit_round = round;
    snprintf(app->habit_session_edit_path, sizeof(app->habit_session_edit_path),
             "%s", entry->path);
    snprintf(app->habit_session_edit_text, sizeof(app->habit_session_edit_text),
             "%d", entry->rounds[round]);
    app->habit_session_edit_cursor = (int)strlen(app->habit_session_edit_text);
}

static int
habit_session_text_filter(int codepoint, void *user_data)
{
    InbeApp *app = (InbeApp *)user_data;

    if(codepoint >= '0' && codepoint <= '9')
        return 1;
    return app != NULL &&
           app->habit_session_edit_kind == HABIT_SESSION_EDIT_TIME &&
           codepoint == ':';
}

static int
habit_session_delete_round(const HabitLinkedEntry *entry, int round)
{
    int rounds[MaxRounds];
    int out_count = 0;

    if(entry == NULL || round < 0 || round >= entry->round_count)
        return 0;
    if(entry->round_count <= 1)
        return data_delete_session(entry->path);

    for(int i = 0; i < entry->round_count; i++) {
        if(i != round)
            rounds[out_count++] = entry->rounds[i];
    }
    return data_replace_session(entry->path, rounds, out_count);
}

static int
habit_session_handle_physical_keyboard(InbeApp *app, const HabitLinkedEntry *entry)
{
    int commit_pressed = 0;

    if(app == NULL || entry == NULL || !app->habit_session_edit_active)
        return 0;

    if(IsKeyPressed(KEY_ESCAPE)) {
        habit_session_cancel_edit(app);
        return 1;
    }

    flint_ui_text_edit((FlintUITextEdit){
        .text = app->habit_session_edit_text,
        .text_size = sizeof(app->habit_session_edit_text),
        .cursor_position = &app->habit_session_edit_cursor,
        .max_codepoints = app->habit_session_edit_kind == HABIT_SESSION_EDIT_TIME ? 5 : 3,
        .filter = habit_session_text_filter,
        .filter_user_data = app,
        .commit_pressed = &commit_pressed
    });

    if(commit_pressed)
        return habit_session_commit_edit(app, entry);

    return 0;
}

/* Habit session edit screen */
void
draw_habit_session_edit_screen(InbeApp *app)
{
    HabitLinkedContext ctx;
    InbeHabit *habit;
    char date_text[32];
    int top_h = flint_px(58);
    int nav_h = flint_px(TAB_BAR_H);
    int keyboard_h;
    int viewport_h;
    int content_x;
    int content_w;
    int max_w = flint_px(400);
    int side_padding = flint_page_side_padding();
    int y = top_h + flint_px(14);
    int content_h;
    FlintUIHeader header;
    FlintUIScrollArea scroll_area;
    FlintUIScrollView scroll_view;

    if(app == NULL)
        return;
    if(app->habit_detail_index < 0 || app->habit_detail_index >= app->habits.count) {
        app->inbe.screen = InbeScreenHabits;
        return;
    }

    habit = &app->habits.items[app->habit_detail_index];
    habit_collect_linked_entries(habit, app->habit_detail_day, &ctx);
    if(ctx.count <= 0) {
        habit_session_cancel_edit(app);
        app->inbe.screen = InbeScreenHabits;
        return;
    }
    habit_format_date(app->habit_detail_day, date_text, sizeof(date_text));

    keyboard_h = habit_session_keyboard_height(app);
    viewport_h = view_height - top_h - nav_h - keyboard_h;
    if(viewport_h < flint_px(80))
        viewport_h = flint_px(80);

    header = ui_draw_title_header(top_h, date_text,
                                  app->icons[UI_ICON_TYPE_RETURN], (Texture2D){0});
    if(header.left_clicked) {
        habit_session_cancel_edit(app);
        app->inbe.screen = InbeScreenHabits;
        return;
    }

    flint_centered_column(max_w, side_padding, &content_x, &content_w);
    content_h = draw_habit_session_edit_content(app, &ctx, content_x, content_w, y, 0) - y;
    scroll_area = (FlintUIScrollArea){
        .bounds = {(float)content_x, (float)y, (float)content_w, (float)viewport_h},
        .content_height = content_h,
        .scroll_offset = &app->habit_session_edit_scroll,
        .wheel_step = flint_px(42)
    };
    scroll_view = ui_scroll_container_begin(scroll_area);
    draw_habit_session_edit_content(app, &ctx, scroll_view.content_x, scroll_view.content_w,
                                    scroll_view.content_y, 1);
    ui_scroll_container_end(scroll_area, scroll_view);

    if(app->habit_session_edit_active) {
        HabitLinkedEntry *active_entry = NULL;
        for(int i = 0; i < ctx.count; i++) {
            if(strcmp(app->habit_session_edit_path, ctx.entries[i].path) == 0) {
                active_entry = &ctx.entries[i];
                break;
            }
        }
        if(active_entry != NULL) {
            if(habit_session_handle_physical_keyboard(app, active_entry))
                return;
            habit_session_draw_keyboard(app, active_entry);
        }
    }
}

int
draw_habit_session_edit_content(InbeApp *app, HabitLinkedContext *ctx, int content_x, int content_w, int y, int draw)
{
    int row_h = flint_px(34);
    int section_gap = flint_px(10);
    int section_h = flint_px(28);
    int section_font = flint_ui_font();

    if(app == NULL || ctx == NULL)
        return y;

    if(ctx->count <= 0) {
        if(draw)
            flint_text_draw("No sessions", content_x, y, flint_ui_font(), theme_get_text());
        return y + row_h;
    }

    for(int activity = 0; activity < EXERCISE_COUNT; activity++) {
        int has_activity = 0;
        for(int i = 0; i < ctx->count; i++) {
            if(ctx->entries[i].activity == activity) {
                has_activity = 1;
                break;
            }
        }
        if(!has_activity)
            continue;

        if(draw) {
            flint_text_draw(practice_activity_label(activity), content_x, y,
                            section_font, flint_darken(theme_get_text(), 12));
        }
        y += section_h;

        for(int i = 0; i < ctx->count; i++) {
            char time_text[16];
            char rounds_text[32];
            int icon_size = flint_px(18);
            int icon_padding = flint_px(6);
            int icon_w = icon_size + icon_padding * 2;
            int trash_x = content_x + content_w - icon_w;
            int edit_x = trash_x - icon_w - flint_px(4);
            int row_font = flint_ui_font_small();
            int rounds_x;
            int hover_edit = 0;
            int hover_trash = 0;
            int editing_time;

            if(ctx->entries[i].activity != activity)
                continue;

            snprintf(time_text, sizeof(time_text), "%02d:%02d",
                     ctx->entries[i].hour, ctx->entries[i].minute);
            locale_format(rounds_text, sizeof(rounds_text), "results_rounds",
                          ctx->entries[i].round_count);
            rounds_x = content_x + flint_px(76);
            if(rounds_x + flint_text_measure(rounds_text, row_font) > edit_x - flint_px(8))
                rounds_x = content_x + flint_px(62);
            editing_time = app->habit_session_edit_active &&
                           app->habit_session_edit_kind == HABIT_SESSION_EDIT_TIME &&
                           strcmp(app->habit_session_edit_path, ctx->entries[i].path) == 0;

            if(draw) {
                const char *visible_time = editing_time ? app->habit_session_edit_text : time_text;
                Color time_color = editing_time ? theme_get_button_hover() : theme_get_text();
                flint_text_draw(visible_time, content_x,
                                flint_ui_text_y(visible_time, y, row_h, row_font),
                                row_font, time_color);
                flint_text_draw(rounds_text, rounds_x,
                                flint_ui_text_y(rounds_text, y, row_h, row_font),
                                row_font, theme_get_text());
                if(ui_draw_icon_btn_padded(edit_x, y - flint_px(4), icon_size, icon_padding,
                                           app->icons[UI_ICON_TYPE_PENCIL], &hover_edit)) {
                    habit_session_begin_time_edit(app, &ctx->entries[i]);
                    return y;
                }
                if(ui_draw_icon_btn_padded(trash_x, y - flint_px(4), icon_size, icon_padding,
                                           app->icons[UI_ICON_TYPE_TRASH], &hover_trash)) {
                    data_delete_session(ctx->entries[i].path);
                    habit_session_cancel_edit(app);
                    return y;
                }
            }
            y += row_h;

            for(int r = 0; r < ctx->entries[i].round_count; r++) {
                char round_line[64];
                int round_trash_x = content_x + content_w - icon_w;
                int round_edit_x = round_trash_x - icon_w - flint_px(4);
                int round_text_x = content_x + flint_px(16);
                int round_text_w = round_edit_x - round_text_x - flint_px(8);
                int editing_round = app->habit_session_edit_active &&
                                    app->habit_session_edit_kind == HABIT_SESSION_EDIT_ROUND &&
                                    app->habit_session_edit_round == r &&
                                    strcmp(app->habit_session_edit_path, ctx->entries[i].path) == 0;
                int hover_round_edit = 0;
                int hover_round_trash = 0;
                locale_format(round_line, sizeof(round_line), "round_result_label",
                              r + 1, ctx->entries[i].rounds[r]);
                if(draw) {
                    if(editing_round)
                        locale_format(round_line, sizeof(round_line), "round_result_label",
                                      r + 1, atoi(app->habit_session_edit_text));
                    if(round_text_w < flint_px(80))
                        round_text_w = flint_px(80);
                    ui_begin_scissor(round_text_x, y, round_text_w, flint_px(24));
                    flint_text_draw(round_line, round_text_x,
                                    flint_ui_text_y(round_line, y, flint_px(24), row_font),
                                    row_font,
                                    editing_round ? theme_get_button_hover()
                                                  : theme_get_text());
                    ui_end_scissor();
                    if(ui_draw_icon_btn_padded(round_edit_x, y - flint_px(6),
                                               icon_size, icon_padding,
                                               app->icons[UI_ICON_TYPE_PENCIL], &hover_round_edit)) {
                        habit_session_begin_round_edit(app, &ctx->entries[i], r);
                        return y;
                    }
                    if(ui_draw_icon_btn_padded(round_trash_x, y - flint_px(6),
                                               icon_size, icon_padding,
                                               app->icons[UI_ICON_TYPE_TRASH], &hover_round_trash)) {
                        habit_session_delete_round(&ctx->entries[i], r);
                        habit_session_cancel_edit(app);
                        return y;
                    }
                }
                y += flint_px(24);
            }
            y += flint_px(4);
        }
        y += section_gap;
    }

    return y;
}

/* Habit session keyboard functions */
int
habit_session_keyboard_height(InbeApp *app)
{
    int key_h = flint_px(48);
    int gap = flint_px(6);
    int pad = flint_px(10);

#if !(defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID))
    if(app == NULL || !app->on_screen_keyboard_enabled)
        return 0;
#endif
    if(app == NULL || !app->habit_session_edit_active)
        return 0;
    return pad * 2 + key_h * 4 + gap * 3;
}

int
habit_session_draw_keyboard(InbeApp *app, const HabitLinkedEntry *entry)
{
    const char *labels[12] = {
        "1", "2", "3",
        "4", "5", "6",
        "7", "8", "9",
        "DEL", "0", "OK"
    };
    int key_h = flint_px(48);
    int gap = flint_px(6);
    int pad = flint_px(10);
    int keyboard_h = habit_session_keyboard_height(app);
    int x = flint_page_side_padding();
    int y = view_height - keyboard_h;
    int w = view_width - x * 2;
    int key_w = (w - gap * 2) / 3;

#if !(defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID))
    if(app == NULL || !app->on_screen_keyboard_enabled)
        return 0;
#endif
    if(app == NULL || !app->habit_session_edit_active || keyboard_h <= 0)
        return 0;

    DrawRectangle(0, y, view_width, keyboard_h, flint_darken(theme_get_bg(), 10));
    DrawLine(0, y, view_width, y, flint_darken(theme_get_bg(), 42));

    for(int i = 0; i < 12; i++) {
        int col = i % 3;
        int row = i / 3;
        int key_x = x + col * (key_w + gap);
        int key_y = y + pad + row * (key_h + gap);
        if(habit_session_keyboard_key(key_x, key_y, key_w, key_h, labels[i])) {
            if(i == 9) {
                habit_session_delete_before_cursor(app);
            } else if(i == 11) {
                if(habit_session_commit_edit(app, entry))
                    return 1;
            } else {
                habit_session_insert_char(app, labels[i][0]);
            }
        }
    }

    return 0;
}

/* Habit session helper functions */
int
habit_session_keyboard_key(int x, int y, int w, int h, const char *label)
{
    int hover = 0;
    return ui_draw_generic_button(x, y, w, h, label, UI_BUTTON_STYLE_SECONDARY, 0, &hover);
}

void
habit_session_clamp_cursor(InbeApp *app)
{
    int len;

    if(app == NULL)
        return;
    len = (int)strlen(app->habit_session_edit_text);
    if(app->habit_session_edit_cursor < 0)
        app->habit_session_edit_cursor = 0;
    if(app->habit_session_edit_cursor > len)
        app->habit_session_edit_cursor = len;
}

void
habit_session_delete_before_cursor(InbeApp *app)
{
    size_t len;
    int cursor;

    if(app == NULL)
        return;
    habit_session_clamp_cursor(app);
    len = strlen(app->habit_session_edit_text);
    cursor = app->habit_session_edit_cursor;
    if(cursor <= 0 || len == 0)
        return;
    memmove(app->habit_session_edit_text + cursor - 1,
            app->habit_session_edit_text + cursor,
            len - (size_t)cursor + 1);
    app->habit_session_edit_cursor--;
}

void
habit_session_insert_char(InbeApp *app, char c)
{
    size_t len;
    int max_len;
    int cursor;

    if(app == NULL)
        return;
    habit_session_clamp_cursor(app);
    len = strlen(app->habit_session_edit_text);
    max_len = app->habit_session_edit_kind == HABIT_SESSION_EDIT_TIME ? 5 : 3;
    cursor = app->habit_session_edit_cursor;

    if(app->habit_session_edit_kind == HABIT_SESSION_EDIT_TIME &&
       c >= '0' && c <= '9' &&
       cursor < (int)len &&
       app->habit_session_edit_text[cursor] == ':') {
        cursor++;
        app->habit_session_edit_cursor = cursor;
    }

    if(len < (size_t)max_len) {
        memmove(app->habit_session_edit_text + cursor + 1,
                app->habit_session_edit_text + cursor,
                len - (size_t)cursor + 1);
        app->habit_session_edit_text[cursor] = c;
        app->habit_session_edit_cursor = cursor + 1;
        return;
    }

    if(cursor < (int)len) {
        if(app->habit_session_edit_kind == HABIT_SESSION_EDIT_TIME &&
           app->habit_session_edit_text[cursor] == ':' &&
           c != ':')
            return;
        app->habit_session_edit_text[cursor] = c;
        app->habit_session_edit_cursor = cursor + 1;
    }
}

int
habit_session_parse_time(const char *text, int *hour, int *minute)
{
    int h;
    int m;
    char tail;

    if(text == NULL || sscanf(text, "%d:%d%c", &h, &m, &tail) != 2)
        return 0;
    if(h < 0 || h > 23 || m < 0 || m > 59)
        return 0;
    if(hour != NULL)
        *hour = h;
    if(minute != NULL)
        *minute = m;
    return 1;
}

int
habit_session_parse_seconds(const char *text, int *seconds)
{
    int value;
    char tail;

    if(text == NULL || sscanf(text, "%d%c", &value, &tail) != 1)
        return 0;
    if(value <= 0 || value > 999)
        return 0;
    if(seconds != NULL)
        *seconds = value;
    return 1;
}

int
habit_session_commit_edit(InbeApp *app, const HabitLinkedEntry *entry)
{
    if(app == NULL || entry == NULL || !app->habit_session_edit_active)
        return 0;

    if(app->habit_session_edit_kind == HABIT_SESSION_EDIT_TIME) {
        int hour;
        int minute;
        char new_path[FS_PATH_MAX];
        char dir[FS_PATH_MAX];
        char *slash;

        if(!habit_session_parse_time(app->habit_session_edit_text, &hour, &minute))
            return 0;
        snprintf(dir, sizeof(dir), "%s", entry->path);
        slash = strrchr(dir, '/');
        if(slash == NULL) {
            snprintf(new_path, sizeof(new_path), "inbe-%02d%02d%02d",
                     hour, minute, entry->second);
        } else {
            *slash = '\0';
            snprintf(new_path, sizeof(new_path), "%s/inbe-%02d%02d%02d",
                     dir, hour, minute, entry->second);
        }
        if(!data_rename_session(entry->path, new_path))
            return 0;
        habit_session_cancel_edit(app);
        return 1;
    }

    if(app->habit_session_edit_kind == HABIT_SESSION_EDIT_ROUND) {
        int seconds;
        int rounds[MaxRounds];

        if(app->habit_session_edit_round < 0 ||
           app->habit_session_edit_round >= entry->round_count)
            return 0;
        if(!habit_session_parse_seconds(app->habit_session_edit_text, &seconds))
            return 0;
        for(int i = 0; i < entry->round_count; i++)
            rounds[i] = entry->rounds[i];
        rounds[app->habit_session_edit_round] = seconds;
        if(!data_replace_session(entry->path, rounds, entry->round_count))
            return 0;
        habit_session_cancel_edit(app);
        return 1;
    }

    return 0;
}
