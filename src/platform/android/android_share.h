#ifndef ANDROID_SHARE_H
#define ANDROID_SHARE_H

#include <stddef.h>

/* Export data using Android share sheet
 * Returns: 1 on success (share sheet shown), 0 on failure */
int android_share_export(const char *filename);
int android_share_bytes(const unsigned char *data, size_t data_size, const char *filename,
                        const char *mime_type);

#endif
