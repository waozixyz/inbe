#ifndef HABITS_SCREEN_H
#define HABITS_SCREEN_H

#include "raylib.h"
#include "breath_engine.h"
#include <stddef.h>

/* Forward declaration for enums defined elsewhere */
enum {
    HABIT_SESSION_EDIT_NONE = 0,
    HABIT_SESSION_EDIT_ROUND = 1
};

/* Habits-specific constants */
enum {
    INBE_HABIT_MAX = 10,
    INBE_HABIT_ID_SIZE = 32,
    INBE_HABIT_NAME_SIZE = 40,
    INBE_HABIT_MAX_DAYS = 366,
    HABIT_LINKED_ENTRY_MAX = 128,
    HABIT_LINKED_PATH_SIZE = 80
};

enum {
    HABIT_VIEW_CALENDAR = 0,
    HABIT_VIEW_WEEKLY = 1
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
} InbeHabitDay;

typedef struct InbeHabit {
    char id[INBE_HABIT_ID_SIZE];
    char name[INBE_HABIT_NAME_SIZE];
    Color color;
    int sync_mode;
    int sync_activity;
    InbeHabitDay days[INBE_HABIT_MAX_DAYS];
    int day_count;
} InbeHabit;

typedef struct InbeHabits {
    InbeHabit items[INBE_HABIT_MAX];
    int count;
    int selected;
    int scroll;
    int month_offset;
    int selector_open;
    int view_mode;
    int loaded;
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

/* Forward declarations and external functions */
typedef struct InbeApp InbeApp;

void save_settings(InbeApp *app);

/* Core habits functions */
void inbe_habits_init(InbeHabits *habits);
void inbe_habits_save(const InbeHabits *habits);
int inbe_habits_clear_days(InbeHabits *habits);
int inbe_habits_today_index(void);
int inbe_habit_completed_day(const InbeHabit *habit, int day_index);
int inbe_habit_completed_today(const InbeHabit *habit);
void inbe_habit_set_day(InbeHabits *habits, int index, int day_index, int completed);
void inbe_habit_toggle_day(InbeHabits *habits, int index, int day_index);
void inbe_habit_toggle_today(InbeHabits *habits, int index);
void inbe_habits_add_default(InbeHabits *habits);
void inbe_habits_add_default_set(InbeHabits *habits);
void inbe_habits_delete(InbeHabits *habits, int index);
int inbe_habits_add_custom(InbeHabits *habits, const char *name, Color color,
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
void draw_habit_edit_screen(InbeApp *app);
void draw_habit_session_edit_screen(InbeApp *app);
int draw_habit_session_edit_content(InbeApp *app, HabitLinkedContext *ctx, int content_x, int content_w, int y, int draw);
int habit_is_linked(const InbeHabit *habit);
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
