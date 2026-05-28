#ifndef ANDROID_SHARE_H
#define ANDROID_SHARE_H

/* Export data using Android share sheet
 * Returns: 1 on success (share sheet shown), 0 on failure */
int android_share_export(const char *filename);

#endif
