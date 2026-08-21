#ifndef INBE_ANDROID_INSETS_H
#define INBE_ANDROID_INSETS_H

#include "platform.h"

#if ANDROID_BUILD
#include <jni.h>

typedef struct {
	int status_bar;
	int nav_bar;
	int cutout_left;
	int cutout_top;
	int cutout_right;
	int cutout_bottom;
} AndroidInsets;

void android_insets_init(void);
void android_insets_get(AndroidInsets *out);
int android_insets_is_initialized(void);
int android_get_system_top_reserved(void);
int android_take_pending_practice_start(void);
int android_take_pending_donation_reminder(void);
void android_wakelock_set_activity(JNIEnv *env, jobject activity);
#else
typedef struct {
	int status_bar;
	int nav_bar;
	int cutout_left;
	int cutout_top;
	int cutout_right;
	int cutout_bottom;
} AndroidInsets;

static inline int android_get_system_top_reserved(void) { return 0; }
static inline void android_insets_init(void) {}
static inline void android_insets_get(AndroidInsets *out) { if(out) memset(out, 0, sizeof(AndroidInsets)); }
static inline int android_insets_is_initialized(void) { return 0; }
static inline int android_take_pending_practice_start(void) { return -1; }
static inline int android_take_pending_donation_reminder(void) { return 0; }

#endif

#endif
