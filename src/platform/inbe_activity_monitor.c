#include "inbe_activity_monitor.h"

#if defined(__linux__) || defined(__FreeBSD__)

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Minimal X declarations for the dlopen'd calls. Types mirror the X headers
 * (XID/Window are unsigned long everywhere we ship; Display is opaque).
 */
typedef struct _XDisplay Display;
typedef unsigned long XID;
typedef XID Window;
typedef XID Drawable;

enum { GrabModeSync = 0, GrabModeAsync = 1 };

typedef struct {
    Window window;              /* screen saver window */
    int state;                  /* ScreenSaverOff/On/Disabled */
    int kind;                   /* ScreenSaverBlanked/Internal/External */
    unsigned long til_or_since;
    unsigned long idle;         /* milliseconds since last input */
    unsigned long eventMask;
} InbeXScreenSaverInfo;

typedef Display *(*InbeXOpenDisplay)(const char *display_name);
typedef int (*InbeXCloseDisplay)(Display *display);
typedef Window (*InbeXDefaultRootWindow)(Display *display);
typedef int (*InbeXGrabKeyboard)(Display *display, Window grab_window,
                                 int owner_events, int pointer_mode,
                                 int keyboard_mode, unsigned long time);
typedef int (*InbeXUngrabKeyboard)(Display *display, unsigned long time);
typedef int (*InbeXSync)(Display *display, int discard);
typedef InbeXScreenSaverInfo *(*InbeXScreenSaverAllocInfo)(void);
typedef int (*InbeXScreenSaverQueryInfo)(Display *display, Drawable drawable,
                                         InbeXScreenSaverInfo *info);

static Display *g_display;
static void *g_x11;
static void *g_xss;
static InbeXScreenSaverAllocInfo g_ss_alloc;
static InbeXScreenSaverQueryInfo g_ss_query;
static InbeXDefaultRootWindow g_root_window;
static InbeXGrabKeyboard g_grab_keyboard;
static InbeXUngrabKeyboard g_ungrab_keyboard;
static InbeXSync g_sync;
static int g_inited;
static int g_available;
static int g_blocked;

static void *
resolve(void *handle, const char *name)
{
    void *sym = handle != NULL ? dlsym(handle, name) : NULL;

    if(sym == NULL)
        return NULL;
    return sym;
}

static void *
load_library(const char *const *names)
{
    int i;

    for(i = 0; names[i] != NULL; i++) {
        void *handle = dlopen(names[i], RTLD_LAZY | RTLD_LOCAL);

        if(handle != NULL)
            return handle;
    }
    return NULL;
}

static int
session_is_wayland(void)
{
    const char *type = getenv("XDG_SESSION_TYPE");

    /*
     * XWayland's idle counter only sees input delivered to X clients, so a
     * Wayland-native session would read native-app use as idle. Only trust
     * the X counter on real X sessions (or when the session type is unset,
     * e.g. started from a tty).
     */
    return type != NULL && strcmp(type, "wayland") == 0;
}

void
inbe_activity_monitor_init(void)
{
    static const char *const x11_names[] = { "libX11.so.6", "libX11.so", NULL };
    static const char *const xss_names[] = { "libXss.so.1", "libXss.so", NULL };
    InbeXOpenDisplay open_display;

    if(g_inited)
        return;
    g_inited = 1;

    if(session_is_wayland())
        return;

    g_x11 = load_library(x11_names);
    g_xss = load_library(xss_names);
    if(g_x11 == NULL || g_xss == NULL)
        return;

    open_display = (InbeXOpenDisplay)resolve(g_x11, "XOpenDisplay");
    g_root_window = (InbeXDefaultRootWindow)resolve(g_x11, "XDefaultRootWindow");
    g_grab_keyboard = (InbeXGrabKeyboard)resolve(g_x11, "XGrabKeyboard");
    g_ungrab_keyboard = (InbeXUngrabKeyboard)resolve(g_x11, "XUngrabKeyboard");
    g_sync = (InbeXSync)resolve(g_x11, "XSync");
    g_ss_alloc = (InbeXScreenSaverAllocInfo)resolve(g_xss, "XScreenSaverAllocInfo");
    g_ss_query = (InbeXScreenSaverQueryInfo)resolve(g_xss, "XScreenSaverQueryInfo");

    if(open_display == NULL || g_ss_alloc == NULL || g_ss_query == NULL ||
       g_root_window == NULL) {
        g_grab_keyboard = NULL;
        g_ungrab_keyboard = NULL;
        g_sync = NULL;
        return;
    }

    g_display = open_display(NULL);
    if(g_display == NULL)
        return;

    g_available = 1;
}

int
inbe_activity_available(void)
{
    if(!g_inited)
        inbe_activity_monitor_init();
    return g_available;
}

long
inbe_activity_get_idle_ms(void)
{
    InbeXScreenSaverInfo *info;
    long idle;

    if(!inbe_activity_available())
        return -1;

    info = g_ss_alloc();
    if(info == NULL)
        return -1;

    idle = -1;
    if(g_ss_query(g_display, g_root_window(g_display), info) != 0)
        idle = (long)info->idle;
    free(info);
    return idle;
}

int
inbe_break_set_input_blocked(int on)
{
    int ok = 0;

    if(!g_inited)
        inbe_activity_monitor_init();

    if(g_display == NULL || g_grab_keyboard == NULL || g_ungrab_keyboard == NULL)
        return 0;

    if(on && !g_blocked) {
        /* GrabModeAsync on the root window: keys stop reaching other
         * clients; the pointer stays usable for the break window buttons. */
        ok = g_grab_keyboard(g_display, g_root_window(g_display), 0,
                             GrabModeAsync, GrabModeAsync, 0UL) == 0;
        g_blocked = ok;
    } else if(!on && g_blocked) {
        g_ungrab_keyboard(g_display, 0UL);
        g_blocked = 0;
        ok = 1;
    } else {
        ok = 1;
    }

    if(g_sync != NULL)
        g_sync(g_display, 0);
    return ok;
}

int
inbe_break_input_blocked(void)
{
    return g_blocked;
}

#else /* !(__linux__ || __FreeBSD__) */

/* Windows (GetLastInputInfo) and friends land with their platform passes. */

void
inbe_activity_monitor_init(void)
{
}

int
inbe_activity_available(void)
{
    return 0;
}

long
inbe_activity_get_idle_ms(void)
{
    return -1;
}

int
inbe_break_set_input_blocked(int on)
{
    (void)on;
    return 0;
}

int
inbe_break_input_blocked(void)
{
    return 0;
}

#endif
