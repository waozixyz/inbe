#include "habits_screen.h"

#include "practice_screen.h"
#include "../data.h"
#include "../storage.h"
#include "../app.h"
#include "../theme.h"
#include "flint_runtime_assets.h"
#include "../locale.h"
#include "../breath_engine.h"
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
                                 Color color, int sync_topic);

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
                           INBE_HABIT_SYNC_NONE,
                           INBE_HABIT_TOPIC_MIND, 0);
}

void
inbe_habits_add_default_set(InbeHabits *habits)
{
    if(habits == NULL)
        return;

    memset(habits, 0, sizeof(*habits));
    inbe_habits_add_seed(habits, "mind", "Mind", (Color){126, 183, 230, 255},
                         INBE_HABIT_TOPIC_MIND);
    inbe_habits_add_seed(habits, "yoga", "Yoga", (Color){208, 128, 80, 255},
                         INBE_HABIT_TOPIC_YOGA);
    inbe_habits_add_seed(habits, "fitness", "Fitness", (Color){208, 96, 128, 255},
                         INBE_HABIT_TOPIC_FITNESS);
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
                       int sync_mode, int sync_topic, int sync_activity)
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
    habit->sync_topic = sync_topic;
    habit->sync_activity = sync_activity;
    habits->selected = habits->count;
    habits->count++;
    inbe_habits_save(habits);
    return habits->selected;
}

static void
inbe_habits_add_seed(InbeHabits *habits, const char *id, const char *name,
                     Color color, int sync_topic)
{
    InbeHabit *habit;

    if(habits == NULL || habits->count >= INBE_HABIT_MAX)
        return;

    habit = &habits->items[habits->count++];
    memset(habit, 0, sizeof(*habit));
    copy_text(habit->id, sizeof(habit->id), id);
    copy_text(habit->name, sizeof(habit->name), name);
    habit->color = color;
    habit->sync_mode = INBE_HABIT_SYNC_TOPIC;
    habit->sync_topic = sync_topic;
    habit->sync_activity = 0;
}

void
inbe_habits_init(InbeHabits *habits)
{
    if(habits == NULL)
        return;
    data_init();
    if(inbe_storage_habits_load(habits)) {
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

/* Integration functions */
int
habit_topic_for_activity(int exercise_type)
{
    if(exercise_type == EXERCISE_SUN_SALUTATION)
        return INBE_HABIT_TOPIC_YOGA;
    if(exercise_type == EXERCISE_7_MINUTE_WORKOUT)
        return INBE_HABIT_TOPIC_FITNESS;
    return INBE_HABIT_TOPIC_MIND;
}

void
sync_habits_for_activity(InbeApp *app, int exercise_type)
{
    int topic;
    int today;
    int selected;
    int changed = 0;

    if(app == NULL)
        return;

    topic = habit_topic_for_activity(exercise_type);
    today = inbe_habits_today_index();
    selected = app->habits.selected;
    for(int i = 0; i < app->habits.count; i++) {
        InbeHabit *habit = &app->habits.items[i];
        if(habit->sync_mode == INBE_HABIT_SYNC_TOPIC &&
           habit->sync_topic == topic) {
            if(!inbe_habit_completed_day(habit, today)) {
                inbe_habit_set_day(&app->habits, i, today, 1);
                changed = 1;
            }
        } else if(habit->sync_mode == INBE_HABIT_SYNC_ACTIVITY &&
                  habit->sync_activity == exercise_type) {
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

void
habits_sync_topic_theme_colors(InbeApp *app, int sync_topic, int save_now)
{
    Color color;
    int changed = 0;

    if(app == NULL || sync_topic < 0 || sync_topic >= 3)
        return;

    color = practice_theme_color(app, sync_topic);
    for(int i = 0; i < app->habits.count; i++) {
        InbeHabit *habit = &app->habits.items[i];
        if(habit->sync_mode != INBE_HABIT_SYNC_TOPIC ||
           habit->sync_topic != sync_topic)
            continue;
        if(habit->color.r != color.r || habit->color.g != color.g ||
           habit->color.b != color.b || habit->color.a != 255) {
            habit->color = color;
            habit->color.a = 255;
            changed = 1;
        }
    }

    if(changed && save_now)
        inbe_habits_save(&app->habits);
}
/* Habit utility helpers */
int
habit_is_linked(const InbeHabit *habit)
{
    return habit != NULL && habit->sync_mode != INBE_HABIT_SYNC_NONE;
}

static int
habit_date_index(int year, int month, int day)
{
    return year * 10000 + month * 100 + day;
}

static void
habit_format_date(int day_index, char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "%04d-%02d-%02d",
             day_index / 10000, (day_index / 100) % 100, day_index % 100);
}

static const char *
habit_month_label(int month)
{
    static const char *months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    if(month < 1 || month > 12)
        return "";
    return months[month - 1];
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
    app->habit_edit_sync_topic = INBE_HABIT_TOPIC_MIND;
    app->habit_edit_sync_activity = EXERCISE_WIM_HOF;
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
    app->habit_edit_sync_topic = app->habits.items[index].sync_topic;
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
        if(app->habit_edit_sync_mode == INBE_HABIT_SYNC_TOPIC) {
            app->habit_edit_sync_topic = clampi(app->habit_edit_sync_topic,
                                                0, INBE_HABIT_TOPIC_COUNT - 1);
            app->habit_edit_color = practice_theme_color(app, app->habit_edit_sync_topic);
        }
        if(app->habit_edit_is_new) {
            inbe_habits_add_custom(&app->habits, text, app->habit_edit_color,
                                   app->habit_edit_sync_mode,
                                   app->habit_edit_sync_topic,
                                   app->habit_edit_sync_activity);
        } else {
            snprintf(app->habits.items[index].name,
                     sizeof(app->habits.items[index].name), "%s", text);
            app->habits.items[index].color = app->habit_edit_color;
            app->habits.items[index].color.a = 255;
            app->habits.items[index].sync_mode = app->habit_edit_sync_mode;
            app->habits.items[index].sync_topic = app->habit_edit_sync_topic;
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
    if(ctx->sync_mode == INBE_HABIT_SYNC_TOPIC && topic != ctx->sync_topic)
        return;
    if(ctx->sync_mode == INBE_HABIT_SYNC_ACTIVITY && activity != ctx->sync_activity)
        return;
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
    ctx->sync_topic = habit != NULL ? habit->sync_topic : INBE_HABIT_TOPIC_MIND;
    ctx->sync_activity = habit != NULL ? habit->sync_activity : EXERCISE_WIM_HOF;
    data_list_session_records(habit_linked_session_callback, ctx);
}

static int
habit_linked_has_day(const HabitLinkedContext *ctx, int day_index)
{
    if(ctx == NULL)
        return 0;
    for(int i = 0; i < ctx->count; i++) {
        int entry_day = ctx->entries[i].year * 10000 + ctx->entries[i].month * 100 + ctx->entries[i].day;
        if(entry_day == day_index)
            return 1;
    }
    return 0;
}

static void
habit_open_linked_details(InbeApp *app, int habit_index, int day_index)
{
    if(app == NULL)
        return;
    app->habit_detail_index = habit_index;
    app->habit_detail_day = day_index;
    app->habit_detail_session_index = -1;
    app->habit_detail_session_path[0] = '\0';
    app->habit_session_edit_active = 0;
    app->habit_session_edit_kind = 0;
    app->habit_session_edit_round = -1;
    app->habit_session_edit_path[0] = '\0';
    app->habit_session_edit_text[0] = '\0';
    app->modal.active = 1;
    app->modal.type = UIModalHabitLinkedDetails;
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
}

static void
habit_open_session_edit_page(InbeApp *app)
{
    if(app == NULL)
        return;
    if(app->habit_detail_index < 0 || app->habit_detail_index >= app->habits.count) {
        app->inbe.screen = InbeScreenHabits;
        return;
    }
    app->modal.active = 0;
    app->modal.type = 0;
    app->habit_session_edit_scroll = 0;
    app->habit_session_edit_active = 0;
    app->habit_session_edit_kind = 0;
    app->habit_session_edit_round = -1;
    app->habit_session_edit_path[0] = '\0';
    app->habit_session_edit_text[0] = '\0';
    app->inbe.screen = InbeScreenHabitSessionEdit;
}

/* UI button functions */
void
draw_habits_manager_button(InbeApp *app)
{
    int icon_size = flint_px(20);
    int icon_padding = flint_px(8);
    int button_w = icon_size + icon_padding * 2;
    int button_x = view_width - button_w - flint_px(10);
    int button_y = (flint_px(58) - button_w) / 2;
    int hover = 0;

    if(app == NULL || app->modal.active)
        return;

    if(ui_draw_icon_btn_padded(button_x, button_y, icon_size, icon_padding,
                               app->icons[UI_ICON_TYPE_STACK], &hover)) {
        app->practice_config_theme_tab = PRACTICE_CONFIG_TAB_LIST;
        app->previous_screen = InbeScreenHabits;
        app->inbe.screen = InbeScreenTrackerConfig;
    }
}

void
draw_habit_view_button(InbeApp *app)
{
    int stack_icon_size = flint_px(20);
    int stack_padding = flint_px(8);
    int stack_w = stack_icon_size + stack_padding * 2;
    int button_w = flint_px(68);
    int button_h = flint_px(34);
    int button_x = view_width - stack_w - flint_px(10) - button_w - flint_px(8);
    int button_y = (flint_px(58) - button_h) / 2;
    int hover = 0;

    if(app == NULL || app->modal.active)
        return;
    if(button_x < flint_px(92))
        return;

    if(ui_draw_generic_button(button_x, button_y, button_w, button_h, "View",
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
        app->habits_view_mode = app->habits_view_mode == 0 ? 1 : 0;
        app->habits_list_scroll = 0;
        app->habits_list_expanded_year = 0;
        app->habits_list_expanded_month = 0;
        app->habits_list_expanded_day = 0;
        app->habits_list_expanded_session = -1;
    }
}

/* Cascade drawing functions */
static int
draw_habit_cascade_row(InbeApp *app, int x, int y, int w, int h,
                       const char *text, int selected, int indent)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int hover = CheckCollisionPointRec(mouse_world, (Rectangle){(float)x, (float)y, (float)w, (float)h});

    if(hover) {
        DrawRectangle(x, y, w, h, selected ? theme_get_button_hover() : flint_darken(theme_get_button_hover(), 6));
        ui_draw_bevel(x, y, w, h, flint_darken(theme_get_button_hover(), 40), flint_lighten(theme_get_button_hover(), 40));
        app->cursor_clickable = 1;
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            ui_draw_bevel(x, y, w, h, flint_lighten(theme_get_button_hover(), 40), flint_darken(theme_get_button_hover(), 40));
    } else {
        DrawRectangle(x, y, w, h, selected ? theme_get_button() : flint_darken(theme_get_bg(), 6));
        ui_draw_bevel(x, y, w, h, flint_lighten(theme_get_button(), 28), flint_darken(theme_get_button(), 20));
    }

    flint_text_draw(text, x + flint_px(indent),
                    flint_ui_text_y(text, y, h, flint_px(16)),
                    flint_px(16), theme_get_text());
    return hover && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

static int
draw_habit_cascade_action(int right_x, int y, int row_h, Texture2D icon)
{
    int icon_size = flint_px(16);
    int icon_padding = flint_px(4);
    int btn_w = icon_size + icon_padding * 2;
    int hover = 0;

    return ui_draw_icon_btn_padded(right_x - btn_w - flint_px(4),
                                   y + (row_h - btn_w) / 2,
                                   icon_size, icon_padding, icon, &hover);
}

static void
habit_open_session_edit_page_for_session(InbeApp *app, int habit_index,
                                         int day_index, const char *path)
{
    if(app == NULL || path == NULL || path[0] == '\0')
        return;
    app->habit_detail_index = habit_index;
    app->habit_detail_day = day_index;
    app->habit_detail_session_index = 0;
    snprintf(app->habit_detail_session_path,
             sizeof(app->habit_detail_session_path), "%s", path);
    habit_open_session_edit_page(app);
}

/* Comparison function for sorting linked entries */
static int
compare_habit_linked_entries(const void *a, const void *b)
{
    const HabitLinkedEntry *ea = a;
    const HabitLinkedEntry *eb = b;

    if(ea->year != eb->year)
        return eb->year - ea->year;
    if(ea->month != eb->month)
        return eb->month - ea->month;
    if(ea->day != eb->day)
        return eb->day - ea->day;
    if(ea->hour != eb->hour)
        return eb->hour - ea->hour;
    if(ea->minute != eb->minute)
        return eb->minute - ea->minute;
    return eb->second - ea->second;
}

static void
habit_filter_ctx_to_session(HabitLinkedContext *ctx, const char *path)
{
    if(ctx == NULL || path == NULL || path[0] == '\0')
        return;
    
    ctx->count = 0;
    ctx->total_seconds = 0;
    ctx->best_seconds = 0;
    
    for(int i = 0; i < HABIT_LINKED_ENTRY_MAX; i++) {
        if(strcmp(ctx->entries[i].path, path) == 0) {
            if(ctx->count < i) {
                ctx->entries[ctx->count] = ctx->entries[i];
            }
            ctx->count++;
        }
    }
}

/* Modal drawing function */
void
draw_habit_linked_details_modal(InbeApp *app)
{
    HabitLinkedContext ctx;
    InbeHabit *habit;
    char date_text[32];
    int modal_w = flint_px(350);
    int modal_h = flint_px(330);
    int y;
    int row_h = flint_px(28);
    FlintUIPanelFrame frame;

    if(app == NULL || !app->modal.active || app->modal.type != UIModalHabitLinkedDetails)
        return;
    if(app->habit_detail_index < 0 || app->habit_detail_index >= app->habits.count) {
        app->modal.active = 0;
        app->modal.type = 0;
        return;
    }

    habit = &app->habits.items[app->habit_detail_index];
    habit_collect_linked_entries(habit, app->habit_detail_day, &ctx);
    if(ctx.count > 1)
        qsort(ctx.entries, (size_t)ctx.count, sizeof(ctx.entries[0]), compare_habit_linked_entries);
    habit_filter_ctx_to_session(&ctx, app->habit_detail_session_path);
    habit_format_date(app->habit_detail_day, date_text, sizeof(date_text));

    frame = ui_draw_modal_frame(modal_w, modal_h, date_text,
                                (Texture2D){0}, app->icons[UI_ICON_TYPE_X]);

    if(frame.right_clicked) {
        app->modal.active = 0;
        app->modal.type = 0;
        return;
    }

    y = frame.content_y;

    if(ctx.count <= 0) {
        flint_text_draw("No sessions", frame.content_x, y,
                        flint_ui_font(), theme_get_text());
        return;
    }

    {
        char summary[128];
        snprintf(summary, sizeof(summary), "%d session%s",
                 ctx.count, ctx.count == 1 ? "" : "s");
        flint_text_draw(summary, frame.content_x, y,
                        flint_ui_font(), theme_get_text());
        y += flint_px(34);
    }

    for(int i = 0; i < ctx.count && y + row_h < frame.y + frame.h - flint_px(14); i++) {
        char line[128];
        int icon_size = flint_px(18);
        int icon_padding = flint_px(6);
        int icon_w = icon_size + icon_padding * 2;
        int edit_x = frame.content_x + frame.content_w - icon_w;
        int line_w = edit_x - frame.content_x - flint_px(8);
        int hover = 0;

        if(line_w < flint_px(80))
            line_w = flint_px(80);
        snprintf(line, sizeof(line), "%02d:%02d  %d rounds",
                 ctx.entries[i].hour, ctx.entries[i].minute,
                 ctx.entries[i].round_count);
        flint_text_draw_fitted_in_rect(line,
                                       (Rectangle){frame.content_x, y, line_w, row_h},
                                       flint_ui_font_small(), flint_px(10), theme_get_text());
        if(ui_draw_icon_btn_padded(edit_x, y - flint_px(4), icon_size, icon_padding,
                                   app->icons[UI_ICON_TYPE_PENCIL], &hover)) {
            habit_open_session_edit_page_for_session(app, app->habit_detail_index,
                                                     app->habit_detail_day,
                                                     ctx.entries[i].path);
            return;
        }
        y += row_h;

        for(int r = 0; r < ctx.entries[i].round_count &&
            y + row_h < frame.y + frame.h - flint_px(14); r++) {
            char round_line[64];
            snprintf(round_line, sizeof(round_line), "Round %d  %ds",
                     r + 1, ctx.entries[i].rounds[r]);
            flint_text_draw(round_line, frame.content_x + flint_px(16), y,
                            flint_ui_font_small(), flint_darken(theme_get_text(), 24));
            y += flint_px(24);
        }
        y += flint_px(4);
    }
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
        if(active_entry != NULL)
            habit_session_draw_keyboard(app, active_entry);
    }
}

int
draw_habit_session_edit_content(InbeApp *app, HabitLinkedContext *ctx, int content_x, int content_w, int y, int draw)
{
    int row_h = flint_px(34);

    if(app == NULL || ctx == NULL)
        return y;

    if(ctx->count <= 0) {
        if(draw)
            flint_text_draw("No sessions", content_x, y, flint_ui_font(), theme_get_text());
        return y + row_h;
    }

    for(int i = 0; i < ctx->count; i++) {
        char line[128];
        int icon_size = flint_px(18);
        int icon_padding = flint_px(6);
        int icon_w = icon_size + icon_padding * 2;
        int edit_x = content_x + content_w - icon_w;
        int line_w = edit_x - content_x - flint_px(8);
        int hover = 0;

        if(line_w < flint_px(80))
            line_w = flint_px(80);
        snprintf(line, sizeof(line), "%02d:%02d  %d rounds",
                 ctx->entries[i].hour, ctx->entries[i].minute,
                 ctx->entries[i].round_count);

        if(draw) {
            flint_text_draw_fitted_in_rect(line,
                                           (Rectangle){content_x, y, line_w, row_h},
                                           flint_ui_font_small(), flint_px(10), theme_get_text());
            if(ui_draw_icon_btn_padded(edit_x, y - flint_px(4), icon_size, icon_padding,
                                       app->icons[UI_ICON_TYPE_PENCIL], &hover)) {
                habit_open_session_edit_page_for_session(app, app->habit_detail_index,
                                                         app->habit_detail_day,
                                                         ctx->entries[i].path);
                return y;
            }
        }
        y += row_h;

        for(int r = 0; r < ctx->entries[i].round_count; r++) {
            char round_line[64];
            snprintf(round_line, sizeof(round_line), "Round %d  %ds",
                     r + 1, ctx->entries[i].rounds[r]);
            if(draw) {
                flint_text_draw(round_line, content_x + flint_px(16), y,
                                flint_ui_font_small(), flint_darken(theme_get_text(), 24));
            }
            y += flint_px(24);
        }
        y += flint_px(4);
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
