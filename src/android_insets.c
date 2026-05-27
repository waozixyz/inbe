#include "android_insets.h"
#include <string.h>

#ifdef __ANDROID__
#include <raylib.h>
#include <pthread.h>
#include <android_native_app_glue.h>

extern struct android_app *GetAndroidApp(void);

static volatile struct {
    int status_bar;
    int nav_bar;
    int cutout_left;
    int cutout_top;
    int cutout_right;
    int cutout_bottom;
} current_insets = {0};
static pthread_mutex_t insets_mutex = PTHREAD_MUTEX_INITIALIZER;
static int insets_initialized = 0;

static void query_insets_ndk(void) {
    if (insets_initialized) return;

    struct android_app *app = GetAndroidApp();
    if (!app || !app->window) {
        insets_initialized = 0;
        return;
    }
    insets_initialized = 1;

    int density = AConfiguration_getDensity(app->config);

    /* Status bar: ~24dp, Navbar: ~48dp */
    int status_height_px = (24 * density + 160) / 160;
    int nav_height_px = (48 * density + 160) / 160;

    pthread_mutex_lock(&insets_mutex);
    current_insets.status_bar = status_height_px;
    current_insets.nav_bar = nav_height_px;
    current_insets.cutout_left = 0;
    current_insets.cutout_top = 0;
    current_insets.cutout_right = 0;
    current_insets.cutout_bottom = 0;
    pthread_mutex_unlock(&insets_mutex);
}

void android_insets_init(void) {
    pthread_mutex_lock(&insets_mutex);
    memset((void *)&current_insets, 0, sizeof(current_insets));
    pthread_mutex_unlock(&insets_mutex);
    insets_initialized = 0;
}

void android_insets_get(AndroidInsets *out) {
    query_insets_ndk();

    if (out) {
        pthread_mutex_lock(&insets_mutex);
        out->status_bar = current_insets.status_bar;
        out->nav_bar = current_insets.nav_bar;
        out->cutout_left = current_insets.cutout_left;
        out->cutout_top = current_insets.cutout_top;
        out->cutout_right = current_insets.cutout_right;
        out->cutout_bottom = current_insets.cutout_bottom;
        pthread_mutex_unlock(&insets_mutex);
    }
}
#endif
