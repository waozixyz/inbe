#include "app_update_check.h"

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

static KryUpdateCheck *update_check;
static int update_available;
static char update_version[32];
static char update_row_text[96];
static char update_download_url[512];

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

    storage_set_setting_int(INBE_UPDATE_LAST_CHECK_KEY, (int)time(NULL));
    if(status == KRY_UPDATE_AVAILABLE && info != NULL) {
        const KryUpdateChannelInfo *entry =
            channel_key != NULL ? kry_update_find_channel(info, channel_key) : NULL;

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

void
inbe_update_check_poll(void)
{
    KryUpdateStatus status;

    if(update_check == NULL)
        return;
    status = kry_update_poll(update_check);
    if(status == KRY_UPDATE_PENDING)
        return;
    update_check_finish(status);
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

#else /* web/Android: stores manage updates */

void inbe_update_check_start(void) {}
void inbe_update_check_poll(void) {}
int inbe_update_available(void) { return 0; }
const char *inbe_update_row_text(void) { return ""; }
const char *inbe_update_download_url(void) { return ""; }

#endif
