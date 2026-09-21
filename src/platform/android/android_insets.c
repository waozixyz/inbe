#include "android_insets.h"
#include "android_import.h"
#include "android_device.h"
#include "android_runtime_assets.h"
#include "android_wakelock.h"
#include "platform/android/android_lifecycle.h"

#include "kryon.h"
#include <pthread.h>
#include <jni.h>

#define LOG_TAG "APP_INSETS"

#ifndef JNI_VERSION_1_6
#define JNI_VERSION_1_6 0x10060000
#endif

extern void android_wakelock_set_activity(JNIEnv *env, jobject activity);
extern void android_wakelock_set_jvm(JavaVM *vm);
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

    extern void SetDeviceDensity(float);
    SetDeviceDensity(density);
}

static jint nativeSyncLifecycleState(JNIEnv *env, jobject thiz,
                                    jboolean paused, jboolean indicator_visible)
{
    (void)env;
    (void)thiz;
    return android_sync_lifecycle(paused != 0, indicator_visible != 0);
}

static void nativeInvalidateGraphicsResources(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;

    android_invalidate_graphics_resources();
}

/* Widget / quick-settings tile / launcher shortcut entry point. Runs on
 * the UI thread like the other Java-spurred natives; the render loop
 * picks the new screen up on its next frame. A pending break is replaced
 * by the practice (same semantics as the desktop break-window chips). */
static jboolean nativeStartPractice(JNIEnv *env, jobject thiz, jint practice_id)
{
    int selected_practice;

    (void)env;
    (void)thiz;
    pthread_mutex_lock(&insets_mutex);
    selected_practice = android_practice_to_start(practice_id);
    if(selected_practice >= 0)
        pending_practice_start = selected_practice;
    pthread_mutex_unlock(&insets_mutex);
    return selected_practice >= 0 ? JNI_TRUE : JNI_FALSE;
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
    const char *native_path;
    int imported;

    (void)thiz;
    if(path == NULL)
        return JNI_FALSE;
    native_path = (*env)->GetStringUTFChars(env, path, NULL);
    if(native_path == NULL)
        return JNI_FALSE;
    imported = android_debug_import_music_for_practice(native_path, practice_id);
    (*env)->ReleaseStringUTFChars(env, path, native_path);
    return imported ? JNI_TRUE : JNI_FALSE;
}

static jboolean nativeDebugStartMusicDownload(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;
    return android_debug_start_music_download() ? JNI_TRUE : JNI_FALSE;
}

static jboolean nativeDebugOpenDonationReminder(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;
    if(!android_can_open_donation_reminder())
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
    {"nativeSyncLifecycleState", "(ZZ)I", (void*)nativeSyncLifecycleState},
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

    jclass clazz = (*env)->FindClass(env, "xyz/waozi/inbe/MainActivity");
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
    android_lifecycle_reset();
    pthread_mutex_lock(&insets_mutex);
    pending_practice_start = -1;
    pending_donation_reminder = 0;
    pthread_mutex_unlock(&insets_mutex);
}
