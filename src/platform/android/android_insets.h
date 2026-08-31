#ifndef INBE_ANDROID_INSETS_H
#define INBE_ANDROID_INSETS_H

#include "android_surface.h"
#include "platform.h"
#include <string.h>

#if ANDROID_BUILD
#include <jni.h>

void android_insets_init(void);
int android_take_pending_practice_start(void);
int android_take_pending_donation_reminder(void);
void android_wakelock_set_activity(JNIEnv *env, jobject activity);
#else
static inline void android_insets_init(void) {}
static inline int android_take_pending_practice_start(void) { return -1; }
static inline int android_take_pending_donation_reminder(void) { return 0; }

#endif

#endif
