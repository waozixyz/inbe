package xyz.waozi.inbe;

import android.content.Context;

/** Process-local presence. A push delivery in a new process starts as absent. */
final class NotificationPresence {
    private static volatile boolean activityVisible;

    private NotificationPresence() {
    }

    static void setActivityVisible(boolean visible) {
        activityVisible = visible;
    }

    static boolean canPostPush(Context context) {
        return activityVisible || SessionForegroundService.hasVisibleIndicator(context);
    }
}
