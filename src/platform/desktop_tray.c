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
#include <string.h>
#include <unistd.h>

static pthread_t tray_thread;
static pthread_mutex_t tray_action_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t tray_state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t tray_state_cond = PTHREAD_COND_INITIALIZER;
static InbeDesktopTrayAction pending_action = INBE_DESKTOP_TRAY_ACTION_NONE;
static int tray_started;
static int tray_state;

enum {
    LINUX_TRAY_STATE_STARTING = 0,
    LINUX_TRAY_STATE_READY,
    LINUX_TRAY_STATE_FAILED,
    LINUX_TRAY_STATE_STOPPED
};

static void
linux_tray_set_action(InbeDesktopTrayAction action)
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
linux_tray_sdl_event_filter(void *userdata, SDL_Event *event)
{
    (void)userdata;

    if(event != NULL && event->type == SDL_QUIT) {
        linux_tray_set_action(INBE_DESKTOP_TRAY_ACTION_MINIMIZE);
        return 0;
    }

    return 1;
}

static void
linux_tray_menu_action(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    linux_tray_set_action((InbeDesktopTrayAction)(intptr_t)user_data);
}

static GtkWidget *
linux_tray_menu_item(const char *label, InbeDesktopTrayAction action)
{
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    g_signal_connect(item, "activate", G_CALLBACK(linux_tray_menu_action),
                     (gpointer)(intptr_t)action);
    return item;
}

static void
linux_tray_set_state(int state)
{
    pthread_mutex_lock(&tray_state_lock);
    tray_state = state;
    pthread_cond_signal(&tray_state_cond);
    pthread_mutex_unlock(&tray_state_lock);
}

static gboolean
linux_tray_quit_main(gpointer user_data)
{
    (void)user_data;
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

static const char *
linux_tray_icon_path(void)
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
linux_tray_create_menu(void)
{
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *start_item;
    GtkWidget *start_menu;

    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                          linux_tray_menu_item("Show Inner Breeze",
                                               INBE_DESKTOP_TRAY_ACTION_SHOW));

    start_item = gtk_menu_item_new_with_label("Start Practice");
    start_menu = gtk_menu_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(start_menu),
                          linux_tray_menu_item("Wim Hof",
                                               INBE_DESKTOP_TRAY_ACTION_START_WHM));
    gtk_menu_shell_append(GTK_MENU_SHELL(start_menu),
                          linux_tray_menu_item("Meditation",
                                               INBE_DESKTOP_TRAY_ACTION_START_MEDITATION));
    gtk_menu_shell_append(GTK_MENU_SHELL(start_menu),
                          linux_tray_menu_item("Sun Salutation",
                                               INBE_DESKTOP_TRAY_ACTION_START_SUN_SALUTATION));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(start_item), start_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), start_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                          linux_tray_menu_item("Quit Inner Breeze",
                                               INBE_DESKTOP_TRAY_ACTION_QUIT));

    gtk_widget_show_all(menu);
    return menu;
}

#if defined(INBE_DESKTOP_TRAY_GTK_STATUS_ICON)
static void
linux_tray_status_icon_activate(GtkStatusIcon *status_icon, gpointer user_data)
{
    (void)status_icon;
    (void)user_data;
    linux_tray_set_action(INBE_DESKTOP_TRAY_ACTION_SHOW);
}

static void
linux_tray_status_icon_popup(GtkStatusIcon *status_icon, guint button,
                             guint activate_time, gpointer user_data)
{
    GtkWidget *menu = user_data;

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, gtk_status_icon_position_menu,
                   status_icon, button, activate_time);
}
#endif

static void *
linux_tray_thread_main(void *arg)
{
#if defined(INBE_DESKTOP_TRAY_AYATANA) || defined(INBE_DESKTOP_TRAY_APPINDICATOR)
    AppIndicator *indicator;
#endif
    GtkWidget *menu;
#if defined(INBE_DESKTOP_TRAY_GTK_STATUS_ICON)
    GtkStatusIcon *status_icon;
#endif

    (void)arg;

    if(!gtk_init_check(NULL, NULL)) {
        linux_tray_set_state(LINUX_TRAY_STATE_FAILED);
        return NULL;
    }

#if defined(INBE_DESKTOP_TRAY_AYATANA) || defined(INBE_DESKTOP_TRAY_APPINDICATOR)
    indicator = app_indicator_new("inbe", "inbe",
                                  APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    if(indicator == NULL) {
        linux_tray_set_state(LINUX_TRAY_STATE_FAILED);
        return NULL;
    }

    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
    {
        const char *icon_path = linux_tray_icon_path();
        if(icon_path != NULL)
            app_indicator_set_icon_full(indicator, icon_path, "Inner Breeze");
        else
            app_indicator_set_icon_full(indicator, "inbe", "Inner Breeze");
    }

    menu = linux_tray_create_menu();
    app_indicator_set_menu(indicator, GTK_MENU(menu));
#elif defined(INBE_DESKTOP_TRAY_GTK_STATUS_ICON)
    menu = linux_tray_create_menu();
    {
        const char *icon_path = linux_tray_icon_path();
        status_icon = icon_path != NULL
                          ? gtk_status_icon_new_from_file(icon_path)
                          : gtk_status_icon_new_from_icon_name("inbe");
    }
    if(status_icon == NULL) {
        linux_tray_set_state(LINUX_TRAY_STATE_FAILED);
        return NULL;
    }
    gtk_status_icon_set_title(status_icon, "Inner Breeze");
    gtk_status_icon_set_tooltip_text(status_icon, "Inner Breeze");
    gtk_status_icon_set_visible(status_icon, TRUE);
    g_signal_connect(status_icon, "activate",
                     G_CALLBACK(linux_tray_status_icon_activate), NULL);
    g_signal_connect(status_icon, "popup-menu",
                     G_CALLBACK(linux_tray_status_icon_popup), menu);
#else
    linux_tray_set_state(LINUX_TRAY_STATE_FAILED);
    return NULL;
#endif

    linux_tray_set_state(LINUX_TRAY_STATE_READY);
    gtk_main();
    linux_tray_set_state(LINUX_TRAY_STATE_STOPPED);

    return NULL;
}

int
inbe_desktop_tray_init(void)
{
    int ready;

    pthread_mutex_lock(&tray_state_lock);
    tray_state = LINUX_TRAY_STATE_STARTING;
    pthread_mutex_unlock(&tray_state_lock);
    if(pthread_create(&tray_thread, NULL, linux_tray_thread_main, NULL) != 0)
        return 0;

    tray_started = 1;
    pthread_mutex_lock(&tray_state_lock);
    while(tray_state == LINUX_TRAY_STATE_STARTING)
        pthread_cond_wait(&tray_state_cond, &tray_state_lock);
    ready = tray_state == LINUX_TRAY_STATE_READY;
    pthread_mutex_unlock(&tray_state_lock);

    if(!ready) {
        pthread_join(tray_thread, NULL);
        tray_started = 0;
        return 0;
    }

    SDL_SetEventFilter(linux_tray_sdl_event_filter, NULL);
    return ready;
}

void
inbe_desktop_tray_shutdown(void)
{
    int ready;

    pthread_mutex_lock(&tray_state_lock);
    ready = tray_state == LINUX_TRAY_STATE_READY;
    pthread_mutex_unlock(&tray_state_lock);

    if(tray_started && ready)
        g_idle_add(linux_tray_quit_main, NULL);

    if(tray_started)
        pthread_join(tray_thread, NULL);

    tray_started = 0;
}

static void
linux_tray_restore_window(void)
{
    ClearWindowState(FLAG_WINDOW_HIDDEN);
    RestoreWindow();
}

static void
linux_tray_start_practice(InbeApp *app, int practice_id)
{
    const PracticeDefinition *practice;

    if(app == NULL)
        return;

    app->exercise_type = practice_clamp_id(practice_id);
    app->main_tab = APP_MAIN_TAB_PRACTICE;
    app->practice_tab = PRACTICE_TAB_PLAY;
    if(app->modal.active)
        app_close_modal(app);
    linux_tray_restore_window();

    practice = practice_get(app->exercise_type);
    if(practice->start != NULL)
        practice->start(app);
}

void
inbe_desktop_tray_apply_action(InbeApp *app, InbeDesktopTrayAction action, int *quit)
{
    switch(action) {
    case INBE_DESKTOP_TRAY_ACTION_SHOW:
        linux_tray_restore_window();
        break;
    case INBE_DESKTOP_TRAY_ACTION_MINIMIZE:
        MinimizeWindow();
        break;
    case INBE_DESKTOP_TRAY_ACTION_START_WHM:
        linux_tray_start_practice(app, PRACTICE_WHM);
        break;
    case INBE_DESKTOP_TRAY_ACTION_START_MEDITATION:
        linux_tray_start_practice(app, PRACTICE_MEDITATION);
        break;
    case INBE_DESKTOP_TRAY_ACTION_START_SUN_SALUTATION:
        linux_tray_start_practice(app, PRACTICE_SUN_SALUTATION);
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

#endif
