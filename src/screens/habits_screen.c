#include "habits_screen.h"

#include "practice_screen.h"
#include "data.h"
#include "storage.h"
#include "app.h"
#include "theme.h"
#include "flint_runtime_assets.h"
#include "locale.h"
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

/* External declarations from app.c */
extern int view_width;
extern int view_height;

enum {
    HABIT_WEEKLY_MONTH_DAYS = 31,
    HABIT_WEEKLY_INITIAL_DAYS = HABIT_WEEKLY_MONTH_DAYS,
    HABIT_WEEKLY_LOAD_DAYS = HABIT_WEEKLY_MONTH_DAYS,
    HABIT_WEEKLY_MAX_DAYS = 36500,
    HABIT_COUNTER_LONG_PRESS_FRAMES = 30
};

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
    if(habit == NULL || habit->days == NULL)
        return -1;
    for(int i = 0; i < habit->day_count; i++) {
        if(habit->days[i].day_index == day_index)
            return i;
    }
    return -1;
}

static int
habit_counting_enabled(const InbeHabit *habit)
{
    return habit != NULL &&
           (habit->counter_enabled ||
            (habit->sync_mode == INBE_HABIT_SYNC_ACTIVITIES &&
             habit->sync_activity != 0));
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

    if(disabled)
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
inbe_habit_reserve_days(InbeHabit *habit, int capacity)
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
inbe_habits_free(InbeHabits *habits)
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
    return index >= 0 &&
           (habit->days[index].completed || habit->days[index].count > 0);
}

int
inbe_habit_day_count(const InbeHabit *habit, int day_index)
{
    int index = habit_find_day(habit, day_index);
    if(index < 0)
        return 0;
    if(habit->days[index].count > 0)
        return habit->days[index].count;
    return habit->days[index].completed ? 1 : 0;
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
            if(habit->days[d].day_index > 0 || habit->days[d].completed ||
               habit->days[d].count > 0)
                cleared++;
        }
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

    inbe_habits_free(habits);
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
    habit->counter_enabled = sync_activity != 0;
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
    habit->counter_enabled = 1;
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
        habit->days[existing_index].count = completed ? 1 : 0;
    } else if(completed && inbe_habit_reserve_days(habit, habit->day_count + 1)) {
        habit->days[habit->day_count].day_index = day_index;
        habit->days[habit->day_count].completed = 1;
        habit->days[habit->day_count].count = 1;
        habit->day_count++;
    }
    habits->selected = index;
    inbe_habits_save(habits);
}

void
inbe_habit_set_day_count(InbeHabits *habits, int index, int day_index, int count)
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
    } else if(count > 0 && inbe_habit_reserve_days(habit, habit->day_count + 1)) {
        habit->days[habit->day_count].day_index = day_index;
        habit->days[habit->day_count].count = count;
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
        int completed = !(habit->days[existing_index].completed ||
                          habit->days[existing_index].count > 0);
        habit->days[existing_index].completed = completed;
        habit->days[existing_index].count = completed ? 1 : 0;
    } else if(inbe_habit_reserve_days(habit, habit->day_count + 1)) {
        habit->days[habit->day_count].day_index = day_index;
        habit->days[habit->day_count].completed = 1;
        habit->days[habit->day_count].count = 1;
        habit->day_count++;
    }
    habits->selected = index;
    inbe_habits_save(habits);
}

void
inbe_habit_increment_day(InbeHabits *habits, int index, int day_index, int delta)
{
    int count;

    if(habits == NULL || index < 0 || index >= habits->count || day_index <= 0)
        return;
    count = inbe_habit_day_count(&habits->items[index], day_index) + delta;
    inbe_habit_set_day_count(habits, index, day_index, count);
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

static void
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
    app->habit_edit_focused = 0;
    app->habit_edit_color = (Color){99, 196, 165, 255};
    app->habit_edit_sync_mode = INBE_HABIT_SYNC_NONE;
    app->habit_edit_sync_activity = 0;
    app->habit_edit_counter_enabled = 0;
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
    app->habit_edit_focused = 0;
    app->habit_edit_color = app->habits.items[index].color;
    app->habit_edit_sync_mode = app->habits.items[index].sync_mode;
    app->habit_edit_sync_activity = app->habits.items[index].sync_activity;
    app->habit_edit_counter_enabled = habit_counting_enabled(&app->habits.items[index]);
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
    app->habit_edit_focused = 0;
    app->habit_edit_text[0] = '\0';
    app->habit_edit_counter_enabled = 0;
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
        if(app->habit_edit_sync_activity != 0)
            app->habit_edit_counter_enabled = 1;
        if(app->habit_edit_is_new) {
            int created = inbe_habits_add_custom(&app->habits, text, app->habit_edit_color,
                                                 app->habit_edit_sync_mode,
                                                 app->habit_edit_sync_activity);
            if(created >= 0 && created < app->habits.count) {
                app->habits.items[created].counter_enabled = app->habit_edit_counter_enabled != 0;
                inbe_habits_save(&app->habits);
            }
        } else {
            snprintf(app->habits.items[index].name,
                     sizeof(app->habits.items[index].name), "%s", text);
            app->habits.items[index].color = app->habit_edit_color;
            app->habits.items[index].color.a = 255;
            app->habits.items[index].sync_mode = app->habit_edit_sync_mode;
            app->habits.items[index].sync_activity = app->habit_edit_sync_activity;
            app->habits.items[index].counter_enabled = app->habit_edit_counter_enabled != 0;
            app->habits.selected = index;
            inbe_habits_save(&app->habits);
        }
        inbe_app_auto_sync(app);
    }
    habit_edit_cancel(app);
    app->inbe.screen = InbeScreenHabits;
}

static void
habit_edit_clamp_cursor(InbeApp *app)
{
    int len;

    if(app == NULL)
        return;

    len = (int)strlen(app->habit_edit_text);
    if(app->habit_edit_cursor < 0)
        app->habit_edit_cursor = 0;
    if(app->habit_edit_cursor > len)
        app->habit_edit_cursor = len;
}

static void
habit_edit_handle_keyboard(InbeApp *app)
{
    if(app == NULL || !app->habit_edit_active)
        return;
    if(IsKeyPressed(KEY_ESCAPE)) {
        habit_edit_cancel(app);
        return;
    }
}

static void
draw_habit_edit_field(InbeApp *app, int x, int y, int w, int h, int font)
{
    FlintUITextInputStyle style = {
        .background = flint_darken(theme_get_bg(), 4),
        .border = theme_get_button(),
        .focus_border = theme_get_button_hover(),
        .text = theme_get_text(),
        .cursor = theme_get_text(),
        .radius = 0.08f,
        .padding_x = flint_px(10)
    };

    if(app == NULL)
        return;

    flint_ui_text_field((FlintUITextField){
        .bounds = {(float)x, (float)y, (float)w, (float)h},
        .text = app->habit_edit_text,
        .text_size = sizeof(app->habit_edit_text),
        .cursor_position = &app->habit_edit_cursor,
        .focused = &app->habit_edit_focused,
        .max_codepoints = INBE_HABIT_NAME_SIZE - 1,
        .font = font,
        .style = style
    });
    habit_edit_clamp_cursor(app);
}

static int
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

static void
habit_refresh_detail_day_completion(InbeApp *app)
{
    HabitLinkedContext ctx;
    InbeHabit *habit;
    int index;
    int day;
    int selected;

    if(app == NULL)
        return;
    index = app->habit_detail_index;
    day = app->habit_detail_day;
    if(index < 0 || index >= app->habits.count || day <= 0)
        return;

    habit = &app->habits.items[index];
    habit_collect_linked_entries(habit, day, &ctx);
    if(ctx.count > 0 || !inbe_habit_completed_day(habit, day))
        return;

    selected = app->habits.selected;
    inbe_habit_set_day(&app->habits, index, day, 0);
    app->habits.selected = selected;
    inbe_habits_save(&app->habits);
    inbe_app_auto_sync(app);
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

static int
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

static int
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

static int
habit_effective_day_count(const InbeHabit *habit, int day_index,
                          const HabitLinkedContext *linked_ctx)
{
    int manual_count = inbe_habit_day_count(habit, day_index);
    int session_count = habit_linked_session_count_for_day(linked_ctx, day_index);
    return manual_count > session_count ? manual_count : session_count;
}

static void
habit_apply_count_action(InbeHabits *habits, int index, int day_index,
                         int delta, int minimum_count)
{
    int count;

    if(habits == NULL || index < 0 || index >= habits->count || day_index <= 0)
        return;
    count = inbe_habit_day_count(&habits->items[index], day_index) + delta;
    if(count < minimum_count)
        count = minimum_count;
    inbe_habit_set_day_count(habits, index, day_index, count);
}

static void
habit_open_linked_edit_page(InbeApp *app, int habit_index, int day_index)
{
    if(app == NULL)
        return;
    app->habit_detail_index = habit_index;
    app->habit_detail_day = day_index;
    app->habit_detail_session_path[0] = '\0';
    app->habit_session_edit_active = 0;
    app->habit_session_edit_kind = HABIT_SESSION_EDIT_NONE;
    app->habit_session_edit_round = -1;
    app->habit_session_edit_cursor = 0;
    app->habit_session_edit_path[0] = '\0';
    app->habit_session_edit_text[0] = '\0';
    app->habit_session_edit_scroll = 0;
    app->modal.active = 0;
    app->modal.type = 0;
    app->inbe.screen = InbeScreenHabitSessionEdit;
}

static void
draw_habits_top_bar(InbeApp *app, int draw_menu)
{
    static const char *options[INBE_HABIT_MAX + 1];
    static int dropdown_selected = 0;
    int option_count;
    int selected;
    int top_h = flint_px(58);
    int icon_size = flint_px(20);
    int icon_padding = flint_px(8);
    int icon_w = icon_size + icon_padding * 2;
    int icon_gap = flint_px(6);
    int edit_x = view_width - icon_w - flint_px(10);
    int view_toggle_x = edit_x - icon_gap - icon_w;
    int controls_x = view_toggle_x;
    int icon_y = (top_h - icon_w) / 2;
    int dropdown_x = flint_px(12);
    int dropdown_y = (top_h - flint_px(36)) / 2;
    int dropdown_w = controls_x - dropdown_x - flint_px(10);
    int hover = 0;

    if(app == NULL)
        return;

    if(dropdown_w < flint_px(150))
        dropdown_w = flint_px(150);
    if(dropdown_w > flint_px(260))
        dropdown_w = flint_px(260);

    option_count = app->habits.count;
    if(option_count > INBE_HABIT_MAX)
        option_count = INBE_HABIT_MAX;
    for(int i = 0; i < option_count; i++)
        options[i] = app->habits.items[i].name;
    options[option_count++] = "Add new habit";

    selected = app->habits.selected;
    if(selected < 0 || selected >= app->habits.count)
        selected = 0;

    if(!draw_menu) {
        dropdown_selected = selected;

        DrawRectangle(0, 0, view_width, top_h, flint_darken(theme_get_bg(), 14));
        DrawLine(0, top_h - 1, view_width, top_h - 1, flint_darken(theme_get_bg(), 42));

        if(!app->modal.active &&
           ui_draw_dropdown_button(301, dropdown_x, dropdown_y, dropdown_w,
                                   flint_px(36), options, option_count, &dropdown_selected)) {
            if(dropdown_selected == app->habits.count) {
                habit_edit_begin_new(app);
                return;
            }
            if(app->habits.selected != dropdown_selected) {
                app->habits.selected = dropdown_selected;
                app->habits.scroll = 0;
                app->habits.weekly_days = HABIT_WEEKLY_INITIAL_DAYS;
            }
            inbe_habits_save(&app->habits);
        }

        if(!app->modal.active && app->habits.count > 0) {
            Texture2D toggle_icon = app->habits.view_mode == HABIT_VIEW_WEEKLY
                                      ? app->icons[UI_ICON_TYPE_WEEKLY]
                                      : app->icons[UI_ICON_TYPE_CALENDAR];
            if(ui_draw_icon_btn_padded(view_toggle_x, icon_y, icon_size, icon_padding,
                                       toggle_icon, &hover)) {
                app->habits.view_mode = app->habits.view_mode == HABIT_VIEW_WEEKLY
                                          ? HABIT_VIEW_CALENDAR
                                          : HABIT_VIEW_WEEKLY;
                app->habits.scroll = 0;
                if(app->habits.view_mode == HABIT_VIEW_WEEKLY)
                    app->habits.weekly_days = HABIT_WEEKLY_INITIAL_DAYS;
                return;
            }
            if(ui_draw_icon_btn_padded(edit_x, icon_y, icon_size, icon_padding,
                                       app->icons[UI_ICON_TYPE_PENCIL], &hover)) {
                habit_edit_begin(app, app->habits.selected);
                return;
            }
        }

        return;
    }

    if(draw_menu && !app->modal.active && ui_draw_dropdown_menu(301)) {
        if(dropdown_selected == app->habits.count) {
            habit_edit_begin_new(app);
            return;
        }
        if(app->habits.selected != dropdown_selected) {
            app->habits.selected = dropdown_selected;
            app->habits.scroll = 0;
            app->habits.weekly_days = HABIT_WEEKLY_INITIAL_DAYS;
        }
        inbe_habits_save(&app->habits);
    }
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

    if(text == NULL || text[0] == '\0' || w <= 0 || h <= 0)
        return;
    font_size = flint_px(text_size);
    flint_text_draw(text, x, flint_text_y(text, y, h, font_size), font_size, color);
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
                 mixed ? "Mixed practice" : practice_activity_label(first_activity));
    }
    if(secondary != NULL && secondary_size > 0)
        snprintf(secondary, secondary_size, "%d session%s", count, count == 1 ? "" : "s");
    return count;
}

static int
habit_weekly_summary_button(InbeApp *app, int x, int y, int w, int h, int completed, int disabled,
                            const char *primary, const char *secondary)
{
    Vector2 mouse_world;
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int hovered;
    Color fill = completed ? theme_get_button() : flint_darken(theme_get_bg(), 10);
    Color text = disabled ? flint_darken(theme_get_text(), 35) : theme_get_text();
    int pad = flint_px(9);
    int text_x;
    int text_w;
    const char *line1 = primary != NULL ? primary : "";
    const char *line2 = secondary != NULL ? secondary : "";

    if(app == NULL)
        return 0;
    mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    hovered = CheckCollisionPointRec(mouse_world, bounds) &&
              !ui_input_captures_click(mouse_world);
    if(hovered) {
        if(disabled)
            app->cursor_disabled = 1;
        else
            app->cursor_clickable = 1;
    }
    hovered = hovered && !disabled;

    if(hovered)
        fill = theme_get_button_hover();
    if(!disabled && completed)
        text = habit_text_color_for_background(fill);
    DrawRectangle(x, y, w, h, fill);
    ui_draw_bevel(x, y, w, h, flint_lighten(fill, 36), flint_darken(fill, 42));

    text_x = x + pad;
    text_w = w - pad * 2;

    if(line1[0] != '\0') {
        if(line2[0] != '\0') {
            int line1_h = flint_px(24);
            int line2_h = flint_px(16);
            int block_h = line1_h + line2_h;
            int block_y = y + (h - block_h) / 2;
            habit_weekly_draw_text_line(line1, text_x, block_y, text_w, line1_h,
                                        FLINT_TEXT_16, text);
            habit_weekly_draw_text_line(line2, text_x, block_y + line1_h, text_w, line2_h,
                                        FLINT_TEXT_8, flint_darken(text, 16));
        } else {
            habit_weekly_draw_text_line(line1, text_x, y + flint_px(3),
                                        text_w, h - flint_px(6),
                                        FLINT_TEXT_16, text);
        }
    }

    if(hovered)
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
    int row_h = flint_px(44);
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
    int today_index = inbe_habits_today_index();
    int label_w = flint_px(88);
    int gap = flint_px(8);
    int button_x;
    int button_w;
    int row_h = flint_px(44);
    int row_gap = flint_px(6);
    int day_font = flint_px(16);
    int date_font = flint_px(FLINT_TEXT_8);
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
        completed = inbe_habit_completed_day(active, day_index);
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
            DrawRectangle(content_x, y, label_w, row_h, flint_darken(theme_get_bg(), 5));
            flint_text_draw(day_label, content_x + flint_px(8), y + flint_px(7),
                            day_font, theme_get_text());
            flint_text_draw(date_label, content_x + flint_px(8), y + flint_px(29),
                            date_font, flint_darken(theme_get_text(), 18));
        }

        if(counting_enabled) {
            (void)habit_weekly_summary_button(app, button_x, y, button_w, row_h,
                                              completed, future_day, primary, secondary);
            action = habit_counter_day_action(app, selected, day_index,
                                              button_x, y, button_w, row_h,
                                              future_day, !has_linked_day);
            if(action == 0 && has_linked_day &&
               !future_day && CheckCollisionPointRec(GetScreenToWorld2D(GetMousePosition(), app->camera),
                                                     (Rectangle){(float)button_x, (float)y, (float)button_w, (float)row_h}) &&
               IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                habit_open_linked_edit_page(app, selected, day_index);
            }
        } else if(habit_weekly_summary_button(app, button_x, y, button_w, row_h, completed, future_day,
                                              primary, secondary)) {
            if(has_linked_day) {
                habit_open_linked_edit_page(app, selected, day_index);
            } else {
                inbe_habit_toggle_day(&app->habits, selected, day_index);
                active = &app->habits.items[selected];
                inbe_app_auto_sync(app);
            }
        }
        if(action == 1 || action == -1) {
            habit_apply_count_action(&app->habits, selected, day_index, action,
                                     minimum_count);
            active = &app->habits.items[selected];
            count = habit_effective_day_count(active, day_index, linked_ctx);
            completed = count > 0;
            inbe_app_auto_sync(app);
        }
        if(completed && !future_day)
            draw_habit_completion_underline(button_x, y, button_w, row_h, active->color);
        if(counting_enabled && count > 0 && !future_day) {
            char count_label[16];
            int count_font = flint_px(8);
            int count_w;
            snprintf(count_label, sizeof(count_label), "%d", count);
            count_w = flint_text_measure(count_label, count_font);
            flint_text_draw(count_label,
                            button_x + button_w - count_w - flint_px(6),
                            y + flint_px(5),
                            count_font, theme_get_text());
        }
        if(!future_day && has_linked_day)
            draw_habit_link_dot(button_x, y, button_w, active->color);

        y += row_h + row_gap;
    }

    y += flint_px(8);
    if(ui_draw_generic_button(content_x, y, content_w, load_h, "Load more",
                              UI_BUTTON_STYLE_SECONDARY,
                              app->habits.weekly_days >= HABIT_WEEKLY_MAX_DAYS,
                              &load_hover)) {
        app->habits.weekly_days += HABIT_WEEKLY_LOAD_DAYS;
        if(app->habits.weekly_days > HABIT_WEEKLY_MAX_DAYS)
            app->habits.weekly_days = HABIT_WEEKLY_MAX_DAYS;
    }
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
    (void)app;
    return 0;
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
        .max_codepoints = 3,
        .filter = habit_session_text_filter,
        .filter_user_data = app,
        .commit_pressed = &commit_pressed
    });

    if(commit_pressed)
        return habit_session_commit_edit(app, entry);

    return 0;
}

void
draw_habits_screen(InbeApp *app)
{
    int top_h = flint_px(58);
    int nav_h = flint_px(TAB_BAR_H);
    int content_x;
    int content_w;
    int y = top_h + flint_px(16);
    int viewport_h = view_height - top_h - nav_h;
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
    int today_index = inbe_habits_today_index();
    int cell_w;
    int cell_h;
    int grid_x;
    int grid_y;
    HabitLinkedContext *linked_ctx = NULL;
    int active_is_linked;
    int forward_disabled;
    int scroll_content_h;
    int scroll_y;
    int scroll_h;
    int weekly_days = HABIT_WEEKLY_INITIAL_DAYS;
    FlintUIScrollArea scroll_area;
    FlintUIScrollView scroll_view;

    if(app == NULL)
        return;

    flint_centered_column(max_w, side_padding, &content_x, &content_w);

    if(app->habits.count <= 0) {
        const char *empty_text = "No habits created";
        const char *create_text = "Create habit";
        int empty_font = flint_px(20);
        int button_w = content_w < flint_px(240) ? content_w : flint_px(240);
        int button_h = flint_px(42);
        int empty_y = top_h + viewport_h / 2 - flint_px(46);
        int empty_w = flint_text_measure(empty_text, empty_font);
        int hover_empty_create = 0;

        flint_clip_begin((int)app->camera.offset.x,
                         (int)(app->camera.offset.y + top_h * app->camera.zoom),
                         (int)(view_width * app->camera.zoom),
                         (int)(viewport_h * app->camera.zoom));
        flint_text_draw(empty_text, content_x + (content_w - empty_w) / 2,
                        empty_y, empty_font, theme_get_text());
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

    if(app->habits.view_mode == HABIT_VIEW_WEEKLY) {
        scroll_content_h = habit_weekly_content_height(weekly_days);
    } else {
        int planned_cell_w = (content_w - grid_gap * 6) / 7;
        if(planned_cell_w < flint_px(28))
            planned_cell_w = flint_px(28);
        scroll_content_h = flint_px(26) + month_h + flint_px(12) +
                           planned_cell_w * 6 + grid_gap * 5 + flint_px(16);
    }

    scroll_y = top_h + flint_px(8);
    scroll_h = viewport_h - flint_px(8);
    if(scroll_h < 0)
        scroll_h = 0;

    scroll_area = (FlintUIScrollArea){
        .bounds = {0.0f, (float)scroll_y, (float)view_width, (float)scroll_h},
        .content_height = scroll_content_h,
        .content_x = content_x,
        .content_width = content_w,
        .scroll_offset = &app->habits.scroll,
        .wheel_step = flint_px(42),
        .scrollbar_x = view_width - flint_px(8)
    };
    scroll_view = ui_scroll_container_begin(scroll_area);
    content_x = scroll_view.content_x;
    content_w = scroll_view.content_w;
    y = scroll_view.content_y + flint_px(26);

    if(app->habits.view_mode == HABIT_VIEW_WEEKLY) {
        ui_set_input_blocked(app->modal.active);
        draw_habits_weekly_view(app, active, selected, linked_ctx,
                                content_x, content_w, y, weekly_days);
        ui_scroll_container_end(scroll_area, scroll_view);
        free(linked_ctx);
        ui_set_input_blocked(0);
        draw_habits_top_bar(app, 0);
        if(app->inbe.screen == InbeScreenHabits)
            draw_habits_top_bar(app, 1);
        return;
    }

    forward_disabled = app->habits.month_offset >= 0;
    ui_set_input_blocked(app->modal.active);
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
                    content_x + (content_w - flint_text_measure(month_label, flint_px(22))) / 2,
                    flint_ui_text_y(month_label, y, month_h, flint_px(22)),
                    flint_px(22), theme_get_text());

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
                DrawRectangle(cell_x, cell_y, cell_w, cell_h, flint_darken(theme_get_bg(), 5));
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
            completed = inbe_habit_completed_day(active, day_index);
            if(!completed && !future_day && (has_linked_day || count > 0))
                completed = 1;
            if(counting_enabled) {
                (void)ui_draw_generic_button(cell_x, cell_y, cell_w, cell_h, day_label,
                                             completed ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_SECONDARY,
                                             future_day, &hover);
                action = habit_counter_day_action(app, selected, day_index,
                                                  cell_x, cell_y, cell_w, cell_h,
                                                  future_day, !has_linked_day);
                if(action == 0 && has_linked_day && !future_day &&
                   CheckCollisionPointRec(GetScreenToWorld2D(GetMousePosition(), app->camera),
                                          (Rectangle){(float)cell_x, (float)cell_y,
                                                      (float)cell_w, (float)cell_h}) &&
                   IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    habit_open_linked_edit_page(app, selected, day_index);
                }
            } else if(ui_draw_generic_button(cell_x, cell_y, cell_w, cell_h, day_label,
                                             completed ? UI_BUTTON_STYLE_PRIMARY : UI_BUTTON_STYLE_SECONDARY,
                                             future_day, &hover)) {
                if(has_linked_day) {
                    habit_open_linked_edit_page(app, selected, day_index);
                } else {
                    inbe_habit_toggle_day(&app->habits, selected, day_index);
                    active = &app->habits.items[selected];
                    inbe_app_auto_sync(app);
                }
            }
            if(action == 1 || action == -1) {
                habit_apply_count_action(&app->habits, selected, day_index, action,
                                         minimum_count);
                active = &app->habits.items[selected];
                count = habit_effective_day_count(active, day_index, linked_ctx);
                completed = count > 0;
                inbe_app_auto_sync(app);
            }
            if(completed && !future_day) {
                draw_habit_completion_underline(cell_x, cell_y, cell_w, cell_h, active->color);
            }
            if(counting_enabled && count > 0 && !future_day) {
                char count_label[16];
                int count_font = flint_px(8);
                int count_w;
                snprintf(count_label, sizeof(count_label), "%d", count);
                count_w = flint_text_measure(count_label, count_font);
                flint_text_draw(count_label,
                                cell_x + cell_w - count_w - flint_px(4),
                                cell_y + flint_px(4),
                                count_font, theme_get_text());
            }
            if(!future_day && has_linked_day) {
                draw_habit_link_dot(cell_x, cell_y, cell_w, active->color);
            }
            if(day_index == today_index) {
                DrawRectangleLinesEx((Rectangle){(float)cell_x, (float)cell_y,
                                                 (float)cell_w, (float)cell_h},
                                     (float)flint_px(2), theme_get_text());
            }
        }
    }

    ui_scroll_container_end(scroll_area, scroll_view);
    free(linked_ctx);
    ui_set_input_blocked(0);
    draw_habits_top_bar(app, 0);
    if(app->inbe.screen == InbeScreenHabits)
        draw_habits_top_bar(app, 1);
}

static int
habit_color_button(InbeApp *app, int x, int y, Color color, int selected)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int radius = flint_px(13);
    Rectangle bounds = {
        (float)(x - radius - flint_px(6)),
        (float)(y - radius - flint_px(6)),
        (float)(radius * 2 + flint_px(12)),
        (float)(radius * 2 + flint_px(12))
    };
    int hovered = CheckCollisionPointRec(mouse_world, bounds);

    DrawCircle(x, y, radius, color);
    DrawCircleLines(x, y, radius + flint_px(2),
                    selected ? theme_get_text() : flint_darken(theme_get_bg(), 42));
    if(hovered) {
        app->cursor_clickable = 1;
        DrawCircleLines(x, y, radius + flint_px(5), theme_get_button_hover());
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            return 1;
    }
    return 0;
}

void
draw_habit_edit_screen(InbeApp *app)
{
    const char *title;
    const char *activity_options[] = {
        "Wim Hof Breathing",
        "Meditation",
        "Sun Salutation",
        "7-Minute Workout"
    };
    Color color_options[6];
    int top_h = flint_px(58);
    int nav_h = flint_px(TAB_BAR_H);
    int content_x;
    int content_w;
    int max_w = flint_px(CONTENT_MAX_W);
    int y = top_h + flint_px(18);
    int font = flint_ui_font();
    int label_font = flint_ui_font_small();
    int field_h = flint_px(40);
    int hover = 0;
    int title_font = flint_px(22);
    int title_w;

    if(app == NULL)
        return;

    if(!app->habit_edit_active) {
        app->inbe.screen = InbeScreenHabits;
        return;
    }

    title = app->habit_edit_is_new ? "New Habit" : "Edit Habit";

    DrawRectangle(0, 0, view_width, top_h, theme_get_bg());
    DrawLine(0, top_h - 1, view_width, top_h - 1, flint_darken(theme_get_button(), 18));
    if(!app->modal.active &&
       ui_draw_icon_btn_padded(flint_px(12), flint_px(12), flint_px(24),
                               flint_px(8), app->icons[UI_ICON_TYPE_RETURN], &hover)) {
        habit_edit_cancel(app);
        app->inbe.screen = InbeScreenHabits;
        return;
    }
    title_w = flint_text_measure(title, title_font);
    flint_text_draw(title, (view_width - title_w) / 2,
                    flint_ui_text_y(title, 0, top_h, title_font),
                    title_font, theme_get_text());
    if(!app->modal.active &&
       ui_draw_icon_btn_padded(view_width - flint_px(52), flint_px(12),
                               flint_px(24), flint_px(8), app->icons[UI_ICON_TYPE_CHECK], &hover)) {
        habit_edit_commit(app);
        return;
    }

    flint_centered_column(max_w, flint_page_side_padding(), &content_x, &content_w);

    if(app->modal.active && app->modal.type == UIModalConfirmDeleteHabit) {
        int modal_result = ui_draw_modal("Delete habit?",
                                         "Delete this habit? This cannot be undone.",
                                         locale_get("cancel_button"),
                                         locale_get("delete_button"));
        if(modal_result == 1) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
        } else if(modal_result == 2) {
            int index = app->habit_edit_index;
            app->modal.active = 0;
            app->modal.type = UIModalNone;
            habit_edit_cancel(app);
            if(index >= 0 && index < app->habits.count) {
                inbe_habits_delete(&app->habits, index);
                inbe_app_auto_sync(app);
            }
            app->inbe.screen = InbeScreenHabits;
        }
        return;
    }

    flint_clip_begin((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + top_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)((view_height - top_h - nav_h) * app->camera.zoom));

    flint_text_draw("Name", content_x, y, label_font, flint_darken(theme_get_text(), 34));
    y += flint_px(22);
    habit_edit_handle_keyboard(app);
    if(!app->habit_edit_active) {
        flint_clip_end();
        return;
    }
    draw_habit_edit_field(app, content_x, y, content_w, field_h, font);
    y += field_h + flint_px(24);

    flint_text_draw("Underline", content_x, y, label_font, flint_darken(theme_get_text(), 34));
    y += flint_px(32);
    color_options[0] = (Color){94, 166, 232, 255};
    color_options[1] = (Color){99, 196, 165, 255};
    color_options[2] = (Color){210, 180, 72, 255};
    color_options[3] = (Color){224, 124, 104, 255};
    color_options[4] = (Color){180, 132, 220, 255};
    color_options[5] = (Color){216, 116, 164, 255};
    for(int i = 0; i < 6; i++) {
        int cx = content_x + flint_px(18) + i * flint_px(42);
        int selected = app->habit_edit_color.r == color_options[i].r &&
                       app->habit_edit_color.g == color_options[i].g &&
                       app->habit_edit_color.b == color_options[i].b;
        if(habit_color_button(app, cx, y, color_options[i], selected))
            app->habit_edit_color = color_options[i];
    }
    y += flint_px(34);

    flint_text_draw("Practice list", content_x, y, label_font, flint_darken(theme_get_text(), 34));
    y += flint_px(24);
    for(int i = 0; i < EXERCISE_COUNT; i++) {
        int enabled = (app->habit_edit_sync_activity & habit_activity_mask_for(i)) != 0;
        if(ui_draw_checkbox_toggle(content_x, y, activity_options[i], &enabled)) {
            if(enabled)
                app->habit_edit_sync_activity |= habit_activity_mask_for(i);
            else
                app->habit_edit_sync_activity &= ~habit_activity_mask_for(i);
            app->habit_edit_sync_mode = app->habit_edit_sync_activity != 0
                                            ? INBE_HABIT_SYNC_ACTIVITIES
                                            : INBE_HABIT_SYNC_NONE;
            if(app->habit_edit_sync_activity != 0)
                app->habit_edit_counter_enabled = 1;
        }
        y += flint_px(42);
    }

    y += flint_px(4);
    flint_text_draw("Counting", content_x, y, label_font, flint_darken(theme_get_text(), 34));
    y += flint_px(24);
    if(app->habit_edit_sync_activity != 0) {
        int forced_counter = 1;
        ui_draw_checkbox_toggle_disabled(content_x, y, "Allow multiple counts",
                                         &forced_counter, 1);
        app->habit_edit_counter_enabled = 1;
        y += flint_px(30);
        flint_text_draw("Required for practice-linked habits", content_x, y,
                        label_font, flint_darken(theme_get_text(), 42));
        y += flint_px(12);
    } else if(ui_draw_checkbox_toggle(content_x, y, "Allow multiple counts",
                                      &app->habit_edit_counter_enabled)) {
        app->habit_edit_counter_enabled = app->habit_edit_counter_enabled != 0;
    }
    y += flint_px(42);

    if(!app->habit_edit_is_new) {
        int delete_w = flint_px(160);
        int delete_h = flint_px(38);
        int hover_delete = 0;
        y += flint_px(10);
        if(ui_draw_generic_button(content_x, y, delete_w, delete_h,
                                  "Delete Habit", UI_BUTTON_STYLE_DANGER,
                                  0, &hover_delete)) {
            app->modal.active = 1;
            app->modal.type = UIModalConfirmDeleteHabit;
            app->modal.selected_button = 0;
        }
    }

    flint_clip_end();
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
        .bounds = {0.0f, (float)y, (float)view_width, (float)viewport_h},
        .content_height = content_h,
        .content_x = content_x,
        .content_width = content_w,
        .scroll_offset = &app->habit_session_edit_scroll,
        .wheel_step = flint_px(42),
        .scrollbar_x = view_width - flint_px(8)
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
            if(habit_session_draw_keyboard(app, active_entry))
                return;
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
            char summary_text[32];
            int icon_size = flint_px(18);
            int icon_padding = flint_px(6);
            int icon_w = icon_size + icon_padding * 2;
            int trash_x = content_x + content_w - icon_w;
            int row_font = flint_ui_font_small();
            int summary_x;
            int hover_trash = 0;

            if(ctx->entries[i].activity != activity)
                continue;

            snprintf(time_text, sizeof(time_text), "%02d:%02d",
                     ctx->entries[i].hour, ctx->entries[i].minute);
            if(activity == EXERCISE_MEDITATION)
                habit_format_duration(ctx->entries[i].total_seconds,
                                      summary_text, sizeof(summary_text));
            else
                locale_format(summary_text, sizeof(summary_text), "results_rounds",
                              ctx->entries[i].round_count);
            summary_x = content_x + flint_px(76);
            if(summary_x + flint_text_measure(summary_text, row_font) > trash_x - flint_px(8))
                summary_x = content_x + flint_px(62);

            if(draw) {
                flint_text_draw(time_text, content_x,
                                flint_ui_text_y(time_text, y, row_h, row_font),
                                row_font, theme_get_text());
                flint_text_draw(summary_text, summary_x,
                                flint_ui_text_y(summary_text, y, row_h, row_font),
                                row_font, flint_darken(theme_get_text(), 12));
                if(ui_draw_icon_btn_padded(trash_x, y - flint_px(4), icon_size, icon_padding,
                                           app->icons[UI_ICON_TYPE_TRASH], &hover_trash)) {
                    if(data_delete_session(ctx->entries[i].path))
                        habit_refresh_detail_day_completion(app);
                    habit_session_cancel_edit(app);
                    return y;
                }
            }
            y += row_h;

            if(activity == EXERCISE_MEDITATION) {
                y += flint_px(4);
                continue;
            }

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
                    flint_ui_draw_text_left_in_rect(
                        round_line,
                        (Rectangle){(float)round_text_x, (float)y,
                                    (float)round_text_w, (float)flint_px(24)},
                        row_font,
                        editing_round ? theme_get_button_hover()
                                      : theme_get_text());
                    if(ui_draw_icon_btn_padded(round_edit_x, y - flint_px(6),
                                               icon_size, icon_padding,
                                               app->icons[UI_ICON_TYPE_PENCIL], &hover_round_edit)) {
                        habit_session_begin_round_edit(app, &ctx->entries[i], r);
                        return y;
                    }
                    if(ui_draw_icon_btn_padded(round_trash_x, y - flint_px(6),
                                               icon_size, icon_padding,
                                               app->icons[UI_ICON_TYPE_TRASH], &hover_round_trash)) {
                        if(habit_session_delete_round(&ctx->entries[i], r))
                            habit_refresh_detail_day_completion(app);
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

    if(ctx->day_filter > 0 &&
       app->habit_detail_index >= 0 &&
       app->habit_detail_index < app->habits.count) {
        InbeHabit *habit = &app->habits.items[app->habit_detail_index];
        int minimum_count = ctx->count;
        int total_count = habit_effective_day_count(habit, ctx->day_filter, ctx);
        int button_h = flint_px(34);
        int step_w = flint_px(42);
        int gap = flint_px(8);
        int minus_x = content_x;
        int plus_x = content_x + content_w - step_w;
        int label_x = minus_x + step_w + gap;
        int label_w = plus_x - label_x - gap;
        int hover = 0;
        char total_text[64];
        char min_text[64];

        y += flint_px(6);
        if(draw) {
            snprintf(total_text, sizeof(total_text), "Total count %d", total_count);
            snprintf(min_text, sizeof(min_text), "Minimum %d from session%s",
                     minimum_count, minimum_count == 1 ? "" : "s");
            if(ui_draw_generic_button(minus_x, y, step_w, button_h, "-",
                                      UI_BUTTON_STYLE_SECONDARY,
                                      total_count <= minimum_count, &hover)) {
                habit_apply_count_action(&app->habits, app->habit_detail_index,
                                         ctx->day_filter, -1, minimum_count);
                inbe_app_auto_sync(app);
                return y;
            }
            DrawRectangle(label_x, y, label_w, button_h, flint_darken(theme_get_bg(), 5));
            flint_text_draw(total_text, label_x + flint_px(8),
                            y + flint_px(4), flint_ui_font_small(), theme_get_text());
            flint_text_draw(min_text, label_x + flint_px(8),
                            y + flint_px(20), flint_px(FLINT_TEXT_8),
                            flint_darken(theme_get_text(), 18));
            if(ui_draw_generic_button(plus_x, y, step_w, button_h, "+",
                                      UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
                habit_apply_count_action(&app->habits, app->habit_detail_index,
                                         ctx->day_filter, 1, minimum_count);
                inbe_app_auto_sync(app);
                return y;
            }
        }
        y += button_h + flint_px(8);
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
    int cursor;

    if(app == NULL)
        return;
    habit_session_clamp_cursor(app);
    len = strlen(app->habit_session_edit_text);
    cursor = app->habit_session_edit_cursor;

    if(len < 3) {
        memmove(app->habit_session_edit_text + cursor + 1,
                app->habit_session_edit_text + cursor,
                len - (size_t)cursor + 1);
        app->habit_session_edit_text[cursor] = c;
        app->habit_session_edit_cursor = cursor + 1;
        return;
    }

    if(cursor < (int)len) {
        app->habit_session_edit_text[cursor] = c;
        app->habit_session_edit_cursor = cursor + 1;
    }
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
