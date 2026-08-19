#ifndef INBE_APP_UPDATE_CHECK_H
#define INBE_APP_UPDATE_CHECK_H

/*
 * Desktop update check against the release appcast (kryon kry_update).
 * Start it once after settings load, poll it from the frame loop; the
 * About screen shows the row when inbe_update_available() turns true.
 * Android and web builds compile to no-ops: stores manage updates there.
 */
void inbe_update_check_start(void);
void inbe_update_check_poll(void);

int inbe_update_available(void);
const char *inbe_update_row_text(void);      /* "Update available: vX.Y.Z" */
const char *inbe_update_download_url(void);  /* channel artifact or release page */

#endif
