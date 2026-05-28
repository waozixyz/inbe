#include "android_share.h"
#include "data.h"
#include "version.h"
#include "raylib.h"
#include "miniz.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

#define FS_PATH_MAX 512

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include <android/native_activity.h>
#include <android_native_app_glue.h>

extern struct android_app *GetAndroidApp(void);

static int is_session_file(const char *path)
{
    const char *filename;

    if(path == NULL)
        return 0;

    filename = GetFileName(path);
    return strncmp(filename, "inbe-", 5) == 0;
}

int android_share_export(const char *filename)
{
    struct android_app *app;
    mz_zip_archive archive;
    void *zip_data;
    size_t zip_size;
    int year, month, day;
    FilePathList files;
    int session_count = 0;
    char metadata[512];
    time_t now;
    struct tm *tm;
    char date_str[64];

    app = GetAndroidApp();
    if(app == NULL || app->activity == NULL) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: GetAndroidApp failed");
        return 0;
    }

    if(!data_has_any()) {
        TraceLog(LOG_WARNING, "ANDROID_SHARE: no data to export");
        return 0;
    }

    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_writer_init_heap(&archive, 0, 0)) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: failed to initialize ZIP archive");
        return 0;
    }

    session_count = data_get_session_count();

    now = time(NULL);
    tm = localtime(&now);
    if(tm != NULL) {
        snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d %02d:%02d:%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else {
        strcpy(date_str, "Unknown");
    }

    snprintf(metadata, sizeof(metadata),
             "Inner Breeze Data Export\n"
             "Version: %s\n"
             "Export Date: %s\n"
             "Session Count: %d\n",
             INBE_VERSION_STRING,
             date_str,
             session_count);

    if(!mz_zip_writer_add_mem(&archive, "lotus-data/metadata.txt", metadata, strlen(metadata), MZ_NO_COMPRESSION)) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: failed to write metadata");
        mz_zip_writer_end(&archive);
        return 0;
    }

    for(year = 1970; year <= 2100; year++) {
        char year_path[FS_PATH_MAX];
        snprintf(year_path, sizeof(year_path), "%s/%04d", data_root(), year);

        if(!DirectoryExists(year_path))
            continue;

        for(month = 1; month <= 12; month++) {
            char month_path[FS_PATH_MAX];
            snprintf(month_path, sizeof(month_path), "%s/%02d", year_path, month);

            if(!DirectoryExists(month_path))
                continue;

            for(day = 1; day <= 31; day++) {
                char day_path[FS_PATH_MAX];
                snprintf(day_path, sizeof(day_path), "%s/%02d", month_path, day);

                if(!DirectoryExists(day_path))
                    continue;

                files = LoadDirectoryFiles(day_path);
                for(unsigned int i = 0; i < files.count; i++) {
                    if(is_session_file(files.paths[i])) {
                        char *content;
                        char zip_path[FS_PATH_MAX];
                        const char *fname = GetFileName(files.paths[i]);

                        snprintf(zip_path, sizeof(zip_path),
                                 "lotus-data/sessions/%04d/%02d/%02d/%s",
                                 year, month, day, fname);

                        content = LoadFileText(files.paths[i]);
                        if(content != NULL) {
                            size_t size = strlen(content);
                            if(!mz_zip_writer_add_mem(&archive, zip_path, content, size, MZ_NO_COMPRESSION)) {
                                TraceLog(LOG_WARNING, "ANDROID_SHARE: failed to add file: %s", files.paths[i]);
                            }
                            UnloadFileText(content);
                        }
                    }
                }
                UnloadDirectoryFiles(files);
            }
        }
    }

    if(!mz_zip_writer_finalize_heap_archive(&archive, &zip_data, &zip_size)) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: failed to finalize ZIP archive");
        mz_zip_writer_end(&archive);
        return 0;
    }

    if(zip_data == NULL || zip_size == 0) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: failed to get ZIP data");
        mz_zip_writer_end(&archive);
        return 0;
    }

    TraceLog(LOG_INFO, "ANDROID_SHARE: calling Java to share %zu bytes", zip_size);

    ANativeActivity *native_activity = app->activity;
    JavaVM *jvm = native_activity->vm;
    JNIEnv *env = NULL;

    if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: failed to attach thread");
        mz_zip_writer_end(&archive);
        return 0;
    }

    /* Use class loader to find ShareHelper - needed when called from native threads */
    jclass activity_class = (*env)->FindClass(env, "android/app/Activity");
    if(!activity_class) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: Activity class not found");
        (*jvm)->DetachCurrentThread(jvm);
        mz_zip_writer_end(&archive);
        return 0;
    }

    jmethodID get_class_loader = (*env)->GetMethodID(env, activity_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if(!get_class_loader) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: getClassLoader method not found");
        (*jvm)->DetachCurrentThread(jvm);
        mz_zip_writer_end(&archive);
        return 0;
    }

    jobject class_loader = (*env)->CallObjectMethod(env, native_activity->clazz, get_class_loader);
    if(!class_loader) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: getClassLoader returned null");
        (*jvm)->DetachCurrentThread(jvm);
        mz_zip_writer_end(&archive);
        return 0;
    }

    jclass class_loader_class = (*env)->FindClass(env, "java/lang/ClassLoader");
    if(!class_loader_class) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: ClassLoader class not found");
        (*jvm)->DetachCurrentThread(jvm);
        mz_zip_writer_end(&archive);
        return 0;
    }

    jmethodID load_class = (*env)->GetMethodID(env, class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if(!load_class) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: loadClass method not found");
        (*jvm)->DetachCurrentThread(jvm);
        mz_zip_writer_end(&archive);
        return 0;
    }

    jstring class_name = (*env)->NewStringUTF(env, "xyz.waozi.inbe.ShareHelper");
    jclass share_helper_class = (jclass)(*env)->CallObjectMethod(env, class_loader, load_class, class_name);
    (*env)->DeleteLocalRef(env, class_name);

    if(!share_helper_class) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: ShareHelper class not found via class loader");
        (*jvm)->DetachCurrentThread(jvm);
        mz_zip_writer_end(&archive);
        return 0;
    }

    jmethodID method = (*env)->GetStaticMethodID(env, share_helper_class, "shareZipFile",
        "(Landroid/app/Activity;[BLjava/lang/String;)V");

    if(!method) {
        TraceLog(LOG_ERROR, "ANDROID_SHARE: shareZipFile method not found");
        (*jvm)->DetachCurrentThread(jvm);
        mz_zip_writer_end(&archive);
        return 0;
    }

    jbyteArray jarr = (*env)->NewByteArray(env, zip_size);
    (*env)->SetByteArrayRegion(env, jarr, 0, zip_size, (jbyte*)zip_data);

    jstring jname = (*env)->NewStringUTF(env, filename);
    (*env)->CallStaticVoidMethod(env, share_helper_class, method, native_activity->clazz, jarr, jname);

    (*env)->DeleteLocalRef(env, jarr);
    (*env)->DeleteLocalRef(env, jname);
    (*env)->DeleteLocalRef(env, class_loader);
    (*jvm)->DetachCurrentThread(jvm);

    mz_zip_writer_end(&archive);

    TraceLog(LOG_INFO, "ANDROID_SHARE: share sheet triggered");
    return 1;
}

#else
int android_share_export(const char *filename) {
    (void)filename;
    return 0;
}
#endif
