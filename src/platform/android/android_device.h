#ifndef INBE_ANDROID_DEVICE_H
#define INBE_ANDROID_DEVICE_H

#include "app.h"

void android_device_init(void);
int android_device_system_dark(void);
int android_device_orientation(void);
void android_device_set_orientation_mode(int mode);
void android_device_native_set_system_dark(int dark);
void android_device_native_set_orientation(int orientation);

#endif
