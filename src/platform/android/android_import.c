#include "android_import.h"

#include "raylib.h"
#include <string.h>

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include <android/native_activity.h>
#include <android_native_app_glue.h>
#include <pthread.h>

extern struct android_app *GetAndroidApp(void);

#define ANDROID_IMPORT_PATH_MAX 512

static pthread_mutex_t import_mutex = PTHREAD_MUTEX_INITIALIZER;
static int pending_result = ANDROID_IMPORT_RESULT_NONE;
static char pending_path[ANDROID_IMPORT_PATH_MAX];

static void
android_import_set_result(int result, const char *path)
{
    pthread_mutex_lock(&import_mutex);
    pending_result = result;
    if(path != NULL) {
        strncpy(pending_path, path, sizeof(pending_path) - 1);
        pending_path[sizeof(pending_path) - 1] = '\0';
    } else {
        pending_path[0] = '\0';
    }
    pthread_mutex_unlock(&import_mutex);
}

int
android_import_open_picker(void)
{
    struct android_app *app = GetAndroidApp();
    if(app == NULL || app->activity == NULL) {
        TraceLog(LOG_ERROR, "ANDROID_IMPORT: GetAndroidApp failed");
        return 0;
    }

    ANativeActivity *activity = app->activity;
    JavaVM *jvm = activity->vm;
    JNIEnv *env = NULL;

    if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK) {
        TraceLog(LOG_ERROR, "ANDROID_IMPORT: failed to attach thread");
        return 0;
    }

    jclass activity_class = (*env)->GetObjectClass(env, activity->clazz);
    if(activity_class == NULL) {
        TraceLog(LOG_ERROR, "ANDROID_IMPORT: MainActivity class not found");
        (*jvm)->DetachCurrentThread(jvm);
        return 0;
    }

    jmethodID method = (*env)->GetMethodID(env, activity_class, "openImportPicker", "()V");
    if(method == NULL) {
        TraceLog(LOG_ERROR, "ANDROID_IMPORT: openImportPicker method not found");
        (*jvm)->DetachCurrentThread(jvm);
        return 0;
    }

    (*env)->CallVoidMethod(env, activity->clazz, method);
    if((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        (*jvm)->DetachCurrentThread(jvm);
        return 0;
    }

    (*jvm)->DetachCurrentThread(jvm);
    TraceLog(LOG_INFO, "ANDROID_IMPORT: file picker opened");
    return 1;
}

void
android_import_native_selected(JNIEnv *env, jobject thiz, jstring path)
{
    (void)thiz;
    if(path == NULL) {
        android_import_set_result(ANDROID_IMPORT_RESULT_SELECTED, "");
        return;
    }

    const char *import_path = (*env)->GetStringUTFChars(env, path, NULL);
    if(import_path == NULL || import_path[0] == '\0') {
        if(import_path != NULL)
            (*env)->ReleaseStringUTFChars(env, path, import_path);
        android_import_set_result(ANDROID_IMPORT_RESULT_SELECTED, "");
        return;
    }

    android_import_set_result(ANDROID_IMPORT_RESULT_SELECTED, import_path);
    (*env)->ReleaseStringUTFChars(env, path, import_path);
}

void
android_import_native_cancelled(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;
    android_import_set_result(ANDROID_IMPORT_RESULT_CANCELLED, NULL);
}

int
android_import_poll_result(char *path, size_t path_size)
{
    int result;

    pthread_mutex_lock(&import_mutex);
    result = pending_result;
    if(path != NULL && path_size > 0) {
        strncpy(path, pending_path, path_size - 1);
        path[path_size - 1] = '\0';
    }
    pending_result = ANDROID_IMPORT_RESULT_NONE;
    pending_path[0] = '\0';
    pthread_mutex_unlock(&import_mutex);

    return result;
}

#else
int
android_import_open_picker(void)
{
    return 0;
}

int
android_import_poll_result(char *path, size_t path_size)
{
    if(path != NULL && path_size > 0)
        path[0] = '\0';
    return ANDROID_IMPORT_RESULT_NONE;
}
#endif
