#include "android_insets.h"
#include <string.h>

#ifdef __ANDROID__
#include <raylib.h>
#include <pthread.h>
#include <android_native_app_glue.h>
#include <jni.h>

#ifndef JNI_VERSION_1_6
#define JNI_VERSION_1_6 0x10060000
#endif

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
static int fallback_used = 0;

static void query_insets_jni(void) {
    struct android_app *app = GetAndroidApp();
    if (!app || !app->activity || !app->window) {
        TraceLog(LOG_WARNING, "INBE: JNI query failed - no app/window");
        return;
    }

    JavaVM *vm = app->activity->vm;
    if (!vm) {
        TraceLog(LOG_WARNING, "INBE: JNI query failed - no VM");
        return;
    }

    JNIEnv *env = NULL;
    jint result = (*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6);

    if (result == JNI_EDETACHED) {
        // Thread not attached, attach it
        result = (*vm)->AttachCurrentThread(vm, &env, NULL);
        if (result != JNI_OK) {
            TraceLog(LOG_WARNING, "INBE: Failed to attach thread to JVM");
            return;
        }
        TraceLog(LOG_INFO, "INBE: Attached native thread to JVM");
    } else if (result != JNI_OK) {
        TraceLog(LOG_WARNING, "INBE: Failed to get JNI environment");
        return;
    }

    // Check for pending exceptions after GetEnv/Attach
    if ((*env)->ExceptionCheck(env)) {
        TraceLog(LOG_ERROR, "INBE: Exception after GetEnv/Attach");
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        if (result == JNI_EDETACHED) {
            (*vm)->DetachCurrentThread(vm);
        }
        return;
    }

    jobject activity = app->activity->clazz;
    if (!env || !activity) {
        TraceLog(LOG_WARNING, "INBE: JNI query failed - no env or activity");
        if (result == JNI_EDETACHED) {
            (*vm)->DetachCurrentThread(vm);
        }
        return;
    }

    // Get the Java class
    jclass activity_class = (*env)->GetObjectClass(env, activity);

    // Check for exception after GetObjectClass
    if ((*env)->ExceptionCheck(env)) {
        TraceLog(LOG_ERROR, "INBE: Exception after GetObjectClass");
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        if (result == JNI_EDETACHED) {
            (*vm)->DetachCurrentThread(vm);
        }
        return;
    }

    if (!activity_class) {
        TraceLog(LOG_WARNING, "INBE: Failed to get activity class");
        if (result == JNI_EDETACHED) {
            (*vm)->DetachCurrentThread(vm);
        }
        return;
    }

    // Get the method ID for getInsetsNative
    jmethodID method_id = (*env)->GetMethodID(env, activity_class, "getInsetsNative", "[I");

    // Check for exception after GetMethodID
    if ((*env)->ExceptionCheck(env)) {
        TraceLog(LOG_ERROR, "INBE: Exception after GetMethodID - method not found?");
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        (*env)->DeleteLocalRef(env, activity_class);
        if (result == JNI_EDETACHED) {
            (*vm)->DetachCurrentThread(vm);
        }
        return;
    }

    if (!method_id) {
        TraceLog(LOG_WARNING, "INBE: Failed to get getInsetsNative method ID");
        (*env)->DeleteLocalRef(env, activity_class);
        if (result == JNI_EDETACHED) {
            (*vm)->DetachCurrentThread(vm);
        }
        return;
    }

    TraceLog(LOG_INFO, "INBE: Found getInsetsNative method, calling it...");

    // Call the method
    jintArray insets_array = (jintArray)(*env)->CallObjectMethod(env, activity, method_id);

    // Check for exception after method call
    if ((*env)->ExceptionCheck(env)) {
        TraceLog(LOG_ERROR, "INBE: Exception after calling getInsetsNative");
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        (*env)->DeleteLocalRef(env, activity_class);
        if (result == JNI_EDETACHED) {
            (*vm)->DetachCurrentThread(vm);
        }
        return;
    }

    if (!insets_array) {
        TraceLog(LOG_WARNING, "INBE: getInsetsNative returned null");
        (*env)->DeleteLocalRef(env, activity_class);
        if (result == JNI_EDETACHED) {
            (*vm)->DetachCurrentThread(vm);
        }
        return;
    }

    // Get the array elements
    jsize length = (*env)->GetArrayLength(env, insets_array);
    if (length >= 6) {
        jint *elements = (*env)->GetIntArrayElements(env, insets_array, NULL);
        if (elements) {
            pthread_mutex_lock(&insets_mutex);
            current_insets.status_bar = elements[0];
            current_insets.nav_bar = elements[1];
            current_insets.cutout_left = elements[2];
            current_insets.cutout_top = elements[3];
            current_insets.cutout_right = elements[4];
            current_insets.cutout_bottom = elements[5];
            pthread_mutex_unlock(&insets_mutex);

            (*env)->ReleaseIntArrayElements(env, insets_array, elements, 0);
            insets_initialized = 1;
            fallback_used = 0;

            TraceLog(LOG_INFO, "INBE: Successfully got insets from JNI: status=%d, nav=%d, cutout_top=%d",
                     elements[0], elements[1], elements[3]);
        } else {
            TraceLog(LOG_WARNING, "INBE: Failed to get array elements");
        }
    } else {
        TraceLog(LOG_WARNING, "INBE: Array too short, got %d elements, expected 6", length);
    }

    (*env)->DeleteLocalRef(env, insets_array);
    (*env)->DeleteLocalRef(env, activity_class);

    // Detach thread if we attached it
    if (result == JNI_EDETACHED) {
        (*vm)->DetachCurrentThread(vm);
    }
}

static void query_insets_fallback(void) {
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
    fallback_used = 1;
}

void android_insets_init(void) {
    pthread_mutex_lock(&insets_mutex);
    memset((void *)&current_insets, 0, sizeof(current_insets));
    pthread_mutex_unlock(&insets_mutex);
    insets_initialized = 0;
    fallback_used = 0;
}

void android_insets_get(AndroidInsets *out) {
    if (!insets_initialized) {
        query_insets_jni();
    }

    // If JNI query failed, fall back to estimates
    if (!insets_initialized) {
        query_insets_fallback();
    }

    // Try JNI again if we're using fallback
    if (fallback_used) {
        query_insets_jni();
    }

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
