#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define MMNOSOUND
#define NOMINMAX
#endif

#include "app.h"
#include "app_internal.h"
#include "app_runtime.h"
#include "app/app_sync.h"
#include "app/app_settings.h"
#include "app/app_update_check.h"
#include "data.h"
#include "app/app_notifications.h"
#include "platform/inbe_desktop_tray.h"
#include "screens/language_screen.h"
#include "screens/manual_screen.h"
#include "screens/break_overlay.h"
#include "screens/break_exercises_screen.h"
#include "breaks/app_breaks.h"
#include "screens/profile_screen.h"
#include "screens/profile_social.h"
#include "screens/settings/settings_screen.h"
#include "screens/settings/settings_data_ui.h"
#include "screens/settings/settings_sync_account_impl.h"
#include "screens/settings/settings_theme.h"
#include "screens/practice_screen.h"
#include "practices/practice_registry.h"
#include "practices/patterns/patterns_practice.h"
#include "app/device_preferences.h"
#include "storage.h"
#include "practices/meditation/meditation_practice.h"
#include "practices/whm/whm_session.h"
#include "sync_account.h"

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
#include <android/log.h>
#include "android_wakelock.h"
#include "android_device.h"
#include "android_insets.h"
#endif

#if defined(PLATFORM_WEB) || ANDROID_BUILD
#define INBE_DEFAULT_WIDTH 320
#define INBE_DEFAULT_HEIGHT 560
#else
#define INBE_DEFAULT_WIDTH 900
#define INBE_DEFAULT_HEIGHT 720
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

static void app_apply_route(InbeApp *app, AppRoute route);

typedef struct InbeRouteBinding {
    const char *id;
    int screen;
} InbeRouteBinding;

static const InbeRouteBinding inbe_route_bindings[] = {
    {"start", InbeScreenStart},
    {"session", InbeScreenSession},
    {"meditation", InbeScreenMeditation},
    {"sun_salutation", InbeScreenSunSalutation},
    {"patterns", InbeScreenPatterns},
    {"results", InbeScreenResults},
    {"settings", InbeScreenSettings},
    {"language", InbeScreenLanguage},
    {"manual", InbeScreenManual},
    {"profile", InbeScreenProfile},
    {"habits", InbeScreenHabits},
    {"customize_nav", InbeScreenCustomizeNav},
    {"nav_sidebar", InbeScreenNavSidebar},
    {"habit_edit", InbeScreenHabitEdit},
    {"habit_session_edit", InbeScreenHabitSessionEdit},
};

static int
app_profile_enabled(void)
{
    if(!g_app_profile.initialized) {
        const char *env = getenv("INBE_PROFILE");
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
           "&buttonbgcolor=445588&textcolor=ffffff&bgcolor=eaeaffff";
}

const char *
app_monero_trocador_url(void)
{
    return "https://trocador.app/en/anonpay/?ticker_to=xmr&network_to=Mainnet"
           "&address=86CbC3d4a2GhT9auh6X99JhmhTMFKVVk8Q9cLrKTHkBu8LLkoNWgkBeAT3YZrvDM6NczYe8brUJNsTiFmwpWDZYnFG5kzSH"
           "&donation=True&simple_mode=True&amount=0.1&name=Inner+Breeze"
           "&email=waotzi@proton.me&ticker_from=xmr&network_from=Mainnet"
           "&buttonbgcolor=445588&textcolor=ffffff&bgcolor=eaeaffff";
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
app_save_donation_reminder(InbeApp *app)
{
    if(app != NULL)
        save_settings(app);
}

static void
app_record_donation_reminder_seen(InbeApp *app)
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
app_donation_reminder_practice_home(const InbeApp *app)
{
    return app != NULL &&
           app->inbe.screen == InbeScreenStart &&
           app->main_tab == APP_MAIN_TAB_PRACTICE &&
           app->practice_tab == PRACTICE_TAB_PLAY;
}

static void
app_init_donation_reminder_observed_practice_count(InbeApp *app)
{
    if(app == NULL ||
       app->donation_reminder_observed_practice_count_initialized)
        return;
    app->donation_reminder_observed_practice_count = storage_session_count();
    app->donation_reminder_observed_practice_count_initialized = 1;
}

static int
app_donation_reminder_next_threshold(const InbeApp *app, int observed_count)
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
app_handle_donation_reminder_action(InbeApp *app, int action)
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

InbeConfig config = {
    .title = "Inner Breeze",
    .width = INBE_DEFAULT_WIDTH,
    .height = INBE_DEFAULT_HEIGHT,
    .loaded = 0,
    .title_custom = 1
};

int view_width = INBE_DEFAULT_WIDTH;
int view_height = INBE_DEFAULT_HEIGHT;
static int app_full_view_width = INBE_DEFAULT_WIDTH;
static InbeApp *g_inbe_app_ptr;
/* Theme colors are now accessed via theme accessor functions */

InbeApp *
get_global_inbe_app(void)
{
    return g_inbe_app_ptr;
}

void
set_global_inbe_app(InbeApp *app)
{
    g_inbe_app_ptr = app;
    TraceLog(LOG_INFO, "INBE: Global app pointer set to %p", app);
}

void *
CreateApp(const char *project_path)
{
    InbeApp *app;

    app = calloc(1, sizeof(*app));
    if(app == NULL)
        return NULL;
    if(project_path != NULL && project_path[0] != '\0')
        ChangeDirectory(project_path);
    app_init(app);
    set_global_inbe_app(app);
    return app;
}

void
DestroyApp(void *vapp)
{
    InbeApp *app = vapp;

    if(app == NULL)
        return;
    app_destroy(app);
    set_global_inbe_app(NULL);
}

void
ApplyRoute(void *vapp, const AppRouteInfo *route_info)
{
    InbeApp *app = vapp;
    AppRoute route;
    int screen = -1;

    if(app == NULL || route_info == NULL || route_info->id == NULL)
        return;
    if(strcmp(route_info->id, "session") == 0) {
        session_start(app);
        return;
    }
    for(size_t i = 0;
        i < sizeof(inbe_route_bindings) / sizeof(inbe_route_bindings[0]);
        i++) {
        if(strcmp(inbe_route_bindings[i].id, route_info->id) == 0) {
            screen = inbe_route_bindings[i].screen;
            break;
        }
    }
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
app_draw_close_title_bar(InbeApp *app, const char *title, int height)
{
    int hover = 0;
    int button_size = ScaleUIPx(18);
    int padding = ScaleUIPx(6);
    int button_total = button_size + padding * 2;
    int x = view_width - button_total - ScaleUIPx(12);
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
app_draw_close_dropdown_title_bar(InbeApp *app, UITitleBarDropdown dropdown,
                                  int height)
{
    int hover = 0;
    int button_size = ScaleUIPx(18);
    int padding = ScaleUIPx(6);
    int button_total = button_size + padding * 2;
    int close_x = view_width - button_total - ScaleUIPx(12);
    int close_y = (height - button_total) / 2;
    int dropdown_x = ScaleUIPx(8);
    int dropdown_h = dropdown.height > 0 ? dropdown.height : ScaleUIPx(32);
    int dropdown_y = (height - dropdown_h) / 2;
    int dropdown_w = close_x - dropdown_x - ScaleUIPx(8);

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
app_leave_practice_config(InbeApp *app)
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

AppRoute
app_current_route(const InbeApp *app)
{
    AppRoute route = {0};

    if(app == NULL)
        return route;
    route.screen = app->inbe.screen;
    route.exercise_type = app->exercise_type;
    route.practice_tab = app->practice_tab;
    route.practice_config_tab = app->practice_config_tab;
    route.settings_tab = app->settings_tab;
    route.profile_view = app->profile_view;
    route.profile_tab = app->profile_tab;
    route.habits_screen_mode = app->habits.screen_mode;
    route.habits_tab = app->habits.tab;
    return route;
}

static int
app_route_equal(AppRoute a, AppRoute b)
{
    if(a.screen != b.screen)
        return 0;
    switch(a.screen) {
    case InbeScreenStart:
        return a.exercise_type == b.exercise_type &&
               a.practice_tab == b.practice_tab &&
               (a.practice_tab != PRACTICE_TAB_CONFIG ||
                a.practice_config_tab == b.practice_config_tab);
    case InbeScreenSettings:
        return a.settings_tab == b.settings_tab;
    case InbeScreenProfile:
        return a.profile_view == b.profile_view &&
               a.profile_tab == b.profile_tab;
    case InbeScreenHabits:
        return a.habits_screen_mode == b.habits_screen_mode &&
               a.habits_tab == b.habits_tab;
    default:
        break;
    }
    return 1;
}

static void
app_enter_route(InbeApp *app, AppRoute route)
{
    if(app == NULL)
        return;
    if(route.screen == InbeScreenProfile) {
        if(route.profile_tab == PROFILE_TAB_FRIENDS) {
            profile_social_load_friends_cache(app);
            app_request_social_refresh(app);
        } else if(route.profile_tab == PROFILE_TAB_LEADERBOARD) {
            profile_social_load_leaderboard_cache(app);
            app_request_social_refresh(app);
        }
    }
}

static void
app_apply_route(InbeApp *app, AppRoute route)
{
    if(app == NULL)
        return;
    app->inbe.screen = route.screen;
    app->exercise_type = route.exercise_type;
    app->practice_tab = route.practice_tab;
    app->practice_config_tab = route.practice_config_tab;
    app->settings_tab = route.settings_tab;
    app->profile_view = route.profile_view;
    app->profile_tab = route.profile_tab;
    app->habits.screen_mode = route.habits_screen_mode;
    app->habits.tab = route.habits_tab;
}

void
app_switch_route(InbeApp *app, AppRoute route)
{
    if(app == NULL)
        return;
    if(app_route_equal(app_current_route(app), route))
        return;

    if(route.screen == InbeScreenHabits && app->inbe.screen != InbeScreenHabits)
        app->habits.focus_selected_tab = 1;

    app_apply_route(app, route);
    app_enter_route(app, app_current_route(app));
}

void
app_switch_screen(InbeApp *app, int screen)
{
    AppRoute route;

    if(app == NULL)
        return;
    route = app_current_route(app);
    route.screen = screen;
    app_switch_route(app, route);
}

static void
app_observe_direct_route_change(InbeApp *app, AppRoute before_route)
{
    if(app == NULL || app_route_equal(before_route, app_current_route(app)))
        return;
    if(app->inbe.screen == InbeScreenHabits && before_route.screen != InbeScreenHabits)
        app->habits.focus_selected_tab = 1;
}

static void
app_flush_deferred_settings(InbeApp *app)
{
    if(app == NULL || app->settings_save_delay_ticks <= 0)
        return;

    app->settings_save_delay_ticks--;
    if(app->settings_save_delay_ticks <= 0 && app->settings_dirty)
        save_settings(app);
}

int
app_toolbar_height(void)
{
    return ScaleUIPx(58);
}

int
app_content_top_reserved(const InbeApp *app)
{
    int system_top = 0;

#if ANDROID_BUILD
    // Include system status bar and camera cutout on Android
    extern int android_get_system_top_reserved(void);
    system_top = android_get_system_top_reserved();
#endif

    if(app != NULL && app->inbe.screen == InbeScreenStart)
        return system_top + UIGetNodeHeight(UINodeTabBar((TabBarProps){0}));
    return system_top + app_toolbar_height();
}

void
app_block_pointer_frame(InbeApp *app)
{
    if(app != NULL)
        app->input_block_frame = app->inbe.frame;
}

void
app_open_modal(InbeApp *app, UIModalType type)
{
    if(app == NULL)
        return;
    app->modal.active = 1;
    app->modal.type = type;
    app_block_pointer_frame(app);
}

static void
app_clear_modal_state(InbeApp *app)
{
    if(app == NULL)
        return;
    app->modal.active = 0;
    app->modal.type = UIModalNone;
    app_block_pointer_frame(app);
}

void
app_close_modal(InbeApp *app)
{
    UIModalType type;

    if(app == NULL)
        return;
    type = app->modal.type;
    if(type == UIModalEditProgressiveStartSpeed &&
       app->inbe.screen == InbeScreenStart &&
       app->practice_tab == PRACTICE_TAB_CONFIG) {
        app->modal.type = UIModalPracticeConfig;
        app_block_pointer_frame(app);
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
app_screen_local_modal_valid(const InbeApp *app, UIModalType type)
{
    if(app == NULL)
        return 0;

    switch(type) {
    case UIModalConfirmExitSession:
        return app->inbe.screen == InbeScreenSession ||
               app->inbe.screen == InbeScreenMeditation ||
               app->inbe.screen == InbeScreenSunSalutation ||
               app->inbe.screen == InbeScreenPatterns;
    case UIModalMeditationSetup:
        return app->inbe.screen == InbeScreenStart;
    case UIModalConfirmDeleteHabit:
    case UIModalHabitPracticeListInfo:
    case UIModalHabitCountingInfo:
        return app->inbe.screen == InbeScreenHabitEdit ||
               (app->inbe.screen == InbeScreenHabits &&
                app->habits.tab == HABIT_TAB_EDIT);
    case UIModalBottomNavConfig:
        return app->inbe.screen == InbeScreenCustomizeNav;
    default:
        return 1;
    }
}

static void
app_clear_invalid_screen_local_modal(InbeApp *app)
{
    if(app == NULL || !app->modal.active)
        return;
    if(app_screen_local_modal_valid(app, app->modal.type))
        return;
    app_clear_modal_state(app);
}

static int
app_donation_reminder_safe(const InbeApp *app, int first_run_guide_active,
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

    switch(app->inbe.screen) {
    case InbeScreenSession:
    case InbeScreenMeditation:
    case InbeScreenSunSalutation:
    case InbeScreenPatterns:
    case InbeScreenResults:
    case InbeScreenBreak:
    case InbeScreenBreakExercises:
    case InbeScreenHabitEdit:
    case InbeScreenHabitSessionEdit:
        return 0;
    default:
        break;
    }
    return 1;
}

static void
app_maybe_open_donation_reminder(InbeApp *app, int first_run_guide_active,
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
app_request_desktop_close(InbeApp *app)
{
    if(app == NULL)
        return;
    /* Configured close behaviour overrides the ask prompt (desktop only). */
    if(app->desktop_close_action == INBE_CLOSE_QUIT) {
        app->request_quit = 1;
        return;
    }
    if(app->desktop_close_action == INBE_CLOSE_KEEP_RUNNING) {
#if defined(INBE_DESKTOP_TRAY_ENABLED)
        inbe_desktop_tray_keep_running();
#else
        MinimizeWindow();
#endif
        return;
    }
    app->close_prompt_open = 1;
    app->close_prompt_input_block_frame = app->inbe.frame;
    app->close_prompt_result = AppClosePromptNone;
}

/*
 * Desktop quit request from a keyboard shortcut (Ctrl+Q) or Esc at the home
 * screen. Tray builds route through the keep-running/quit prompt; no-tray
 * builds have nothing to keep running, so they exit immediately (matching the
 * window-close button behaviour).
 */
void
app_request_desktop_quit(InbeApp *app)
{
    if(app == NULL)
        return;
#if defined(INBE_DESKTOP_TRAY_ENABLED)
    app_request_desktop_close(app);
#else
    app->request_quit = 1;
#endif
}

AppClosePromptResult
app_consume_close_prompt_result(InbeApp *app)
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
app_draw_close_prompt(InbeApp *app)
{
    int modal_result;
    UIModalAction actions[2];

    if(app == NULL || !app->close_prompt_open)
        return;
    if(app->close_prompt_input_block_frame == app->inbe.frame)
        return;

    ClearUIInputCaptures();
    actions[0] = (UIModalAction){
        .label = GetLocaleText("desktop_close_keep_running_button"),
        .style = UI_BUTTON_STYLE_PRIMARY
    };
    actions[1] = (UIModalAction){
        .label = GetLocaleText("desktop_close_quit_button"),
        .style = UI_BUTTON_STYLE_DANGER
    };
    modal_result = ActionModal((ModalProps){
        .title = GetLocaleText("desktop_close_prompt_title"),
        .message = GetLocaleText("desktop_close_prompt_message"),
        .actions = actions,
        .action_count = 2,
        .close_icon = app->icons[UI_ICON_TYPE_X],
        .max_width = 420
    });
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
exercise_manual_seen(InbeApp *app, int exercise_type)
{
    int bit = exercise_manual_seen_bit(exercise_type);
    if(app == NULL || bit == 0)
        return 1;
    return (app->exercise_manual_seen_mask & bit) != 0;
}

void
mark_exercise_manual_seen(InbeApp *app, int exercise_type)
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
app_reload_graphics_resources(InbeApp *app)
{
    if(app == NULL || !app->graphics_reload_requested)
        return;

    app->graphics_reload_requested = 0;
    if(app_running_in_kryon_preview())
        return;

    TraceLog(LOG_INFO, "ANDROID: Reloading graphics resources");

    for(int i = 0; i < UI_ICON_TYPE_COUNT; i++)
        app->icons[i] = (Texture2D){0};
    LoadAllUIIconTextures(app->icons);
    SetUIIcons(app->icons[UI_ICON_TYPE_GEAR], app->icons[UI_ICON_TYPE_X]);

    app->easteregg_art = (Texture2D){0};
    app->easteregg_waozi = (Texture2D){0};
    app->font_shapes_texture = (Texture2D){0};
    discard_locale_font_cpu(app);
    if(!load_locale_font(app))
        TraceLog(LOG_WARNING, "FONT: Failed to reload Noto UI font");
}

void
refresh_locale_dependent_text(InbeApp *app)
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
apply_language_code(InbeApp *app, const char *code)
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
apply_system_language_selection(InbeApp *app, int save_now)
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
apply_language_selection(InbeApp *app, int language_index, int save_now)
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
app_accept_language_selection(InbeApp *app)
{
    if(app == NULL)
        return;

    if(app->language_system || !app->language_selected)
        apply_system_language_selection(app, 1);
    else
        save_settings(app);
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
app_restore_habits_view_settings(InbeApp *app)
{
    const char *selected_id;
    int screen_mode;
    int habit_tab;
    int view_mode;

    if(app == NULL)
        return;

    screen_mode = storage_get_setting_int("habits_screen_mode", HABITS_SCREEN_OVERVIEW);
    habit_tab = storage_get_setting_int("habits_tab", HABIT_TAB_WEEKLY);
    view_mode = storage_get_setting_int("habits_view_mode", HABIT_VIEW_WEEKLY);
    app->habits.screen_mode = clampi(screen_mode, HABITS_SCREEN_OVERVIEW,
                                     HABITS_SCREEN_REORDER);
    if(app->habits.screen_mode == HABITS_SCREEN_REORDER)
        app->habits.screen_mode = HABITS_SCREEN_OVERVIEW;
    app->habits.tab = clampi(habit_tab, HABIT_TAB_WEEKLY, HABIT_TAB_COUNT - 1);
    app->habits.view_mode = clampi(view_mode, HABIT_VIEW_CALENDAR, HABIT_VIEW_WEEKLY);

    selected_id = storage_get_setting_text("habits_selected_id");
    if(selected_id != NULL && selected_id[0] != '\0') {
        for(int i = 0; i < app->habits.count; i++) {
            if(strcmp(app->habits.items[i].id, selected_id) == 0) {
                app->habits.selected = i;
                break;
            }
        }
    }
    if(app->habits.selected < 0 || app->habits.selected >= app->habits.count)
        app->habits.selected = app->habits.count > 0 ? 0 : -1;
}

void
app_reload_after_import(InbeApp *app, int reload_settings)
{
    char selected_habit_id[INBE_HABIT_ID_SIZE] = "";
    char detail_habit_id[INBE_HABIT_ID_SIZE] = "";
    int selected = -1;
    int detail_index = -1;
    int view_mode = HABIT_VIEW_CALENDAR;
    int habit_tab = HABIT_TAB_WEEKLY;
    int month_offset = 0;
    int scroll = 0;
    int weekly_days = 0;
    int hold_stats_range_days = 0;
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

    habits_init(&app->habits);
    if(reload_settings)
        app_restore_habits_view_settings(app);
    if(!reload_settings) {
        app->habits.selected = -1;
        for(int i = 0; i < app->habits.count; i++) {
            if(selected_habit_id[0] != '\0' &&
               strcmp(app->habits.items[i].id, selected_habit_id) == 0) {
                app->habits.selected = i;
                break;
            }
        }
        if(app->habits.selected < 0 && selected >= 0 && selected < app->habits.count)
            app->habits.selected = selected;
        app->habit_detail_index = -1;
        for(int i = 0; i < app->habits.count; i++) {
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
    if(app->habits.selected < 0 || app->habits.selected >= app->habits.count)
        app->habits.selected = app->habits.count > 0 ? 0 : -1;
    app->habit_session_edit.active = 0;
    app->habit_edit.active = 0;
}

#if ANDROID_BUILD
void
app_request_graphics_reload(InbeApp *app)
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

    if(asset == NULL || asset->data == NULL || asset->size == 0)
        return texture;

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
app_draw_blank_home_easteregg(InbeApp *app)
{
    Texture2D texture;
    Texture2D logo;
    Rectangle src;
    Rectangle dst;
    Vector2 mouse;
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
    if(available_h < ScaleUIPx(120))
        available_h = view_height;

    scale = (float)view_width / (float)texture.width;
    if((float)available_h / (float)texture.height > scale)
        scale = (float)available_h / (float)texture.height;

    src = (Rectangle){0, 0, (float)texture.width, (float)texture.height};
    dst = (Rectangle){
        ((float)view_width - (float)texture.width * scale) * 0.5f,
        ((float)available_h - (float)texture.height * scale) * 0.5f,
        (float)texture.width * scale,
        (float)texture.height * scale
    };
    DrawTexturePro(texture, src, dst, (Vector2){0}, 0.0f, WHITE);

    if(app->easteregg_waozi.id == 0)
        app->easteregg_waozi = app_load_asset_texture("easteregg/waozi.png");
    logo = app->easteregg_waozi;
    if(logo.id == 0 || logo.width <= 0 || logo.height <= 0)
        return;

    logo_size = (float)view_width * 0.58f;
    if(logo_size > (float)available_h * 0.58f)
        logo_size = (float)available_h * 0.58f;
    if(logo_size > (float)ScaleUIPx(320))
        logo_size = (float)ScaleUIPx(320);
    if(logo_size < (float)logo.width)
        logo_size = (float)logo.width;
    if(logo_size > (float)view_width)
        logo_size = (float)view_width;
    if(logo_size > (float)available_h)
        logo_size = (float)available_h;
    logo_scale = logo_size / (float)logo.width;

    src = (Rectangle){0, 0, (float)logo.width, (float)logo.height};
    dst = (Rectangle){
        ((float)view_width - (float)logo.width * logo_scale) * 0.5f,
        ((float)available_h - (float)logo.height * logo_scale) * 0.5f,
        (float)logo.width * logo_scale,
        (float)logo.height * logo_scale
    };
    mouse = GetScreenToWorld2D(GetMousePosition(), app->camera);
    logo_hover = CheckCollisionPointRec(mouse, dst) &&
                 !UIInputCapturesClick(mouse);
    if(logo_hover) {
        MarkUIClickable();
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            (void)OpenURI("https://waozi.xyz");
    }
    DrawTexturePro(logo, src, dst, (Vector2){0}, 0.0f,
                   logo_hover ? (Color){210, 210, 210, 255}
                              : (Color){150, 150, 150, 255});
}

void
app_init(void *vapp) {
    InbeApp *app = vapp;
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
    TraceLog(LOG_INFO, "INBE: app init width=%d height=%d embedded=%d",
             config.width, config.height, config.loaded);
    load_config();
    app_notifications_init(app);

    view_width = config.width > 0 ? config.width : INBE_DEFAULT_WIDTH;
    view_height = config.height > 0 ? config.height : INBE_DEFAULT_HEIGHT;
    UpdateUIDPI(view_width, view_height);
    {
        float user_scale = app->ui_scale_tenths > 0
                               ? (float)app->ui_scale_tenths / 10.0f
                               : 1.0f;
        InitUI(view_width, view_height, GetUIDPIScale() * user_scale);
    }
    TraceLog(LOG_INFO, "INBE: DPI scale=%.2f (viewport %dx%d)", GetUIDPIScale(), view_width, view_height);
#if ANDROID_BUILD
    SetUITextInputPlatformCallback(android_device_set_soft_keyboard_visible);
#endif

    inbeinit(&app->inbe);
    break_engine_init(&app->breaks);
    app->breaks_enabled = 0;
    app->desktop_startup_mode = INBE_STARTUP_SHOW;
    app->desktop_close_action = INBE_CLOSE_ASK;
    app->break_hud_x = -1;
    app->break_hud_y = -1;
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    app->break_sounds_enabled = 1;
#else
    app->break_sounds_enabled = 0;
#endif
    app->break_exercise_count = 3;
    app->break_ex_pick_count = 0;
    app->break_ex_offset_s = 0;
    app->break_ex_paused = 0;
    app->break_ex_hidden = 0;
    data_init();
    if(app_load_settings(app))
        save_settings(app);
    app_init_donation_reminder_observed_practice_count(app);
    if(!load_locale_font(app))
        TraceLog(LOG_WARNING, "FONT: Failed to load Noto UI font -> using built-in default");
    inbe_update_check_start();
    app->practice_tab = PRACTICE_TAB_PLAY;
    app->practice_config_tab = 0;
    if(app->language_needs_save) {
        save_settings(app);
        app->language_needs_save = 0;
    }
    practice_update_circle_bounds(app, app_content_top_reserved(app),
                                  app_content_bottom_reserved(app));
    habits_init(&app->habits);
    app_restore_habits_view_settings(app);
    app->habit_detail_index = -1;
    app->habit_session_edit = (HabitSessionEditState){.round = -1};
    app->inbe.screen = app->main_tab == APP_MAIN_TAB_HABITS
                           ? InbeScreenHabits
                           : InbeScreenStart;
    app->habits.focus_selected_tab = app->inbe.screen == InbeScreenHabits;
#if !defined(PLATFORM_WEB)
    if(!app_running_in_kryon_preview())
        init_audio(app);
#endif
    for(int i = 0; i < practice_count(); i++) {
        const PracticeDefinition *practice = practice_get(i);
        if(practice->init != NULL)
            practice->init(app);
    }
    app->camera = (Camera2D){0};
    app->play_circle_hover = 0;
    app->play_circle_scale = 1.0f;
    app->settings_tab = SETTINGS_TAB_DEVICE;
    app->habit_edit = (HabitEditState){
        .index = -1,
        .color = {99, 196, 165, 255},
        .sync_mode = INBE_HABIT_SYNC_NONE
    };
    practice_update_session_sounds(app);
    reset_settings_preview(app);
    inbeinit(&app->start_speed_preview);

    // Load all icons
    LoadAllUIIconTextures(app->icons);

    SetUIIcons(app->icons[UI_ICON_TYPE_GEAR], app->icons[UI_ICON_TYPE_X]);

    if(!app->language_selected)
        app->inbe.screen = InbeScreenLanguage;
    else
        app->inbe.screen = InbeScreenStart;
    app->modal = (InbeModal){0};
    app->meditation.duration_seconds = 0;
    app->meditation.remaining_seconds = 0;
    app->meditation.frame_ticks = 0;
    app_auto_sync(app);
}

static void
handle_back_button(InbeApp *app)
{
    /* If modal is active, let modal drawing handle it */
    if(app->modal.active) {
        return;
    }

    switch(app->inbe.screen) {
    case InbeScreenStart:
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

    case InbeScreenSettings:
        if(app->settings_dirty)
            save_settings(app);
        settings_screen_clear_status();
        if(app_return_to_nav_sidebar_if_needed(app))
            break;
        app_switch_screen(app, app->main_tab == APP_MAIN_TAB_HABITS
                                  ? InbeScreenHabits
                                  : InbeScreenStart);
        app->settings_scroll = 0;
        break;

    case InbeScreenProfile:
        app->profile_scroll = 0;
        app->sync_server_url_focused = 0;
        settings_screen_clear_status();
        if(app_return_to_nav_sidebar_if_needed(app))
            break;
        app_switch_screen(app, app->main_tab == APP_MAIN_TAB_HABITS
                                  ? InbeScreenHabits
                                  : InbeScreenStart);
        break;

    case InbeScreenCustomizeNav:
        if(app_return_to_nav_sidebar_if_needed(app))
            break;
        app_switch_screen(app, app->main_tab == APP_MAIN_TAB_HABITS
                                  ? InbeScreenHabits
                                  : InbeScreenStart);
        break;

    case InbeScreenNavSidebar:
        app_close_nav_sidebar(app);
        break;

    case InbeScreenPracticeConfig:
        app_leave_practice_config(app);
        app_switch_screen(app, InbeScreenStart);
        break;

    case InbeScreenHabits:
        if(app->habits.tab == HABIT_TAB_EDIT && app->habit_edit.active) {
            habit_edit_commit(app);
            break;
        }
        app_switch_screen(app, InbeScreenStart);
        break;

    case InbeScreenHabitEdit:
        habit_edit_cancel(app);
        app_switch_screen(app, InbeScreenHabits);
        break;

    case InbeScreenHabitSessionEdit:
        habit_session_cancel_edit(app);
        app_switch_screen(app, InbeScreenHabits);
        break;

    case InbeScreenLanguage:
        break;

    case InbeScreenManual:
        manual_screen_close_tutorial(app, 0);
        break;

    case InbeScreenResults:
        practice_ensure_results_saved(app);
#if ANDROID_BUILD
        android_allow_screen_off();
        android_wakelock_release();
#endif
        app_init(app);
        break;

    case InbeScreenSession:
        /* When paused, exit immediately */
        if(app->session_paused) {
            practice_active_background_stop(app);
            app_init(app);
        } else {
            /* Show confirmation modal */
            app_open_modal(app, UIModalConfirmExitSession);
        }
        break;

    case InbeScreenMeditation:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_MEDITATION);
            if(practice->request_exit != NULL)
                practice->request_exit(app);
        }
        break;

    case InbeScreenPatterns:
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
draw_profile_picture_picker_modal(InbeApp *app)
{
    UIProfilePicturePickerResult result;

    if(app == NULL)
        return;

    result = ProfilePicturePicker((UIProfilePicturePickerModal){
        .title = "Profile picture",
        .icons = app->icons,
        .selected_icon_type = &app->profile_picture_icon,
        .close_icon = app->icons[UI_ICON_TYPE_X],
        .max_width = 520,
        .scroll_offset = &app->profile_picture_picker_scroll
    });
    if(result.changed)
        save_settings(app);
    if(result.closed)
        app_close_modal(app);
}

static UITextInputStyle
app_donation_address_style(void)
{
    return (UITextInputStyle){
        .background = DarkenUIColor(GetThemeBackground(), 4),
        .border = GetThemeButton(),
        .focus_border = GetThemeButtonHover(),
        .text = GetThemeText(),
        .cursor = GetThemeText(),
        .radius = 0.08f,
        .padding_x = ScaleUIPx(10),
        .padding_y = ScaleUIPx(8),
    };
}

static int
app_donation_address_box_height(const char *address, int w, int font,
                                UITextInputStyle style)
{
    return UIGetNodeHeight(UINodeReadonlyTextBox((ReadonlyTextBoxProps){
        .bounds = {0, 0, (float)w, 0},
        .text = address,
        .font = font,
        .style = style,
        .line_gap = ScaleUIPx(2),
    }));
}

static int
app_donation_coin_section_height(const char *address, int w)
{
    int label_font = GetUIFontSize();
    int address_font = GetUISmallFontSize();
    int button_h = ScaleUIPx(36);

    return GetUITextLineHeight(label_font) +
           ScaleUIPx(6) +
           app_donation_address_box_height(address, w, address_font,
                                           app_donation_address_style()) +
           ScaleUIPx(8) + button_h + ScaleUIPx(18);
}

static char app_donation_bitcoin_text[96];
static char app_donation_monero_text[160];
static int app_donation_bitcoin_cursor;
static int app_donation_monero_cursor;
static int app_donation_bitcoin_focused;
static int app_donation_monero_focused;
static int app_donation_bitcoin_scroll;
static int app_donation_monero_scroll;

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
    int label_font = GetUIFontSize();
    int address_font = GetUISmallFontSize();
    int button_h = ScaleUIPx(36);
    int gap = ScaleUIPx(8);
    int button_w = (w - gap * 2) / 3;
    int box_h;
    int hover = 0;
    UITextInputStyle style = app_donation_address_style();

    if(y == NULL)
        return;

    if(address_text == NULL || address_text_size == 0 ||
       cursor == NULL || focused == NULL || scroll == NULL)
        return;

    snprintf(address_text, address_text_size, "%s", address);

    Text(label, x, *y, label_font, GetThemeText());
    *y += GetUITextLineHeight(label_font) + ScaleUIPx(6);

    box_h = app_donation_address_box_height(address, w, address_font, style);
    TextArea((TextAreaProps){
        .bounds = {(float)x, (float)*y, (float)w, (float)box_h},
        .text = address_text,
        .text_size = address_text_size,
        .cursor_position = cursor,
        .focused = focused,
        .scroll_y = scroll,
        .font = address_font,
        .line_gap = ScaleUIPx(2),
        .focus_id = focus_id,
        .style = style,
        .read_only = 1,
        .wrap = 1,
    });
    *y += box_h + ScaleUIPx(8);

    if(GenericButton(0, x, *y, button_w, button_h,
                     GetLocaleText("copy_address_button"),
                     UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
#if ANDROID_BUILD
        int android_copy_ok =
            android_device_copy_text_and_toast(address,
                                               GetLocaleText("address_copied"));
        if(!android_copy_ok) {
            SetUIClipboardTextValue(address);
            ShowUIToast(GetLocaleText("address_copied"));
        }
#else
        SetUIClipboardTextValue(address);
        ShowUIToast(GetLocaleText("address_copied"));
#endif
    }
    if(GenericButton(0, x + button_w + gap, *y, button_w, button_h,
                     GetLocaleText("open_wallet_button"),
                     UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
        if(!OpenURI(wallet_url))
            ShowUIToast(GetLocaleText("wallet_not_installed_toast"));
    }
    if(GenericButton(0, x + (button_w + gap) * 2, *y, button_w, button_h,
                     GetLocaleText("trocador_button"),
                     UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
        (void)OpenURI(trocador_url);
    }

    *y += button_h + ScaleUIPx(18);
}

static void
draw_about_donation_modal(InbeApp *app)
{
    UIPanelFrame frame;
    UIParagraphSpec message;
    int modal_w;
    int modal_h;
    int content_w;
    int y;
    int button_w;
    int button_h = ScaleUIPx(36);
    int hover = 0;

    if(app == NULL)
        return;

    modal_w = ScaleUIPx(460);
    if(modal_w > view_width - ScaleUIPx(24))
        modal_w = view_width - ScaleUIPx(24);
    if(modal_w < ScaleUIPx(240))
        modal_w = ScaleUIPx(240);
    content_w = modal_w - ScaleUIPx(36);

    message = (UIParagraphSpec){
        .text = GetLocaleText("about_donation_message"),
        .width = content_w,
        .font = GetUISmallFontSize(),
        .line_gap = ScaleUIPx(4),
        .color = DarkenUIColor(GetThemeText(), 28),
    };
    modal_h = ScaleUIPx(58) +
              UIGetNodeHeight(UINodeParagraph(message, 0, 0)) +
              ScaleUIPx(14) +
              app_donation_coin_section_height(app_bitcoin_donation_address(),
                                               content_w) +
              app_donation_coin_section_height(app_monero_donation_address(),
                                               content_w) +
              button_h + ScaleUIPx(16);

    frame = ModalFrame(modal_w, modal_h,
                       GetLocaleText("donation_reminder_title"),
                       (Texture2D){0}, app->icons[UI_ICON_TYPE_X]);
    if(frame.right_clicked) {
        app_close_modal(app);
        return;
    }

    y = frame.content_y;
    message.width = frame.content_w;
    Paragraph(message, frame.content_x, &y);
    y += ScaleUIPx(14);

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
                                   frame.content_x, frame.content_w, &y);
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
                                   frame.content_x, frame.content_w, &y);

    button_w = ScaleUIPx(120);
    if(button_w > frame.content_w)
        button_w = frame.content_w;
    if(GenericButton(0, frame.x + (frame.w - button_w) / 2,
                     frame.y + frame.h - button_h - ScaleUIPx(16),
                     button_w, button_h,
                     GetLocaleText("close_button"),
                     UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
        app_close_modal(app);
    }
}

static void
draw_donation_reminder_modal(InbeApp *app)
{
    int modal_result;
    UIModalAction actions[3];

    if(app == NULL)
        return;

    actions[0] = (UIModalAction){
        .label = GetLocaleText("donation_reminder_donate_button"),
        .style = UI_BUTTON_STYLE_PRIMARY
    };
    actions[1] = (UIModalAction){
        .label = GetLocaleText("donation_reminder_skip_button"),
        .style = UI_BUTTON_STYLE_SECONDARY
    };
    actions[2] = (UIModalAction){
        .label = GetLocaleText("donation_reminder_dismiss_button"),
        .style = UI_BUTTON_STYLE_DANGER
    };
    modal_result = ActionModal((ModalProps){
        .title = GetLocaleText("donation_reminder_title"),
        .message = GetLocaleText("donation_reminder_message"),
        .actions = actions,
        .action_count = 3,
        .close_icon = app->icons[UI_ICON_TYPE_X],
        .max_width = 420
    });
    if(modal_result == -1) {
        app_record_donation_reminder_seen(app);
        app_close_modal(app);
    } else if(modal_result > 0) {
        app_handle_donation_reminder_action(app, modal_result);
    }
}

static void
draw_global_modal(InbeApp *app)
{
    int modal_result;

    if(app == NULL || !app->modal.active)
        return;
    if(app->input_block_frame == app->inbe.frame)
        return;

    ClearUIInputCaptures();

    if(settings_data_draw_modals(app))
        return;

    if(app->modal.type == UIModalDonationReminder) {
        draw_donation_reminder_modal(app);
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
app_update_desktop_background_state(InbeApp *app)
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
static void app_draw_settings_screen(InbeApp *app)      { (void)settings_screen_draw(app); }
static void app_draw_profile_screen(InbeApp *app)       { (void)profile_screen_draw(app); }
static void app_draw_customize_nav_screen(InbeApp *app) { (void)app_draw_customize_nav_page(app); }

/* Simple screens: draw, then finish the frame. Adding a screen is a one-line
 * table entry. The practice screens (Start/Session/Meditation/SunSalutation/
 * Results) are dispatched separately below -- they share circle-bounds and
 * active-session logic that does not fit a flat table. */
static const struct {
    int screen;
    void (*draw)(InbeApp *app);
} g_simple_screens[] = {
    { InbeScreenSettings,         app_draw_settings_screen },
    { InbeScreenProfile,          app_draw_profile_screen },
    { InbeScreenCustomizeNav,     app_draw_customize_nav_screen },
    { InbeScreenNavSidebar,       NULL },
    { InbeScreenLanguage,         language_screen_draw },
    { InbeScreenHabits,           draw_habits_screen },
    { InbeScreenHabitEdit,        habit_edit_draw },
    { InbeScreenHabitSessionEdit, habit_session_draw_edit_screen },
    { InbeScreenBreak,            break_overlay_draw },
    { InbeScreenBreakExercises,   break_exercises_screen_draw },
};

static void
updateapp(InbeApp *app)
{
    int center_x = view_width / 2;
    int frame_view_height = view_height;
    int center_y;
    int hover = 0;
    AppRoute frame_route = app_current_route(app);
    int first_run_guide_active = 0;
    int habits_guide_active = 0;
    int practice_fullscreen_modal = 0;
    int global_modal_drawn = 0;
    int content_input_clip_active = 0;
    int bottom_input_reserved = 0;

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
                TraceLog(LOG_WARNING,
                         "AUDIO_E2E: main-thread start practice=%d track=%d mask=%d",
                         app->exercise_type,
                         app->meditation.music_practice_tracks[app->exercise_type],
                         app->meditation.music_practice_mask);
                __android_log_print(ANDROID_LOG_WARN, "INBE_AUDIO_E2E",
                                    "main-thread start practice=%d track=%d mask=%d",
                                    app->exercise_type,
                                    app->meditation.music_practice_tracks[app->exercise_type],
                                    app->meditation.music_practice_mask);
                if(practice != NULL && practice->start != NULL)
                    practice->start(app);
                TraceLog(LOG_WARNING,
                         "AUDIO_E2E: music loaded=%d playing=%d test=%d volume=%d",
                         app->meditation.music_loaded,
                         app->meditation.music_playing,
                         app->meditation.music_test_playing,
                         app->music_volume);
                __android_log_print(ANDROID_LOG_WARN, "INBE_AUDIO_E2E",
                                    "music loaded=%d playing=%d test=%d volume=%d valid=%d stream=%d",
                                    app->meditation.music_loaded,
                                    app->meditation.music_playing,
                                    app->meditation.music_test_playing,
                                    app->music_volume,
                                    IsMusicValid(app->meditation.music),
                                    IsMusicStreamPlaying(app->meditation.music));
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
    for(int i = 0; i < practice_count(); i++) {
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
    if(app->nav_sidebar_open)
        PushUIInputCapture((Rectangle){0, 0, (float)view_width,
                                       (float)view_height}, 0);
    if(app->modal.active)
        BeginUIModalLayer();
    if(app->close_prompt_open || first_run_guide_active || habits_guide_active) {
        PushUIInputCapture((Rectangle){0, 0, (float)view_width, (float)view_height}, 0);
    }

    view_height = app_page_height(app, view_height);
    center_y = view_height / 2;
    bottom_input_reserved = app_content_bottom_reserved(app);
    if(bottom_input_reserved > 0 && bottom_input_reserved < view_height) {
        PushUIInputClip((Rectangle){0, 0, (float)view_width,
                                       (float)(view_height - bottom_input_reserved)});
        content_input_clip_active = 1;
    }
    if(IsKeyPressed(KEY_BACK)
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
       || (IsKeyPressed(KEY_BACKSPACE) &&
           ((app->inbe.screen == InbeScreenStart &&
             app->practice_tab != PRACTICE_TAB_PLAY) ||
            app->inbe.screen == InbeScreenSession ||
            app->inbe.screen == InbeScreenMeditation ||
            app->inbe.screen == InbeScreenSunSalutation ||
            app->inbe.screen == InbeScreenPatterns))
#endif
       ) {
        if(app->nav_sidebar_open || app->inbe.screen == InbeScreenNavSidebar) {
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
    /* Ctrl+Q quits from any screen. Esc quits only when idle on the home
     * screen (Play tab, nothing open); deeper screens already bind Esc to
     * their own back/cancel handlers. */
    if(IsKeyPressed(KEY_Q) &&
       (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))) {
        app_request_desktop_quit(app);
    } else if(IsKeyPressed(KEY_ESCAPE) &&
              app->inbe.screen == InbeScreenStart &&
              app->practice_tab == PRACTICE_TAB_PLAY &&
              !app->nav_sidebar_open && !app->modal.active &&
              !first_run_guide_active && !habits_guide_active) {
        app_request_desktop_quit(app);
    }
#endif

    for(size_t i = 0; i < sizeof(g_simple_screens) / sizeof(g_simple_screens[0]); i++) {
        if(app->inbe.screen == g_simple_screens[i].screen) {
            if(g_simple_screens[i].draw != NULL)
                g_simple_screens[i].draw(app);
            goto finish_frame;
        }
    }

    if(app->inbe.screen == InbeScreenStart) {
        practice_update_circle_bounds(app, app_content_top_reserved(app),
                                      app_content_bottom_reserved(app));
    } else if(app->inbe.screen == InbeScreenSession) {
        practice_update_circle_bounds(app, UIGetNodeHeight(UINodeTitleBar(0)), 84);
    }

    if(app->inbe.screen == InbeScreenSession)
        practice_draw_active_breathing(app, center_x, center_y);

    switch (app->inbe.screen) {
    case InbeScreenStart:
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
           app->input_block_frame != app->inbe.frame) {
            const PracticeDefinition *practice = practice_get(PRACTICE_MEDITATION);
            if(practice->draw_setup_modal != NULL)
                practice->draw_setup_modal(app);
        }
        break;

    case InbeScreenSession:
        practice_update_active_breathing(app, center_x, center_y, &hover);
        break;

    case InbeScreenMeditation:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_MEDITATION);
            if(practice->draw_active_session != NULL)
                practice->draw_active_session(app, center_x, center_y);
        }
        break;

    case InbeScreenSunSalutation:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_SUN_SALUTATION);
            if(practice->draw_active_session != NULL)
                practice->draw_active_session(app, center_x, center_y);
        }
        break;
    case InbeScreenPatterns:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_PATTERNS);
            if(practice->draw_active_session != NULL)
                practice->draw_active_session(app, center_x, center_y);
        }
        break;

    case InbeScreenResults:
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
    DrawUIToast();
    app_flush_deferred_settings(app);
    app_observe_direct_route_change(app, frame_route);
    app->inbe.frame++;
}

void
app_update_draw(void *vapp, Rectangle viewport) {
    InbeApp *app = vapp;
    double profile_frame_start = app_profile_now();
    double profile_update_start;
    double profile_habits_start;
    double profile_sync_start;
    int full_width;
    int full_height;
    int content_x = 0;
    int content_w;

    if(app == 0 || viewport.width <= 0 || viewport.height <= 0)
        return;

    app_reload_graphics_resources(app);

    full_width = (int)viewport.width;
    full_height = (int)viewport.height;
    app_full_view_width = full_width;
    view_width = full_width;
    view_height = full_height;

    /* Update DPI cache */
    UpdateUIDPI(view_width, view_height);
    if(!(GetUIDPIScale() > 0.0f) || GetUIDPIScale() > 8.0f) {
        TraceLog(LOG_WARNING, "INBE_EMBED: repairing invalid dpi %.2f for %dx%d",
                 GetUIDPIScale(), view_width, view_height);
        InitUIDPI();
        UpdateUIDPI(view_width, view_height);
    }
    SetUIViewSize(view_width, view_height);

    {
        float user_scale = app->ui_scale_tenths > 0
                               ? (float)app->ui_scale_tenths / 10.0f
                               : 1.0f;
        InitUI(view_width, view_height, GetUIDPIScale() * user_scale);
    }
    if(app->modal.active)
        BeginUIModalLayer();
    practice_update_circle_bounds(app, app_content_top_reserved(app),
                                  app_content_bottom_reserved(app));

    app_device_preferences_update(app);
    inbe_update_check_poll();
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
            TraceLog(LOG_INFO, "INBE_EMBED: geometry viewport=%dx%d content_x=%d content=%dx%d dpi=%.2f",
                     full_width, full_height, content_x, content_w, full_height,
                     GetUIDPIScale());
            last_log_time = now;
        }
        if(changed) {
            last_full_w = full_width;
            last_full_h = full_height;
            last_content_x = content_x;
            last_content_w = content_w;
        }
    }
    view_width = content_w;
    view_height = full_height;
    SetUIViewSize(view_width, view_height);
    app->camera = (Camera2D){0};
    app->camera.zoom = 1.0f;
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
    } else {
    BeginUIClip((int)viewport.x + content_x, (int)viewport.y, content_w, full_height);
        BeginMode2D(app->camera);
            DrawRectangle(0, 0, view_width, view_height, GetThemeBackground());
            profile_update_start = app_profile_now();
            updateapp(app);
            Overlays();
            app_profile_accum(&g_app_profile.update_total,
                              &g_app_profile.update_max,
                              profile_update_start);
        EndMode2D();
    EndUIClip();
    }
    profile_habits_start = app_profile_now();
    habits_flush_save(app);
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
    InbeApp *app = vapp;
    if (app == NULL) return;

    CloseUIWindow(app->break_window);
    app->break_window = NULL;
    CloseUIWindow(app->break_hud);
    app->break_hud = NULL;

    save_settings(app);
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
    for(int i = 0; i < practice_count(); i++) {
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

    free(app);
}

int
app_should_use_tab_bar(const InbeApp *app)
{
    if(app == NULL)
        return 0;

    return app->navigation_mode == NAV_MODE_TABBAR;
}
