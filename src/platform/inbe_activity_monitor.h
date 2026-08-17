#ifndef INBE_ACTIVITY_MONITOR_H
#define INBE_ACTIVITY_MONITOR_H

/*
 * Workrave-style activity monitoring for the break engine.
 *
 * Reports system-wide input idle time (keyboard + mouse, the whole session,
 * not just this window) so the engine can tell active time from idle time,
 * and owns the break input grab for "block input" mode.
 *
 * Linux/FreeBSD: polls the X server's screensaver idle counter through
 * dlopen'd libX11/libXss, so there is no build-time dependency and builds
 * without X headers still work. Wayland-native sessions do not expose the
 * idle counter through XWayland (native-app input would look like idle), so
 * the monitor reports unavailable there and the app falls back to a
 * window-focus heuristic. Other platforms are stubs for now.
 */

/* Idempotent; also called lazily by the getters. Cheap when unsupported. */
void inbe_activity_monitor_init(void);

/* 1 when this process runs in a native Wayland desktop session.  This is
 * deliberately separate from availability: Wayland may gain a system idle
 * provider in the future, while X11-only auxiliary windows and input grabs
 * must remain disabled under Wayland regardless. */
int inbe_activity_is_wayland(void);

/* 1 when system-wide idle detection works on this platform/session. */
int inbe_activity_available(void);

/* Milliseconds since the last user input anywhere, or -1 when unknown. */
long inbe_activity_get_idle_ms(void);

/*
 * Block mode: grab the keyboard so the user cannot keep typing through a
 * break (the pointer stays free so the break window's buttons work; the
 * overlay covers the screen in "block input and screen" mode). Returns 1 on
 * success, 0 when unsupported (Wayland, headless). Ungrab with on == 0.
 */
int inbe_break_set_input_blocked(int on);

/* 1 while the input grab is held. */
int inbe_break_input_blocked(void);

#endif /* INBE_ACTIVITY_MONITOR_H */
