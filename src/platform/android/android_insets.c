#include "android_insets.h"
#include "android_import.h"
#include "android_device.h"
#include "android_runtime_assets.h"
#include "android_timer.h"
#include "android_wakelock.h"
#include "app.h"
#include "core/breath_engine.h"
#include <string.h>
#include <stdio.h>

#include "kryon.h"
#include <pthread.h>
#include <jni.h>
#include <android/log.h>

extern InbeApp* get_global_inbe_app(void);
extern void app_request_graphics_reload(InbeApp *app);

#define LOG_TAG "INBE_INSETS"

#ifndef JNI_VERSION_1_6
#define JNI_VERSION_1_6 0x10060000
#endif

extern void android_wakelock_set_activity(JNIEnv *env, jobject activity);
extern void android_wakelock_set_jvm(JavaVM *vm);
extern void android_timer_activate(void);
extern void android_timer_deactivate(void);

static void android_wakelock_set_activity_impl(JNIEnv *env, jobject thiz) {
    android_wakelock_set_activity(env, thiz);
}

static volatile struct {
    int status_bar;
    int nav_bar;
    int cutout_left;
    int cutout_top;
    int cutout_right;
    int cutout_bottom;
} current_insets = {0};
static pthread_mutex_t insets_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int insets_initialized = 0;

static void nativeSetInsets(JNIEnv *env, jobject thiz,
    jint status_bar, jint nav_bar,
    jint cutout_left, jint cutout_top, jint cutout_right, jint cutout_bottom)
{
    TraceLog(LOG_INFO,
             "INBE: Java insets: status=%d nav=%d cutout_top=%d",
             status_bar, nav_bar, cutout_top);

    pthread_mutex_lock(&insets_mutex);
    current_insets.status_bar = status_bar;
    current_insets.nav_bar = nav_bar;
    current_insets.cutout_left = cutout_left;
    current_insets.cutout_top = cutout_top;
    current_insets.cutout_right = cutout_right;
    current_insets.cutout_bottom = cutout_bottom;
    insets_initialized = 1;
    pthread_mutex_unlock(&insets_mutex);
}

static jint nativeGetPlayInBackground(JNIEnv *env, jobject thiz)
{
	void *app = get_global_inbe_app();
	if(app == NULL)
		return 0;
	return get_play_in_background(&((InbeApp*)app)->inbe);
}

static void nativeSetBackgroundActive(JNIEnv *env, jobject thiz, jboolean active)
{
	(void)env;
	(void)thiz;

	void *app = get_global_inbe_app();
	InbeApp *inbe_app = (InbeApp*)app;

	if (active) {
		if (inbe_app != NULL) {
			inbe_app->backgrounded = 1;
		}
		android_timer_activate();
	} else {
		android_timer_deactivate();
		if (inbe_app != NULL) {
			inbe_app->backgrounded = 0;
		}
	}
}

static jint nativePauseSession(JNIEnv *env, jobject thiz)
{
	(void)env;
	(void)thiz;

	InbeApp *inbe_app = get_global_inbe_app();
	if (inbe_app == NULL) {
		__android_log_write(ANDROID_LOG_ERROR, "INBE_JNI", "nativePauseSession: app is NULL!");
		return 0;
	}
	if (inbe_app->session_paused)
		return 0;

	inbe_app->session_paused = 1;
	inbe_app->backgrounded = 1;
	return 1;
}

static void nativeResumeSession(JNIEnv *env, jobject thiz)
{
	(void)env;
	(void)thiz;

	InbeApp *inbe_app = get_global_inbe_app();
	if (inbe_app == NULL) {
		__android_log_write(ANDROID_LOG_ERROR, "INBE_JNI", "nativeResumeSession: app is NULL!");
		return;
	}
	if (!inbe_app->session_paused)
		return;

	inbe_app->session_paused = 0;
	inbe_app->backgrounded = 0;
}

static void nativeInvalidateGraphicsResources(JNIEnv *env, jobject thiz)
{
	(void)env;
	(void)thiz;

	InbeApp *inbe_app = get_global_inbe_app();
	if(inbe_app != NULL)
		app_request_graphics_reload(inbe_app);
}

static const JNINativeMethod g_methods[] = {
    {"nativeSetInsets", "(IIIIII)V", (void*)nativeSetInsets},
    {"nativeWakeLockReady", "()V", (void*)android_wakelock_set_activity_impl},
    {"nativeSetBackgroundActive", "(Z)V", (void*)nativeSetBackgroundActive},
    {"nativeGetPlayInBackground", "()I", (void*)nativeGetPlayInBackground},
    {"nativePauseSession", "()I", (void*)nativePauseSession},
    {"nativeResumeSession", "()V", (void*)nativeResumeSession},
    {"nativeSetSystemDark", "(I)V", (void*)android_device_native_set_system_dark},
    {"nativeSetOrientation", "(I)V", (void*)android_device_native_set_orientation},
    {"nativeImportSelectedFile", "(ILjava/lang/String;)V", (void*)android_import_native_selected},
    {"nativeImportCancelled", "(I)V", (void*)android_import_native_cancelled},
    {"nativeRuntimeAssetDownloadSucceeded", "(JJI)V", (void*)android_runtime_asset_native_succeeded},
    {"nativeRuntimeAssetDownloadProgress", "(JJJ)V", (void*)android_runtime_asset_native_progress},
    {"nativeRuntimeAssetDownloadFailed", "(JILjava/lang/String;)V", (void*)android_runtime_asset_native_failed},
    {"nativeTextInputCommit", "(I)V", (void*)android_device_native_text_input_commit},
    {"nativeTextInputBackspace", "()V", (void*)android_device_native_text_input_backspace},
    {"nativeTextInputEnter", "()V", (void*)android_device_native_text_input_enter},
    {"nativeInvalidateGraphicsResources", "()V", (void*)nativeInvalidateGraphicsResources},
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env = NULL;
    jint result;

    android_wakelock_set_jvm(vm);

    result = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);
    if (result != JNI_OK) {
        TraceLog(LOG_ERROR, "INBE: Failed to get JNI environment");
        return result;
    }

    jclass clazz = (*env)->FindClass(env, "xyz/waozi/inbe/MainActivity");
    if (clazz == NULL) {
        TraceLog(LOG_ERROR, "INBE: Failed to find MainActivity class");
        return JNI_ERR;
    }

    result = (*env)->RegisterNatives(env, clazz, g_methods, sizeof(g_methods) / sizeof(g_methods[0]));
    if (result != JNI_OK) {
        TraceLog(LOG_ERROR, "INBE: Failed to register native methods");
        return result;
    }

    return JNI_VERSION_1_6;
}

void android_insets_init(void) {
    pthread_mutex_lock(&insets_mutex);
    memset((void *)&current_insets, 0, sizeof(current_insets));
    insets_initialized = 0;
    pthread_mutex_unlock(&insets_mutex);
}

void android_insets_get(AndroidInsets *out) {
    pthread_mutex_lock(&insets_mutex);

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

int android_insets_is_initialized(void) {
    int result;
    pthread_mutex_lock(&insets_mutex);
    result = insets_initialized;
    pthread_mutex_unlock(&insets_mutex);
    return result;
}
