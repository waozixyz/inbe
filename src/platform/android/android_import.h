#ifndef ANDROID_IMPORT_H
#define ANDROID_IMPORT_H

#include "platform.h"
#include <stddef.h>

enum {
    ANDROID_IMPORT_RESULT_NONE = 0,
    ANDROID_IMPORT_RESULT_SELECTED = 1,
    ANDROID_IMPORT_RESULT_CANCELLED = 2
};

enum {
    ANDROID_IMPORT_KIND_DATA = 0,
    ANDROID_IMPORT_KIND_SYNC_KEY = 1,
    ANDROID_IMPORT_KIND_AUDIO_SOUND = 2,
    ANDROID_IMPORT_KIND_AUDIO_MUSIC = 3
};

int android_import_open_picker(const char *mime_types);
int android_import_open_picker_for(int kind, const char *mime_types);
int android_import_poll_result(char *path, size_t path_size);
int android_import_poll_result_for(int kind, char *path, size_t path_size);

#if ANDROID_BUILD
#include <jni.h>
void android_import_native_selected(JNIEnv *env, jobject thiz, jint kind, jstring path);
void android_import_native_cancelled(JNIEnv *env, jobject thiz, jint kind);
#endif

#endif
