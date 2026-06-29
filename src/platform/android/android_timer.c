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
#define FRAME_TIME_US (1000000 / TARGET_FPS)

static pthread_t timer_thread;
static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t inbe_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static int timer_running = 0;
static int timer_active = 0; // Only step when timer is active (activity paused)
static void *g_app = NULL;

static int
monotonic_elapsed_ms(struct timespec *last_tick)
{
    struct timespec now;
    long long elapsed_ms;

    clock_gettime(CLOCK_MONOTONIC, &now);
    if(last_tick->tv_sec == 0 && last_tick->tv_nsec == 0) {
        *last_tick = now;
        return FRAME_TIME_US / 1000;
    }

    elapsed_ms = (long long)(now.tv_sec - last_tick->tv_sec) * 1000LL +
                 (now.tv_nsec - last_tick->tv_nsec) / 1000000LL;
    *last_tick = now;
    if(elapsed_ms <= 0)
        return FRAME_TIME_US / 1000;
    if(elapsed_ms > 5 * 60 * 1000)
        return 5 * 60 * 1000;
    return (int)elapsed_ms;
}

static void*
timer_thread_func(void *arg) {
    (void)arg;
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Timer thread started");
    int frame_count = 0;
    struct timespec last_tick = {0};

    while (1) {
        pthread_mutex_lock(&timer_mutex);

        if (!timer_running) {
            pthread_mutex_unlock(&timer_mutex);
            __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Timer thread: stopping");
            break;
        }

        int should_run = timer_active && g_app != NULL;
        pthread_mutex_unlock(&timer_mutex);

        if (should_run) {
            frame_count++;

            pthread_mutex_lock(&inbe_state_mutex);
            InbeApp *app = (InbeApp*)g_app;
            practice_active_advance_elapsed(app, monotonic_elapsed_ms(&last_tick));
            pthread_mutex_unlock(&inbe_state_mutex);

            usleep(FRAME_TIME_US);
        } else {
            if (frame_count > 0) {
                __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Timer: Paused (inactive)");
                frame_count = 0;
            }
            last_tick = (struct timespec){0};
            usleep(50000);
        }
    }

    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Timer thread stopped");
    return NULL;
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
        timer_active = 1;
        pthread_mutex_unlock(&timer_mutex);
        return;
    }

    timer_running = 1;
    timer_active = 0;
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

    if(pthread_equal(pthread_self(), timer_thread))
        pthread_detach(timer_thread);
    else
        pthread_join(timer_thread, NULL);
    TraceLog(LOG_INFO, "INBE: Background timer stopped");
}

void
android_timer_activate(void) {
    pthread_mutex_lock(&timer_mutex);
    timer_active = 1;
    pthread_mutex_unlock(&timer_mutex);
    TraceLog(LOG_INFO, "INBE: Background timer ACTIVATED - screen off");
}

void
android_timer_deactivate(void) {
    pthread_mutex_lock(&timer_mutex);
    timer_active = 0;
    pthread_mutex_unlock(&timer_mutex);
    TraceLog(LOG_INFO, "INBE: Background timer DEACTIVATED - screen on");
}
