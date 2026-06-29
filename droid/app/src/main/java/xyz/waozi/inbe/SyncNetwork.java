package xyz.waozi.inbe;

import android.util.Base64;
import android.util.Log;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.Socket;
import java.net.URI;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import javax.net.ssl.SSLSocketFactory;

final class SyncNetwork {
    private SyncNetwork() {
    }

    static String httpRequest(String logTag, String method, String urlText, String body,
                              String[] headers) {
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
            if (bodyBytes.length > 0) {
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
            Log.e(logTag, "Sync HTTP request failed", e);
            return status + "\n" + (e.getMessage() != null ? e.getMessage() : "request failed");
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    static String webSocketWait(String logTag, String urlText, String[] headers) {
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
            Log.i(logTag, "Sync WebSocket connected");

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
            Log.w(logTag, "WebSocket wait failed", e);
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
}
