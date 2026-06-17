#ifndef INBE_ANDROID_DEVICE_H
#define INBE_ANDROID_DEVICE_H

#include "app.h"
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include <jni.h>
#else
typedef void JNIEnv;
typedef void *jobject;
typedef int jint;
#endif

void android_device_init(void);
int android_device_system_dark(void);
int android_device_orientation(void);
void android_device_set_orientation_mode(int mode);
void android_device_set_soft_keyboard_visible(int visible);
void android_device_native_set_system_dark(int dark);
void android_device_native_set_orientation(int orientation);
void android_device_native_text_input_commit(JNIEnv *env, jobject thiz, jint codepoint);
void android_device_native_text_input_backspace(JNIEnv *env, jobject thiz);
void android_device_native_text_input_enter(JNIEnv *env, jobject thiz);

#endif
