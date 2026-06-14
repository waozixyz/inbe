#ifndef FLINT_WEB_H
#define FLINT_WEB_H

#ifdef __cplusplus
extern "C" {
#endif

int flint_web_viewport_width(int fallback_width);
int flint_web_viewport_height(int fallback_height);
void flint_web_viewport_size(int fallback_width, int fallback_height,
                             int *width, int *height);

/*
 * Web builds should create a resizable raylib window in CSS-pixel units and
 * keep the window size synced to the browser viewport. Native builds return 0.
 */
unsigned int flint_web_window_flags(void);

/*
 * Resize raylib's window to the current browser viewport when it changes.
 * Returns 1 when a resize was applied, 0 otherwise.
 */
int flint_web_sync_window_size(void);

#ifdef __cplusplus
}
#endif

#endif
