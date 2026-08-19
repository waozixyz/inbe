#include "app_update_check.h"
#include "app.h"
#include "app_update_zip.h"

#include "storage.h"
#include "version.h"
#include "kryon.h"
#include "kry_update_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Desktop update flow. The lifecycle (check, channel choice, download,
 * sha256 verify, apply mechanics) lives in kryon's kry_update_flow; this
 * file adds inbe's persistence (one check per day in settings), locale,
 * the toast, and the Windows zip extractor. Screenshot mode
 * (INBE_DATA_ROOT) skips the check so CI stays offline-deterministic.
 */
#define INBE_APPCAST_URL \
    "https://github.com/waozixyz/inbe/releases/latest/download/appcast.json"
#define INBE_UPDATE_CHECK_INTERVAL_S (24 * 60 * 60)
#define INBE_UPDATE_LAST_CHECK_KEY "update_check_last_unix"

#if !defined(PLATFORM_WEB) && !ANDROID_BUILD

static KryUpdateFlow *flow;
static int update_available;
static int shown_available_toast;
static int logged_ready;
static int check_resolved;
static int apply_armed;

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

static int
update_zip_extract_cb(const char *archive, const char *dest_dir, void *user)
{
    (void)user;
    return inbe_update_zip_extract(archive, dest_dir);
}

static void
update_flow_note_state(void)
{
    KryUpdateFlowState state = kry_update_flow_state(flow);

    if(!check_resolved && state != KRY_UPDATE_FLOW_CHECKING &&
       state != KRY_UPDATE_FLOW_IDLE) {
        check_resolved = 1;   /* any terminal check state arms the throttle */
        storage_set_setting_int(INBE_UPDATE_LAST_CHECK_KEY, (int)time(NULL));
    }
    switch(state) {
    case KRY_UPDATE_FLOW_AVAILABLE:
        if(!update_available) {
            update_available = 1;
            TraceLog(LOG_INFO, "INBE: update %s available (channel %s)",
                     kry_update_flow_new_version(flow),
                     kry_update_channel_name(kry_update_flow_channel(flow)));
            /* Toast only where a download is actionable; system channels
             * hear about updates from their package manager. */
            if(kry_update_flow_artifact(flow) != NULL &&
               !shown_available_toast) {
                ShowUIToast(GetLocaleText("update_available_toast"));
                shown_available_toast = 1;
            }
            /* Test/automation hook: fetch immediately instead of waiting
             * for a click on the About row. */
            if(getenv("INBE_UPDATE_AUTO_DOWNLOAD") != NULL)
                kry_update_flow_download(flow);
        }
        break;
    case KRY_UPDATE_FLOW_READY:
        if(!logged_ready) {
            logged_ready = 1;
            TraceLog(LOG_INFO, "INBE: update staged and verified");
        }
        break;
    case KRY_UPDATE_FLOW_FAILED:
        TraceLog(LOG_WARNING, "INBE: update flow: %s",
                 kry_update_flow_error(flow) != NULL
                   ? kry_update_flow_error(flow) : "?");
        break;
    default:
        break;
    }
}

void
inbe_update_check_start(void)
{
    KryUpdateFlowConfig cfg = {
        .app_name = "inbe",
        .current_version = INBE_VERSION_STRING,
    };
    const char *override = getenv("INBE_UPDATE_APPCAST_OVERRIDE");

    if(flow != NULL || update_available || update_check_throttled())
        return;
    if(getenv("INBE_DATA_ROOT") != NULL && (override == NULL || override[0] == '\0'))
        return;    /* screenshot/test runs stay offline unless overridden */
    flow = kry_update_flow_start(&cfg, update_appcast_url());
    if(flow == NULL)
        TraceLog(LOG_INFO, "INBE: update check unavailable (no HTTP client)");
    else
        kry_update_flow_set_extractor(flow, update_zip_extract_cb, NULL);
}

void
inbe_update_check_poll(void)
{
    if(flow == NULL)
        return;
    kry_update_flow_poll(flow);
    update_flow_note_state();
}

int
inbe_update_available(void)
{
    return update_available;
}

const char *
inbe_update_row_text(void)
{
    static char text[96];

    if(!update_available)
        return "";
    snprintf(text, sizeof(text), "%s: v%s",
             GetLocaleText("update_available_row"),
             kry_update_flow_new_version(flow));
    return text;
}

const char *
inbe_update_download_url(void)
{
    if(flow == NULL)
        return "";
    {
        const char *release = kry_update_flow_release_url(flow);

        if(release[0] != '\0')
            return release;
    }
    return "https://github.com/waozixyz/inbe/releases";
}

InbeUpdateFlow
inbe_update_flow(void)
{
    /* map kryon states onto the historical app enum */
    switch(kry_update_flow_state(flow)) {
    case KRY_UPDATE_FLOW_AVAILABLE: return INBE_UPDATE_FLOW_AVAILABLE;
    case KRY_UPDATE_FLOW_DOWNLOADING: return INBE_UPDATE_FLOW_DOWNLOADING;
    case KRY_UPDATE_FLOW_READY: return INBE_UPDATE_FLOW_READY;
    case KRY_UPDATE_FLOW_FAILED: return INBE_UPDATE_FLOW_FAILED;
    default: return INBE_UPDATE_FLOW_IDLE;
    }
}

int
inbe_update_can_self_update(void)
{
    return flow != NULL && kry_update_flow_artifact(flow) != NULL;
}

double
inbe_update_download_fraction(void)
{
    return kry_update_flow_progress(flow);
}

const char *
inbe_update_flow_error(void)
{
    return kry_update_flow_error(flow);
}

int
inbe_update_is_downloading(void)
{
    return kry_update_flow_state(flow) == KRY_UPDATE_FLOW_DOWNLOADING;
}

const char *
inbe_update_action_label(void)
{
    static char label[96];

    if(kry_update_flow_state(flow) == KRY_UPDATE_FLOW_READY)
        return GetLocaleText("update_restart");
    if(kry_update_flow_state(flow) == KRY_UPDATE_FLOW_FAILED)
        return GetLocaleText("update_retry");
    snprintf(label, sizeof(label), "%s v%s",
             GetLocaleText("update_download"), kry_update_flow_new_version(flow));
    return label;
}

void
inbe_update_row_action(void)
{
    if(kry_update_flow_state(flow) == KRY_UPDATE_FLOW_READY)
        inbe_update_apply();
    else
        kry_update_flow_download(flow);
}

const char *
inbe_update_error_text(void)
{
    const char *error;

    if(kry_update_flow_state(flow) != KRY_UPDATE_FLOW_FAILED)
        return "";
    error = kry_update_flow_error(flow);
    return error != NULL && error[0] != '\0' ? error
                                             : GetLocaleText("update_failed");
}

void
inbe_update_apply(void)
{
    InbeApp *app = get_global_inbe_app();

    if(kry_update_flow_apply(flow)) {
        apply_armed = 1;
        if(app != NULL)
            app->request_quit = 1;   /* already user-confirmed; skip the close prompt */
    }
}

int
inbe_update_apply_at_exit(void)
{
    if(flow == NULL || !apply_armed)
        return 0;
    TraceLog(LOG_INFO, "INBE: applying update and restarting");
    if(!kry_update_flow_exec_pending(flow))
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
void inbe_update_apply(void) {}
int inbe_update_apply_at_exit(void) { return 0; }

#endif
