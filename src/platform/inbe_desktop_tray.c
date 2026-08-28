#include "inbe_desktop_tray.h"
#include "kryon.h" /* raylib API (MinimizeWindow) for the stub branch below */

#if defined(INBE_DESKTOP_TRAY_ENABLED)

#include "app.h"
#include "breaks/app_breaks.h"
#include "desktop_tray.h"
#include "habits_screen.h"
#include "practices/patterns/patterns_practice.h"
#include "practices/practice_registry.h"
#include "practices/sun_salutation/sun_salutation_practice.h"

#include <stdio.h>
#include <string.h>

typedef struct InbeTraySnapshot {
    int window_visible;
    int count;
    int break_mode;
    int break_reading_mode;
    char status_label[128];
    char show_hide_label[96];
    char start_practice_label[96];
    char habits_label[96];
    char quit_label[96];
    char whm_label[96];
    char meditation_label[96];
    char sun_salutation_label[96];
    char patterns_label[96];
    char break_label[96];
    char break_rest_now_label[96];
    char break_mode_label[96];
    char break_mode_normal_label[96];
    char break_mode_quiet_label[96];
    char break_mode_suspended_label[96];
    char break_reading_label[96];
    char break_settings_label[96];
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

static int
TraySunSalutationStepSeconds(const InbeApp *app)
{
    int start_seconds;
    int end_seconds;
    int repetitions;
    int repetition;

    if(app == NULL)
        return SUN_SALUTATION_DEFAULT_START_SECONDS;

    start_seconds = app->sun_salutation.start_seconds;
    end_seconds = app->sun_salutation.end_seconds;
    repetitions = app->sun_salutation.repetitions;
    repetition = app->sun_salutation.repetition;

    if(start_seconds < SUN_SALUTATION_SECONDS_MIN ||
       start_seconds > SUN_SALUTATION_SECONDS_MAX)
        start_seconds = SUN_SALUTATION_DEFAULT_START_SECONDS;
    if(end_seconds < SUN_SALUTATION_SECONDS_MIN ||
       end_seconds > SUN_SALUTATION_SECONDS_MAX)
        end_seconds = SUN_SALUTATION_DEFAULT_END_SECONDS;
    if(start_seconds < end_seconds)
        end_seconds = start_seconds;
    if(repetitions < 2)
        return start_seconds;
    if(repetition < 0)
        repetition = 0;
    if(repetition >= repetitions)
        repetition = repetitions - 1;

    return start_seconds + ((end_seconds - start_seconds) * repetition) /
                               (repetitions - 1);
}

static void
BuildTrayStatusText(InbeApp *app, char *text, size_t text_size)
{
    if(text == NULL || text_size == 0)
        return;

    snprintf(text, text_size, "Inner Breeze");

    if(app == NULL)
        return;

    if(app->inbe.screen == InbeScreenSession) {
        int count = int_from_count(app->inbe.count);
        int max_breaths = int_from_count(app->inbe.maxbreaths);

        if(app->session_paused) {
            snprintf(text, text_size, "%s", GetLocaleText("tray_wim_hof_paused"));
        } else {
            switch(app->inbe.phase) {
            case InbePhaseBreathe:
                FormatLocaleText(text, text_size, "tray_wim_hof_breath",
                                 count, max_breaths);
                break;
            case InbePhaseHold:
                FormatLocaleText(text, text_size, "tray_wim_hof_hold", count);
                break;
            case InbePhaseRecover:
                count = 15 - count;
                if(count < 0)
                    count = 0;
                FormatLocaleText(text, text_size, "tray_wim_hof_breathe_in",
                                 count);
                break;
            case InbePhaseNext:
                snprintf(text, text_size, "%s", GetLocaleText("tray_wim_hof_next_round"));
                break;
            case InbePhaseStarting:
            default:
                {
                    int pause = app->inbe.round == 0 ? 3 : app->inbe.pause_seconds;
                    int remaining = pause > 0 ? pause - app->inbe.sectick / 60 : 0;
                    if(remaining < 0)
                        remaining = 0;
                    FormatLocaleText(text, text_size,
                                     "tray_wim_hof_starting_seconds",
                                     remaining);
                }
                break;
            }
        }
    } else if(app->inbe.screen == InbeScreenMeditation) {
        int remaining = app->meditation.remaining_seconds;
        if(remaining < 0)
            remaining = 0;
        FormatLocaleText(text, text_size,
                         app->session_paused ? "tray_meditation_paused"
                                             : "tray_meditation_left",
                         remaining / 60, remaining % 60);
    } else if(app->inbe.screen == InbeScreenSunSalutation) {
        int step_seconds = TraySunSalutationStepSeconds(app);
        int elapsed = app->sun_salutation.step_ticks / 60;
        if(elapsed < 0)
            elapsed = 0;
        if(elapsed > step_seconds)
            elapsed = step_seconds;
        FormatLocaleText(text, text_size,
                         app->session_paused ? "tray_sun_salutation_paused"
                                             : "tray_sun_salutation",
                         sun_salutation_step_label(app->sun_salutation.step),
                         app->sun_salutation.repetition + 1,
                         app->sun_salutation.repetitions,
                         elapsed,
                         step_seconds);
    } else if(app->inbe.screen == InbeScreenPatterns && app->patterns.active) {
        int phase = app->patterns.phase;
        int left = patterns_phase_remaining_seconds(app);
        int session_time;

        if(app->patterns.duration_minutes > 0) {
            session_time = app->patterns.duration_minutes * 60 -
                           app->patterns.elapsed_seconds;
            if(session_time < 0)
                session_time = 0;
            FormatLocaleText(text, text_size,
                             app->patterns.paused ? "tray_patterns_left_paused"
                                                  : "tray_patterns_left",
                             GetLocaleText(patterns_phase_label_key(phase)),
                             left, session_time / 60, session_time % 60);
        } else {
            session_time = app->patterns.elapsed_seconds;
            if(session_time < 0)
                session_time = 0;
            FormatLocaleText(text, text_size,
                             app->patterns.paused ? "tray_patterns_elapsed_paused"
                                                  : "tray_patterns_elapsed",
                             GetLocaleText(patterns_phase_label_key(phase)),
                             left, session_time / 60, session_time % 60);
        }
    } else if(app->breaks_enabled) {
        if(app->breaks.mode == BreakModeSuspended) {
            snprintf(text, text_size, "%s", GetLocaleText("settings_breaks_mode_suspended"));
        } else if(app->breaks.mode == BreakModeQuiet) {
            snprintf(text, text_size, "%s", GetLocaleText("settings_breaks_mode_quiet"));
        } else {
            int next_break = -1;
            for(int t = 0; t < BREAK_TYPE_COUNT; t++) {
                int due = break_timer_next_due_s(&app->breaks, t);
                if(due > 0 && (next_break < 0 || due < next_break))
                    next_break = due;
            }
            if(next_break > 0) {
                char due[16];

                break_format_duration(due, sizeof(due), next_break);
                FormatLocaleText(text, text_size, "tray_next_break", due);
            }
        }
    }
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
    snprintf(snapshot->patterns_label, sizeof(snapshot->patterns_label), "%s",
             GetLocaleText("exercise_patterns"));
    snprintf(snapshot->break_label, sizeof(snapshot->break_label), "%s",
             GetLocaleText("settings_tab_breaks"));
    snprintf(snapshot->break_rest_now_label, sizeof(snapshot->break_rest_now_label), "%s",
             GetLocaleText("settings_breaks_rest_now"));
    snprintf(snapshot->break_mode_label, sizeof(snapshot->break_mode_label), "%s",
             GetLocaleText("settings_breaks_mode"));
    snprintf(snapshot->break_mode_normal_label, sizeof(snapshot->break_mode_normal_label), "%s",
             GetLocaleText("settings_breaks_mode_normal"));
    snprintf(snapshot->break_mode_quiet_label, sizeof(snapshot->break_mode_quiet_label), "%s",
             GetLocaleText("settings_breaks_mode_quiet"));
    snprintf(snapshot->break_mode_suspended_label, sizeof(snapshot->break_mode_suspended_label), "%s",
             GetLocaleText("settings_breaks_mode_suspended"));
    snprintf(snapshot->break_reading_label, sizeof(snapshot->break_reading_label), "%s",
             GetLocaleText("settings_breaks_reading_mode"));
    snprintf(snapshot->break_settings_label, sizeof(snapshot->break_settings_label), "%s",
             GetLocaleText("tray_break_settings"));
}

static void
SeedTraySnapshot(void)
{
    InbeTraySnapshot next;
    InbeApp *app = get_global_inbe_app();

    memset(&next, 0, sizeof(next));
    next.window_visible = !IsWindowHidden();
    if(app != NULL) {
        next.break_mode = app->breaks.mode;
        next.break_reading_mode = app->breaks.reading_mode;
    }
    BuildTrayStatusText(app, next.status_label, sizeof(next.status_label));
    FillTraySnapshotLabels(&next);
    TraySnapshot = next;
}

static int
BuildTrayMenu(const InbeTraySnapshot *snapshot,
              DesktopTrayMenuItem *items, int item_count,
              DesktopTrayMenuItem *start_items, int start_item_count,
              DesktopTrayMenuItem *habit_items, int habit_item_count,
              DesktopTrayMenuItem *break_items, int break_item_count,
              DesktopTrayMenuItem *break_mode_items, int break_mode_item_count)
{
    InbeTraySnapshot local_snapshot;
    int item_index = 0;

    if(items == NULL || item_count < 7 ||
       start_items == NULL || start_item_count < 4 ||
       habit_items == NULL || habit_item_count < INBE_HABIT_MAX ||
       break_items == NULL || break_item_count < 4 ||
       break_mode_items == NULL || break_mode_item_count < 3)
        return 0;

    memset(&local_snapshot, 0, sizeof(local_snapshot));
    FillTraySnapshotLabels(&local_snapshot);
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
    start_items[3] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.patterns_label,
        .action = INBE_DESKTOP_TRAY_ACTION_START_PATTERNS,
        .enabled = 1
    };

    for(int i = 0; i < local_snapshot.count && i < INBE_HABIT_MAX &&
                i < 10; i++) {
        habit_items[i] = (DesktopTrayMenuItem){
            .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
            .label = local_snapshot.habit_labels[i],
            .action = GetTrayHabitAction(i),
            .enabled = local_snapshot.habit_enabled[i]
        };
    }

    break_mode_items[0] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.break_mode_normal_label,
        .action = INBE_DESKTOP_TRAY_ACTION_BREAK_MODE_NORMAL,
        .enabled = local_snapshot.break_mode != BreakModeNormal
    };
    break_mode_items[1] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.break_mode_quiet_label,
        .action = INBE_DESKTOP_TRAY_ACTION_BREAK_MODE_QUIET,
        .enabled = local_snapshot.break_mode != BreakModeQuiet
    };
    break_mode_items[2] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.break_mode_suspended_label,
        .action = INBE_DESKTOP_TRAY_ACTION_BREAK_MODE_SUSPENDED,
        .enabled = local_snapshot.break_mode != BreakModeSuspended
    };

    break_items[0] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.break_rest_now_label,
        .action = INBE_DESKTOP_TRAY_ACTION_BREAK_REST_NOW,
        .enabled = 1
    };
    break_items[1] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_SUBMENU,
        .label = local_snapshot.break_mode_label,
        .enabled = 1,
        .children = break_mode_items,
        .child_count = 3
    };
    break_items[2] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.break_reading_label,
        .action = INBE_DESKTOP_TRAY_ACTION_BREAK_READING_TOGGLE,
        .enabled = 1
    };
    break_items[3] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.break_settings_label,
        .action = INBE_DESKTOP_TRAY_ACTION_BREAK_SETTINGS,
        .enabled = 1
    };

    items[item_index++] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_ACTION,
        .label = local_snapshot.status_label[0] != '\0'
                     ? local_snapshot.status_label
                     : "Inner Breeze",
        .enabled = 0
    };
    items[item_index++] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_SEPARATOR
    };
    items[item_index++] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_SUBMENU,
        .label = local_snapshot.start_practice_label,
        .enabled = 1,
        .children = start_items,
        .child_count = 4
    };
    items[item_index++] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_SUBMENU,
        .label = local_snapshot.habits_label,
        .enabled = local_snapshot.count > 0,
        .children = habit_items,
        .child_count = local_snapshot.count
    };
    items[item_index++] = (DesktopTrayMenuItem){
        .kind = DESKTOP_TRAY_MENU_ITEM_SUBMENU,
        .label = local_snapshot.break_label,
        .enabled = 1,
        .children = break_items,
        .child_count = 4
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
    DesktopTrayMenuItem items[7];
    DesktopTrayMenuItem start_items[4];
    DesktopTrayMenuItem habit_items[INBE_HABIT_MAX];
    DesktopTrayMenuItem break_items[4];
    DesktopTrayMenuItem break_mode_items[3];
    int count;

    memset(items, 0, sizeof(items));
    memset(start_items, 0, sizeof(start_items));
    memset(habit_items, 0, sizeof(habit_items));
    memset(break_items, 0, sizeof(break_items));
    memset(break_mode_items, 0, sizeof(break_mode_items));

    count = BuildTrayMenu(snapshot, items, 7, start_items, 4,
                          habit_items, INBE_HABIT_MAX,
                          break_items, 4, break_mode_items, 3);
    SetDesktopTrayMenu(items, count);
    SetDesktopTrayActivateAction(GetTrayWindowAction(snapshot != NULL
                                                         ? snapshot->window_visible
                                                         : !IsWindowHidden()));
}

int
inbe_desktop_tray_init(void)
{
    DesktopTraySpec spec;
    DesktopTrayMenuItem items[7];
    DesktopTrayMenuItem start_items[4];
    DesktopTrayMenuItem habit_items[INBE_HABIT_MAX];
    DesktopTrayMenuItem break_items[4];
    DesktopTrayMenuItem break_mode_items[3];
    int item_count;

    /* INBE_NO_TRAY=1 skips the tray entirely. With the lazy-GTK tray build
     * this also keeps libgtk-3 and its dependency chain out of the process;
     * closing the window then quits or minimizes instead of hiding. */
    if(getenv("INBE_NO_TRAY") != NULL)
        return 0;

    SeedTraySnapshot();
    memset(items, 0, sizeof(items));
    memset(start_items, 0, sizeof(start_items));
    memset(habit_items, 0, sizeof(habit_items));
    memset(break_items, 0, sizeof(break_items));
    memset(break_mode_items, 0, sizeof(break_mode_items));
    item_count = BuildTrayMenu(&TraySnapshot, items, 7, start_items, 4,
                               habit_items, INBE_HABIT_MAX,
                               break_items, 4, break_mode_items, 3);

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

int
inbe_desktop_tray_ready(void)
{
    return TrayReady;
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
    /* The window vanishing with the process still running is the single
     * most confusing failure mode this app has; always say why it hid. */
    TraceLog(LOG_INFO, "INBE: hiding main window (tray action)");
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

    if(app == NULL || index < 0 || index >= app->habits.count ||
       index >= INBE_HABIT_MAX)
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
    next.break_mode = app->breaks.mode;
    next.break_reading_mode = app->breaks.reading_mode;
    BuildTrayStatusText(app, next.status_label, sizeof(next.status_label));
    FillTraySnapshotLabels(&next);

    today = habits_today_index();
    for(int i = 0; i < app->habits.count && i < INBE_HABIT_MAX &&
                   next.count < INBE_HABIT_MAX; i++) {
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
    case INBE_DESKTOP_TRAY_ACTION_START_PATTERNS:
        StartTrayPractice(app, PRACTICE_PATTERNS);
        break;
    case INBE_DESKTOP_TRAY_ACTION_QUIT:
        if(quit != NULL)
            *quit = 1;
        break;
    case INBE_DESKTOP_TRAY_ACTION_BREAK_REST_NOW:
        if(app->breaks_enabled)
            app_breaks_rest_break_now(app);
        break;
    case INBE_DESKTOP_TRAY_ACTION_BREAK_MODE_NORMAL:
        app_breaks_set_mode(app, BreakModeNormal, 0);
        break;
    case INBE_DESKTOP_TRAY_ACTION_BREAK_MODE_QUIET:
        app_breaks_set_mode(app, BreakModeQuiet, 0);
        break;
    case INBE_DESKTOP_TRAY_ACTION_BREAK_MODE_SUSPENDED:
        app_breaks_set_mode(app, BreakModeSuspended, 0);
        break;
    case INBE_DESKTOP_TRAY_ACTION_BREAK_READING_TOGGLE:
        app->breaks.reading_mode = !app->breaks.reading_mode;
        app->settings_dirty = 1;
        break;
    case INBE_DESKTOP_TRAY_ACTION_BREAK_SETTINGS:
        if(app->modal.active)
            app_close_modal(app);
        RestoreTrayWindow();
        app->settings_tab = SETTINGS_TAB_BREAKS;
        app_switch_screen(app, InbeScreenSettings);
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

    BuildTrayStatusText(app, text, sizeof(text));
    SetDesktopTrayStatus(text);
}

#else

int inbe_desktop_tray_init(void) { return 0; }
int inbe_desktop_tray_ready(void) { return 0; }
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
