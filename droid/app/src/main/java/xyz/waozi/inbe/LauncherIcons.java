package xyz.waozi.inbe;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.util.Log;

/** The component state is device-local and survives process restarts and upgrades. */
final class LauncherIcons {
    static final int SKY_CRADLE = 0;
    static final int INK_AND_AIR = 1;
    private static final String[] ALIASES = {
        "xyz.waozi.inbe.SkyCradleLauncher",
        "xyz.waozi.inbe.InkAndAirLauncher"
    };

    private LauncherIcons() {
    }

    private static ComponentName component(Context context, int icon) {
        // The application ID has a .debug suffix in debug builds; alias names don't.
        return new ComponentName(context.getPackageName(), ALIASES[icon]);
    }

    private static boolean enabled(Context context, int icon) {
        int state = context.getPackageManager().getComponentEnabledSetting(component(context, icon));
        return state == PackageManager.COMPONENT_ENABLED_STATE_ENABLED
            || (state == PackageManager.COMPONENT_ENABLED_STATE_DEFAULT && icon == SKY_CRADLE);
    }

    static synchronized int selected(Context context) {
        return enabled(context, INK_AND_AIR) ? INK_AND_AIR : SKY_CRADLE;
    }

    static synchronized boolean select(Context context, int icon) {
        if (icon != SKY_CRADLE && icon != INK_AND_AIR) {
            return false;
        }
        PackageManager manager = context.getPackageManager();
        int previous = selected(context);
        int other = icon == SKY_CRADLE ? INK_AND_AIR : SKY_CRADLE;
        if (enabled(context, icon) && !enabled(context, other)) {
            return true;
        }
        try {
            // Always leave a launchable entry, including on Android 21–32 where
            // component changes cannot be applied atomically. Never disable MainActivity.
            manager.setComponentEnabledSetting(component(context, icon),
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
            manager.setComponentEnabledSetting(component(context, other),
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);
            Log.i("LauncherIcons", "Selected " + ALIASES[icon]);
            return true;
        } catch (RuntimeException error) {
            Log.e("LauncherIcons", "Could not change launcher icon", error);
            try {
                manager.setComponentEnabledSetting(component(context, previous),
                    PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);
                int unused = previous == SKY_CRADLE ? INK_AND_AIR : SKY_CRADLE;
                manager.setComponentEnabledSetting(component(context, unused),
                    PackageManager.COMPONENT_ENABLED_STATE_DISABLED, PackageManager.DONT_KILL_APP);
            } catch (RuntimeException restoreError) {
                Log.e("LauncherIcons", "Could not restore launcher icon", restoreError);
            }
            return false;
        }
    }
}
