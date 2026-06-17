#include "android_runtime_assets.h"

#include "flint_runtime_assets.h"
#include "raylib.h"

#include <android/native_activity.h>
#include <android_native_app_glue.h>
#include <stdint.h>
#include <stdio.h>

extern struct android_app *GetAndroidApp(void);

static int
android_runtime_asset_download(FlintRuntimeAssetDownload *download,
                               const char *url,
                               const char *path)
{
    struct android_app *app;
    JavaVM *jvm;
    JNIEnv *env = NULL;
    int did_attach = 0;
    jclass activity_class;
    jmethodID method;
    jstring jurl;
    jstring jpath;

    if(download == NULL || url == NULL || path == NULL)
        return 0;

    app = GetAndroidApp();
    if(app == NULL || app->activity == NULL || app->activity->vm == NULL ||
       app->activity->clazz == NULL) {
        snprintf(download->error, sizeof(download->error), "Android activity is not ready");
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        return 0;
    }

    jvm = app->activity->vm;
    if((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK || env == NULL) {
            snprintf(download->error, sizeof(download->error), "failed to attach Android thread");
            download->status = FLINT_RUNTIME_ASSET_ERROR;
            return 0;
        }
        did_attach = 1;
    }

    activity_class = (*env)->GetObjectClass(env, app->activity->clazz);
    if(activity_class == NULL) {
        snprintf(download->error, sizeof(download->error), "Android activity class not found");
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        if(did_attach)
            (*jvm)->DetachCurrentThread(jvm);
        return 0;
    }

    method = (*env)->GetMethodID(env, activity_class, "startRuntimeAssetDownload",
                                 "(Ljava/lang/String;Ljava/lang/String;J)V");
    if(method == NULL) {
        snprintf(download->error, sizeof(download->error), "Android downloader method not found");
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        (*env)->DeleteLocalRef(env, activity_class);
        if(did_attach)
            (*jvm)->DetachCurrentThread(jvm);
        return 0;
    }

    jurl = (*env)->NewStringUTF(env, url);
    jpath = (*env)->NewStringUTF(env, path);
    if(jurl == NULL || jpath == NULL) {
        snprintf(download->error, sizeof(download->error), "failed to allocate Android strings");
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        if(jurl != NULL)
            (*env)->DeleteLocalRef(env, jurl);
        if(jpath != NULL)
            (*env)->DeleteLocalRef(env, jpath);
        (*env)->DeleteLocalRef(env, activity_class);
        if(did_attach)
            (*jvm)->DetachCurrentThread(jvm);
        return 0;
    }

    (*env)->CallVoidMethod(env, app->activity->clazz, method, jurl, jpath,
                           (jlong)(intptr_t)download);
    (*env)->DeleteLocalRef(env, jurl);
    (*env)->DeleteLocalRef(env, jpath);
    (*env)->DeleteLocalRef(env, activity_class);

    if((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        snprintf(download->error, sizeof(download->error), "Android downloader failed to start");
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        if(did_attach)
            (*jvm)->DetachCurrentThread(jvm);
        return 0;
    }

    if(did_attach)
        (*jvm)->DetachCurrentThread(jvm);
    return 1;
}

void
android_runtime_assets_init(void)
{
    flint_runtime_asset_set_download_backend(android_runtime_asset_download);
}

void
android_runtime_asset_native_succeeded(JNIEnv *env, jobject thiz,
                                       jlong handle, jlong bytes,
                                       jint http_status)
{
    FlintRuntimeAssetDownload *download = (FlintRuntimeAssetDownload *)(intptr_t)handle;
    (void)env;
    (void)thiz;

    if(download == NULL)
        return;

    download->http_status = (long)http_status;
    download->bytes = bytes > 0 ? (size_t)bytes : 0;
    if(download->total_bytes == 0)
        download->total_bytes = download->bytes;
    download->error[0] = '\0';
    download->status = FLINT_RUNTIME_ASSET_READY;
}

void
android_runtime_asset_native_progress(JNIEnv *env, jobject thiz,
                                      jlong handle, jlong bytes,
                                      jlong total_bytes)
{
    FlintRuntimeAssetDownload *download = (FlintRuntimeAssetDownload *)(intptr_t)handle;
    (void)env;
    (void)thiz;

    if(download == NULL)
        return;

    download->bytes = bytes > 0 ? (size_t)bytes : 0;
    download->total_bytes = total_bytes > 0 ? (size_t)total_bytes : 0;
}

void
android_runtime_asset_native_failed(JNIEnv *env, jobject thiz,
                                    jlong handle, jint http_status,
                                    jstring error)
{
    FlintRuntimeAssetDownload *download = (FlintRuntimeAssetDownload *)(intptr_t)handle;
    const char *message = NULL;
    (void)thiz;

    if(download == NULL)
        return;

    if(error != NULL)
        message = (*env)->GetStringUTFChars(env, error, NULL);

    download->http_status = (long)http_status;
    snprintf(download->error, sizeof(download->error), "%s",
             message != NULL && message[0] != '\0' ? message : "Android download failed");
    download->status = FLINT_RUNTIME_ASSET_ERROR;

    if(message != NULL)
        (*env)->ReleaseStringUTFChars(env, error, message);
}
