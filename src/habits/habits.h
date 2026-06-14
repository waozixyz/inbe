#ifndef INBE_HABITS_H
#define INBE_HABITS_H

#include "raylib.h"
#include <stddef.h>

enum {
    INBE_HABIT_MAX = 10,
    INBE_HABIT_ID_SIZE = 32,
    INBE_HABIT_NAME_SIZE = 40,
    INBE_HABIT_MAX_DAYS = 366
};

typedef enum InbeHabitSyncMode {
    INBE_HABIT_SYNC_NONE = 0,
    INBE_HABIT_SYNC_TOPIC = 1,
    INBE_HABIT_SYNC_ACTIVITY = 2
} InbeHabitSyncMode;

typedef enum InbeHabitTopic {
    INBE_HABIT_TOPIC_MIND = 0,
    INBE_HABIT_TOPIC_YOGA = 1,
    INBE_HABIT_TOPIC_FITNESS = 2,
    INBE_HABIT_TOPIC_COUNT = 3
} InbeHabitTopic;

typedef struct InbeHabitDay {
    int day_index;
    int completed;
} InbeHabitDay;

typedef struct InbeHabit {
    char id[INBE_HABIT_ID_SIZE];
    char name[INBE_HABIT_NAME_SIZE];
    Color color;
    int sync_mode;
    int sync_topic;
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
    int loaded;
} InbeHabits;

void inbe_habits_init(InbeHabits *habits);
void inbe_habits_save(const InbeHabits *habits);
int inbe_habits_today_index(void);
int inbe_habit_completed_day(const InbeHabit *habit, int day_index);
int inbe_habit_completed_today(const InbeHabit *habit);
void inbe_habit_set_day(InbeHabits *habits, int index, int day_index, int completed);
void inbe_habit_toggle_day(InbeHabits *habits, int index, int day_index);
void inbe_habit_toggle_today(InbeHabits *habits, int index);
void inbe_habits_add_default(InbeHabits *habits);
void inbe_habits_delete(InbeHabits *habits, int index);
int inbe_habits_add_custom(InbeHabits *habits, const char *name, Color color,
                           int sync_mode, int sync_topic, int sync_activity);

#endif
