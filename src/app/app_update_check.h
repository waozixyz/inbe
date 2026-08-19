#ifndef INBE_APP_UPDATE_CHECK_H
#define INBE_APP_UPDATE_CHECK_H

/*
 * Desktop update flow against the release appcast (kryon kry_update).
 * Start once after settings load, poll from the frame loop; the About
 * screen renders from the flow state. Android and web builds compile to
 * no-ops: stores manage updates there.
 */
void inbe_update_check_start(void);
void inbe_update_check_poll(void);

int inbe_update_available(void);
const char *inbe_update_row_text(void);      /* "Update available: vX.Y.Z" */
const char *inbe_update_download_url(void);  /* channel artifact or release page */

/* Self-update flow, AppImage and Windows portable only; packaged and
 * source channels stay on the link-only row. */
typedef enum {
    INBE_UPDATE_FLOW_IDLE = 0,
    INBE_UPDATE_FLOW_AVAILABLE,   /* row + download button */
    INBE_UPDATE_FLOW_DOWNLOADING, /* row shows progress */
    INBE_UPDATE_FLOW_READY,       /* verified; restart to apply */
    INBE_UPDATE_FLOW_FAILED,      /* download/verify failed; retry offered */
} InbeUpdateFlow;

InbeUpdateFlow inbe_update_flow(void);
int inbe_update_can_self_update(void);
double inbe_update_download_fraction(void);  /* 0..1; -1 while unknown */
const char *inbe_update_flow_error(void);

/* Row helpers so the About screen stays logic-free. */
int inbe_update_is_downloading(void);
const char *inbe_update_action_label(void);  /* button text for the row */
void inbe_update_row_action(void);           /* download / retry / restart */
const char *inbe_update_error_text(void);    /* flow error, "" when none */

/* READY -> stage the swap (Windows) / arm the exit re-exec (AppImage) and
 * ask the app to quit. */
void inbe_update_apply(void);
/* main.c calls this after cleanup; performs the AppImage re-exec. Returns
 * 1 when an update was pending (either way the app should exit now). */
int inbe_update_apply_at_exit(void);

#endif
