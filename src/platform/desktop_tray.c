#include "desktop_tray.h"

#if defined(INBE_DESKTOP_TRAY_ENABLED)

#include "app.h"
#include "DesktopTray.h"
#include "habits_screen.h"
#include "locale.h"
#include "practices/practice_registry.h"
#include "practices/sun_salutation/sun_salutation_practice.h"
#include "raylib.h"

#include <stdio.h>
#include <string.h>

typedef struct InbeTraySnapshot {
    int window_visible;
    int count;
    char show_hide_label[96];
    char start_practice_label[96];
    char habits_label[96];
    char quit_label[96];
    char whm_label[96];
    char meditation_label[96];
    char sun_salutation_label[96];
    char habit_labels[INBE_HABIT_MAX][128];
    int habit_indices[INBE_HABIT_MAX];
    int habit_enabled[INBE_HABIT_MAX];
} InbeTraySnapshot;

static InbeTraySnapshot TraySnapshot;
static int TrayReady;

static const char *const TrayIconPaths[] = {
    "inbe.png",
    "packaging/linux/appimage/inbe.png",
    "packaging/snap/snap/gui/inbe.png",
    "web-assets/icons/inbe.png",
    NULL
};

static InbeDesktopTrayAction
GetTrayHabitAction(int index)
{
    if(index < 0 || index >= INBE_HABIT_MAX)
        return INBE_DESKTOP_TRAY_ACTION_NONE;
    return (InbeDesktopTrayAction)(INBE_DESKTOP_TRAY_ACTION_MARK_HABIT_0 + index);
}

static int
GetTrayHabitIndex(InbeDesktopTrayAction action)
{
    if(action < INBE_DESKTOP_TRAY_ACTION_MARK_HABIT_0 ||
       action > INBE_DESKTOP_TRAY_ACTION_MARK_HABIT_9)
        return -1;
    return (int)(action - INBE_DESKTOP_TRAY_ACTION_MARK_HABIT_0);
}

static const char *
GetTrayWindowLabelKey(int visible)
{
    return visible ? "tray_hide_inner_breeze" : "tray_show_inner_breeze";
}

static InbeDesktopTrayAction
GetTrayWindowAction(int visible)
{
    return visible ? INBE_DESKTOP_TRAY_ACTION_HIDE : INBE_DESKTOP_TRAY_ACTION_SHOW;
}

static void
InitTraySnapshotLabels(InbeTraySnapshot *snapshot)
{
    if(snapshot == NULL)
        return;
    snprintf(snapshot->show_hide_label, sizeof(snapshot->show_hide_label),
             "Show Inner Breeze");
    snprintf(snapshot->start_practice_label, sizeof(snapshot->start_practice_label),
             "Start Practice");
    snprintf(snapshot->habits_label, sizeof(snapshot->habits_label), "Mark Complete");
    snprintf(snapshot->quit_label, sizeof(snapshot->quit_label), "Quit Inner Breeze");
    snprintf(snapshot->whm_label, sizeof(snapshot->whm_label), "Wim Hof");
    snprintf(snapshot->meditation_label, sizeof(snapshot->meditation_label), "Meditation");
    snprintf(snapshot->sun_salutation_label, sizeof(snapshot->sun_salutation_label),
             "Sun Salutation");
}

static void
FillTraySnapshotLabels(InbeTraySnapshot *snapshot)
{
    if(snapshot == NULL)
        return;
    snprintf(snapshot->show_hide_label, sizeof(snapshot->show_hide_label), "%s",
             GetLocaleText(GetTrayWindowLabelKey(snapshot->window_visible)));
    snprintf(snapshot->start_practice_label, sizeof(snapshot->start_practice_label), "%s",
             GetLocaleText("tray_start_practice"));
    snprintf(snapshot->habits_label, sizeof(snapshot->habits_label), "%s",
             GetLocaleText("tray_mark_complete"));
    snprintf(snapshot->quit_label, sizeof(snapshot->quit_label), "%s",
             GetLocaleText("tray_quit_inner_breeze"));
    snprintf(snapshot->whm_label, sizeof(snapshot->whm_label), "%s",
             GetLocaleText("exercise_wim_hof"));
    snprintf(snapshot->meditation_label, sizeof(snapshot->meditation_label), "%s",
             GetLocaleText("exercise_meditation"));
    snprintf(snapshot->sun_salutation_label, sizeof(snapshot->sun_salutation_label), "%s",
             GetLocaleText("exercise_sun_salutation"));
}

static void
SeedTraySnapshot(void)
{
    InbeTraySnapshot next;

    memset(&next, 0, sizeof(next));
    next.window_visible = !IsWindowHidden();
    FillTraySnapshotLabels(&next);
    TraySnapshot = next;
}

static int
BuildTrayMenu(const InbeTraySnapshot *snapshot,
              DesktopTrayMenuItem *items, int item_count,
              DesktopTrayMenuItem *start_items, int start_item_count,
              DesktopTrayMenuItem *habit_items, int habit_item_count)
{
    InbeTraySnapshot local_snapshot;
    int item_index = 0;

    if(items == NULL || item_count < 5 ||
       start_items == NULL || start_item_count < 3 ||
       habit_items == NULL || habit_item_count < INBE_HABIT_MAX)
        return 0;

    memset(&local_snapshot, 0, sizeof(local_snapshot));
    InitTraySnapshotLabels(&local_snapshot);
    if(snapshot != NULL)
        local_snapshot = *snapshot;

    start_items[0] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.whm_label,
        .action = INBE_DESKTOP_TRAY_ACTION_START_WHM,
        .enabled = 1
    };
    start_items[1] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.meditation_label,
        .action = INBE_DESKTOP_TRAY_ACTION_START_MEDITATION,
        .enabled = 1
    };
    start_items[2] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.sun_salutation_label,
        .action = INBE_DESKTOP_TRAY_ACTION_START_SUN_SALUTATION,
        .enabled = 1
    };

    for(int i = 0; i < local_snapshot.count && i < INBE_HABIT_MAX; i++) {
        habit_items[i] = (DesktopTrayMenuItem){
            .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
            .label = local_snapshot.habit_labels[i],
            .action = GetTrayHabitAction(i),
            .enabled = local_snapshot.habit_enabled[i]
        };
    }

    items[item_index++] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.show_hide_label,
        .action = GetTrayWindowAction(local_snapshot.window_visible),
        .enabled = 1
    };
    items[item_index++] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_SUBMENU,
        .label = local_snapshot.start_practice_label,
        .enabled = 1,
        .children = start_items,
        .child_count = 3
    };
    items[item_index++] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_SUBMENU,
        .label = local_snapshot.habits_label,
        .enabled = local_snapshot.count > 0,
        .children = habit_items,
        .child_count = local_snapshot.count
    };
    items[item_index++] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_SEPARATOR
    };
    items[item_index++] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.quit_label,
        .action = INBE_DESKTOP_TRAY_ACTION_QUIT,
        .enabled = 1
    };

    return item_index;
}

static void
ApplyTrayMenuSnapshot(const InbeTraySnapshot *snapshot)
{
    DesktopTrayMenuItem items[5];
    DesktopTrayMenuItem start_items[3];
    DesktopTrayMenuItem habit_items[INBE_HABIT_MAX];
    int count;

    memset(items, 0, sizeof(items));
    memset(start_items, 0, sizeof(start_items));
    memset(habit_items, 0, sizeof(habit_items));

    count = BuildTrayMenu(snapshot, items, 5, start_items, 3,
                          habit_items, INBE_HABIT_MAX);
    SetDesktopTrayMenu(items, count);
    SetDesktopTrayActivateAction(GetTrayWindowAction(snapshot != NULL
                                                         ? snapshot->window_visible
                                                         : !IsWindowHidden()));
}

int
inbe_desktop_tray_init(void)
{
    DesktopTraySpec spec;
    DesktopTrayMenuItem items[5];
    DesktopTrayMenuItem start_items[3];
    DesktopTrayMenuItem habit_items[INBE_HABIT_MAX];
    int item_count;

    SeedTraySnapshot();
    memset(items, 0, sizeof(items));
    memset(start_items, 0, sizeof(start_items));
    memset(habit_items, 0, sizeof(habit_items));
    item_count = BuildTrayMenu(&TraySnapshot, items, 5, start_items, 3,
                               habit_items, INBE_HABIT_MAX);

    memset(&spec, 0, sizeof(spec));
    spec.id = "inbe";
    spec.title = "Inner Breeze";
    spec.icon_name = "inbe";
    spec.icon_paths = TrayIconPaths;
    spec.close_action = INBE_DESKTOP_TRAY_ACTION_CLOSE_REQUEST;
    spec.activate_action = GetTrayWindowAction(TraySnapshot.window_visible);
    spec.menu_items = items;
    spec.menu_item_count = item_count;

    TrayReady = InitDesktopTray(&spec);
    return TrayReady;
}

void
inbe_desktop_tray_shutdown(void)
{
    ShutdownDesktopTray();
    TrayReady = 0;
}

InbeDesktopTrayAction
inbe_desktop_tray_poll_action(void)
{
    return (InbeDesktopTrayAction)PollDesktopTrayAction();
}

static void
RestoreTrayWindow(void)
{
    ClearWindowState(FLAG_WINDOW_HIDDEN);
    RestoreWindow();
}

static void
HideTrayWindow(void)
{
    SetWindowState(FLAG_WINDOW_HIDDEN);
}

void
inbe_desktop_tray_keep_running(void)
{
    if(TrayReady)
        SetWindowState(FLAG_WINDOW_HIDDEN);
    else
        MinimizeWindow();
}

static void
StartTrayPractice(InbeApp *app, int practice_id)
{
    const PracticeDefinition *practice;

    if(app == NULL)
        return;

    app->exercise_type = practice_clamp_id(practice_id);
    app->main_tab = APP_MAIN_TAB_PRACTICE;
    app->practice_tab = PRACTICE_TAB_PLAY;
    if(app->modal.active)
        app_close_modal(app);
    RestoreTrayWindow();

    practice = practice_get(app->exercise_type);
    if(practice->start != NULL)
        practice->start(app);
}

static void
MarkTrayHabit(InbeApp *app, int index)
{
    int today;
    InbeHabit *habit;

    if(app == NULL || index < 0 || index >= app->habits.count)
        return;

    today = habits_today_index();
    habit = &app->habits.items[index];
    if(habit->counter_enabled)
        habit_set_day_count(&app->habits, index, today,
                            habit_day_count(habit, today) + 1);
    else
        habit_set_day(&app->habits, index, today, 1);
}

static void
UpdateTrayMenuSnapshot(InbeApp *app)
{
    InbeTraySnapshot next;
    int today;

    if(app == NULL)
        return;

    memset(&next, 0, sizeof(next));
    next.window_visible = !IsWindowHidden();
    FillTraySnapshotLabels(&next);

    today = habits_today_index();
    for(int i = 0; i < app->habits.count && next.count < INBE_HABIT_MAX; i++) {
        InbeHabit *habit = &app->habits.items[i];
        int count = habit_day_count(habit, today);
        int completed = habit_completed_day(habit, today) || count > 0;

        if(completed)
            continue;

        snprintf(next.habit_labels[next.count], sizeof(next.habit_labels[next.count]),
                 "%s", habit->name);
        next.habit_indices[next.count] = i;
        next.habit_enabled[next.count] = 1;
        next.count++;
    }

    if(memcmp(&TraySnapshot, &next, sizeof(next)) != 0) {
        TraySnapshot = next;
        ApplyTrayMenuSnapshot(&TraySnapshot);
    }
}

void
inbe_desktop_tray_apply_action(InbeApp *app, InbeDesktopTrayAction action, int *quit)
{
    int habit_index = GetTrayHabitIndex(action);

    if(habit_index >= 0) {
        if(habit_index < TraySnapshot.count)
            MarkTrayHabit(app, TraySnapshot.habit_indices[habit_index]);
        return;
    }

    switch(action) {
    case INBE_DESKTOP_TRAY_ACTION_SHOW:
        RestoreTrayWindow();
        break;
    case INBE_DESKTOP_TRAY_ACTION_HIDE:
        HideTrayWindow();
        break;
    case INBE_DESKTOP_TRAY_ACTION_MINIMIZE:
        MinimizeWindow();
        break;
    case INBE_DESKTOP_TRAY_ACTION_CLOSE_REQUEST:
        app_request_desktop_close(app);
        break;
    case INBE_DESKTOP_TRAY_ACTION_START_WHM:
        StartTrayPractice(app, PRACTICE_WHM);
        break;
    case INBE_DESKTOP_TRAY_ACTION_START_MEDITATION:
        StartTrayPractice(app, PRACTICE_MEDITATION);
        break;
    case INBE_DESKTOP_TRAY_ACTION_START_SUN_SALUTATION:
        StartTrayPractice(app, PRACTICE_SUN_SALUTATION);
        break;
    case INBE_DESKTOP_TRAY_ACTION_QUIT:
        if(quit != NULL)
            *quit = 1;
        break;
    case INBE_DESKTOP_TRAY_ACTION_NONE:
    default:
        break;
    }
}

void
inbe_desktop_tray_update_status(InbeApp *app)
{
    char text[128];

    if(app == NULL)
        return;

    UpdateTrayMenuSnapshot(app);

    snprintf(text, sizeof(text), "Inner Breeze");
    if(app->inbe.screen == InbeScreenSession) {
        int count = int_from_count(app->inbe.count);
        int max_breaths = int_from_count(app->inbe.maxbreaths);

        if(app->session_paused) {
            snprintf(text, sizeof(text), "%s", GetLocaleText("tray_wim_hof_paused"));
        } else {
            switch(app->inbe.phase) {
            case InbePhaseBreathe:
                FormatLocaleText(text, sizeof(text), "tray_wim_hof_breath",
                                 count, max_breaths);
                break;
            case InbePhaseHold:
                FormatLocaleText(text, sizeof(text), "tray_wim_hof_hold", count);
                break;
            case InbePhaseRecover:
                FormatLocaleText(text, sizeof(text), "tray_wim_hof_breathe_in",
                                 count);
                break;
            case InbePhaseNext:
                snprintf(text, sizeof(text), "%s", GetLocaleText("tray_wim_hof_next_round"));
                break;
            case InbePhaseStarting:
            default:
                snprintf(text, sizeof(text), "%s", GetLocaleText("tray_wim_hof_starting"));
                break;
            }
        }
    } else if(app->inbe.screen == InbeScreenMeditation) {
        int remaining = app->meditation.remaining_seconds;
        if(remaining < 0)
            remaining = 0;
        FormatLocaleText(text, sizeof(text),
                         app->session_paused ? "tray_meditation_paused"
                                             : "tray_meditation_left",
                         remaining / 60, remaining % 60);
    } else if(app->inbe.screen == InbeScreenSunSalutation) {
        FormatLocaleText(text, sizeof(text),
                         app->session_paused ? "tray_sun_salutation_paused"
                                             : "tray_sun_salutation",
                         app->sun_salutation.step + 1,
                         app->sun_salutation.repetition + 1,
                         app->sun_salutation.repetitions);
    }

    SetDesktopTrayStatus(text);
}

#else

int inbe_desktop_tray_init(void) { return 0; }
void inbe_desktop_tray_shutdown(void) {}
InbeDesktopTrayAction inbe_desktop_tray_poll_action(void)
{
    return INBE_DESKTOP_TRAY_ACTION_NONE;
}
void inbe_desktop_tray_apply_action(InbeApp *app, InbeDesktopTrayAction action, int *quit)
{
    (void)app;
    (void)action;
    (void)quit;
}
void inbe_desktop_tray_update_status(InbeApp *app) { (void)app; }
void inbe_desktop_tray_keep_running(void) { MinimizeWindow(); }

#endif
