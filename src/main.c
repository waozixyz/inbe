#include "raylib.h"
#include "app.h"
#include "storage.h"
#include "practices/practice_registry.h"
#include "flint_clip.h"
#include "flint_dpi.h"
#include "flint_theme_meta.h"
#include "flint_web.h"
#include "theme.h"
#include "device_preferences.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define INBE_ANDROID_BUILD 1
#else
#define INBE_ANDROID_BUILD 0
#endif


#if defined(_WIN32) && !defined(__ANDROID__)
__declspec(dllimport) int __stdcall MessageBoxA(void *hwnd, const char *text,
                                                const char *caption, unsigned int type);
#define MB_OK 0x00000000u
#define MB_ICONERROR 0x00000010u
#define WIN_ERROR_LOG_CAP 2048
#endif

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
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

static InbeApp inbe_app;
static InbeApp *g_inbe_app_ptr = NULL;

typedef struct ScreenshotRequest {
    int active;
    int width;
    int height;
    int theme_id;
    int dark_mode;
    char scene[64];
    char output[512];
} ScreenshotRequest;

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
draw_full_frame(int width, int height)
{
    BeginDrawing();
    ClearBackground(theme_get_bg());
    app_update_draw(&inbe_app, (Rectangle){
        0,
        0,
        (float)width,
        (float)height
    });
    if(inbe_app.cursor_disabled)
        SetMouseCursor(MOUSE_CURSOR_NOT_ALLOWED);
    else if(inbe_app.cursor_clickable)
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    else
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
}

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
    flint_clip_begin(content_x, content_y, content_width, content_height);
    app_update_draw(&inbe_app, (Rectangle){
        (float)content_x,
        (float)content_y,
        (float)content_width,
        (float)content_height
    });
    flint_clip_end();
#elif defined(PLATFORM_WEB)
    draw_full_frame(width, height);
#else
    draw_full_frame(width, height);
#endif
    EndDrawing();
}

static int
parse_int_arg(const char *text, int fallback)
{
    char *end = NULL;
    long value;

    if(text == NULL || text[0] == '\0')
        return fallback;
    value = strtol(text, &end, 10);
    if(end == text)
        return fallback;
    return (int)value;
}

static void
parse_screenshot_args(int argc, char **argv, ScreenshotRequest *request)
{
    if(request == NULL)
        return;
    *request = (ScreenshotRequest){
        .width = config.width,
        .height = config.height,
        .theme_id = FLINT_THEME_SKY,
        .dark_mode = 0
    };

    for(int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : NULL;
        if(strcmp(arg, "--screenshot") == 0 && value != NULL) {
            request->active = 1;
            snprintf(request->output, sizeof(request->output), "%s", value);
            i++;
        } else if(strcmp(arg, "--screenshot-scene") == 0 && value != NULL) {
            snprintf(request->scene, sizeof(request->scene), "%s", value);
            i++;
        } else if(strcmp(arg, "--screenshot-width") == 0 && value != NULL) {
            request->width = parse_int_arg(value, request->width);
            i++;
        } else if(strcmp(arg, "--screenshot-height") == 0 && value != NULL) {
            request->height = parse_int_arg(value, request->height);
            i++;
        } else if(strcmp(arg, "--screenshot-theme") == 0 && value != NULL) {
            request->theme_id = parse_int_arg(value, request->theme_id);
            i++;
        } else if(strcmp(arg, "--screenshot-dark") == 0 && value != NULL) {
            request->dark_mode = parse_int_arg(value, request->dark_mode) != 0;
            i++;
        }
    }

    if(request->scene[0] == '\0')
        snprintf(request->scene, sizeof(request->scene), "home");
    if(request->width < 320)
        request->width = 320;
    if(request->height < 320)
        request->height = 320;
    if(request->theme_id < 0 || request->theme_id >= FLINT_THEME_COUNT)
        request->theme_id = FLINT_THEME_SKY;
}

static int
screenshot_day_index_offset(int offset_days)
{
    time_t now = time(NULL);
    struct tm day;

    day = *localtime(&now);
    day.tm_hour = 12;
    day.tm_min = 0;
    day.tm_sec = 0;
    day.tm_mday -= offset_days;
    mktime(&day);
    return (day.tm_year + 1900) * 10000 + (day.tm_mon + 1) * 100 + day.tm_mday;
}

static void
screenshot_seed_habits(InbeApp *app)
{
    static const int offsets[] = {0, 1, 2, 4, 5, 7, 8, 10, 12, 13,
                                  15, 17, 18, 20, 21, 24, 25, 27};
    static const int counts[] = {2, 1, 1, 3, 1, 1, 2, 1, 1, 2,
                                 1, 1, 2, 1, 3, 1, 1, 2};

    if(app == NULL)
        return;
    if(app->habits.count <= 0)
        habits_add_default_set(&app->habits);
    if(app->habits.count <= 0)
        return;

    app->habits.selected = 0;
    app->habits.items[0].sync_mode = INBE_HABIT_SYNC_ACTIVITIES;
    app->habits.items[0].sync_activity = habit_activity_mask_for(EXERCISE_WIM_HOF) |
                                         habit_activity_mask_for(EXERCISE_MEDITATION);
    app->habits.items[0].counter_enabled = 0;
    habits_clear_days(&app->habits);
    for(size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++)
        habit_set_day_count(&app->habits, 0, screenshot_day_index_offset(offsets[i]), counts[i]);

    for(int day = 6; day >= 0; day--) {
        int progress = 6 - day;
        int local_date = screenshot_day_index_offset(day);
        int rounds[] = {
            32 + progress * 4,
            37 + progress * 4,
            41 + progress * 5
        };
        storage_save_session_at_for_activity(local_date, 7, 15 + progress, 0,
                                             rounds, 3, 0, EXERCISE_WIM_HOF,
                                             NULL, 0);
    }
}

static void
screenshot_apply_theme(InbeApp *app, int theme_id, int dark_mode)
{
    if(app == NULL)
        return;
    app->theme_id = theme_id;
    app->theme_mode = dark_mode ? APP_THEME_DARK : APP_THEME_LIGHT;
    app->dark_mode = dark_mode != 0;
    refresh_theme_colors(app->theme_id, app->dark_mode);
}

static void
setup_screenshot_scene(InbeApp *app, const ScreenshotRequest *request)
{
    if(app == NULL || request == NULL)
        return;

    app->language_selected = 1;
    app->modal.active = 0;
    app->settings_scroll = 0;
    app->manual_scroll = 0;
    app->tutorial_step = 0;
    screenshot_seed_habits(app);
    screenshot_apply_theme(app, request->theme_id, request->dark_mode);

    if(strcmp(request->scene, "calendar_meditation") == 0) {
        app->main_tab = APP_MAIN_TAB_HABITS;
        app->inbe.screen = InbeScreenHabits;
        app->habits.view_mode = HABIT_VIEW_CALENDAR;
    } else if(strcmp(request->scene, "habits_stats") == 0) {
        app->main_tab = APP_MAIN_TAB_HABITS;
        app->inbe.screen = InbeScreenHabits;
        app->habits.view_mode = HABIT_VIEW_STATS;
    } else if(strcmp(request->scene, "theme_selection") == 0) {
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->settings_tab = SETTINGS_TAB_THEME;
        app->inbe.screen = InbeScreenSettings;
    } else if(strcmp(request->scene, "cobalt_dark") == 0) {
        screenshot_apply_theme(app, FLINT_THEME_COBALT, 1);
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->inbe.screen = InbeScreenStart;
    } else if(strcmp(request->scene, "manual_whm") == 0) {
        app->exercise_type = EXERCISE_WIM_HOF;
        app->tutorial_step = 0;
        app->inbe.screen = InbeScreenManual;
    } else if(strcmp(request->scene, "tutorial_whm_step2") == 0) {
        app->exercise_type = EXERCISE_WIM_HOF;
        app->tutorial_step = 2;
        app->inbe.screen = InbeScreenManual;
    } else if(strcmp(request->scene, "meditation_tutorial") == 0) {
        app->exercise_type = EXERCISE_MEDITATION;
        app->tutorial_step = 0;
        app->inbe.screen = InbeScreenManual;
    } else {
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->inbe.screen = InbeScreenStart;
    }
}

static int
run_screenshot_mode(const ScreenshotRequest *request)
{
    Image capture;
    char output[512];
    char cwd[512];
    char *slash;
    int warmup_frames = 4;

    if(request == NULL || !request->active)
        return 0;

    setup_screenshot_scene(&inbe_app, request);
    if(strcmp(request->scene, "tutorial_whm_step2") == 0)
        warmup_frames = 150;
    for(int i = 0; i < warmup_frames; i++)
        frame();
    capture = LoadImageFromScreen();
    if(capture.data == NULL)
        return 1;

    snprintf(output, sizeof(output), "%s", request->output);
    snprintf(cwd, sizeof(cwd), "%s", GetWorkingDirectory());
    slash = strrchr(output, '/');
    if(slash != NULL) {
        *slash = '\0';
        if(ChangeDirectory(output))
            ExportImage(capture, slash + 1);
        ChangeDirectory(cwd);
    } else {
        ExportImage(capture, output);
    }
    UnloadImage(capture);
    return 1;
}

int main(int argc, char **argv) {
    ScreenshotRequest screenshot;
    parse_screenshot_args(argc, argv, &screenshot);
    if(screenshot.active) {
        SetTraceLogLevel(LOG_WARNING);
        config.width = screenshot.width;
        config.height = screenshot.height;
    }
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
    android_insets_init();
    android_device_init();
    android_wakelock_init();
    android_timer_init();
    android_runtime_assets_init();
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
    app_init(&inbe_app);
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
    if(run_screenshot_mode(&screenshot)) {
        CloseWindow();
#if defined(_WIN32) && !defined(__ANDROID__)
        windows_close_logger();
#endif
        return 0;
    }

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
