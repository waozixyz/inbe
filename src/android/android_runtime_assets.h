#ifndef INBE_ANDROID_RUNTIME_ASSETS_H
#define INBE_ANDROID_RUNTIME_ASSETS_H

#include <jni.h>

void android_runtime_assets_init(void);
void android_runtime_asset_native_succeeded(JNIEnv *env, jobject thiz,
                                            jlong handle, jlong bytes,
                                            jint http_status);
void android_runtime_asset_native_failed(JNIEnv *env, jobject thiz,
                                         jlong handle, jint http_status,
                                         jstring error);

#endif
