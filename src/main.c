#include "kryon.h"
#if !defined(PLATFORM_WEB) && !defined(_WIN32) && !ANDROID_BUILD
#include <SDL2/SDL.h>
#endif
#include "app.h"
#include "breaks/app_breaks.h"
#include "desktop.h"
#include "storage.h"
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

#if !defined(PLATFORM_WEB) && !ANDROID_BUILD
/* From kryon's screenshot backend (src/backend/kry_screenshot.c): the PNG
 * writer that works on this GL stack, where raylib's ExportImage does not
 * honor the passed image. */
int kry_write_png_file(const char *path, const unsigned char *rgba,
                       int w, int h);
#endif

#if defined(__GLIBC__)
#include <malloc.h>
#endif

#if !defined(PLATFORM_WEB) && !defined(_WIN32) && !ANDROID_BUILD
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(INBE_DESKTOP_TRAY_ENABLED)
#include "platform/inbe_desktop_tray.h"
#endif

#if defined(_WIN32) && !ANDROID_BUILD
#include <process.h>
__declspec(dllimport) int __stdcall MessageBoxA(void *hwnd, const char *text,
                                                const char *caption, unsigned int type);
#define MB_OK 0x00000000u
#define MB_ICONERROR 0x00000010u
#define WIN_ERROR_LOG_CAP 2048
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

static void
set_desktop_window_icon(void)
{
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    const char *path = "assets/app/icon.png";
    const EmbeddedAsset *asset = GetEmbeddedAsset(path);
    Image icon;

    if(asset == NULL || asset->data == NULL || asset->size == 0) {
        TraceLog(LOG_WARNING, "INBE: Missing window icon asset: %s", path);
        return;
    }

    icon = LoadImageFromMemory(GetEmbeddedAssetExtension(path), asset->data, (int)asset->size);
    if(icon.data == NULL) {
        TraceLog(LOG_WARNING, "INBE: Failed to decode window icon asset: %s", path);
        return;
    }

    ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    SetWindowIcon(icon);
    UnloadImage(icon);
#endif
}

static InbeApp inbe_app;

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
           trace_has_prefix(text, "TEXTURE:");
}

static void
filtered_trace_log(int log_level, const char *text, va_list args)
{
    if(log_level < LOG_WARNING && trace_is_quiet_text(text))
        return;

    fprintf(stderr, "%s: ", trace_level_name(log_level));
    vfprintf(stderr, text, args);
    fputc('\n', stderr);
}

static void
install_trace_log_filter(void)
{
    SetTraceLogCallback(filtered_trace_log);
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

#if !defined(PLATFORM_WEB) && !defined(_WIN32) && !ANDROID_BUILD
static void
inbe_ensure_single_instance(void)
{
    char lock_path[600];
    int acquired;

    acquired = AcquireDesktopSingleInstanceMode(
        "xyz.waozi.inbe", DESKTOP_SINGLE_INSTANCE_REPLACE,
        lock_path, (int)sizeof(lock_path));
    if(acquired != 1) {
        TraceLog(LOG_WARNING, "INBE: cannot acquire single-instance lock: %s",
                 lock_path[0] != '\0' ? lock_path : "unknown path");
        return;
    }
    TraceLog(LOG_INFO, "INBE: single-instance lock acquired: %s", lock_path);
}
#else
static void
inbe_ensure_single_instance(void)
{
}
#endif

typedef struct ScreenshotRequest {
    int active;
    int width;
    int height;
    int theme_id;
    int dark_mode;
    int theme_style;
    char scene[64];
    char output[512];
} ScreenshotRequest;

#if defined(_WIN32) && !ANDROID_BUILD
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

#if ANDROID_BUILD
static AndroidInsets insets;

typedef struct AndroidViewport {
    int x;
    int y;
    int width;
    int height;
    int top;
    int bottom;
    int left;
    int right;
} AndroidViewport;

static int
android_clamp_content_size(int size, int leading_inset, int trailing_inset)
{
    int content_size = size - leading_inset - trailing_inset;

    if(content_size <= 0)
        return size;

    return content_size;
}

static int
android_nonnegative(int value)
{
    return value > 0 ? value : 0;
}

static AndroidViewport
android_resolve_viewport(int width, int height, AndroidInsets value)
{
    static AndroidViewport last_logged = {-1, -1, -1, -1, -1, -1, -1, -1};
    AndroidViewport viewport;
    int cutout_left = android_nonnegative(value.cutout_left);
    int cutout_right = android_nonnegative(value.cutout_right);
    int nav_bar = android_nonnegative(value.nav_bar);
    int content_top = android_nonnegative(value.status_bar);
    int cutout_top = android_nonnegative(value.cutout_top);

    if(cutout_top > content_top)
        content_top = cutout_top;

    viewport.left = cutout_left;
    viewport.right = cutout_right;
    viewport.top = content_top;
    viewport.bottom = 0;

    viewport.x = viewport.left;
    viewport.y = viewport.top;
    viewport.width = android_clamp_content_size(width, viewport.left, viewport.right);
    viewport.height = height - viewport.top;

    if(viewport.width <= 0) {
        viewport.x = 0;
        viewport.width = width;
    }
    if(viewport.height <= 0) {
        viewport.y = 0;
        viewport.height = height;
    }

    if(viewport.x != last_logged.x || viewport.y != last_logged.y ||
       viewport.width != last_logged.width || viewport.height != last_logged.height ||
       viewport.top != last_logged.top || viewport.bottom != last_logged.bottom ||
       viewport.left != last_logged.left || viewport.right != last_logged.right) {
        TraceLog(LOG_INFO,
                 "ANDROID_VIEWPORT: surface=%dx%d top=%d nav=%d viewport=%d,%d %dx%d insets l=%d t=%d r=%d b=%d",
                 width, height, content_top, nav_bar,
                 viewport.x, viewport.y, viewport.width, viewport.height,
                 viewport.left, viewport.top, viewport.right, viewport.bottom);
        last_logged = viewport;
    }

    return viewport;
}
#endif

static void
draw_full_frame(int width, int height)
{
    BeginDrawing();
    ClearBackground(GetThemeBackground());
    app_update_draw(&inbe_app, (Rectangle){
        0,
        0,
        (float)width,
        (float)height
    });
}

static void
frame(void)
{
#if defined(PLATFORM_WEB)
    SyncWebWindowSize();
#endif
#if !defined(PLATFORM_WEB) && !ANDROID_BUILD
    /* Memory snapshots at two steady-state points when diagnostics are on. */
    static int mem_debug_frame_count = 0;

    if(mem_debug_frame_count == 2 || mem_debug_frame_count == 240) {
        KryonMemReport(mem_debug_frame_count == 2 ? "frame-2" : "frame-240");
        UIFontMemoryReport(mem_debug_frame_count == 2 ? "frame-2" : "frame-240");
    }
    mem_debug_frame_count++;
#endif

    int width = GetScreenWidth();
    int height = GetScreenHeight();
#if !ANDROID_BUILD && !defined(PLATFORM_WEB)
    int render_width = GetRenderWidth();
    int render_height = GetRenderHeight();

    if(render_width > width)
        width = render_width;
    if(render_height > height)
        height = render_height;
#endif

#if ANDROID_BUILD
    AndroidViewport viewport;
    static AndroidViewport previous_viewport = {-1, -1, -1, -1, -1, -1, -1, -1};

    android_insets_get(&insets);
    if (!android_insets_is_initialized()) {
        return;
    }
    app_set_android_bottom_nav_height(android_nonnegative(insets.nav_bar));

    viewport = android_resolve_viewport(width, height, insets);
    if(viewport.x != previous_viewport.x || viewport.y != previous_viewport.y ||
       viewport.width != previous_viewport.width || viewport.height != previous_viewport.height ||
       viewport.top != previous_viewport.top || viewport.bottom != previous_viewport.bottom ||
       viewport.left != previous_viewport.left || viewport.right != previous_viewport.right) {
        InvalidateUIDPI();
        previous_viewport = viewport;
    }

    BeginDrawing();
    ClearBackground(BLACK);
    BeginUIClip(viewport.x, viewport.y, viewport.width, viewport.height);
    app_update_draw(&inbe_app, (Rectangle){
        (float)viewport.x,
        (float)viewport.y,
        (float)viewport.width,
        (float)viewport.height
    });
    EndUIClip();
#elif defined(PLATFORM_WEB)
    draw_full_frame(width, height);
#else
    draw_full_frame(width, height);
#endif
    EndDrawing();
    app_breaks_hud_update(&inbe_app);
    app_breaks_window_update(&inbe_app);
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

#if defined(PLATFORM_WEB) || ANDROID_BUILD
static void
parse_screenshot_args(int argc, char **argv, ScreenshotRequest *request)
{
    (void)argc;
    (void)argv;
    if(request != NULL)
        *request = (ScreenshotRequest){0};
}
#else
static void
parse_screenshot_args(int argc, char **argv, ScreenshotRequest *request)
{
    if(request == NULL)
        return;
    *request = (ScreenshotRequest){
        .width = config.width,
        .height = config.height,
        .theme_id = THEME_SKY,
        .dark_mode = 0,
        .theme_style = THEME_STYLE_SYSTEM
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
        } else if(strcmp(arg, "--screenshot-style") == 0 && value != NULL) {
            request->theme_style = parse_int_arg(value, request->theme_style);
            i++;
        }
    }

    if(request->scene[0] == '\0')
        snprintf(request->scene, sizeof(request->scene), "home");
    if(request->width < 320)
        request->width = 320;
    if(request->height < 320)
        request->height = 320;
    if(request->theme_style < THEME_STYLE_SYSTEM ||
       request->theme_style > THEME_STYLE_MATERIAL)
        request->theme_style = THEME_STYLE_SYSTEM;
    if(request->theme_id < 0 || request->theme_id >= THEME_COUNT)
        request->theme_id = THEME_SKY;
}
#endif

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
    app_refresh_theme(app);
}

static void
setup_screenshot_scene(InbeApp *app, const ScreenshotRequest *request)
{
    if(app == NULL || request == NULL)
        return;

    app->language_selected = 1;
    app_close_modal(app);
    app->settings_scroll = 0;
    app->manual_scroll = 0;
    app->tutorial_step = 0;
    app->habits_guide_seen = 1;
    app->tutorial_seen = 1;
    screenshot_seed_habits(app);
    app->theme_style = request->theme_style;
    screenshot_apply_theme(app, request->theme_id, request->dark_mode);

    if(strcmp(request->scene, "statistics") == 0) {
        app->main_tab = APP_MAIN_TAB_HABITS;
        app->habits.tab = HABIT_TAB_STATISTICS;
        app->inbe.screen = InbeScreenHabits;
    } else if(strcmp(request->scene, "profile") == 0) {
        app->main_tab = APP_MAIN_TAB_NONE;
        app->inbe.screen = InbeScreenProfile;
    } else if(strcmp(request->scene, "habit_edit") == 0) {
        app->main_tab = APP_MAIN_TAB_HABITS;
        app->inbe.screen = InbeScreenHabitEdit;
        habit_edit_begin_new(app);
    } else if(strcmp(request->scene, "language") == 0) {
        app->main_tab = APP_MAIN_TAB_NONE;
        app->inbe.screen = InbeScreenLanguage;
    } else if(strcmp(request->scene, "first_run_guide") == 0) {
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->exercise_type = EXERCISE_WIM_HOF;
        app->tutorial_seen = 0;
        app->tutorial_step = 0;
        app->inbe.screen = InbeScreenStart;
    } else if(strcmp(request->scene, "first_run_guide_blank") == 0) {
        app->main_tab = APP_MAIN_TAB_NONE;
        app->exercise_type = EXERCISE_WIM_HOF;
        app->tutorial_seen = 0;
        app->tutorial_step = 0;
        app->inbe.screen = InbeScreenStart;
    } else if(strcmp(request->scene, "background_music") == 0) {
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->exercise_type = EXERCISE_MEDITATION;
        app->practice_tab = PRACTICE_TAB_CONFIG;
        app->practice_config_tab = 1;
        app->inbe.screen = InbeScreenStart;
        app_open_modal(app, UIModalPracticeConfig);
    } else if(strcmp(request->scene, "practice_config_whm") == 0) {
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->exercise_type = EXERCISE_WIM_HOF;
        app->practice_tab = PRACTICE_TAB_CONFIG;
        app->inbe.screen = InbeScreenStart;
        app_open_modal(app, UIModalPracticeConfig);
    } else if(strcmp(request->scene, "practice_manual_whm") == 0) {
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->exercise_type = EXERCISE_WIM_HOF;
        app->practice_tab = PRACTICE_TAB_MANUAL;
        app->inbe.screen = InbeScreenStart;
        app_open_modal(app, UIModalPracticeManual);
    } else if(strcmp(request->scene, "wim_hof_session") == 0) {
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->exercise_type = EXERCISE_WIM_HOF;
        app->inbe.screen = InbeScreenSession;
    } else if(strcmp(request->scene, "calendar_meditation") == 0) {
        app->main_tab = APP_MAIN_TAB_HABITS;
        app->inbe.screen = InbeScreenHabits;
        app->habits.view_mode = HABIT_VIEW_CALENDAR;
    } else if(strcmp(request->scene, "habits_stats") == 0) {
        app->main_tab = APP_MAIN_TAB_HABITS;
        app->habits.tab = HABIT_TAB_STATISTICS;
        app->inbe.screen = InbeScreenHabits;
    } else if(strcmp(request->scene, "theme_selection") == 0) {
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->settings_tab = SETTINGS_TAB_THEME;
        app->inbe.screen = InbeScreenSettings;
    } else if(strcmp(request->scene, "settings_session") == 0) {
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->settings_tab = SETTINGS_TAB_DEVICE;
        app->inbe.screen = InbeScreenSettings;
    } else if(strcmp(request->scene, "settings_notifications") == 0) {
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->settings_tab = SETTINGS_TAB_NOTIFICATIONS;
        app->inbe.screen = InbeScreenSettings;
    } else if(strcmp(request->scene, "data") == 0 ||
              strcmp(request->scene, "profile_data") == 0) {
        app->profile_view = PROFILE_VIEW_MAIN;
        app->profile_tab = PROFILE_TAB_DATA;
        app->inbe.screen = InbeScreenProfile;
    } else if(strcmp(request->scene, "my_practices") == 0 ||
              strcmp(request->scene, "profile_practices") == 0) {
        app->profile_view = PROFILE_VIEW_PRACTICES;
        app->profile_tab = PROFILE_TAB_OVERVIEW;
        app->inbe.screen = InbeScreenProfile;
    } else if(strcmp(request->scene, "configure_account") == 0 ||
              strcmp(request->scene, "profile_sync_account") == 0) {
        app->profile_view = PROFILE_VIEW_MAIN;
        app->profile_tab = PROFILE_TAB_OVERVIEW;
        app->inbe.screen = InbeScreenProfile;
    } else if(strcmp(request->scene, "cobalt_dark") == 0) {
        screenshot_apply_theme(app, THEME_COBALT, 1);
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->inbe.screen = InbeScreenStart;
    } else if(strcmp(request->scene, "patterns") == 0) {
        app->exercise_type = EXERCISE_PATTERNS;
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        patterns_practice_start(app);
    } else if(strcmp(request->scene, "patterns_config") == 0) {
        app->exercise_type = EXERCISE_PATTERNS;
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->practice_tab = PRACTICE_TAB_CONFIG;
        app->inbe.screen = InbeScreenStart;
    } else if(strcmp(request->scene, "break_exercises") == 0) {
#if !defined(PLATFORM_WEB) && !ANDROID_BUILD
        app->inbe.screen = InbeScreenBreakExercises;
#endif
    } else if(strcmp(request->scene, "break_settings") == 0) {
#if !defined(PLATFORM_WEB) && !ANDROID_BUILD
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->settings_tab = SETTINGS_TAB_BREAKS;
        app->breaks_enabled = 1;
        app->inbe.screen = InbeScreenSettings;
#endif
    } else if(strcmp(request->scene, "break_micro") == 0 ||
              strcmp(request->scene, "break_rest") == 0 ||
              strcmp(request->scene, "break_daily") == 0) {
#if !defined(PLATFORM_WEB) && !ANDROID_BUILD
        int t = strcmp(request->scene, "break_rest") == 0 ? BREAK_REST
                : strcmp(request->scene, "break_daily") == 0 ? BREAK_DAILY
                : BREAK_MICRO;

        app->breaks_enabled = 1;
        app->break_block_mode = 0;
        app->breaks.timers[t].active_s = app->breaks.timers[t].limit_s;
        app->breaks.timers[t].state = BreakStateBreaking;
        app->breaks.timers[t].idle_s = app->breaks.timers[t].duration_s > 0
                                           ? app->breaks.timers[t].duration_s / 3
                                           : 0;
        app->inbe.screen = InbeScreenBreak;
        if(t == BREAK_REST) {
            app->break_ex_picked[0] = 0;
            app->break_ex_picked[1] = 6;
            app->break_ex_picked[2] = 8;
            app->break_ex_pick_count = 3;
            app->breaks.timers[BREAK_REST].idle_s = 200;
            app->break_ex_offset_s = 0;
            app->break_ex_paused = 0;
            app->break_ex_hidden = 0;
        }
#endif
    } else if(strcmp(request->scene, "tutorial_whm_step0") == 0) {
        app->exercise_type = EXERCISE_WIM_HOF;
        app->practice_tab = PRACTICE_TAB_MANUAL;
        app->inbe.screen = InbeScreenStart;
        app->tutorial_step = 0;
    } else if(strcmp(request->scene, "tutorial_whm_step2") == 0) {
        app->exercise_type = EXERCISE_WIM_HOF;
        app->practice_tab = PRACTICE_TAB_MANUAL;
        app->inbe.screen = InbeScreenStart;
        app->tutorial_step = 2;
    } else if(strcmp(request->scene, "tutorial_meditation") == 0) {
        app->exercise_type = EXERCISE_MEDITATION;
        app->practice_tab = PRACTICE_TAB_MANUAL;
        app->inbe.screen = InbeScreenStart;
        app->tutorial_step = 0;
    } else {
        app->main_tab = APP_MAIN_TAB_PRACTICE;
        app->inbe.screen = InbeScreenStart;
    }
}

#if defined(PLATFORM_WEB) || ANDROID_BUILD
static int
run_screenshot_mode(const ScreenshotRequest *request)
{
    (void)request;
    return 0;
}
#else
static int
run_screenshot_mode(const ScreenshotRequest *request)
{
    Image capture;
    int warmup_frames = 4;
    int saved;

    if(request == NULL || !request->active)
        return 0;

    /* LoadImageFromScreen only returns a frame while kryon's EndDrawing is
     * armed for the pre-swap readback; screenshot mode arms it itself so no
     * wrapper script has to. */
#if defined(_WIN32)
    _putenv("KRYON_SHOT_ARM=1");
#else
    setenv("KRYON_SHOT_ARM", "1", 1);
#endif
    setup_screenshot_scene(&inbe_app, request);
    if(strcmp(request->scene, "tutorial_whm_step2") == 0)
        warmup_frames = 150;
    for(int i = 0; i < warmup_frames; i++)
        frame();

    if(strcmp(request->scene, "tutorial_whm_step2") == 0) {
        inbe_app.tutorial_step = 2;
    } else if(strcmp(request->scene, "tutorial_whm_step0") == 0) {
        inbe_app.tutorial_step = 0;
    } else if(strcmp(request->scene, "tutorial_meditation") == 0) {
        inbe_app.tutorial_step = 0;
    }
    if(getenv("INBE_SHOT_WINDOW") != NULL) {
        /* Fallback for GL stacks where LoadImageFromScreen reads blank:
         * hold the warmed-up scene on screen for external capture. */
        for(;;)
            frame();
    }
    capture = LoadImageFromScreen();
    if(capture.data == NULL)
        return 1;

    /* raylib's ExportImage does not honor the passed image on this GL
     * stack; kryon's own writer is the working path. */
    saved = kry_write_png_file(request->output, capture.data,
                               capture.width, capture.height) == 0;
    free(capture.data);
    capture.data = NULL;
    return saved ? 1 : -1;
}
#endif

int main(int argc, char **argv) {
    ScreenshotRequest screenshot;
    char screenshot_data_root[256] = {0};

#if defined(__GLIBC__)
    /* Cap glibc's per-thread malloc arenas. The app runs ~19 threads (audio,
     * tray, sync, SDL) and the default arena ceiling (8 per core) lets each
     * grow its own heap, inflating idle RSS. Four arenas are plenty here. */
    mallopt(M_ARENA_MAX, 4);
#endif
    parse_screenshot_args(argc, argv, &screenshot);
    if(screenshot.active) {
#if defined(_WIN32)
        snprintf(screenshot_data_root, sizeof(screenshot_data_root),
                 "build/screenshot-data-%ld", (long)_getpid());
        _putenv_s("INBE_DATA_ROOT", screenshot_data_root);
#elif !defined(PLATFORM_WEB) && !ANDROID_BUILD
        snprintf(screenshot_data_root, sizeof(screenshot_data_root),
                 "/tmp/inbe-screenshot-%ld", (long)getpid());
        setenv("INBE_DATA_ROOT", screenshot_data_root, 1);
#endif
    }
    install_trace_log_filter();
    if(screenshot.active) {
        SetTraceLogLevel(LOG_WARNING);
        config.width = screenshot.width;
        config.height = screenshot.height;
    }
    int window_w = ANDROID_BUILD ? 0 : config.width;
    int window_h = ANDROID_BUILD ? 0 : config.height;

#if defined(PLATFORM_WEB)
    GetWebViewportSize(config.width, config.height, &window_w, &window_h);
    config.width = window_w;
    config.height = window_h;
#endif

#if ANDROID_BUILD
    __android_log_write(ANDROID_LOG_INFO, "INBE_MAIN", "=== MAIN START ===");
#endif

#if !defined(PLATFORM_WEB) && !ANDROID_BUILD
    /*
     * SDL (the window backend here, plus the tray) disables the OS screensaver
     * by default. On X11 that suspends/reset the server's idle counter - the
     * same counter inbe_activity_monitor reads to tell active time from idle
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

#if defined(PLATFORM_WEB)
    SetConfigFlags(GetWebWindowFlags());
#elif !ANDROID_BUILD
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_ALWAYS_RUN);
#endif

#if !defined(PLATFORM_WEB) && !defined(_WIN32) && !ANDROID_BUILD
    /* One live instance per user: a normal launch replaces any existing
     * one; screenshot mode is one-shot and leaves the running app alone. */
    if(!screenshot.active)
        inbe_ensure_single_instance();
#endif

#if defined(_WIN32) && !ANDROID_BUILD
    windows_install_logger();
    TraceLog(LOG_INFO, "INBE: Windows startup");
#endif

#if ANDROID_BUILD
    android_insets_init();
    android_device_init();
    android_wakelock_init();
    android_runtime_assets_init();
    if(!ChangeDirectory("/data/user/0/xyz.waozi.inbe/files"))
        TraceLog(LOG_WARNING, "INBE: failed to switch to Android files directory");
#endif

    InitWindow(window_w, window_h, config.title);
    if(!IsWindowReady()) {
        TraceLog(LOG_ERROR, "INBE: InitWindow failed");
#if defined(_WIN32) && !ANDROID_BUILD
        windows_show_startup_error();
        windows_close_logger();
#endif
        return 1;
    }
#if !defined(PLATFORM_WEB) && !defined(_WIN32) && !ANDROID_BUILD
    /* raylib asks SDL for MOUSE_CAPTURE at window creation. An active
     * pointer grab on the main window swallows clicks and drags on every
     * other window of the process (break HUD), and nothing needs it. */
    SDL_SetWindowGrab(SDL_GetWindowFromID(1), SDL_FALSE);
#endif

    set_desktop_window_icon();
#if !defined(PLATFORM_WEB) && !ANDROID_BUILD
    /* Disable raylib's built-in ESC-to-exit. We surface close requests through
     * our own "keep running / quit?" prompt (see app_request_desktop_close),
     * and ESC is already handled by individual screens via IsKeyPressed. */
    SetExitKey(0);
#endif
    InitUIDPI();
    app_init(&inbe_app);
    set_global_inbe_app(&inbe_app);
    TraceLog(LOG_INFO, "INBE: Global app pointer set");
    KryonMemReport("after-app-init");
    UIFontMemoryReport("after-app-init");

    #if ANDROID_BUILD
    inbe_app.fullscreen_enabled = 0;
    #endif
    SetTargetFPS(60);

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(frame, 0, 1);
#else
    int quit = 0;
    signal(SIGINT, handle_shutdown_signal);
    signal(SIGTERM, handle_shutdown_signal);
#if defined(INBE_DESKTOP_TRAY_ENABLED)
    inbe_desktop_tray_init();
    /* "Start minimized" startup mode: launch straight to the tray; break
     * windows restore on top when a break fires. Hidden startup is only
     * honored with a live tray - otherwise there is nothing to restore
     * the window from and the app would be unreachable. */
    if(inbe_app.desktop_startup_mode == INBE_STARTUP_HIDDEN &&
       !screenshot.active && inbe_desktop_tray_ready()) {
        TraceLog(LOG_INFO, "INBE: starting hidden in the tray (startup mode)");
        inbe_desktop_tray_keep_running();
    }
    KryonMemReport("after-tray-init");
#endif

    int screenshot_result = run_screenshot_mode(&screenshot);
    if(screenshot_result != 0) {
#if defined(INBE_DESKTOP_TRAY_ENABLED)
        inbe_desktop_tray_shutdown();
#endif
        CloseWindow();
#if defined(_WIN32) && !ANDROID_BUILD
        windows_close_logger();
#endif
        return screenshot_result > 0 ? 0 : 1;
    }

#if defined(INBE_DESKTOP_TRAY_ENABLED)
    /* With a tray, the close button asks whether to keep running in the
     * background (hiding to tray) or quit. The SDL_QUIT that raylib would
     * otherwise latch is already swallowed by the tray's event filter, so
     * WindowShouldClose() can only become true via a stray path; guard the
     * re-fire so the prompt never pops again while already open. */
    while(!quit) {
        InbeDesktopTrayAction tray_action = inbe_desktop_tray_poll_action();
        AppClosePromptResult close_result;
        if(g_shutdown_requested)
            quit = 1;
        if(tray_action != INBE_DESKTOP_TRAY_ACTION_NONE)
            inbe_desktop_tray_apply_action(&inbe_app, tray_action, &quit);
        /* The core window's X button arrives as SDL_WINDOWEVENT_CLOSE, which
         * raylib's SDL backend never latches as SDL_QUIT; kryon's window
         * pump records it and we act on it exactly like a tray close. */
        if(StealUICoreWindowClose())
            inbe_desktop_tray_apply_action(&inbe_app,
                                           INBE_DESKTOP_TRAY_ACTION_CLOSE_REQUEST,
                                           &quit);
        if(!quit) {
            frame();
            inbe_desktop_tray_update_status(&inbe_app);
        }
        close_result = app_consume_close_prompt_result(&inbe_app);
        if(close_result == AppClosePromptKeepRunning)
            inbe_desktop_tray_keep_running();
        else if(close_result == AppClosePromptQuit)
            quit = 1;
        if(inbe_app.request_quit)   /* app layer (e.g. update restart) exits directly */
            quit = 1;
        tray_action = inbe_desktop_tray_poll_action();
        if(tray_action != INBE_DESKTOP_TRAY_ACTION_NONE)
            inbe_desktop_tray_apply_action(&inbe_app, tray_action, &quit);
        if((WindowShouldClose() || StealUICoreWindowClose()) &&
           !inbe_app.close_prompt_open)
            app_request_desktop_close(&inbe_app);
    }
#else
    /* No tray: there is nothing to keep running in the background, so a close
     * request (window button, WM, signal, or a Ctrl+Q/Esc shortcut via
     * app_request_desktop_quit) just quits. No prompt. */
    while(!g_shutdown_requested && !quit) {
        frame();
        if(WindowShouldClose() || StealUICoreWindowClose() ||
           g_shutdown_requested || inbe_app.request_quit)
            quit = 1;
    }
#endif

#if defined(INBE_DESKTOP_TRAY_ENABLED)
    inbe_desktop_tray_shutdown();
#endif
    CloseWindow();
#if defined(_WIN32) && !ANDROID_BUILD
    windows_close_logger();
#endif
#endif
    /* Desktop self-update: re-exec the staged AppImage after all state is
     * saved and the window/audio stack is down. */
    if(inbe_update_apply_at_exit())
        return 0;
    return 0;
}
