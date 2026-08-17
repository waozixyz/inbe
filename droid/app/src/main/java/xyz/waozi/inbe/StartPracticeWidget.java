package xyz.waozi.inbe;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.widget.RemoteViews;

/**
 * Home-screen widget: a single "Breathe" button that opens the app and
 * starts the last-used practice straight away (the practice can also be
 * pinned per-shortcut via the launcher's long-press shortcuts).
 */
public class StartPracticeWidget extends AppWidgetProvider {
    @Override
    public void onUpdate(Context context, AppWidgetManager manager, int[] appWidgetIds) {
        Intent intent = new Intent(context, MainActivity.class);
        intent.setAction(MainActivity.ACTION_START_PRACTICE);
        intent.putExtra(MainActivity.EXTRA_PRACTICE_ID, -1);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);

        int flags = PendingIntent.FLAG_UPDATE_CURRENT;
        if (Build.VERSION.SDK_INT >= 23) {
            flags |= PendingIntent.FLAG_IMMUTABLE;
        }
        PendingIntent pending = PendingIntent.getActivity(
                context, 0, intent, flags);

        RemoteViews views = new RemoteViews(context.getPackageName(),
                R.layout.widget_breathe);
        views.setOnClickPendingIntent(R.id.widget_root, pending);
        manager.updateAppWidget(appWidgetIds, views);
    }
}
