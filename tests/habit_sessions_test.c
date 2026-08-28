#include "app/app.h"
#include "screens/habits_screen.h"
#include "screens/habits/habits.h"

#include <stdio.h>
#include <string.h>

typedef struct TestSessionRecord {
    const char *path;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int topic;
    int activity;
    int rounds[MaxRounds];
    int round_count;
} TestSessionRecord;

static int failures;
static TestSessionRecord records[8];
static int record_count;
static int storage_day_save_count;
static int storage_save_count;
static int auto_sync_count;
static int close_modal_count;
static int switched_screen = -1;
static int focus_active = -1;

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
    memset(records, 0, sizeof(records));
    record_count = 0;
    storage_day_save_count = 0;
    storage_save_count = 0;
    auto_sync_count = 0;
    close_modal_count = 0;
    switched_screen = -1;
    focus_active = -1;
}

static void
add_record(const char *path, int year, int month, int day, int activity,
           int a, int b, int c)
{
    TestSessionRecord *record;

    if(record_count >= (int)(sizeof(records) / sizeof(records[0])))
        return;
    record = &records[record_count++];
    record->path = path;
    record->year = year;
    record->month = month;
    record->day = day;
    record->hour = 8 + record_count;
    record->minute = 10 + record_count;
    record->second = 20 + record_count;
    record->topic = 0;
    record->activity = activity;
    record->rounds[0] = a;
    record->rounds[1] = b;
    record->rounds[2] = c;
    record->round_count = c > 0 ? 3 : (b > 0 ? 2 : 1);
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
}

const char *
GetLocaleText(const char *key)
{
    return key != NULL ? key : "";
}

int
storage_habits_load(void *habits)
{
    (void)habits;
    return 0;
}

void
data_list_session_records(data_session_record_callback callback, void *user)
{
    for(int i = 0; i < record_count; i++) {
        callback(records[i].path, records[i].year, records[i].month,
                 records[i].day, records[i].hour, records[i].minute,
                 records[i].second, records[i].topic, records[i].activity,
                 records[i].rounds, records[i].round_count, user);
    }
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
    return 1;
}

void
storage_make_uuid(char out[37])
{
    snprintf(out, 37, "00000000-0000-4000-8000-000000000001");
}

int
app_auto_sync(InbeApp *app)
{
    (void)app;
    auto_sync_count++;
    return 1;
}

void
app_close_modal(InbeApp *app)
{
    (void)app;
    close_modal_count++;
}

void
app_switch_screen(InbeApp *app, int screen)
{
    if(app != NULL)
        app->inbe.screen = screen;
    switched_screen = screen;
}

void
SetUIFocusTextInputActive(int active)
{
    focus_active = active;
}

static void
init_habit(InbeHabit *habit, int sync_mode, int sync_activity)
{
    memset(habit, 0, sizeof(*habit));
    snprintf(habit->id, sizeof(habit->id), "%s", "habit-1");
    snprintf(habit->name, sizeof(habit->name), "%s", "Linked");
    habit->sync_mode = sync_mode;
    habit->sync_activity = sync_activity;
}

static void
test_collect_filters_and_summarizes(void)
{
    InbeHabit habit;
    HabitLinkedContext ctx;

    reset_stubs();
    add_record("pattern-a", 2026, 8, 28, EXERCISE_PATTERNS, 10, 20, 0);
    add_record("meditation-a", 2026, 8, 28, EXERCISE_MEDITATION, 99, 0, 0);
    add_record("pattern-b", 2026, 8, 29, EXERCISE_PATTERNS, 30, 0, 0);
    add_record("invalid-whm", 2026, 8, 30, -1, 40, 0, 0);

    init_habit(&habit, INBE_HABIT_SYNC_ACTIVITIES,
               habit_activity_mask_for(EXERCISE_PATTERNS));
    habit_collect_linked_entries(&habit, 0, &ctx);

    expect_int("filtered linked count", ctx.count, 2);
    expect_int("filtered total seconds", ctx.total_seconds, 60);
    expect_int("filtered best seconds", ctx.best_seconds, 30);
    expect_str("first linked path", ctx.entries[0].path, "pattern-a");
    expect_int("first linked round count", ctx.entries[0].round_count, 2);
    expect_int("has included day", habit_linked_has_day(&ctx, 20260828), 1);
    expect_int("does not have filtered day", habit_linked_has_day(&ctx, 20260830), 0);
    expect_int("count for included day",
               habit_linked_session_count_for_day(&ctx, 20260828), 1);

    init_habit(&habit, INBE_HABIT_SYNC_ACTIVITIES,
               habit_activity_mask_for(EXERCISE_WIM_HOF));
    habit_collect_linked_entries(&habit, 20260830, &ctx);
    expect_int("invalid activity falls back to whm", ctx.count, 1);
    expect_int("day filter excludes other days",
               habit_linked_session_count_for_day(&ctx, 20260828), 0);
}

static void
test_effective_counts_and_count_actions(void)
{
    InbeApp app = {0};
    HabitLinkedContext ctx;

    reset_stubs();
    init_habit(&app.habits.items[0], INBE_HABIT_SYNC_ACTIVITIES,
               habit_activity_mask_for(EXERCISE_PATTERNS));
    app.habits.count = 1;
    app.habits.selected = 0;

    add_record("pattern-a", 2026, 8, 28, EXERCISE_PATTERNS, 10, 0, 0);
    add_record("pattern-b", 2026, 8, 28, EXERCISE_PATTERNS, 20, 0, 0);
    habit_collect_linked_entries(&app.habits.items[0], 20260828, &ctx);

    habit_set_day_count(&app.habits, 0, 20260828, 1);
    expect_int("linked count wins",
               habit_effective_day_count(&app.habits.items[0], 20260828, &ctx), 2);
    habit_set_day_count(&app.habits, 0, 20260828, 4);
    expect_int("manual count wins",
               habit_effective_day_count(&app.habits.items[0], 20260828, &ctx), 4);

    habit_apply_count_action(&app, 0, 20260828, -10, 2);
    expect_int("count action respects minimum",
               habit_day_count(&app.habits.items[0], 20260828), 2);

    habits_free(&app.habits);
}

static void
test_session_changed_preserves_manual_extra(void)
{
    InbeApp app = {0};

    reset_stubs();
    init_habit(&app.habits.items[0], INBE_HABIT_SYNC_ACTIVITIES,
               habit_activity_mask_for(EXERCISE_PATTERNS));
    app.habits.count = 1;
    app.habit_detail_index = 0;
    app.habit_detail_day = 20260828;

    add_record("pattern-a", 2026, 8, 28, EXERCISE_PATTERNS, 10, 0, 0);
    add_record("pattern-b", 2026, 8, 28, EXERCISE_PATTERNS, 20, 0, 0);
    habit_set_day_count(&app.habits, 0, 20260828, 4);

    habit_session_changed(&app, 1);
    expect_int("session change keeps extra manual count",
               habit_day_count(&app.habits.items[0], 20260828), 5);
    expect_int("session change syncs", auto_sync_count, 1);

    habit_session_changed(&app, -1);
    expect_int("unknown old count only syncs", auto_sync_count, 2);

    habits_free(&app.habits);
}

static void
test_navigation_helpers(void)
{
    InbeApp app = {0};

    reset_stubs();
    app.habit_session_edit.active = 1;
    app.habit_session_edit.round = 3;
    habit_session_cancel_edit(&app);
    expect_int("cancel clears active", app.habit_session_edit.active, 0);
    expect_int("cancel round sentinel", app.habit_session_edit.round, -1);
    expect_int("cancel clears focus", focus_active, 0);

    app.habit_session_edit.active = 1;
    app.habit_session_edit.round = 2;
    snprintf(app.habit_detail_session_path, sizeof(app.habit_detail_session_path),
             "%s", "old-path");
    habit_open_linked_edit_page(&app, 4, 20260828);
    expect_int("open stores habit index", app.habit_detail_index, 4);
    expect_int("open stores day", app.habit_detail_day, 20260828);
    expect_str("open clears selected session", app.habit_detail_session_path, "");
    expect_int("open resets edit round", app.habit_session_edit.round, -1);
    expect_int("open closes modal", close_modal_count, 1);
    expect_int("open switches screen", switched_screen, InbeScreenHabitSessionEdit);
}

int
main(void)
{
    test_collect_filters_and_summarizes();
    test_effective_counts_and_count_actions();
    test_session_changed_preserves_manual_extra();
    test_navigation_helpers();

    if(failures != 0) {
        fprintf(stderr, "%d habit sessions test failure(s)\n", failures);
        return 1;
    }
    printf("habit sessions tests passed\n");
    return 0;
}
