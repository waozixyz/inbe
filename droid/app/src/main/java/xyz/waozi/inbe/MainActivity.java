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
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.util.Base64;
import android.util.Log;
import android.view.DisplayCutout;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.Socket;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.net.URI;
import java.net.UnknownHostException;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import javax.net.ssl.SSLSocketFactory;

public class MainActivity extends NativeActivity {
    private static final String TAG = "InbeMainActivity";
    private static final int REQUEST_IMPORT_ZIP = 1001;
    private static final int REQUEST_POST_NOTIFICATIONS = 1002;
    private static final String DOWNLOAD_CHANNEL_ID = "runtime_downloads";
    private static final int DOWNLOAD_NOTIFICATION_ID = 2001;

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
    private int lastDeleteRepeatCount = -1;

    private native void nativeSetInsets(int status, int nav,
        int cutoutLeft, int cutoutTop, int cutoutRight, int cutoutBottom);
    private native void nativeSetSystemDark(int dark);
    private native void nativeSetOrientation(int orientation);
    private native void nativeWakeLockReady();
    private native void nativeSetBackgroundActive(boolean active);
    private native int nativeGetPlayInBackground();
    private native int nativePauseSession();
    private native void nativeResumeSession();
    private native void nativeImportSelectedFile(String path);
    private native void nativeImportCancelled();
    private native void nativeRuntimeAssetDownloadSucceeded(long handle, long bytes, int httpStatus);
    private native void nativeRuntimeAssetDownloadProgress(long handle, long bytes, long totalBytes);
    private native void nativeRuntimeAssetDownloadFailed(long handle, int httpStatus, String error);
    private native void nativeTextInputCommit(int codepoint);
    private native void nativeTextInputBackspace();
    private native void nativeTextInputEnter();
    private native void nativeInvalidateGraphicsResources();

    private void requestDownloadNotificationPermissionIfNeeded() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
                        checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                        requestPermissions(new String[] { Manifest.permission.POST_NOTIFICATIONS },
                                           REQUEST_POST_NOTIFICATIONS);
                    }
                }
            });
        }
    }

    private NotificationManager getDownloadNotificationManager() {
        requestDownloadNotificationPermissionIfNeeded();
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
                    connection.setRequestProperty("User-Agent", "flint-runtime-assets/1");
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
        HttpURLConnection connection = null;
        int status = 0;

        try {
            byte[] bodyBytes = body != null ? body.getBytes(StandardCharsets.UTF_8) : new byte[0];
            connection = (HttpURLConnection)new URL(urlText).openConnection();
            connection.setInstanceFollowRedirects(false);
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(30000);
            connection.setRequestMethod(method);
            connection.setRequestProperty("User-Agent", "inbe-sync/1");
            if (headers != null) {
                for (String header : headers) {
                    if (header == null) continue;
                    int colon = header.indexOf(':');
                    if (colon <= 0) continue;
                    String key = header.substring(0, colon).trim();
                    String value = header.substring(colon + 1).trim();
                    if (!key.isEmpty()) {
                        connection.setRequestProperty(key, value);
                    }
                }
            }
            if ("POST".equals(method) || "DELETE".equals(method)) {
                connection.setDoOutput(true);
                connection.setFixedLengthStreamingMode(bodyBytes.length);
                try (OutputStream output = connection.getOutputStream()) {
                    output.write(bodyBytes);
                }
            }

            status = connection.getResponseCode();
            InputStream stream = status >= 400 ? connection.getErrorStream() : connection.getInputStream();
            String response = "";
            if (stream != null) {
                try (InputStream input = stream) {
                    byte[] bytes = readAllBytesCompat(input);
                    response = new String(bytes, StandardCharsets.UTF_8);
                }
            }
            return status + "\n" + response;
        } catch (Exception e) {
            Log.e(TAG, "Sync HTTP request failed", e);
            return status + "\n" + (e.getMessage() != null ? e.getMessage() : "request failed");
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    public String syncWebSocketWait(String urlText, String[] headers) {
        Socket socket = null;

        try {
            URI url = new URI(urlText);
            String protocol = url.getScheme();
            boolean secure = "wss".equals(protocol);
            int port = url.getPort();
            String path = url.getRawPath();
            if (path == null || path.isEmpty()) path = "/";
            if (url.getRawQuery() != null && !url.getRawQuery().isEmpty()) {
                path += "?" + url.getRawQuery();
            }
            if (port <= 0) port = secure ? 443 : 80;
            if (!secure && !"ws".equals(protocol)) {
                return "0\ninvalid websocket url";
            }

            socket = secure
                ? SSLSocketFactory.getDefault().createSocket(url.getHost(), port)
                : new Socket(url.getHost(), port);
            socket.setTcpNoDelay(true);

            byte[] keyBytes = new byte[16];
            new SecureRandom().nextBytes(keyBytes);
            String key = Base64.encodeToString(keyBytes, Base64.NO_WRAP);

            StringBuilder request = new StringBuilder();
            request.append("GET ").append(path).append(" HTTP/1.1\r\n");
            request.append("Host: ").append(url.getHost());
            if ((secure && port != 443) || (!secure && port != 80)) {
                request.append(":").append(port);
            }
            request.append("\r\n");
            request.append("Upgrade: websocket\r\n");
            request.append("Connection: Upgrade\r\n");
            request.append("Sec-WebSocket-Version: 13\r\n");
            request.append("Sec-WebSocket-Key: ").append(key).append("\r\n");
            request.append("User-Agent: inbe-sync/1\r\n");
            if (headers != null) {
                for (String header : headers) {
                    if (header != null && !header.isEmpty()) {
                        request.append(header).append("\r\n");
                    }
                }
            }
            request.append("\r\n");
            socket.getOutputStream().write(request.toString().getBytes(StandardCharsets.US_ASCII));
            socket.getOutputStream().flush();

            String statusLine = readAsciiLine(socket);
            int status = parseHttpStatus(statusLine);
            while (true) {
                String line = readAsciiLine(socket);
                if (line == null || line.isEmpty()) break;
            }
            if (status != 101) {
                return status + "\nwebsocket upgrade failed";
            }
            Log.i(TAG, "Sync WebSocket connected");

            while (true) {
                String message = readWebSocketText(socket);
                if (message == null) {
                    return "0\nwebsocket closed";
                }
                if (message.contains("\"type\":\"sync_changed\"")) {
                    return "101\n" + message;
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "WebSocket wait failed", e);
            return "0\n" + (e.getMessage() != null ? e.getMessage() : "websocket failed");
        } finally {
            if (socket != null) {
                try {
                    socket.close();
                } catch (Exception ignored) {
                }
            }
        }
    }

    private static int parseHttpStatus(String statusLine) {
        if (statusLine == null) return 0;
        String[] parts = statusLine.split(" ", 3);
        if (parts.length < 2) return 0;
        try {
            return Integer.parseInt(parts[1]);
        } catch (NumberFormatException e) {
            return 0;
        }
    }

    private static String readAsciiLine(Socket socket) throws java.io.IOException {
        StringBuilder line = new StringBuilder();
        while (true) {
            int b = socket.getInputStream().read();
            if (b < 0) {
                return line.length() > 0 ? line.toString() : null;
            }
            if (b == '\n') break;
            if (b != '\r') line.append((char)b);
        }
        return line.toString();
    }

    private static String readWebSocketText(Socket socket) throws java.io.IOException {
        int b0 = socket.getInputStream().read();
        int b1 = socket.getInputStream().read();
        if (b0 < 0 || b1 < 0) return null;

        int opcode = b0 & 0x0f;
        boolean masked = (b1 & 0x80) != 0;
        long length = b1 & 0x7f;
        if (length == 126) {
            length = ((long)readByte(socket) << 8) | readByte(socket);
        } else if (length == 127) {
            length = 0;
            for (int i = 0; i < 8; i++) {
                length = (length << 8) | readByte(socket);
            }
        }
        if (length < 0 || length > 1024 * 1024) {
            throw new java.io.IOException("websocket frame too large");
        }

        byte[] mask = null;
        if (masked) {
            mask = readExact(socket, 4);
        }
        byte[] payload = readExact(socket, (int)length);
        if (masked) {
            for (int i = 0; i < payload.length; i++) {
                payload[i] = (byte)(payload[i] ^ mask[i % 4]);
            }
        }
        if (opcode == 8) return null;
        if (opcode != 1) return "";
        return new String(payload, StandardCharsets.UTF_8);
    }

    private static int readByte(Socket socket) throws java.io.IOException {
        int b = socket.getInputStream().read();
        if (b < 0) throw new java.io.IOException("unexpected eof");
        return b;
    }

    private static byte[] readExact(Socket socket, int count) throws java.io.IOException {
        byte[] data = new byte[count];
        int offset = 0;
        while (offset < count) {
            int read = socket.getInputStream().read(data, offset, count - offset);
            if (read < 0) throw new java.io.IOException("unexpected eof");
            offset += read;
        }
        return data;
    }

    private static byte[] readAllBytesCompat(InputStream input) throws java.io.IOException {
        byte[] buffer = new byte[8192];
        int read;
        java.io.ByteArrayOutputStream output = new java.io.ByteArrayOutputStream();
        while ((read = input.read(buffer)) != -1) {
            output.write(buffer, 0, read);
        }
        return output.toByteArray();
    }

    public void openImportPicker(final String mimeTypesCsv) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("*/*");
                    String[] mimeTypes = parseMimeTypes(mimeTypesCsv);
                    if (mimeTypes.length > 0) {
                        intent.putExtra(Intent.EXTRA_MIME_TYPES, mimeTypes);
                    }
                    startActivityForResult(intent, REQUEST_IMPORT_ZIP);
                } catch (Exception e) {
                    Log.e(TAG, "Failed to open import picker", e);
                    nativeImportCancelled();
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
        File importFile = new File(importDir, "inbe-import");

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
        pushDeviceConfiguration();

        // Notify native code that activity is ready for wake lock
        nativeWakeLockReady();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        nativeInvalidateGraphicsResources();
        pushDeviceConfiguration();
        requestInsetRefresh();
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
        nativeInvalidateGraphicsResources();
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
