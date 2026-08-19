#include "app_update_check.h"
#include "app_update_zip.h"
#include "app.h"

#include "storage.h"
#include "version.h"
#include "kryon.h"
#include "kry_update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * One check per day, desktop only. The appcast is published by the release
 * workflow as a release asset, so the latest one is always at a version-less
 * URL (GitHub redirects "latest/download" to the newest release). Screenshot
 * mode (INBE_DATA_ROOT) skips the check so CI stays offline-deterministic.
 */
#define INBE_APPCAST_URL \
    "https://github.com/waozixyz/inbe/releases/latest/download/appcast.json"
#define INBE_UPDATE_CHECK_INTERVAL_S (24 * 60 * 60)
#define INBE_UPDATE_LAST_CHECK_KEY "update_check_last_unix"

#if !defined(PLATFORM_WEB) && !ANDROID_BUILD

#ifdef _WIN32
#include <windows.h>
#endif

static KryUpdateCheck *update_check;
static int update_available;
static char update_version[32];
static char update_row_text[96];
static char update_download_url[512];

static InbeUpdateFlow flow_state;
static KryUpdateDownload *update_download;
static KryUpdateChannelInfo update_entry;
static char verified_path[600];
static char flow_error[160];
static int apply_after_quit;

static int
update_check_throttled(void)
{
    long last = storage_get_setting_int(INBE_UPDATE_LAST_CHECK_KEY, 0);
    long now = (long)time(NULL);

    return last > 0 && now >= last &&
           now - last < INBE_UPDATE_CHECK_INTERVAL_S;
}

static const char *
update_appcast_url(void)
{
    const char *override = getenv("INBE_UPDATE_APPCAST_OVERRIDE");

    return override != NULL && override[0] != '\0' ? override
                                                   : INBE_APPCAST_URL;
}

static void
update_check_finish(KryUpdateStatus status)
{
    const KryUpdateInfo *info = kry_update_info(update_check);
    KryUpdateChannel channel = kry_update_detect_channel();
    const char *channel_key = kry_update_channel_key(channel);
    const KryUpdateChannelInfo *entry =
        channel_key != NULL ? kry_update_find_channel(info, channel_key) : NULL;

    storage_set_setting_int(INBE_UPDATE_LAST_CHECK_KEY, (int)time(NULL));
    if(status == KRY_UPDATE_AVAILABLE && info != NULL) {
        snprintf(update_version, sizeof(update_version), "%s", info->version);
        snprintf(update_download_url, sizeof(update_download_url), "%s",
                 entry != NULL && entry->url[0] != '\0'
                   ? entry->url
                   : (info->notes_url[0] != '\0' ? info->notes_url
                                                 : "https://github.com/waozixyz/inbe/releases"));
        snprintf(update_row_text, sizeof(update_row_text), "%s: v%s",
                 GetLocaleText("update_available_row"), info->version);
        update_available = 1;
        TraceLog(LOG_INFO, "INBE: update %s available (channel %s)",
                 info->version, kry_update_channel_name(channel));

        if(entry != NULL && entry->url[0] != '\0') {
            update_entry = *entry;
            flow_state = INBE_UPDATE_FLOW_AVAILABLE;
            /* Test/automation hook: fetch immediately instead of waiting
             * for a click on the About row. */
            if(getenv("INBE_UPDATE_AUTO_DOWNLOAD") != NULL)
                inbe_update_download_begin();
        }

        /* Toast only where a download link is actually actionable; system
         * channels hear about updates from their package manager. */
        if(channel_key != NULL)
            ShowUIToast(GetLocaleText("update_available_toast"));
    } else if(status == KRY_UPDATE_FAILED) {
        const char *error = kry_update_error(update_check);

        TraceLog(LOG_INFO, "INBE: update check failed: %s",
                 error != NULL ? error : "?");
    }
    kry_update_free(update_check);
    update_check = NULL;
}

void
inbe_update_check_start(void)
{
    const char *override = getenv("INBE_UPDATE_APPCAST_OVERRIDE");

    if(update_check != NULL || update_available || update_check_throttled())
        return;
    if(getenv("INBE_DATA_ROOT") != NULL && (override == NULL || override[0] == '\0'))
        return;    /* screenshot/test runs stay offline unless overridden */
    update_check = kry_update_check(update_appcast_url(), INBE_VERSION_STRING);
    if(update_check == NULL)
        TraceLog(LOG_INFO, "INBE: update check unavailable (no HTTP client)");
}

static void
update_download_poll(void)
{
    KryUpdateDownloadStatus s;
    const char *path;

    if(update_download == NULL)
        return;
    s = kry_update_download_poll(update_download);
    if(s == KRY_UPDATE_DL_PENDING || s == KRY_UPDATE_DL_RUNNING)
        return;
    if(s == KRY_UPDATE_DL_DONE) {
        path = kry_update_download_path(update_download);
        snprintf(verified_path, sizeof(verified_path), "%s",
                 path != NULL ? path : "");
        flow_state = verified_path[0] != '\0' ? INBE_UPDATE_FLOW_READY
                                              : INBE_UPDATE_FLOW_FAILED;
        if(flow_state == INBE_UPDATE_FLOW_FAILED)
            snprintf(flow_error, sizeof(flow_error), "%s",
                     "verified download vanished");
        else
            TraceLog(LOG_INFO, "INBE: update staged: %s", verified_path);
    } else {
        const char *error = kry_update_download_error(update_download);

        snprintf(flow_error, sizeof(flow_error), "%s",
                 error != NULL ? error : "download failed");
        TraceLog(LOG_WARNING, "INBE: update download failed: %s", flow_error);
        flow_state = INBE_UPDATE_FLOW_FAILED;
    }
    kry_update_download_free(update_download);
    update_download = NULL;
}

void
inbe_update_check_poll(void)
{
    if(update_check != NULL) {
        KryUpdateStatus status = kry_update_poll(update_check);

        if(status != KRY_UPDATE_PENDING)
            update_check_finish(status);
    }
    update_download_poll();
}

int
inbe_update_available(void)
{
    return update_available;
}

const char *
inbe_update_row_text(void)
{
    return update_row_text;
}

const char *
inbe_update_download_url(void)
{
    return update_download_url;
}

InbeUpdateFlow
inbe_update_flow(void)
{
    return flow_state;
}

int
inbe_update_can_self_update(void)
{
    return flow_state != INBE_UPDATE_FLOW_IDLE;
}

double
inbe_update_download_fraction(void)
{
    return kry_update_download_progress(update_download);
}

const char *
inbe_update_flow_error(void)
{
    return flow_state == INBE_UPDATE_FLOW_FAILED ? flow_error : NULL;
}

int
inbe_update_is_downloading(void)
{
    return flow_state == INBE_UPDATE_FLOW_DOWNLOADING;
}

const char *
inbe_update_action_label(void)
{
    static char label[96];

    if(flow_state == INBE_UPDATE_FLOW_READY)
        return GetLocaleText("update_restart");
    if(flow_state == INBE_UPDATE_FLOW_FAILED)
        return GetLocaleText("update_retry");
    snprintf(label, sizeof(label), "%s v%s",
             GetLocaleText("update_download"), update_version);
    return label;
}

void
inbe_update_row_action(void)
{
    if(flow_state == INBE_UPDATE_FLOW_READY) {
        inbe_update_apply();
    } else if(flow_state == INBE_UPDATE_FLOW_AVAILABLE ||
              flow_state == INBE_UPDATE_FLOW_FAILED) {
        inbe_update_download_begin();
    }
}

const char *
inbe_update_error_text(void)
{
    if(flow_state != INBE_UPDATE_FLOW_FAILED)
        return "";
    return flow_error[0] != '\0' ? flow_error : GetLocaleText("update_failed");
}

void
inbe_update_download_begin(void)
{
    char dir[512];

    if(flow_state != INBE_UPDATE_FLOW_AVAILABLE &&
       flow_state != INBE_UPDATE_FLOW_FAILED)
        return;
    if(update_download != NULL)
        return;
    if(!kry_update_download_dir("inbe", dir, sizeof(dir))) {
        snprintf(flow_error, sizeof(flow_error), "%s", "cannot create update dir");
        flow_state = INBE_UPDATE_FLOW_FAILED;
        return;
    }
    update_download = kry_update_download_begin(&update_entry, dir);
    if(update_download == NULL) {
        snprintf(flow_error, sizeof(flow_error), "%s", "download unavailable");
        flow_state = INBE_UPDATE_FLOW_FAILED;
        return;
    }
    flow_error[0] = '\0';
    flow_state = INBE_UPDATE_FLOW_DOWNLOADING;
    TraceLog(LOG_INFO, "INBE: update download started (%s)", update_entry.url);
}

/* Windows portable: stage the extracted zip beside the install dir and
 * hand the swap to the detached script; returns 1 when the app should
 * quit now. */
#ifdef _WIN32
static int
update_windows_stage(void)
{
    char exe_path[MAX_PATH];
    char new_dir[MAX_PATH + 32];
    const char *slash;
    DWORD len;

    len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if(len == 0 || len >= sizeof(exe_path))
        return 0;
    slash = strrchr(exe_path, '\\');
    if(slash == NULL || slash == exe_path)
        return 0;
    snprintf(new_dir, sizeof(new_dir), "%.*s-update-new", (int)(slash - exe_path),
             exe_path);
    if(!inbe_update_zip_extract(verified_path, new_dir)) {
        TraceLog(LOG_WARNING, "INBE: update zip extraction failed");
        return 0;
    }
    if(kry_update_windows_stage_swap(new_dir) != KRY_UPDATE_APPLY_RESTARTING) {
        TraceLog(LOG_WARNING, "INBE: update swap staging failed");
        return 0;
    }
    return 1;
}
#endif

void
inbe_update_apply(void)
{
    InbeApp *app = get_global_inbe_app();

    if(flow_state != INBE_UPDATE_FLOW_READY)
        return;
#ifdef _WIN32
    if(update_windows_stage()) {
        TraceLog(LOG_INFO, "INBE: update staged; restarting");
        if(app != NULL)
            app->request_quit = 1;   /* already user-confirmed; skip the close prompt */
    } else {
        snprintf(flow_error, sizeof(flow_error), "%s", "could not stage update");
        flow_state = INBE_UPDATE_FLOW_FAILED;
    }
#else
    {
        KryUpdateChannel channel = kry_update_detect_channel();

        if(channel == KRY_UPDATE_CHANNEL_APPIMAGE) {
            apply_after_quit = 1;
            if(app != NULL)
                app->request_quit = 1;   /* already user-confirmed; skip the close prompt */
        } else if(update_download_url[0] != '\0') {
            OpenURL(update_download_url);
        }
    }
#endif
}

int
inbe_update_apply_at_exit(void)
{
    if(!apply_after_quit || verified_path[0] == '\0')
        return 0;
    TraceLog(LOG_INFO, "INBE: applying update and restarting");
    kry_update_appimage_apply(verified_path);
    TraceLog(LOG_WARNING, "INBE: update re-exec failed; continuing exit");
    return 1;
}

#else /* web/Android: stores manage updates */

void inbe_update_check_start(void) {}
void inbe_update_check_poll(void) {}
int inbe_update_available(void) { return 0; }
const char *inbe_update_row_text(void) { return ""; }
const char *inbe_update_download_url(void) { return ""; }
InbeUpdateFlow inbe_update_flow(void) { return INBE_UPDATE_FLOW_IDLE; }
int inbe_update_can_self_update(void) { return 0; }
double inbe_update_download_fraction(void) { return -1.0; }
const char *inbe_update_flow_error(void) { return NULL; }
int inbe_update_is_downloading(void) { return 0; }
const char *inbe_update_action_label(void) { return ""; }
void inbe_update_row_action(void) {}
const char *inbe_update_error_text(void) { return ""; }
void inbe_update_download_begin(void) {}
void inbe_update_apply(void) {}
int inbe_update_apply_at_exit(void) { return 0; }

#endif
