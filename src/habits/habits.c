#include "habits/habits.h"

#include "data.h"
#include "flint_runtime_assets.h"
#include "../../vendor/rini/src/rini.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
    HABITS_PATH_SIZE = 512
};

static void
habits_path(char *out, size_t out_size)
{
    snprintf(out, out_size, "%s/apps/habits/habits.ini", data_root());
}

static void
habits_ensure_dir(void)
{
    char path[HABITS_PATH_SIZE];
    snprintf(path, sizeof(path), "%s/apps/habits", data_root());
    flint_runtime_asset_ensure_dir(path);
}

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
    rini_data data;
    char path[HABITS_PATH_SIZE];

    if(habits == NULL)
        return;

    habits_ensure_dir();
    habits_path(path, sizeof(path));
    data = rini_load(NULL);
    rini_set_value(&data, "count", habits->count, NULL);
    rini_set_value(&data, "selected", habits->selected, NULL);

    for(int i = 0; i < habits->count; i++) {
        const InbeHabit *habit = &habits->items[i];
        char key[96];

        snprintf(key, sizeof(key), "habit_%d_id", i);
        rini_set_value_text(&data, key, habit->id, NULL);
        snprintf(key, sizeof(key), "habit_%d_name", i);
        rini_set_value_text(&data, key, habit->name, NULL);
        snprintf(key, sizeof(key), "habit_%d_color_r", i);
        rini_set_value(&data, key, habit->color.r, NULL);
        snprintf(key, sizeof(key), "habit_%d_color_g", i);
        rini_set_value(&data, key, habit->color.g, NULL);
        snprintf(key, sizeof(key), "habit_%d_color_b", i);
        rini_set_value(&data, key, habit->color.b, NULL);
        snprintf(key, sizeof(key), "habit_%d_sync_mode", i);
        rini_set_value(&data, key, habit->sync_mode, NULL);
        snprintf(key, sizeof(key), "habit_%d_sync_topic", i);
        rini_set_value(&data, key, habit->sync_topic, NULL);
        snprintf(key, sizeof(key), "habit_%d_sync_activity", i);
        rini_set_value(&data, key, habit->sync_activity, NULL);
        snprintf(key, sizeof(key), "habit_%d_day_count", i);
        rini_set_value(&data, key, habit->day_count, NULL);

        for(int d = 0; d < habit->day_count; d++) {
            snprintf(key, sizeof(key), "habit_%d_day_%d_index", i, d);
            rini_set_value(&data, key, habit->days[d].day_index, NULL);
            snprintf(key, sizeof(key), "habit_%d_day_%d_completed", i, d);
            rini_set_value(&data, key, habit->days[d].completed, NULL);
        }
    }

    rini_save(data, path);
    rini_unload(&data);
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
inbe_habits_delete(InbeHabits *habits, int index)
{
    if(habits == NULL || index < 0 || index >= habits->count)
        return;
    if(habits->count <= 1)
        return;

    for(int i = index; i < habits->count - 1; i++)
        habits->items[i] = habits->items[i + 1];
    habits->count--;
    memset(&habits->items[habits->count], 0, sizeof(habits->items[habits->count]));

    if(habits->selected > index)
        habits->selected--;
    else if(habits->selected >= habits->count)
        habits->selected = habits->count - 1;
    if(habits->selected < 0)
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
    rini_data data;
    char path[HABITS_PATH_SIZE];
    int needs_save = 0;

    if(habits == NULL)
        return;

    memset(habits, 0, sizeof(*habits));
    habits_path(path, sizeof(path));
    data = rini_load(path);

    habits->count = rini_get_value_fallback(data, "count", 0);
    if(habits->count < 0 || habits->count > INBE_HABIT_MAX)
        habits->count = 0;
    habits->selected = rini_get_value_fallback(data, "selected", 0);

    for(int i = 0; i < habits->count; i++) {
        InbeHabit *habit = &habits->items[i];
        char key[96];
        const char *text;

        snprintf(key, sizeof(key), "habit_%d_id", i);
        text = rini_get_value_text(data, key);
        copy_text(habit->id, sizeof(habit->id), text != NULL && text[0] != '\0' ? text : "habit");

        snprintf(key, sizeof(key), "habit_%d_name", i);
        text = rini_get_value_text(data, key);
        copy_text(habit->name, sizeof(habit->name), text != NULL && text[0] != '\0' ? text : habit->id);

        snprintf(key, sizeof(key), "habit_%d_color_r", i);
        habit->color.r = (unsigned char)rini_get_value_fallback(data, key, 99);
        snprintf(key, sizeof(key), "habit_%d_color_g", i);
        habit->color.g = (unsigned char)rini_get_value_fallback(data, key, 196);
        snprintf(key, sizeof(key), "habit_%d_color_b", i);
        habit->color.b = (unsigned char)rini_get_value_fallback(data, key, 165);
        habit->color.a = 255;

        snprintf(key, sizeof(key), "habit_%d_sync_mode", i);
        text = rini_get_value_text(data, key);
        habit->sync_mode = text != NULL
                               ? rini_get_value_fallback(data, key, INBE_HABIT_SYNC_NONE)
                               : INBE_HABIT_SYNC_NONE;
        if(habit->sync_mode < INBE_HABIT_SYNC_NONE || habit->sync_mode > INBE_HABIT_SYNC_ACTIVITY)
            habit->sync_mode = INBE_HABIT_SYNC_NONE;
        snprintf(key, sizeof(key), "habit_%d_sync_topic", i);
        habit->sync_topic = rini_get_value_fallback(data, key, INBE_HABIT_TOPIC_MIND);
        if(habit->sync_topic < 0 || habit->sync_topic >= INBE_HABIT_TOPIC_COUNT)
            habit->sync_topic = INBE_HABIT_TOPIC_MIND;
        snprintf(key, sizeof(key), "habit_%d_sync_activity", i);
        habit->sync_activity = rini_get_value_fallback(data, key, 0);
        if(habit->sync_activity < 0)
            habit->sync_activity = 0;
        if(text == NULL) {
            if(strcmp(habit->id, "mind") == 0 || strcmp(habit->name, "Mind") == 0) {
                habit->sync_mode = INBE_HABIT_SYNC_TOPIC;
                habit->sync_topic = INBE_HABIT_TOPIC_MIND;
                needs_save = 1;
            } else if(strcmp(habit->id, "yoga") == 0 || strcmp(habit->name, "Yoga") == 0) {
                habit->sync_mode = INBE_HABIT_SYNC_TOPIC;
                habit->sync_topic = INBE_HABIT_TOPIC_YOGA;
                needs_save = 1;
            } else if(strcmp(habit->id, "fitness") == 0 || strcmp(habit->name, "Fitness") == 0) {
                habit->sync_mode = INBE_HABIT_SYNC_TOPIC;
                habit->sync_topic = INBE_HABIT_TOPIC_FITNESS;
                needs_save = 1;
            }
        }

        snprintf(key, sizeof(key), "habit_%d_day_count", i);
        habit->day_count = rini_get_value_fallback(data, key, 0);
        if(habit->day_count < 0 || habit->day_count > INBE_HABIT_MAX_DAYS)
            habit->day_count = 0;

        for(int d = 0; d < habit->day_count; d++) {
            snprintf(key, sizeof(key), "habit_%d_day_%d_index", i, d);
            habit->days[d].day_index = rini_get_value_fallback(data, key, 0);
            snprintf(key, sizeof(key), "habit_%d_day_%d_completed", i, d);
            habit->days[d].completed = rini_get_value_fallback(data, key, 0) != 0;
        }
    }

    rini_unload(&data);

    if(habits->count == 0) {
        inbe_habits_add_seed(habits, "mind", "Mind", (Color){126, 183, 230, 255},
                             INBE_HABIT_TOPIC_MIND);
        inbe_habits_add_seed(habits, "yoga", "Yoga", (Color){208, 128, 80, 255},
                             INBE_HABIT_TOPIC_YOGA);
        inbe_habits_add_seed(habits, "fitness", "Fitness", (Color){208, 96, 128, 255},
                             INBE_HABIT_TOPIC_FITNESS);
        habits->selected = 0;
        inbe_habits_save(habits);
    }

    if(habits->selected < 0 || habits->selected >= habits->count)
        habits->selected = 0;
    if(needs_save)
        inbe_habits_save(habits);
    habits->loaded = 1;
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
