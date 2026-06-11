package xyz.waozi.inbe;

import android.app.NativeActivity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Insets;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.util.Log;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;
import android.view.WindowManager;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public class MainActivity extends NativeActivity {
    private static final String TAG = "InbeMainActivity";
    private static final int REQUEST_IMPORT_ZIP = 1001;

    static {
        System.loadLibrary("main");
    }

    // [status_bar, nav_bar, cutout_left, cutout_top, cutout_right, cutout_bottom]
    private final int[] cachedInsets = new int[6];
    private boolean insetsInitialized = false;
    private PowerManager.WakeLock wakeLock = null;
    private boolean activityPaused = false;
    private boolean windowFocused = true;
    private boolean backgroundExecutionActive = false;
    private boolean autoPausedForLifecycle = false;

    private native void nativeSetInsets(int status, int nav,
        int cutoutLeft, int cutoutTop, int cutoutRight, int cutoutBottom);
    private native void nativeWakeLockReady();
    private native void nativeSetBackgroundActive(boolean active);
    private native int nativeGetPlayInBackground();
    private native int nativePauseSession();
    private native void nativeResumeSession();
    private native void nativeImportSelectedFile(String path);
    private native void nativeImportCancelled();

    public void openImportPicker() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("*/*");
                    intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {
                        "application/zip",
                        "application/octet-stream",
                        "application/x-zip-compressed"
                    });
                    startActivityForResult(intent, REQUEST_IMPORT_ZIP);
                } catch (Exception e) {
                    Log.e(TAG, "Failed to open import picker", e);
                    nativeImportCancelled();
                }
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode != REQUEST_IMPORT_ZIP) return;

        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            nativeImportCancelled();
            return;
        }

        Uri uri = data.getData();
        File importDir = new File(getCacheDir(), "imports");
        File importFile = new File(importDir, "inbe-import.zip");

        try {
            if (!importDir.exists() && !importDir.mkdirs()) {
                nativeImportSelectedFile("");
                return;
            }

            try (InputStream input = getContentResolver().openInputStream(uri);
                 FileOutputStream output = new FileOutputStream(importFile)) {
                if (input == null) {
                    nativeImportSelectedFile("");
                    return;
                }

                byte[] buffer = new byte[8192];
                int read;
                while ((read = input.read(buffer)) != -1) {
                    output.write(buffer, 0, read);
                }
            }

            nativeImportSelectedFile(importFile.getAbsolutePath());
        } catch (Exception e) {
            Log.e(TAG, "Failed to import selected file", e);
            nativeImportSelectedFile("");
        }
    }

    public void acquireWakeLock() {
        Log.d(TAG, "acquireWakeLock called - wakeLock=" + wakeLock);
        if (wakeLock == null) {
            Log.d(TAG, "Creating new PARTIAL_WAKE_LOCK");
            PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
            wakeLock = pm.newWakeLock(
                PowerManager.PARTIAL_WAKE_LOCK,
                "inbe:meditation_session"
            );
            wakeLock.acquire();
            Log.d(TAG, "Wake lock acquired successfully - held=" + wakeLock.isHeld());
        } else {
            Log.d(TAG, "Wake lock already exists - held=" + wakeLock.isHeld());
        }
    }

    public void releaseWakeLock() {
        Log.d(TAG, "releaseWakeLock called - wakeLock=" + wakeLock);
        if (wakeLock != null && wakeLock.isHeld()) {
            Log.d(TAG, "Releasing wake lock - was held=" + wakeLock.isHeld());
            wakeLock.release();
            Log.d(TAG, "Wake lock released successfully");
            wakeLock = null;
        } else {
            Log.d(TAG, "Wake lock not held - wakeLock=" + wakeLock + " isHeld=" + (wakeLock != null ? wakeLock.isHeld() : "null"));
        }
    }

    public void keepScreenOn() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                Log.d(TAG, "Screen lock timeout disabled for active session");
            }
        });
    }

    public void allowScreenOff() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                Log.d(TAG, "Screen lock timeout restored");
            }
        });
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        synchronized (cachedInsets) {
            for (int i = 0; i < 6; i++) {
                cachedInsets[i] = 0;
            }
        }

        setupInsetsListener();

        // Notify native code that activity is ready for wake lock
        nativeWakeLockReady();
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

    private void syncLifecycleState(String reason) {
        int playInBackground = nativeGetPlayInBackground();
        boolean shouldRunInBackground = playInBackground != 0 && (activityPaused || !windowFocused);

        Log.d(TAG, reason + ": play_in_background=" + playInBackground
            + " activityPaused=" + activityPaused
            + " windowFocused=" + windowFocused
            + " backgroundActive=" + backgroundExecutionActive);

        if (backgroundExecutionActive != shouldRunInBackground) {
            nativeSetBackgroundActive(shouldRunInBackground);
            backgroundExecutionActive = shouldRunInBackground;
        }

        if (playInBackground == 0) {
            if (activityPaused && !autoPausedForLifecycle) {
                autoPausedForLifecycle = nativePauseSession() != 0;
            } else if (!activityPaused && autoPausedForLifecycle) {
                nativeResumeSession();
                autoPausedForLifecycle = false;
            }
        } else if (autoPausedForLifecycle) {
            nativeResumeSession();
            autoPausedForLifecycle = false;
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        activityPaused = true;
        syncLifecycleState("onPause");
    }

    @Override
    protected void onResume() {
        super.onResume();
        activityPaused = false;
        syncLifecycleState("onResume");
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        windowFocused = hasFocus;
        syncLifecycleState("onWindowFocusChanged");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy called - releasing wake lock");
        allowScreenOff();
        releaseWakeLock();
    }
}
