#include "android_timer.h"
#include "app.h"
#include "practices/practice_registry.h"
#include <time.h>

#include <raylib.h>
#include <pthread.h>
#include <android/log.h>
#include <unistd.h>

#define LOG_TAG "INBE_TIMER"
#define TARGET_FPS 60
#define FRAME_TIME_US (1000000 / TARGET_FPS) // ~16666 microseconds for 60fps

static pthread_t timer_thread;
static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t inbe_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static int timer_running = 0;
static int timer_active = 0; // Only step when timer is active (activity paused)
static void *g_app = NULL;

static void*
timer_thread_func(void *arg) {
    (void)arg;
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Timer thread started");
    int frame_count = 0;

    while (1) {
        pthread_mutex_lock(&timer_mutex);

        // Check if we should stop
        if (!timer_running) {
            pthread_mutex_unlock(&timer_mutex);
            __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Timer thread: stopping");
            break;
        }

        // Check if we should run (active and have app pointer)
        int should_run = timer_active && g_app != NULL;
        pthread_mutex_unlock(&timer_mutex);

        if (should_run) {
            // Log every 60 frames (1 second)
            if (frame_count % 60 == 0) {
                __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Timer: Running background practice step + sounds");
            }
            frame_count++;

            // Update practice state and sounds with mutex protection
            pthread_mutex_lock(&inbe_state_mutex);
            InbeApp *app = (InbeApp*)g_app;
            inbestep(&app->inbe);
            practice_update_session_sounds(app);
            pthread_mutex_unlock(&inbe_state_mutex);

            // Sleep for exact frame time (60fps = ~16.666ms)
            usleep(FRAME_TIME_US);
        } else {
            // Not active, sleep a bit before checking again
            if (frame_count > 0) {
                __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Timer: Paused (inactive)");
                frame_count = 0;
            }
            usleep(50000); // 50ms
        }
    }

    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Timer thread stopped");
    return NULL;
}

void
android_timer_init(void) {
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "=== android_timer_init ===");
}

void
android_timer_set_app(void *app_ptr) {
    pthread_mutex_lock(&timer_mutex);
    g_app = app_ptr;
    pthread_mutex_unlock(&timer_mutex);
    TraceLog(LOG_INFO, "INBE: Timer app pointer set");
}

pthread_mutex_t*
android_timer_get_mutex(void) {
    return &inbe_state_mutex;
}

void
android_timer_start(void) {
    pthread_mutex_lock(&timer_mutex);

    if (timer_running) {
        timer_active = 1; // Activate timer (activity paused)
        pthread_mutex_unlock(&timer_mutex);
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Timer activated (activity paused)");
        return;
    }

    timer_running = 1;
    timer_active = 0; // Start inactive (activity active)
    pthread_mutex_unlock(&timer_mutex);

    int result = pthread_create(&timer_thread, NULL, timer_thread_func, NULL);
    if (result != 0) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "Failed to create timer thread");
    } else {
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Timer thread created");
    }

    TraceLog(LOG_INFO, "INBE: Background timer started");
}

void
android_timer_stop(void) {
    pthread_mutex_lock(&timer_mutex);

    if (!timer_running) {
        pthread_mutex_unlock(&timer_mutex);
        return;
    }

    timer_running = 0;
    timer_active = 0;
    pthread_mutex_unlock(&timer_mutex);

    pthread_join(timer_thread, NULL);
    TraceLog(LOG_INFO, "INBE: Background timer stopped");
}

void
android_timer_activate(void) {
    pthread_mutex_lock(&timer_mutex);
    timer_active = 1;
    pthread_mutex_unlock(&timer_mutex);
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "ACTIVATE: Timer active");
    TraceLog(LOG_INFO, "INBE: Background timer ACTIVATED - screen off");
}

void
android_timer_deactivate(void) {
    pthread_mutex_lock(&timer_mutex);
    timer_active = 0;
    pthread_mutex_unlock(&timer_mutex);
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "DEACTIVATE: Timer inactive");
    TraceLog(LOG_INFO, "INBE: Background timer DEACTIVATED - screen on");
}

void*
android_timer_get_app(void)
{
    void *app;
    pthread_mutex_lock(&timer_mutex);
    app = g_app;
    pthread_mutex_unlock(&timer_mutex);
    return app;
}
