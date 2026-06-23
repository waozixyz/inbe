#ifndef ANDROID_IMPORT_H
#define ANDROID_IMPORT_H

#include "platform.h"
#include <stddef.h>

int android_import_open_picker(const char *mime_types);
int android_import_poll_result(char *path, size_t path_size);

enum {
    ANDROID_IMPORT_RESULT_NONE = 0,
    ANDROID_IMPORT_RESULT_SELECTED = 1,
    ANDROID_IMPORT_RESULT_CANCELLED = 2
};

#if ANDROID_BUILD
#include <jni.h>
void android_import_native_selected(JNIEnv *env, jobject thiz, jstring path);
void android_import_native_cancelled(JNIEnv *env, jobject thiz);
#endif

#endif
