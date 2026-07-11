#include "android_share.h"
#include "platform.h"
#include "data.h"
#include "locale.h"
#include "storage.h"
#include "version.h"
#include "flint.h"
#include "miniz.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

#define FS_PATH_MAX 512

#if ANDROID_BUILD
#include <android/native_activity.h>
#include <android_native_app_glue.h>

extern struct android_app *GetAndroidApp(void);

static jclass
android_share_helper_class(JNIEnv *env, jobject activity)
{
    jclass activity_class;
    jmethodID get_class_loader;
    jobject class_loader;
    jclass class_loader_class;
    jmethodID load_class;
    jstring class_name;
    jclass share_helper_class;

    activity_class = (*env)->FindClass(env, "android/app/Activity");
    if(!activity_class)
        return NULL;
    get_class_loader = (*env)->GetMethodID(env, activity_class, "getClassLoader",
                                           "()Ljava/lang/ClassLoader;");
    if(!get_class_loader)
        return NULL;
    class_loader = (*env)->CallObjectMethod(env, activity, get_class_loader);
    if(!class_loader)
        return NULL;
    class_loader_class = (*env)->FindClass(env, "java/lang/ClassLoader");
    if(!class_loader_class)
        return NULL;
    load_class = (*env)->GetMethodID(env, class_loader_class, "loadClass",
                                     "(Ljava/lang/String;)Ljava/lang/Class;");
    if(!load_class)
        return NULL;
    class_name = (*env)->NewStringUTF(env, "xyz.waozi.inbe.ShareHelper");
    share_helper_class = (jclass)(*env)->CallObjectMethod(env, class_loader, load_class, class_name);
    (*env)->DeleteLocalRef(env, class_name);
    (*env)->DeleteLocalRef(env, class_loader);
    return share_helper_class;
}

int
android_share_bytes(const unsigned char *data, size_t data_size, const char *filename,
                    const char *mime_type)
{
    struct android_app *app = GetAndroidApp();
    ANativeActivity *native_activity;
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jclass share_helper_class;
    jmethodID method;
    jbyteArray jarr;
    jstring jname;
    jstring jmime;
    jstring jtitle;

    if(app == NULL || app->activity == NULL || data == NULL || data_size == 0 ||
       filename == NULL || filename[0] == '\0')
        return 0;

    native_activity = app->activity;
    jvm = native_activity->vm;
    if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK)
        return 0;

    share_helper_class = android_share_helper_class(env, native_activity->clazz);
    if(!share_helper_class) {
        (*jvm)->DetachCurrentThread(jvm);
        return 0;
    }
    method = (*env)->GetStaticMethodID(env, share_helper_class, "shareFile",
                                       "(Landroid/app/Activity;[BLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if(!method) {
        (*jvm)->DetachCurrentThread(jvm);
        return 0;
    }

    jarr = (*env)->NewByteArray(env, data_size);
    (*env)->SetByteArrayRegion(env, jarr, 0, data_size, (const jbyte *)data);
    jname = (*env)->NewStringUTF(env, filename);
    jmime = (*env)->NewStringUTF(env, mime_type != NULL ? mime_type : "application/octet-stream");
    jtitle = (*env)->NewStringUTF(env, GetLocaleText("share_sheet_title"));
    (*env)->CallStaticVoidMethod(env, share_helper_class, method, native_activity->clazz,
                                 jarr, jname, jmime, jtitle);

    (*env)->DeleteLocalRef(env, jarr);
    (*env)->DeleteLocalRef(env, jname);
    (*env)->DeleteLocalRef(env, jmime);
    (*env)->DeleteLocalRef(env, jtitle);
    (*jvm)->DetachCurrentThread(jvm);
    return 1;
}

int android_share_export(const char *filename)
{
    struct android_app *app;
    char export_path[FS_PATH_MAX];
    FILE *fp;
    char *zip_data;
    size_t zip_size;
    long file_size;

    app = GetAndroidApp();
    if(app == NULL || app->activity == NULL) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: GetAndroidApp failed");
        return 0;
    }

    if(!data_has_any()) {
        TraceLog(LOG_WARNING, "ANDROID_SHARE: no data to export");
        return 0;
    }

    snprintf(export_path, sizeof(export_path), "%s/%s", data_root(), filename);
    if(!storage_export_zip(export_path)) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: failed to export SQLite ZIP");
        return 0;
    }

    fp = fopen(export_path, "rb");
    if(fp == NULL)
        return 0;
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if(file_size <= 0) {
        fclose(fp);
        return 0;
    }
    zip_size = (size_t)file_size;
    zip_data = malloc(zip_size);
    if(zip_data == NULL) {
        fclose(fp);
        return 0;
    }
    if(fread(zip_data, 1, zip_size, fp) != zip_size) {
        fclose(fp);
        free(zip_data);
        return 0;
    }
    fclose(fp);

    TraceLog(LOG_INFO, "ANDROID_SHARE: calling Java to share %zu bytes", zip_size);

    ANativeActivity *native_activity = app->activity;
    JavaVM *jvm = native_activity->vm;
    JNIEnv *env = NULL;

    if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: failed to attach thread");
        free(zip_data);
        return 0;
    }

    /* Use class loader to find ShareHelper - needed when called from native threads */
    jclass activity_class = (*env)->FindClass(env, "android/app/Activity");
    if(!activity_class) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: Activity class not found");
        (*jvm)->DetachCurrentThread(jvm);
        free(zip_data);
        return 0;
    }

    jmethodID get_class_loader = (*env)->GetMethodID(env, activity_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if(!get_class_loader) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: getClassLoader method not found");
        (*jvm)->DetachCurrentThread(jvm);
        free(zip_data);
        return 0;
    }

    jobject class_loader = (*env)->CallObjectMethod(env, native_activity->clazz, get_class_loader);
    if(!class_loader) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: getClassLoader returned null");
        (*jvm)->DetachCurrentThread(jvm);
        free(zip_data);
        return 0;
    }

    jclass class_loader_class = (*env)->FindClass(env, "java/lang/ClassLoader");
    if(!class_loader_class) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: ClassLoader class not found");
        (*jvm)->DetachCurrentThread(jvm);
        free(zip_data);
        return 0;
    }

    jmethodID load_class = (*env)->GetMethodID(env, class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if(!load_class) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: loadClass method not found");
        (*jvm)->DetachCurrentThread(jvm);
        free(zip_data);
        return 0;
    }

    jstring class_name = (*env)->NewStringUTF(env, "xyz.waozi.inbe.ShareHelper");
    jclass share_helper_class = (jclass)(*env)->CallObjectMethod(env, class_loader, load_class, class_name);
    (*env)->DeleteLocalRef(env, class_name);

    if(!share_helper_class) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: ShareHelper class not found via class loader");
        (*jvm)->DetachCurrentThread(jvm);
        free(zip_data);
        return 0;
    }

    jmethodID method = (*env)->GetStaticMethodID(env, share_helper_class, "shareZipFile",
        "(Landroid/app/Activity;[BLjava/lang/String;Ljava/lang/String;)V");

    if(!method) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: shareZipFile method not found");
        (*jvm)->DetachCurrentThread(jvm);
        free(zip_data);
        return 0;
    }

    jbyteArray jarr = (*env)->NewByteArray(env, zip_size);
    (*env)->SetByteArrayRegion(env, jarr, 0, zip_size, (jbyte*)zip_data);

    jstring jname = (*env)->NewStringUTF(env, filename);
    jstring jtitle = (*env)->NewStringUTF(env, GetLocaleText("share_sheet_title"));
    (*env)->CallStaticVoidMethod(env, share_helper_class, method, native_activity->clazz, jarr, jname, jtitle);

    (*env)->DeleteLocalRef(env, jarr);
    (*env)->DeleteLocalRef(env, jname);
    (*env)->DeleteLocalRef(env, jtitle);
    (*env)->DeleteLocalRef(env, class_loader);
    (*jvm)->DetachCurrentThread(jvm);
    free(zip_data);

    TraceLog(LOG_INFO, "ANDROID_SHARE: share sheet triggered");
    return 1;
}

#else
int android_share_export(const char *filename) {
    (void)filename;
    return 0;
}
int android_share_bytes(const unsigned char *data, size_t data_size, const char *filename,
                        const char *mime_type) {
    (void)data;
    (void)data_size;
    (void)filename;
    (void)mime_type;
    return 0;
}
#endif
