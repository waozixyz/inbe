#ifndef INBE_HABITS_INTERNAL_H
#define INBE_HABITS_INTERNAL_H

#include "habits_screen.h"
#include "screens/practice_screen.h"
#include "data.h"
#include "storage.h"
#include "app.h"
#include "runtime_assets.h"
#include "core/breath_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern int view_width;
extern int view_height;

enum {
    HABIT_WEEKLY_MONTH_DAYS = 31,
    HABIT_WEEKLY_INITIAL_DAYS = HABIT_WEEKLY_MONTH_DAYS,
    HABIT_WEEKLY_LOAD_DAYS = HABIT_WEEKLY_MONTH_DAYS,
    HABIT_WEEKLY_MAX_DAYS = 36500,
    HABIT_COUNTER_LONG_PRESS_FRAMES = 30
};

int habit_counting_enabled(const InbeHabit *habit);
void habit_format_date(int day_index, char *out, size_t out_size);
void habit_format_duration(int seconds, char *out, size_t out_size);
int habit_tm_date_index(const struct tm *tm);
void habit_collect_linked_entries(const InbeHabit *habit, int day_filter, HabitLinkedContext *ctx);
int habit_linked_has_day(const HabitLinkedContext *ctx, int day_index);
int habit_linked_session_count_for_day(const HabitLinkedContext *ctx, int day_index);
int habit_effective_day_count(const InbeHabit *habit, int day_index, const HabitLinkedContext *linked_ctx);
void habit_session_changed(InbeApp *app, int old_session_count);
void habit_apply_count_action(InbeApp *app, int index, int day_index, int delta, int minimum_count);
void habit_open_linked_edit_page(InbeApp *app, int habit_index, int day_index);

#endif
