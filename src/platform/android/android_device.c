#include "android_device.h"
#include "platform.h"
#include "flint_ui.h"

#if ANDROID_BUILD
#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <pthread.h>

extern struct android_app *GetAndroidApp(void);

static pthread_mutex_t device_mutex = PTHREAD_MUTEX_INITIALIZER;
static int current_system_dark = 0;
static int current_orientation = APP_DEVICE_ORIENTATION_UNKNOWN;

void
android_device_native_set_system_dark(int dark)
{
    pthread_mutex_lock(&device_mutex);
    current_system_dark = dark != 0;
    pthread_mutex_unlock(&device_mutex);
}

void
android_device_native_set_orientation(int orientation)
{
    pthread_mutex_lock(&device_mutex);
    current_orientation = orientation;
    pthread_mutex_unlock(&device_mutex);
}

void
android_device_init(void)
{
    pthread_mutex_lock(&device_mutex);
    current_system_dark = 0;
    current_orientation = APP_DEVICE_ORIENTATION_UNKNOWN;
    pthread_mutex_unlock(&device_mutex);
}

int
android_device_system_dark(void)
{
    int dark;
    pthread_mutex_lock(&device_mutex);
    dark = current_system_dark;
    pthread_mutex_unlock(&device_mutex);
    return dark;
}

int
android_device_orientation(void)
{
    int orientation;
    pthread_mutex_lock(&device_mutex);
    orientation = current_orientation;
    pthread_mutex_unlock(&device_mutex);
    return orientation;
}

void
android_device_set_orientation_mode(int mode)
{
    struct android_app *app = GetAndroidApp();
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    int attached = 0;

    if(app == NULL || app->activity == NULL || app->activity->vm == NULL ||
       app->activity->clazz == NULL)
        return;

    jvm = app->activity->vm;
    activity = app->activity->clazz;
    if((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK || env == NULL)
            return;
        attached = 1;
    }

    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;

    method = (*env)->GetMethodID(env, activity_class, "applyOrientationMode", "(I)V");
    if(method == NULL) {
        __android_log_write(ANDROID_LOG_ERROR, "INBE_DEVICE", "applyOrientationMode not found");
        goto done;
    }

    (*env)->CallVoidMethod(env, activity, method, (jint)mode);

done:
    if((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
    if(attached)
        (*jvm)->DetachCurrentThread(jvm);
}

void
android_device_set_soft_keyboard_visible(int visible)
{
    struct android_app *app = GetAndroidApp();
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method;
    int attached = 0;

    if(app == NULL || app->activity == NULL || app->activity->vm == NULL ||
       app->activity->clazz == NULL)
        return;

    jvm = app->activity->vm;
    activity = app->activity->clazz;
    if((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK || env == NULL)
            return;
        attached = 1;
    }

    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;

    method = (*env)->GetMethodID(env, activity_class, "setSoftKeyboardVisible", "(Z)V");
    if(method == NULL) {
        __android_log_write(ANDROID_LOG_ERROR, "INBE_DEVICE", "setSoftKeyboardVisible not found");
        goto done;
    }

    (*env)->CallVoidMethod(env, activity, method, visible ? JNI_TRUE : JNI_FALSE);

done:
    if((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
    if(attached)
        (*jvm)->DetachCurrentThread(jvm);
}

void
android_device_native_text_input_commit(JNIEnv *env, jobject thiz, jint codepoint)
{
    (void)env;
    (void)thiz;
    flint_ui_text_input_queue_codepoint((int)codepoint);
}

void
android_device_native_text_input_backspace(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;
    flint_ui_text_input_queue_backspace();
}

void
android_device_native_text_input_enter(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;
    flint_ui_text_input_queue_enter();
}

#else

void android_device_native_set_system_dark(int dark) { (void)dark; }
void android_device_native_set_orientation(int orientation) { (void)orientation; }
void android_device_native_text_input_commit(JNIEnv *env, jobject thiz, jint codepoint) { (void)env; (void)thiz; (void)codepoint; }
void android_device_native_text_input_backspace(JNIEnv *env, jobject thiz) { (void)env; (void)thiz; }
void android_device_native_text_input_enter(JNIEnv *env, jobject thiz) { (void)env; (void)thiz; }
void android_device_init(void) {}
int android_device_system_dark(void) { return 0; }
int android_device_orientation(void) { return APP_DEVICE_ORIENTATION_UNKNOWN; }
void android_device_set_orientation_mode(int mode) { (void)mode; }
void android_device_set_soft_keyboard_visible(int visible) { (void)visible; }

#endif
