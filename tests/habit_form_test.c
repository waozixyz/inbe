#include "app/app.h"
#include "screens/habits_screen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int saves, syncs;
void PushUIInspectSource(const char *p, int n) { (void)p; (void)n; }
void PopUIInspectSource(void) {}
void SetUIFocusTextInputActive(int active) { (void)active; }
void app_switch_screen(InbeApp *app, int screen) { app->inbe.screen = screen; }
void save_settings(InbeApp *app) { (void)app; }
int app_auto_sync(InbeApp *app) { (void)app; ++syncs; return 0; }
void habits_save(InbeHabits *habits) { (void)habits; ++saves; }
int habits_name_exists(const InbeHabits *habits, const char *name, int exclude)
{
    for(int i = 0; i < habits->count; ++i)
        if(i != exclude && strcmp(habits->items[i].name, name) == 0) return 1;
    return 0;
}
int habits_add_custom(InbeHabits *habits, const char *name, Color color, int mode, int activity)
{
    int index = habits->count++;
    snprintf(habits->items[index].name, sizeof(habits->items[index].name), "%s", name);
    habits->items[index].color = color;
    habits->items[index].sync_mode = mode;
    habits->items[index].sync_activity = activity;
    return index;
}

int main(void)
{
    static InbeApp app;
    habit_edit_begin_new(&app);
    assert(app.inbe.screen == InbeScreenHabitEdit);
    assert(app.habit_edit.text[0] == 0);
    assert(app.habit_edit.reminder_hour == -1);
    assert(app.habit_edit.weekdays == INBE_HABIT_SCHEDULE_OFF);
    assert(app.habit_edit.sections[0]);
    assert(!app.habit_edit.sections[1] && app.habit_edit.sections[2]);
    habit_edit_commit(&app);
    assert(app.habit_edit.active && app.habit_edit.name_error == 1);
    assert(app.habits.count == 0 && saves == 0);
    strcpy(app.habit_edit.text, "   \t ");
    habit_edit_commit(&app);
    assert(app.habit_edit.active && app.habits.count == 0);
    strcpy(app.habit_edit.text, "Test counter");
    strcpy(app.habit_edit.description, "Only for testing");
    app.habit_edit.counter_enabled = 1;
    app.habit_edit.sync_activity = 3;
    app.habit_edit.weekdays = 31;
    app.habit_edit.reminder_hour = 9;
    habit_edit_commit(&app);
    assert(app.habits.count == 1 && !app.habit_edit.active);
    assert(saves == 1 && syncs == 1);
    assert(app.habits.items[0].counter_enabled == 1);
    assert(app.habits.items[0].sync_activity == 3);
    assert(app.habits.items[0].sync_mode == INBE_HABIT_SYNC_ACTIVITIES);
    assert(app.habits.items[0].weekdays == 31 && app.habits.items[0].reminder_hour == 9);
    assert(strcmp(app.habits.items[0].description, "Only for testing") == 0);
    habit_edit_begin_new(&app);
    strcpy(app.habit_edit.text, "Test counter");
    habit_edit_commit(&app);
    assert(app.habit_edit.active && app.habit_edit.name_error == 2 && app.habits.count == 1);
    habit_edit_cancel(&app);
    assert(saves == 1 && app.habits.screen_mode == HABITS_SCREEN_OVERVIEW);
    habit_edit_begin(&app, 0);
    assert(app.inbe.screen == InbeScreenHabitEdit && !app.habit_edit.is_new);
    assert(app.habit_edit.counter_enabled && app.habit_edit.sync_activity == 3);
    strcpy(app.habit_edit.text, "Unsaved edit");
    habit_edit_cancel(&app);
    assert(strcmp(app.habits.items[0].name, "Test counter") == 0 && saves == 1);
    habit_edit_begin(&app, 0);
    app.habit_edit.counter_enabled = 0;
    app.habit_edit.weekdays = 0;
    app.habit_edit.reminder_hour = -1;
    habit_edit_commit(&app);
    assert(!app.habits.items[0].counter_enabled && app.habits.items[0].sync_activity == 3);
    assert(app.habits.items[0].weekdays == 0 && app.habits.items[0].reminder_hour == -1);
    assert(saves == 2 && syncs == 2);
    puts("PASS habit form defaults, validation, counter/link independence, edit and cancellation");
}
