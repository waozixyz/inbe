#include "habits_screen.h"

#include "habits/habits.h"

#include "practice_screen.h"
#include "statistics_screen.h"
#include "data.h"
#include "storage.h"
#include "sync_account.h"
#include "app.h"
#include "text_utils.h"
#include "flint_theme.h"
#include "flint_runtime_assets.h"
#include "flint_locale.h"
#include "breath_engine.h"
#include "flint_clip.h"
#include "flint_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

/* Helper functions */
static void habits_add_seed(InbeHabits *habits, const char *id, const char *name,
                            const char *description, Color color,
                            int activity_mask);
static void draw_habit_link_dot(int x, int y, int w, Color color);
static Color habit_text_color_for_background(Color background);

enum {
    HABITS_GUIDE_STEPS = 6,
    HABITS_TOP_H = 36,
    HABITS_TAB_H = 40,
    HABITS_OVERVIEW_DAY_COLUMNS = 4,
    HABITS_OVERVIEW_DETAIL_COLUMNS = 1,
    HABITS_OVERVIEW_COLUMNS = HABITS_OVERVIEW_DAY_COLUMNS + HABITS_OVERVIEW_DETAIL_COLUMNS
};

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
    if(habit == NULL || habit->days == NULL)
        return -1;
    for(int i = 0; i < habit->day_count; i++) {
        if(habit->days[i].day_index == day_index)
            return i;
    }
    return -1;
}

int
habit_counting_enabled(const InbeHabit *habit)
{
    return habit != NULL && habit->counter_enabled;
}

static int
habit_web_context_click_in_bounds(InbeApp *app, Rectangle bounds)
{
#if defined(PLATFORM_WEB)
    Vector2 top_left;
    Vector2 bottom_right;

    if(app == NULL)
        return 0;
    top_left = GetWorldToScreen2D((Vector2){bounds.x, bounds.y}, app->camera);
    bottom_right = GetWorldToScreen2D((Vector2){bounds.x + bounds.width,
                                                bounds.y + bounds.height}, app->camera);
    return EM_ASM_INT({
        const click = Module.__inbeContextClick;
        if(!click)
            return 0;
        if(Date.now() - click.time > 750) {
            Module.__inbeContextClick = null;
            return 0;
        }
        if(click.x >= $0 && click.x <= $2 && click.y >= $1 && click.y <= $3) {
            Module.__inbeContextClick = null;
            return 1;
        }
        return 0;
    }, (int)top_left.x, (int)top_left.y, (int)bottom_right.x, (int)bottom_right.y);
#else
    (void)app;
    (void)bounds;
    return 0;
#endif
}

static int
habit_counter_day_action(InbeApp *app, int habit_index, int day_index,
                         int x, int y, int w, int h, int disabled,
                         int allow_left_increment)
{
    Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int inside = !disabled && CheckCollisionPointRec(mouse, bounds);
    int same = app->habit_counter_press_index == habit_index &&
               app->habit_counter_press_day == day_index;
    int captured = ui_input_captures_click(mouse);

    if(disabled || captured)
        return 0;
    if((inside && IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) ||
       habit_web_context_click_in_bounds(app, bounds))
        return -1;

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inside) {
        app->habit_counter_press_index = habit_index;
        app->habit_counter_press_day = day_index;
        app->habit_counter_press_frames = 0;
        app->habit_counter_press_long_done = 0;
        app->habit_counter_press_start_x = (int)mouse.x;
        app->habit_counter_press_start_y = (int)mouse.y;
        same = 1;
    }

    if(same && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        int dx = (int)mouse.x - app->habit_counter_press_start_x;
        int dy = (int)mouse.y - app->habit_counter_press_start_y;
        int threshold = flint_px(8);

        if(dx > threshold || dx < -threshold || dy > threshold || dy < -threshold) {
            app->habit_counter_press_day = 0;
            app->habit_counter_press_index = -1;
            return 0;
        }
        app->habit_counter_press_frames++;
        if(inside && !app->habit_counter_press_long_done &&
           app->habit_counter_press_frames >= HABIT_COUNTER_LONG_PRESS_FRAMES) {
            app->habit_counter_press_long_done = 1;
            return -1;
        }
    }

    if(same && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        int action;
        if(inside && app->habit_counter_press_long_done)
            action = -2;
        else
            action = inside && allow_left_increment ? 1 : 0;
        app->habit_counter_press_day = 0;
        app->habit_counter_press_index = -1;
        app->habit_counter_press_frames = 0;
        app->habit_counter_press_long_done = 0;
        return action;
    }

    return 0;
}

int
habit_reserve_days(InbeHabit *habit, int capacity)
{
    InbeHabitDay *days;
    int new_capacity;

    if(habit == NULL || capacity <= habit->day_capacity)
        return habit != NULL;
    new_capacity = habit->day_capacity > 0 ? habit->day_capacity : 16;
    while(new_capacity < capacity) {
        if(new_capacity > 1073741823 / 2)
            return 0;
        new_capacity *= 2;
    }
    days = realloc(habit->days, (size_t)new_capacity * sizeof(*days));
    if(days == NULL)
        return 0;
    memset(days + habit->day_capacity, 0,
           (size_t)(new_capacity - habit->day_capacity) * sizeof(*days));
    habit->days = days;
    habit->day_capacity = new_capacity;
    return 1;
}

void
habits_free(InbeHabits *habits)
{
    if(habits == NULL)
        return;
    for(int i = 0; i < INBE_HABIT_MAX; i++) {
        free(habits->items[i].days);
        habits->items[i].days = NULL;
        habits->items[i].day_count = 0;
        habits->items[i].day_capacity = 0;
    }
}

/* Core habits functions from habits.c */
int
habits_today_index(void)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if(tm == NULL)
        return 0;
    return (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday;
}

int
habit_completed_day(const InbeHabit *habit, int day_index)
{
    int index = habit_find_day(habit, day_index);
    return index >= 0 &&
           (habit->days[index].completed || habit->days[index].count > 0);
}

int
habit_day_count(const InbeHabit *habit, int day_index)
{
    int index = habit_find_day(habit, day_index);
    if(index < 0)
        return 0;
    if(habit->days[index].count > 0)
        return habit->days[index].count;
    return habit->days[index].completed ? 1 : 0;
}

static int
habit_day_session_count(const InbeHabit *habit, int day_index)
{
    int index = habit_find_day(habit, day_index);

    if(index < 0)
        return 0;
    return habit->days[index].session_count;
}

int
habit_completed_today(const InbeHabit *habit)
{
    return habit_completed_day(habit, habits_today_index());
}

void
habits_save(InbeHabits *habits)
{
    if(habits != NULL)
        habits->dirty = 0;
    storage_habits_save(habits);
    if(habits != NULL) {
        habits->loaded_count = habits->count;
        for(int i = 0; i < habits->loaded_count; i++)
            copy_text(habits->loaded_ids[i], sizeof(habits->loaded_ids[i]),
                      habits->items[i].id);
        for(int i = habits->loaded_count; i < INBE_HABIT_MAX; i++)
            habits->loaded_ids[i][0] = '\0';
    }
    return;
}

void
habits_flush_save(InbeApp *app)
{
    if(app == NULL || !app->habits.dirty)
        return;
    habits_save(&app->habits);
}

int
habits_clear_days(InbeHabits *habits)
{
    int cleared = 0;

    if(habits == NULL)
        return 0;
    for(int i = 0; i < habits->count; i++) {
        InbeHabit *habit = &habits->items[i];
        for(int d = 0; d < habit->day_count; d++) {
            if(habit->days[d].day_index > 0 || habit->days[d].completed ||
               habit->days[d].count > 0)
                cleared++;
        }
        habit->day_count = 0;
    }
    return cleared;
}

void
habits_add_default(InbeHabits *habits)
{
    int number;
    char name[INBE_HABIT_NAME_SIZE];

    if(habits == NULL || habits->count >= INBE_HABIT_MAX)
        return;

    number = habits->count + 1;
    snprintf(name, sizeof(name), locale_get("habit_default_name_format"), number);
    habits_add_custom(habits, name, (Color){99, 196, 165, 255},
                           INBE_HABIT_SYNC_NONE, 0);
}

void
habits_add_default_set(InbeHabits *habits)
{
    if(habits == NULL)
        return;

    habits_free(habits);
    memset(habits, 0, sizeof(*habits));
    habits_add_seed(habits, "meditation", locale_get("habit_default_meditation_name"),
                    locale_get("habit_default_meditation_description"),
                    (Color){126, 183, 230, 255},
                    habit_activity_mask_for(EXERCISE_WIM_HOF) |
                    habit_activity_mask_for(EXERCISE_MEDITATION));
    habits_add_seed(habits, "yoga",
                    locale_get("habit_default_yoga_name"),
                    locale_get("habit_default_yoga_description"),
                    (Color){239, 178, 102, 255},
                    habit_activity_mask_for(EXERCISE_SUN_SALUTATION));
    habits->selected = 0;
    habits->loaded = 1;
    habits_save(habits);
}

void
habits_delete(InbeHabits *habits, int index)
{
    if(habits == NULL || index < 0 || index >= habits->count)
        return;

    free(habits->items[index].days);
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

    habits_save(habits);
}

int
habits_move(InbeHabits *habits, int from_index, int to_index)
{
    InbeHabit moved;

    if(habits == NULL || from_index < 0 || to_index < 0 ||
       from_index >= habits->count || to_index >= habits->count ||
       from_index == to_index)
        return 0;

    moved = habits->items[from_index];
    if(from_index < to_index) {
        for(int i = from_index; i < to_index; i++)
            habits->items[i] = habits->items[i + 1];
    } else {
        for(int i = from_index; i > to_index; i--)
            habits->items[i] = habits->items[i - 1];
    }
    habits->items[to_index] = moved;

    if(habits->selected == from_index) {
        habits->selected = to_index;
    } else if(from_index < to_index &&
              habits->selected > from_index && habits->selected <= to_index) {
        habits->selected--;
    } else if(from_index > to_index &&
              habits->selected >= to_index && habits->selected < from_index) {
        habits->selected++;
    }

    habits_save(habits);
    return 1;
}

int
habits_name_exists(const InbeHabits *habits, const char *name, int exclude_index)
{
    if(habits == NULL || name == NULL)
        return 0;

    for(int i = 0; i < habits->count; i++) {
        if(i == exclude_index)
            continue;  // Skip current habit when editing
        if(strcmp(habits->items[i].name, name) == 0)
            return 1;
    }
    return 0;
}

void
habits_generate_unique_name(InbeHabits *habits, char *name_buffer, size_t buffer_size, const char *base_name)
{
    if(habits == NULL || name_buffer == NULL || base_name == NULL)
        return;

    // Try base name first
    snprintf(name_buffer, buffer_size, "%s", base_name);

    if(!habits_name_exists(habits, name_buffer, -1))
        return;

    // Append numbers until we find a unique name
    for(int i = 2; i < 100; i++) {
        snprintf(name_buffer, buffer_size, "%s %d", base_name, i);
        if(!habits_name_exists(habits, name_buffer, -1))
            return;
    }

    // Fallback to timestamp-based name
    snprintf(name_buffer, buffer_size, "%s %ld", base_name, (long)time(NULL));
}

int
habits_add_custom(InbeHabits *habits, const char *name, Color color,
                       int sync_mode, int sync_activity)
{
    InbeHabit *habit;
    char unique_name[INBE_HABIT_NAME_SIZE];

    if(habits == NULL || habits->count >= INBE_HABIT_MAX)
        return -1;

    // Ensure name is unique
    const char *final_name = name != NULL && name[0] != '\0' ? name : "Habit";
    if(habits_name_exists(habits, final_name, -1)) {
        habits_generate_unique_name(habits, unique_name, sizeof(unique_name), final_name);
        final_name = unique_name;
    }

    habit = &habits->items[habits->count];
    memset(habit, 0, sizeof(*habit));
    storage_make_uuid(habit->id);
    copy_text(habit->name, sizeof(habit->name), final_name);
    habit->color = color;
    habit->color.a = 255;
    habit->sync_mode = sync_mode;
    habit->sync_activity = sync_activity;
    habit->counter_enabled = 0;
    habits->selected = habits->count;
    habits->count++;
    habits_save(habits);
    return habits->selected;
}

static void
habits_add_seed(InbeHabits *habits, const char *id, const char *name,
                const char *description, Color color, int activity_mask)
{
    InbeHabit *habit;

    if(habits == NULL || habits->count >= INBE_HABIT_MAX)
        return;

    habit = &habits->items[habits->count++];
    memset(habit, 0, sizeof(*habit));
    copy_text(habit->id, sizeof(habit->id), id);
    copy_text(habit->name, sizeof(habit->name), name);
    copy_text(habit->description, sizeof(habit->description), description);
    habit->color = color;
    habit->sync_mode = INBE_HABIT_SYNC_ACTIVITIES;
    habit->sync_activity = activity_mask;
    habit->counter_enabled = 0;
}

void
habits_init(InbeHabits *habits)
{
    if(habits == NULL)
        return;
    if(habits->hold_stats_range_days != 31)
        habits->hold_stats_range_days = 7;
    data_init();
    if(storage_habits_load(habits)) {
        if(habits->hold_stats_range_days != 31)
            habits->hold_stats_range_days = 7;
        if(habits->count <= 0) {
            habits_add_default_set(habits);
            return;
        }
        if(habits->count == 3 &&
           strcmp(habits->items[0].id, "mind") == 0 &&
           strcmp(habits->items[1].id, "yoga") == 0 &&
           strcmp(habits->items[2].id, "fitness") == 0) {
            habits_add_default_set(habits);
            return;
        }
        if(habits->selected < 0 || habits->selected >= habits->count)
            habits->selected = 0;
        habits->loaded = 1;
        return;
    }
    habits_add_default_set(habits);
    if(habits->hold_stats_range_days != 31)
        habits->hold_stats_range_days = 7;
}

void
habit_set_day(InbeHabits *habits, int index, int day_index, int completed)
{
    InbeHabit *habit;
    int existing_index;
    int count;

    if(habits == NULL || index < 0 || index >= habits->count || day_index <= 0)
        return;

    habit = &habits->items[index];
    count = completed ? 1 : 0;
    existing_index = habit_find_day(habit, day_index);
    if(existing_index >= 0) {
        habit->days[existing_index].completed = completed != 0;
        habit->days[existing_index].count = count;
    } else if(completed && habit_reserve_days(habit, habit->day_count + 1)) {
        habit->days[habit->day_count].day_index = day_index;
        habit->days[habit->day_count].completed = 1;
        habit->days[habit->day_count].count = count;
        habit->day_count++;
    }
    habits->selected = index;
    habits->dirty = !storage_habit_day_save(habit->id, day_index, completed != 0, count);
}

void
habit_set_day_count(InbeHabits *habits, int index, int day_index, int count)
{
    InbeHabit *habit;
    int existing_index;

    if(habits == NULL || index < 0 || index >= habits->count || day_index <= 0)
        return;
    if(count < 0)
        count = 0;

    habit = &habits->items[index];
    existing_index = habit_find_day(habit, day_index);
    if(existing_index >= 0) {
        habit->days[existing_index].count = count;
        habit->days[existing_index].completed = count > 0;
    } else if(count > 0 && habit_reserve_days(habit, habit->day_count + 1)) {
        habit->days[habit->day_count].day_index = day_index;
        habit->days[habit->day_count].count = count;
        habit->days[habit->day_count].completed = 1;
        habit->day_count++;
    }
    habits->selected = index;
    habits->dirty = !storage_habit_day_save(habit->id, day_index, count > 0, count);
}

void
habit_toggle_day(InbeHabits *habits, int index, int day_index)
{
    InbeHabit *habit;
    int existing_index;

    if(habits == NULL || index < 0 || index >= habits->count || day_index <= 0)
        return;

    habit = &habits->items[index];
    existing_index = habit_find_day(habit, day_index);
    if(existing_index >= 0) {
        int completed = !(habit->days[existing_index].completed ||
                          habit->days[existing_index].count > 0);
        habit->days[existing_index].completed = completed;
        habit->days[existing_index].count = completed ? 1 : 0;
    } else if(habit_reserve_days(habit, habit->day_count + 1)) {
        habit->days[habit->day_count].day_index = day_index;
        habit->days[habit->day_count].completed = 1;
        habit->days[habit->day_count].count = 1;
        habit->day_count++;
    }
    habits->selected = index;
    habits->dirty = !storage_habit_day_save(habit->id, day_index,
                                            habit_completed_day(habit, day_index),
                                            habit_day_count(habit, day_index));
}

void
habit_increment_day(InbeHabits *habits, int index, int day_index, int delta)
{
    int count;

    if(habits == NULL || index < 0 || index >= habits->count || day_index <= 0)
        return;
    count = habit_day_count(&habits->items[index], day_index) + delta;
    habit_set_day_count(habits, index, day_index, count);
}

void
habit_toggle_today(InbeHabits *habits, int index)
{
    habit_toggle_day(habits, index, habits_today_index());
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

    today = habits_today_index();
    selected = app->habits.selected;
    for(int i = 0; i < app->habits.count; i++) {
        InbeHabit *habit = &app->habits.items[i];
        if(habit_matches_activity(habit, exercise_type)) {
            if(!habit_completed_day(habit, today)) {
                habit_set_day(&app->habits, i, today, 1);
                changed = 1;
            }
        }
    }
    if(changed) {
        app->habits.selected = selected;
        habits_save(&app->habits);
    }
}

/* Habit utility helpers */
int
habit_is_linked(const InbeHabit *habit)
{
    return habit != NULL && habit->sync_mode != INBE_HABIT_SYNC_NONE;
}

void
habit_format_date(int day_index, char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "%04d-%02d-%02d",
             day_index / 10000, (day_index / 100) % 100, day_index % 100);
}

void
habit_format_duration(int seconds, char *out, size_t out_size)
{
    int hours;
    int minutes;

    if(out == NULL || out_size == 0)
        return;
    if(seconds < 0)
        seconds = 0;

    hours = seconds / 3600;
    minutes = (seconds % 3600) / 60;
    seconds %= 60;

    if(hours > 0)
        snprintf(out, out_size, "%dh %02dm", hours, minutes);
    else
        snprintf(out, out_size, "%dm %02ds", minutes, seconds);
}

int
habit_tm_date_index(const struct tm *tm)
{
    if(tm == NULL)
        return 0;
    return (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday;
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

void
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
    app->habit_session_edit = (HabitSessionEditState){.round = -1};
    ui_focus_set_text_input_active(0);
}

int
habit_linked_has_day(const HabitLinkedContext *ctx, int day_index)
{
    if(ctx == NULL)
        return 0;
    for(int i = 0; i < ctx->count; i++) {
        if(ctx->entries[i].year * 10000 + ctx->entries[i].month * 100 + ctx->entries[i].day == day_index)
            return 1;
    }
    return 0;
}

int
habit_linked_session_count_for_day(const HabitLinkedContext *ctx, int day_index)
{
    int count = 0;

    if(ctx == NULL)
        return 0;
    for(int i = 0; i < ctx->count; i++) {
        int entry_day = ctx->entries[i].year * 10000 +
                        ctx->entries[i].month * 100 +
                        ctx->entries[i].day;
        if(entry_day == day_index)
            count++;
    }
    return count;
}

int
habit_effective_day_count(const InbeHabit *habit, int day_index,
                          const HabitLinkedContext *linked_ctx)
{
    int manual_count = habit_day_count(habit, day_index);
    int session_count = habit_linked_session_count_for_day(linked_ctx, day_index);
    return manual_count > session_count ? manual_count : session_count;
}

void
habit_session_changed(InbeApp *app, int old_session_count)
{
    HabitLinkedContext ctx;
    InbeHabit *habit;
    int manual_count;
    int extra_count;
    int new_count;

    if(app == NULL || app->habit_detail_index < 0 ||
       app->habit_detail_index >= app->habits.count || app->habit_detail_day <= 0)
        return;

    habit = &app->habits.items[app->habit_detail_index];
    if(old_session_count < 0) {
        app_auto_sync(app);
        return;
    }

    manual_count = habit_day_count(habit, app->habit_detail_day);
    extra_count = manual_count - old_session_count;
    if(extra_count < 0)
        extra_count = 0;

    habit_collect_linked_entries(habit, app->habit_detail_day, &ctx);
    new_count = ctx.count + extra_count;
    if(new_count != manual_count)
        habit_set_day_count(&app->habits, app->habit_detail_index,
                                 app->habit_detail_day, new_count);
    app_auto_sync(app);
}

void
habit_apply_count_action(InbeApp *app, int index, int day_index,
                         int delta, int minimum_count)
{
    InbeHabits *habits;
    int count;

    if(app == NULL)
        return;
    habits = &app->habits;
    if(habits == NULL || index < 0 || index >= habits->count || day_index <= 0)
        return;
    count = habit_day_count(&habits->items[index], day_index) + delta;
    if(count < minimum_count)
        count = minimum_count;
    habit_set_day_count(habits, index, day_index, count);
}

void
habit_open_linked_edit_page(InbeApp *app, int habit_index, int day_index)
{
    if(app == NULL)
        return;
    app->habit_detail_index = habit_index;
    app->habit_detail_day = day_index;
    app->habit_detail_session_path[0] = '\0';
    app->habit_session_edit = (HabitSessionEditState){.round = -1};
    app_close_modal(app);
    app_switch_screen(app, InbeScreenHabitSessionEdit);
}

static int
habits_screen_selector_height(InbeApp *app)
{
    (void)app;
    return flint_px(HABITS_TOP_H);
}

int
habits_screen_top_reserved(InbeApp *app)
{
    if(app != NULL && app->habits.screen_mode == HABITS_SCREEN_DETAIL)
        return habits_screen_selector_height(app) + flint_px(HABITS_TAB_H);
    return habits_screen_selector_height(app);
}

static void
habits_persist_view_state(InbeApp *app)
{
    if(app != NULL)
        save_settings(app);
}

static void
habits_select_detail_tab(InbeApp *app, int tab)
{
    if(app == NULL)
        return;
    tab = clampi(tab, HABIT_TAB_WEEKLY, HABIT_TAB_COUNT - 1);
    if(app->habit_edit.active && tab != HABIT_TAB_EDIT)
        habit_edit_commit(app);
    app->habits.tab = tab;
    app->habits.scroll = 0;
    if(tab == HABIT_TAB_WEEKLY) {
        app->habits.view_mode = HABIT_VIEW_WEEKLY;
        if(app->habits.weekly_days < HABIT_WEEKLY_INITIAL_DAYS)
            app->habits.weekly_days = HABIT_WEEKLY_INITIAL_DAYS;
    } else if(tab == HABIT_TAB_MONTHLY) {
        app->habits.view_mode = HABIT_VIEW_CALENDAR;
    } else if(tab == HABIT_TAB_EDIT) {
        if(app->habits.count > 0)
            habit_edit_begin(app, app->habits.selected);
        else
            habit_edit_begin_new(app);
    }
    habits_persist_view_state(app);
}

static void
habits_select_habit(InbeApp *app, int selected_habit)
{
    int was_edit_tab;

    if(app == NULL || selected_habit < 0 || selected_habit >= app->habits.count)
        return;
    was_edit_tab = app->habits.tab == HABIT_TAB_EDIT;
    if(was_edit_tab && app->habit_edit.active)
        habit_edit_commit(app);
    app->habits.selected = selected_habit;
    app->habits.scroll = 0;
    app->habits.weekly_days = HABIT_WEEKLY_INITIAL_DAYS;
    if(was_edit_tab)
        habit_edit_begin(app, app->habits.selected);
    habits_persist_view_state(app);
}

static void
habits_enter_detail(InbeApp *app, int selected_habit)
{
    if(app == NULL)
        return;
    if(selected_habit >= 0 && selected_habit < app->habits.count)
        habits_select_habit(app, selected_habit);
    app->habits.screen_mode = HABITS_SCREEN_DETAIL;
    app->habits.scroll = 0;
    habits_persist_view_state(app);
}

static void
habits_return_to_overview(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app->habit_edit.active)
        habit_edit_commit(app);
    app->habits.screen_mode = HABITS_SCREEN_OVERVIEW;
    app->habits.scroll = 0;
    habits_persist_view_state(app);
}

static void
habits_enter_reorder(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app->habit_edit.active)
        habit_edit_commit(app);
    app->habits.screen_mode = HABITS_SCREEN_REORDER;
    app->habits.scroll = 0;
}

static void
draw_habits_top_bar(InbeApp *app, int draw_menu)
{
    static const char *options[INBE_HABIT_MAX + 2];
    static int dropdown_selected = 0;
    FlintUISubtab tabs[HABIT_TAB_COUNT];
    int option_count;
    int habit_offset = 0;
    int draft_new;
    int selected;
    int top_h = flint_px(HABITS_TOP_H);
    int clicked_tab;

    if(app == NULL)
        return;

    draft_new = app->habit_edit.active && app->habit_edit.is_new;
    option_count = 0;
    if(draft_new) {
        options[option_count++] = "";
        habit_offset = 1;
    }
    for(int i = 0; i < app->habits.count && i < INBE_HABIT_MAX; i++)
        options[option_count++] = app->habits.items[i].name;

    selected = draft_new ? 0 : app->habits.selected + habit_offset;
    if(!draft_new && (app->habits.selected < 0 || app->habits.selected >= app->habits.count))
        selected = 0;

    if(app->habits.screen_mode == HABITS_SCREEN_REORDER) {
        if(draw_menu)
            return;
        if(flint_ui_return_title_bar(app->icons[UI_ICON_TYPE_RETURN], locale_get("habit_reorder_title"), top_h))
            habits_return_to_overview(app);
        return;
    }

    if(app->habits.screen_mode != HABITS_SCREEN_DETAIL) {
        if(draw_menu)
            return;
        flint_ui_title_bar(locale_get("tab_habits"), top_h);
        return;
    }

    if(!draw_menu) {
        int dropdown_h = flint_px(32);

        dropdown_selected = selected;
        if(flint_ui_return_dropdown_title_bar(app->icons[UI_ICON_TYPE_RETURN], (FlintUITitleBarDropdown){
                                                .id = 301,
                                                .options = options,
                                                .option_count = option_count,
                                                .selected_index = &dropdown_selected,
                                                .disabled = app->modal.active,
                                                .min_width = flint_px(120),
                                                .height = dropdown_h
                                            }, top_h))
            habits_return_to_overview(app);

        tabs[HABIT_TAB_WEEKLY] = (FlintUISubtab){
            .icon = app->icons[UI_ICON_TYPE_WEEKLY],
            .icon_size = flint_px(20),
            .disabled = app->modal.active || app->habits.count <= 0
        };
        tabs[HABIT_TAB_MONTHLY] = (FlintUISubtab){
            .icon = app->icons[UI_ICON_TYPE_CALENDAR],
            .icon_size = flint_px(20),
            .disabled = app->modal.active || app->habits.count <= 0
        };
        tabs[HABIT_TAB_STATISTICS] = (FlintUISubtab){
            .icon = app->icons[UI_ICON_TYPE_STAT],
            .icon_size = flint_px(20),
            .disabled = app->modal.active || app->habits.count <= 0
        };
        tabs[HABIT_TAB_EDIT] = (FlintUISubtab){
            .icon = app->icons[UI_ICON_TYPE_EDIT],
            .icon_size = flint_px(20),
            .disabled = app->modal.active
        };
        clicked_tab = ui_draw_subtab_bar((FlintUISubtabBar){
            .bounds = {0, (float)habits_screen_selector_height(app),
                       (float)view_width, (float)flint_px(HABITS_TAB_H)},
            .tabs = tabs,
            .count = HABIT_TAB_COUNT,
            .selected_index = app->habits.tab
        });
        if(clicked_tab >= 0 && clicked_tab < HABIT_TAB_COUNT &&
           clicked_tab != app->habits.tab)
            habits_select_detail_tab(app, clicked_tab);

        return;
    }

    if(app->modal.active)
        return;

    if(ui_draw_dropdown_menu(301)) {
        int selected_habit = dropdown_selected - habit_offset;

        if(draft_new && dropdown_selected == 0)
            return;
        if(selected_habit >= 0 && selected_habit < app->habits.count &&
           app->habits.selected != selected_habit)
            habits_select_habit(app, selected_habit);
    }
}

static int
habits_overview_label_width(int content_w)
{
    int min_label_w = flint_px(56);
    int max_label_w = flint_px(136);
    int gap = flint_px(4);
    int cell = flint_px(28);
    int label_w = content_w - gap * HABITS_OVERVIEW_COLUMNS -
                  cell * HABITS_OVERVIEW_COLUMNS;

    if(label_w < min_label_w)
        label_w = min_label_w;
    if(label_w > max_label_w)
        label_w = max_label_w;
    return label_w;
}

static int
habits_overview_cell_size(int content_w)
{
    int label_w = habits_overview_label_width(content_w);
    int gap = flint_px(4);
    int cell = (content_w - label_w - gap * HABITS_OVERVIEW_COLUMNS) /
               HABITS_OVERVIEW_COLUMNS;

    if(cell > flint_px(28))
        cell = flint_px(28);
    if(cell < flint_px(18))
        cell = flint_px(18);
    return cell;
}

static int
habits_button_label_fits(const char *label, int button_w)
{
    int pad = flint_px(20);

    return flint_text_measure(label != NULL ? label : "", flint_ui_font_small()) + pad <=
           button_w;
}

static int
habits_overview_actions_stack(int content_w)
{
    int btn_gap = flint_px(8);
    int half_w = (content_w - btn_gap) / 2;

    return !habits_button_label_fits(locale_get("habit_new_button"), half_w) ||
           !habits_button_label_fits(locale_get("habit_reorder_title"), half_w);
}

static int
habits_reorder_move_buttons_stack(int content_w)
{
    int btn_w = flint_px(74);
    int gap = flint_px(8);

    return content_w < flint_px(260) ||
           content_w < btn_w * 2 + gap ||
           !habits_button_label_fits(locale_get("move_up_button"), btn_w) ||
           !habits_button_label_fits(locale_get("move_down_button"), btn_w);
}

static int
habits_overview_day_index(int days_ago)
{
    time_t now = time(NULL);
    struct tm day;
    struct tm *local = localtime(&now);

    if(local == NULL)
        return habits_today_index();
    day = *local;
    day.tm_hour = 12;
    day.tm_min = 0;
    day.tm_sec = 0;
    day.tm_mday -= days_ago;
    if(mktime(&day) == (time_t)-1)
        return habits_today_index();
    return (day.tm_year + 1900) * 10000 + (day.tm_mon + 1) * 100 +
           day.tm_mday;
}

static void
habits_overview_date_label(int day_index, char *out, size_t out_size)
{
    int month;
    int day;

    if(out == NULL || out_size == 0)
        return;
    month = (day_index / 100) % 100;
    day = day_index % 100;
    snprintf(out, out_size, "%d/%d", month, day);
}

static int
habits_overview_content_height(int content_w, void *user_data)
{
    InbeApp *app = user_data;
    int rows = app != NULL && app->habits.count > 0 ? app->habits.count : 1;
    int cell = habits_overview_cell_size(content_w);
    int action_h = flint_px(34);

    if(habits_overview_actions_stack(content_w))
        action_h = action_h * 2 + flint_px(8);

    return flint_px(52) + flint_px(20) + rows * (cell + flint_px(8)) +
           flint_px(24) + action_h;
}

static int
habits_reorder_content_height(int content_w, void *user_data)
{
    InbeApp *app = user_data;
    int row_h;
    int count = app != NULL ? app->habits.count : 0;

    if(habits_reorder_move_buttons_stack(content_w))
        row_h = flint_px(36) + flint_px(32) * 2 + flint_px(8) * 2;
    else
        row_h = flint_px(48);
    return flint_px(70) + row_h * (count > 0 ? count : 1);
}

static void
habits_draw_divider(int x, int w, int y)
{
    DrawLine(x, y, x + w, y, flint_darken(flint_theme_get_bg(), 28));
}

static void
habits_draw_fitted_text(const char *text, int x, int y, int max_w, int font, Color color)
{
    char fitted[INBE_HABIT_NAME_SIZE + 4];

    if(text == NULL || max_w <= 0)
        return;
    inbe_text_fit_ellipsis(text, fitted, sizeof(fitted), max_w, font);
    flint_text_draw(fitted, x, y, font, color);
}

static void
draw_habits_reorder(InbeApp *app, int content_top)
{
    int content_bottom;
    int max_w = flint_px(CONTENT_MAX_W);
    int y;

    if(app == NULL)
        return;

    content_bottom = 0;
    {
        FlintUIScrollPage page = ui_scroll_page_begin((FlintUIScrollPageSpec){
            .y = content_top + flint_px(4),
            .height = view_height - content_top - content_bottom - flint_px(4),
            .max_content_width = max_w,
            .scroll_offset = &app->habits.scroll,
            .content_height = habits_reorder_content_height,
            .user_data = app
        });
        int x = page.content_x;
        int w = page.content_w;
        int font = flint_ui_font();
        int small = flint_ui_font_small();
        int btn_h = flint_px(32);
        int fixed_btn_w = flint_px(74);
        int gap = flint_px(8);
        int stack_rows = habits_reorder_move_buttons_stack(w);

        y = page.content_y + flint_px(16);

        if(app->habits.count <= 0) {
            flint_text_draw(locale_get("habit_empty_title"), x, y, small,
                            flint_darken(flint_theme_get_text(), 35));
            ui_scroll_page_end(page);
            return;
        }

        for(int i = 0; i < app->habits.count; i++) {
            int row_y = y;
            int up_hover = 0;
            int down_hover = 0;
            int controls_w = fixed_btn_w * 2 + gap;
            int name_w = stack_rows ? w : w - controls_w - gap;

            habits_draw_fitted_text(app->habits.items[i].name, x, row_y + flint_px(7),
                                    name_w, font, flint_theme_get_text());
            if(stack_rows) {
                row_y += flint_px(36);
            }
            if(ui_draw_generic_button(stack_rows ? x : x + w - fixed_btn_w * 2 - gap,
                                      row_y, stack_rows ? w : fixed_btn_w, btn_h,
                                      locale_get("move_up_button"),
                                      UI_BUTTON_STYLE_SECONDARY, i == 0, &up_hover)) {
                if(habits_move(&app->habits, i, i - 1)) {
                    habits_persist_view_state(app);
                    app_auto_sync(app);
                }
                ui_scroll_page_end(page);
                return;
            }
            if(stack_rows)
                row_y += btn_h + gap;
            if(ui_draw_generic_button(stack_rows ? x : x + w - fixed_btn_w,
                                      row_y, stack_rows ? w : fixed_btn_w, btn_h,
                                      locale_get("move_down_button"),
                                      UI_BUTTON_STYLE_SECONDARY,
                                      i == app->habits.count - 1, &down_hover)) {
                if(habits_move(&app->habits, i, i + 1)) {
                    habits_persist_view_state(app);
                    app_auto_sync(app);
                }
                ui_scroll_page_end(page);
                return;
            }
            y += stack_rows ? flint_px(108) : flint_px(46);
            habits_draw_divider(x, w, y - flint_px(6));
        }

        ui_scroll_page_end(page);
    }
}

static int
habits_overview_done_today(InbeApp *app)
{
    int today = habits_today_index();
    int done = 0;

    if(app == NULL)
        return 0;
    for(int i = 0; i < app->habits.count; i++) {
        InbeHabit *habit = &app->habits.items[i];
        HabitLinkedContext ctx;
        HabitLinkedContext *linked_ctx = NULL;
        int completed;

        if(habit_is_linked(habit)) {
            memset(&ctx, 0, sizeof(ctx));
            habit_collect_linked_entries(habit, today, &ctx);
            linked_ctx = &ctx;
        }
        completed = habit_completed_day(habit, today);
        if(!completed && linked_ctx != NULL && habit_linked_has_day(linked_ctx, today))
            completed = 1;
        if(!completed && habit_counting_enabled(habit) &&
           habit_effective_day_count(habit, today, linked_ctx) > 0)
            completed = 1;
        if(completed)
            done++;
    }
    return done;
}

static int
habits_overview_cell_clicked(InbeApp *app, int x, int y, int w, int h, int disabled)
{
    Vector2 mouse;
    int inside;

    if(app == NULL)
        return 0;
    mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);
    inside = CheckCollisionPointRec(mouse, (Rectangle){(float)x, (float)y, (float)w, (float)h}) &&
             !ui_input_captures_click(mouse);
    if(!inside)
        return 0;
    if(disabled) {
        app->cursor_disabled = 1;
        return 0;
    }
    app->cursor_clickable = 1;
    return IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

static void
draw_habits_overview(InbeApp *app, int content_top)
{
    int content_bottom;
    int max_w = flint_px(CONTENT_MAX_W);
    int content_x;
    int content_w;
    int y;
    int font = flint_ui_font();
    int small = flint_ui_font_small();
    int btn_h = flint_px(34);
    int btn_gap = flint_px(8);
    int half_w;
    int add_hover = 0;
    int reorder_hover = 0;
    int done_count;
    char progress[64];
    int label_w;
    int cell_gap = flint_px(4);
    int cell;
    int grid_total_w;
    int overview_x;
    int grid_x;
    int grid_y;
    int today_index = habits_today_index();

    if(app == NULL)
        return;

    content_bottom = app_content_bottom_reserved(app);
    {
        FlintUIScrollPage page = ui_scroll_page_begin((FlintUIScrollPageSpec){
            .y = content_top + flint_px(4),
            .height = view_height - content_top - content_bottom - flint_px(4),
            .max_content_width = max_w,
            .scroll_offset = &app->habits.scroll,
            .content_height = habits_overview_content_height,
            .user_data = app
        });
        content_x = page.content_x;
        content_w = page.content_w;
        y = page.content_y + flint_px(12);

        done_count = habits_overview_done_today(app);
        snprintf(progress, sizeof(progress), locale_get("habits_done_count_label"),
                 done_count, app->habits.count);
        flint_text_draw(progress, content_x, y, font, flint_theme_get_text());
        y += flint_px(28);

        if(app->habits.count <= 0) {
            flint_text_draw(locale_get("habit_empty_title"), content_x, y, font,
                            flint_darken(flint_theme_get_text(), 35));
            y += flint_px(36);
            if(ui_draw_generic_button(content_x, y, content_w, btn_h,
                                      locale_get("habit_new_button"),
                                      UI_BUTTON_STYLE_SECONDARY,
                                      app->habits.count >= INBE_HABIT_MAX,
                                      &add_hover)) {
                habit_edit_begin_new(app);
                app->habits.screen_mode = HABITS_SCREEN_DETAIL;
                app->habits.tab = HABIT_TAB_EDIT;
                habits_persist_view_state(app);
            }
            ui_scroll_page_end(page);
            return;
        }

        label_w = habits_overview_label_width(content_w);
        cell = habits_overview_cell_size(content_w);
        grid_total_w = label_w + cell_gap * HABITS_OVERVIEW_COLUMNS +
                       cell * HABITS_OVERVIEW_COLUMNS;
        overview_x = content_x + (content_w - grid_total_w) / 2;
        grid_x = overview_x + label_w;
        grid_y = y;

        for(int day = 0; day < HABITS_OVERVIEW_DAY_COLUMNS; day++) {
            int day_index = habits_overview_day_index(day);
            int day_x = grid_x + day * (cell + cell_gap);
            char date_label[16];
            int text_w;
            habits_overview_date_label(day_index, date_label, sizeof(date_label));
            text_w = flint_text_measure(date_label, small);
            if(text_w > cell) {
                snprintf(date_label, sizeof(date_label), "%d", day_index % 100);
                text_w = flint_text_measure(date_label, small);
            }
            flint_text_draw(date_label, day_x + (cell - text_w) / 2,
                            grid_y, small, flint_darken(flint_theme_get_text(), 34));
        }
        y += flint_px(20);

        for(int i = 0; i < app->habits.count; i++) {
            InbeHabit *habit = &app->habits.items[i];
            int row_y = y;
            int swatch = flint_px(8);
            int label_text_w = label_w - swatch - flint_px(8);
            const char *label = habit->name;
            char short_name[8];
            char fitted_label[INBE_HABIT_NAME_SIZE + 4];
            Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);
            Rectangle row_bounds = {(float)(overview_x - flint_px(4)),
                                    (float)(row_y - flint_px(3)),
                                    (float)(grid_total_w + flint_px(8)),
                                    (float)(cell + flint_px(6))};
            Rectangle label_bounds = {(float)overview_x, (float)row_y,
                                      (float)label_w, (float)cell};
            int row_hover = CheckCollisionPointRec(mouse, row_bounds) &&
                            !ui_input_captures_click(mouse);
            int label_hover = CheckCollisionPointRec(mouse, label_bounds) &&
                              !ui_input_captures_click(mouse);

            if(label_w <= flint_px(62)) {
                inbe_text_short_label(habit->name, 3, 1, short_name, sizeof(short_name));
                label = short_name;
            } else if(flint_text_measure(label, small) > label_text_w) {
                inbe_text_fit_ellipsis(habit->name, fitted_label, sizeof(fitted_label),
                                       label_text_w, small);
                label = fitted_label;
            }

            if(row_hover)
                DrawRectangleRec(row_bounds, flint_darken(flint_theme_get_bg(), 10));
            DrawRectangle(overview_x, row_y + (cell - swatch) / 2, swatch, swatch,
                          habit->color);
            flint_text_draw(label, overview_x + swatch + flint_px(5),
                            flint_ui_text_y(label, row_y, cell, small), small,
                            flint_theme_get_text());
            if(label_hover) {
                ui_mark_clickable();
                if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                    habits_enter_detail(app, i);
            }

            for(int day = 0; day < HABITS_OVERVIEW_DAY_COLUMNS; day++) {
                int day_index = habits_overview_day_index(day);
                int day_x = grid_x + day * (cell + cell_gap);
                int future_day = day_index > today_index;
                int session_count = habit_day_session_count(habit, day_index);
                int has_linked_day = session_count > 0;
                int counting_enabled = habit_counting_enabled(habit);
                int minimum_count = session_count;
                int manual_count = habit_day_count(habit, day_index);
                int count = counting_enabled
                                ? (manual_count > session_count ? manual_count : session_count)
                                : 0;
                int completed = habit_completed_day(habit, day_index);
                int action = 0;
                Color fill;
                Color border = flint_darken(flint_theme_get_text(), 46);
                Rectangle cell_bounds = {(float)day_x, (float)row_y,
                                         (float)cell, (float)cell};
                int cell_active = CheckCollisionPointRec(mouse, cell_bounds) &&
                                  !ui_input_captures_click(mouse);
                int cell_hover = cell_active && !future_day && ui_hover_effects_enabled();

                if(!completed && !future_day && (has_linked_day || count > 0))
                    completed = 1;
                fill = completed ? habit->color : flint_darken(flint_theme_get_bg(), 7);
                if(day_index == today_index)
                    fill = completed ? flint_lighten(fill, 12) : flint_darken(flint_theme_get_bg(), 14);
                if(cell_hover)
                    fill = completed ? flint_lighten(fill, 22) : flint_theme_get_button_hover();

                DrawRectangle(day_x, row_y, cell, cell, fill);
                DrawRectangleLinesEx(cell_bounds, 1.0f, border);
                if(day == 0) {
                    DrawRectangle(day_x, row_y, cell, flint_px(3),
                                  flint_theme_get_text());
                }

                if(counting_enabled) {
                    action = habit_counter_day_action(app, i, day_index, day_x, row_y,
                                                      cell, cell, future_day,
                                                      !has_linked_day);
                    if(action == 0 && has_linked_day && !future_day &&
                       habits_overview_cell_clicked(app, day_x, row_y, cell, cell, 0))
                        habit_open_linked_edit_page(app, i, day_index);
                } else if(habits_overview_cell_clicked(app, day_x, row_y, cell, cell,
                                                       future_day)) {
                    if(has_linked_day) {
                        habit_open_linked_edit_page(app, i, day_index);
                    } else {
                        habit_toggle_day(&app->habits, i, day_index);
                        app_auto_sync(app);
                    }
                }
                if(action == 1 || action == -1) {
                    habit_apply_count_action(app, i, day_index, action, minimum_count);
                    app_auto_sync(app);
                }
                if(counting_enabled && count > 0 && !future_day && !has_linked_day) {
                    char count_label[16];
                    int count_w;
                    snprintf(count_label, sizeof(count_label), "%d", count);
                    count_w = flint_text_measure(count_label, FLINT_TEXT_12);
                    flint_text_draw(count_label, day_x + cell - count_w - flint_px(3),
                                    row_y + flint_px(3), FLINT_TEXT_12,
                                    flint_theme_get_text());
                }
                if(!future_day && has_linked_day)
                    draw_habit_link_dot(day_x, row_y, cell, habit->color);
            }
            {
                int detail_x = grid_x + HABITS_OVERVIEW_DAY_COLUMNS * (cell + cell_gap);
                Rectangle detail_bounds = {(float)detail_x, (float)row_y,
                                           (float)cell, (float)cell};
                int detail_active = CheckCollisionPointRec(mouse, detail_bounds) &&
                                    !ui_input_captures_click(mouse);
                int detail_hover = detail_active && ui_hover_effects_enabled();
                Color detail_fill = flint_darken(flint_theme_get_bg(), 7);
                Color detail_border = flint_darken(flint_theme_get_text(), 46);

                if(detail_hover) {
                    detail_fill = flint_darken(flint_theme_get_button_hover(), 8);
                    detail_border = flint_theme_get_text();
                }
                if(detail_active)
                    app->cursor_clickable = 1;
                DrawRectangle(detail_x, row_y, cell, cell, detail_fill);
                ui_draw_bevel(detail_x, row_y, cell, cell,
                              flint_lighten(detail_fill, detail_hover ? 34 : 24),
                              flint_darken(detail_fill, detail_hover ? 38 : 28));
                DrawRectangleLines(detail_x, row_y, cell, cell, detail_border);
                if(detail_active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    habits_enter_detail(app, i);
                    ui_scroll_page_end(page);
                    return;
                }
                {
                    const char *more = "...";
                    int more_w = flint_text_measure(more, FLINT_TEXT_16);
                    flint_text_draw(more, detail_x + (cell - more_w) / 2,
                                    flint_ui_text_y(more, row_y, cell, FLINT_TEXT_16),
                                    FLINT_TEXT_16, flint_theme_get_text());
                }
            }
            y += cell + flint_px(8);
        }

        y += flint_px(10);
        if(habits_overview_actions_stack(content_w)) {
            if(ui_draw_generic_button(content_x, y, content_w, btn_h,
                                      locale_get("habit_new_button"),
                                      UI_BUTTON_STYLE_SECONDARY,
                                      app->habits.count >= INBE_HABIT_MAX,
                                      &add_hover)) {
                habit_edit_begin_new(app);
                app->habits.screen_mode = HABITS_SCREEN_DETAIL;
                app->habits.tab = HABIT_TAB_EDIT;
                habits_persist_view_state(app);
            }
            y += btn_h + btn_gap;
            if(ui_draw_generic_button(content_x, y, content_w, btn_h,
                                      locale_get("habit_reorder_title"),
                                      UI_BUTTON_STYLE_SECONDARY,
                                      app->habits.count <= 0, &reorder_hover)) {
                habits_enter_reorder(app);
            }
        } else {
            half_w = (content_w - btn_gap) / 2;
            if(ui_draw_generic_button(content_x, y, half_w, btn_h,
                                      locale_get("habit_new_button"),
                                      UI_BUTTON_STYLE_SECONDARY,
                                      app->habits.count >= INBE_HABIT_MAX,
                                      &add_hover)) {
                habit_edit_begin_new(app);
                app->habits.screen_mode = HABITS_SCREEN_DETAIL;
                app->habits.tab = HABIT_TAB_EDIT;
                habits_persist_view_state(app);
            }
            if(ui_draw_generic_button(content_x + half_w + btn_gap, y, half_w, btn_h,
                                      locale_get("habit_reorder_title"),
                                      UI_BUTTON_STYLE_SECONDARY,
                                      app->habits.count <= 0, &reorder_hover)) {
                habits_enter_reorder(app);
            }
        }

        ui_scroll_page_end(page);
    }
}

static void
habits_screen_finish_first_run_guide(InbeApp *app)
{
    if(app == NULL)
        return;
    app->habits_guide_seen = 1;
    app->habits_guide_step = 0;
    save_settings(app);
}

int
habits_screen_first_run_guide_active(const InbeApp *app)
{
    InbeSyncAccount account;

    return app != NULL && !app->habits_guide_seen && !app->modal.active &&
           app->inbe.screen == InbeScreenHabits &&
           !sync_account_load(&account);
}

void
habits_screen_prepare_first_run_guide(InbeApp *app)
{
    int step;
    int meditation_index = 0;

    if(!habits_screen_first_run_guide_active(app))
        return;

    step = clampi(app->habits_guide_step, 0, HABITS_GUIDE_STEPS - 1);
    app->habits_guide_step = step;

    if(app->habit_edit.active)
        habit_edit_cancel(app);
    for(int i = 0; i < app->habits.count; i++) {
        if(strcmp(app->habits.items[i].id, "meditation") == 0) {
            meditation_index = i;
            break;
        }
    }

    if(step <= 1) {
        app->habits.screen_mode = HABITS_SCREEN_OVERVIEW;
        app->habits.scroll = 0;
    } else if(step == 2) {
        app->habits.screen_mode = HABITS_SCREEN_DETAIL;
        if(app->habits.count > 0)
            app->habits.selected = clampi(meditation_index, 0, app->habits.count - 1);
        app->habits.tab = HABIT_TAB_WEEKLY;
        app->habits.view_mode = HABIT_VIEW_WEEKLY;
        app->habits.scroll = 0;
    } else if(step == 3 || step == 5) {
        app->habits.screen_mode = HABITS_SCREEN_DETAIL;
        if(app->habits.count > 0)
            app->habits.selected = clampi(meditation_index, 0, app->habits.count - 1);
        app->habits.tab = HABIT_TAB_STATISTICS;
        app->habits.scroll = 0;
    } else if(step == 4) {
        app->habits.screen_mode = HABITS_SCREEN_DETAIL;
        if(app->habits.count > 0)
            app->habits.selected = clampi(meditation_index, 0, app->habits.count - 1);
        app->habits.tab = HABIT_TAB_EDIT;
        app->habits.scroll = 0;
    }
}

static Rectangle
habits_screen_tab_anchor(InbeApp *app, int first_tab, int tab_count)
{
    int tab_h = flint_px(HABITS_TAB_H);
    int tab_w = view_width / HABIT_TAB_COUNT;
    int x = first_tab * tab_w;
    int w = tab_count * tab_w;

    if(first_tab + tab_count >= HABIT_TAB_COUNT)
        w = view_width - x;
    if(w < 1)
        w = 1;

    return (Rectangle){(float)x, (float)habits_screen_selector_height(app),
                       (float)w, (float)tab_h};
}

static Rectangle
habits_overview_progress_anchor(InbeApp *app)
{
    int content_x;
    int content_w;
    int y = habits_screen_top_reserved(app) + flint_px(16);

    flint_centered_column(flint_px(CONTENT_MAX_W), flint_page_side_padding(),
                          &content_x, &content_w);
    return (Rectangle){(float)content_x, (float)y,
                       (float)content_w, (float)flint_px(34)};
}

static Rectangle
habits_overview_grid_anchor(InbeApp *app)
{
    int content_x;
    int content_w;
    int label_w;
    int cell;
    int cell_gap = flint_px(4);
    int grid_total_w;
    int rows = app != NULL && app->habits.count > 0 ? app->habits.count : 1;
    int y = habits_screen_top_reserved(app) + flint_px(16) + flint_px(34);

    flint_centered_column(flint_px(CONTENT_MAX_W), flint_page_side_padding(),
                          &content_x, &content_w);
    label_w = habits_overview_label_width(content_w);
    cell = habits_overview_cell_size(content_w);
    grid_total_w = label_w + cell_gap * HABITS_OVERVIEW_COLUMNS +
                   cell * HABITS_OVERVIEW_COLUMNS;

    return (Rectangle){
        (float)(content_x + (content_w - grid_total_w) / 2 - flint_px(4)),
        (float)(y - flint_px(4)),
        (float)(grid_total_w + flint_px(8)),
        (float)(flint_px(20) + rows * (cell + flint_px(8)))
    };
}

void
habits_screen_draw_first_run_guide(InbeApp *app)
{
    FlintUIGuideStep steps[HABITS_GUIDE_STEPS];
    FlintUIGuideResult result;

    if(!habits_screen_first_run_guide_active(app))
        return;

    steps[0] = (FlintUIGuideStep){
        habits_overview_progress_anchor(app),
        locale_get("habits_guide_overview_progress")
    };
    steps[1] = (FlintUIGuideStep){
        habits_overview_grid_anchor(app),
        locale_get("habits_guide_overview_grid")
    };
    steps[2] = (FlintUIGuideStep){
        habits_screen_tab_anchor(app, HABIT_TAB_WEEKLY, 2),
        locale_get("habits_guide_meditation_views")
    };
    steps[3] = (FlintUIGuideStep){
        habits_screen_tab_anchor(app, HABIT_TAB_STATISTICS, 1),
        locale_get("habits_guide_meditation_statistics")
    };
    steps[4] = (FlintUIGuideStep){
        habits_screen_tab_anchor(app, HABIT_TAB_EDIT, 1),
        locale_get("habits_guide_meditation_edit")
    };
    steps[5] = (FlintUIGuideStep){
        habits_screen_tab_anchor(app, HABIT_TAB_STATISTICS, 1),
        locale_get("habits_guide_meditation_session_data")
    };

    result = flint_ui_draw_guide_overlay((FlintUIGuideOverlay){
        .steps = steps,
        .count = HABITS_GUIDE_STEPS,
        .step = &app->habits_guide_step,
        .view_width = view_width,
        .view_height = view_height,
        .reserved_top = habits_screen_top_reserved(app),
        .reserved_bottom = ui_bottom_nav_height(),
        .max_width = flint_px(300),
        .paragraph_font = flint_ui_font_small(),
        .close_icon = app->icons[UI_ICON_TYPE_X],
        .back_icon = app->icons[UI_ICON_TYPE_BACKWARD],
        .next_icon = app->icons[UI_ICON_TYPE_FORWARD],
        .done_icon = app->icons[UI_ICON_TYPE_CHECK]
    });
    if(result.closed || result.finished)
        habits_screen_finish_first_run_guide(app);
}

static void
draw_habit_completion_underline(int x, int y, int w, int h, Color color)
{
    DrawRectangle(x + flint_px(4), y + h - flint_px(6),
                  w - flint_px(8), flint_px(3), color);
}

static void
draw_habit_link_dot(int x, int y, int w, Color color)
{
    DrawCircle(x + w - flint_px(8), y + flint_px(8),
               flint_px(3), color);
}

static int
habit_calendar_day_cell(InbeApp *app, int x, int y, int w, int h,
                        const char *label, int completed, int disabled, int current_day)
{
    Vector2 mouse_world = app != NULL
                              ? GetScreenToWorld2D(GetMousePosition(), app->camera)
                              : GetMousePosition();
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int inside = !disabled && CheckCollisionPointRec(mouse_world, bounds) &&
                 !ui_input_captures_click(mouse_world);
    int hovered = inside && ui_hover_effects_enabled();
    Color fill = completed ? flint_theme_get_button() : flint_darken(flint_theme_get_bg(), 10);
    Color text = disabled ? flint_darken(flint_theme_get_text(), 35) : flint_theme_get_text();
    int font = FLINT_TEXT_16;
    int text_w;

    if(hovered)
        fill = flint_theme_get_button_hover();
    else if(current_day)
        fill = completed ? flint_lighten(fill, 12) : flint_darken(flint_theme_get_bg(), 14);
    if(!disabled && completed)
        text = habit_text_color_for_background(fill);

    DrawRectangle(x, y, w, h, fill);
    ui_draw_bevel(x, y, w, h,
                  flint_lighten(fill, hovered ? 36 : 28),
                  flint_darken(fill, hovered ? 42 : 34));
    text_w = flint_text_measure(label, font);
    flint_text_draw(label, x + (w - text_w) / 2,
                    flint_ui_text_y(label, y, h, font), font, text);

    if(inside && app != NULL)
        app->cursor_clickable = 1;
    return inside && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

static int
habit_weekly_row_height(void)
{
    return flint_px(52);
}

static Color
habit_text_color_for_background(Color background)
{
    int luma = background.r * 299 + background.g * 587 + background.b * 114;

    return luma >= 128000 ? BLACK : WHITE;
}

static void
habit_weekly_draw_text_line(const char *text, int x, int y, int w, int h,
                            int text_size, Color color)
{
    int font_size;
    const char *draw_text = text;
    char fitted[128];

    if(text == NULL || text[0] == '\0' || w <= 0 || h <= 0)
        return;
    font_size = text_size;
    while(font_size > FLINT_TEXT_8 && flint_text_measure(text, font_size) > w)
        font_size -= flint_px(1);
    if(flint_text_measure(text, font_size) > w) {
        inbe_text_fit_ellipsis(text, fitted, sizeof(fitted), w, font_size);
        draw_text = fitted;
    }
    flint_text_draw(draw_text, x + (w - flint_text_measure(draw_text, font_size)) / 2,
                    flint_text_y(draw_text, y, h, font_size), font_size, color);
}

static int
habit_weekly_summary(const HabitLinkedContext *ctx, int day_index,
                     char *primary, size_t primary_size,
                     char *secondary, size_t secondary_size)
{
    int count = 0;
    int first_activity = -1;
    int mixed = 0;

    if(primary != NULL && primary_size > 0)
        primary[0] = '\0';
    if(secondary != NULL && secondary_size > 0)
        secondary[0] = '\0';
    if(ctx == NULL)
        return 0;

    for(int i = 0; i < ctx->count; i++) {
        int entry_day = ctx->entries[i].year * 10000 +
                        ctx->entries[i].month * 100 +
                        ctx->entries[i].day;
        if(entry_day != day_index)
            continue;
        if(first_activity < 0)
            first_activity = ctx->entries[i].activity;
        else if(first_activity != ctx->entries[i].activity)
            mixed = 1;
        count++;
    }

    if(count <= 0)
        return 0;

    if(primary != NULL && primary_size > 0) {
        snprintf(primary, primary_size, "%s",
                 mixed ? locale_get("habit_mixed_practice_label")
                       : practice_activity_label(first_activity));
    }
    if(secondary != NULL && secondary_size > 0)
        locale_format(secondary, secondary_size,
                      count == 1 ? "session_count_singular"
                                 : "session_count_plural",
                      count);
    return count;
}

static int
habit_weekly_summary_button(InbeApp *app, int x, int y, int w, int h, int completed, int disabled,
                            const char *primary, const char *secondary)
{
    Vector2 mouse_world;
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int active;
    int hovered;
    Color fill = completed ? flint_theme_get_button() : flint_darken(flint_theme_get_bg(), 10);
    Color text = disabled ? flint_darken(flint_theme_get_text(), 35) : flint_theme_get_text();
    int pad = flint_px(9);
    int text_x;
    int text_w;
    const char *line1 = primary != NULL ? primary : "";
    const char *line2 = secondary != NULL ? secondary : "";

    if(app == NULL)
        return 0;
    mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    active = CheckCollisionPointRec(mouse_world, bounds) &&
             !ui_input_captures_click(mouse_world);
    hovered = active && ui_hover_effects_enabled();
    if(active) {
        if(disabled)
            app->cursor_disabled = 1;
        else
            app->cursor_clickable = 1;
    }
    active = active && !disabled;
    hovered = hovered && !disabled;

    if(hovered)
        fill = flint_theme_get_button_hover();
    if(!disabled && completed)
        text = habit_text_color_for_background(fill);
    DrawRectangle(x, y, w, h, fill);
    ui_draw_bevel(x, y, w, h, flint_lighten(fill, 36), flint_darken(fill, 42));

    text_x = x + pad;
    text_w = w - pad * 2;

    if(line1[0] != '\0') {
        if(line2[0] != '\0') {
            int line1_h = flint_px(24);
            int line2_h = flint_px(18);
            int underline_space = completed ? flint_px(5) : 0;
            int block_h = line1_h + line2_h;
            int block_y = y + (h - block_h - underline_space) / 2;
            habit_weekly_draw_text_line(line1, text_x, block_y, text_w, line1_h,
                                        FLINT_TEXT_16, text);
            habit_weekly_draw_text_line(line2, text_x, block_y + line1_h, text_w, line2_h,
                                        FLINT_TEXT_12, flint_darken(text, 16));
        } else {
            habit_weekly_draw_text_line(line1, text_x, y + flint_px(3),
                                        text_w, h - flint_px(6),
                                        FLINT_TEXT_16, text);
        }
    }

    if(active)
        return IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    return 0;
}

static int
habit_weekly_visible_days(InbeHabits *habits)
{
    if(habits == NULL)
        return HABIT_WEEKLY_INITIAL_DAYS;
    if(habits->weekly_days < HABIT_WEEKLY_INITIAL_DAYS)
        habits->weekly_days = HABIT_WEEKLY_INITIAL_DAYS;
    if(habits->weekly_days > HABIT_WEEKLY_MAX_DAYS)
        habits->weekly_days = HABIT_WEEKLY_MAX_DAYS;
    return habits->weekly_days;
}

static int
habit_weekly_content_height(int visible_days)
{
    int row_h = habit_weekly_row_height();
    int row_gap = flint_px(6);
    int load_h = flint_px(38);

    if(visible_days < HABIT_WEEKLY_INITIAL_DAYS)
        visible_days = HABIT_WEEKLY_INITIAL_DAYS;
    return flint_px(26) +
           row_h * visible_days +
           row_gap * (visible_days - 1) +
           flint_px(14) + load_h +
           flint_px(16);
}

static void
draw_habits_weekly_view(InbeApp *app, InbeHabit *active, int selected,
                        HabitLinkedContext *linked_ctx,
                        int content_x, int content_w, int y, int visible_days)
{
    time_t now = time(NULL);
    struct tm day_tm;
    int today_index = habits_today_index();
    int label_w = flint_px(88);
    int gap = flint_px(8);
    int button_x;
    int button_w;
    int row_h = habit_weekly_row_height();
    int row_gap = flint_px(6);
    int day_font = FLINT_TEXT_16;
    int date_font = FLINT_TEXT_12;
    int load_h = flint_px(38);
    int load_hover = 0;

    if(localtime(&now) != NULL)
        day_tm = *localtime(&now);
    else
        memset(&day_tm, 0, sizeof(day_tm));
    day_tm.tm_hour = 12;
    day_tm.tm_min = 0;
    day_tm.tm_sec = 0;
    mktime(&day_tm);

    if(label_w > content_w / 2)
        label_w = content_w / 2;
    button_x = content_x + label_w + gap;
    button_w = content_w - label_w - gap;
    if(button_w < flint_px(80)) {
        button_w = content_w;
        label_w = 0;
        button_x = content_x;
    }

    if(visible_days < HABIT_WEEKLY_INITIAL_DAYS)
        visible_days = HABIT_WEEKLY_INITIAL_DAYS;
    if(visible_days > HABIT_WEEKLY_MAX_DAYS)
        visible_days = HABIT_WEEKLY_MAX_DAYS;

    for(int i = 0; i < visible_days; i++) {
        struct tm row_tm = day_tm;
        char day_label[16];
        char date_label[32];
        int day_index;
        int completed;
        int has_linked_day;
        int future_day;
        int count = 0;
        int action = 0;
        int counting_enabled;
        int minimum_count;
        char primary[64] = "";
        char secondary[64] = "";
        int session_count;

        row_tm.tm_mday -= i;
        mktime(&row_tm);
        day_index = habit_tm_date_index(&row_tm);
        future_day = day_index > today_index;
        has_linked_day = linked_ctx != NULL && habit_linked_has_day(linked_ctx, day_index);
        counting_enabled = habit_counting_enabled(active);
        minimum_count = habit_linked_session_count_for_day(linked_ctx, day_index);
        session_count = habit_weekly_summary(linked_ctx, day_index,
                                             primary, sizeof(primary),
                                             secondary, sizeof(secondary));
        if(counting_enabled)
            count = habit_effective_day_count(active, day_index, linked_ctx);
        completed = habit_completed_day(active, day_index);
        if(!completed && !future_day && (has_linked_day || count > 0))
            completed = 1;
        if(counting_enabled && count > 0 && session_count > 0 &&
           secondary[0] != '\0') {
            size_t len = strlen(secondary);
            snprintf(secondary + len, sizeof(secondary) - len,
                     " / total %d", count);
        }
        if(session_count <= 0 && completed) {
            snprintf(primary, sizeof(primary), "Completed");
            secondary[0] = '\0';
        }

        strftime(day_label, sizeof(day_label), "%a", &row_tm);
        for(char *p = day_label; *p != '\0'; p++) {
            if(*p >= 'a' && *p <= 'z')
                *p = (char)(*p - 'a' + 'A');
        }
        snprintf(date_label, sizeof(date_label), "%02d/%02d/%02d",
                 row_tm.tm_mday, row_tm.tm_mon + 1, (row_tm.tm_year + 1900) % 100);

        if(label_w > 0) {
            DrawRectangle(content_x, y, label_w, row_h, flint_darken(flint_theme_get_bg(), 5));
            flint_text_draw(day_label, content_x + flint_px(8), y + flint_px(7),
                            day_font, flint_theme_get_text());
            flint_text_draw(date_label, content_x + flint_px(8), y + flint_px(33),
                            date_font, flint_darken(flint_theme_get_text(), 18));
        }

        if(counting_enabled) {
            (void)habit_weekly_summary_button(app, button_x, y, button_w, row_h,
                                              completed, future_day, primary, secondary);
            action = habit_counter_day_action(app, selected, day_index,
                                              button_x, y, button_w, row_h,
                                              future_day, !has_linked_day);
            if(action == 0 && has_linked_day && !future_day) {
                Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
                if(CheckCollisionPointRec(mouse_world,
                                          (Rectangle){(float)button_x, (float)y, (float)button_w, (float)row_h}) &&
                   !ui_input_captures_click(mouse_world) &&
                   IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                    habit_open_linked_edit_page(app, selected, day_index);
            }
        } else if(habit_weekly_summary_button(app, button_x, y, button_w, row_h, completed, future_day,
                                              primary, secondary)) {
            if(has_linked_day) {
                habit_open_linked_edit_page(app, selected, day_index);
            } else {
                habit_toggle_day(&app->habits, selected, day_index);
                active = &app->habits.items[selected];
                app_auto_sync(app);
            }
        }
        if(action == 1 || action == -1) {
            habit_apply_count_action(app, selected, day_index, action, minimum_count);
            active = &app->habits.items[selected];
            count = habit_effective_day_count(active, day_index, linked_ctx);
            completed = count > 0;
            app_auto_sync(app);
        }
        if(completed && !future_day)
            draw_habit_completion_underline(button_x, y, button_w, row_h, active->color);
        if(counting_enabled && count > 0 && !future_day && !has_linked_day) {
            char count_label[16];
            int count_font = FLINT_TEXT_12;
            int count_w;
            snprintf(count_label, sizeof(count_label), "%d", count);
            count_w = flint_text_measure(count_label, count_font);
            flint_text_draw(count_label,
                            button_x + button_w - count_w - flint_px(6),
                            y + flint_px(5),
                            count_font, flint_theme_get_text());
        }
        if(!future_day && has_linked_day)
            draw_habit_link_dot(button_x, y, button_w, active->color);

        y += row_h + row_gap;
    }

    y += flint_px(8);
    if(ui_draw_generic_button(content_x, y, content_w, load_h, locale_get("habit_load_more_button"),
                              UI_BUTTON_STYLE_SECONDARY,
                              app->habits.weekly_days >= HABIT_WEEKLY_MAX_DAYS,
                              &load_hover)) {
        app->habits.weekly_days += HABIT_WEEKLY_LOAD_DAYS;
        if(app->habits.weekly_days > HABIT_WEEKLY_MAX_DAYS)
            app->habits.weekly_days = HABIT_WEEKLY_MAX_DAYS;
    }
}

typedef struct HabitsScrollPageContext {
    int view_mode;
    int weekly_days;
    int month_h;
    int grid_gap;
} HabitsScrollPageContext;

static int
habits_scroll_page_content_height(int content_w, void *user_data)
{
    HabitsScrollPageContext *ctx = user_data;

    if(ctx->view_mode == HABIT_VIEW_WEEKLY)
        return habit_weekly_content_height(ctx->weekly_days);
    {
        int planned_cell_w = (content_w - ctx->grid_gap * 6) / 7;
        if(planned_cell_w < flint_px(28))
            planned_cell_w = flint_px(28);
        return flint_px(8) + ctx->month_h + flint_px(12) +
               planned_cell_w * 6 + ctx->grid_gap * 5 + flint_px(16);
    }
}

void
draw_habits_screen(InbeApp *app)
{
    int content_top;
    int content_bottom;
    int content_x;
    int content_w;
    int y;
    int viewport_h;
    int hover = 0;
    int side_padding = flint_page_side_padding();
    int max_w = flint_px(CONTENT_MAX_W);
    int month_h = flint_px(42);
    int grid_gap = flint_px(4);
    int selected;
    InbeHabit *active;
    time_t now;
    struct tm month;
    struct tm next_month;
    char month_label[64];
    int year;
    int mon;
    int first_wday;
    int days_in_month;
    int today_index = habits_today_index();
    int cell_w;
    int cell_h;
    int grid_x;
    int grid_y;
    HabitLinkedContext *linked_ctx = NULL;
    int active_is_linked;
    int forward_disabled;
    int scroll_y;
    int scroll_h;
    int weekly_days = HABIT_WEEKLY_INITIAL_DAYS;

    if(app == NULL)
        return;

    if(app->habits.tab < HABIT_TAB_WEEKLY || app->habits.tab >= HABIT_TAB_COUNT) {
        app->habits.tab = app->habits.view_mode == HABIT_VIEW_WEEKLY
                              ? HABIT_TAB_WEEKLY
                              : HABIT_TAB_MONTHLY;
    }
    if(app->habits.tab == HABIT_TAB_WEEKLY)
        app->habits.view_mode = HABIT_VIEW_WEEKLY;
    else if(app->habits.tab == HABIT_TAB_MONTHLY)
        app->habits.view_mode = HABIT_VIEW_CALENDAR;

    content_top = habits_screen_top_reserved(app);
    content_bottom = app_content_bottom_reserved(app);
    y = content_top + flint_px(8);
    viewport_h = view_height - content_top - content_bottom;

    flint_centered_column(max_w, side_padding, &content_x, &content_w);

    if(app->habits.screen_mode == HABITS_SCREEN_REORDER) {
        draw_habits_reorder(app, content_top);
        draw_habits_top_bar(app, 0);
        if(app->inbe.screen == InbeScreenHabits)
            draw_habits_top_bar(app, 1);
        return;
    }

    if(app->habits.screen_mode != HABITS_SCREEN_DETAIL) {
        draw_habits_overview(app, content_top);
        draw_habits_top_bar(app, 0);
        if(app->inbe.screen == InbeScreenHabits)
            draw_habits_top_bar(app, 1);
        return;
    }

    if(app->habits.count <= 0) {
        const char *empty_text = locale_get("habit_empty_title");
        const char *create_text = locale_get("habit_create_button");
        int empty_font = FLINT_TEXT_16;
        int button_w = content_w < flint_px(240) ? content_w : flint_px(240);
        int button_h = flint_px(42);
        int empty_y = content_top + viewport_h / 2 - flint_px(46);
        int empty_w = flint_text_measure(empty_text, empty_font);
        int hover_empty_create = 0;

        flint_clip_begin((int)app->camera.offset.x,
                         (int)(app->camera.offset.y + content_top * app->camera.zoom),
                         (int)(view_width * app->camera.zoom),
                         (int)(viewport_h * app->camera.zoom));
        flint_text_draw(empty_text, content_x + (content_w - empty_w) / 2,
                        empty_y, empty_font, flint_theme_get_text());
        if(ui_draw_generic_button(content_x + (content_w - button_w) / 2,
                                  empty_y + flint_px(38), button_w, button_h,
                                  create_text, UI_BUTTON_STYLE_PRIMARY,
                                  0, &hover_empty_create)) {
            habit_edit_begin_new(app);
        }
        flint_clip_end();
        draw_habits_top_bar(app, 0);
        if(app->inbe.screen == InbeScreenHabits)
            draw_habits_top_bar(app, 1);
        return;
    }
    if(app->habits.selected < 0 || app->habits.selected >= app->habits.count)
        app->habits.selected = 0;

    selected = app->habits.selected;
    active = &app->habits.items[selected];
    active_is_linked = habit_is_linked(active);
    if(app->habits.view_mode != HABIT_VIEW_WEEKLY)
        app->habits.view_mode = HABIT_VIEW_CALENDAR;
    weekly_days = habit_weekly_visible_days(&app->habits);

    if(app->habits.tab == HABIT_TAB_STATISTICS) {
        draw_statistics_content(app, content_top);
        draw_habits_top_bar(app, 0);
        if(app->inbe.screen == InbeScreenHabits)
            draw_habits_top_bar(app, 1);
        return;
    }

    if(app->habits.tab == HABIT_TAB_EDIT) {
        if(!app->habit_edit.active ||
           (!app->habit_edit.is_new && app->habit_edit.index != app->habits.selected))
            habit_edit_begin(app, app->habits.selected);
        draw_habit_edit_screen(app);
        draw_habits_top_bar(app, 0);
        if(app->inbe.screen == InbeScreenHabits)
            draw_habits_top_bar(app, 1);
        return;
    }

    if(app->habits.month_offset > 0)
        app->habits.month_offset = 0;

    now = time(NULL);
    {
        struct tm *local = localtime(&now);
        if(local != NULL)
            month = *local;
        else
            memset(&month, 0, sizeof(month));
    }
    month.tm_mday = 1;
    month.tm_hour = 12;
    month.tm_min = 0;
    month.tm_sec = 0;
    month.tm_mon += app->habits.month_offset;
    mktime(&month);

    year = month.tm_year + 1900;
    mon = month.tm_mon + 1;
    first_wday = month.tm_wday;
    next_month = month;
    next_month.tm_mon++;
    next_month.tm_mday = 0;
    mktime(&next_month);
    days_in_month = next_month.tm_mday;
    strftime(month_label, sizeof(month_label), "%B %Y", &month);

    if(active_is_linked) {
        linked_ctx = calloc(1, sizeof(*linked_ctx));
        if(linked_ctx != NULL)
            habit_collect_linked_entries(active, 0, linked_ctx);
    }

    scroll_y = content_top + flint_px(4);
    scroll_h = viewport_h - flint_px(4);
    if(scroll_h < 0)
        scroll_h = 0;

    {
        HabitsScrollPageContext page_ctx = {
            app->habits.view_mode,
            weekly_days,
            month_h,
            grid_gap
        };
        FlintUIScrollPage page = ui_scroll_page_begin((FlintUIScrollPageSpec){
            .y = scroll_y,
            .height = scroll_h,
            .max_content_width = max_w,
            .scroll_offset = &app->habits.scroll,
            .content_height = habits_scroll_page_content_height,
            .user_data = &page_ctx
        });
        content_x = page.content_x;
        content_w = page.content_w;
        y = page.content_y + flint_px(8);

        if(app->habits.view_mode == HABIT_VIEW_WEEKLY) {
            draw_habits_weekly_view(app, active, selected, linked_ctx,
                                    content_x, content_w, y, weekly_days);
            ui_scroll_page_end(page);
            free(linked_ctx);
            draw_habits_top_bar(app, 0);
            if(app->inbe.screen == InbeScreenHabits)
                draw_habits_top_bar(app, 1);
            return;
        }

        forward_disabled = app->habits.month_offset >= 0;
        if(ui_draw_generic_button(content_x, y, flint_px(44), month_h, "<",
                                  UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
            app->habits.month_offset--;
        }
        if(ui_draw_generic_button(content_x + content_w - flint_px(44), y,
                                     flint_px(44), month_h, ">",
                                     UI_BUTTON_STYLE_SECONDARY,
                                     forward_disabled, &hover)) {
            app->habits.month_offset++;
        }
        flint_text_draw(month_label,
                        content_x + (content_w - flint_text_measure(month_label, FLINT_TEXT_24)) / 2,
                        flint_ui_text_y(month_label, y, month_h, FLINT_TEXT_24),
                        FLINT_TEXT_24, flint_theme_get_text());

        grid_x = content_x;
        grid_y = y + month_h + flint_px(12);
        cell_w = (content_w - grid_gap * 6) / 7;
        cell_h = cell_w;
        if(cell_h < flint_px(28))
            cell_h = flint_px(28);

    for(int row = 0; row < 6; row++) {
        for(int col = 0; col < 7; col++) {
            int slot = row * 7 + col;
            int day = slot - first_wday + 1;
            int cell_x = grid_x + col * (cell_w + grid_gap);
            int cell_y = grid_y + row * (cell_h + grid_gap);
            char day_label[16];
            int day_index;
            int completed;
            int future_day;
            int has_linked_day;
            int count = 0;
            int action = 0;
            int counting_enabled;
            int minimum_count;

            if(day < 1 || day > days_in_month) {
                DrawRectangle(cell_x, cell_y, cell_w, cell_h, flint_darken(flint_theme_get_bg(), 5));
                continue;
            }

            snprintf(day_label, sizeof(day_label), "%d", day);
            day_index = year * 10000 + mon * 100 + day;
            future_day = day_index > today_index;
            has_linked_day = linked_ctx != NULL && habit_linked_has_day(linked_ctx, day_index);
            counting_enabled = habit_counting_enabled(active);
            minimum_count = habit_linked_session_count_for_day(linked_ctx, day_index);
            if(counting_enabled)
                count = habit_effective_day_count(active, day_index, linked_ctx);
            completed = habit_completed_day(active, day_index);
            if(!completed && !future_day && (has_linked_day || count > 0))
                completed = 1;
            if(counting_enabled) {
                (void)habit_calendar_day_cell(app, cell_x, cell_y, cell_w, cell_h,
                                              day_label, completed, future_day,
                                              day_index == today_index);
                action = habit_counter_day_action(app, selected, day_index,
                                                  cell_x, cell_y, cell_w, cell_h,
                                                  future_day, !has_linked_day);
                if(action == 0 && has_linked_day && !future_day) {
                    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
                    if(CheckCollisionPointRec(mouse_world,
                                              (Rectangle){(float)cell_x, (float)cell_y,
                                                          (float)cell_w, (float)cell_h}) &&
                       !ui_input_captures_click(mouse_world) &&
                       IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                        habit_open_linked_edit_page(app, selected, day_index);
                }
            } else if(habit_calendar_day_cell(app, cell_x, cell_y, cell_w, cell_h,
                                              day_label, completed, future_day,
                                              day_index == today_index)) {
                if(has_linked_day) {
                    habit_open_linked_edit_page(app, selected, day_index);
                } else {
                    habit_toggle_day(&app->habits, selected, day_index);
                    active = &app->habits.items[selected];
                    app_auto_sync(app);
                }
            }
            if(action == 1 || action == -1) {
                habit_apply_count_action(app, selected, day_index, action, minimum_count);
                active = &app->habits.items[selected];
                count = habit_effective_day_count(active, day_index, linked_ctx);
                completed = count > 0;
                app_auto_sync(app);
            }
            if(completed && !future_day) {
                draw_habit_completion_underline(cell_x, cell_y, cell_w, cell_h, active->color);
            }
            if(counting_enabled && count > 0 && !future_day && !has_linked_day) {
                char count_label[16];
                int count_font = FLINT_TEXT_12;
                int count_w;
                snprintf(count_label, sizeof(count_label), "%d", count);
                count_w = flint_text_measure(count_label, count_font);
                flint_text_draw(count_label,
                                cell_x + cell_w - count_w - flint_px(4),
                                cell_y + flint_px(4),
                                count_font, flint_theme_get_text());
            }
            if(!future_day && has_linked_day) {
                draw_habit_link_dot(cell_x, cell_y, cell_w, active->color);
            }
        }
    }

        ui_scroll_page_end(page);
    }
    free(linked_ctx);
    draw_habits_top_bar(app, 0);
    if(app->inbe.screen == InbeScreenHabits)
        draw_habits_top_bar(app, 1);
}
