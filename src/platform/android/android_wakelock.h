#ifndef INBE_ANDROID_WAKELOCK_H
#define INBE_ANDROID_WAKELOCK_H

#include "platform.h"

#if ANDROID_BUILD
#include <jni.h>
void android_wakelock_init(void);
void android_wakelock_acquire(void);
void android_wakelock_release(void);
void android_wakelock_update_session_notification(const char *status_text);
void android_keep_screen_on(void);
void android_allow_screen_off(void);
void android_wakelock_set_activity(JNIEnv *env, jobject activity);
void android_wakelock_set_jvm(JavaVM *vm);
#else
// Stub functions for non-Android platforms
static inline void android_wakelock_init(void) {}
static inline void android_wakelock_acquire(void) {}
static inline void android_wakelock_release(void) {}
static inline void android_wakelock_update_session_notification(const char *status_text) { (void)status_text; }
static inline void android_keep_screen_on(void) {}
static inline void android_allow_screen_off(void) {}
#endif

#endif
