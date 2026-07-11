#include "android_wakelock.h"

#include "flint.h"
#include <pthread.h>
#include <android/log.h>
#include <jni.h>

#define LOG_TAG "INBE_WAKE"

static JavaVM *g_jvm = NULL;
static jobject g_activity = NULL;
static jmethodID g_acquire_method = NULL;
static jmethodID g_release_method = NULL;
static jmethodID g_update_notification_method = NULL;
static jmethodID g_keep_screen_on_method = NULL;
static jmethodID g_allow_screen_off_method = NULL;
static pthread_mutex_t wakelock_mutex = PTHREAD_MUTEX_INITIALIZER;

void android_wakelock_init(void) {
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
            g_update_notification_method =
                (*env)->GetMethodID(env, clazz, "updateSessionNotification", "(Ljava/lang/String;)V");
            g_keep_screen_on_method = (*env)->GetMethodID(env, clazz, "keepScreenOn", "()V");
            g_allow_screen_off_method = (*env)->GetMethodID(env, clazz, "allowScreenOff", "()V");
            (*env)->DeleteLocalRef(env, clazz);
        }
    }

    pthread_mutex_unlock(&wakelock_mutex);
}

void android_wakelock_acquire(void) {
    pthread_mutex_lock(&wakelock_mutex);

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
    }

    pthread_mutex_unlock(&wakelock_mutex);
}

void android_wakelock_update_session_notification(const char *status_text) {
    if(status_text == NULL)
        return;

    pthread_mutex_lock(&wakelock_mutex);

    if (!g_jvm || !g_activity) {
        pthread_mutex_unlock(&wakelock_mutex);
        return;
    }

    JNIEnv *env = NULL;
    jint result = (*g_jvm)->GetEnv(g_jvm, (void**)&env, JNI_VERSION_1_6);
    if (result == JNI_EDETACHED) {
        result = (*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL);
    }

    if (env && g_update_notification_method) {
        jstring text = (*env)->NewStringUTF(env, status_text);
        if(text != NULL) {
            (*env)->CallVoidMethod(env, g_activity, g_update_notification_method, text);
            (*env)->DeleteLocalRef(env, text);
        }
    }

    pthread_mutex_unlock(&wakelock_mutex);
}

void android_wakelock_release(void) {
    pthread_mutex_lock(&wakelock_mutex);

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
    }

    pthread_mutex_unlock(&wakelock_mutex);
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
    }

    pthread_mutex_unlock(&wakelock_mutex);
}

void android_wakelock_set_jvm(JavaVM *vm) {
    g_jvm = vm;
}
