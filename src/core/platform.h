#ifndef INBE_PLATFORM_H
#define INBE_PLATFORM_H

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define INBE_ANDROID_BUILD 1
#else
#define INBE_ANDROID_BUILD 0
#endif

#endif
