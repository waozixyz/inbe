#ifndef HABITS_SCREEN_H
#define HABITS_SCREEN_H

#include "kryon.h"
#include "core/breath_engine.h"
#include "app_fwd.h"
#include <stddef.h>

/* Forward declaration for enums defined elsewhere */
enum {
    HABIT_SESSION_EDIT_NONE = 0,
    HABIT_SESSION_EDIT_ROUND = 1
};

/* Habits-specific constants */
enum {
    INBE_HABIT_MAX = 32,
    INBE_HABIT_ID_SIZE = 40,
    INBE_HABIT_PENDING_DAY_SAVE_MAX = 128,
    INBE_HABIT_NAME_SIZE = 40,
    INBE_HABIT_DESCRIPTION_SIZE = 256,
    HABIT_LINKED_ENTRY_MAX = 128,
    HABIT_LINKED_PATH_SIZE = 80
};

enum {
    HABIT_VIEW_CALENDAR = 0,
    HABIT_VIEW_WEEKLY = 1
};

enum {
    HABITS_SCREEN_OVERVIEW = 0,
    HABITS_SCREEN_DETAIL = 1,
    HABITS_SCREEN_REORDER = 2
};

enum {
    HABIT_TAB_WEEKLY = 0,
    HABIT_TAB_MONTHLY,
    HABIT_TAB_STATISTICS,
    HABIT_TAB_EDIT,
    HABIT_TAB_COUNT
};

/* Habits-specific enums */
typedef enum InbeHabitSyncMode {
    INBE_HABIT_SYNC_NONE = 0,
    INBE_HABIT_SYNC_ACTIVITIES = 1
} InbeHabitSyncMode;

/* Data structures */
typedef struct InbeHabitDay {
    int day_index;
    int completed;
    int count;
    int session_count;
} InbeHabitDay;

typedef struct InbeHabit {
    char id[INBE_HABIT_ID_SIZE];
    char name[INBE_HABIT_NAME_SIZE];
    char description[INBE_HABIT_DESCRIPTION_SIZE];
    Color color;
    int sync_mode;
    int sync_activity;
    int counter_enabled;
    int weekdays;          /* bit0=Mon .. bit6=Sun; 0 = every day */
    int reminder_hour;     /* -1 = off, else 0..23 local notification hour */
    InbeHabitDay *days;
    int day_count;
    int day_capacity;
} InbeHabit;

typedef struct InbeHabitDaySave {
    char habit_id[INBE_HABIT_ID_SIZE];
    int day_index;
    int completed;
    int count;
} InbeHabitDaySave;

typedef struct InbeHabits {
    InbeHabit items[INBE_HABIT_MAX];
    char loaded_ids[INBE_HABIT_MAX][INBE_HABIT_ID_SIZE];
    InbeHabitDaySave pending_day_saves[INBE_HABIT_PENDING_DAY_SAVE_MAX];
    int count;
    int loaded_count;
    int pending_day_save_count;
    int selected;
    int scroll;
    int tab_scroll;
    int focus_selected_tab;
    int month_offset;
    int selector_open;
    int screen_mode;
    int view_mode;
    int tab;
    int weekly_days;
    int hold_stats_range_days;
    int loaded;
    int dirty;
} InbeHabits;

/* Additional structures */
typedef struct HabitLinkedEntry {
    char path[HABIT_LINKED_PATH_SIZE];
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int activity;
    int rounds[MaxRounds];
    int round_count;
    int total_seconds;
    int best_seconds;
} HabitLinkedEntry;

typedef struct HabitLinkedContext {
    HabitLinkedEntry entries[HABIT_LINKED_ENTRY_MAX];
    int count;
    int day_filter;
    int sync_mode;
    int sync_activity;
    int total_seconds;
    int best_seconds;
} HabitLinkedContext;

/* Core habits functions */
void habits_init(InbeHabits *habits);
void habits_init_with_defaults(InbeHabits *habits, int seed_defaults);
void habits_free(InbeHabits *habits);
void habits_save(InbeHabits *habits);
void habits_flush_save(InbeApp *app);
int habits_clear_days(InbeHabits *habits);
int habit_reserve_days(InbeHabit *habit, int capacity);
int habits_today_index(void);
int habit_completed_day(const InbeHabit *habit, int day_index);
int habit_completed_today(const InbeHabit *habit);
void habit_set_day(InbeHabits *habits, int index, int day_index, int completed);
void habit_set_day_count(InbeHabits *habits, int index, int day_index, int count);
int habit_day_count(const InbeHabit *habit, int day_index);
void habit_toggle_day(InbeHabits *habits, int index, int day_index);
void habit_increment_day(InbeHabits *habits, int index, int day_index, int delta);
void habit_toggle_today(InbeHabits *habits, int index);
void habits_add_default(InbeHabits *habits);
void habits_add_default_set(InbeHabits *habits);
int habits_seed_default_set_if_needed(InbeHabits *habits);
void habits_delete(InbeHabits *habits, int index);
int habits_move(InbeHabits *habits, int from_index, int to_index);
int habits_name_exists(const InbeHabits *habits, const char *name, int exclude_index);
void habits_generate_unique_name(InbeHabits *habits, char *name_buffer, size_t buffer_size, const char *base_name);
int habits_add_custom(InbeHabits *habits, const char *name, Color color,
                           int sync_mode, int sync_activity);
int habit_activity_mask_for(int exercise);
int habit_matches_activity(const InbeHabit *habit, int exercise_type);

void sync_habits_for_activity(InbeApp *app, int exercise_type);

/* Habit edit functions (moved to habits_screen.c) */
void habit_edit_begin(InbeApp *app, int index);
void habit_edit_begin_new(InbeApp *app);
void habit_edit_commit(InbeApp *app);
void habit_edit_cancel(InbeApp *app);

/* UI functions (moved to habits_screen.c) */
void draw_habits_screen(InbeApp *app);
void habit_edit_draw(InbeApp *app);
void habit_session_draw_edit_screen(InbeApp *app);
int habits_screen_selector_height(InbeApp *app);
int habits_screen_top_reserved(InbeApp *app);
int habits_screen_first_run_guide_active(const InbeApp *app);
void habits_screen_prepare_first_run_guide(InbeApp *app);
void habits_screen_dismiss_first_run_guide(InbeApp *app);
void habits_screen_draw_first_run_guide(InbeApp *app);
void draw_habits_top_bar(InbeApp *app, int draw_menu);
void draw_habits_reorder(InbeApp *app, int content_top);
void draw_habits_overview(InbeApp *app, int content_top);
void habits_enter_detail(InbeApp *app, int selected_habit);
void habits_enter_reorder(InbeApp *app);
void habits_begin_new_detail(InbeApp *app);
int habit_counter_day_action(InbeApp *app, int habit_index, int day_index,
                             int x, int y, int w, int h, int disabled,
                             int allow_left_increment);
int habit_weekly_visible_days(InbeHabits *habits);
int habits_scroll_page_content_height(int content_w, void *user_data);
int habits_overview_test_click_point(InbeApp *app, int *out_x, int *out_y);
void draw_habits_weekly_view(InbeApp *app, InbeHabit *active, int selected,
                             HabitLinkedContext *linked_ctx,
                             int content_x, int content_w, int y,
                             int visible_days);
int habit_calendar_day_cell(InbeApp *app, int x, int y, int w, int h,
                            const char *label, int completed, int disabled,
                            int current_day);
void draw_habit_completion_underline(int x, int y, int w, int h, Color color);
void draw_habit_day_count_label(int x, int y, int w, int h, int count);
void draw_habit_link_dot(int x, int y, int w, Color color);
int habit_session_draw_edit_content(InbeApp *app, HabitLinkedContext *ctx, int content_x, int content_w, int y, int draw);
int habit_is_linked(const InbeHabit *habit);
int habit_weekday_bit(int day_index);
int habit_scheduled_day(const InbeHabit *habit, int day_index);
int habit_completed_day(const InbeHabit *habit, int day_index);
void habit_session_cancel_edit(InbeApp *app);

/* Habit session keyboard functions */
int habit_session_keyboard_height(InbeApp *app);
int habit_session_draw_keyboard(InbeApp *app, const HabitLinkedEntry *entry);
int habit_session_keyboard_key(int x, int y, int w, int h, const char *label);
void habit_session_delete_before_cursor(InbeApp *app);
void habit_session_insert_char(InbeApp *app, char c);
int habit_session_commit_edit(InbeApp *app, const HabitLinkedEntry *entry);
void habit_session_clamp_cursor(InbeApp *app);
int habit_session_parse_seconds(const char *text, int *seconds);

#endif
