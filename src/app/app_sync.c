#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define MMNOSOUND
#define NOMINMAX
#include <windows.h>
#endif

#include "app_sync.h"

#include "profile_social.h"
#include "storage.h"
#include "sync_account.h"
#include "sync_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#elif !defined(_WIN32)
#include <pthread.h>
#include <unistd.h>
#endif

#define INBE_SYNC_SERVER_URL_KEY "sync_server_url"
#define INBE_SYNC_SERVER_URL_DEFAULT "https://api.waozi.xyz"
#define INBE_SYNC_INPUT_QUIET_SECONDS 0.45
#define INBE_SOCIAL_ACTION_QUEUE_MAX 8

typedef struct InbeSyncCoordinator {
    double dirty_sync_at;
    double last_input_at;
    int sync_running;
    int data_refresh_pending;
    int social_refresh_pending;
    int social_refresh_running;
    int remote_sync_due;
    int social_action_head;
    int social_action_count;
    char social_actions[INBE_SOCIAL_ACTION_QUEUE_MAX][16];
    char social_action_values[INBE_SOCIAL_ACTION_QUEUE_MAX][96];
} InbeSyncCoordinator;

static InbeSyncCoordinator g_sync = {0};

#if !defined(PLATFORM_WEB)
typedef struct InbeSyncWorkerArgs {
    char url[256];
} InbeSyncWorkerArgs;

typedef struct InbeSocialWorkerArgs {
    char url[256];
    char practice[32];
    char metric[32];
    char leaderboard_key[96];
    char action[16];
    char action_value[96];
} InbeSocialWorkerArgs;

typedef struct InbeSyncWorkerResult {
    int finished;
    int result;
    int changed;
} InbeSyncWorkerResult;

typedef struct InbeSocialWorkerResult {
    int finished;
    int result;
    char friend_requests_json[8192];
    char friends_json[8192];
    char leaderboard_json[8192];
    char leaderboard_key[96];
} InbeSocialWorkerResult;

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

static InbeSyncWorkerResult g_sync_result = {0, LYRA_SYNC_OK, 0};
static InbeSocialWorkerResult g_social_result = {0, LYRA_SYNC_OK, "", "", "", ""};
static int g_sync_events_running = 0;
static char g_sync_events_url[256];
#else
static char g_sync_events_url[256];
static double g_sync_events_retry_at = 0.0;
#endif

static int app_sync_url(char *url, size_t url_size);
static int app_background_sync_safe(const InbeApp *app);
static int app_input_active(void);
static int app_sync_now(InbeApp *app);
static const char *app_modal_type_name(UIModalType type);
static const char *app_social_practice_id(int practice);
static const char *app_social_metric_id(int practice, int metric);
static void app_social_leaderboard_key(char *out, size_t out_size,
                                       int practice, int metric);
static LyraSyncResult app_social_fetch(const char *url, const char *practice,
                                       const char *metric, const char *action,
                                       const char *action_value,
                                       char *requests_json, size_t requests_size,
                                       char *friends_json, size_t friends_size,
                                       char *leaderboard_json,
                                       size_t leaderboard_size);
static void app_social_apply_result(InbeApp *app, const char *requests_json,
                                    const char *friends_json,
                                    const char *leaderboard_json,
                                    const char *leaderboard_key);

static void
app_queue_social_refresh(void)
{
    g_sync.social_refresh_pending = 1;
}

static int
app_social_peek_action(char *action, size_t action_size,
                       char *value, size_t value_size)
{
    int index;

    if(action != NULL && action_size > 0)
        action[0] = '\0';
    if(value != NULL && value_size > 0)
        value[0] = '\0';
    if(g_sync.social_action_count <= 0)
        return 0;
    index = g_sync.social_action_head;
    if(action != NULL && action_size > 0)
        snprintf(action, action_size, "%s", g_sync.social_actions[index]);
    if(value != NULL && value_size > 0)
        snprintf(value, value_size, "%s", g_sync.social_action_values[index]);
    return 1;
}

static int
app_social_pop_action(char *action, size_t action_size,
                      char *value, size_t value_size)
{
    int index;

    if(action != NULL && action_size > 0)
        action[0] = '\0';
    if(value != NULL && value_size > 0)
        value[0] = '\0';
    if(g_sync.social_action_count <= 0)
        return 0;
    index = g_sync.social_action_head;
    if(action != NULL && action_size > 0)
        snprintf(action, action_size, "%s", g_sync.social_actions[index]);
    if(value != NULL && value_size > 0)
        snprintf(value, value_size, "%s", g_sync.social_action_values[index]);
    g_sync.social_actions[index][0] = '\0';
    g_sync.social_action_values[index][0] = '\0';
    g_sync.social_action_head = (g_sync.social_action_head + 1) %
                                INBE_SOCIAL_ACTION_QUEUE_MAX;
    g_sync.social_action_count--;
    return 1;
}

static int
app_social_push_action(const char *action, const char *value)
{
    int index;

    if(action == NULL || action[0] == '\0' || value == NULL || value[0] == '\0')
        return 0;
    if(g_sync.social_action_count >= INBE_SOCIAL_ACTION_QUEUE_MAX) {
        TraceLog(LOG_WARNING, "SYNC: social action queue full");
        return 0;
    }
    index = (g_sync.social_action_head + g_sync.social_action_count) %
            INBE_SOCIAL_ACTION_QUEUE_MAX;
    snprintf(g_sync.social_actions[index], sizeof(g_sync.social_actions[index]),
             "%s", action);
    snprintf(g_sync.social_action_values[index],
             sizeof(g_sync.social_action_values[index]), "%s", value);
    g_sync.social_action_count++;
    g_sync.social_refresh_pending = 1;
    return 1;
}

static LyraSyncResult
app_social_fetch(const char *url, const char *practice, const char *metric,
                 const char *action, const char *action_value,
                 char *requests_json, size_t requests_size,
                 char *friends_json, size_t friends_size,
                 char *leaderboard_json, size_t leaderboard_size)
{
    LyraSyncResult result = LYRA_SYNC_INVALID_URL;
    LyraSyncResult friends_result = LYRA_SYNC_REQUEST_FAILED;
    LyraSyncResult leaderboard_result = LYRA_SYNC_REQUEST_FAILED;

    if(requests_json != NULL && requests_size > 0)
        snprintf(requests_json, requests_size, "{\"incoming\":[],\"outgoing\":[]}");
    if(friends_json != NULL && friends_size > 0)
        snprintf(friends_json, friends_size, "{\"friends\":[]}");
    if(leaderboard_json != NULL && leaderboard_size > 0)
        snprintf(leaderboard_json, leaderboard_size, "{\"rows\":[]}");
    if(url == NULL || url[0] == '\0')
        return LYRA_SYNC_INVALID_URL;

    if(action != NULL && strcmp(action, "send") == 0) {
        result = sync_client_send_friend_request(url, action_value);
        if(result != LYRA_SYNC_OK)
            return result;
    } else if(action != NULL && strcmp(action, "accept") == 0) {
        result = sync_client_accept_friend_request(url, action_value);
        if(result != LYRA_SYNC_OK)
            return result;
    } else if(action != NULL && strcmp(action, "decline") == 0) {
        result = sync_client_decline_friend_request(url, action_value);
        if(result != LYRA_SYNC_OK)
            return result;
    } else if(action != NULL && strcmp(action, "remove") == 0) {
        result = sync_client_remove_friend(url, action_value);
        if(result != LYRA_SYNC_OK)
            return result;
    }

    result = sync_client_get_friend_requests(url, requests_json, requests_size);
    if(result != LYRA_SYNC_OK)
        return result;
    friends_result = sync_client_get_friends(url, friends_json, friends_size);
    if(friends_result != LYRA_SYNC_OK)
        return friends_result;
    leaderboard_result = sync_client_get_friend_stats(url, "inbe", practice,
                                                      metric, leaderboard_json,
                                                      leaderboard_size);
    return leaderboard_result;
}

static void
app_social_apply_result(InbeApp *app, const char *requests_json,
                        const char *friends_json,
                        const char *leaderboard_json,
                        const char *leaderboard_key)
{
    storage_set_social_cache_json("friends.requests", requests_json);
    storage_set_social_cache_json("friends.list", friends_json);
    if(leaderboard_key != NULL && leaderboard_key[0] != '\0')
        storage_set_social_cache_json(leaderboard_key, leaderboard_json);
    if(app != NULL) {
        snprintf(app->profile_friend_requests_json,
                 sizeof(app->profile_friend_requests_json), "%s",
                 requests_json != NULL ? requests_json : "{\"incoming\":[],\"outgoing\":[]}");
        snprintf(app->profile_friends_json, sizeof(app->profile_friends_json),
                 "%s", friends_json != NULL ? friends_json : "{\"friends\":[]}");
        if(leaderboard_key != NULL && leaderboard_key[0] != '\0') {
            char current_key[96];
            app_social_leaderboard_key(current_key, sizeof(current_key),
                                       app->profile_leaderboard_practice,
                                       app->profile_leaderboard_metric);
            if(strcmp(current_key, leaderboard_key) == 0)
                snprintf(app->profile_leaderboard_json,
                         sizeof(app->profile_leaderboard_json), "%s",
                         leaderboard_json != NULL ? leaderboard_json : "{\"rows\":[]}");
        }
        app->profile_friends_loaded = 1;
        app->profile_leaderboard_loaded = 1;
    }
}

#if !defined(PLATFORM_WEB)
static sync_thread_return
app_sync_worker(void *userdata)
{
    InbeSyncWorkerArgs *args = userdata;
    LyraSyncResult result = LYRA_SYNC_INVALID_URL;
    int changed = 0;

    if(args != NULL) {
        result = sync_client_sync(args->url);
        changed = result == LYRA_SYNC_OK && storage_last_sync_changed();
        free(args);
    }

    sync_lock();
    g_sync_result.result = result;
    g_sync_result.changed = changed;
    g_sync_result.finished = 1;
    g_sync.sync_running = 0;
    sync_unlock();
    return sync_thread_done;
}

static sync_thread_return
app_social_worker(void *userdata)
{
    InbeSocialWorkerArgs *args = userdata;
    LyraSyncResult result = LYRA_SYNC_INVALID_URL;
    char requests_json[8192];
    char friends_json[8192];
    char leaderboard_json[8192];

    if(args != NULL)
        result = app_social_fetch(args->url, args->practice, args->metric,
                                  args->action, args->action_value,
                                  requests_json, sizeof(requests_json),
                                  friends_json, sizeof(friends_json),
                                  leaderboard_json, sizeof(leaderboard_json));
    else {
        snprintf(requests_json, sizeof(requests_json), "{\"incoming\":[],\"outgoing\":[]}");
        snprintf(friends_json, sizeof(friends_json), "{\"friends\":[]}");
        snprintf(leaderboard_json, sizeof(leaderboard_json), "{\"rows\":[]}");
    }

    sync_lock();
    g_social_result.result = result;
    snprintf(g_social_result.friend_requests_json,
             sizeof(g_social_result.friend_requests_json),
             "%s", requests_json);
    snprintf(g_social_result.friends_json, sizeof(g_social_result.friends_json), "%s",
             friends_json);
    snprintf(g_social_result.leaderboard_json,
             sizeof(g_social_result.leaderboard_json), "%s",
             leaderboard_json);
    snprintf(g_social_result.leaderboard_key, sizeof(g_social_result.leaderboard_key), "%s",
             args != NULL ? args->leaderboard_key : "");
    g_social_result.finished = 1;
    g_sync.social_refresh_running = 0;
    sync_unlock();
    free(args);
    return sync_thread_done;
}

static sync_thread_return
app_sync_events_worker(void *userdata)
{
    InbeSyncWorkerArgs *args = userdata;

    if(args == NULL)
        return sync_thread_done;
    for(;;) {
        LyraSyncResult result;

        sync_lock();
        if(g_sync.sync_running) {
            sync_unlock();
            sync_sleep(1);
            continue;
        }
        sync_unlock();

        result = sync_client_wait_for_remote_event(args->url);
        sync_lock();
        if(result == LYRA_SYNC_OK)
            g_sync.remote_sync_due = 1;
        sync_unlock();
        if(result != LYRA_SYNC_OK)
            sync_sleep(2);
    }
    return sync_thread_done;
}
#endif

static void
app_collect_finished_sync(void)
{
#if !defined(PLATFORM_WEB)
    int finished;
    int result;
    int changed;

    sync_lock();
    finished = g_sync_result.finished;
    result = g_sync_result.result;
    changed = g_sync_result.changed;
    g_sync_result.finished = 0;
    sync_unlock();

    if(!finished)
        return;
#else
    LyraSyncResult result;
    int changed = 0;

    if(sync_client_web_poll_remote_event())
        g_sync.remote_sync_due = 1;
    if(!g_sync.sync_running)
        return;
    if(!sync_client_web_sync_poll(&result, &changed))
        return;
    g_sync.sync_running = 0;
#endif

    if(result == LYRA_SYNC_OK) {
        TraceLog(LOG_INFO, "SYNC: background sync complete changed=%d", changed);
        app_queue_social_refresh();
        if(changed)
            g_sync.data_refresh_pending = 1;
    } else {
        TraceLog(LOG_WARNING, "SYNC: background sync failed result=%d name=%s",
                 result, GetLyraSyncResultName(result));
    }
}

static void
app_ensure_sync_events(const char *url)
{
#if !defined(PLATFORM_WEB)
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
    g_sync.remote_sync_due = 1;
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
#else
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
    g_sync.remote_sync_due = 1;
    snprintf(g_sync_events_url, sizeof(g_sync_events_url), "%s", url);
    TraceLog(LOG_INFO, "SYNC: websocket event listener started");
#endif
}

static int
app_background_sync_safe(const InbeApp *app)
{
    if(app == NULL)
        return 0;
    if(app->modal.active)
        return 0;
    if(app->habit_edit.active ||
       app->sync_server_url_focused ||
       app->sync_alias_focused ||
       app->profile_friend_input_focused)
        return 0;
    if(app->inbe.screen == InbeScreenPracticeSession ||
       app->inbe.screen == InbeScreenMeditation ||
       app->inbe.screen == InbeScreenSunSalutation)
        return 0;
    return 1;
}

static const char *
app_modal_type_name(UIModalType type)
{
    switch(type) {
    case UIModalNone: return "none";
    case UIModalConfirmExitSession: return "confirm_exit_session";
    case UIModalMeditationSetup: return "meditation_setup";
    case UIModalConfirmDeleteData: return "confirm_delete_data";
    case UIModalConfirmDeleteHabit: return "confirm_delete_habit";
    case UIModalEditProgressiveStartSpeed: return "edit_progressive_start_speed";
    case UIModalMeditationNetworkError: return "meditation_network_error";
    case UIModalConfirmImportDataSettings: return "confirm_import_data_settings";
    case UIModalSyncAccountBackup: return "sync_account_backup";
    case UIModalConfirmDeleteSyncAccount: return "confirm_delete_sync_account";
    case UIModalHabitPracticeListInfo: return "habit_practice_list_info";
    case UIModalHabitCountingInfo: return "habit_counting_info";
    case UIModalPracticeManual: return "practice_manual";
    case UIModalPracticeConfig: return "practice_config";
    case UIModalThemePicker: return "theme_picker";
    case UIModalSyncReview: return "sync_review";
    case UIModalSyncAlias: return "sync_alias";
    case UIModalSyncPublicId: return "sync_public_id";
    case UIModalConfirmRemoveFriend: return "confirm_remove_friend";
    case UIModalConfirmSyncAccountSwitch: return "confirm_sync_account_switch";
    case UIModalBottomNavConfig: return "bottom_nav_config";
    case UIModalProfilePicturePicker: return "profile_picture_picker";
    default: break;
    }
    return "unknown";
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
    if(!g_sync.data_refresh_pending || !app_background_sync_safe(app))
        return;
    g_sync.data_refresh_pending = 0;
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
                                       : INBE_SYNC_SERVER_URL_DEFAULT,
                                       url, url_size))
        return 0;
    return 1;
}

static const char *
app_social_practice_id(int practice)
{
    switch(practice) {
    case EXERCISE_MEDITATION:
        return "meditation";
    case EXERCISE_SUN_SALUTATION:
        return "sun_salutation";
    case EXERCISE_WIM_HOF:
    default:
        return "whm";
    }
}

static const char *
app_social_metric_id(int practice, int metric)
{
    if(metric != 1)
        return "streak";
    return practice == EXERCISE_MEDITATION ? "avg_time" : "avg_hold";
}

static void
app_social_leaderboard_key(char *out, size_t out_size, int practice, int metric)
{
    if(out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "leaderboard.inbe.%s.%s",
             app_social_practice_id(practice),
             app_social_metric_id(practice, metric));
}

static void
app_apply_pending_social_refresh(InbeApp *app)
{
    char url[256];
    char action[16] = "";
    char action_value[96] = "";

    if(!g_sync.social_refresh_pending || !app_background_sync_safe(app))
        return;
    if(g_sync.dirty_sync_at > 0.0)
        return;
#if !defined(PLATFORM_WEB)
    {
        SyncThread thread;
        InbeSocialWorkerArgs *args;

        if(!app_sync_url(url, sizeof(url))) {
            sync_lock();
            g_sync.social_refresh_pending = g_sync.social_action_count > 0;
            sync_unlock();
            return;
        }
        args = malloc(sizeof(*args));
        if(args == NULL)
            return;
        sync_lock();
        if(g_sync.sync_running || g_sync.social_refresh_running) {
            sync_unlock();
            free(args);
            return;
        }
        app_social_peek_action(action, sizeof(action), action_value,
                               sizeof(action_value));
        g_sync.social_refresh_pending = 0;
        g_sync.social_refresh_running = 1;
        sync_unlock();

        snprintf(args->url, sizeof(args->url), "%s", url);
        snprintf(args->practice, sizeof(args->practice), "%s",
                 app_social_practice_id(app->profile_leaderboard_practice));
        snprintf(args->metric, sizeof(args->metric), "%s",
                 app_social_metric_id(app->profile_leaderboard_practice,
                                      app->profile_leaderboard_metric));
        app_social_leaderboard_key(args->leaderboard_key,
                                   sizeof(args->leaderboard_key),
                                   app->profile_leaderboard_practice,
                                   app->profile_leaderboard_metric);
        snprintf(args->action, sizeof(args->action), "%s", action);
        snprintf(args->action_value, sizeof(args->action_value), "%s", action_value);
        TraceLog(LOG_INFO, "SYNC: starting background social refresh");
        if(!sync_thread_start(&thread, app_social_worker, args)) {
            free(args);
            sync_lock();
            g_sync.social_refresh_running = 0;
            g_sync.social_refresh_pending = 1;
            sync_unlock();
            TraceLog(LOG_WARNING, "SYNC: failed to start social refresh thread");
            return;
        }
        sync_lock();
        app_social_pop_action(action, sizeof(action), action_value,
                              sizeof(action_value));
        sync_unlock();
        sync_thread_detach(thread);
    }
#else
    if(!app_sync_url(url, sizeof(url))) {
        g_sync.social_refresh_pending = g_sync.social_action_count > 0;
        return;
    }
    app_social_peek_action(action, sizeof(action), action_value,
                           sizeof(action_value));
    {
        char requests_json[8192];
        char friends_json[8192];
        char leaderboard_json[8192];
        char leaderboard_key[96];
        LyraSyncResult result;

        app_social_leaderboard_key(leaderboard_key, sizeof(leaderboard_key),
                                   app->profile_leaderboard_practice,
                                   app->profile_leaderboard_metric);
        TraceLog(LOG_INFO, "SYNC: refreshing social cache");
        result = app_social_fetch(url,
                                  app_social_practice_id(app->profile_leaderboard_practice),
                                  app_social_metric_id(app->profile_leaderboard_practice,
                                                       app->profile_leaderboard_metric),
                                  action, action_value,
                                  requests_json, sizeof(requests_json),
                                  friends_json, sizeof(friends_json),
                                  leaderboard_json, sizeof(leaderboard_json));
        if(result == LYRA_SYNC_OK)
            app_social_apply_result(app, requests_json, friends_json,
                                    leaderboard_json, leaderboard_key);
        else
            TraceLog(LOG_WARNING, "SYNC: social refresh failed result=%d name=%s",
                     result, GetLyraSyncResultName(result));
        if(result == LYRA_SYNC_OK)
            app_social_pop_action(action, sizeof(action), action_value,
                                  sizeof(action_value));
    }
#endif
#if defined(PLATFORM_WEB)
    g_sync.social_refresh_pending = g_sync.social_action_count > 0;
#endif
}

#if !defined(PLATFORM_WEB)
static void
app_collect_finished_social_refresh(InbeApp *app)
{
    int finished;
    int result;
    char requests_json[8192];
    char friends_json[8192];
    char leaderboard_json[8192];
    char leaderboard_key[96];

    sync_lock();
    finished = g_social_result.finished;
    result = g_social_result.result;
    if(finished) {
        snprintf(requests_json, sizeof(requests_json), "%s",
                 g_social_result.friend_requests_json);
        snprintf(friends_json, sizeof(friends_json), "%s",
                 g_social_result.friends_json);
        snprintf(leaderboard_json, sizeof(leaderboard_json), "%s",
                 g_social_result.leaderboard_json);
        snprintf(leaderboard_key, sizeof(leaderboard_key), "%s",
                 g_social_result.leaderboard_key);
        g_social_result.finished = 0;
    }
    sync_unlock();

    if(!finished)
        return;
    if(result != LYRA_SYNC_OK) {
        TraceLog(LOG_WARNING, "SYNC: social refresh failed result=%d name=%s",
                 result, GetLyraSyncResultName(result));
        return;
    }
    app_social_apply_result(app, requests_json, friends_json, leaderboard_json,
                            leaderboard_key);
    sync_lock();
    if(g_sync.social_action_count > 0)
        g_sync.social_refresh_pending = 1;
    sync_unlock();
    TraceLog(LOG_INFO, "SYNC: social cache refreshed");
}
#else
static void
app_collect_finished_social_refresh(InbeApp *app)
{
    (void)app;
}
#endif

void
app_request_social_refresh(InbeApp *app)
{
    (void)app;
    app_queue_social_refresh();
}

static void
app_request_social_action(const char *action, const char *value)
{
    if(action == NULL || action[0] == '\0' || value == NULL || value[0] == '\0')
        return;
#if !defined(PLATFORM_WEB)
    sync_lock();
    app_social_push_action(action, value);
    sync_unlock();
#else
    app_social_push_action(action, value);
#endif
}

void
app_request_friend_send(InbeApp *app, const char *target)
{
    (void)app;
    app_request_social_action("send", target);
}

void
app_request_friend_accept(InbeApp *app, const char *request_id)
{
    (void)app;
    app_request_social_action("accept", request_id);
}

void
app_request_friend_decline(InbeApp *app, const char *request_id)
{
    (void)app;
    app_request_social_action("decline", request_id);
}

void
app_request_friend_remove(InbeApp *app, const char *friend_user_id)
{
    (void)app;
    app_request_social_action("remove", friend_user_id);
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
        if(g_sync.sync_running) {
            sync_unlock();
            return 0;
        }
        g_sync.sync_running = 1;
        sync_unlock();

        args = malloc(sizeof(*args));
        if(args == NULL) {
            sync_lock();
            g_sync.sync_running = 0;
            sync_unlock();
            return 0;
        }
        snprintf(args->url, sizeof(args->url), "%s", url);
        TraceLog(LOG_INFO, "SYNC: starting background sync");
        if(!sync_thread_start(&thread, app_sync_worker, args)) {
            free(args);
            sync_lock();
            g_sync.sync_running = 0;
            sync_unlock();
            TraceLog(LOG_WARNING, "SYNC: failed to start background sync thread");
            return 0;
        }
        sync_thread_detach(thread);
        return 1;
    }
#else
    if(g_sync.sync_running)
        return 0;
    g_sync.sync_running = 1;
    TraceLog(LOG_INFO, "SYNC: starting background sync");
    if(!sync_client_web_sync_start(url)) {
        g_sync.sync_running = 0;
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
    g_sync.last_input_at = now;
    due = now + INBE_SYNC_INPUT_QUIET_SECONDS;
    if(g_sync.dirty_sync_at <= 0.0 || due < g_sync.dirty_sync_at)
        g_sync.dirty_sync_at = due;
    TraceLog(LOG_INFO, "SYNC: queued local changes");
    return 1;
}

void
app_sync_pump(InbeApp *app)
{
    double now;
    int should_sync = 0;
    int dirty_due = 0;
    int repair_due = 0;
    char url[256];
    InbeStorageSyncStatus sync_status;

    if(app == NULL)
        return;
    app_collect_finished_sync();
    app_collect_finished_social_refresh(app);
    if(storage_sync_review_pending() && app_background_sync_safe(app)) {
        if(storage_sync_review_clear_if_no_visible_diff()) {
            app_reload_after_import(app, 0);
        } else {
            app_open_modal(app, UIModalSyncReview);
        }
    }
    app_apply_pending_sync_refresh(app);
    app_apply_pending_social_refresh(app);
    if(!app_sync_url(url, sizeof(url)))
        return;
    app_ensure_sync_events(url);
    now = GetTime();
    if(app_input_active())
        g_sync.last_input_at = now;
    if(g_sync.dirty_sync_at > 0.0 && now >= g_sync.dirty_sync_at) {
        should_sync = 1;
        dirty_due = 1;
    }
    if(storage_sync_status(&sync_status) && sync_status.has_account && sync_status.repair_pending &&
       (g_sync.dirty_sync_at <= 0.0 || now >= g_sync.dirty_sync_at)) {
        should_sync = 1;
        repair_due = 1;
    }
#if !defined(PLATFORM_WEB)
    sync_lock();
#endif
    if(g_sync.remote_sync_due) {
        should_sync = 1;
        g_sync.remote_sync_due = 0;
    }
#if !defined(PLATFORM_WEB)
    sync_unlock();
#endif
    if(!should_sync)
        return;
    if(now < g_sync.last_input_at + INBE_SYNC_INPUT_QUIET_SECONDS) {
        if(g_sync.dirty_sync_at <= 0.0)
            g_sync.dirty_sync_at = g_sync.last_input_at + INBE_SYNC_INPUT_QUIET_SECONDS;
        return;
    }
    if(!app_background_sync_safe(app)) {
        TraceLog(LOG_INFO,
                 "SYNC: delayed; modal=%d type=%s screen=%d pending_review=%d tutorial_seen=%d habits_guide_seen=%d",
                 app->modal.active,
                 app_modal_type_name(app->modal.type),
                 app->inbe.screen,
                 storage_sync_review_pending(),
                 app->tutorial_seen,
                 app->habits_guide_seen);
        if(g_sync.dirty_sync_at <= 0.0 || repair_due)
            g_sync.dirty_sync_at = now + 1.0;
        return;
    }
    if(app_sync_now(app)) {
        if(dirty_due || repair_due)
            g_sync.dirty_sync_at = 0.0;
        TraceLog(LOG_INFO, "SYNC: dispatched");
    } else if(dirty_due || repair_due) {
        g_sync.dirty_sync_at = now + INBE_SYNC_INPUT_QUIET_SECONDS;
    }
    app_apply_pending_sync_refresh(app);
}
