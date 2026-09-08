#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define MMNOSOUND
#define NOMINMAX
#endif

#include "app.h"
#include "src/screens/elist_screen.h"
#include "app_internal.h"
#include "app_runtime.h"
#include "app/app_sync.h"
#include "app/app_route_state.h"
#include "app/app_route_url.h"
#include "app/app_settings.h"
#include "app/app_update_check.h"
#include "data.h"
#include "app/app_notifications.h"
#include "platform/app_desktop_tray.h"
#include "screens/language_screen.h"
#include "screens/manual_screen.h"
#include "screens/break_overlay.h"
#include "breaks/app_breaks.h"
#include "screens/profile_screen.h"
#include "screens/profile_social.h"
#include "screens/settings/settings_screen.h"
#include "screens/settings/settings_data_ui.h"
#include "screens/settings/settings_sync_account_impl.h"
#include "screens/settings/settings_theme.h"
#include "screens/practice_screen.h"
#include "practices/practice_registry.h"
#include "screens/session_results.h"
#include "practices/patterns/patterns_practice.h"
#include "app/device_preferences.h"
#include "storage.h"
#include "practices/meditation/meditation_practice.h"
#include "practices/whm/whm_session.h"
#include "sync_account.h"
#include "../../vendor/kryon/src/ui/ui_internal.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#if ANDROID_BUILD
#include "android_wakelock.h"
#include "android_device.h"
#include "android_insets.h"
#endif

#if defined(PLATFORM_WEB) || ANDROID_BUILD
#define APP_DEFAULT_WIDTH 320
#define APP_DEFAULT_HEIGHT 560
#else
#define APP_DEFAULT_WIDTH 900
#define APP_DEFAULT_HEIGHT 720
#endif

typedef struct AppProfileStats {
    int initialized;
    int enabled;
    int frames;
    double frame_total;
    double frame_max;
    double update_total;
    double update_max;
    double habits_total;
    double habits_max;
    double sync_total;
    double sync_max;
} AppProfileStats;

static AppProfileStats g_app_profile;

int
TextButton(int id, int x, int y, const char *label, int *hover)
{
    (void)id;
    return RenderTextButton(x, y, label, hover);
}

void
ReadonlyTextBox(ReadonlyTextBoxProps props)
{
    DrawUIReadonlyTextBox(props);
}

static void app_restore_habits_view_settings(InnerBreeze*app);

static int
app_profile_enabled(void)
{
    if(!g_app_profile.initialized) {
        const char *env = getenv("APP_PROFILE");
        g_app_profile.enabled = env != NULL && env[0] != '\0' && env[0] != '0';
        g_app_profile.initialized = 1;
    }
    return g_app_profile.enabled;
}

static double
app_profile_now(void)
{
    return app_profile_enabled() ? GetTime() : 0.0;
}

const char *
app_bitcoin_donation_address(void)
{
    return "bc1qxzcetg50f6epgddc09n82xqn3zswlmk44235y5";
}

const char *
app_monero_donation_address(void)
{
    return "86CbC3d4a2GhT9auh6X99JhmhTMFKVVk8Q9cLrKTHkBu8LLkoNWgkBeAT3YZrvDM6NczYe8brUJNsTiFmwpWDZYnFG5kzSH";
}

const char *
app_bitcoin_wallet_url(void)
{
    return "bitcoin:bc1qxzcetg50f6epgddc09n82xqn3zswlmk44235y5"
           "?amount=0.001";
}

const char *
app_monero_wallet_url(void)
{
    return "monero:86CbC3d4a2GhT9auh6X99JhmhTMFKVVk8Q9cLrKTHkBu8LLkoNWgkBeAT3YZrvDM6NczYe8brUJNsTiFmwpWDZYnFG5kzSH"
           "?tx_amount=0.1";
}

const char *
app_bitcoin_trocador_url(void)
{
    return "https://trocador.app/en/anonpay/?ticker_to=btc&network_to=Mainnet"
           "&address=bc1qxzcetg50f6epgddc09n82xqn3zswlmk44235y5"
           "&donation=True&simple_mode=True&amount=0.001&name=Inner+Breeze"
           "&email=waotzi@proton.me&ticker_from=btc&network_from=Mainnet"
           "&buttonbgcolor=23657d&textcolor=fffdf8&bgcolor=f3f1eaff";
}

const char *
app_monero_trocador_url(void)
{
    return "https://trocador.app/en/anonpay/?ticker_to=xmr&network_to=Mainnet"
           "&address=86CbC3d4a2GhT9auh6X99JhmhTMFKVVk8Q9cLrKTHkBu8LLkoNWgkBeAT3YZrvDM6NczYe8brUJNsTiFmwpWDZYnFG5kzSH"
           "&donation=True&simple_mode=True&amount=0.1&name=Inner+Breeze"
           "&email=waotzi@proton.me&ticker_from=xmr&network_from=Mainnet"
           "&buttonbgcolor=23657d&textcolor=fffdf8&bgcolor=f3f1eaff";
}

const char *
app_bitcoin_donation_url(void)
{
#if defined(PLATFORM_WEB)
    return app_bitcoin_trocador_url();
#else
    return app_bitcoin_wallet_url();
#endif
}

const char *
app_monero_donation_url(void)
{
#if defined(PLATFORM_WEB)
    return app_monero_trocador_url();
#else
    return app_monero_wallet_url();
#endif
}

const char *
app_donation_url(void)
{
    return app_monero_donation_url();
}

static void
app_save_donation_reminder(InnerBreeze*app)
{
    if(app != NULL)
        save_settings(app);
}

static void
app_record_donation_reminder_seen(InnerBreeze*app)
{
    int practice_count;

    if(app == NULL)
        return;
    practice_count = storage_session_count();
    if(practice_count < DONATION_REMINDER_PRACTICE_INTERVAL)
        practice_count = DONATION_REMINDER_PRACTICE_INTERVAL;
    app->donation_reminder_last_prompt_practice_count = practice_count;
    app->donation_reminder_next_prompt_practice_count =
        practice_count + DONATION_REMINDER_PRACTICE_INTERVAL;
    app_save_donation_reminder(app);
}

static int
app_donation_reminder_practice_home(const InnerBreeze*app)
{
    return app != NULL &&
           app->breathing.screen == ScreenStart &&
           app->main_tab == APP_MAIN_TAB_PRACTICE &&
           app->practice_tab == PRACTICE_TAB_PLAY;
}

static void
app_init_donation_reminder_observed_practice_count(InnerBreeze*app)
{
    if(app == NULL ||
       app->donation_reminder_observed_practice_count_initialized)
        return;
    app->donation_reminder_observed_practice_count = storage_session_count();
    app->donation_reminder_observed_practice_count_initialized = 1;
}

static int
app_donation_reminder_next_threshold(const InnerBreeze*app, int observed_count)
{
    int next_prompt_count;

    if(app == NULL)
        return DONATION_REMINDER_PRACTICE_INTERVAL;
    next_prompt_count = app->donation_reminder_last_prompt_practice_count +
                        DONATION_REMINDER_PRACTICE_INTERVAL;
    if(next_prompt_count < DONATION_REMINDER_PRACTICE_INTERVAL)
        next_prompt_count = DONATION_REMINDER_PRACTICE_INTERVAL;
    while(next_prompt_count <= observed_count)
        next_prompt_count += DONATION_REMINDER_PRACTICE_INTERVAL;
    return next_prompt_count;
}

static void
app_handle_donation_reminder_action(InnerBreeze*app, int action)
{
    if(app == NULL)
        return;
    switch(action) {
    case 1:
        app_record_donation_reminder_seen(app);
        app_open_modal(app, UIModalAboutDonation);
        break;
    case 2:
        app_record_donation_reminder_seen(app);
        app_close_modal(app);
        break;
    case 3:
        app->donation_reminder_dismissed = 1;
        app_record_donation_reminder_seen(app);
        app_close_modal(app);
        break;
    default:
        break;
    }
}

static void
app_profile_accum(double *total, double *max_value, double start)
{
    double elapsed;

    if(!app_profile_enabled() || start <= 0.0)
        return;
    elapsed = (GetTime() - start) * 1000.0;
    *total += elapsed;
    if(elapsed > *max_value)
        *max_value = elapsed;
}

static void
app_profile_frame_end(double frame_start)
{
    double elapsed;

    if(!app_profile_enabled() || frame_start <= 0.0)
        return;
    elapsed = (GetTime() - frame_start) * 1000.0;
    g_app_profile.frame_total += elapsed;
    if(elapsed > g_app_profile.frame_max)
        g_app_profile.frame_max = elapsed;
    g_app_profile.frames++;
    if(g_app_profile.frames >= 120) {
        TraceLog(LOG_INFO,
                 "PROFILE: frame avg=%.2f max=%.2f update avg=%.2f max=%.2f habits avg=%.2f max=%.2f sync avg=%.2f max=%.2f",
                 g_app_profile.frame_total / g_app_profile.frames,
                 g_app_profile.frame_max,
                 g_app_profile.update_total / g_app_profile.frames,
                 g_app_profile.update_max,
                 g_app_profile.habits_total / g_app_profile.frames,
                 g_app_profile.habits_max,
                 g_app_profile.sync_total / g_app_profile.frames,
                 g_app_profile.sync_max);
        memset(&g_app_profile, 0, sizeof(g_app_profile));
        g_app_profile.initialized = 1;
        g_app_profile.enabled = 1;
    }
}

static void
app_update_frame_pacing(InnerBreeze*app)
{
#if ANDROID_BUILD
    int active = 0;

    if(app != NULL &&
       (practice_active(app) != NULL ||
        app->modal.active ||
        app->file_dialog_active ||
        IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT))) {
        active = 1;
    }
    SetFramePacingActive(active);
    UpdateFramePacing();
#else
    (void)app;
#endif
}

AppConfig config = {
    .title = "Inner Breeze",
    .width = APP_DEFAULT_WIDTH,
    .height = APP_DEFAULT_HEIGHT,
    .loaded = 0,
    .title_custom = 1
};

int view_width = APP_DEFAULT_WIDTH;
int view_height = APP_DEFAULT_HEIGHT;
static int app_full_view_width = APP_DEFAULT_WIDTH;
static InnerBreeze*g_app_ptr;
/* Theme colors are now accessed via theme accessor functions */

InnerBreeze*
get_global_app(void)
{
    return g_app_ptr;
}

void
set_global_app(InnerBreeze*app)
{
    g_app_ptr = app;
    TraceLog(LOG_INFO, "APP: Global app pointer set to %p", app);
}

void *
CreateApp(const char *project_path)
{
    InnerBreeze*app;

    app = calloc(1, sizeof(*app));
    if(app == NULL)
        return NULL;
    if(project_path != NULL && project_path[0] != '\0')
        ChangeDirectory(project_path);
    app_init(app);
    set_global_app(app);
    return app;
}

void
DestroyApp(void *vapp)
{
    InnerBreeze*app = vapp;

    if(app == NULL)
        return;
    app_destroy(app);
    free(app);
    set_global_app(NULL);
}

void
ApplyRoute(void *vapp, const AppRouteInfo *route_info)
{
    InnerBreeze*app = vapp;
    AppRoute route;
    int screen;

    if(app == NULL || route_info == NULL || route_info->id == NULL)
        return;
    if(strcmp(route_info->id, "session") == 0) {
        session_start(app);
        return;
    }
    screen = screen_for_route_id(route_info->id);
    if(screen < 0)
        return;
    route = app_current_route(app);
    route.screen = screen;
    app_switch_route(app, route);
}

void
BeginScreenDraw(void *vapp, Rectangle viewport)
{
    (void)vapp;
    view_width = (int)viewport.width;
    view_height = (int)viewport.height;
}

int
app_draw_close_title_bar(InnerBreeze*app, const char *title, int height)
{
    int hover = 0;
    int button_size = Scale(22);
    int padding = Scale(11);
    int button_total = button_size + padding * 2;
    int x = view_width - button_total - Scale(8);
    int y = (height - button_total) / 2;

    if(y < 0)
        y = 0;
    TitleBar(title, height);
    if(app != NULL && !app->modal.active &&
       PaddedIconBtn(0, x, y, button_size, padding,
                           app->icons[UI_ICON_TYPE_X], &hover))
        return 1;
    return 0;
}

int
app_scaffold_close_title(const char *title, int height, void *user_data)
{
    return app_draw_close_title_bar((InnerBreeze*)user_data, title, height);
}

int
app_draw_close_dropdown_title_bar(InnerBreeze*app, UITitleBarDropdown dropdown,
                                  int height)
{
    int hover = 0;
    int button_size = Scale(22);
    int padding = Scale(11);
    int button_total = button_size + padding * 2;
    int close_x = view_width - button_total - Scale(8);
    int close_y = (height - button_total) / 2;
    int dropdown_x = Scale(8);
    int dropdown_h = dropdown.height > 0 ? dropdown.height : Scale(32);
    int dropdown_y = (height - dropdown_h) / 2;
    int dropdown_w = close_x - dropdown_x - Scale(8);

    if(close_y < 0)
        close_y = 0;
    if(dropdown_y < 0)
        dropdown_y = 0;
    if(dropdown_w < 1)
        dropdown_w = 1;
    DrawRectangle(0, 0, view_width, height, GetThemeBackground());
    DrawLine(0, height - 1, view_width, height - 1,
             DarkenUIColor(GetThemeButton(), 18));
    Dropdown(dropdown.id, dropdown_x, dropdown_y, dropdown_w,
                         dropdown_h, dropdown.options, dropdown.option_count,
                         dropdown.selected_index);
    if(app != NULL && !app->modal.active &&
       PaddedIconBtn(0, close_x, close_y, button_size, padding,
                           app->icons[UI_ICON_TYPE_X], &hover))
        return 1;
    return 0;
}

void
app_leave_practice_config(InnerBreeze*app)
{
    if(app == NULL)
        return;
    if(app->settings_dirty)
        save_settings(app);
    {
        const PracticeDefinition *practice = practice_get(app->exercise_type);
        if(practice->leave_config != NULL)
            practice->leave_config(app);
    }
    app->settings_scroll = 0;
}

static void
app_flush_deferred_settings(InnerBreeze*app)
{
    if(app == NULL || app->settings_save_delay_ticks <= 0)
        return;

    app->settings_save_delay_ticks--;
    if(app->settings_save_delay_ticks <= 0 && app->settings_dirty)
        save_settings(app);
}

static int
app_habits_save_pending(const InnerBreeze*app)
{
    return app != NULL &&
           (app->habits.dirty || app->habits.pending_day_save_count > 0);
}

static void
app_flush_habits_post_frame(void *userdata)
{
    InnerBreeze*app = userdata;

    if(app == NULL)
        return;
    app->habits_flush_post_frame_scheduled = 0;
    habits_flush_save(app);
}

static void
app_schedule_habits_post_frame_flush(InnerBreeze*app)
{
    if(!app_habits_save_pending(app) ||
       app->habits_flush_post_frame_scheduled)
        return;
    app->habits_flush_post_frame_scheduled = 1;
    if(!SchedulePostFrameCallback(app_flush_habits_post_frame, app)) {
        app->habits_flush_post_frame_scheduled = 0;
        habits_flush_save(app);
    }
}

int
app_toolbar_height(void)
{
    return Scale(58);
}

int
app_content_top_reserved(const InnerBreeze*app)
{
    TabBarProps tabs;

    if(app != NULL && app->breathing.screen == ScreenStart) {
        memset(&tabs, 0, sizeof(tabs));
        return GetNodeHeight(NodeTabBar(tabs));
    }
    return app_toolbar_height();
}

void
app_block_current_click(InnerBreeze*app)
{
    if(app != NULL)
        app->blocked_input_frame = app->breathing.frame;
}

void
app_open_modal(InnerBreeze*app, UIModalType type)
{
    if(app == NULL)
        return;
    app->modal.active = 1;
    app->modal.type = type;
    app_block_current_click(app);
}

static void
app_clear_modal_state(InnerBreeze*app)
{
    if(app == NULL)
        return;
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app_block_current_click(app);
}

void
app_close_modal(InnerBreeze*app)
{
    UIModalType type;

    if(app == NULL)
        return;
    type = app->modal.type;
    if(type == UIModalEditProgressiveStartSpeed &&
       app->breathing.screen == ScreenStart &&
       app->practice_tab == PRACTICE_TAB_CONFIG) {
        app->modal.type = UIModalPracticeConfig;
        app_block_current_click(app);
        return;
    }
    if(type == UIModalPracticeConfig) {
        app_leave_practice_config(app);
        app->settings_scroll = 0;
        app->practice_tab = PRACTICE_TAB_PLAY;
    } else if(type == UIModalPracticeManual) {
        app->manual_scroll = 0;
        app->tutorial_step = 0;
        app->practice_tab = PRACTICE_TAB_PLAY;
    }
    app_clear_modal_state(app);
}

static int
app_screen_local_modal_valid(const InnerBreeze*app, UIModalType type)
{
    if(app == NULL)
        return 0;

    switch(type) {
    case UIModalConfirmExitSession:
        return app->breathing.screen == ScreenSession ||
               app->breathing.screen == ScreenMeditation ||
               app->breathing.screen == ScreenSunSalutation ||
               app->breathing.screen == ScreenPatterns;
    case UIModalMeditationSetup:
        return app->breathing.screen == ScreenStart;
    case UIModalConfirmDeleteHabit:
    case UIModalHabitPracticePicker:
        return app->breathing.screen == ScreenHabitEdit ||
               (app->breathing.screen == ScreenHabits &&
                app->habits.tab == HABIT_TAB_EDIT);
    case UIModalBottomNavConfig:
        return app->breathing.screen == ScreenCustomizeNav;
    default:
        return 1;
    }
}

static void
app_clear_invalid_screen_local_modal(InnerBreeze*app)
{
    if(app == NULL || !app->modal.active)
        return;
    if(app_screen_local_modal_valid(app, app->modal.type))
        return;
    app_clear_modal_state(app);
}

static int
app_donation_reminder_safe(const InnerBreeze*app, int first_run_guide_active,
                           int habits_guide_active)
{
    if(app == NULL)
        return 0;
    if(app->donation_reminder_dismissed)
        return 0;
    if(app->modal.active || app->close_prompt_open || app->nav_sidebar_open)
        return 0;
    if(first_run_guide_active || habits_guide_active)
        return 0;
    if(app->file_dialog_active || app->backgrounded)
        return 0;
    if(app->habit_edit.active || app->habit_session_edit.active)
        return 0;
    if(app->sync_server_url_focused || app->sync_alias_focused ||
       app->profile_friend_input_focused || app->profile_name_focused ||
       app->profile_intention_focused)
        return 0;

    switch(app->breathing.screen) {
    case ScreenSession:
    case ScreenMeditation:
    case ScreenSunSalutation:
    case ScreenPatterns:
    case ScreenResults:
    case ScreenBreak:
    case ScreenHabitEdit:
    case ScreenHabitSessionEdit:
        return 0;
    default:
        break;
    }
    return 1;
}

static void
app_maybe_open_donation_reminder(InnerBreeze*app, int first_run_guide_active,
                                 int habits_guide_active)
{
    int practice_count;
    int next_prompt_count;

    if(!app_donation_reminder_safe(app, first_run_guide_active,
                                   habits_guide_active))
        return;
    practice_count = storage_session_count();
    if(!app->donation_reminder_observed_practice_count_initialized) {
        app->donation_reminder_observed_practice_count = practice_count;
        app->donation_reminder_observed_practice_count_initialized = 1;
        return;
    }
    if(practice_count < app->donation_reminder_observed_practice_count) {
        app->donation_reminder_observed_practice_count = practice_count;
        return;
    }
    if(practice_count <= app->donation_reminder_observed_practice_count)
        return;
    if(!app_donation_reminder_practice_home(app))
        return;
    next_prompt_count = app_donation_reminder_next_threshold(
        app, app->donation_reminder_observed_practice_count);
    if(app->donation_reminder_next_prompt_practice_count != next_prompt_count) {
        app->donation_reminder_next_prompt_practice_count = next_prompt_count;
        app_save_donation_reminder(app);
    }
    if(practice_count < next_prompt_count) {
        app->donation_reminder_observed_practice_count = practice_count;
        return;
    }
    app->donation_reminder_observed_practice_count = practice_count;
    app_open_modal(app, UIModalDonationReminder);
}

void
app_request_desktop_close(InnerBreeze*app)
{
    if(app == NULL)
        return;
    /* Configured close behaviour overrides the ask prompt (desktop only). */
    if(app->desktop_close_action == CLOSE_QUIT) {
        app->request_quit = 1;
        return;
    }
    if(app->desktop_close_action == CLOSE_KEEP_RUNNING) {
#if defined(DESKTOP_TRAY_ENABLED)
        desktop_tray_keep_running();
#else
        MinimizeWindow();
#endif
        return;
    }
    app->close_prompt_open = 1;
    app_block_current_click(app);
    app->close_prompt_result = AppClosePromptNone;
}

/*
 * Desktop quit request from a keyboard shortcut (Ctrl+Q) or Esc at the home
 * screen. Tray builds route through the keep-running/quit prompt; no-tray
 * builds have nothing to keep running, so they exit immediately (matching the
 * window-close button behaviour).
 */
void
app_request_desktop_quit(InnerBreeze*app)
{
    if(app == NULL)
        return;
#if defined(DESKTOP_TRAY_ENABLED)
    app_request_desktop_close(app);
#else
    app->request_quit = 1;
#endif
}

AppClosePromptResult
app_consume_close_prompt_result(InnerBreeze*app)
{
    AppClosePromptResult result;

    if(app == NULL)
        return AppClosePromptNone;
    result = app->close_prompt_result;
    app->close_prompt_result = AppClosePromptNone;
    return result;
}

SessionExitModalResult
app_draw_session_exit_modal(int can_save, const char *save_message,
                            const char *discard_message)
{
    int modal_result;

    if(can_save) {
        modal_result = Modal3Button(GetLocaleText("exit_session_title"),
                                          save_message,
                                          GetLocaleText("cancel_button"),
                                          GetLocaleText("save_button"),
                                          GetLocaleText("discard_button"));
        if(modal_result == 2)
            return SessionExitModalSave;
        if(modal_result == 3)
            return SessionExitModalDiscard;
    } else {
        modal_result = Modal(GetLocaleText("exit_session_title"),
                                     discard_message,
                                     GetLocaleText("cancel_button"),
                                     GetLocaleText("exit_button"));
        if(modal_result == 2)
            return SessionExitModalDiscard;
    }
    return modal_result == 1 ? SessionExitModalCancel : SessionExitModalNone;
}

static void
app_draw_close_prompt(InnerBreeze*app)
{
    int modal_result;
    ModalAction actions[2];
    ModalProps props;

    if(app == NULL || !app->close_prompt_open)
        return;
    if(app->blocked_input_frame == app->breathing.frame)
        return;

    ClearUIInputCaptures();
    memset(actions, 0, sizeof(actions));
    actions[0].label = GetLocaleText("desktop_close_keep_running_button");
    actions[0].style = ButtonStylePrimary;
    actions[1].label = GetLocaleText("desktop_close_quit_button");
    actions[1].style = ButtonStyleDanger;
    memset(&props, 0, sizeof(props));
    props.title = GetLocaleText("desktop_close_prompt_title");
    props.message = GetLocaleText("desktop_close_prompt_message");
    props.actions = actions;
    props.action_count = 2;
    props.close_icon = app->icons[UI_ICON_TYPE_X];
    props.max_width = 420;
    modal_result = ActionModal(props);
    if(modal_result == -1) {
        app->close_prompt_open = 0;
    } else if(modal_result == 1) {
        app->close_prompt_open = 0;
        app->close_prompt_result = AppClosePromptKeepRunning;
    } else if(modal_result == 2) {
        app->close_prompt_open = 0;
        app->close_prompt_result = AppClosePromptQuit;
    }
}

static int
exercise_manual_seen_bit(int exercise_type)
{
    if(exercise_type < 0 || exercise_type >= EXERCISE_COUNT)
        return 0;
    return 1 << exercise_type;
}

int
exercise_manual_seen(InnerBreeze*app, int exercise_type)
{
    int bit = exercise_manual_seen_bit(exercise_type);
    if(app == NULL || bit == 0)
        return 1;
    return (app->exercise_manual_seen_mask & bit) != 0;
}

void
mark_exercise_manual_seen(InnerBreeze*app, int exercise_type)
{
    int bit = exercise_manual_seen_bit(exercise_type);
    if(app == NULL || bit == 0)
        return;
    if((app->exercise_manual_seen_mask & bit) == 0) {
        app->exercise_manual_seen_mask |= bit;
        save_settings(app);
    }
}

static void
app_reload_graphics_resources(InnerBreeze*app)
{
    int i;
    Texture2D empty;

    if(app == NULL || !app->graphics_reload_requested)
        return;

    app->graphics_reload_requested = 0;
    if(app_running_in_kryon_preview())
        return;

    TraceLog(LOG_INFO, "ANDROID: Reloading graphics resources");

    memset(&empty, 0, sizeof(empty));
    for(i = 0; i < UI_ICON_TYPE_COUNT; i++)
        app->icons[i] = empty;
    LoadAllUIIconTextures(app->icons);
    SetUIIcons(app->icons[UI_ICON_TYPE_GEAR], app->icons[UI_ICON_TYPE_X]);

    app->easteregg_art = empty;
    app->easteregg_waozi = empty;
    app->font_shapes_texture = empty;
    discard_locale_font_cpu(app);
    if(!load_locale_font(app))
        TraceLog(LOG_WARNING, "FONT: Failed to reload Noto UI font");
}

void
refresh_locale_dependent_text(InnerBreeze*app)
{
    if(app == NULL)
        return;

    if(!config.title_custom) {
        snprintf(config.title, sizeof(config.title), "%s", GetLocaleText("app_title"));
    }
    manual_screen_reset_layouts(app);
    app->language_index = GetCurrentLocaleIndex();
    if(app->language_index < 0)
        app->language_index = 0;
}

static void
apply_language_code(InnerBreeze*app, const char *code)
{
    if(app == NULL)
        return;

    if(code == NULL || code[0] == '\0')
        code = "en";

    if(!SetLocale(code)) {
        code = "en";
        SetLocale(code);
    }

    snprintf(app->language, sizeof(app->language), "%s", code);
    if(!load_locale_font(app))
        TraceLog(LOG_WARNING, "FONT: Failed to load Noto UI font for locale %s", code);
    refresh_locale_dependent_text(app);
}

void
apply_system_language_selection(InnerBreeze*app, int save_now)
{
    if(app == NULL)
        return;

    app->language_system = 1;
    app->language_selected = 1;
    apply_language_code(app, GetDefaultLocaleCode());

    if(save_now)
        save_settings(app);
}

void
apply_language_selection(InnerBreeze*app, int language_index, int save_now)
{
    const char *code;

    if(app == NULL)
        return;

    if(language_index < 0 || language_index >= GetLocaleCount())
        language_index = 0;

    code = GetLocaleCode(language_index);
    if(code == NULL || code[0] == '\0')
        code = "en";

    app->language_system = 0;
    app->language_selected = 1;
    apply_language_code(app, code);

    if(save_now)
        save_settings(app);
}

void
app_accept_language_selection(InnerBreeze*app)
{
    int first_language_setup;

    if(app == NULL)
        return;

    first_language_setup = storage_get_setting_int("language_setup_done", 0) == 0;

    if(app->language_system || !app->language_selected)
        apply_system_language_selection(app, 1);
    else
        save_settings(app);
#if defined(PLATFORM_WEB)
    (void)sync_web_storage_critical();
#endif
    if(first_language_setup && habits_seed_default_set_if_needed(&app->habits))
        app_restore_habits_view_settings(app);
}

static void
load_config(void)
{
    if(config.loaded)
        return;

    if(config.title[0] == '\0') {
        snprintf(config.title, sizeof(config.title), "%s", GetLocaleText("app_title"));
        config.title_custom = 0;
    }

#if defined(PLATFORM_WEB)
    refresh_theme_colors(THEME_SKY, 1);
#else
    refresh_theme_colors(THEME_SKY, 0);
#endif

    config.loaded = 1;
}

int
clampi(int value, int min, int max)
{
    if(value < min)
        return min;
    if(value > max)
        return max;
    return value;
}

void
count_from_int(char dst[CountSize], int value)
{
    value = clampi(value, 0, 999);
    dst[0] = (char)('0' + (value / 100) % 10);
    dst[1] = (char)('0' + (value / 10) % 10);
    dst[2] = (char)('0' + value % 10);
    dst[3] = 0;
}

int
int_from_count(const char src[CountSize])
{
    int a = (src[0] >= '0' && src[0] <= '9') ? src[0] - '0' : 0;
    int b = (src[1] >= '0' && src[1] <= '9') ? src[1] - '0' : 0;
    int c = (src[2] >= '0' && src[2] <= '9') ? src[2] - '0' : 0;
    return a * 100 + b * 10 + c;
}

static void
app_restore_habits_view_settings(InnerBreeze*app)
{
    const char *selected_id;
    int screen_mode;
    int habit_tab;
    int view_mode;
    int i;

    if(app == NULL)
        return;

    screen_mode = storage_get_setting_int("habits_screen_mode", HABITS_SCREEN_OVERVIEW);
    habit_tab = storage_get_setting_int("habits_tab", HABIT_TAB_WEEKLY);
    view_mode = storage_get_setting_int("habits_view_mode", HABIT_VIEW_WEEKLY);
    app->habits.screen_mode = clampi(screen_mode, HABITS_SCREEN_OVERVIEW,
                                     HABITS_SCREEN_STATISTICS);
    if(app->habits.screen_mode == HABITS_SCREEN_REORDER)
        app->habits.screen_mode = HABITS_SCREEN_OVERVIEW;
    app->habits.tab = clampi(habit_tab, HABIT_TAB_WEEKLY, HABIT_TAB_COUNT - 1);
    app->habits.view_mode = clampi(view_mode, HABIT_VIEW_CALENDAR, HABIT_VIEW_WEEKLY);

    selected_id = storage_get_setting_text("habits_selected_id");
    if(selected_id != NULL && selected_id[0] != '\0') {
        for(i = 0; i < app->habits.count; i++) {
            if(strcmp(app->habits.items[i].id, selected_id) == 0) {
                app->habits.selected = i;
                break;
            }
        }
    }
    /* The cards screen is the root. Restoring a previously viewed habit must
     * not reopen it automatically on launch. */
    app->habits.selected = -1;
}

void
app_reload_after_import(InnerBreeze*app, int reload_settings)
{
    char selected_habit_id[HABIT_ID_SIZE] = "";
    char detail_habit_id[HABIT_ID_SIZE] = "";
    int selected = -1;
    int detail_index = -1;
    int view_mode = HABIT_VIEW_CALENDAR;
    int habit_tab = HABIT_TAB_WEEKLY;
    int month_offset = 0;
    int scroll = 0;
    int weekly_days = 0;
    int hold_stats_range_days = 0;
    int i;
    if(app == NULL)
        return;

    if(!reload_settings) {
        if(app->habits.selected >= 0 && app->habits.selected < app->habits.count)
            snprintf(selected_habit_id, sizeof(selected_habit_id), "%s",
                     app->habits.items[app->habits.selected].id);
        if(app->habit_detail_index >= 0 && app->habit_detail_index < app->habits.count)
            snprintf(detail_habit_id, sizeof(detail_habit_id), "%s",
                     app->habits.items[app->habit_detail_index].id);
        selected = app->habits.selected;
        detail_index = app->habit_detail_index;
        view_mode = app->habits.view_mode;
        habit_tab = app->habits.tab;
        month_offset = app->habits.month_offset;
        scroll = app->habits.scroll;
        weekly_days = app->habits.weekly_days;
        hold_stats_range_days = app->habits.hold_stats_range_days;
    }

    if(reload_settings) {
        if(app_load_settings(app))
            save_settings(app);
        reset_settings_preview(app);
        practice_update_session_sounds(app);
    }

    habits_init_with_defaults(&app->habits, app->language_selected);
    if(reload_settings)
        app_restore_habits_view_settings(app);
    if(!reload_settings) {
        app->habits.selected = -1;
        for(i = 0; i < app->habits.count; i++) {
            if(selected_habit_id[0] != '\0' &&
               strcmp(app->habits.items[i].id, selected_habit_id) == 0) {
                app->habits.selected = i;
                break;
            }
        }
        if(app->habits.selected < 0 && selected >= 0 && selected < app->habits.count)
            app->habits.selected = selected;
        app->habit_detail_index = -1;
        for(i = 0; i < app->habits.count; i++) {
            if(detail_habit_id[0] != '\0' &&
               strcmp(app->habits.items[i].id, detail_habit_id) == 0) {
                app->habit_detail_index = i;
                break;
            }
        }
        if(app->habit_detail_index < 0 && detail_index >= 0 && detail_index < app->habits.count)
            app->habit_detail_index = detail_index;
        app->habits.view_mode = view_mode;
        app->habits.tab = habit_tab;
        app->habits.month_offset = month_offset;
        app->habits.scroll = scroll;
        app->habits.weekly_days = weekly_days;
        app->habits.hold_stats_range_days = hold_stats_range_days == 31 ? 31 : 7;
    }
    if(app->habit_detail_index >= app->habits.count)
        app->habit_detail_index = -1;
    if(app->habits.selected >= app->habits.count)
        app->habits.selected = -1;
    app->habit_session_edit.active = 0;
    app->habit_edit.active = 0;
}

#if ANDROID_BUILD
void
app_request_graphics_reload(InnerBreeze*app)
{
    if(app != NULL)
        app->graphics_reload_requested = 1;
}
#endif

static Texture2D
load_pixel_texture_from_asset(const char *path)
{
    const EmbeddedAsset *asset = GetEmbeddedAsset(path);
    Image image;
    Texture2D texture = {0};

    if(asset == NULL || asset->data == NULL || asset->size == 0) {
#if defined(KRYON_PLATFORM_PLAN9)
        texture = LoadTexture(path);
        if(texture.id != 0)
            SetTextureFilter(texture, TEXTURE_FILTER_POINT);
#endif
        return texture;
    }

    image = LoadImageFromMemory(GetEmbeddedAssetExtension(path), asset->data, (int)asset->size);
    if(image.data == NULL)
        return texture;

#if defined(_WIN32)
    {
        int pot_w = 1;
        int pot_h = 1;
        while(pot_w < image.width)
            pot_w <<= 1;
        while(pot_h < image.height)
            pot_h <<= 1;
        if(pot_w != image.width || pot_h != image.height)
            ImageResizeCanvas(&image, pot_w, pot_h, 0, 0, BLANK);
    }
#endif

    texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if(texture.id != 0)
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

Texture2D
app_load_asset_texture(const char *name)
{
    char path[64];

    snprintf(path, sizeof(path), "assets/%s", name);
    return load_pixel_texture_from_asset(path);
}

static void
app_draw_blank_home_easteregg(InnerBreeze*app)
{
    Texture2D texture;
    Texture2D logo;
    Rectangle src;
    Rectangle dst;
    Vector2 mouse;
    Vector2 origin;
    Color logo_tint;
    float scale;
    float logo_size;
    float logo_scale;
    int bottom_reserved;
    int available_h;
    int logo_hover;

    if(app == NULL)
        return;
    if(app->easteregg_art.id == 0)
        app->easteregg_art = app_load_asset_texture("easteregg/art.png");
    texture = app->easteregg_art;
    if(texture.id == 0 || texture.width <= 0 || texture.height <= 0)
        return;

    bottom_reserved = app_content_bottom_reserved(app);
    available_h = view_height - bottom_reserved;
    if(available_h < Scale(120))
        available_h = view_height;

    scale = (float)view_width / (float)texture.width;
    if((float)available_h / (float)texture.height > scale)
        scale = (float)available_h / (float)texture.height;

    src.x = 0;
    src.y = 0;
    src.width = (float)texture.width;
    src.height = (float)texture.height;
    dst.x = ((float)view_width - (float)texture.width * scale) * 0.5f;
    dst.y = ((float)available_h - (float)texture.height * scale) * 0.5f;
    dst.width = (float)texture.width * scale;
    dst.height = (float)texture.height * scale;
    origin.x = 0;
    origin.y = 0;
    DrawTexturePro(texture, src, dst, origin, 0.0f, WHITE);

    if(app->easteregg_waozi.id == 0)
        app->easteregg_waozi = app_load_asset_texture("easteregg/waozi.png");
    logo = app->easteregg_waozi;
    if(logo.id == 0 || logo.width <= 0 || logo.height <= 0)
        return;

    logo_size = (float)view_width * 0.58f;
    if(logo_size > (float)available_h * 0.58f)
        logo_size = (float)available_h * 0.58f;
    if(logo_size > (float)Scale(320))
        logo_size = (float)Scale(320);
    if(logo_size < (float)logo.width)
        logo_size = (float)logo.width;
    if(logo_size > (float)view_width)
        logo_size = (float)view_width;
    if(logo_size > (float)available_h)
        logo_size = (float)available_h;
    logo_scale = logo_size / (float)logo.width;

    src.x = 0;
    src.y = 0;
    src.width = (float)logo.width;
    src.height = (float)logo.height;
    dst.x = ((float)view_width - (float)logo.width * logo_scale) * 0.5f;
    dst.y = ((float)available_h - (float)logo.height * logo_scale) * 0.5f;
    dst.width = (float)logo.width * logo_scale;
    dst.height = (float)logo.height * logo_scale;
    mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);
    logo_hover = CheckCollisionPointRec(mouse, dst) &&
                 !UIInputCapturesClick(mouse);
    if(logo_hover) {
        MarkUIClickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            (void)OpenURI("https://waozi.xyz");
    }
    if(logo_hover) {
        logo_tint.r = 210;
        logo_tint.g = 210;
        logo_tint.b = 210;
        logo_tint.a = 255;
    } else {
        logo_tint.r = 150;
        logo_tint.g = 150;
        logo_tint.b = 150;
        logo_tint.a = 255;
    }
    DrawTexturePro(logo, src, dst, origin, 0.0f, logo_tint);
}

static void
app_apply_initial_screen(InnerBreeze*app)
{
    if(app == NULL)
        return;
    if(!app->language_selected) {
        app->breathing.screen = ScreenLanguage;
        app->habits.focus_selected_tab = 0;
        return;
    }
    app->breathing.screen = app_screen_for_main_tab(app->main_tab);
    app->habits.focus_selected_tab = app->breathing.screen == ScreenHabits;
}

void
app_init(void *vapp) {
    InnerBreeze*app = vapp;
    int i;
    if(app == 0)
        return;

#if ANDROID_BUILD
    if (practice_active(app) != NULL) {
        android_allow_screen_off();
    }
    practice_active_background_stop(app);
#endif

    InitLocale();
    InitUIDPI();
    TraceLog(LOG_INFO, "APP: app init width=%d height=%d embedded=%d",
             config.width, config.height, config.loaded);
    load_config();
    app_notifications_init(app);

    view_width = config.width > 0 ? config.width : APP_DEFAULT_WIDTH;
    view_height = config.height > 0 ? config.height : APP_DEFAULT_HEIGHT;
    UpdateUIDPI(view_width, view_height);
    view_width = GetLayoutWidth();
    view_height = GetLayoutHeight();
    {
        float user_scale = app->ui_scale_tenths > 0
                               ? (float)app->ui_scale_tenths / 10.0f
                               : 1.0f;
        InitUI(view_width, view_height, user_scale);
    }
    TraceLog(LOG_INFO, "APP: render scale=%.2f layout=%dx%d",
             GetRenderScale(), view_width, view_height);
#if ANDROID_BUILD
    SetTextInputPlatformCallback(android_device_set_soft_keyboard_visible);
#endif

    breath_session_init(&app->breathing);
    break_engine_init(&app->breaks);
    app->breaks_enabled = 0;
    app->desktop_startup_mode = STARTUP_SHOW;
    app->desktop_close_action = CLOSE_ASK;
    app->break_hud_x = -1;
    app->break_hud_y = -1;
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    app->break_sounds_enabled = 1;
#else
    app->break_sounds_enabled = 0;
#endif
    data_init();
    if(app_load_settings(app))
        save_settings(app);
    app_init_donation_reminder_observed_practice_count(app);
    if(!load_locale_font(app))
        TraceLog(LOG_WARNING, "FONT: Failed to load Noto UI font -> using built-in default");
    update_check_start();
    app->practice_tab = PRACTICE_TAB_PLAY;
    app->practice_config_tab = 0;
    memset(&app->session_result, 0, sizeof(app->session_result));
    if(app->language_needs_save) {
        save_settings(app);
        app->language_needs_save = 0;
    }
    habits_init_with_defaults(&app->habits, app->language_selected);
    app_restore_habits_view_settings(app);
    app->habit_detail_index = -1;
    app->elist.editing_item = -1;
    memset(&app->habit_session_edit, 0, sizeof(app->habit_session_edit));
    app->habit_session_edit.round = -1;
    app_apply_initial_screen(app);
    practice_update_circle_bounds(app, app_content_top_reserved(app),
                                  app_content_bottom_reserved(app));
#if !defined(PLATFORM_WEB)
    if(!app_running_in_kryon_preview())
        init_audio(app);
#endif
    for(i = 0; i < practice_count(); i++) {
        const PracticeDefinition *practice = practice_get(i);
        if(practice->init != NULL)
            practice->init(app);
    }
    memset(&app->camera, 0, sizeof(app->camera));
    app->play_circle_hover = 0;
    app->play_circle_scale = 1.0f;
    app->settings_tab = SETTINGS_TAB_DEVICE;
    memset(&app->habit_edit, 0, sizeof(app->habit_edit));
    app->habit_edit.index = -1;
    app->habit_edit.color.r = 99;
    app->habit_edit.color.g = 196;
    app->habit_edit.color.b = 165;
    app->habit_edit.color.a = 255;
    app->habit_edit.sync_mode = HABIT_SYNC_NONE;
    practice_update_session_sounds(app);
    reset_settings_preview(app);
    breath_session_init(&app->start_speed_preview);

    // Load all icons
    LoadAllUIIconTextures(app->icons);

    SetUIIcons(app->icons[UI_ICON_TYPE_GEAR], app->icons[UI_ICON_TYPE_X]);

    memset(&app->modal, 0, sizeof(app->modal));
    app->meditation.duration_seconds = 0;
    app->meditation.remaining_seconds = 0;
    app->meditation.frame_ticks = 0;
    app_auto_sync(app);
}

static void
handle_back_button(InnerBreeze*app)
{
    /* If modal is active, let modal drawing handle it */
    if(app->modal.active) {
        return;
    }

    switch(app->breathing.screen) {
    case ScreenStart:
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        if(app->practice_tab != PRACTICE_TAB_PLAY) {
            AppRoute route = app_current_route(app);
            route.practice_tab = PRACTICE_TAB_PLAY;
            app->manual_scroll = 0;
            app->settings_scroll = 0;
            app->tutorial_step = 0;
            app_switch_route(app, route);
            break;
        }
        break;

    case ScreenSettings:
        if(app->settings_dirty)
            save_settings(app);
        settings_screen_clear_status();
        if(app_return_to_nav_sidebar_if_needed(app))
            break;
        app_switch_screen(app, app->main_tab == APP_MAIN_TAB_HABITS
                                  ? ScreenHabits
                                  : ScreenStart);
        app->settings_scroll = 0;
        break;

    case ScreenProfile:
        app->profile_scroll = 0;
        app->sync_server_url_focused = 0;
        settings_screen_clear_status();
        if(app_return_to_nav_sidebar_if_needed(app))
            break;
        app_switch_screen(app, app->main_tab == APP_MAIN_TAB_HABITS
                                  ? ScreenHabits
                                  : ScreenStart);
        break;

    case ScreenCustomizeNav:
        if(app_return_to_nav_sidebar_if_needed(app))
            break;
        app_switch_screen(app, app->main_tab == APP_MAIN_TAB_HABITS
                                  ? ScreenHabits
                                  : ScreenStart);
        break;

    case ScreenNavSidebar:
        app_close_nav_sidebar(app);
        break;

    case ScreenPracticeConfig:
        app_leave_practice_config(app);
        app_switch_screen(app, ScreenStart);
        break;

    case ScreenHabits:
        if(app->habits.screen_mode == HABITS_SCREEN_HISTORY ||
           app->habits.screen_mode == HABITS_SCREEN_STATISTICS) {
            app->habits.screen_mode = HABITS_SCREEN_DETAIL;
            app->habits.tab = HABIT_TAB_WEEKLY;
            app->habits.view_mode = HABIT_VIEW_WEEKLY;
            app->habits.scroll = 0;
            save_settings(app);
            break;
        }
        if(app->habits.screen_mode == HABITS_SCREEN_DETAIL) {
            app->habits.screen_mode = HABITS_SCREEN_OVERVIEW;
            app->habits.tab = HABIT_TAB_WEEKLY;
            app->habits.view_mode = HABIT_VIEW_WEEKLY;
            app->habits.scroll = 0;
            save_settings(app);
            break;
        }
        if(app->habits.tab == HABIT_TAB_EDIT && app->habit_edit.active) {
            habit_edit_cancel(app);
            break;
        }
        app_switch_screen(app, ScreenStart);
        break;
    case ScreenEList:
        app_switch_screen(app, app_screen_for_main_tab(APP_MAIN_TAB_PRACTICE));
        break;

    case ScreenHabitEdit:
        habit_edit_cancel(app);
        app_switch_screen(app, ScreenHabits);
        break;

    case ScreenHabitSessionEdit:
        habit_session_cancel_edit(app);
        app_switch_screen(app, ScreenHabits);
        break;

    case ScreenLanguage:
        break;

    case ScreenManual:
        manual_screen_close_tutorial(app, 0);
        break;

    case ScreenResults:
        practice_ensure_results_saved(app);
#if ANDROID_BUILD
        android_allow_screen_off();
        android_wakelock_release();
#endif
        app_init(app);
        break;

    case ScreenSession:
        /* When paused, exit immediately */
        if(app->session_paused) {
            practice_active_background_stop(app);
            app_init(app);
        } else {
            /* Show confirmation modal */
            app_open_modal(app, UIModalConfirmExitSession);
        }
        break;

    case ScreenMeditation:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_MEDITATION);
            if(practice->request_exit != NULL)
                practice->request_exit(app);
        }
        break;

    case ScreenPatterns:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_PATTERNS);
            if(practice->request_exit != NULL)
                practice->request_exit(app);
        }
        break;

    default:
        break;
    }
}

static void
draw_profile_picture_picker_modal(InnerBreeze*app)
{
    ProfilePicturePickerResult result;
    ProfilePicturePickerProps props;

    if(app == NULL)
        return;

    memset(&props, 0, sizeof(props));
    props.title = "Profile picture";
    props.icons = app->icons;
    props.selected_icon_type = &app->profile_picture_icon;
    props.close_icon = app->icons[UI_ICON_TYPE_X];
    props.max_width = 520;
    props.scroll_offset = &app->profile_picture_picker_scroll;
    result = ProfilePicturePicker(props);
    if(result.changed)
        save_settings(app);
    if(result.closed)
        app_close_modal(app);
}

static TextInputStyle
app_donation_address_style(void)
{
    TextInputStyle style;

    memset(&style, 0, sizeof(style));
    style.background = DarkenUIColor(GetThemeBackground(), 4);
    style.border = GetThemeButton();
    style.focus_border = GetThemeButtonHover();
    style.text = GetThemeText();
    style.cursor = GetThemeText();
    style.radius = 0.08f;
    style.padding_x = Scale(10);
    style.padding_y = Scale(8);
    return style;
}

static int
app_donation_address_box_height(const char *address, int w, int font,
                                TextInputStyle style)
{
    ReadonlyTextBoxProps props;

    memset(&props, 0, sizeof(props));
    props.bounds.width = (float)w;
    props.text = address;
    props.font = font;
    props.style = style;
    props.line_gap = Scale(2);
    return GetNodeHeight(NodeReadonlyTextBox(props));
}

static int app_donation_button_stack(int w);

static int
app_donation_coin_section_height(const char *address, int w)
{
    int label_font = GetFontSize();
    int address_font = GetSmallFontSize();
    int pad = Scale(12);
    int button_h = Scale(36);
    int gap = Scale(8);
    int content_w = w - pad * 2;
    int action_h = button_h;

    if(content_w < Scale(120))
        content_w = Scale(120);
    if(app_donation_button_stack(content_w))
        action_h = button_h * 3 + gap * 2;

    return pad +
           TextLineHeight(label_font) +
           Scale(6) +
           app_donation_address_box_height(address, content_w, address_font,
                                           app_donation_address_style()) +
           Scale(10) + action_h + pad;
}

static char app_donation_bitcoin_text[96];
static char app_donation_monero_text[160];
static int app_donation_bitcoin_cursor;
static int app_donation_monero_cursor;
static int app_donation_bitcoin_focused;
static int app_donation_monero_focused;
static int app_donation_bitcoin_scroll;
static int app_donation_monero_scroll;
static int app_donation_modal_scroll;

static int
app_donation_button_stack(int w)
{
    return w < Scale(330);
}

static void
app_draw_donation_coin_section(const char *label, const char *address,
                               const char *wallet_url,
                               const char *trocador_url,
                               char *address_text,
                               size_t address_text_size,
                               int *cursor,
                               int *focused,
                               int *scroll,
                               int focus_id,
                               int x, int w, int *y)
{
    int label_font = GetFontSize();
    int address_font = GetSmallFontSize();
    int pad = Scale(12);
    int button_h = Scale(36);
    int gap = Scale(8);
    int content_x = x + pad;
    int content_w = w - pad * 2;
    int button_w = (content_w - gap * 2) / 3;
    int stack_buttons = app_donation_button_stack(content_w);
    int card_y = y != NULL ? *y : 0;
    int card_h;
    int box_h;
    int hover = 0;
    TextInputStyle style = app_donation_address_style();
    TextAreaProps text_area;

    if(y == NULL)
        return;

    if(address_text == NULL || address_text_size == 0 ||
       cursor == NULL || focused == NULL || scroll == NULL)
        return;

    snprintf(address_text, address_text_size, "%s", address);

    card_h = app_donation_coin_section_height(address, w);
    DrawRectangleRounded((Rectangle){(float)x, (float)card_y, (float)w,
                         (float)card_h}, 0.08f, 8,
                         DarkenUIColor(GetThemeBackground(), 5));
    DrawRectangleRoundedLinesEx((Rectangle){(float)x, (float)card_y,
                                (float)w, (float)card_h}, 0.08f, 8,
                                (float)Scale(1),
                                DarkenUIColor(GetThemeBackground(), 26));

    *y = card_y + pad;
    Text((TextProps){
        .bounds = {(float)content_x, (float)*y, 0, 0},
        .text = label,
        .font = label_font,
        .color = GetThemeText(),
        .wrap = TextWrapNone,
    });
    *y += TextLineHeight(label_font) + Scale(6);

    box_h = app_donation_address_box_height(address, content_w, address_font,
                                           style);
    memset(&text_area, 0, sizeof(text_area));
    text_area.bounds.x = (float)content_x;
    text_area.bounds.y = (float)*y;
    text_area.bounds.width = (float)content_w;
    text_area.bounds.height = (float)box_h;
    text_area.text = address_text;
    text_area.text_size = address_text_size;
    text_area.cursor_position = cursor;
    text_area.focused = focused;
    text_area.scroll_y = scroll;
    text_area.font = address_font;
    text_area.line_gap = Scale(2);
    text_area.focus_id = focus_id;
    text_area.style = style;
    text_area.read_only = 1;
    text_area.wrap = 1;
    TextArea(text_area);
    *y += box_h + Scale(10);

    if(stack_buttons)
        button_w = content_w;

    if(StyledButton(content_x, *y, button_w, button_h,
                     GetLocaleText("copy_address_button"),
                     ButtonStyleSecondary, 0, &hover)) {
#if ANDROID_BUILD
        int android_copy_ok =
            android_device_copy_text_and_toast(address,
                                               GetLocaleText("address_copied"));
        if(!android_copy_ok) {
            SetUIClipboardTextValue(address);
            ShowToast(GetLocaleText("address_copied"));
        }
#else
        SetUIClipboardTextValue(address);
        ShowToast(GetLocaleText("address_copied"));
#endif
    }

    if(stack_buttons)
        *y += button_h + gap;
    if(StyledButton(stack_buttons ? content_x : content_x + button_w + gap,
                     *y, button_w, button_h,
                     GetLocaleText("open_wallet_button"),
                     ButtonStyleSecondary, 0, &hover)) {
        if(!OpenURI(wallet_url))
            ShowToast(GetLocaleText("wallet_not_installed_toast"));
    }

    if(stack_buttons)
        *y += button_h + gap;
    if(StyledButton(stack_buttons ? content_x :
                     content_x + (button_w + gap) * 2, *y, button_w, button_h,
                     GetLocaleText("trocador_button"),
                     ButtonStylePrimary, 0, &hover)) {
        (void)OpenURI(trocador_url);
    }

    *y = card_y + card_h;
}

static void
draw_about_donation_modal(InnerBreeze*app)
{
    UIPanelFrame frame;
    ParagraphSpec message;
    UIScrollArea scroll_area;
    UIScrollView scroll_view;
    int modal_w;
    int modal_h;
    int content_w;
    int max_modal_h;
    int message_h;
    int content_h;
    int coin_w;
    int coin_gap;
    int bitcoin_h;
    int monero_h;
    int coins_h;
    int columns;
    int scroll_h;
    int scroll_content_w;
    int y;
    int coin_y;
    int button_w;
    int button_h = Scale(36);
    int hover = 0;
    Texture2D empty_icon;

    if(app == NULL)
        return;

    modal_w = Scale(760);
    if(view_width < Scale(820))
        modal_w = Scale(460);
    if(modal_w > view_width - Scale(24))
        modal_w = view_width - Scale(24);
    if(modal_w < Scale(240))
        modal_w = Scale(240);
    content_w = modal_w - Scale(36);
    coin_gap = Scale(12);
    columns = content_w >= Scale(640);
    coin_w = columns ? (content_w - coin_gap) / 2 : content_w;

    memset(&message, 0, sizeof(message));
    message.text = GetLocaleText("about_donation_message");
    message.width = content_w;
    message.font = GetSmallFontSize();
    message.line_gap = Scale(4);
    message.color = DarkenUIColor(GetThemeText(), 28);
    message_h = GetNodeHeight(NodeParagraph(message, 0, 0));
    bitcoin_h = app_donation_coin_section_height(app_bitcoin_donation_address(),
                                                 coin_w);
    monero_h = app_donation_coin_section_height(app_monero_donation_address(),
                                                coin_w);
    coins_h = columns ? (bitcoin_h > monero_h ? bitcoin_h : monero_h)
                      : bitcoin_h + coin_gap + monero_h;
    content_h = message_h + Scale(16) + coins_h;
    modal_h = Scale(58) + content_h + Scale(14) + button_h +
              Scale(16);
    max_modal_h = view_height - Scale(24);
    if(modal_h > max_modal_h)
        modal_h = max_modal_h;
    if(modal_h < Scale(260))
        modal_h = Scale(260);

    memset(&empty_icon, 0, sizeof(empty_icon));
    frame = ModalFrame(modal_w, modal_h,
                       GetLocaleText("donation_reminder_title"),
                       empty_icon, app->icons[UI_ICON_TYPE_X]);
    if(frame.right_clicked) {
        app_close_modal(app);
        return;
    }

    button_w = Scale(120);
    if(button_w > frame.content_w)
        button_w = frame.content_w;

    scroll_h = frame.content_h - button_h - Scale(14);
    if(scroll_h < Scale(120))
        scroll_h = Scale(120);
    memset(&scroll_area, 0, sizeof(scroll_area));
    scroll_area.bounds = (Rectangle){
        (float)frame.content_x,
        (float)frame.content_y,
        (float)frame.content_w,
        (float)scroll_h
    };
    scroll_area.content_height = content_h;
    scroll_area.content_x = frame.content_x;
    scroll_area.content_width = frame.content_w;
    scroll_area.scroll_offset = &app_donation_modal_scroll;
    scroll_area.wheel_step = Scale(34);
    scroll_area.scrollbar_x = frame.content_x + frame.content_w - Scale(8);

    scroll_view = BeginUIScrollContainer(scroll_area);
    scroll_content_w = scroll_view.content_w;
    columns = scroll_content_w >= Scale(640);
    coin_w = columns ? (scroll_content_w - coin_gap) / 2 : scroll_content_w;
    message.width = scroll_content_w;
    y = scroll_view.content_y;
    Paragraph(message, scroll_view.content_x, &y);
    y += Scale(16);
    coin_y = y;

    app_draw_donation_coin_section("Bitcoin",
                                   app_bitcoin_donation_address(),
                                   app_bitcoin_wallet_url(),
                                   app_bitcoin_trocador_url(),
                                   app_donation_bitcoin_text,
                                   sizeof(app_donation_bitcoin_text),
                                   &app_donation_bitcoin_cursor,
                                   &app_donation_bitcoin_focused,
                                   &app_donation_bitcoin_scroll,
                                   6101,
                                   scroll_view.content_x, coin_w, &coin_y);
    if(columns) {
        coin_y = y;
        app_draw_donation_coin_section("Monero",
                                       app_monero_donation_address(),
                                       app_monero_wallet_url(),
                                       app_monero_trocador_url(),
                                       app_donation_monero_text,
                                       sizeof(app_donation_monero_text),
                                       &app_donation_monero_cursor,
                                       &app_donation_monero_focused,
                                       &app_donation_monero_scroll,
                                       6102,
                                       scroll_view.content_x + coin_w +
                                           coin_gap,
                                       coin_w, &coin_y);
    } else {
        coin_y += coin_gap;
        app_draw_donation_coin_section("Monero",
                                       app_monero_donation_address(),
                                       app_monero_wallet_url(),
                                       app_monero_trocador_url(),
                                       app_donation_monero_text,
                                       sizeof(app_donation_monero_text),
                                       &app_donation_monero_cursor,
                                       &app_donation_monero_focused,
                                       &app_donation_monero_scroll,
                                       6102,
                                       scroll_view.content_x, coin_w, &coin_y);
    }
    EndUIScrollContainer(scroll_area, scroll_view);

    if(StyledButton(frame.x + (frame.w - button_w) / 2,
                     frame.y + frame.h - button_h - Scale(16),
                     button_w, button_h,
                     GetLocaleText("close_button"),
                     ButtonStyleSecondary, 0, &hover)) {
        app_close_modal(app);
    }
}

static void
draw_donation_reminder_modal(InnerBreeze*app)
{
    int modal_result;
    ModalAction actions[3];
    ModalProps props;

    if(app == NULL)
        return;

    memset(actions, 0, sizeof(actions));
    actions[0].label = GetLocaleText("donation_reminder_donate_button");
    actions[0].style = ButtonStylePrimary;
    actions[1].label = GetLocaleText("donation_reminder_skip_button");
    actions[1].style = ButtonStyleSecondary;
    actions[2].label = GetLocaleText("donation_reminder_dismiss_button");
    actions[2].style = ButtonStyleDanger;
    memset(&props, 0, sizeof(props));
    props.title = GetLocaleText("donation_reminder_title");
    props.message = GetLocaleText("donation_reminder_message");
    props.actions = actions;
    props.action_count = 3;
    props.close_icon = app->icons[UI_ICON_TYPE_X];
    props.max_width = 420;
    modal_result = ActionModal(props);
    if(modal_result == -1) {
        app_record_donation_reminder_seen(app);
        app_close_modal(app);
    } else if(modal_result > 0) {
        app_handle_donation_reminder_action(app, modal_result);
    }
}

static void
draw_secure_migration_modal(InnerBreeze*app)
{
    static const Texture2D no_texture;
    StorageSyncStatus status;
    UIPanelFrame frame;
    ParagraphSpec message;
    const char *message_text;
    int modal_h;
    int message_h;
    int y;
    int button_y;
    int button_h;
    int gap;
    int hover = 0;

    if(app == NULL)
        return;

    memset(&status, 0, sizeof(status));
    storage_sync_status(&status);

    if(!status.has_account || !status.server_connected) {
        app_close_modal(app);
        return;
    }

    if(!status.secure_migration_pending) {
        message_text = GetLocaleText("sync_secure_migration_done_message");
    } else if(app->secure_migration_started) {
        message_text = GetLocaleText("sync_secure_migration_updating_message");
    } else {
        message_text = GetLocaleText("sync_secure_migration_message");
    }

    memset(&message, 0, sizeof(message));
    message.text = message_text;
    message.width = Scale(380) - Scale(36);
    message.font = GetFontSize();
    message.line_gap = Scale(4);
    message.color = GetThemeText();
    message_h = GetNodeHeight(NodeParagraph(message, 0, 0));
    button_h = Scale(36);
    gap = Scale(10);
    modal_h = Scale(74) + message_h + Scale(24) + button_h +
              Scale(24);
    if(status.secure_migration_pending && app->secure_migration_started)
        modal_h += Scale(42);
    if(modal_h < Scale(210))
        modal_h = Scale(210);

    frame = ModalFrame(Scale(380), modal_h,
                       GetLocaleText("sync_secure_migration_title"),
                       no_texture, no_texture);
    y = frame.content_y;
    message.width = frame.content_w;
    Paragraph(message, frame.content_x, &y);
    y += Scale(18);

    if(status.secure_migration_pending && app->secure_migration_started) {
        long long total = status.secure_migration_total;
        long long done = status.secure_migration_done;
        char progress_label[64];
        ProgressBarProps progress_props;
        int progress_max;
        int progress_value;

        if(total <= 0)
            total = status.secure_migration_queued > 0
                        ? status.secure_migration_queued
                        : 1;
        if(done < 0)
            done = 0;
        if(done > total)
            done = total;
        progress_max = total > INT_MAX ? INT_MAX : (int)total;
        progress_value = done > INT_MAX ? INT_MAX : (int)done;
        snprintf(progress_label, sizeof(progress_label), "%lld / %lld",
                 done, total);
        memset(&progress_props, 0, sizeof(progress_props));
        progress_props.bounds = (Rectangle){
            (float)frame.content_x,
            (float)y,
            (float)frame.content_w,
            (float)Scale(24)
        };
        progress_props.min = 0;
        progress_props.max = progress_max > 0 ? progress_max : 1;
        progress_props.value = progress_value;
        progress_props.label = progress_label;
        Progress(progress_props);
        return;
    }

    button_y = frame.y + frame.h - Scale(24) - button_h;
    if(!status.secure_migration_pending) {
        int button_w = Scale(120);
        if(StyledButton(frame.x + (frame.w - button_w) / 2, button_y,
                        button_w, button_h, GetLocaleText("ok_button"),
                        ButtonStylePrimary, 0, &hover))
            app_close_modal(app);
    } else {
        int button_w = (frame.content_w - gap) / 2;
        if(StyledButton(frame.content_x, button_y, button_w, button_h,
                        GetLocaleText("sync_secure_migration_later_button"),
                        ButtonStyleSecondary, 0, &hover)) {
            app->secure_migration_prompt_seen = 1;
            app->secure_migration_deferred = 1;
            app_close_modal(app);
        }
        if(StyledButton(frame.content_x + button_w + gap, button_y,
                        button_w, button_h,
                        GetLocaleText("sync_secure_migration_start_button"),
                        ButtonStylePrimary, 0, &hover)) {
            app->secure_migration_prompt_seen = 1;
            app->secure_migration_deferred = 0;
            app->secure_migration_started = 1;
            app_auto_sync(app);
        }
    }
}

static void
draw_global_modal(InnerBreeze*app)
{
    int modal_result;

    if(app == NULL || !app->modal.active)
        return;
    if(app->blocked_input_frame == app->breathing.frame)
        return;

    ClearUIInputCaptures();

    if(settings_data_draw_modals(app))
        return;

    if(app->modal.type == UIModalDonationReminder) {
        draw_donation_reminder_modal(app);
        return;
    }
    if(app->modal.type == UIModalSecureMigration) {
        draw_secure_migration_modal(app);
        return;
    }
    if(app->modal.type == UIModalAboutDonation) {
        draw_about_donation_modal(app);
        return;
    }
    if(app->modal.type == UIModalMeditationNetworkError) {
        modal_result = Modal(GetLocaleText("meditation_music_network_error_title"),
                                     GetLocaleText("meditation_music_network_error_message"),
                                     GetLocaleText("cancel_button"),
                                     GetLocaleText("retry_button"));
        if(modal_result == 1) {
            TraceLog(LOG_INFO, "AUDIO: Network error download cancelled");
            app_close_modal(app);
        } else if(modal_result == 2) {
            TraceLog(LOG_INFO, "AUDIO: Network error download retry requested");
            app_close_modal(app);
            meditation_music_start_download(app);
        }
    }
    if(app->modal.type == UIModalThemePicker)
        settings_screen_draw_theme_picker_modal(app);
    if(app->modal.type == UIModalProfilePicturePicker) {
        draw_profile_picture_picker_modal(app);
        return;
    }
    if(app->modal.type == UIModalConfirmRemoveFriend) {
        profile_screen_draw_remove_friend_modal(app);
        return;
    }
    if(app->modal.type == UIModalPracticeManual ||
       app->modal.type == UIModalPracticeConfig ||
       app->modal.type == UIModalEditProgressiveStartSpeed) {
        practice_screen_draw_modal(app);
        return;
    }
    if(app->modal.type == UIModalSyncAlias) {
        modal_result = settings_sync_account_draw_alias_modal(app);
        if(modal_result == 1) {
            int then_backup = app->sync_alias_then_backup;
            app->sync_alias_then_backup = 0;
            app_close_modal(app);
            settings_screen_set_status_success(GetLocaleText("sync_alias_saved"), NULL);
            if(then_backup)
                app_open_modal(app, UIModalSyncAccountBackup);
        } else if(modal_result == 2) {
            settings_screen_set_status_error(GetLocaleText("sync_alias_failed"));
        } else if(modal_result == 3) {
            int then_backup = app->sync_alias_then_backup;
            app->sync_alias_then_backup = 0;
            app_close_modal(app);
            if(then_backup)
                app_open_modal(app, UIModalSyncAccountBackup);
        }
    }
    if(app->modal.type == UIModalSyncPublicId) {
        modal_result = settings_sync_account_draw_public_id_modal(app);
        if(modal_result != 0)
            app_close_modal(app);
    }
}

#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
static void
app_update_desktop_background_state(InnerBreeze*app)
{
    int backgrounded;
    double now;
    int elapsed_ms;

    if(app == NULL)
        return;

    backgrounded = (!IsWindowFocused() || IsWindowMinimized()) ? 1 : 0;
    now = GetTime();
    if(!backgrounded) {
        app->backgrounded = 0;
        app->desktop_background_last_time = 0.0;
        return;
    }

    if(!app->backgrounded || app->desktop_background_last_time <= 0.0)
        app->desktop_background_last_time = now;
    elapsed_ms = (int)((now - app->desktop_background_last_time) * 1000.0);
    app->backgrounded = 1;
    app->desktop_background_last_time = now;

    if(elapsed_ms > 0)
        practice_active_advance_elapsed(app, elapsed_ms);
}
#endif

/* Wrappers for screen draws that return int (e.g. "handled?"); the dispatcher
 * in updateapp() always finishes the frame after them, so the return is unused. */
static void app_draw_settings_screen(InnerBreeze*app)      { (void)settings_screen_draw(app); }
static void app_draw_profile_screen(InnerBreeze*app)       { (void)profile_screen_draw(app); }
static void app_draw_customize_nav_screen(InnerBreeze*app) { (void)app_draw_customize_nav_page(app); }

/* Simple screens: draw, then finish the frame. Adding a screen is a one-line
 * table entry. The practice screens (Start/Session/Meditation/SunSalutation/
 * Results) are dispatched separately below -- they share circle-bounds and
 * active-session logic that does not fit a flat table. */
static const struct {
    int screen;
    void (*draw)(InnerBreeze*app);
} g_simple_screens[] = {
    { ScreenSettings,         app_draw_settings_screen },
    { ScreenProfile,          app_draw_profile_screen },
    { ScreenCustomizeNav,     app_draw_customize_nav_screen },
    { ScreenNavSidebar,       NULL },
    { ScreenLanguage,         language_screen_draw },
    { ScreenHabits,           draw_habits_screen },
    { ScreenEList,            draw_elist_screen },
    { ScreenHabitEdit,        habit_edit_draw },
    { ScreenHabitSessionEdit, habit_session_draw_edit_screen },
    { ScreenBreak,            break_overlay_draw },
};

static void
updateapp(InnerBreeze*app)
{
    int center_x = view_width / 2;
    int frame_view_height = view_height;
    int center_y;
    int i;
    int hover = 0;
    AppRoute frame_route;
    int first_run_guide_active = 0;
    int habits_guide_active = 0;
    int practice_fullscreen_modal = 0;
    int global_modal_drawn = 0;
    int content_input_clip_active = 0;
    int bottom_input_reserved = 0;
    Rectangle input_rect;
    Rectangle input_clip_rect;

    app_sync_route_from_url(app);
    frame_route = app_current_route(app);

#if ANDROID_BUILD
    {
        if(android_take_pending_donation_reminder()) {
            if(app->modal.active)
                app_close_modal(app);
            app_open_modal(app, UIModalDonationReminder);
        }
    }
    {
        int practice_id = android_take_pending_practice_start();
        if(practice_id >= 0) {
            int active_break;
            const PracticeDefinition *practice;

            app->exercise_type = practice_clamp_id(practice_id);
            if(app->modal.active)
                app_close_modal(app);
            active_break = break_engine_active_break(&app->breaks);
            if(active_break >= 0) {
                app_breaks_start_practice(app, active_break, app->exercise_type);
            } else {
                app->main_tab = APP_MAIN_TAB_PRACTICE;
                app->practice_tab = PRACTICE_TAB_PLAY;
                practice = practice_get(app->exercise_type);
                if(practice != NULL && practice->start != NULL)
                    practice->start(app);
            }
        }
    }
#endif

#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    app_update_desktop_background_state(app);
#endif
    app_breaks_update(app);
    app_update_nav_sidebar_mode(app);
    app_notifications_tick(app);
    for(i = 0; i < practice_count(); i++) {
        const PracticeDefinition *practice = practice_get(i);
        if(practice->update != NULL)
            practice->update(app);
    }
    if(app->practice_coming_soon_ticks > 0)
        app->practice_coming_soon_ticks--;
    practice_screen_prepare_first_run_guide(app);
    habits_screen_prepare_first_run_guide(app);
    first_run_guide_active = practice_screen_first_run_guide_active(app);
    habits_guide_active = habits_screen_first_run_guide_active(app);
    app_clear_invalid_screen_local_modal(app);
    app_maybe_open_donation_reminder(app, first_run_guide_active,
                                     habits_guide_active);
    practice_fullscreen_modal =
        app->modal.active &&
        app->modal.type == UIModalEditProgressiveStartSpeed;
    input_rect.x = 0;
    input_rect.y = 0;
    input_rect.width = (float)view_width;
    input_rect.height = (float)view_height;
    if(app->nav_sidebar_open)
        PushUIInputCapture(input_rect, 0);
    if(app->modal.active)
        BeginUIModalLayer();
    if(app->close_prompt_open || first_run_guide_active || habits_guide_active) {
        PushUIInputCapture(input_rect, 0);
    }

    view_height = app_page_height(app, view_height);
    center_y = view_height / 2;
    bottom_input_reserved = app_content_bottom_reserved(app);
    if(bottom_input_reserved > 0 && bottom_input_reserved < view_height) {
        input_clip_rect.x = 0;
        input_clip_rect.y = 0;
        input_clip_rect.width = (float)view_width;
        input_clip_rect.height = (float)(view_height - bottom_input_reserved);
        PushUIInputClip(input_clip_rect);
        content_input_clip_active = 1;
    }
    if(IsKeyPressed(KEY_BACK)
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
       || (IsKeyPressed(KEY_BACKSPACE) &&
           ((app->breathing.screen == ScreenStart &&
             app->practice_tab != PRACTICE_TAB_PLAY) ||
            app->breathing.screen == ScreenSession ||
            app->breathing.screen == ScreenMeditation ||
            app->breathing.screen == ScreenSunSalutation ||
            app->breathing.screen == ScreenPatterns))
#endif
       ) {
        if(app->nav_sidebar_open || app->breathing.screen == ScreenNavSidebar) {
            app_close_nav_sidebar(app);
        } else if(app->close_prompt_open) {
            app->close_prompt_open = 0;
        } else if(first_run_guide_active || habits_guide_active) {
            if(first_run_guide_active)
                practice_screen_dismiss_first_run_guide(app);
            if(habits_guide_active)
                habits_screen_dismiss_first_run_guide(app);
        } else if(app->modal.active) {
            app_close_modal(app);
        } else {
            handle_back_button(app);
        }
    }

#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    /* Ctrl+Q quits from any screen. Esc closes the navigation sidebar first,
     * then quits only when idle on the home screen; deeper screens bind Esc
     * to their own back/cancel handlers. */
    if(IsKeyPressed(KEY_Q) &&
       (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) {
        app_request_desktop_quit(app);
    } else if(IsKeyPressed(KEY_ESCAPE) &&
              (app->nav_sidebar_open ||
               app->breathing.screen == ScreenNavSidebar)) {
        app_close_nav_sidebar(app);
    } else if(IsKeyPressed(KEY_ESCAPE) &&
              app->breathing.screen == ScreenStart &&
              app->practice_tab == PRACTICE_TAB_PLAY &&
              !app->nav_sidebar_open && !app->modal.active &&
              !first_run_guide_active && !habits_guide_active) {
        app_request_desktop_quit(app);
    }
#endif

    for(size_t i = 0; i < sizeof(g_simple_screens) / sizeof(g_simple_screens[0]); i++) {
        if(app->breathing.screen == g_simple_screens[i].screen) {
            if(g_simple_screens[i].draw != NULL)
                g_simple_screens[i].draw(app);
            goto finish_frame;
        }
    }

    if(app->breathing.screen == ScreenStart) {
        practice_update_circle_bounds(app, app_content_top_reserved(app),
                                      app_content_bottom_reserved(app));
    } else if(app->breathing.screen == ScreenSession) {
        practice_update_circle_bounds(app, GetNodeHeight(NodeTitleBar(0)), 84);
    }

    if(app->breathing.screen == ScreenSession)
        practice_draw_active_breathing(app, center_x, center_y);

    switch (app->breathing.screen) {
    case ScreenStart:
        {
            if(!practice_fullscreen_modal &&
               app->main_tab == APP_MAIN_TAB_NONE) {
                app_draw_blank_home_easteregg(app);
            } else if(!practice_fullscreen_modal &&
                      app->main_tab != APP_MAIN_TAB_NONE) {
                if(app->practice_tab == PRACTICE_TAB_MANUAL) {
                    manual_screen_draw(app);
                } else if(app->practice_tab == PRACTICE_TAB_CONFIG) {
                    const PracticeDefinition *practice = practice_get(app->exercise_type);
                    if(practice->draw_config != NULL)
                        practice->draw_config(app);
                } else {
                    practice_screen_draw_home(app);
                }
            }
        }
        // Skip drawing on the same frame modal opens to prevent click propagation
        if(app->modal.active && app->modal.type == UIModalMeditationSetup &&
           app->blocked_input_frame != app->breathing.frame) {
            const PracticeDefinition *practice = practice_get(PRACTICE_MEDITATION);
            if(practice->draw_setup_modal != NULL)
                practice->draw_setup_modal(app);
        }
        break;

    case ScreenSession:
        practice_update_active_breathing(app, center_x, center_y, &hover);
        break;

    case ScreenMeditation:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_MEDITATION);
            if(practice->draw_active_session != NULL)
                practice->draw_active_session(app, center_x, center_y);
        }
        break;

    case ScreenSunSalutation:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_SUN_SALUTATION);
            if(practice->draw_active_session != NULL)
                practice->draw_active_session(app, center_x, center_y);
        }
        break;
    case ScreenPatterns:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_PATTERNS);
            if(practice->draw_active_session != NULL)
                practice->draw_active_session(app, center_x, center_y);
        }
        break;

    case ScreenResults:
        practice_draw_results(app, center_x, center_y, &hover);
        break;

    }

finish_frame:
    if(content_input_clip_active)
        PopUIInputClip();
    if(practice_fullscreen_modal) {
        draw_global_modal(app);
        global_modal_drawn = 1;
    }
    view_height = frame_view_height;
    app_draw_bottom_nav(app);
    practice_screen_draw_first_run_guide(app);
    habits_screen_draw_first_run_guide(app);
    if(!global_modal_drawn)
        draw_global_modal(app);
    app_draw_close_prompt(app);
    DrawToast();
    app_flush_deferred_settings(app);
    app_observe_direct_route_change(app, frame_route);
    app->breathing.frame++;
}

void
app_update_draw(void *vapp, Rectangle viewport) {
    InnerBreeze*app = vapp;
    double profile_frame_start = app_profile_now();
    double profile_update_start;
    double profile_habits_start;
    double profile_sync_start;
    int full_width;
    int full_height;
    int content_x = 0;
    int content_w;
    int layout_width;
    int layout_height;
    int layout_content_x;
    int layout_content_w;
    float render_scale;

    if(app == 0 || viewport.width <= 0 || viewport.height <= 0)
        return;
    app_update_frame_pacing(app);

    app_reload_graphics_resources(app);

    full_width = (int)viewport.width;
    full_height = (int)viewport.height;
    view_width = full_width;
    view_height = full_height;

    /* Update DPI cache */
    UpdateUIDPI(view_width, view_height);
    if(!(GetUIDPIScale() > 0.0f) || GetUIDPIScale() > 8.0f) {
        TraceLog(LOG_WARNING, "APP_EMBED: repairing invalid dpi %.2f for %dx%d",
                 GetUIDPIScale(), view_width, view_height);
        InitUIDPI();
        UpdateUIDPI(view_width, view_height);
    }
    content_x = app_nav_desktop_rail_enabled(app) ? app_nav_desktop_content_offset() : 0;
    if(content_x >= full_width)
        content_x = 0;
    layout_width = GetLayoutWidth();
    layout_height = GetLayoutHeight();
    render_scale = GetRenderScale();
    if(!(render_scale > 0.0f))
        render_scale = 1.0f;
    layout_content_x = (int)((float)content_x / render_scale + 0.5f);
    layout_content_w = layout_width - layout_content_x;
    if(layout_content_w < 1)
        layout_content_w = 1;
    app_full_view_width = layout_width;
    view_width = layout_width;
    view_height = layout_height;
    SetUIViewSize(view_width, view_height);

    {
        float user_scale = app->ui_scale_tenths > 0
                               ? (float)app->ui_scale_tenths / 10.0f
                               : 1.0f;
        InitUI(view_width, view_height, user_scale);
    }
    if(app->modal.active)
        BeginUIModalLayer();
    practice_update_circle_bounds(app, app_content_top_reserved(app),
                                  app_content_bottom_reserved(app));

    app_device_preferences_update(app);
    update_check_poll();
    app_refresh_theme(app);
    SetUITransitionCuesEnabled(0);

    DrawRectangleRec(viewport, GetThemeBackground());

    content_w = full_width - content_x;
    if(content_w < 1)
        content_w = 1;
    {
        static int last_full_w = -1;
        static int last_full_h = -1;
        static int last_content_x = -1;
        static int last_content_w = -1;
        static double last_log_time = -1.0;
        double now = GetTime();
        int changed = full_width != last_full_w || full_height != last_full_h ||
                      content_x != last_content_x || content_w != last_content_w;

        if(changed && (last_log_time < 0.0 || now - last_log_time >= 0.25)) {
            TraceLog(LOG_INFO, "APP_EMBED: geometry viewport=%dx%d layout=%dx%d content_x=%d content=%dx%d render_scale=%.2f",
                     full_width, full_height, layout_width, layout_height,
                     layout_content_x, layout_content_w, layout_height,
                     GetRenderScale());
            last_log_time = now;
        }
        if(changed) {
            last_full_w = full_width;
            last_full_h = full_height;
            last_content_x = content_x;
            last_content_w = content_w;
        }
    }
    view_width = layout_content_w;
    view_height = layout_height;
    SetUIViewSize(view_width, view_height);
    memset(&app->camera, 0, sizeof(app->camera));
    app->camera.zoom = render_scale;
    app->camera.offset.x = IsUIInspectActive() ? 0.0f : viewport.x + content_x;
    app->camera.offset.y = IsUIInspectActive() ? 0.0f : viewport.y;
    SetUIFrame(app->camera);

    if(IsUIInspectActive()) {
        DrawRectangle(0, 0, view_width, view_height, GetThemeBackground());
        profile_update_start = app_profile_now();
        updateapp(app);
        app_profile_accum(&g_app_profile.update_total,
                          &g_app_profile.update_max,
                          profile_update_start);
        EndUIFrame();
    } else {
    BeginUIClip((int)viewport.x, (int)viewport.y, full_width, full_height);
        BeginMode2D(app->camera);
            DrawRectangle(0, 0, view_width, view_height, GetThemeBackground());
            profile_update_start = app_profile_now();
            updateapp(app);
            Overlays();
            app_profile_accum(&g_app_profile.update_total,
                              &g_app_profile.update_max,
                              profile_update_start);
            EndUIFrame();
        EndMode2D();
    EndUIClip();
    }
    profile_habits_start = app_profile_now();
    app_schedule_habits_post_frame_flush(app);
    app_profile_accum(&g_app_profile.habits_total,
                      &g_app_profile.habits_max,
                      profile_habits_start);
    profile_sync_start = app_profile_now();
    app_sync_pump(app);
    app_profile_accum(&g_app_profile.sync_total,
                      &g_app_profile.sync_max,
                      profile_sync_start);
    app_profile_frame_end(profile_frame_start);
}

void
app_unload_texture(Texture2D texture) {
    if (texture.id != 0) {
        UnloadTexture(texture);
    }
}

void
app_destroy(void *vapp)
{
    InnerBreeze*app = vapp;
    int i;

    if (app == NULL) return;

    CloseUIWindow(app->break_window);
    app->break_window = NULL;
    CloseUIWindow(app->break_hud);
    app->break_hud = NULL;

    save_settings(app);
    app->habits_flush_post_frame_scheduled = 0;
    habits_flush_save(app);

    if(!IsUIInspectActive())
        UnloadAllUIIconTextures(app->icons);
    app_unload_texture(app->pet.egg);
    app_unload_texture(app->easteregg_art);
    app_unload_texture(app->easteregg_waozi);
    app_unload_texture(app->font_shapes_texture);
    if(!IsUIInspectActive())
        unload_locale_font(app);

    unload_cue_sounds(app);
    for(i = 0; i < practice_count(); i++) {
        const PracticeDefinition *practice = practice_get(i);
        if(practice->destroy != NULL)
            practice->destroy(app);
    }

    if (app->audio_ready) {
        if(app->audio_meter_attached) {
            DetachAudioMixedProcessor(audio_mixed_meter);
            app->audio_meter_attached = 0;
        }
        CloseAudioDevice();
        app->audio_ready = 0;
    }

    memset(app, 0, sizeof(*app));
}

int
app_should_use_tab_bar(const InnerBreeze*app)
{
    if(app == NULL)
        return 0;

    return app->navigation_mode == NAV_MODE_TABBAR;
}
