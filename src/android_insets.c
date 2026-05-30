#include "android_insets.h"
#include <string.h>

#ifdef __ANDROID__
#include <raylib.h>
#include <pthread.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <android/log.h>

#define LOG_TAG "INBE_INSETS"

#ifndef JNI_VERSION_1_6
#define JNI_VERSION_1_6 0x10060000
#endif

extern struct android_app *GetAndroidApp(void);
extern void android_wakelock_set_activity(JNIEnv *env, jobject activity);
extern void android_wakelock_set_jvm(JavaVM *vm);
extern void android_timer_activate(void);
extern void android_timer_deactivate(void);

// Called from Java when activity is ready for wake lock
static void android_wakelock_set_activity_impl(JNIEnv *env, jobject thiz) {
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Wake lock ready callback from Java");
    android_wakelock_set_activity(env, thiz);
}

// Called from Java when activity is paused
static void android_timer_activate_impl(JNIEnv *env, jobject thiz) {
    (void)env;
    (void)thiz;
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "JNI: Timer ACTIVATE callback");
    android_timer_activate();
}

// Called from Java when activity is resumed
static void android_timer_deactivate_impl(JNIEnv *env, jobject thiz) {
    (void)env;
    (void)thiz;
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "JNI: Timer DEACTIVATE callback");
    android_timer_deactivate();
}

static JavaVM *g_jvm = NULL;
static jobject g_activity = NULL;

static volatile struct {
    int status_bar;
    int nav_bar;
    int cutout_left;
    int cutout_top;
    int cutout_right;
    int cutout_bottom;
} current_insets = {0};
static pthread_mutex_t insets_mutex = PTHREAD_MUTEX_INITIALIZER;

// JNI function - Java calls this to push inset values
static void nativeSetInsets(JNIEnv *env, jobject thiz,
    jint status_bar, jint nav_bar,
    jint cutout_left, jint cutout_top, jint cutout_right, jint cutout_bottom)
{
    TraceLog(LOG_INFO, "INBE: Java insets: status=%d, nav=%d, cutout_top=%d",
             status_bar, nav_bar, cutout_top);

    pthread_mutex_lock(&insets_mutex);
    current_insets.status_bar = status_bar;
    current_insets.nav_bar = nav_bar;
    current_insets.cutout_left = cutout_left;
    current_insets.cutout_top = cutout_top;
    current_insets.cutout_right = cutout_right;
    current_insets.cutout_bottom = cutout_bottom;
    pthread_mutex_unlock(&insets_mutex);

    // Memory barrier to ensure all threads see the updated values
    __sync_synchronize();
}

// JNI method table
static const JNINativeMethod g_methods[] = {
    {"nativeSetInsets", "(IIIIII)V", (void*)nativeSetInsets},
    {"nativeWakeLockReady", "()V", (void*)android_wakelock_set_activity_impl},
    {"nativeTimerActivate", "()V", (void*)android_timer_activate_impl},
    {"nativeTimerDeactivate", "()V", (void*)android_timer_deactivate_impl},
};

// JNI_OnLoad - Register native methods
JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env = NULL;
    jint result;

    g_jvm = vm;

    // Also set JVM for wake lock module
    android_wakelock_set_jvm(vm);

    result = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);
    if (result != JNI_OK) {
        TraceLog(LOG_ERROR, "INBE: Failed to get JNI environment");
        return result;
    }

    // Find the MainActivity class
    jclass clazz = (*env)->FindClass(env, "xyz/waozi/inbe/MainActivity");
    if (clazz == NULL) {
        TraceLog(LOG_ERROR, "INBE: Failed to find MainActivity class");
        return JNI_ERR;
    }

    // Register native methods
    result = (*env)->RegisterNatives(env, clazz, g_methods, sizeof(g_methods) / sizeof(g_methods[0]));
    if (result != JNI_OK) {
        TraceLog(LOG_ERROR, "INBE: Failed to register native methods");
        return result;
    }

    return JNI_VERSION_1_6;
}

void android_insets_init(void) {
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "=== android_insets_init ===");
    pthread_mutex_lock(&insets_mutex);
    memset((void *)&current_insets, 0, sizeof(current_insets));
    pthread_mutex_unlock(&insets_mutex);
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "=== android_insets_init DONE ===");
}

void android_insets_get(AndroidInsets *out) {
    pthread_mutex_lock(&insets_mutex);

    // Simply return the current values
    if (out) {
        out->status_bar = current_insets.status_bar;
        out->nav_bar = current_insets.nav_bar;
        out->cutout_left = current_insets.cutout_left;
        out->cutout_top = current_insets.cutout_top;
        out->cutout_right = current_insets.cutout_right;
        out->cutout_bottom = current_insets.cutout_bottom;
    }

    pthread_mutex_unlock(&insets_mutex);
}
#endif
