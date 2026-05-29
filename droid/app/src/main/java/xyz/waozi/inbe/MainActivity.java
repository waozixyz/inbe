package xyz.waozi.inbe;

import android.app.NativeActivity;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;

public class MainActivity extends NativeActivity {
    private static final String TAG = "InbeMainActivity";

    // Cache latest inset values for JNI access
    // [status_bar, nav_bar, cutout_left, cutout_top, cutout_right, cutout_bottom]
    private final int[] cachedInsets = new int[6];
    private boolean insetsInitialized = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Initialize with zeros
        for (int i = 0; i < 6; i++) {
            cachedInsets[i] = 0;
        }

        // Set up window insets listener
        setupInsetsListener();
    }

    private void setupInsetsListener() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            View decorView = getWindow().getDecorView();

            // Use ViewTreeObserver to listen for inset changes
            decorView.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
                @Override
                public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                    updateInsets(insets);
                    // Consume the insets to prevent them from being passed to child views
                    return v.onApplyWindowInsets(insets);
                }
            });

            // Force an initial update
            decorView.post(new Runnable() {
                @Override
                public void run() {
                    WindowInsets insets = decorView.getRootWindowInsets();
                    if (insets != null) {
                        updateInsets(insets);
                    }
                }
            });
        }
    }

    private void updateInsets(WindowInsets insets) {
        try {
            // System bars (status and navigation)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                cachedInsets[0] = insets.getSystemWindowInsetTop();     // status bar
                cachedInsets[1] = insets.getSystemWindowInsetBottom();  // navigation bar
            }

            // Display cutout (punch hole / notch) - API 28+
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                DisplayCutout cutout = insets.getDisplayCutout();
                if (cutout != null) {
                    cachedInsets[2] = cutout.getSafeInsetLeft();
                    cachedInsets[3] = cutout.getSafeInsetTop();
                    cachedInsets[4] = cutout.getSafeInsetRight();
                    cachedInsets[5] = cutout.getSafeInsetBottom();

                    Log.d(TAG, "Display cutout detected: left=" + cachedInsets[2] +
                          ", top=" + cachedInsets[3] + ", right=" + cachedInsets[4] +
                          ", bottom=" + cachedInsets[5]);
                } else {
                    // No cutout
                    cachedInsets[2] = 0;
                    cachedInsets[3] = 0;
                    cachedInsets[4] = 0;
                    cachedInsets[5] = 0;
                }
            }

            insetsInitialized = true;

            Log.d(TAG, "Insets updated: status=" + cachedInsets[0] +
                  ", nav=" + cachedInsets[1] + ", cutout_top=" + cachedInsets[3]);

        } catch (Exception e) {
            Log.e(TAG, "Error updating insets: " + e.getMessage(), e);
        }
    }

    /**
     * Called from native code via JNI to get current window insets.
     * Returns array of 6 ints: [status_bar, nav_bar, cutout_left, cutout_top, cutout_right, cutout_bottom]
     *
     * @return Array of inset values in pixels
     */
    public int[] getInsetsNative() {
        // Return a copy to avoid race conditions
        int[] result = new int[6];
        synchronized (cachedInsets) {
            System.arraycopy(cachedInsets, 0, result, 0, 6);
        }

        Log.d(TAG, "getInsetsNative called, returning: [" + result[0] + ", " + result[1] +
              ", " + result[2] + ", " + result[3] + ", " + result[4] + ", " + result[5] + "]");

        return result;
    }
}