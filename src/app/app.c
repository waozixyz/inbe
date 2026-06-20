#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define MMNOSOUND
#define NOMINMAX
#endif

#include "app.h"
#include "data.h"
#include "flint_locale.h"
#include "screens/language_screen.h"
#include "screens/manual_screen.h"
#include "screens/settings/settings_screen.h"
#include "screens/practice_screen.h"
#include "practices/practice_registry.h"
#include "device_preferences.h"
#include "storage.h"
#include "sync_account.h"
#include "sync_client.h"
#include "theme.h"
#include "version.h"
#include "flint_clip.h"
#include "flint_ui.h"
#include "flint_dpi.h"
#include "flint_text.h"
#include "flint_embedded_assets.h"
#include "practices/meditation/meditation_practice.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#ifdef __ANDROID__
#include "android_wakelock.h"
#include "android_timer.h"
#include "android_device.h"
void set_global_inbe_app(InbeApp *app);
#endif

#if defined(PLATFORM_WEB)
InbeApp *get_global_inbe_app(void);
#endif

#define INBE_DEFAULT_WIDTH 320
#define INBE_DEFAULT_HEIGHT 560
#define INBE_SYNC_SERVER_URL_KEY "sync_server_url"
#define INBE_SYNC_INPUT_QUIET_SECONDS 0.45

InbeConfig config = {
    .title = "Inner Breeze",
    .width = INBE_DEFAULT_WIDTH,
    .height = INBE_DEFAULT_HEIGHT,
    .loaded = 0,
    .title_custom = 1
};

static double g_dirty_sync_at = 0.0;
static double g_last_sync_input_at = 0.0;
static int g_sync_running = 0;
static int g_sync_refresh_pending = 0;
static int g_remote_sync_due = 0;

#if defined(PLATFORM_WEB)
static int g_web_background_remainder_ms = 0;
static int g_web_background_meditation_remainder_ms = 0;
#endif

#if !defined(PLATFORM_WEB)
typedef struct InbeSyncWorkerArgs {
    char url[256];
} InbeSyncWorkerArgs;

#if defined(_WIN32)
#define sync_thread_return DWORD WINAPI
#define sync_thread_done 0
#define sync_sleep(seconds) Sleep((DWORD)((seconds) * 1000))
#define sync_lock() AcquireSRWLockExclusive(&g_sync_mutex)
#define sync_unlock() ReleaseSRWLockExclusive(&g_sync_mutex)
typedef HANDLE SyncThread;
static SRWLOCK g_sync_mutex = SRWLOCK_INIT;
static int
sync_thread_start(SyncThread *thread, LPTHREAD_START_ROUTINE func, void *arg)
{
    *thread = CreateThread(NULL, 0, func, arg, 0, NULL);
    return *thread != NULL;
}
static void
sync_thread_detach(SyncThread thread)
{
    CloseHandle(thread);
}
#else
#define sync_thread_return void *
#define sync_thread_done NULL
#define sync_sleep(seconds) sleep(seconds)
#define sync_lock() pthread_mutex_lock(&g_sync_mutex)
#define sync_unlock() pthread_mutex_unlock(&g_sync_mutex)
typedef pthread_t SyncThread;
static pthread_mutex_t g_sync_mutex = PTHREAD_MUTEX_INITIALIZER;
static int
sync_thread_start(SyncThread *thread, void *(*func)(void *), void *arg)
{
    return pthread_create(thread, NULL, func, arg) == 0;
}
static void
sync_thread_detach(SyncThread thread)
{
    pthread_detach(thread);
}
#endif

static int g_sync_finished = 0;
static int g_sync_finished_result = INBE_SYNC_CLIENT_OK;
static int g_sync_finished_changed = 0;
static int g_sync_events_running = 0;
static char g_sync_events_url[256];

static sync_thread_return
app_sync_worker(void *userdata)
{
    InbeSyncWorkerArgs *args = userdata;
    InbeSyncClientResult result = INBE_SYNC_CLIENT_INVALID_URL;
    int changed = 0;

    if(args != NULL) {
        result = sync_client_sync(args->url);
        changed = result == INBE_SYNC_CLIENT_OK && storage_last_sync_changed();
        free(args);
    }

    sync_lock();
    g_sync_finished_result = result;
    g_sync_finished_changed = changed;
    g_sync_finished = 1;
    g_sync_running = 0;
    sync_unlock();
    return sync_thread_done;
}

static void
app_collect_finished_sync(void)
{
    int finished;
    int result;
    int changed;

    sync_lock();
    finished = g_sync_finished;
    result = g_sync_finished_result;
    changed = g_sync_finished_changed;
    g_sync_finished = 0;
    sync_unlock();

    if(!finished)
        return;
    if(result == INBE_SYNC_CLIENT_OK) {
        TraceLog(LOG_INFO, "SYNC: background sync complete changed=%d", changed);
        if(changed)
            g_sync_refresh_pending = 1;
    } else {
        TraceLog(LOG_WARNING, "SYNC: background sync failed result=%d name=%s",
                 result, sync_client_result_name(result));
    }
}

static sync_thread_return
app_sync_events_worker(void *userdata)
{
    InbeSyncWorkerArgs *args = userdata;

    if(args == NULL)
        return sync_thread_done;
    for(;;) {
        InbeSyncClientResult result;
        sync_lock();
        if(g_sync_running) {
            sync_unlock();
            sync_sleep(1);
            continue;
        }
        sync_unlock();

        result = sync_client_wait_for_remote_event(args->url);
        sync_lock();
        if(result == INBE_SYNC_CLIENT_OK)
            g_remote_sync_due = 1;
        sync_unlock();
        if(result != INBE_SYNC_CLIENT_OK)
            sync_sleep(2);
    }
    return sync_thread_done;
}

static void
app_ensure_sync_events(const char *url)
{
    SyncThread thread;
    InbeSyncWorkerArgs *args;

    if(url == NULL || url[0] == '\0')
        return;
    sync_lock();
    if(g_sync_events_running && strcmp(g_sync_events_url, url) == 0) {
        sync_unlock();
        return;
    }
    if(g_sync_events_running) {
        sync_unlock();
        return;
    }
    g_sync_events_running = 1;
    g_remote_sync_due = 1;
    snprintf(g_sync_events_url, sizeof(g_sync_events_url), "%s", url);
    sync_unlock();

    args = malloc(sizeof(*args));
    if(args == NULL) {
        sync_lock();
        g_sync_events_running = 0;
        g_sync_events_url[0] = '\0';
        sync_unlock();
        return;
    }
    snprintf(args->url, sizeof(args->url), "%s", url);
    if(!sync_thread_start(&thread, app_sync_events_worker, args)) {
        free(args);
        sync_lock();
        g_sync_events_running = 0;
        g_sync_events_url[0] = '\0';
        sync_unlock();
        TraceLog(LOG_WARNING, "SYNC: failed to start websocket event thread");
        return;
    }
    sync_thread_detach(thread);
    TraceLog(LOG_INFO, "SYNC: websocket event listener started");
}
#else
static char g_sync_events_url[256];
static double g_sync_events_retry_at = 0.0;

static void
app_collect_finished_sync(void)
{
    InbeSyncClientResult result;
    int changed = 0;

    if(sync_client_web_poll_remote_event())
        g_remote_sync_due = 1;
    if(!g_sync_running)
        return;
    if(!sync_client_web_sync_poll(&result, &changed))
        return;
    g_sync_running = 0;
    if(result == INBE_SYNC_CLIENT_OK) {
        TraceLog(LOG_INFO, "SYNC: background sync complete changed=%d", changed);
        if(changed)
            g_sync_refresh_pending = 1;
    } else {
        TraceLog(LOG_WARNING, "SYNC: background sync failed result=%d name=%s",
                 result, sync_client_result_name(result));
    }
}

static void
app_ensure_sync_events(const char *url)
{
    double now;

    if(url == NULL || url[0] == '\0')
        return;
    if(strcmp(g_sync_events_url, url) == 0) {
        sync_client_web_start_remote_events(url);
        return;
    }
    now = GetTime();
    if(now < g_sync_events_retry_at)
        return;
    if(!sync_client_web_start_remote_events(url)) {
        g_sync_events_retry_at = now + 2.0;
        return;
    }
    g_remote_sync_due = 1;
    snprintf(g_sync_events_url, sizeof(g_sync_events_url), "%s", url);
    TraceLog(LOG_INFO, "SYNC: websocket event listener started");
}
#endif

static int
app_background_sync_safe(const InbeApp *app)
{
    if(app == NULL)
        return 0;
    if(app->modal.active)
        return 0;
    if(app->inbe.screen == InbeScreenPracticeSession ||
       app->inbe.screen == InbeScreenMeditation)
        return 0;
    return 1;
}

static int
app_input_active(void)
{
    return IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
           IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
           IsMouseButtonReleased(MOUSE_BUTTON_LEFT) ||
           IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
           IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
           IsMouseButtonReleased(MOUSE_BUTTON_RIGHT);
}

static void
app_apply_pending_sync_refresh(InbeApp *app)
{
    if(!g_sync_refresh_pending || !app_background_sync_safe(app))
        return;
    g_sync_refresh_pending = 0;
    TraceLog(LOG_INFO, "SYNC: applying remote refresh");
    app_reload_after_import(app, 0);
}

static int
app_sync_url(char *url, size_t url_size)
{
    const char *saved_url;
    InbeSyncAccount account;

    if(url == NULL || url_size == 0)
        return 0;
    url[0] = '\0';
    if(!sync_account_load(&account))
        return 0;
    saved_url = storage_get_setting_text(INBE_SYNC_SERVER_URL_KEY);
    if(!sync_client_normalize_url(saved_url != NULL && saved_url[0] != '\0'
                                       ? saved_url
                                       : "https://api.waozi.xyz",
                                       url, url_size))
        return 0;
    return 1;
}

static int
app_sync_now(InbeApp *app)
{
    char url[256];

    if(app == NULL || !app_sync_url(url, sizeof(url)))
        return 0;
#if !defined(PLATFORM_WEB)
    {
        SyncThread thread;
        InbeSyncWorkerArgs *args;

        sync_lock();
        if(g_sync_running) {
            sync_unlock();
            return 0;
        }
        g_sync_running = 1;
        sync_unlock();

        args = malloc(sizeof(*args));
        if(args == NULL) {
            sync_lock();
            g_sync_running = 0;
            sync_unlock();
            return 0;
        }
        snprintf(args->url, sizeof(args->url), "%s", url);
        TraceLog(LOG_INFO, "SYNC: starting background sync");
        if(!sync_thread_start(&thread, app_sync_worker, args)) {
            free(args);
            sync_lock();
            g_sync_running = 0;
            sync_unlock();
            TraceLog(LOG_WARNING, "SYNC: failed to start background sync thread");
            return 0;
        }
        sync_thread_detach(thread);
        return 1;
    }
#else
    if(g_sync_running)
        return 0;
    g_sync_running = 1;
    TraceLog(LOG_INFO, "SYNC: starting background sync");
    if(!sync_client_web_sync_start(url)) {
        g_sync_running = 0;
        TraceLog(LOG_WARNING, "SYNC: failed to start background sync");
        return 0;
    }
    return 1;
#endif
}

int
app_auto_sync(InbeApp *app)
{
    double due;
    double now;
    char url[256];

    if(app == NULL || !app_sync_url(url, sizeof(url)))
        return 0;
    now = GetTime();
    g_last_sync_input_at = now;
    due = now + INBE_SYNC_INPUT_QUIET_SECONDS;
    if(g_dirty_sync_at <= 0.0 || due < g_dirty_sync_at)
        g_dirty_sync_at = due;
    TraceLog(LOG_INFO, "SYNC: queued local changes");
    return 1;
}

static void
app_pump_sync(InbeApp *app)
{
    double now;
    int should_sync = 0;
    int dirty_due = 0;
    char url[256];

    if(app == NULL)
        return;
    app_collect_finished_sync();
    app_apply_pending_sync_refresh(app);
    if(!app_sync_url(url, sizeof(url)))
        return;
    app_ensure_sync_events(url);
    now = GetTime();
    if(app_input_active())
        g_last_sync_input_at = now;
    if(g_dirty_sync_at > 0.0 && now >= g_dirty_sync_at) {
        should_sync = 1;
        dirty_due = 1;
    }
#if !defined(PLATFORM_WEB)
    sync_lock();
#endif
    if(g_remote_sync_due) {
        should_sync = 1;
        g_remote_sync_due = 0;
    }
#if !defined(PLATFORM_WEB)
    sync_unlock();
#endif
    if(!should_sync)
        return;
    if(now < g_last_sync_input_at + INBE_SYNC_INPUT_QUIET_SECONDS) {
        if(g_dirty_sync_at <= 0.0)
            g_dirty_sync_at = g_last_sync_input_at + INBE_SYNC_INPUT_QUIET_SECONDS;
        return;
    }
    if(!app_background_sync_safe(app)) {
        TraceLog(LOG_INFO, "SYNC: delayed; app is active");
        if(g_dirty_sync_at <= 0.0)
            g_dirty_sync_at = now + 1.0;
        return;
    }
    if(app_sync_now(app)) {
        if(dirty_due)
            g_dirty_sync_at = 0.0;
        TraceLog(LOG_INFO, "SYNC: dispatched");
    } else if(dirty_due) {
        g_dirty_sync_at = now + INBE_SYNC_INPUT_QUIET_SECONDS;
    }
    app_apply_pending_sync_refresh(app);
}

int view_width = INBE_DEFAULT_WIDTH;
int view_height = INBE_DEFAULT_HEIGHT;
/* Theme colors are now accessed via theme accessor functions */

#define LOCALE_FONT_PNG "assets/fonts/locales.png"
#define LOCALE_FONT_8_PNG "assets/fonts/locales-8.png"

#define LOCALE_FONT_DAT "assets/fonts/locales.dat"
#define LOCALE_FONT_8_DAT "assets/fonts/locales-8.dat"
#define LOCALE_FONT_BASE_SIZE 16
#define LOCALE_FONT_8_BASE_SIZE 8


static void
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

static void
app_open_main_tab(InbeApp *app, int main_tab, int persist)
{
    if(app == NULL)
        return;

    if(app->inbe.screen == InbeScreenPracticeConfig)
        app_leave_practice_config(app);

    app->main_tab = clampi(main_tab, APP_MAIN_TAB_HABITS, APP_MAIN_TAB_PRACTICE);
    app->inbe.screen = app->main_tab == APP_MAIN_TAB_HABITS
                           ? InbeScreenHabits
                           : InbeScreenStart;
    if(persist)
        save_settings(app);
}

enum {
    APP_BOTTOM_TAB_NONE = -1,
    APP_BOTTOM_TAB_HABITS = 0,
    APP_BOTTOM_TAB_PRACTICE = 1,
    APP_BOTTOM_TAB_SETTINGS = 2,
    APP_SETTINGS_SAVE_DELAY_TICKS = 30
};

static void
app_schedule_settings_save(InbeApp *app)
{
    if(app == NULL)
        return;
    app->settings_dirty = 1;
    app->settings_save_delay_ticks = APP_SETTINGS_SAVE_DELAY_TICKS;
}

void
app_request_bottom_tab(InbeApp *app, int bottom_tab)
{
    if(app == NULL)
        return;
    app->pending_bottom_tab = bottom_tab;
}

static void
app_apply_bottom_tab(InbeApp *app, int bottom_tab)
{
    if(app == NULL)
        return;

    switch(bottom_tab) {
    case APP_BOTTOM_TAB_HABITS:
        app_open_main_tab(app, APP_MAIN_TAB_HABITS, 1);
        break;
    case APP_BOTTOM_TAB_PRACTICE:
        app_open_main_tab(app, APP_MAIN_TAB_PRACTICE, 1);
        break;
    case APP_BOTTOM_TAB_SETTINGS:
        if(app->inbe.screen == InbeScreenPracticeConfig)
            app_leave_practice_config(app);
        reset_settings_preview(app);
        app->settings_tab = SETTINGS_TAB_DEVICE;
        app->settings_scroll = 0;
        app->inbe.screen = InbeScreenSettings;
        app_schedule_settings_save(app);
        break;
    default:
        break;
    }
}

static void
app_apply_pending_bottom_tab(InbeApp *app)
{
    int pending;

    if(app == NULL || app->pending_bottom_tab == APP_BOTTOM_TAB_NONE)
        return;

    pending = app->pending_bottom_tab;
    app->pending_bottom_tab = APP_BOTTOM_TAB_NONE;
    app_apply_bottom_tab(app, pending);
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

static int
app_should_draw_bottom_nav(const InbeApp *app)
{
    if(app == NULL || app->modal.active || app->habit_session_edit.active)
        return 0;
    switch(app->inbe.screen) {
    case InbeScreenStart:
    case InbeScreenSettings:
    case InbeScreenHabits:
    case InbeScreenHabitEdit:
        return 1;
    default:
        return 0;
    }
}

static void app_draw_bottom_nav(InbeApp *app);

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

static UITab g_tabs[] = {
    {NULL, {0}, UI_ICON_TYPE_HOME},
    {NULL, {0}, UI_ICON_TYPE_AMEN},
    {NULL, {0}, UI_ICON_TYPE_GEAR}
};

static UITabBar g_tab_bar = {g_tabs, 3};

static void
app_draw_bottom_nav(InbeApp *app)
{
    int tab;

    if(!app_should_draw_bottom_nav(app))
        return;
    tab = ui_draw_tab_bar(g_tab_bar.tabs, g_tab_bar.count);
    if(tab == 0)
        app_request_bottom_tab(app, APP_BOTTOM_TAB_HABITS);
    else if(tab == 1)
        app_request_bottom_tab(app, APP_BOTTOM_TAB_PRACTICE);
    else if(tab == 2)
        app_request_bottom_tab(app, APP_BOTTOM_TAB_SETTINGS);
}

static int
load_locale_font(InbeApp *app)
{
    Font font;
    Font font_8;
    Image white;
    const FlintEmbeddedAsset *png;
    const FlintEmbeddedAsset *dat;
    const FlintEmbeddedAsset *png_8;
    const FlintEmbeddedAsset *dat_8;

    if(app == NULL)
        return 0;

    png = flint_embedded_asset(LOCALE_FONT_PNG);
    dat = flint_embedded_asset(LOCALE_FONT_DAT);
    png_8 = flint_embedded_asset(LOCALE_FONT_8_PNG);
    dat_8 = flint_embedded_asset(LOCALE_FONT_8_DAT);
    if(png == NULL || dat == NULL || png_8 == NULL || dat_8 == NULL)
        return 0;

    font = flint_text_load_chopped_font_from_memory(png->data, png->size, dat->data, dat->size,
                                                    LOCALE_FONT_BASE_SIZE);
    if(font.texture.id == 0)
        return 0;
    font_8 = flint_text_load_chopped_font_from_memory(png_8->data, png_8->size,
                                                      dat_8->data, dat_8->size,
                                                      LOCALE_FONT_8_BASE_SIZE);
    if(font_8.texture.id == 0) {
        flint_text_unload_font(&font);
        return 0;
    }

    white = GenImageColor(1, 1, WHITE);
    app->font_shapes_texture = LoadTextureFromImage(white);
    UnloadImage(white);
    if(app->font_shapes_texture.id == 0) {
        flint_text_unload_font(&font);
        flint_text_unload_font(&font_8);
        return 0;
    }
    SetTextureFilter(app->font_shapes_texture, TEXTURE_FILTER_POINT);

    // Store the locale font in the app for use in text rendering
    app->locale_font = font;
    app->locale_font_8 = font_8;
    flint_text_set_font(font);
    flint_text_set_small_font(font_8);
    SetShapesTexture(app->font_shapes_texture, (Rectangle){0, 0, 1, 1});
    return 1;
}

static void
unload_locale_font(InbeApp *app)
{
    if(app == NULL)
        return;

    flint_text_set_font((Font){0});
    flint_text_set_small_font((Font){0});
    flint_text_unload_font(&app->locale_font);
    flint_text_unload_font(&app->locale_font_8);
}

static void
refresh_tab_labels(void)
{
    g_tabs[0].label = locale_get("tab_habits");
    g_tabs[1].label = locale_get("tab_practice");
    g_tabs[2].label = locale_get("tab_settings");
}

void
refresh_locale_dependent_text(InbeApp *app)
{
    if(app == NULL)
        return;

    refresh_tab_labels();
    if(!config.title_custom) {
        snprintf(config.title, sizeof(config.title), "%s", locale_get("app_title"));
    }
    manual_screen_reset_layouts(app);
    app->language_index = locale_current_index();
    if(app->language_index < 0)
        app->language_index = 0;
}

void
apply_language_selection(InbeApp *app, int language_index, int save_now)
{
    const char *code;

    if(app == NULL)
        return;

    if(language_index < 0 || language_index >= locale_count())
        language_index = 0;

    code = locale_code_at(language_index);
    if(code == NULL || code[0] == '\0')
        code = "en";

    if(!locale_set(code)) {
        code = "en";
        locale_set(code);
    }

    snprintf(app->language, sizeof(app->language), "%s", code);
    app->language_selected = 1;
    refresh_locale_dependent_text(app);

    if(save_now)
        save_settings(app);
}

#if defined(PLATFORM_WEB)
#include <emscripten.h>

static int web_storage_ready = 0;

static void
init_web_storage(void)
{
    int ok;

    if(web_storage_ready)
        return;

    ok = EM_ASM_INT({
        if(typeof FS === 'undefined' || typeof IDBFS === 'undefined')
            return 0;

        try {
            FS.mkdir('/home');
        } catch(e) {}

        try {
            if(!FS.analyzePath('/home').object.isFolder) return 0;
            FS.mount(IDBFS, {root: '/'}, '/home');
        } catch(e) {
            if(e.errno !== 10 && String(e).indexOf('already mounted') === -1) {
                console.error('IDBFS mount failed:', e);
                return 0;
            }
        }

        return 1;
    });
    web_storage_ready = ok != 0;
}

#endif

static void
load_config(void)
{
    if(config.loaded)
        return;

    if(config.title[0] == '\0') {
        snprintf(config.title, sizeof(config.title), "%s", locale_get("app_title"));
        config.title_custom = 0;
    }

    refresh_theme_colors(FLINT_THEME_SKY, 0);  /* Default: Sky light mode */

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

void
app_reload_after_import(InbeApp *app, int reload_settings)
{
    char selected_habit_id[INBE_HABIT_ID_SIZE] = "";
    char detail_habit_id[INBE_HABIT_ID_SIZE] = "";
    int selected = -1;
    int detail_index = -1;
    int view_mode = HABIT_VIEW_CALENDAR;
    int month_offset = 0;
    int scroll = 0;
    int weekly_days = 0;
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
        month_offset = app->habits.month_offset;
        scroll = app->habits.scroll;
        weekly_days = app->habits.weekly_days;
    }

    if(reload_settings) {
        if(app_load_settings(app))
            save_settings(app);
        reset_settings_preview(app);
        practice_update_session_sounds(app);
    }

    habits_init(&app->habits);
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
        app->habits.month_offset = month_offset;
        app->habits.scroll = scroll;
        app->habits.weekly_days = weekly_days;
    }
    if(app->habit_detail_index >= app->habits.count)
        app->habit_detail_index = -1;
    if(app->habits.selected < 0 || app->habits.selected >= app->habits.count)
        app->habits.selected = app->habits.count > 0 ? 0 : -1;
    app->habit_session_edit.active = 0;
    app->habit_edit.active = 0;
}

static Texture2D
load_pixel_texture_from_asset(const char *path)
{
    const FlintEmbeddedAsset *asset = flint_embedded_asset(path);
    Image image;
    Texture2D texture = {0};

    if(asset == NULL || asset->data == NULL || asset->size == 0)
        return texture;

    image = LoadImageFromMemory(flint_embedded_asset_extension(path), asset->data, (int)asset->size);
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

static Sound
load_sound_asset(const char *name)
{
    char path[96];
    const FlintEmbeddedAsset *asset;
    Wave wave;
    Sound sound = {0};

    snprintf(path, sizeof(path), "assets/sounds/%s", name);
    asset = flint_embedded_asset(path);
    if(asset == NULL || asset->data == NULL || asset->size == 0) {
        TraceLog(LOG_ERROR, "AUDIO: Missing embedded sound asset: %s", path);
        return sound;
    }

    wave = LoadWaveFromMemory(flint_embedded_asset_extension(path), asset->data, (int)asset->size);
    if(wave.data == NULL) {
        TraceLog(LOG_ERROR, "AUDIO: Failed to decode embedded sound asset: %s", path);
        return sound;
    }

    sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    if(sound.frameCount == 0)
        TraceLog(LOG_ERROR, "AUDIO: Failed to create sound from embedded asset: %s", path);
    else
        TraceLog(LOG_INFO, "AUDIO: Loaded sound asset %s (%u frames)", path, sound.frameCount);
    return sound;
}

void
app_play_sound(InbeApp *app, Sound sound, float scale)
{
    float volume;

    if(app == NULL)
        return;
    if(!app->audio_ready) {
        TraceLog(LOG_ERROR, "AUDIO: Cannot play sound because audio device is not ready");
        return;
    }
    if(sound.frameCount == 0) {
        TraceLog(LOG_ERROR, "AUDIO: Cannot play sound because sound is not loaded");
        return;
    }
    if(app->sound_volume <= 0)
        return;

    volume = ((float)app->sound_volume / 100.0f) * scale;
    if(volume < 0.0f)
        volume = 0.0f;
    if(volume > 1.0f)
        volume = 1.0f;

    StopSound(sound);
    SetSoundVolume(sound, volume);
    PlaySound(sound);
    if(!IsSoundPlaying(sound))
        TraceLog(LOG_ERROR, "AUDIO: PlaySound returned but sound is not playing");
}

static void
init_audio(InbeApp *app)
{
    if(app == NULL || app->audio_ready)
        return;

    InitAudioDevice();
    app->audio_ready = IsAudioDeviceReady();
    if(!app->audio_ready) {
        TraceLog(LOG_ERROR, "AUDIO: Audio device failed to initialize");
        return;
    }
    TraceLog(LOG_INFO, "AUDIO: Audio device initialized");

    app->breath_in_sound = load_sound_asset("breath-in.ogg");
    app->breath_out_sound = load_sound_asset("breath-out.ogg");
    app->bell_sound = load_sound_asset("bell.ogg");
}

void
app_init(void *vapp) {
    InbeApp *app = vapp;
    if(app == 0)
        return;

    // Initialize locale_font to empty
    app->locale_font = (Font){0};
    app->locale_font_8 = (Font){0};

#ifdef __ANDROID__
    if (app->inbe.screen == InbeScreenSession) {
        android_allow_screen_off();
    }
    if (app->inbe.play_in_background) {
        android_timer_stop();
        android_wakelock_release();
    }
#endif

#if defined(PLATFORM_WEB)
    init_web_storage();
#endif
    locale_init();
    if(!load_locale_font(app)) {
        TraceLog(LOG_WARNING, "FONT: Failed to load chopped locale font -> using built-in default");
    }
    refresh_tab_labels();
    load_config();

    view_width = config.width > 0 ? config.width : INBE_DEFAULT_WIDTH;
    view_height = config.height > 0 ? config.height : INBE_DEFAULT_HEIGHT;
    flint_dpi_update(view_width, view_height);
    ui_init(view_width, view_height, flint_dpi_scale());
#ifdef __ANDROID__
    flint_ui_set_text_input_platform_callback(android_device_set_soft_keyboard_visible);
#endif

    inbeinit(&app->inbe);
    data_init();
    if(app_load_settings(app))
        save_settings(app);
    if(app->language_needs_save) {
        save_settings(app);
        app->language_needs_save = 0;
    }
    practice_update_circle_bounds(app, flint_px(48), flint_px(56) + flint_px(80));
    habits_init(&app->habits);
    app->habit_detail_index = -1;
    app->habit_session_edit = (HabitSessionEditState){.round = -1};
    app->pending_bottom_tab = APP_BOTTOM_TAB_NONE;
    app_open_main_tab(app, app->main_tab, 0);
    init_audio(app);
    for(int i = 0; i < practice_count(); i++) {
        const PracticeDefinition *practice = practice_get(i);
        if(practice->init != NULL)
            practice->init(app);
    }
    app->camera = (Camera2D){0};
    app->cursor_clickable = 0;
    app->cursor_disabled = 0;
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
    flint_load_all_icons(app->icons);

    /* Update tab bar icons */
    g_tabs[0].icon = app->icons[UI_ICON_TYPE_HABIT];
    g_tabs[0].icon_type = UI_ICON_TYPE_NONE;
    g_tabs[1].icon = app->icons[UI_ICON_TYPE_AMEN];
    g_tabs[1].icon_type = UI_ICON_TYPE_AMEN;
    g_tabs[2].icon = app->icons[UI_ICON_TYPE_GEAR];
    g_tabs[2].icon_type = UI_ICON_TYPE_GEAR;

    ui_set_icons(app->icons[UI_ICON_TYPE_GEAR], app->icons[UI_ICON_TYPE_X]);

    if(!app->language_selected)
        app->inbe.screen = InbeScreenLanguage;
    else
        app->inbe.screen = InbeScreenStart;

    app->modal = (UIModal){0};
    app->meditation.duration_seconds = 0;
    app->meditation.remaining_seconds = 0;
    app->meditation.frame_ticks = 0;
    app_auto_sync(app);
}

#if defined(PLATFORM_WEB)
EMSCRIPTEN_KEEPALIVE
int
app_web_get_play_in_background(void)
{
    InbeApp *app = get_global_inbe_app();
    return app != NULL && app->inbe.play_in_background;
}

EMSCRIPTEN_KEEPALIVE
void
app_web_set_backgrounded(int active)
{
    InbeApp *app = get_global_inbe_app();

    if(app == NULL)
        return;

    app->backgrounded = active ? 1 : 0;
    if(!active) {
        g_web_background_remainder_ms = 0;
        g_web_background_meditation_remainder_ms = 0;
    }
}

EMSCRIPTEN_KEEPALIVE
void
app_web_background_tick(int elapsed_ms)
{
    InbeApp *app = get_global_inbe_app();
    int frame_count;
    int elapsed_total;

    if(app == NULL || elapsed_ms <= 0)
        return;
    app->backgrounded = 1;
    if(!app->inbe.play_in_background)
        return;

    if(app->inbe.screen == InbeScreenSession && !app->session_paused) {
        g_web_background_meditation_remainder_ms = 0;
        elapsed_total = elapsed_ms + g_web_background_remainder_ms;
        if(elapsed_total > 5 * 60 * 1000)
            elapsed_total = 5 * 60 * 1000;
        frame_count = (elapsed_total * 60) / 1000;
        g_web_background_remainder_ms = elapsed_total - (frame_count * 1000) / 60;

        for(int i = 0; i < frame_count && app->inbe.screen == InbeScreenSession; i++) {
            inbestep(&app->inbe);
            practice_update_session_sounds(app);
        }
        return;
    }

    g_web_background_remainder_ms = 0;
    if(app->inbe.screen == InbeScreenMeditation && !app->session_paused) {
        elapsed_total = elapsed_ms + g_web_background_meditation_remainder_ms;
        g_web_background_meditation_remainder_ms = elapsed_total % 1000;
        meditation_background_tick(app, elapsed_total - g_web_background_meditation_remainder_ms);
        if(app->inbe.screen == InbeScreenMeditation) {
            const PracticeDefinition *practice = practice_get(PRACTICE_MEDITATION);
            if(practice->update != NULL)
                practice->update(app);
        }
    }
}
#endif

static void
handle_back_button(InbeApp *app)
{
    /* If modal is active, let modal drawing handle it */
    if(app->modal.active) {
        return;
    }

    switch(app->inbe.screen) {
    case InbeScreenSettings:
        if(app->settings_dirty)
            save_settings(app);
        settings_screen_clear_status();
        app->inbe.screen = app->main_tab == APP_MAIN_TAB_HABITS
                               ? InbeScreenHabits
                               : InbeScreenStart;
        app->settings_scroll = 0;
        break;

    case InbeScreenPracticeConfig:
        app_leave_practice_config(app);
        app->inbe.screen = InbeScreenStart;
        break;

    case InbeScreenHabits:
        app->inbe.screen = InbeScreenStart;
        break;

    case InbeScreenHabitEdit:
        habit_edit_cancel(app);
        app->inbe.screen = InbeScreenHabits;
        break;

    case InbeScreenHabitSessionEdit:
        habit_session_cancel_edit(app);
        app->inbe.screen = InbeScreenHabits;
        break;

    case InbeScreenLanguage:
        break;

    case InbeScreenManual:
        manual_screen_close_tutorial(app, 0);
        break;

    case InbeScreenResults:
        practice_ensure_results_saved(app);
        app_init(app);
        break;

    case InbeScreenSession:
        /* When paused, exit immediately */
        if(app->session_paused) {
#ifdef __ANDROID__
            if (app->inbe.play_in_background) {
                android_wakelock_release();
                android_timer_stop();
            }
#endif
            app_init(app);
        } else {
            /* Show confirmation modal */
            app->modal.active = 1;
            app->modal.type = UIModalConfirmExitSession;
            app->modal.selected_button = 0;
        }
        break;

    case InbeScreenMeditation:
        {
            const PracticeDefinition *practice = practice_get(PRACTICE_MEDITATION);
            if(practice->request_exit != NULL)
                practice->request_exit(app);
        }
        break;

    case InbeScreenStart:
    default:
        break;
    }
}

static void
draw_global_modal(InbeApp *app)
{
    int modal_result;

    if(app == NULL || !app->modal.active)
        return;

    if(app->modal.type == UIModalMeditationNetworkError) {
        modal_result = ui_draw_modal(locale_get("meditation_music_network_error_title"),
                                     locale_get("meditation_music_network_error_message"),
                                     locale_get("ok_button"),
                                     locale_get("ok_button"));
        if(modal_result != 0) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
        }
    }
}

static void
updateapp(InbeApp *app)
{
    int center_x = view_width / 2;
    int center_y = view_height / 2;
    int hover = 0;

    app_apply_pending_bottom_tab(app);
    for(int i = 0; i < practice_count(); i++) {
        const PracticeDefinition *practice = practice_get(i);
        if(practice->update != NULL)
            practice->update(app);
    }
    if(app->practice_coming_soon_ticks > 0)
        app->practice_coming_soon_ticks--;

    if(IsKeyPressed(KEY_BACK)) {
        if(app->modal.active) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
        } else {
            handle_back_button(app);
        }
    }

    if(app->inbe.screen == InbeScreenSettings) {
        if(settings_screen_draw(app))
            goto finish_frame;
        app_draw_bottom_nav(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenPracticeConfig) {
        const PracticeDefinition *practice = practice_get(app->exercise_type);
        if(practice->draw_config != NULL)
            practice->draw_config(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenLanguage) {
        language_screen_draw(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenManual) {
        manual_screen_draw(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenHabits) {
        draw_habits_screen(app);
        app_draw_bottom_nav(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenHabitEdit) {
        draw_habit_edit_screen(app);
        app_draw_bottom_nav(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenHabitSessionEdit) {
        draw_habit_session_edit_screen(app);
        app_draw_bottom_nav(app);
        goto finish_frame;
    }

    if(app->inbe.screen == InbeScreenStart) {
        practice_update_circle_bounds(app, flint_px(48), flint_px(58) + flint_px(64));
    } else if(app->inbe.screen == InbeScreenSession) {
        practice_update_circle_bounds(app, 0, 84);
    }

    int play_circle_clicked = 0;
    if(app->inbe.screen == InbeScreenStart)
        play_circle_clicked = practice_draw_start_preview(app, center_x, center_y);
    else if(app->inbe.screen == InbeScreenSession)
        practice_draw_active_breathing(app, center_x, center_y);


    switch (app->inbe.screen) {
    case InbeScreenStart:
        DrawRectangle(0, 0, view_width, flint_px(58), flint_darken(theme_get_bg(), 14));
        DrawLine(0, flint_px(58) - 1, view_width, flint_px(58) - 1,
                 flint_darken(theme_get_bg(), 42));

        {
            const char *exercise_options[EXERCISE_COUNT];
            int exercise_values[EXERCISE_COUNT];
            int activity_count;
            int activity_index;
            int dropdown_w = flint_px(230);
            int dropdown_h = flint_px(36);
            int manual_icon_size = flint_px(20);
            int manual_icon_padding = flint_px(8);
            int manual_btn_w = manual_icon_size + manual_icon_padding * 2;
            int settings_icon_size = flint_px(20);
            int settings_icon_padding = flint_px(8);
            int settings_btn_w = settings_icon_size + settings_icon_padding * 2;
            int selector_gap = flint_px(8);
            int dropdown_x;
            int dropdown_y;
            int manual_x;
            int settings_x;
            int exercise_changed = 0;

            activity_count = EXERCISE_COUNT;
            for(int i = 0; i < activity_count; i++) {
                exercise_values[i] = i;
                exercise_options[i] = practice_activity_label(exercise_values[i]);
            }
            activity_index = clampi(app->exercise_type, 0, EXERCISE_COUNT - 1);

            settings_x = view_width - settings_btn_w - flint_px(10);
            manual_x = settings_x - selector_gap - manual_btn_w;
            dropdown_x = flint_px(12);
            dropdown_y = (flint_px(58) - dropdown_h) / 2;
            if(dropdown_x + dropdown_w > manual_x - selector_gap)
                dropdown_w = manual_x - selector_gap - dropdown_x;
            if(dropdown_w < flint_px(160))
                dropdown_w = flint_px(160);

            if(!app->modal.active &&
               ui_draw_dropdown_button(300, dropdown_x, dropdown_y, dropdown_w, dropdown_h,
                                       exercise_options, activity_count, &activity_index)) {
                app->exercise_type = exercise_values[activity_index];
                exercise_changed = 1;
            }
            if(!app->modal.active &&
               ui_draw_icon_btn_padded(manual_x, dropdown_y, manual_icon_size, manual_icon_padding,
                                       app->icons[UI_ICON_TYPE_MANUAL], &hover)) {
                app->tutorial_step = 0;
                app->manual_scroll = 0;
                app->inbe.screen = InbeScreenManual;
            }
            if(!app->modal.active &&
               ui_draw_icon_btn_padded(settings_x, dropdown_y, settings_icon_size, settings_icon_padding,
                                       app->icons[UI_ICON_TYPE_WRENCH], &hover)) {
                reset_settings_preview(app);
                app->settings_scroll = 0;
                app->practice_config_tab = 0;
                app->inbe.screen = InbeScreenPracticeConfig;
            }

            if(!app->modal.active && play_circle_clicked) {
                if(!exercise_manual_seen(app, app->exercise_type)) {
                    app->tutorial_step = 0;
                    app->manual_scroll = 0;
                    app->inbe.screen = InbeScreenManual;
                } else {
                    const PracticeDefinition *practice = practice_get(app->exercise_type);
                    if(practice->start != NULL)
                        practice->start(app);
                }
            }

            if(!app->modal.active && ui_draw_dropdown_menu(300)) {
                app->exercise_type = exercise_values[activity_index];
                exercise_changed = 1;
            }
            if(exercise_changed)
                save_settings(app);
        }
        app_draw_bottom_nav(app);
        if(app->modal.active && app->modal.type == UIModalMeditationSetup) {
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

    case InbeScreenResults:
        practice_draw_results(app, center_x, center_y, &hover);
        break;

    }

finish_frame:
    draw_global_modal(app);
    app_flush_deferred_settings(app);
    app->inbe.frame++;
}

void
app_update_draw(void *vapp, Rectangle viewport) {
    InbeApp *app = vapp;
    if(app == 0 || viewport.width <= 0 || viewport.height <= 0)
        return;

    view_width = (int)viewport.width;
    view_height = (int)viewport.height;

    /* Update DPI cache */
    flint_dpi_update(view_width, view_height);
    flint_set_view_size(view_width, view_height);

    ui_init(view_width, view_height, flint_dpi_scale());
    practice_update_circle_bounds(app, flint_px(48), flint_px(56) + flint_px(80));

    app->cursor_clickable = 0;
    app->cursor_disabled = 0;
    app->camera.zoom = 1.0f;
    app->camera.offset.x = viewport.x;
    app->camera.offset.y = viewport.y;
    ui_set_frame(app->camera);
    ui_set_cursor_clickable(&app->cursor_clickable);
    ui_set_cursor_disabled(&app->cursor_disabled);
    app_device_preferences_update(app);
    app_refresh_theme(app);

    DrawRectangleRec(viewport, theme_get_bg());
    flint_clip_begin((int)viewport.x, (int)viewport.y, (int)viewport.width, (int)viewport.height);
        BeginMode2D(app->camera);
            DrawRectangle(0, 0, view_width, view_height, theme_get_bg());
            updateapp(app);
        EndMode2D();
    flint_clip_end();
    habits_flush_save(app);
    app_pump_sync(app);
}

void
app_unload_texture(Texture2D texture) {
    if (texture.id != 0) {
        UnloadTexture(texture);
    }
}

static void SafeUnloadSound(Sound sound) {
    if (sound.frameCount != 0) {
        UnloadSound(sound);
    }
}

void
app_destroy(void *vapp)
{
    InbeApp *app = vapp;
    if (app == NULL) return;

    if(app->settings_dirty || app->settings_save_delay_ticks > 0)
        save_settings(app);
    habits_flush_save(app);

    // Unload all icons
    flint_unload_all_icons(app->icons);
    app_unload_texture(app->font_shapes_texture);
    unload_locale_font(app);

    SafeUnloadSound(app->breath_in_sound);
    SafeUnloadSound(app->breath_out_sound);
    SafeUnloadSound(app->bell_sound);
    for(int i = 0; i < practice_count(); i++) {
        const PracticeDefinition *practice = practice_get(i);
        if(practice->destroy != NULL)
            practice->destroy(app);
    }

    if (app->audio_ready) {
        CloseAudioDevice();
        app->audio_ready = 0;
    }

    free(app);
}
