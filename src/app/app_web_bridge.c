#include "app.h"
#include "app_internal.h"
#include "data.h"
#include "sync_account.h"
#include "sync_client.h"
#include "storage.h"
#include "practices/practice_registry.h"
#include "practices/session_results.h"
#include "screens/practice_screen.h"
#include <stdio.h>
#include <string.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#if defined(PLATFORM_WEB)
EM_JS(int, web_extension_host_js, (void), {
    if(typeof window === 'undefined')
        return 0;
    if(window.__inbeExtension)
        return 1;
    return (window.location && window.location.protocol === 'chrome-extension:' &&
            typeof chrome !== 'undefined' && chrome.runtime) ? 1 : 0;
});

EM_JS(void, web_extension_break_now_js, (int break_type), {
    if (typeof window !== 'undefined' &&
        typeof window.__inbeExtensionBreakNow === 'function') {
        window.__inbeExtensionBreakNow(break_type);
    }
});

static SyncAccount web_test_source_account;
static uint8_t web_test_public_key[1312];
static uint8_t web_test_private_key[2560];
static int web_test_sync_key_import_status;
static char web_test_completed_session[FS_PATH_MAX];
static int web_test_completion_stage;
void app_web_launch_practice(int practice_id);

EMSCRIPTEN_KEEPALIVE
int app_web_test_complete_practice(void)
{
    InnerBreeze*app = get_global_app();
    if(app == NULL) return 0;
    web_test_completion_stage = 1;
    app->meditation.duration_mode = 5;
    app->meditation.custom_minutes = 1;
    app->meditation.show_extend_controls = 0;
    app->breathing.play_in_background = 1;
    app_web_launch_practice(EXERCISE_MEDITATION);
    web_test_completion_stage = 2;
    practice_active_advance_elapsed(app, 1000);
    app->session_paused = 1;
    practice_active_advance_elapsed(app, 30000);
    if(app->meditation.remaining_seconds != 59) return 0;
    web_test_completion_stage = 3;
    app->session_paused = 0;
    practice_active_advance_elapsed(app, 59000);
    if(!app->session_result.active || !app->session_result.saved) return 0;
    web_test_completion_stage = 4;
    snprintf(web_test_completed_session, sizeof(web_test_completed_session), "%s", app->session_result.path);
    storage_set_setting_text("test_completed_session", web_test_completed_session);
    app->session_result.mood = 4;
    session_result_done(app);
    web_test_completion_stage = app->session_result.active ? 5 : 6;
    sync_web_storage_critical();
    return !app->session_result.active;
}

EMSCRIPTEN_KEEPALIVE
int app_web_test_completion_stage(void) { return web_test_completion_stage; }

EMSCRIPTEN_KEEPALIVE
int app_web_test_completed_practice_persisted(void)
{
    StorageSessionCheckin checkin = {0};
    const char *path = storage_get_setting_text("test_completed_session");
    return path != NULL && storage_load_session_checkin(path, &checkin) &&
           checkin.mood_after == 4;
}

static void
web_test_bytes_to_hex(const uint8_t *bytes, size_t len, char *out, size_t out_size)
{
    static const char hex[] = "0123456789abcdef";

    if(bytes == NULL || out == NULL || out_size < len * 2 + 1)
        return;
    for(size_t i = 0; i < len; i++) {
        out[i * 2] = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    out[len * 2] = '\0';
}

static void
web_test_make_sync_account(SyncAccount *account)
{
    if(account == NULL)
        return;
    memset(account, 0, sizeof(*account));
    for(size_t i = 0; i < sizeof(web_test_public_key); i++)
        web_test_public_key[i] = (uint8_t)(i * 31U + 7U);
    for(size_t i = 0; i < sizeof(web_test_private_key); i++)
        web_test_private_key[i] = (uint8_t)(i * 17U + 3U);
    sync_sha256_hex(web_test_public_key, sizeof(web_test_public_key),
                    account->public_id);
    web_test_bytes_to_hex(web_test_public_key, sizeof(web_test_public_key),
                          account->public_key_hex,
                          sizeof(account->public_key_hex));
    web_test_bytes_to_hex(web_test_private_key, sizeof(web_test_private_key),
                          account->private_key_hex,
                          sizeof(account->private_key_hex));
}

static int
web_test_first_run_guide_expected(const InnerBreeze*app)
{
    return app != NULL &&
           !app->tutorial_seen &&
           !app->modal.active &&
           app->exercise_type != EXERCISE_SUN_SALUTATION &&
           app->breathing.screen == ScreenStart &&
           app->main_tab != APP_MAIN_TAB_NONE;
}

static void
web_test_save_onboarding_settings(const InnerBreeze*app)
{
    if(app == NULL)
        return;

    storage_settings_begin_write();
    storage_set_setting_text("language",
                             app->language_selected && !app->language_system &&
                                     app->language[0] != '\0'
                                 ? app->language
                                 : "");
    storage_set_setting_int("language_setup_done",
                            app->language_selected ? 1 : 0);
    storage_set_setting_int("tutorial_seen", app->tutorial_seen ? 1 : 0);
    storage_set_setting_int("tutorial_step", app->tutorial_step);
    storage_set_setting_int("habits_guide_seen",
                            app->habits_guide_seen ? 1 : 0);
    storage_set_setting_int("main_tab", app->main_tab);
    storage_settings_end_write();
}

EMSCRIPTEN_KEEPALIVE
int
app_web_get_play_in_background(void)
{
    InnerBreeze*app = get_global_app();
    return app != NULL && app->breathing.play_in_background;
}

EMSCRIPTEN_KEEPALIVE
void
app_web_set_backgrounded(int active)
{
    InnerBreeze*app = get_global_app();

    if(app == NULL)
        return;

    app->backgrounded = active ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
void
app_web_background_tick(int elapsed_ms)
{
    InnerBreeze*app = get_global_app();

    if(app == NULL || elapsed_ms <= 0)
        return;
    app->backgrounded = 1;
    practice_active_advance_elapsed(app, elapsed_ms);
}

EMSCRIPTEN_KEEPALIVE
void
app_web_launch_practice(int practice_id)
{
    InnerBreeze*app = get_global_app();
    const PracticeDefinition *practice;

    if(app == NULL)
        return;

    app->exercise_type = practice_clamp_id(practice_id);
    app->main_tab = APP_MAIN_TAB_PRACTICE;
    app->practice_tab = PRACTICE_TAB_PLAY;
    if(app->modal.active)
        app_close_modal(app);

    practice = practice_get(app->exercise_type);
    if(practice->start != NULL)
        practice->start(app);
}

EMSCRIPTEN_KEEPALIVE
int
app_web_extension_host(void)
{
    return web_extension_host_js();
}

EMSCRIPTEN_KEEPALIVE
void
app_web_extension_break_now(int break_type)
{
    if(!app_web_extension_host())
        return;
    web_extension_break_now_js(break_type);
}

EMSCRIPTEN_KEEPALIVE
int
app_web_extension_breaks_enabled(void)
{
    InnerBreeze*app = get_global_app();

    if(app == NULL || !app_web_extension_host())
        return 0;
    return app->breaks_enabled ? 1 : 0;
}

static BreakTimer *
web_extension_break_timer(InnerBreeze*app, int break_type)
{
    if(app == NULL || break_type < 0 || break_type >= BREAK_TYPE_COUNT)
        return NULL;
    return &app->breaks.timers[break_type];
}

EMSCRIPTEN_KEEPALIVE
int
app_web_extension_break_timer_enabled(int break_type)
{
    InnerBreeze*app = get_global_app();
    BreakTimer *timer = web_extension_break_timer(app, break_type);

    return timer != NULL && timer->enabled ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_extension_break_timer_limit_s(int break_type)
{
    InnerBreeze*app = get_global_app();
    BreakTimer *timer = web_extension_break_timer(app, break_type);

    return timer != NULL ? timer->limit_s : 0;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_extension_break_timer_duration_s(int break_type)
{
    InnerBreeze*app = get_global_app();
    BreakTimer *timer = web_extension_break_timer(app, break_type);

    return timer != NULL ? timer->duration_s : 0;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_extension_break_timer_postpone_s(int break_type)
{
    InnerBreeze*app = get_global_app();
    BreakTimer *timer = web_extension_break_timer(app, break_type);

    return timer != NULL ? timer->postpone_s : 0;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_extension_break_timer_max_prompts(int break_type)
{
    InnerBreeze*app = get_global_app();
    BreakTimer *timer = web_extension_break_timer(app, break_type);

    return timer != NULL ? timer->max_prompts : 0;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_extension_break_timer_show_skip(int break_type)
{
    InnerBreeze*app = get_global_app();
    BreakTimer *timer = web_extension_break_timer(app, break_type);

    return timer != NULL && timer->show_skip ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_extension_break_timer_show_postpone(int break_type)
{
    InnerBreeze*app = get_global_app();
    BreakTimer *timer = web_extension_break_timer(app, break_type);

    return timer != NULL && timer->show_postpone ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
void
app_web_extension_open_break_settings(void)
{
    InnerBreeze*app = get_global_app();

    if(app == NULL)
        return;
    app->settings_tab = SETTINGS_TAB_BREAKS;
    app->main_tab = APP_MAIN_TAB_PRACTICE;
    if(app->modal.active)
        app_close_modal(app);
    app_switch_screen(app, ScreenSettings);
}

EMSCRIPTEN_KEEPALIVE
void
app_web_extension_open_habits(void)
{
    InnerBreeze*app = get_global_app();

    if(app == NULL)
        return;
    app->main_tab = APP_MAIN_TAB_HABITS;
    app->habits.screen_mode = HABITS_SCREEN_OVERVIEW;
    app->habits.scroll = 0;
    app->habits.focus_selected_tab = 1;
    if(app->modal.active)
        app_close_modal(app);
    app_switch_screen(app, ScreenHabits);
}

EMSCRIPTEN_KEEPALIVE
void
app_web_test_save_onboarding_state(void)
{
    InnerBreeze*app = get_global_app();

    if(app == NULL)
        return;

    snprintf(app->language, sizeof(app->language), "%s", "es");
    app->language_system = 0;
    app->language_selected = 1;
    app->tutorial_seen = 1;
    app->habits_guide_seen = 1;
    web_test_save_onboarding_settings(app);
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_onboarding_state(void)
{
    InnerBreeze*app = get_global_app();

    if(app == NULL)
        return 0;

    return app->language_selected && strcmp(app->language, "es") == 0 &&
           app->tutorial_seen && app->habits_guide_seen;
}

EMSCRIPTEN_KEEPALIVE
void
app_web_test_show_first_run_guide(void)
{
    InnerBreeze*app = get_global_app();

    if(app == NULL)
        return;

    snprintf(app->language, sizeof(app->language), "%s", "es");
    app->language_system = 0;
    app->language_selected = 1;
    app->tutorial_seen = 0;
    app->tutorial_step = 0;
    app->habits_guide_seen = 1;
    app->main_tab = APP_MAIN_TAB_PRACTICE;
    app->practice_tab = PRACTICE_TAB_PLAY;
    app->exercise_type = EXERCISE_WIM_HOF;
    storage_settings_begin_write();
    storage_set_setting_text("sync_public_id", "");
    storage_set_setting_text("sync_public_key", "");
    storage_set_setting_text("sync_private_key", "");
    storage_set_setting_text("sync_account_alias", "");
    storage_set_setting_text("sync_auth_token", "");
    storage_set_setting_text("sync_auth_token_expires_at", "");
    storage_set_setting_int("sync_server_connected", 0);
    storage_settings_end_write();
    web_test_sync_key_import_status = 0;
    if(app->modal.active)
        app_close_modal(app);
    app->breathing.screen = ScreenStart;
    web_test_save_onboarding_settings(app);
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_first_run_guide_active(void)
{
    InnerBreeze*app = get_global_app();

    return web_test_first_run_guide_expected(app);
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_first_run_guide_step(void)
{
    InnerBreeze*app = get_global_app();

    return app != NULL ? app->tutorial_step : -1;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_first_run_guide_text_clipped(void)
{
    UIGuideOverlayDebug debug;

    if(!GetUIGuideOverlayDebug(&debug))
        return web_test_first_run_guide_expected(get_global_app()) ? 0 : -1;
    return debug.text_clipped;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_first_run_guide_next_x(void)
{
    UIGuideOverlayDebug debug;

    if(!GetUIGuideOverlayDebug(&debug))
        return -1;
    return (int)(debug.next_button.x + debug.next_button.width * 0.5f);
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_first_run_guide_next_y(void)
{
    UIGuideOverlayDebug debug;

    if(!GetUIGuideOverlayDebug(&debug))
        return -1;
    return (int)(debug.next_button.y + debug.next_button.height * 0.5f);
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_first_run_guide_close_x(void)
{
    UIGuideOverlayDebug debug;

    if(!GetUIGuideOverlayDebug(&debug))
        return -1;
    return (int)(debug.close_button.x + debug.close_button.width * 0.5f);
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_first_run_guide_close_y(void)
{
    UIGuideOverlayDebug debug;

    if(!GetUIGuideOverlayDebug(&debug))
        return -1;
    return (int)(debug.close_button.y + debug.close_button.height * 0.5f);
}

static int
web_test_rect_valid(Rectangle rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

static Rectangle
web_test_first_run_guide_action_anchor(InnerBreeze*app)
{
    static const Rectangle zero_rect;
    Rectangle manual;
    Rectangle config;
    Rectangle anchor;
    int manual_valid;
    int config_valid;
    float right;
    float bottom;
    float config_right;
    float config_bottom;

    if(app == NULL)
        return zero_rect;

    manual = app->practice_home_bounds_manual;
    config = app->practice_home_bounds_config;
    manual_valid = web_test_rect_valid(manual);
    config_valid = web_test_rect_valid(config);

    if(!manual_valid)
        return config_valid ? config : zero_rect;
    if(!config_valid)
        return manual;

    anchor = manual;
    if(config.x < anchor.x)
        anchor.x = config.x;
    if(config.y < anchor.y)
        anchor.y = config.y;
    right = manual.x + manual.width;
    bottom = manual.y + manual.height;
    config_right = config.x + config.width;
    config_bottom = config.y + config.height;
    if(config_right > right)
        right = config_right;
    if(config_bottom > bottom)
        bottom = config_bottom;
    anchor.width = right - anchor.x;
    anchor.height = bottom - anchor.y;
    return anchor;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_first_run_guide_anchor_x(void)
{
    Rectangle anchor = web_test_first_run_guide_action_anchor(get_global_app());

    return (int)anchor.x;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_first_run_guide_anchor_y(void)
{
    Rectangle anchor = web_test_first_run_guide_action_anchor(get_global_app());

    return (int)anchor.y;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_first_run_guide_anchor_w(void)
{
    Rectangle anchor = web_test_first_run_guide_action_anchor(get_global_app());

    return (int)anchor.width;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_first_run_guide_anchor_h(void)
{
    Rectangle anchor = web_test_first_run_guide_action_anchor(get_global_app());

    return (int)anchor.height;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_sync_key_state(void)
{
    SyncAccount *source = &web_test_source_account;

    if(web_test_sync_key_import_status != 1)
        return web_test_sync_key_import_status;
    if(!HasSyncAccountValues(source))
        return -10;
    return 1;
}

EMSCRIPTEN_KEEPALIVE
void
app_web_test_import_sync_key(void)
{
    SyncAccount *source = &web_test_source_account;

    web_test_sync_key_import_status = 0;
    data_init();

    web_test_make_sync_account(source);
    if(!HasSyncAccountValues(source)) {
        web_test_sync_key_import_status = -1;
        return;
    }
    storage_settings_begin_write();
    storage_set_setting_text("sync_public_id", source->public_id);
    storage_set_setting_text("sync_public_key", source->public_key_hex);
    storage_set_setting_text("sync_private_key", source->private_key_hex);
    storage_set_setting_text("sync_account_alias", "");
    storage_set_sync_server_connected(0);
    storage_settings_end_write();
    web_test_sync_key_import_status = 1;
}

EMSCRIPTEN_KEEPALIVE
void
app_web_test_show_practice_home(void)
{
    InnerBreeze*app = get_global_app();

    if(app == NULL)
        return;

    snprintf(app->language, sizeof(app->language), "%s", "es");
    app->language_system = 0;
    app->language_selected = 1;
    app->tutorial_seen = 1;
    app->main_tab = APP_MAIN_TAB_PRACTICE;
    app->practice_tab = PRACTICE_TAB_PLAY;
    app->exercise_type = EXERCISE_WIM_HOF;
    if(app->modal.active)
        app_close_modal(app);
    app->breathing.screen = ScreenStart;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_practice_selected(void)
{
    InnerBreeze*app = get_global_app();
    return app != NULL ? app->exercise_type : -1;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_screen(void)
{
    InnerBreeze*app = get_global_app();

    return app != NULL ? (int)app->breathing.screen : -1;
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_practice_start_click_x(void)
{
    InnerBreeze*app = get_global_app();

    if(app == NULL || !web_test_rect_valid(app->practice_home_bounds_start))
        return -1;
    return (int)(app->practice_home_bounds_start.x +
                 app->practice_home_bounds_start.width * 0.5f);
}

EMSCRIPTEN_KEEPALIVE
int
app_web_test_practice_start_click_y(void)
{
    InnerBreeze*app = get_global_app();

    if(app == NULL || !web_test_rect_valid(app->practice_home_bounds_start))
        return -1;
    return (int)(app->practice_home_bounds_start.y +
                 app->practice_home_bounds_start.height * 0.5f);
}

EMSCRIPTEN_KEEPALIVE
void
app_web_test_enable_extension_breaks(int limit_s)
{
    InnerBreeze*app = get_global_app();
    BreakTimer *timer;

    if(app == NULL || !app_web_extension_host())
        return;
    app->breaks_enabled = 1;
    timer = &app->breaks.timers[BREAK_REST];
    timer->enabled = 1;
    timer->limit_s = limit_s > 0 ? limit_s : 60;
    timer->duration_s = 60;
    timer->postpone_s = 300;
    timer->max_prompts = 1;
    timer->show_skip = 1;
    timer->show_postpone = 1;
    save_settings(app);
}
#endif
