#include "app/app.h"
#include "screens/habits_screen.h"
#include "screens/habits/habits.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int failures;
static int storage_save_count;
static int storage_day_save_count;
static int storage_day_save_result = 1;
static int storage_load_result;
static int data_init_count;
static int uuid_counter;

static void
expect_int(const char *label, int got, int want)
{
    if(got != want) {
        fprintf(stderr, "FAIL %s: got %d, want %d\n", label, got, want);
        failures++;
    }
}

static void
expect_str(const char *label, const char *got, const char *want)
{
    if(strcmp(got != NULL ? got : "", want != NULL ? want : "") != 0) {
        fprintf(stderr, "FAIL %s: got [%s], want [%s]\n", label,
                got != NULL ? got : "(null)", want != NULL ? want : "(null)");
        failures++;
    }
}

static void
reset_stubs(void)
{
    storage_save_count = 0;
    storage_day_save_count = 0;
    storage_day_save_result = 1;
    storage_load_result = 0;
    data_init_count = 0;
    uuid_counter = 0;
}

void
PushUIInspectSource(const char *path, int line)
{
    (void)path;
    (void)line;
}

void
PopUIInspectSource(void)
{
}

void
data_init(void)
{
    data_init_count++;
}

const char *
GetLocaleText(const char *key)
{
    if(key == NULL)
        return "";
    if(strcmp(key, "habit_default_name_format") == 0)
        return "Habit %d";
    if(strcmp(key, "habit_default_meditation_name") == 0)
        return "Meditation";
    if(strcmp(key, "habit_default_meditation_description") == 0)
        return "Mindful breathing and meditation sessions.";
    if(strcmp(key, "habit_default_yoga_name") == 0)
        return "Yoga";
    if(strcmp(key, "habit_default_yoga_description") == 0)
        return "Sun salutation practice.";
    return key;
}

int
storage_habits_load(void *habits)
{
    (void)habits;
    return storage_load_result;
}

void
storage_habits_save(const void *habits)
{
    (void)habits;
    storage_save_count++;
}

int
storage_habit_day_save(const char *habit_id, int local_date, int completed, int count)
{
    (void)habit_id;
    (void)local_date;
    (void)completed;
    (void)count;
    storage_day_save_count++;
    return storage_day_save_result;
}

void
storage_make_uuid(char out[37])
{
    uuid_counter++;
    snprintf(out, 37, "00000000-0000-4000-8000-%012d", uuid_counter);
}

static void
test_date_helpers(void)
{
    InbeHabit habit = {0};
    struct tm tm_value = {0};
    char text[32];

    expect_int("monday weekday", habit_weekday_bit(20260105), 0);
    expect_int("sunday weekday", habit_weekday_bit(20260111), 6);

    expect_int("unscheduled means every day", habit_scheduled_day(&habit, 20260106), 1);
    habit.weekdays = 1 << 2;
    expect_int("scheduled day included", habit_scheduled_day(&habit, 20260107), 1);
    expect_int("scheduled day excluded", habit_scheduled_day(&habit, 20260108), 0);

    habit_format_date(20261203, text, sizeof(text));
    expect_str("format date", text, "2026-12-03");
    habit_format_duration(65, text, sizeof(text));
    expect_str("format seconds duration", text, "1m 05s");
    habit_format_duration(3661, text, sizeof(text));
    expect_str("format hour duration", text, "1h 01m");

    tm_value.tm_year = 2026 - 1900;
    tm_value.tm_mon = 7;
    tm_value.tm_mday = 28;
    expect_int("tm date index", habit_tm_date_index(&tm_value), 20260828);
}

static void
test_count_mutations(void)
{
    InbeHabits habits = {0};
    int index;

    reset_stubs();
    index = habits_add_custom(&habits, "Walk", (Color){1, 2, 3, 255},
                              INBE_HABIT_SYNC_NONE, 0);
    expect_int("add custom index", index, 0);
    expect_int("add custom count", habits.count, 1);
    expect_str("add custom name", habits.items[0].name, "Walk");
    expect_str("add custom id copied to loaded id", habits.loaded_ids[0],
               "00000000-0000-4000-8000-000000000001");

    habit_toggle_day(&habits, 0, 20260828);
    expect_int("toggle creates one day", habits.items[0].day_count, 1);
    expect_int("toggle completes day", habit_completed_day(&habits.items[0], 20260828), 1);
    expect_int("toggle count", habit_day_count(&habits.items[0], 20260828), 1);

    habit_toggle_day(&habits, 0, 20260828);
    expect_int("second toggle leaves day record", habits.items[0].day_count, 1);
    expect_int("second toggle clears completed", habit_completed_day(&habits.items[0], 20260828), 0);
    expect_int("second toggle count", habit_day_count(&habits.items[0], 20260828), 0);

    habit_set_day_count(&habits, 0, 20260828, 3);
    expect_int("explicit count", habit_day_count(&habits.items[0], 20260828), 3);
    habit_increment_day(&habits, 0, 20260828, -5);
    expect_int("count clamps at zero", habit_day_count(&habits.items[0], 20260828), 0);
    expect_int("day save calls", storage_day_save_count, 4);

    habits_free(&habits);
}

static void
test_dirty_flush_on_storage_failure(void)
{
    InbeApp app = {0};

    reset_stubs();
    habits_add_custom(&app.habits, "Walk", (Color){1, 2, 3, 255},
                      INBE_HABIT_SYNC_NONE, 0);
    storage_day_save_result = 0;
    habit_toggle_day(&app.habits, 0, 20260828);
    expect_int("failed day save marks dirty", app.habits.dirty, 1);

    habits_flush_save(&app);
    expect_int("flush clears dirty", app.habits.dirty, 0);
    expect_int("flush saves once after add", storage_save_count, 2);

    habits_free(&app.habits);
}

static void
test_move_delete_and_unique_names(void)
{
    InbeHabits habits = {0};
    char unique[INBE_HABIT_NAME_SIZE];

    reset_stubs();
    habits_add_custom(&habits, "Walk", (Color){1, 2, 3, 255},
                      INBE_HABIT_SYNC_NONE, 0);
    habits_add_custom(&habits, "Walk", (Color){1, 2, 3, 255},
                      INBE_HABIT_SYNC_NONE, 0);
    habits_add_custom(&habits, "Read", (Color){1, 2, 3, 255},
                      INBE_HABIT_SYNC_NONE, 0);

    expect_str("duplicate name gets suffix", habits.items[1].name, "Walk 2");
    expect_int("exclude current duplicate", habits_name_exists(&habits, "Walk", 0), 0);
    expect_int("find other duplicate", habits_name_exists(&habits, "Walk", 1), 1);

    habits_generate_unique_name(&habits, unique, sizeof(unique), "Walk");
    expect_str("generate unique name", unique, "Walk 3");

    habits.selected = 1;
    expect_int("move 0 to 2", habits_move(&habits, 0, 2), 1);
    expect_str("move order first", habits.items[0].name, "Walk 2");
    expect_str("move order last", habits.items[2].name, "Walk");
    expect_int("move keeps selected habit", habits.selected, 0);

    habits_delete(&habits, 0);
    expect_int("delete count", habits.count, 2);
    expect_str("delete shifts first", habits.items[0].name, "Read");
    expect_int("delete selected clamps", habits.selected, 0);

    habits_free(&habits);
}

static void
test_defaults_and_activity_matching(void)
{
    InbeHabits habits = {0};
    InbeApp app = {0};
    int today;

    reset_stubs();
    habits_init(&habits);
    expect_int("init calls data init", data_init_count, 1);
    expect_int("default habit count", habits.count, 2);
    expect_str("default meditation id", habits.items[0].id, "meditation");
    expect_str("default yoga id", habits.items[1].id, "yoga");
    expect_int("default meditation matches patterns",
               habit_matches_activity(&habits.items[0], EXERCISE_PATTERNS), 1);
    expect_int("default yoga excludes meditation",
               habit_matches_activity(&habits.items[1], EXERCISE_MEDITATION), 0);
    habits_free(&habits);

    reset_stubs();
    habits_add_custom(&app.habits, "Linked", (Color){1, 2, 3, 255},
                      INBE_HABIT_SYNC_ACTIVITIES,
                      habit_activity_mask_for(EXERCISE_PATTERNS));
    habits_add_custom(&app.habits, "Manual", (Color){1, 2, 3, 255},
                      INBE_HABIT_SYNC_NONE, 0);
    app.habits.selected = 1;
    sync_habits_for_activity(&app, EXERCISE_PATTERNS);
    today = habits_today_index();
    expect_int("sync completes linked habit",
               habit_completed_day(&app.habits.items[0], today), 1);
    expect_int("sync leaves manual habit alone",
               habit_completed_day(&app.habits.items[1], today), 0);
    expect_int("sync restores selected", app.habits.selected, 1);
    habits_free(&app.habits);
}

int
main(void)
{
    test_date_helpers();
    test_count_mutations();
    test_dirty_flush_on_storage_failure();
    test_move_delete_and_unique_names();
    test_defaults_and_activity_matching();

    if(failures != 0) {
        fprintf(stderr, "%d habit model test failure(s)\n", failures);
        return 1;
    }
    printf("habit model tests passed\n");
    return 0;
}
