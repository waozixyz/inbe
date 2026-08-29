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
#include "breaks/app_breaks.h"
#include "practices/practice_registry.h"
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
    int system_left;
    int system_top;
    int system_right;
    int system_bottom;
    int cutout_left;
    int cutout_top;
    int cutout_right;
    int cutout_bottom;
} current_insets = {0};
static pthread_mutex_t insets_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile int insets_initialized = 0;
static int pending_practice_start = -1;
static int pending_donation_reminder = 0;

static void nativeSetInsets(JNIEnv *env, jobject thiz,
    jint system_left, jint system_top, jint system_right, jint system_bottom,
    jint cutout_left, jint cutout_top, jint cutout_right, jint cutout_bottom)
{
    TraceLog(LOG_INFO,
             "INBE: Java insets: system=%d,%d,%d,%d cutout=%d,%d,%d,%d",
             system_left, system_top, system_right, system_bottom,
             cutout_left, cutout_top, cutout_right, cutout_bottom);

    pthread_mutex_lock(&insets_mutex);
    current_insets.system_left = system_left;
    current_insets.system_top = system_top;
    current_insets.system_right = system_right;
    current_insets.system_bottom = system_bottom;
    current_insets.cutout_left = cutout_left;
    current_insets.cutout_top = cutout_top;
    current_insets.cutout_right = cutout_right;
    current_insets.cutout_bottom = cutout_bottom;
    insets_initialized = 1;
    pthread_mutex_unlock(&insets_mutex);
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

/* Widget / quick-settings tile / launcher shortcut entry point. Runs on
 * the UI thread like the other Java-spurred natives; the render loop
 * picks the new screen up on its next frame. A pending break is replaced
 * by the practice (same semantics as the desktop break-window chips). */
static jboolean nativeStartPractice(JNIEnv *env, jobject thiz, jint practice_id)
{
    InbeApp *app = get_global_inbe_app();

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
    InbeApp *app = get_global_inbe_app();
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
        TraceLog(LOG_ERROR, "AUDIO_E2E: import failed error=%d path=%s",
                 error_code, native_path);
        (*env)->ReleaseStringUTFChars(env, path, native_path);
        return JNI_FALSE;
    }
    (*env)->ReleaseStringUTFChars(env, path, native_path);

    practice_id = practice_clamp_id(practice_id);
    track = INBE_AUDIO_BUILTIN_MUSIC_COUNT + app->audio_custom_music_count - 1;
    app->meditation.music_practice_tracks[practice_id] = track;
    app->meditation.music_track = track;
    app_audio_music_sanitize_selection(app);
    app->sound_volume = 0;
    app->music_volume = 100;
    save_settings(app);
    TraceLog(LOG_INFO,
             "AUDIO_E2E: imported and selected track=%d practice=%d path=%s",
             track, (int)practice_id, app->audio_custom_music[track - INBE_AUDIO_BUILTIN_MUSIC_COUNT].path);
    return JNI_TRUE;
}

static jboolean nativeDebugStartMusicDownload(JNIEnv *env, jobject thiz)
{
    InbeApp *app = get_global_inbe_app();
    (void)env;
    (void)thiz;
    if(app == NULL)
        return JNI_FALSE;
    meditation_music_start_download(app);
    return JNI_TRUE;
}

static jboolean nativeDebugOpenDonationReminder(JNIEnv *env, jobject thiz)
{
    InbeApp *app = get_global_inbe_app();

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
    {"nativeSetInsets", "(IIIIIIII)V", (void*)nativeSetInsets},
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
    pending_practice_start = -1;
    pending_donation_reminder = 0;
    insets_initialized = 0;
    pthread_mutex_unlock(&insets_mutex);
}

void android_insets_get(AndroidInsets *out) {
    pthread_mutex_lock(&insets_mutex);

    if (out) {
        out->system_left = current_insets.system_left;
        out->system_top = current_insets.system_top;
        out->system_right = current_insets.system_right;
        out->system_bottom = current_insets.system_bottom;
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

int android_get_system_top_reserved(void) {
    int system_bar;
    int cutout_top;

    pthread_mutex_lock(&insets_mutex);
    system_bar = current_insets.system_top;
    cutout_top = current_insets.cutout_top;
    pthread_mutex_unlock(&insets_mutex);

    // Return the max of system bar and camera cutout, ensuring non-negative
    int system_top = system_bar > cutout_top ? system_bar : cutout_top;
    return system_top > 0 ? system_top : 0;
}
