#include "kryon.h"
#if !defined(PLATFORM_WEB) && !defined(_WIN32) && !ANDROID_BUILD && defined(NATIVE_WINDOW_HAVE_SDL)
#include <SDL2/SDL.h>
#endif
#include "app.h"
#include "platform/app_host.h"
#include "desktop.h"
#include "storage.h"
#include "sync_account.h"
#include "sync_client.h"
#include "practices/practice_registry.h"
#include "app/device_preferences.h"
#include "app/app_update_check.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

#if !defined(PLATFORM_WEB) && !defined(_WIN32) && !ANDROID_BUILD && defined(NATIVE_WINDOW_HAVE_SDL)
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(DESKTOP_TRAY_ENABLED)
#include "platform/app_desktop_tray.h"
#endif

#if defined(_WIN32) && !ANDROID_BUILD
#include <process.h>
__declspec(dllimport) LRESULT __stdcall SendMessageW(HWND, UINT, WPARAM, LPARAM);
__declspec(dllimport) int __stdcall MessageBoxA(HWND, const char *, const char *, UINT);
__declspec(dllimport) HANDLE __stdcall LoadImageW(HINSTANCE, LPCWSTR, UINT, int, int, UINT);
__declspec(dllimport) int __stdcall GetSystemMetrics(int);
#define MB_OK 0u
#define MB_ICONERROR 0x10u
#define WIN_ERROR_LOG_CAP 2048
#define APP_ICON_RESOURCE 101
#define APP_WM_SETICON 0x0080u
#define ICON_SMALL 0u
#define ICON_BIG 1u
#define APP_IMAGE_ICON 1u
#define SM_CXICON 11
#define SM_CYICON 12
#define SM_CXSMICON 49
#define SM_CYSMICON 50
#endif

#if ANDROID_BUILD
#include "android_insets.h"
#include "android_device.h"
#include "android_runtime_assets.h"
#include "android_wakelock.h"
#include "android_timer.h"
#include <android/log.h>
#include <android_native_app_glue.h>
extern struct android_app *GetAndroidApp(void);
#endif

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#if !defined(PLATFORM_WEB) && !ANDROID_BUILD
static void
init_desktop_identity(void)
{
    DesktopAppInfo info = DesktopIdentity();

    InitDesktopApp(&info);
#if defined(_WIN32)
    _putenv_s("SDL_APP_NAME", info.display_name);
#else
    setenv("SDL_APP_NAME", info.display_name, 1);
    setenv("SDL_VIDEO_X11_WMCLASS", info.app_id, 1);
    setenv("SDL_VIDEO_WAYLAND_WMCLASS", info.app_id, 1);
    setenv("SDL_VIDEO_WAYLAND_APP_ID", info.app_id, 1);
#endif
}
#else
static void
init_desktop_identity(void)
{
}
#endif

static void
set_desktop_window_icon(const char *path)
{
    (void)path;
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
#if defined(_WIN32)
    {
        HWND window = (HWND)GetWindowHandle();
        HMODULE instance = GetModuleHandleW(NULL);
        LPCWSTR resource = (LPCWSTR)(uintptr_t)APP_ICON_RESOURCE;
        HICON large = (HICON)LoadImageW(instance, resource, APP_IMAGE_ICON,
                                        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
        HICON small = (HICON)LoadImageW(instance, resource, APP_IMAGE_ICON,
                                        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
        if(window) {
            if(large)
                SendMessageW(window, APP_WM_SETICON, ICON_BIG, (LPARAM)large);
            if(small)
                SendMessageW(window, APP_WM_SETICON, ICON_SMALL, (LPARAM)small);
        }
    }
#endif
    if(path == NULL || path[0] == '\0')
        return;
    const EmbeddedAsset *asset = GetEmbeddedAsset(path);
    Image icon;

    if(asset == NULL || asset->data == NULL || asset->size == 0) {
        TraceLog(LOG_WARNING, "APP: Missing window icon asset: %s", path);
        return;
    }

    icon = LoadImageFromMemory(GetEmbeddedAssetExtension(path), asset->data, (int)asset->size);
    if(icon.data == NULL) {
        TraceLog(LOG_WARNING, "APP: Failed to decode window icon asset: %s", path);
        return;
    }

    ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    SetWindowIcon(icon);
    UnloadImage(icon);
#endif
}

#if defined(PLATFORM_WEB)
static InnerBreeze*g_web_loop_app;
#endif

static const char *
trace_level_name(int log_level)
{
    switch(log_level) {
    case LOG_TRACE: return "TRACE";
    case LOG_DEBUG: return "DEBUG";
    case LOG_INFO: return "INFO";
    case LOG_WARNING: return "WARNING";
    case LOG_ERROR: return "ERROR";
    case LOG_FATAL: return "FATAL";
    default: return "INFO";
    }
}

static int
trace_has_prefix(const char *text, const char *prefix)
{
    size_t len;

    if(text == NULL || prefix == NULL)
        return 0;
    len = strlen(prefix);
    return strncmp(text, prefix, len) == 0;
}

static int
trace_is_quiet_text(const char *text)
{
    return trace_has_prefix(text, "IMAGE:") ||
           trace_has_prefix(text, "TEXTURE:") ||
           trace_has_prefix(text, "TIMER: Target time per frame");
}

static void
filtered_trace_log(int log_level, const char *text, va_list args)
{
    if(log_level < LOG_WARNING && trace_is_quiet_text(text))
        return;

#if ANDROID_BUILD
    if(log_level >= LOG_WARNING)
        __android_log_vprint(ANDROID_LOG_WARN, "InnerBreeze", text, args);
    return;
#endif
    fprintf(stderr, "%s: ", trace_level_name(log_level));
    vfprintf(stderr, text, args);
    fputc('\n', stderr);
}

static void
install_trace_log_filter(void)
{
#if defined(KRYON_BACKEND_TERMI)
    if(getenv("APP_TERMI_LOG") != NULL) {
        SetTraceLogCallback(filtered_trace_log);
        return;
    }
    SetTraceLogLevel(LOG_NONE);
#else
    SetTraceLogCallback(filtered_trace_log);
#endif
}

#if !defined(PLATFORM_WEB)
static volatile sig_atomic_t g_shutdown_requested;

static void
handle_shutdown_signal(int signum)
{
    (void)signum;
    g_shutdown_requested = 1;
}
#endif

#if defined(_WIN32) && !ANDROID_BUILD
static FILE *win_log_file;
static char win_recent_errors[WIN_ERROR_LOG_CAP];
static int win_recent_errors_len;

static void
windows_remember_error(const char *level, const char *message)
{
    if(message == NULL || message[0] == '\0')
        return;

    int written = snprintf(win_recent_errors + win_recent_errors_len,
                           sizeof(win_recent_errors) - (size_t)win_recent_errors_len,
                           "[%s] %s\n",
                           level,
                           message);

    if(written <= 0)
        return;

    if((size_t)written >= sizeof(win_recent_errors) - (size_t)win_recent_errors_len) {
        win_recent_errors_len = (int)sizeof(win_recent_errors) - 1;
        return;
    }

    win_recent_errors_len += written;
}

static void
windows_trace_log(int log_level, const char *text, va_list args)
{
    const char *level = "INFO";
    char message[1024];
    va_list message_args;

    va_copy(message_args, args);
    vsnprintf(message, sizeof(message), text, message_args);
    va_end(message_args);

    switch(log_level) {
    case LOG_TRACE: level = "TRACE"; break;
    case LOG_DEBUG: level = "DEBUG"; break;
    case LOG_INFO: level = "INFO"; break;
    case LOG_WARNING: level = "WARNING"; break;
    case LOG_ERROR: level = "ERROR"; break;
    case LOG_FATAL: level = "FATAL"; break;
    default: break;
    }

    if(log_level >= LOG_WARNING)
        windows_remember_error(level, message);

    if(win_log_file != NULL) {
        fprintf(win_log_file, "[%s] %s\n", level, message);
        fflush(win_log_file);
    }
}

static void
windows_show_startup_error(void)
{
    char dialog[3072];
    WindowsStartupErrorMessage(win_recent_errors, dialog, sizeof(dialog));
    MessageBoxA(NULL, dialog, "Inner Breeze", MB_OK | MB_ICONERROR);
}

static void
windows_install_logger(void)
{
    win_log_file = fopen("inbe.log", "ab");
    if(win_log_file != NULL)
        SetTraceLogCallback(windows_trace_log);
}

static void
windows_close_logger(void)
{
    if(win_log_file != NULL) {
        fclose(win_log_file);
        win_log_file = NULL;
    }
}
#endif

#if ANDROID_BUILD
static int
android_viewport_equal(AndroidViewport a, AndroidViewport b)
{
    return a.x == b.x && a.y == b.y &&
           a.width == b.width && a.height == b.height &&
           a.insets.left == b.insets.left &&
           a.insets.top == b.insets.top &&
           a.insets.right == b.insets.right &&
           a.insets.bottom == b.insets.bottom &&
           a.ready == b.ready;
}

static void
android_log_viewport_if_changed(int width, int height, AndroidViewport viewport)
{
    static AndroidViewport last_logged = {-1, -1, -1, -1, {-1, -1, -1, -1}, -1};

    if(android_viewport_equal(viewport, last_logged))
        return;
    TraceLog(LOG_INFO,
             "ANDROID_VIEWPORT: surface=%dx%d viewport=%d,%d %dx%d insets l=%d t=%d r=%d b=%d",
             width, height, viewport.x, viewport.y, viewport.width,
             viewport.height, viewport.insets.left, viewport.insets.top,
             viewport.insets.right, viewport.insets.bottom);
    last_logged = viewport;
}
#endif

void
native_frame_diagnostics(void)
{
#if !defined(PLATFORM_WEB) && !ANDROID_BUILD
    /* Memory snapshots at two steady-state points when diagnostics are on. */
    static int mem_debug_frame_count = 0;

    if(mem_debug_frame_count == 2 || mem_debug_frame_count == 240) {
        KryonMemReport(mem_debug_frame_count == 2 ? "frame-2" : "frame-240");
        TextFontMemoryReport(mem_debug_frame_count == 2 ? "frame-2" : "frame-240");
    }
    mem_debug_frame_count++;
#endif
}

#if ANDROID_BUILD
int
native_android_frame_viewport(AndroidViewport *viewport)
{
    int width;
    int height;
    if(!SyncAndroidViewport(viewport))
        return 0;
    GetAndroidSurfaceSize(&width, &height);
    android_log_viewport_if_changed(width, height, *viewport);
    return 1;
}
#endif

#if defined(PLATFORM_WEB)
static void
web_frame(void)
{
    if(g_web_loop_app != NULL)
        app_frame(g_web_loop_app);
}
#endif

#if !defined(PLATFORM_WEB) && !ANDROID_BUILD
void
native_screenshot_arm_readback(void)
{
#if defined(_WIN32)
    _putenv("KRYON_SHOT_ARM=1");
#else
    setenv("KRYON_SHOT_ARM", "1", 1);
#endif
}
#endif

void
native_limit_allocators(void)
{
#if defined(__GLIBC__)
    /* Cap glibc's per-thread malloc arenas. The app runs ~19 threads (audio,
     * tray, sync, SDL) and the default arena ceiling (8 per core) lets each
     * grow its own heap, inflating idle RSS. Four arenas are plenty here. */
    mallopt(M_ARENA_MAX, 4);
#endif
}

void
native_screenshot_data_root(void)
{
#if defined(_WIN32) || (!defined(PLATFORM_WEB) && !ANDROID_BUILD)
    char screenshot_data_root[256] = {0};
#endif
#if defined(_WIN32)
    snprintf(screenshot_data_root, sizeof(screenshot_data_root),
             "build/screenshot-data-%ld", (long)_getpid());
    _putenv_s("APP_DATA_ROOT", screenshot_data_root);
#elif !defined(PLATFORM_WEB) && !ANDROID_BUILD
    snprintf(screenshot_data_root, sizeof(screenshot_data_root),
             "/tmp/inbe-screenshot-%ld", (long)getpid());
    setenv("APP_DATA_ROOT", screenshot_data_root, 1);
#endif
}

void
native_install_trace_log_filter(void)
{
    install_trace_log_filter();
}

void
native_init_desktop_identity(void)
{
    init_desktop_identity();
}

void
native_window_environment(void)
{
#if ANDROID_BUILD
    __android_log_write(ANDROID_LOG_INFO, "APP_MAIN", "=== MAIN START ===");
#endif

#if !defined(PLATFORM_WEB) && !ANDROID_BUILD
    /*
     * SDL (the window backend here, plus the tray) disables the OS screensaver
     * by default. On X11 that suspends/reset the server's idle counter - the
     * same counter activity_monitor reads to tell active time from idle
     * time for break scheduling. With SDL's default the app looks permanently
     * active and the break timers count down even with nobody at the machine.
     * Let SDL leave the screensaver alone: a break reminder app has no
     * business blocking the screen saver anyway. Must be set before
     * InitWindow() initializes SDL video.
     */
#if defined(_WIN32)
    _putenv("SDL_VIDEO_ALLOW_SCREENSAVER=1");
#else
    setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "1", 1);
#endif
#endif

#if defined(_WIN32) && !ANDROID_BUILD
    windows_install_logger();
    TraceLog(LOG_INFO, "APP: Windows startup");
#endif
}

void
native_before_window(void)
{
#if ANDROID_BUILD
    android_insets_init();
    android_device_init();
    android_wakelock_init();
    android_runtime_assets_init();
    if(!ChangeDirectory("/data/user/0/xyz.waozi.inbe/files"))
        TraceLog(LOG_WARNING, "APP: failed to switch to Android files directory");
#endif
}

int
native_after_window(const char *icon_path)
{
    if(!IsWindowReady()) {
        TraceLog(LOG_ERROR, "APP: InitWindow failed");
#if defined(_WIN32) && !ANDROID_BUILD
        windows_show_startup_error();
        windows_close_logger();
#endif
        return 0;
    }
#if !defined(PLATFORM_WEB) && !defined(_WIN32) && !ANDROID_BUILD && defined(NATIVE_WINDOW_HAVE_SDL)
    /* raylib asks SDL for MOUSE_CAPTURE at window creation. An active
     * pointer grab on the main window swallows clicks and drags on every
     * other window of the process (break HUD), and nothing needs it. */
    SDL_SetWindowGrab(SDL_GetWindowFromID(1), SDL_FALSE);
#endif

    set_desktop_window_icon(icon_path);
    return 1;
}



#if defined(PLATFORM_WEB)
void
native_start_web_loop(InnerBreeze*app)
{
    g_web_loop_app = app;
    emscripten_set_main_loop(web_frame, 0, 1);
}
#else
void
native_install_shutdown_handlers(void)
{
    signal(SIGINT, handle_shutdown_signal);
    signal(SIGTERM, handle_shutdown_signal);
}

int
native_shutdown_requested(void)
{
    return g_shutdown_requested != 0;
}
#endif

void
native_platform_shutdown(void)
{
#if defined(_WIN32) && !ANDROID_BUILD
    windows_close_logger();
#endif
}

int
native_update_apply_at_exit(void)
{
    return update_apply_at_exit();
}
