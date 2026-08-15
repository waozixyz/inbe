package xyz.waozi.inbe;

import android.Manifest;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.NativeActivity;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.net.Uri;
import android.widget.Toast;
import android.app.AlertDialog;
import java.util.List;
import org.unifiedpush.android.connector.UnifiedPush;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.webkit.MimeTypeMap;
import android.view.DisplayCutout;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewTreeObserver;
import android.util.DisplayMetrics;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import androidx.core.view.WindowCompat;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.net.UnknownHostException;
import java.net.URL;
import java.util.Arrays;

public class MainActivity extends NativeActivity {
    private static final String TAG = "InbeMainActivity";
    private static final int REQUEST_IMPORT_ZIP = 1001;
    private static final int REQUEST_POST_NOTIFICATIONS = 1002;
    private static final String DOWNLOAD_CHANNEL_ID = "runtime_downloads";
    private static final int DOWNLOAD_NOTIFICATION_ID = 2001;

    static {
        System.loadLibrary("main");
    }

    // [status_bar, nav_bar, cutouts] mirrored in native.
    private final int[] cachedInsets = new int[6];
    private boolean activityPaused = false;
    private boolean windowFocused = true;
    private boolean backgroundExecutionActive = false;
    private boolean autoPausedForLifecycle = false;
    private boolean notificationPermissionRequestInFlight = false;
    private int lastDeleteRepeatCount = -1;
    private int pendingImportKind = 0;

    private native void nativeSetInsets(int status, int nav,
        int cutoutLeft, int cutoutTop, int cutoutRight, int cutoutBottom);
    private native void nativeSetDeviceDensity(float density);
    private native void nativeSetSystemDark(int dark);
    private native void nativeSetOrientation(int orientation);
    private native void nativeWakeLockReady();
    private native void nativeSetBackgroundActive(boolean active);
    private native int nativeGetPlayInBackground();
    private native int nativePauseSession();
    private native void nativeResumeSession();
    private native void nativeImportSelectedFile(int kind, String path);
    private native void nativeImportCancelled(int kind);
    private native void nativeRuntimeAssetDownloadSucceeded(long handle, long bytes, int httpStatus);
    private native void nativeRuntimeAssetDownloadProgress(long handle, long bytes, long totalBytes);
    private native void nativeRuntimeAssetDownloadFailed(long handle, int httpStatus, String error);
    private native void nativeTextInputCommit(int codepoint);
    private native void nativeTextInputBackspace();
    private native void nativeTextInputEnter();
    private native void nativeInvalidateGraphicsResources();

    private void requestNotificationPermissionIfNeeded() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED &&
            !notificationPermissionRequestInFlight) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
                        checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED &&
                        !notificationPermissionRequestInFlight) {
                        notificationPermissionRequestInFlight = true;
                        requestPermissions(new String[] { Manifest.permission.POST_NOTIFICATIONS },
                                           REQUEST_POST_NOTIFICATIONS);
                    }
                }
            });
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_POST_NOTIFICATIONS) {
            notificationPermissionRequestInFlight = false;
            boolean granted = grantResults.length > 0 &&
                grantResults[0] == PackageManager.PERMISSION_GRANTED;
            Log.d(TAG, "Notification permission result: granted=" + granted);
        }
    }

    private NotificationManager getDownloadNotificationManager() {
        requestNotificationPermissionIfNeeded();
        NotificationManager manager = (NotificationManager)getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                DOWNLOAD_CHANNEL_ID,
                "Downloads",
                NotificationManager.IMPORTANCE_LOW
            );
            manager.createNotificationChannel(channel);
        }
        return manager;
    }

    private void showDownloadNotification(NotificationManager manager, long written, long total) {
        if (manager == null) return;

        Notification.Builder builder = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
            ? new Notification.Builder(this, DOWNLOAD_CHANNEL_ID)
            : new Notification.Builder(this);
        builder.setSmallIcon(android.R.drawable.stat_sys_download)
            .setContentTitle("Downloading audio")
            .setOngoing(true)
            .setOnlyAlertOnce(true);
        if (total > 0) {
            int progress = (int)Math.max(0, Math.min(100, (written * 100) / total));
            builder.setProgress(100, progress, false)
                .setContentText(progress + "%");
        } else {
            builder.setProgress(0, 0, true);
        }
        manager.notify(DOWNLOAD_NOTIFICATION_ID, builder.build());
    }

    private void clearDownloadNotification(NotificationManager manager) {
        if (manager != null) {
            manager.cancel(DOWNLOAD_NOTIFICATION_ID);
        }
    }

    public void setSoftKeyboardVisible(final boolean visible) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                InputMethodManager imm = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
                View view = getWindow() != null ? getWindow().getDecorView() : null;
                if (imm == null || view == null) return;

                if (visible) {
                    view.requestFocus();
                    imm.showSoftInput(view, InputMethodManager.SHOW_FORCED);
                } else {
                    imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
                }
            }
        });
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event != null && event.getAction() == KeyEvent.ACTION_DOWN) {
            int keyCode = event.getKeyCode();
            if (keyCode == KeyEvent.KEYCODE_DEL) {
                int repeatCount = event.getRepeatCount();
                int deleteCount = lastDeleteRepeatCount < 0
                    ? 1
                    : Math.max(1, repeatCount - lastDeleteRepeatCount);
                lastDeleteRepeatCount = repeatCount;
                for (int i = 0; i < deleteCount; i++) {
                    nativeTextInputBackspace();
                }
            } else if (keyCode == KeyEvent.KEYCODE_ENTER ||
                       keyCode == KeyEvent.KEYCODE_NUMPAD_ENTER) {
                lastDeleteRepeatCount = -1;
                nativeTextInputEnter();
            } else {
                lastDeleteRepeatCount = -1;
                int unicode = event.getUnicodeChar();
                if (unicode >= 32) {
                    nativeTextInputCommit(unicode);
                }
            }
        } else if (event != null && event.getAction() == KeyEvent.ACTION_UP) {
            if (event.getKeyCode() == KeyEvent.KEYCODE_DEL) {
                lastDeleteRepeatCount = -1;
            }
        } else if (event != null && event.getAction() == KeyEvent.ACTION_MULTIPLE &&
                   event.getCharacters() != null) {
            lastDeleteRepeatCount = -1;
            String chars = event.getCharacters();
            for (int i = 0; i < chars.length();) {
                int codepoint = chars.codePointAt(i);
                if (codepoint >= 32) {
                    nativeTextInputCommit(codepoint);
                }
                i += Character.charCount(codepoint);
            }
        }
        return super.dispatchKeyEvent(event);
    }

    public void applyOrientationMode(final int mode) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                int requested = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
                switch (mode) {
                    case 1:
                        requested = ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT;
                        break;
                    case 2:
                        requested = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
                        break;
                    case 3:
                        requested = ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;
                        break;
                    default:
                        requested = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED;
                        break;
                }
                setRequestedOrientation(requested);
                pushDeviceConfiguration();
                requestInsetRefresh();
            }
        });
    }

    public void startRuntimeAssetDownload(final String url, final String path, final long handle) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                HttpURLConnection connection = null;
                int status = 0;
                long written = 0;
                long total = 0;
                long lastNotify = 0;
                File outputFile = new File(path);
                File parent = outputFile.getParentFile();
                NotificationManager notificationManager = getDownloadNotificationManager();

                try {
                    if (parent != null && !parent.exists() && !parent.mkdirs()) {
                        nativeRuntimeAssetDownloadFailed(handle, status, "failed to create download directory");
                        return;
                    }

                    connection = (HttpURLConnection) new URL(url).openConnection();
                    connection.setInstanceFollowRedirects(true);
                    connection.setConnectTimeout(15000);
                    connection.setReadTimeout(30000);
                    connection.setRequestProperty("User-Agent", "kryon-runtime-assets/1");
                    status = connection.getResponseCode();

                    if (status < 200 || status >= 300) {
                        outputFile.delete();
                        nativeRuntimeAssetDownloadFailed(handle, status, "HTTP " + status);
                        return;
                    }

                    total = connection.getContentLengthLong();
                    nativeRuntimeAssetDownloadProgress(handle, written, total);
                    showDownloadNotification(notificationManager, written, total);

                    try (InputStream input = connection.getInputStream();
                         FileOutputStream output = new FileOutputStream(outputFile)) {
                        byte[] buffer = new byte[32768];
                        int read;
                        while ((read = input.read(buffer)) != -1) {
                            output.write(buffer, 0, read);
                            written += read;
                            nativeRuntimeAssetDownloadProgress(handle, written, total);
                            long now = android.os.SystemClock.uptimeMillis();
                            if (now - lastNotify > 250 || (total > 0 && written >= total)) {
                                showDownloadNotification(notificationManager, written, total);
                                lastNotify = now;
                            }
                        }
                    }

                    nativeRuntimeAssetDownloadSucceeded(handle, written, status);
                } catch (UnknownHostException | SocketException | SocketTimeoutException e) {
                    outputFile.delete();
                    nativeRuntimeAssetDownloadFailed(handle, status, "NETWORK_UNAVAILABLE");
                } catch (Exception e) {
                    outputFile.delete();
                    nativeRuntimeAssetDownloadFailed(handle, status, e.getMessage());
                } finally {
                    clearDownloadNotification(notificationManager);
                    if (connection != null) {
                        connection.disconnect();
                    }
                }
            }
        }, "inbe-runtime-asset-download").start();
    }

    public String syncHttpRequest(String method, String urlText, String body, String[] headers) {
        return SyncNetwork.httpRequest(TAG, method, urlText, body, headers);
    }

    public String syncWebSocketWait(String urlText, String[] headers) {
        return SyncNetwork.webSocketWait(TAG, urlText, headers);
    }

    public void openImportPicker(final String mimeTypesCsv, final int kind) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    String[] mimeTypes = parseMimeTypes(mimeTypesCsv);
                    intent.setType(primaryMimeType(mimeTypes));
                    if (mimeTypes.length > 0) {
                        intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
                    }
                    pendingImportKind = kind;
                    Log.d(TAG, "Opening import picker - kind=" + kind + ", mimeTypes=" + mimeTypesCsv + ", parsed=" + Arrays.toString(mimeTypes));
                    startActivityForResult(intent, REQUEST_IMPORT_ZIP);
                } catch (Exception e) {
                    Log.e(TAG, "Failed to open import picker", e);
                    nativeImportCancelled(kind);
                }
            }
        });
    }

    private static String[] parseMimeTypes(String mimeTypesCsv) {
        if (mimeTypesCsv == null || mimeTypesCsv.trim().isEmpty()) {
            return new String[0];
        }

        String[] raw = mimeTypesCsv.split(",");
        java.util.ArrayList<String> result = new java.util.ArrayList<>();
        for (String mimeType : raw) {
            String trimmed = mimeType.trim();
            if (!trimmed.isEmpty()) {
                result.add(trimmed);
            }
        }
        return result.toArray(new String[0]);
    }

    private static String primaryMimeType(String[] mimeTypes) {
        if (mimeTypes == null || mimeTypes.length == 0) {
            return "*/*";
        }
        for (String mimeType : mimeTypes) {
            if (mimeType != null && mimeType.endsWith("/*")) {
                return mimeType;
            }
        }
        return mimeTypes[0] != null && !mimeTypes[0].isEmpty()
            ? mimeTypes[0]
            : "*/*";
    }

    private String extensionForUri(Uri uri) {
        if (uri == null) return null;

        // Prefer the MIME type reported by the content resolver; it is the most
        // reliable signal for content:// URIs and maps unambiguously to an ext.
        String mimeType = null;
        try {
            mimeType = getContentResolver().getType(uri);
        } catch (Exception e) {
            Log.w(TAG, "Failed to resolve MIME type for uri=" + uri, e);
        }
        String ext = mimeType != null ? extensionForMimeType(mimeType) : null;
        if (ext != null && !ext.isEmpty()) return ext;

        // Fall back to whatever extension the URI itself exposes. For Media Store
        // documents this is usually absent, but it covers file:// and some providers.
        String uriExt = MimeTypeMap.getFileExtensionFromUrl(uri.getLastPathSegment());
        return (uriExt != null && !uriExt.isEmpty()) ? uriExt : null;
    }

    private static String extensionForMimeType(String mimeType) {
        if (mimeType == null) return null;
        String ext = MimeTypeMap.getSingleton().getExtensionFromMimeType(mimeType);
        if (ext != null && !ext.isEmpty()) return ext;

        // Handle audio types the system map does not always resolve.
        switch (mimeType) {
            case "audio/mpeg":
            case "audio/mp3":
                return "mp3";
            case "audio/ogg":
            case "application/ogg":
                return "ogg";
            case "audio/flac":
                return "flac";
            case "audio/mp4":
            case "audio/x-m4a":
            case "audio/m4a":
                return "m4a";
            case "audio/opus":
            case "audio/x-opus+ogg":
                return "opus";
            case "audio/x-wav":
            case "audio/wav":
                return "wav";
            default:
                return null;
        }
    }

    private String displayNameForUri(Uri uri) {
        if (uri == null) return null;

        // Query the provider's DISPLAY_NAME column; this is the canonical way to
        // get the original filesystem filename for a content:// URI.
        try (android.database.Cursor cursor = getContentResolver().query(
                uri, new String[]{android.provider.OpenableColumns.DISPLAY_NAME},
                null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int idx = cursor.getColumnIndex(android.provider.OpenableColumns.DISPLAY_NAME);
                if (idx >= 0) {
                    String name = cursor.getString(idx);
                    if (name != null && !name.isEmpty()) return name;
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "Failed to query display name for uri=" + uri, e);
        }

        // Fall back to the URI's last path segment (useful for file:// URIs).
        String last = uri.getLastPathSegment();
        return (last != null && !last.isEmpty()) ? last : null;
    }

    private static String sanitizeFileName(String name) {
        if (name == null || name.isEmpty()) return null;
        // Strip path separators / parent refs so a crafted name can't escape the
        // import directory, and trim surrounding whitespace.
        String cleaned = name.replaceAll("[\\\\/]", "_").trim();
        return cleaned.isEmpty() ? null : cleaned;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode != REQUEST_IMPORT_ZIP) return;

        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            Log.d(TAG, "Import cancelled - kind=" + pendingImportKind + ", resultCode=" + resultCode + ", data=" + (data != null));
            nativeImportCancelled(pendingImportKind);
            return;
        }

        Uri uri = data.getData();
        Log.d(TAG, "File selected for import - kind=" + pendingImportKind + ", uri=" + uri);

        File importDir = new File(getCacheDir(), "imports");
        String extension = extensionForUri(uri);
        String displayName = sanitizeFileName(displayNameForUri(uri));
        String importName;
        if (displayName != null) {
            // Use the real filesystem name. If it already carries the right
            // extension (per MIME type) keep it; otherwise append the MIME-derived
            // extension so native validation accepts it.
            boolean hasExt = displayName.lastIndexOf('.') > 0
                    && extension != null
                    && !extension.isEmpty()
                    && displayName.toLowerCase().endsWith("." + extension.toLowerCase());
            importName = hasExt ? displayName : displayName + "."
                    + (extension != null && !extension.isEmpty() ? extension : "bin");
        } else {
            importName = "inbe-import-" + pendingImportKind
                    + (extension != null && !extension.isEmpty() ? "." + extension : "");
        }
        File importFile = new File(importDir, importName);

        try {
            if (!importDir.exists() && !importDir.mkdirs()) {
                Log.e(TAG, "Failed to create import directory");
                nativeImportSelectedFile(pendingImportKind, "");
                return;
            }

            try (InputStream input = getContentResolver().openInputStream(uri);
                 FileOutputStream output = new FileOutputStream(importFile)) {
                if (input == null) {
                    Log.e(TAG, "Failed to open input stream for uri=" + uri);
                    nativeImportSelectedFile(pendingImportKind, "");
                    return;
                }

                byte[] buffer = new byte[8192];
                int read;
                long totalBytes = 0;
                while ((read = input.read(buffer)) != -1) {
                    output.write(buffer, 0, read);
                    totalBytes += read;
                }
                Log.d(TAG, "File copied successfully - kind=" + pendingImportKind + ", path=" + importFile.getAbsolutePath() + ", size=" + totalBytes + " bytes");
            }

            String mimeType = getContentResolver().getType(uri);
            Log.d(TAG, "Calling nativeImportSelectedFile - kind=" + pendingImportKind + ", path=" + importFile.getAbsolutePath() + ", mimeType=" + mimeType);
            nativeImportSelectedFile(pendingImportKind, importFile.getAbsolutePath());
        } catch (Exception e) {
            Log.e(TAG, "Failed to import selected file", e);
            nativeImportSelectedFile(pendingImportKind, "");
        }
    }

    /**
     * Called from native settings: set up UnifiedPush. With no distributor
     * installed, offer Sunup on F-Droid; with several, let the user pick.
     */
    public boolean isUnifiedPushRegistered() {
        return PushServiceImpl.getEndpoint(this) != null;
    }

    public void configureUnifiedPush() {
        requestNotificationPermissionIfNeeded();
        final List<String> distributors = UnifiedPush.getDistributors(this);
        if (distributors.isEmpty()) {
            Toast.makeText(this,
                    "Install a UnifiedPush distributor (e.g. Sunup)",
                    Toast.LENGTH_LONG).show();
            try {
                startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(
                        "https://f-droid.org/en/packages/org.unifiedpush.distributor.sunup/")));
            } catch (Exception e) {
                Log.w(TAG, "no activity to open F-Droid");
            }
            return;
        }
        if (distributors.size() == 1) {
            UnifiedPush.saveDistributor(this, distributors.get(0));
            UnifiedPush.register(this, "inbe", "Inner Breeze", null);
            Toast.makeText(this,
                    "Push: registering with " + distributors.get(0),
                    Toast.LENGTH_SHORT).show();
            return;
        }
        final String[] names = distributors.toArray(new String[0]);
        new AlertDialog.Builder(this)
                .setTitle("Push provider")
                .setItems(names, (dialog, which) -> {
                    UnifiedPush.saveDistributor(this, names[which]);
                    UnifiedPush.register(this, "inbe", "Inner Breeze", null);
                    Toast.makeText(this,
                            "Push: registering with " + names[which],
                            Toast.LENGTH_SHORT).show();
                })
                .show();
    }

    public void acquireWakeLock() {
        Log.d(TAG, "Starting session foreground service");
        requestNotificationPermissionIfNeeded();
        Intent intent = new Intent(this, SessionForegroundService.class);
        intent.setAction(SessionForegroundService.ACTION_START);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(intent);
        } else {
            startService(intent);
        }
    }

    public void releaseWakeLock() {
        Log.d(TAG, "Stopping session foreground service");
        Intent intent = new Intent(this, SessionForegroundService.class);
        intent.setAction(SessionForegroundService.ACTION_STOP);
        startService(intent);
    }

    public void updateSessionNotification(final String statusText) {
        SessionForegroundService.updateStatus(statusText == null ? "" : statusText);
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
        configureSystemBars();
        super.onCreate(savedInstanceState);
        configureSystemBars();

        synchronized (cachedInsets) {
            for (int i = 0; i < 6; i++) {
                cachedInsets[i] = 0;
            }
        }

        setupInsetsListener();
        pushDeviceConfiguration();

        // Notify native code that activity is ready for wake lock
        nativeWakeLockReady();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        nativeInvalidateGraphicsResources();
        configureSystemBars();
        pushDeviceConfiguration();
        requestInsetRefresh();
    }

    private void configureSystemBars() {
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);

        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams attrs = getWindow().getAttributes();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                attrs.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
            } else {
                attrs.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            }
            getWindow().setAttributes(attrs);
        }

        int flags = getWindow().getDecorView().getSystemUiVisibility();
        flags |= View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
        flags |= View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
        flags |= View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
        flags &= ~View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
        flags &= ~View.SYSTEM_UI_FLAG_FULLSCREEN;
        flags &= ~View.SYSTEM_UI_FLAG_IMMERSIVE;
        flags &= ~View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            flags &= ~View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
        }
        getWindow().getDecorView().setSystemUiVisibility(flags);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars());
                controller.show(WindowInsets.Type.navigationBars());
            }
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q &&
            Build.VERSION.SDK_INT < Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            getWindow().setNavigationBarContrastEnforced(false);
        }
    }

    private void pushDeviceConfiguration() {
        Configuration config = getResources().getConfiguration();
        int nightMask = config.uiMode & Configuration.UI_MODE_NIGHT_MASK;
        int dark = nightMask == Configuration.UI_MODE_NIGHT_YES ? 1 : 0;
        int orientation = 0;

        if (config.orientation == Configuration.ORIENTATION_PORTRAIT) {
            orientation = 1;
        } else if (config.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            orientation = 2;
        }

        nativeSetSystemDark(dark);
        nativeSetOrientation(orientation);
    }

    private void requestInsetRefresh() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            final View decorView = getWindow().getDecorView();
            decorView.post(new Runnable() {
                @Override
                public void run() {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                        decorView.requestApplyInsets();
                    }
                }
            });
        }
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
        }
    }

    private void updateInsets(WindowInsets insets) {
        if (insets == null) return;

        try {
            int statusBar = 0;
            int navBar = 0;
            int cLeft = 0, cTop = 0, cRight = 0, cBottom = 0;

            // Inbe owns a single edge-to-edge native surface. Java reports the
            // system bar insets; native applies them once against the real GL surface.
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

            nativeSetInsets(statusBar, navBar, cLeft, cTop, cRight, cBottom);

            // Set device density for proper DPI scaling
            DisplayMetrics metrics = new DisplayMetrics();
            getWindowManager().getDefaultDisplay().getMetrics(metrics);
            nativeSetDeviceDensity(metrics.density);

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
        boolean shouldRunInBackground = playInBackground != 0 && activityPaused;

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
        configureSystemBars();
        nativeInvalidateGraphicsResources();
        requestInsetRefresh();
        syncLifecycleState("onResume");
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        configureSystemBars();
        requestInsetRefresh();
        syncLifecycleState("onNewIntent");
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        windowFocused = hasFocus;
        if (hasFocus) {
            configureSystemBars();
            requestInsetRefresh();
        }
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
