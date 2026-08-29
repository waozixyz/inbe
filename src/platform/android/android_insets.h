#ifndef INBE_ANDROID_INSETS_H
#define INBE_ANDROID_INSETS_H

#include "android_surface.h"
#include "platform.h"
#include <string.h>

#if ANDROID_BUILD
#include <jni.h>

typedef AndroidWindowInsets AndroidInsets;

void android_insets_init(void);
void android_insets_get(AndroidInsets *out);
int android_insets_is_initialized(void);
int android_get_system_top_reserved(void);
int android_take_pending_practice_start(void);
int android_take_pending_donation_reminder(void);
void android_wakelock_set_activity(JNIEnv *env, jobject activity);
#else
typedef AndroidWindowInsets AndroidInsets;

static inline int android_get_system_top_reserved(void) { return 0; }
static inline void android_insets_init(void) {}
static inline void android_insets_get(AndroidInsets *out) { if(out) memset(out, 0, sizeof(AndroidInsets)); }
static inline int android_insets_is_initialized(void) { return 0; }
static inline int android_take_pending_practice_start(void) { return -1; }
static inline int android_take_pending_donation_reminder(void) { return 0; }

#endif

#endif
