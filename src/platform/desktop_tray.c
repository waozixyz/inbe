#include "desktop_tray.h"

#if defined(INBE_DESKTOP_TRAY_ENABLED)

#include "app.h"
#include "practices/practice_registry.h"
#include "raylib.h"

#if defined(INBE_DESKTOP_TRAY_AYATANA)
#include <libayatana-appindicator/app-indicator.h>
#elif defined(INBE_DESKTOP_TRAY_APPINDICATOR)
#include <libappindicator/app-indicator.h>
#endif

#include <SDL.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static pthread_t tray_thread;
static pthread_mutex_t tray_action_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tray_state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tray_status_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t tray_state_cond = PTHREAD_COND_INITIALIZER;
static InbeDesktopTrayAction pending_action = INBE_DESKTOP_TRAY_ACTION_NONE;
static int tray_started;
static int tray_state;
static char tray_status_text[128] = "Inner Breeze";
static int tray_status_update_pending;
#if defined(INBE_DESKTOP_TRAY_GTK_STATUS_ICON)
static GtkStatusIcon *tray_status_icon;
#endif

enum {
    DESKTOP_TRAY_STATE_STARTING = 0,
    DESKTOP_TRAY_STATE_READY,
    DESKTOP_TRAY_STATE_FAILED,
    DESKTOP_TRAY_STATE_STOPPED
};

static void
desktop_tray_set_action(InbeDesktopTrayAction action)
{
    pthread_mutex_lock(&tray_action_lock);
    pending_action = action;
    pthread_mutex_unlock(&tray_action_lock);
}

InbeDesktopTrayAction
inbe_desktop_tray_poll_action(void)
{
    InbeDesktopTrayAction action;

    pthread_mutex_lock(&tray_action_lock);
    action = pending_action;
    pending_action = INBE_DESKTOP_TRAY_ACTION_NONE;
    pthread_mutex_unlock(&tray_action_lock);

    return action;
}

static int
desktop_tray_sdl_event_filter(void *userdata, SDL_Event *event)
{
    (void)userdata;

    if(event != NULL && event->type == SDL_QUIT) {
        desktop_tray_set_action(INBE_DESKTOP_TRAY_ACTION_CLOSE_REQUEST);
        return 0;
    }

    return 1;
}

static void
desktop_tray_menu_action(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    desktop_tray_set_action((InbeDesktopTrayAction)(intptr_t)user_data);
}

static GtkWidget *
desktop_tray_menu_item(const char *label, InbeDesktopTrayAction action)
{
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    g_signal_connect(item, "activate", G_CALLBACK(desktop_tray_menu_action),
                     (gpointer)(intptr_t)action);
    return item;
}

static void
desktop_tray_set_state(int state)
{
    pthread_mutex_lock(&tray_state_lock);
    tray_state = state;
    pthread_cond_signal(&tray_state_cond);
    pthread_mutex_unlock(&tray_state_lock);
}

static gboolean
desktop_tray_apply_status(gpointer user_data)
{
    char text[sizeof(tray_status_text)];

    (void)user_data;

    pthread_mutex_lock(&tray_status_lock);
    snprintf(text, sizeof(text), "%s", tray_status_text);
    tray_status_update_pending = 0;
    pthread_mutex_unlock(&tray_status_lock);

#if defined(INBE_DESKTOP_TRAY_GTK_STATUS_ICON)
    if(tray_status_icon != NULL) {
        gtk_status_icon_set_title(tray_status_icon, text);
        gtk_status_icon_set_tooltip_text(tray_status_icon, text);
    }
#endif

    return G_SOURCE_REMOVE;
}

static gboolean
desktop_tray_quit_main(gpointer user_data)
{
    (void)user_data;
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

static const char *
desktop_tray_icon_path(void)
{
    static char path[512];
    static const char *const fallback_paths[] = {
        "inbe.png",
        "packaging/linux/appimage/inbe.png",
        "packaging/snap/snap/gui/inbe.png",
        "web-assets/icons/inbe.png",
        NULL
    };
    static const char *const exe_links[] = {
#if defined(__FreeBSD__)
        "/proc/curproc/file",
#endif
#if defined(__linux__)
        "/proc/self/exe",
#endif
        NULL
    };

    for(int i = 0; exe_links[i] != NULL; i++) {
        ssize_t len = readlink(exe_links[i], path, sizeof(path) - 16);
        if(len > 0 && (size_t)len < sizeof(path) - 16) {
            char *slash;
            path[len] = '\0';
            slash = strrchr(path, '/');
            if(slash != NULL) {
                slash[1] = '\0';
                strncat(path, "inbe.png", sizeof(path) - strlen(path) - 1);
                if(g_file_test(path, G_FILE_TEST_IS_REGULAR))
                    return path;
            }
        }
    }

    for(int i = 0; fallback_paths[i] != NULL; i++) {
        if(g_file_test(fallback_paths[i], G_FILE_TEST_IS_REGULAR))
            return fallback_paths[i];
    }

    return NULL;
}

static GtkWidget *
desktop_tray_create_menu(void)
{
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *start_item;
    GtkWidget *start_menu;

    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                          desktop_tray_menu_item("Show Inner Breeze",
                                               INBE_DESKTOP_TRAY_ACTION_SHOW));

    start_item = gtk_menu_item_new_with_label("Start Practice");
    start_menu = gtk_menu_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(start_menu),
                          desktop_tray_menu_item("Wim Hof",
                                               INBE_DESKTOP_TRAY_ACTION_START_WHM));
    gtk_menu_shell_append(GTK_MENU_SHELL(start_menu),
                          desktop_tray_menu_item("Meditation",
                                               INBE_DESKTOP_TRAY_ACTION_START_MEDITATION));
    gtk_menu_shell_append(GTK_MENU_SHELL(start_menu),
                          desktop_tray_menu_item("Sun Salutation",
                                               INBE_DESKTOP_TRAY_ACTION_START_SUN_SALUTATION));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(start_item), start_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), start_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                          desktop_tray_menu_item("Quit Inner Breeze",
                                               INBE_DESKTOP_TRAY_ACTION_QUIT));

    gtk_widget_show_all(menu);
    return menu;
}

#if defined(INBE_DESKTOP_TRAY_GTK_STATUS_ICON)
static void
desktop_tray_status_icon_activate(GtkStatusIcon *status_icon, gpointer user_data)
{
    (void)status_icon;
    (void)user_data;
    desktop_tray_set_action(INBE_DESKTOP_TRAY_ACTION_SHOW);
}

static void
desktop_tray_status_icon_popup(GtkStatusIcon *status_icon, guint button,
                             guint activate_time, gpointer user_data)
{
    GtkWidget *menu = user_data;

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, gtk_status_icon_position_menu,
                   status_icon, button, activate_time);
}
#endif

static void *
desktop_tray_thread_main(void *arg)
{
#if defined(INBE_DESKTOP_TRAY_AYATANA) || defined(INBE_DESKTOP_TRAY_APPINDICATOR)
    AppIndicator *indicator;
#endif
    GtkWidget *menu;
    (void)arg;

    if(!gtk_init_check(NULL, NULL)) {
        desktop_tray_set_state(DESKTOP_TRAY_STATE_FAILED);
        return NULL;
    }

#if defined(INBE_DESKTOP_TRAY_AYATANA) || defined(INBE_DESKTOP_TRAY_APPINDICATOR)
    indicator = app_indicator_new("inbe", "inbe",
                                  APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    if(indicator == NULL) {
        desktop_tray_set_state(DESKTOP_TRAY_STATE_FAILED);
        return NULL;
    }

    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
    {
        const char *icon_path = desktop_tray_icon_path();
        if(icon_path != NULL)
            app_indicator_set_icon_full(indicator, icon_path, "Inner Breeze");
        else
            app_indicator_set_icon_full(indicator, "inbe", "Inner Breeze");
    }

    menu = desktop_tray_create_menu();
    app_indicator_set_menu(indicator, GTK_MENU(menu));
#elif defined(INBE_DESKTOP_TRAY_GTK_STATUS_ICON)
    menu = desktop_tray_create_menu();
    {
        const char *icon_path = desktop_tray_icon_path();
        tray_status_icon = icon_path != NULL
                          ? gtk_status_icon_new_from_file(icon_path)
                          : gtk_status_icon_new_from_icon_name("inbe");
    }
    if(tray_status_icon == NULL) {
        desktop_tray_set_state(DESKTOP_TRAY_STATE_FAILED);
        return NULL;
    }
    gtk_status_icon_set_title(tray_status_icon, "Inner Breeze");
    gtk_status_icon_set_tooltip_text(tray_status_icon, "Inner Breeze");
    gtk_status_icon_set_visible(tray_status_icon, TRUE);
    g_signal_connect(tray_status_icon, "activate",
                     G_CALLBACK(desktop_tray_status_icon_activate), NULL);
    g_signal_connect(tray_status_icon, "popup-menu",
                     G_CALLBACK(desktop_tray_status_icon_popup), menu);
#else
    desktop_tray_set_state(DESKTOP_TRAY_STATE_FAILED);
    return NULL;
#endif

    desktop_tray_set_state(DESKTOP_TRAY_STATE_READY);
    gtk_main();
    desktop_tray_set_state(DESKTOP_TRAY_STATE_STOPPED);

    return NULL;
}

int
inbe_desktop_tray_init(void)
{
    int ready;

    pthread_mutex_lock(&tray_state_lock);
    tray_state = DESKTOP_TRAY_STATE_STARTING;
    pthread_mutex_unlock(&tray_state_lock);
    if(pthread_create(&tray_thread, NULL, desktop_tray_thread_main, NULL) != 0)
        return 0;

    tray_started = 1;
    pthread_mutex_lock(&tray_state_lock);
    while(tray_state == DESKTOP_TRAY_STATE_STARTING)
        pthread_cond_wait(&tray_state_cond, &tray_state_lock);
    ready = tray_state == DESKTOP_TRAY_STATE_READY;
    pthread_mutex_unlock(&tray_state_lock);

    if(!ready) {
        pthread_join(tray_thread, NULL);
        tray_started = 0;
        return 0;
    }

    SDL_SetEventFilter(desktop_tray_sdl_event_filter, NULL);
    return ready;
}

void
inbe_desktop_tray_shutdown(void)
{
    int ready;

    pthread_mutex_lock(&tray_state_lock);
    ready = tray_state == DESKTOP_TRAY_STATE_READY;
    pthread_mutex_unlock(&tray_state_lock);

    if(tray_started && ready)
        g_idle_add(desktop_tray_quit_main, NULL);

    if(tray_started)
        pthread_join(tray_thread, NULL);

    tray_started = 0;
}

static void
desktop_tray_restore_window(void)
{
    ClearWindowState(FLAG_WINDOW_HIDDEN);
    RestoreWindow();
}

void
inbe_desktop_tray_keep_running(void)
{
    int ready;

    pthread_mutex_lock(&tray_state_lock);
    ready = tray_state == DESKTOP_TRAY_STATE_READY;
    pthread_mutex_unlock(&tray_state_lock);

    if(ready)
        SetWindowState(FLAG_WINDOW_HIDDEN);
    else
        MinimizeWindow();
}

static void
desktop_tray_start_practice(InbeApp *app, int practice_id)
{
    const PracticeDefinition *practice;

    if(app == NULL)
        return;

    app->exercise_type = practice_clamp_id(practice_id);
    app->main_tab = APP_MAIN_TAB_PRACTICE;
    app->practice_tab = PRACTICE_TAB_PLAY;
    if(app->modal.active)
        app_close_modal(app);
    desktop_tray_restore_window();

    practice = practice_get(app->exercise_type);
    if(practice->start != NULL)
        practice->start(app);
}

void
inbe_desktop_tray_apply_action(InbeApp *app, InbeDesktopTrayAction action, int *quit)
{
    switch(action) {
    case INBE_DESKTOP_TRAY_ACTION_SHOW:
        desktop_tray_restore_window();
        break;
    case INBE_DESKTOP_TRAY_ACTION_MINIMIZE:
        MinimizeWindow();
        break;
    case INBE_DESKTOP_TRAY_ACTION_CLOSE_REQUEST:
        app_request_desktop_close(app);
        break;
    case INBE_DESKTOP_TRAY_ACTION_START_WHM:
        desktop_tray_start_practice(app, PRACTICE_WHM);
        break;
    case INBE_DESKTOP_TRAY_ACTION_START_MEDITATION:
        desktop_tray_start_practice(app, PRACTICE_MEDITATION);
        break;
    case INBE_DESKTOP_TRAY_ACTION_START_SUN_SALUTATION:
        desktop_tray_start_practice(app, PRACTICE_SUN_SALUTATION);
        break;
    case INBE_DESKTOP_TRAY_ACTION_QUIT:
        if(quit != NULL)
            *quit = 1;
        break;
    case INBE_DESKTOP_TRAY_ACTION_NONE:
    default:
        break;
    }
}

void
inbe_desktop_tray_update_status(InbeApp *app)
{
    char text[sizeof(tray_status_text)];
    int ready;

    if(app == NULL)
        return;

    pthread_mutex_lock(&tray_state_lock);
    ready = tray_state == DESKTOP_TRAY_STATE_READY;
    pthread_mutex_unlock(&tray_state_lock);
    if(!ready)
        return;

    snprintf(text, sizeof(text), "Inner Breeze");
    if(app->inbe.screen == InbeScreenSession) {
        int count = int_from_count(app->inbe.count);
        int max_breaths = int_from_count(app->inbe.maxbreaths);

        if(app->session_paused) {
            snprintf(text, sizeof(text), "Wim Hof - Paused");
        } else {
            switch(app->inbe.phase) {
            case InbePhaseBreathe:
                snprintf(text, sizeof(text), "Wim Hof - Breath %d/%d",
                         count, max_breaths);
                break;
            case InbePhaseHold:
                snprintf(text, sizeof(text), "Wim Hof - Hold %ds", count);
                break;
            case InbePhaseRecover:
                snprintf(text, sizeof(text), "Wim Hof - Breathe in %ds",
                         count);
                break;
            case InbePhaseNext:
                snprintf(text, sizeof(text), "Wim Hof - Next round");
                break;
            case InbePhaseStarting:
            default:
                snprintf(text, sizeof(text), "Wim Hof - Starting");
                break;
            }
        }
    } else if(app->inbe.screen == InbeScreenMeditation) {
        int remaining = app->meditation.remaining_seconds;
        if(remaining < 0)
            remaining = 0;
        snprintf(text, sizeof(text), "Meditation - %d:%02d%s",
                 remaining / 60, remaining % 60,
                 app->session_paused ? " paused" : " left");
    } else if(app->inbe.screen == InbeScreenSunSalutation) {
        snprintf(text, sizeof(text), "Sun Salutation - Step %d, Rep %d/%d%s",
                 app->sun_salutation.step + 1,
                 app->sun_salutation.repetition + 1,
                 app->sun_salutation.repetitions,
                 app->session_paused ? " paused" : "");
    }

    pthread_mutex_lock(&tray_status_lock);
    if(strcmp(tray_status_text, text) != 0) {
        snprintf(tray_status_text, sizeof(tray_status_text), "%s", text);
        if(!tray_status_update_pending) {
            tray_status_update_pending = 1;
            g_idle_add(desktop_tray_apply_status, NULL);
        }
    }
    pthread_mutex_unlock(&tray_status_lock);
}

#else

int
inbe_desktop_tray_init(void)
{
    return 0;
}

void
inbe_desktop_tray_shutdown(void)
{
}

InbeDesktopTrayAction
inbe_desktop_tray_poll_action(void)
{
    return INBE_DESKTOP_TRAY_ACTION_NONE;
}

void
inbe_desktop_tray_apply_action(InbeApp *app, InbeDesktopTrayAction action, int *quit)
{
    (void)app;
    (void)action;
    (void)quit;
}

void
inbe_desktop_tray_update_status(InbeApp *app)
{
    (void)app;
}

void
inbe_desktop_tray_keep_running(void)
{
}

#endif
