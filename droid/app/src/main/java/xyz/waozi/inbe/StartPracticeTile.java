package xyz.waozi.inbe;

import android.app.PendingIntent;
import android.content.Intent;
import android.os.Build;
import android.service.quicksettings.TileService;

/**
 * Quick-settings tile: "Breathe" starts the last-used practice with one
 * tap from anywhere in the system.
 */
public class StartPracticeTile extends TileService {
    @Override
    public void onClick() {
        super.onClick();

        Intent intent = new Intent(this, MainActivity.class);
        intent.setAction(MainActivity.ACTION_START_PRACTICE);
        intent.putExtra(MainActivity.EXTRA_PRACTICE_ID, -1);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);

        if (Build.VERSION.SDK_INT >= 34) {
            int flags = PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE;
            PendingIntent pending = PendingIntent.getActivity(this, 0, intent, flags);
            startActivityAndCollapse(pending);
        } else {
            startActivityAndCollapse(intent);
        }
    }
}
