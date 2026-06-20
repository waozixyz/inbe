#include "habits_screen.h"

#include "habits/habits.h"

#include "practice_screen.h"
#include "data.h"
#include "storage.h"
#include "app.h"
#include "theme.h"
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
    snprintf(name, sizeof(name), "Habit %d", number);
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
    habits_add_seed(habits, "meditation", "Meditation", (Color){126, 183, 230, 255},
                         habit_activity_mask_for(EXERCISE_WIM_HOF) |
                         habit_activity_mask_for(EXERCISE_MEDITATION));
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
habits_add_custom(InbeHabits *habits, const char *name, Color color,
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
    habit->counter_enabled = 0;
    habits->selected = habits->count;
    habits->count++;
    habits_save(habits);
    return habits->selected;
}

static void
habits_add_seed(InbeHabits *habits, const char *id, const char *name,
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
    habit->counter_enabled = 0;
}

void
habits_init(InbeHabits *habits)
{
    if(habits == NULL)
        return;
    data_init();
    if(storage_habits_load(habits)) {
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
}

void
habit_set_day(InbeHabits *habits, int index, int day_index, int completed)
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
    } else if(completed && habit_reserve_days(habit, habit->day_count + 1)) {
        habit->days[habit->day_count].day_index = day_index;
        habit->days[habit->day_count].completed = 1;
        habit->days[habit->day_count].count = 1;
        habit->day_count++;
    }
    habits->selected = index;
    habits->dirty = 1;
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
    habits->dirty = 1;
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
    habits->dirty = 1;
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
    FlintUIToolbarHeaderResult header_result;
    FlintUIToolbarResult toolbar_result;
    FlintUIToolbarAction actions[2];

    if(app == NULL)
        return;

    option_count = app->habits.count;
    if(option_count > INBE_HABIT_MAX)
        option_count = INBE_HABIT_MAX;
    for(int i = 0; i < option_count; i++)
        options[i] = app->habits.items[i].name;
    options[option_count++] = locale_get("habit_add_new_option");

    selected = app->habits.selected;
    if(selected < 0 || selected >= app->habits.count)
        selected = 0;

    actions[0] = (FlintUIToolbarAction){
        app->icons[app->habits.view_mode == HABIT_VIEW_WEEKLY
                       ? UI_ICON_TYPE_CALENDAR
                       : UI_ICON_TYPE_WEEKLY],
        app->modal.active || app->habits.count <= 0
    };
    actions[1] = (FlintUIToolbarAction){
        app->icons[UI_ICON_TYPE_PENCIL],
        app->modal.active || app->habits.count <= 0
    };

    if(!draw_menu) {
        dropdown_selected = selected;
        header_result = ui_draw_toolbar_header((FlintUIToolbarHeader){
            .leading_icon = app->sidebar_open ? (Texture2D){0} : app->icons[UI_ICON_TYPE_STACK],
            .toolbar = (FlintUIToolbar){
            .id = 301,
            .height = top_h,
            .options = app->modal.active ? NULL : options,
            .option_count = app->modal.active ? 0 : option_count,
            .selected_index = &dropdown_selected,
            .dropdown_min_width = flint_px(150),
            .dropdown_max_width = flint_px(260),
            .dropdown_height = flint_px(36),
            .actions = actions,
            .action_count = 2,
            .action_icon_size = flint_px(20),
            .action_icon_padding = flint_px(8),
            .action_gap = flint_px(6),
            .side_padding = flint_px(12)
            }
        });
        if(header_result.leading_clicked)
            app->sidebar_open = 1;
        toolbar_result = header_result.toolbar;

        if(toolbar_result.clicked_action == 0) {
            app->habits.view_mode = app->habits.view_mode == HABIT_VIEW_WEEKLY
                                        ? HABIT_VIEW_CALENDAR
                                        : HABIT_VIEW_WEEKLY;
            app->habits.scroll = 0;
            if(app->habits.view_mode == HABIT_VIEW_WEEKLY)
                app->habits.weekly_days = HABIT_WEEKLY_INITIAL_DAYS;
            return;
        }
        if(toolbar_result.clicked_action == 1) {
            habit_edit_begin(app, app->habits.selected);
            return;
        }

        return;
    }

    if(app->modal.active)
        return;

    toolbar_result = ui_draw_toolbar((FlintUIToolbar){
        .id = 301,
        .draw_menu = 1,
        .options = options,
        .option_count = option_count,
        .selected_index = &dropdown_selected
    });
    if(toolbar_result.selected_menu_item >= 0) {
        if(dropdown_selected == app->habits.count) {
            habit_edit_begin_new(app);
            return;
        }
        if(app->habits.selected != dropdown_selected) {
            app->habits.selected = dropdown_selected;
            app->habits.scroll = 0;
            app->habits.weekly_days = HABIT_WEEKLY_INITIAL_DAYS;
        }
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

static int
habit_calendar_day_cell(InbeApp *app, int x, int y, int w, int h,
                        const char *label, int completed, int disabled)
{
    Vector2 mouse_world = app != NULL
                              ? GetScreenToWorld2D(GetMousePosition(), app->camera)
                              : GetMousePosition();
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int inside = !disabled && CheckCollisionPointRec(mouse_world, bounds);
    Color fill = completed ? theme_get_button() : flint_darken(theme_get_bg(), 10);
    Color text = disabled ? flint_darken(theme_get_text(), 35) : theme_get_text();
    int font = FLINT_TEXT_16;
    int text_w;

    DrawRectangle(x, y, w, h, fill);
    ui_draw_bevel(x, y, w, h, flint_lighten(fill, 28), flint_darken(fill, 34));
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

    if(text == NULL || text[0] == '\0' || w <= 0 || h <= 0)
        return;
    font_size = text_size;
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
            DrawRectangle(content_x, y, label_w, row_h, flint_darken(theme_get_bg(), 5));
            flint_text_draw(day_label, content_x + flint_px(8), y + flint_px(7),
                            day_font, theme_get_text());
            flint_text_draw(date_label, content_x + flint_px(8), y + flint_px(33),
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
                            count_font, theme_get_text());
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

    content_top = app_content_top_reserved(app);
    content_bottom = app_content_bottom_reserved(app);
    y = content_top + flint_px(8);
    viewport_h = view_height - content_top - content_bottom;

    flint_centered_column(max_w, side_padding, &content_x, &content_w);

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
            ui_set_input_blocked(app->modal.active);
            draw_habits_weekly_view(app, active, selected, linked_ctx,
                                    content_x, content_w, y, weekly_days);
            ui_scroll_page_end(page);
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
                        content_x + (content_w - flint_text_measure(month_label, FLINT_TEXT_24)) / 2,
                        flint_ui_text_y(month_label, y, month_h, FLINT_TEXT_24),
                        FLINT_TEXT_24, theme_get_text());

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
            completed = habit_completed_day(active, day_index);
            if(!completed && !future_day && (has_linked_day || count > 0))
                completed = 1;
            if(counting_enabled) {
                (void)habit_calendar_day_cell(app, cell_x, cell_y, cell_w, cell_h,
                                              day_label, completed, future_day);
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
            } else if(habit_calendar_day_cell(app, cell_x, cell_y, cell_w, cell_h,
                                              day_label, completed, future_day)) {
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

        ui_scroll_page_end(page);
    }
    free(linked_ctx);
    ui_set_input_blocked(0);
    draw_habits_top_bar(app, 0);
    if(app->inbe.screen == InbeScreenHabits)
        draw_habits_top_bar(app, 1);
}
