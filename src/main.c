#include "raylib.h"
#include "app.h"
#include "flint_dpi.h"
#include "flint_web.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#if defined(_WIN32) && !defined(__ANDROID__)
__declspec(dllimport) int __stdcall MessageBoxA(void *hwnd, const char *text,
                                                const char *caption, unsigned int type);
#define MB_OK 0x00000000u
#define MB_ICONERROR 0x00000010u
#define WIN_ERROR_LOG_CAP 2048
#endif

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include "android_insets.h"
#include "android_wakelock.h"
#include "android_timer.h"
#include <android/log.h>
#include <android_native_app_glue.h>
extern struct android_app *GetAndroidApp(void);
#endif

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

static InbeApp inbe_app;
static InbeApp *g_inbe_app_ptr = NULL;  // Global pointer for JNI access

#if defined(_WIN32) && !defined(__ANDROID__)
static FILE *win_log_file;
static char win_recent_errors[WIN_ERROR_LOG_CAP];
static int win_recent_errors_len;

static int
windows_text_contains(const char *text, const char *needle)
{
    if(text == NULL || needle == NULL || needle[0] == '\0')
        return 0;

    for(const char *p = text; *p != '\0'; p++) {
        const char *a = p;
        const char *b = needle;
        while(*a != '\0' && *b != '\0' && *a == *b) {
            a++;
            b++;
        }
        if(*b == '\0')
            return 1;
    }

    return 0;
}

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
    const char *detail = win_recent_errors[0] != '\0' ?
                         win_recent_errors :
                         "No detailed startup error was reported.";
    const char *hint = "";

    if(windows_text_contains(detail, "OpenGL") ||
       windows_text_contains(detail, "WGL") ||
       windows_text_contains(detail, "GLFW")) {
        hint = "\nThis is usually a graphics driver or virtual GPU problem. "
               "Update the GPU driver, enable VM 3D acceleration, or install the VM guest graphics driver.\n";
    }

    snprintf(dialog,
             sizeof(dialog),
             "Inner Breeze could not create a window.\n\n%s%s\nA full log was written to inbe.log next to the executable.",
             detail,
             hint);

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

InbeApp* get_global_inbe_app(void) {
    if (g_inbe_app_ptr == NULL) {
        TraceLog(LOG_WARNING, "WARNING: get_global_inbe_app called with NULL pointer");
    }
    return g_inbe_app_ptr;
}

void set_global_inbe_app(InbeApp *app);

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define INBE_ANDROID_BUILD 1
#else
#define INBE_ANDROID_BUILD 0
#endif

#if INBE_ANDROID_BUILD
static AndroidInsets insets;

static int
android_clamp_content_size(int size, int leading_inset, int trailing_inset)
{
    int content_size = size - leading_inset - trailing_inset;

    if(content_size <= 0)
        return size;

    return content_size;
}
#endif

static void
frame(void)
{
#if defined(PLATFORM_WEB)
    flint_web_sync_window_size();
#endif

    int width = GetScreenWidth();
    int height = GetScreenHeight();
#if !defined(PLATFORM_WEB)
    int render_width = GetRenderWidth();
    int render_height = GetRenderHeight();

    if(render_width > width)
        width = render_width;
    if(render_height > height)
        height = render_height;
#endif

#if INBE_ANDROID_BUILD
    android_insets_get(&insets);

    int safe_top = insets.cutout_top > insets.status_bar ?
                   insets.cutout_top : insets.status_bar;
    int safe_bottom = insets.cutout_bottom > insets.nav_bar ?
                      insets.cutout_bottom : insets.nav_bar;
    int safe_left = insets.cutout_left;
    int safe_right = insets.cutout_right;

    int content_x = safe_left;
    int content_y = safe_top;
    int content_width = android_clamp_content_size(width, safe_left, safe_right);
    int content_height = android_clamp_content_size(height, safe_top, safe_bottom);

    BeginDrawing();
    ClearBackground(BLACK);
    BeginScissorMode(content_x, content_y, content_width, content_height);
    inbe_app_update_draw(&inbe_app, (Rectangle){
        (float)content_x,
        (float)content_y,
        (float)content_width,
        (float)content_height
    });
    EndScissorMode();
#else
    BeginDrawing();
    ClearBackground(BLACK);
    inbe_app_update_draw(&inbe_app, (Rectangle){
        0,
        0,
        (float)width,
        (float)height
    });
    if(inbe_app.cursor_clickable)
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    else
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
#endif
    EndDrawing();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int window_w = INBE_ANDROID_BUILD ? 0 : config.width;
    int window_h = INBE_ANDROID_BUILD ? 0 : config.height;

#if defined(PLATFORM_WEB)
    flint_web_viewport_size(config.width, config.height, &window_w, &window_h);
    config.width = window_w;
    config.height = window_h;
#endif

#if INBE_ANDROID_BUILD
    __android_log_write(ANDROID_LOG_INFO, "INBE_MAIN", "=== MAIN START ===");
#endif

#if defined(PLATFORM_WEB)
    SetConfigFlags(flint_web_window_flags());
#elif !defined(PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(ANDROID)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif

#if defined(_WIN32) && !defined(__ANDROID__)
    windows_install_logger();
    TraceLog(LOG_INFO, "INBE: Windows startup");
#endif

#if INBE_ANDROID_BUILD
    __android_log_write(ANDROID_LOG_INFO, "INBE_MAIN", "Calling android_insets_init");
    android_insets_init();
    __android_log_write(ANDROID_LOG_INFO, "INBE_MAIN", "Calling android_wakelock_init");
    android_wakelock_init();
    __android_log_write(ANDROID_LOG_INFO, "INBE_MAIN", "Calling android_timer_init");
    android_timer_init();
    __android_log_write(ANDROID_LOG_INFO, "INBE_MAIN", "Init calls done");
    if(!ChangeDirectory("/data/user/0/xyz.waozi.inbe/files"))
        TraceLog(LOG_WARNING, "INBE: failed to switch to Android files directory");
#endif


    InitWindow(window_w, window_h, config.title);
    if(!IsWindowReady()) {
        TraceLog(LOG_ERROR, "INBE: InitWindow failed");
#if defined(_WIN32) && !defined(__ANDROID__)
        windows_show_startup_error();
        windows_close_logger();
#endif
        return 1;
    }

    flint_dpi_init();
    inbe_app_init(&inbe_app);
    set_global_inbe_app(&inbe_app);
    TraceLog(LOG_INFO, "INBE: Global app pointer set");

    #if INBE_ANDROID_BUILD
    inbe_app.fullscreen_enabled = 0;
    #endif
    if(inbe_app.fullscreen_enabled && !IsWindowFullscreen())
        ToggleFullscreen();

    SetTargetFPS(60);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(frame, 0, 1);
#else
    while(!WindowShouldClose()) {
        frame();
    }

    CloseWindow();
#if defined(_WIN32) && !defined(__ANDROID__)
    windows_close_logger();
#endif
#endif
    return 0;
}

void set_global_inbe_app(InbeApp *app) {
    g_inbe_app_ptr = app;
    TraceLog(LOG_INFO, "INBE: Global app pointer set to %p", app);
}
