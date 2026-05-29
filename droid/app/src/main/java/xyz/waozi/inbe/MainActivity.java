package xyz.waozi.inbe;

import android.app.NativeActivity;
import android.graphics.Insets;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;

public class MainActivity extends NativeActivity {
    private static final String TAG = "InbeMainActivity";

    static {
        System.loadLibrary("main");
    }

    // [status_bar, nav_bar, cutout_left, cutout_top, cutout_right, cutout_bottom]
    private final int[] cachedInsets = new int[6];
    private boolean insetsInitialized = false;

    private native void nativeSetInsets(int status, int nav,
        int cutoutLeft, int cutoutTop, int cutoutRight, int cutoutBottom);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        synchronized (cachedInsets) {
            for (int i = 0; i < 6; i++) {
                cachedInsets[i] = 0;
            }
        }

        setupInsetsListener();
    }

    private void setupInsetsListener() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            final View decorView = getWindow().getDecorView();

            // 1. Primary listener for runtime inset modifications (swiping, rotating)
            decorView.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
                @Override
                public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                    updateInsets(insets);
                    return insets;
                }
            });

            // 2. Startup safety net: Catches frame sizing on the initial layout pass
            decorView.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
                @Override
                public void onGlobalLayout() {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                        WindowInsets insets = decorView.getRootWindowInsets();
                        if (insets != null) {
                            updateInsets(insets);
                        }
                    }
                    decorView.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                }
            });

            // 3. Force synchronous scheduling update pass
            decorView.post(new Runnable() {
                @Override
                public void run() {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                        decorView.requestApplyInsets();
                    }
                }
            });
        } else {
            fallbackForOldPhones();
        }
    }

    private void fallbackForOldPhones() {
        int statusBarHeight = 0;
        int navBarHeight = 0;

        int resourceId = getResources().getIdentifier("status_bar_height", "dimen", "android");
        if (resourceId > 0) {
            statusBarHeight = getResources().getDimensionPixelSize(resourceId);
        }

        int navResourceId = getResources().getIdentifier("navigation_bar_height", "dimen", "android");
        if (navResourceId > 0) {
            navBarHeight = getResources().getDimensionPixelSize(navResourceId);
        }

        synchronized (cachedInsets) {
            cachedInsets[0] = statusBarHeight;
            cachedInsets[1] = navBarHeight;
            cachedInsets[2] = 0;
            cachedInsets[3] = 0;
            cachedInsets[4] = 0;
            cachedInsets[5] = 0;
        }
        
        insetsInitialized = true;
        nativeSetInsets(statusBarHeight, navBarHeight, 0, 0, 0, 0);
    }

    private void updateInsets(WindowInsets insets) {
        if (insets == null) return;

        try {
            int statusBar = 0;
            int navBar = 0;
            int cLeft = 0, cTop = 0, cRight = 0, cBottom = 0;

            // System bar calculations
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                Insets systemBars = insets.getInsetsIgnoringVisibility(WindowInsets.Type.systemBars());
                statusBar = systemBars.top;
                navBar = systemBars.bottom;
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                statusBar = insets.getSystemWindowInsetTop();
                navBar = insets.getSystemWindowInsetBottom();
            }

            // Display cutout calculations
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                DisplayCutout cutout = insets.getDisplayCutout();
                if (cutout != null) {
                    cLeft = cutout.getSafeInsetLeft();
                    cTop = cutout.getSafeInsetTop();
                    cRight = cutout.getSafeInsetRight();
                    cBottom = cutout.getSafeInsetBottom();
                }
            }

            synchronized (cachedInsets) {
                cachedInsets[0] = statusBar;
                cachedInsets[1] = navBar;
                cachedInsets[2] = cLeft;
                cachedInsets[3] = cTop;
                cachedInsets[4] = cRight;
                cachedInsets[5] = cBottom;
            }

            insetsInitialized = true;
            nativeSetInsets(statusBar, navBar, cLeft, cTop, cRight, cBottom);

        } catch (Exception e) {
            Log.e(TAG, "Error structuralizing window layout properties: " + e.getMessage());
        }
    }

    /**
     * Called from native code via JNI to pull array memory safely.
     * * @return Array copy containing [status, nav, left, top, right, bottom]
     */
    public int[] getInsetsNative() {
        int[] result = new int[6];
        synchronized (cachedInsets) {
            System.arraycopy(cachedInsets, 0, result, 0, 6);
        }
        return result;
    }
}
