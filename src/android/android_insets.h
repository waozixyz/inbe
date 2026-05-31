#ifndef INBE_ANDROID_INSETS_H
#define INBE_ANDROID_INSETS_H

#ifdef __ANDROID__
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

#endif

#endif
