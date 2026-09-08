#include "android_insets.h"
#include "android_import.h"
#include "android_device.h"
#include "android_runtime_assets.h"
#include "android_timer.h"
#include "android_wakelock.h"
#include "app.h"
#include "core/breath_engine.h"
#include <stdio.h>

#include "kryon.h"
#include "breaks/app_breaks.h"
#include "practices/practice_registry.h"
#include <pthread.h>
#include <jni.h>
#include <android/log.h>

extern InnerBreeze* get_global_app(void);
extern void app_request_graphics_reload(InnerBreeze*app);

#define LOG_TAG "APP_INSETS"

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

static pthread_mutex_t insets_mutex = PTHREAD_MUTEX_INITIALIZER;
static int pending_practice_start = -1;
static int pending_donation_reminder = 0;

static void nativeSetInsets(JNIEnv *env, jobject thiz,
    jint system_left, jint system_top, jint system_right, jint system_bottom,
    jint ime_bottom,
    jint cutout_left, jint cutout_top, jint cutout_right, jint cutout_bottom)
{
    TraceLog(LOG_INFO,
             "APP: Java insets: system=%d,%d,%d,%d ime=%d cutout=%d,%d,%d,%d",
             system_left, system_top, system_right, system_bottom, ime_bottom,
             cutout_left, cutout_top, cutout_right, cutout_bottom);

    (void)env;
    (void)thiz;
    SetAndroidWindowInsets(system_left, system_top, system_right, system_bottom,
                           ime_bottom,
                           cutout_left, cutout_top, cutout_right, cutout_bottom);
}

static void nativeSetDeviceDensity(JNIEnv *env, jobject thiz, jfloat density)
{
    (void)env;
    (void)thiz;

    extern void SetUIDeviceDensity(float);
    SetUIDeviceDensity(density);
}

static jint nativeGetPlayInBackground(JNIEnv *env, jobject thiz)
{
	void *app = get_global_app();
	if(app == NULL)
		return 0;
	return get_play_in_background(&((InnerBreeze*)app)->breathing);
}

static void nativeSetBackgroundActive(JNIEnv *env, jobject thiz, jboolean active)
{
	(void)env;
	(void)thiz;

	void *app = get_global_app();
	InnerBreeze*app = (InnerBreeze*)app;

	if (active) {
		if (app != NULL) {
			app->backgrounded = 1;
		}
		android_timer_activate();
	} else {
		android_timer_deactivate();
		if (app != NULL) {
			app->backgrounded = 0;
		}
	}
}

static jint nativePauseSession(JNIEnv *env, jobject thiz)
{
	(void)env;
	(void)thiz;

	InnerBreeze*app = get_global_app();
	if (app == NULL) {
		__android_log_write(ANDROID_LOG_ERROR, "APP_JNI", "nativePauseSession: app is NULL!");
		return 0;
	}
	if (app->session_paused)
		return 0;

	app->session_paused = 1;
	app->backgrounded = 1;
	return 1;
}

static void nativeResumeSession(JNIEnv *env, jobject thiz)
{
	(void)env;
	(void)thiz;

	InnerBreeze*app = get_global_app();
	if (app == NULL) {
		__android_log_write(ANDROID_LOG_ERROR, "APP_JNI", "nativeResumeSession: app is NULL!");
		return;
	}
	if (!app->session_paused)
		return;

	app->session_paused = 0;
	app->backgrounded = 0;
}

static void nativeInvalidateGraphicsResources(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;

    InnerBreeze*app = get_global_app();
    if(app != NULL)
        app_request_graphics_reload(app);
}

/* Widget / quick-settings tile / launcher shortcut entry point. Runs on
 * the UI thread like the other Java-spurred natives; the render loop
 * picks the new screen up on its next frame. A pending break is replaced
 * by the practice (same semantics as the desktop break-window chips). */
static jboolean nativeStartPractice(JNIEnv *env, jobject thiz, jint practice_id)
{
    InnerBreeze*app = get_global_app();

    (void)env;
    (void)thiz;
    if(app == NULL)
        return JNI_FALSE;

    pthread_mutex_lock(&insets_mutex);
    pending_practice_start = practice_id >= 0
        ? practice_clamp_id(practice_id)
        : practice_clamp_id(app->exercise_type);
    pthread_mutex_unlock(&insets_mutex);
    return JNI_TRUE;
}

int android_take_pending_practice_start(void)
{
    int practice_id;

    pthread_mutex_lock(&insets_mutex);
    practice_id = pending_practice_start;
    pending_practice_start = -1;
    pthread_mutex_unlock(&insets_mutex);
    return practice_id;
}

int android_take_pending_donation_reminder(void)
{
    int pending;

    pthread_mutex_lock(&insets_mutex);
    pending = pending_donation_reminder;
    pending_donation_reminder = 0;
    pthread_mutex_unlock(&insets_mutex);
    return pending;
}

static jboolean
nativeDebugImportMusicForPractice(JNIEnv *env, jobject thiz, jstring path,
                                  jint practice_id)
{
    InnerBreeze*app = get_global_app();
    const char *native_path;
    int error_code = AUDIO_IMPORT_ERROR_UNKNOWN;
    int track;

    (void)thiz;
    if(app == NULL || path == NULL)
        return JNI_FALSE;
    native_path = (*env)->GetStringUTFChars(env, path, NULL);
    if(native_path == NULL)
        return JNI_FALSE;
    app_audio_ensure_ready(app);
    if(!app_audio_import_custom_music_ex(app, native_path, &error_code)) {
        TraceLog(LOG_ERROR, "ANDROID_DEBUG_MUSIC: import failed error=%d path=%s",
                 error_code, native_path);
        (*env)->ReleaseStringUTFChars(env, path, native_path);
        return JNI_FALSE;
    }
    (*env)->ReleaseStringUTFChars(env, path, native_path);

    practice_id = practice_clamp_id(practice_id);
    track = AUDIO_BUILTIN_MUSIC_COUNT + app->audio_custom_music_count - 1;
    app->meditation.music_practice_tracks[practice_id] = track;
    app->meditation.music_track = track;
    app_audio_music_sanitize_selection(app);
    app->sound_volume = 0;
    app->music_volume = 100;
    save_settings(app);
    TraceLog(LOG_INFO,
             "ANDROID_DEBUG_MUSIC: imported and selected track=%d practice=%d path=%s",
             track, (int)practice_id, app->audio_custom_music[track - AUDIO_BUILTIN_MUSIC_COUNT].path);
    return JNI_TRUE;
}

static jboolean nativeDebugStartMusicDownload(JNIEnv *env, jobject thiz)
{
    InnerBreeze*app = get_global_app();
    (void)env;
    (void)thiz;
    if(app == NULL)
        return JNI_FALSE;
    meditation_music_start_download(app);
    return JNI_TRUE;
}

static jboolean nativeDebugOpenDonationReminder(JNIEnv *env, jobject thiz)
{
    InnerBreeze*app = get_global_app();

    (void)env;
    (void)thiz;
    if(app == NULL)
        return JNI_FALSE;
    pthread_mutex_lock(&insets_mutex);
    pending_donation_reminder = 1;
    pthread_mutex_unlock(&insets_mutex);
    return JNI_TRUE;
}

static const JNINativeMethod g_methods[] = {
    {"nativeSetInsets", "(IIIIIIIII)V", (void*)nativeSetInsets},
    {"nativeSetDeviceDensity", "(F)V", (void*)nativeSetDeviceDensity},
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
    {"nativeStartPractice", "(I)Z", (void*)nativeStartPractice},
    {"nativeDebugImportMusicForPractice", "(Ljava/lang/String;I)Z", (void*)nativeDebugImportMusicForPractice},
    {"nativeDebugStartMusicDownload", "()Z", (void*)nativeDebugStartMusicDownload},
    {"nativeDebugOpenDonationReminder", "()Z", (void*)nativeDebugOpenDonationReminder},
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env = NULL;
    jint result;

    android_wakelock_set_jvm(vm);

    result = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);
    if (result != JNI_OK) {
        TraceLog(LOG_ERROR, "APP: Failed to get JNI environment");
        return result;
    }

    jclass clazz = (*env)->FindClass(env, "xyz/waozi/breathing/MainActivity");
    if (clazz == NULL) {
        TraceLog(LOG_ERROR, "APP: Failed to find MainActivity class");
        return JNI_ERR;
    }

    result = (*env)->RegisterNatives(env, clazz, g_methods, sizeof(g_methods) / sizeof(g_methods[0]));
    if (result != JNI_OK) {
        TraceLog(LOG_ERROR, "APP: Failed to register native methods");
        return result;
    }

    return JNI_VERSION_1_6;
}

void android_insets_init(void) {
    pthread_mutex_lock(&insets_mutex);
    pending_practice_start = -1;
    pending_donation_reminder = 0;
    pthread_mutex_unlock(&insets_mutex);
}
