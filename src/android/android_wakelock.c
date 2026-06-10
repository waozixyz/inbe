#include "android_wakelock.h"
#include <stdio.h>
#include <string.h>

#include <raylib.h>
#include <pthread.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>

#define LOG_TAG "INBE_WAKE"

static JavaVM *g_jvm = NULL;
static jobject g_activity = NULL;
static jmethodID g_acquire_method = NULL;
static jmethodID g_release_method = NULL;
static jmethodID g_keep_screen_on_method = NULL;
static jmethodID g_allow_screen_off_method = NULL;
static pthread_mutex_t wakelock_mutex = PTHREAD_MUTEX_INITIALIZER;

void android_wakelock_init(void) {
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "=== android_wakelock_init ===");
}

void android_wakelock_set_activity(JNIEnv *env, jobject activity) {
    pthread_mutex_lock(&wakelock_mutex);

    if (g_activity) {
        (*env)->DeleteGlobalRef(env, g_activity);
    }

    g_activity = (*env)->NewGlobalRef(env, activity);

    if (g_activity) {
        jclass clazz = (*env)->GetObjectClass(env, activity);
        if (clazz) {
            g_acquire_method = (*env)->GetMethodID(env, clazz, "acquireWakeLock", "()V");
            g_release_method = (*env)->GetMethodID(env, clazz, "releaseWakeLock", "()V");
            g_keep_screen_on_method = (*env)->GetMethodID(env, clazz, "keepScreenOn", "()V");
            g_allow_screen_off_method = (*env)->GetMethodID(env, clazz, "allowScreenOff", "()V");
            (*env)->DeleteLocalRef(env, clazz);
            __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "Wake lock activity set successfully");
        }
    }

    pthread_mutex_unlock(&wakelock_mutex);
}

void android_wakelock_acquire(void) {
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "=== ACQUIRE START ===");

    pthread_mutex_lock(&wakelock_mutex);

    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "ACQUIRE: Checking if g_jvm and g_activity are set");
    char msg[128];
    snprintf(msg, sizeof(msg), "ACQUIRE: g_jvm=%p, g_activity=%p", g_jvm, g_activity);
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, msg);

    if (!g_jvm || !g_activity) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "ACQUIRE: JNI not initialized - skipping wake lock");
        pthread_mutex_unlock(&wakelock_mutex);
        return;
    }

    JNIEnv *env = NULL;
    jint result = (*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        result = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
    }

    if (env && g_acquire_method) {
        (*env)->CallVoidMethod(env, g_activity, g_acquire_method);
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "ACQUIRE: SUCCESS");
    }

    pthread_mutex_unlock(&wakelock_mutex);
    TraceLog(LOG_INFO, "INBE: Wake lock acquired");
}

void android_wakelock_release(void) {
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "=== RELEASE START ===");

    pthread_mutex_lock(&wakelock_mutex);

    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "RELEASE: Checking if g_jvm and g_activity are set");
    char msg[128];
    snprintf(msg, sizeof(msg), "RELEASE: g_jvm=%p, g_activity=%p", g_jvm, g_activity);
    __android_log_write(ANDROID_LOG_INFO, LOG_TAG, msg);

    if (!g_jvm || !g_activity) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "RELEASE: JNI not initialized - skipping wake lock");
        pthread_mutex_unlock(&wakelock_mutex);
        return;
    }

    JNIEnv *env = NULL;
    jint result = (*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        result = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
    }

    if (env && g_release_method) {
        (*env)->CallVoidMethod(env, g_activity, g_release_method);
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "RELEASE: SUCCESS");
    }

    pthread_mutex_unlock(&wakelock_mutex);
    TraceLog(LOG_INFO, "INBE: Wake lock released");
}

void android_keep_screen_on(void) {
    pthread_mutex_lock(&wakelock_mutex);

    if (!g_jvm || !g_activity) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "KEEP_SCREEN_ON: JNI not initialized - skipping");
        pthread_mutex_unlock(&wakelock_mutex);
        return;
    }

    JNIEnv *env = NULL;
    jint result = (*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        result = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
    }

    if (env && g_keep_screen_on_method) {
        (*env)->CallVoidMethod(env, g_activity, g_keep_screen_on_method);
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "KEEP_SCREEN_ON: SUCCESS");
    }

    pthread_mutex_unlock(&wakelock_mutex);
}

void android_allow_screen_off(void) {
    pthread_mutex_lock(&wakelock_mutex);

    if (!g_jvm || !g_activity) {
        __android_log_write(ANDROID_LOG_ERROR, LOG_TAG, "ALLOW_SCREEN_OFF: JNI not initialized - skipping");
        pthread_mutex_unlock(&wakelock_mutex);
        return;
    }

    JNIEnv *env = NULL;
    jint result = (*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        result = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
    }

    if (env && g_allow_screen_off_method) {
        (*env)->CallVoidMethod(env, g_activity, g_allow_screen_off_method);
        __android_log_write(ANDROID_LOG_INFO, LOG_TAG, "ALLOW_SCREEN_OFF: SUCCESS");
    }

    pthread_mutex_unlock(&wakelock_mutex);
}

// Called from JNI to set the JVM reference
void android_wakelock_set_jvm(JavaVM *vm) {
    g_jvm = vm;
}
