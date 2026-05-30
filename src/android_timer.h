#ifndef INBE_ANDROID_TIMER_H
#define INBE_ANDROID_TIMER_H

#ifdef __ANDROID__
#include <jni.h>
#include <pthread.h>
void android_timer_init(void);
void android_timer_start(void);
void android_timer_stop(void);
void android_timer_set_app(void *app_ptr);
void android_timer_activate(void);
void android_timer_deactivate(void);
void* android_timer_get_app(void);
pthread_mutex_t* android_timer_get_mutex(void);
#else
// Stub functions for non-Android platforms
static inline void android_timer_init(void) {}
static inline void android_timer_start(void) {}
static inline void android_timer_stop(void) {}
static inline void android_timer_set_app(void *app_ptr) {(void)app_ptr;}
static inline void android_timer_activate(void) {}
static inline void android_timer_deactivate(void) {}
static inline void* android_timer_get_app(void) { return NULL; }
static inline void* android_timer_get_mutex(void) { return NULL; }
#endif

#endif
