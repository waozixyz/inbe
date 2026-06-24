#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define MMNOSOUND
#define NOMINMAX
#include <windows.h>
#endif

#include "sync_client.h"
#include "platform.h"

#include "storage.h"
#include "sync_account.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if ANDROID_BUILD
#include <android_native_app_glue.h>
#include <jni.h>
extern struct android_app *GetAndroidApp(void);
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#include <curl/curl.h>
#if LIBCURL_VERSION_NUM < 0x075600
#error "Inbe sync requires libcurl 7.86.0 or newer with websocket support"
#endif
#endif

#define INBE_SYNC_PATH "/api/v1/sync"
#define INBE_SYNC_WS_PATH "/api/v1/sync/ws"
#define INBE_CHALLENGE_PATH "/api/v1/sync/challenge"
#define INBE_LOGIN_PATH "/api/v1/sync/login"
#define INBE_ACCOUNT_ALIAS_PATH "/api/v1/account/alias"
#define INBE_ACCOUNT_DELETE_WITH_KEY_PATH "/api/v1/account/delete-with-key"
#define INBE_SYNC_WEB_RESPONSE_MAX (4 * 1024 * 1024)
#define INBE_SYNC_AUTH_TOKEN_KEY "sync_auth_token"
#define INBE_SYNC_AUTH_TOKEN_EXPIRES_KEY "sync_auth_token_expires_at"

typedef struct SyncBuffer {
    char *data;
    size_t len;
    size_t cap;
} SyncBuffer;

const char *
sync_client_result_name(InbeSyncClientResult result)
{
    switch(result) {
        case INBE_SYNC_CLIENT_OK:
            return "ok";
        case INBE_SYNC_CLIENT_INVALID_URL:
            return "invalid_url";
        case INBE_SYNC_CLIENT_NO_ACCOUNT:
            return "no_account";
        case INBE_SYNC_CLIENT_PAYLOAD_FAILED:
            return "payload_failed";
        case INBE_SYNC_CLIENT_CHALLENGE_FAILED:
            return "challenge_failed";
        case INBE_SYNC_CLIENT_SIGN_FAILED:
            return "sign_failed";
        case INBE_SYNC_CLIENT_REQUEST_FAILED:
            return "request_failed";
        case INBE_SYNC_CLIENT_AUTH_FAILED:
            return "auth_failed";
        default:
            return "unknown";
    }
}

static void
sync_log_http_failure(const char *step, long status, const char *response)
{
    char snippet[161];
    size_t len;

    if(response == NULL)
        response = "";
    len = strlen(response);
    if(len >= sizeof(snippet))
        len = sizeof(snippet) - 1;
    memcpy(snippet, response, len);
    snippet[len] = '\0';
    for(size_t i = 0; i < len; i++) {
        if(snippet[i] == '\n' || snippet[i] == '\r' || snippet[i] == '\t')
            snippet[i] = ' ';
    }
    TraceLog(LOG_WARNING, "SYNC: %s failed status=%ld response=%s", step, status, snippet);
}

static InbeSyncClientResult sync_login(const char *base_url, const InbeSyncAccount *account);
static int sync_load_valid_auth_token(char *out, size_t out_size);

static int
sync_has_prefix(const char *text, const char *prefix)
{
    return text != NULL && prefix != NULL &&
           strncmp(text, prefix, strlen(prefix)) == 0;
}

static int
sync_url_host_boundary(char ch)
{
    return ch == '\0' || ch == ':' || ch == '/' || ch == '?' || ch == '#';
}

static int
sync_loopback_authority_valid(const char *authority)
{
    static const char *const hosts[] = {
        "localhost",
        "127.0.0.1",
        "10.0.2.2"
    };

    if(authority == NULL || authority[0] == '\0')
        return 0;
    for(size_t i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++) {
        size_t len = strlen(hosts[i]);
        if(strncmp(authority, hosts[i], len) == 0 &&
           sync_url_host_boundary(authority[len]))
            return 1;
    }
    return 0;
}

int
sync_client_url_valid(const char *url)
{
    if(url == NULL || url[0] == '\0')
        return 0;
    if(sync_has_prefix(url, "https://"))
        return url[8] != '\0';
    if(sync_has_prefix(url, "http://"))
        return sync_loopback_authority_valid(url + 7);
    return sync_loopback_authority_valid(url);
}

int
sync_client_normalize_url(const char *input, char *out, size_t out_size)
{
    int len;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(!sync_client_url_valid(input))
        return 0;
    if(sync_has_prefix(input, "https://") || sync_has_prefix(input, "http://"))
        len = snprintf(out, out_size, "%s", input);
    else
        len = snprintf(out, out_size, "http://%s", input);
    return len > 0 && (size_t)len < out_size;
}

static int
sync_buffer_append(SyncBuffer *buffer, const void *data, size_t bytes)
{
    char *next;
    size_t next_cap;

    if(buffer == NULL || data == NULL || bytes == 0)
        return 1;
    if(buffer->cap == 0 || bytes >= buffer->cap - buffer->len) {
        next_cap = buffer->cap > 0 ? buffer->cap : 1024;
        while(bytes >= next_cap - buffer->len)
            next_cap *= 2;
        next = (char *)realloc(buffer->data, next_cap);
        if(next == NULL)
            return 0;
        buffer->data = next;
        buffer->cap = next_cap;
    }
    memcpy(buffer->data + buffer->len, data, bytes);
    buffer->len += bytes;
    buffer->data[buffer->len] = '\0';
    return 1;
}

static int
sync_buffer_append_json_string(SyncBuffer *buffer, const char *text)
{
    const char *p;

    if(!sync_buffer_append(buffer, "\"", 1))
        return 0;
    if(text == NULL)
        text = "";
    for(p = text; *p != '\0'; p++) {
        char escaped[2];
        switch(*p) {
            case '\\':
                if(!sync_buffer_append(buffer, "\\\\", 2))
                    return 0;
                break;
            case '"':
                if(!sync_buffer_append(buffer, "\\\"", 2))
                    return 0;
                break;
            case '\n':
                if(!sync_buffer_append(buffer, "\\n", 2))
                    return 0;
                break;
            case '\r':
                if(!sync_buffer_append(buffer, "\\r", 2))
                    return 0;
                break;
            case '\t':
                if(!sync_buffer_append(buffer, "\\t", 2))
                    return 0;
                break;
            default:
                escaped[0] = *p;
                escaped[1] = '\0';
                if(!sync_buffer_append(buffer, escaped, 1))
                    return 0;
                break;
        }
    }
    return sync_buffer_append(buffer, "\"", 1);
}

#if defined(INBE_SYNC_CLIENT_TESTS)
int
sync_client_test_response_buffer(const char *first, const char *second,
                                      char *out, size_t out_size)
{
    SyncBuffer buffer = {0};
    int ok;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    ok = sync_buffer_append(&buffer, first, first != NULL ? strlen(first) : 0) &&
         sync_buffer_append(&buffer, second, second != NULL ? strlen(second) : 0);
    if(ok && buffer.data != NULL)
        snprintf(out, out_size, "%s", buffer.data);
    free(buffer.data);
    return ok;
}
#endif

static void
sync_join_url(char *out, size_t out_size, const char *base_url, const char *path)
{
    size_t len;

    if(out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    if(base_url == NULL || path == NULL)
        return;
    len = strlen(base_url);
    while(len > 0 && base_url[len - 1] == '/')
        len--;
    snprintf(out, out_size, "%.*s%s", (int)len, base_url, path);
}

static int
sync_join_ws_url(char *out, size_t out_size, const char *base_url, const char *path)
{
    char http_url[768];
    const char *body;
    const char *scheme;
    int len;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    sync_join_url(http_url, sizeof(http_url), base_url, path);
    if(sync_has_prefix(http_url, "https://")) {
        scheme = "wss://";
        body = http_url + 8;
    } else if(sync_has_prefix(http_url, "http://")) {
        scheme = "ws://";
        body = http_url + 7;
    } else {
        return 0;
    }
    len = snprintf(out, out_size, "%s%s", scheme, body);
    return len > 0 && (size_t)len < out_size;
}

static int
sync_build_message(const char *method, const char *path, const char *nonce_hex,
                   const char *body, char *out, size_t out_size)
{
    char body_hash[65];
    int len;

    if(method == NULL || path == NULL || nonce_hex == NULL || body == NULL ||
       out == NULL || out_size == 0)
        return 0;
    sync_sha256_hex((const uint8_t *)body, strlen(body), body_hash);
    if(body_hash[0] == '\0')
        return 0;
    len = snprintf(out, out_size, "inbe-sync-v1\n%s\n%s\n%s\n%s\n",
                   method, path, body_hash, nonce_hex);
    return len > 0 && (size_t)len < out_size;
}

static int
sync_find_json_string(const char *json, const char *key, char *out, size_t out_size)
{
    char pattern[64];
    const char *p;
    char *w;
    size_t remaining;

    if(json == NULL || key == NULL || out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if(p == NULL)
        return 0;
    p += strlen(pattern);
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    if(*p++ != ':')
        return 0;
    while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    if(*p++ != '"')
        return 0;
    w = out;
    remaining = out_size - 1;
    while(*p != '\0' && *p != '"' && remaining > 0) {
        if(*p == '\\')
            return 0;
        *w++ = *p++;
        remaining--;
    }
    *w = '\0';
    return *p == '"' && out[0] != '\0';
}

static long long
sync_find_json_int64(const char *json, const char *key, long long fallback)
{
    const char *p;
    char pattern[64];

    if(json == NULL || key == NULL)
        return fallback;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if(p == NULL)
        return fallback;
    p = strchr(p + strlen(pattern), ':');
    if(p == NULL)
        return fallback;
    p++;
    while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return strtoll(p, NULL, 10);
}

#if !ANDROID_BUILD && !defined(__EMSCRIPTEN__)
static size_t
sync_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    SyncBuffer *buffer = (SyncBuffer *)userdata;
    size_t bytes = size * nmemb;

    if(buffer == NULL || bytes == 0)
        return bytes;
    return sync_buffer_append(buffer, ptr, bytes) ? bytes : 0;
}

static int
sync_http_request(const char *method, const char *url, const char *body,
                  const char *const *headers, int header_count,
                  SyncBuffer *response, long *status)
{
    CURL *curl;
    CURLcode res;
    struct curl_slist *curl_headers = NULL;

    if(status != NULL)
        *status = 0;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl == NULL)
        return 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "inbe-sync/1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sync_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    for(int i = 0; i < header_count; i++) {
        if(headers[i] != NULL)
            curl_headers = curl_slist_append(curl_headers, headers[i]);
    }
    if(curl_headers != NULL)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
    if(strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body != NULL ? body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)(body != NULL ? strlen(body) : 0));
    } else if(strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body != NULL ? body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)(body != NULL ? strlen(body) : 0));
    }
    res = curl_easy_perform(curl);
    if(status != NULL)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, status);
    curl_slist_free_all(curl_headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}
#elif ANDROID_BUILD
static int
sync_buffer_set(SyncBuffer *response, const char *text)
{
    size_t len;

    if(response == NULL)
        return 0;
    len = text != NULL ? strlen(text) : 0;
    response->data = (char *)malloc(len + 1);
    if(response->data == NULL)
        return 0;
    if(len > 0)
        memcpy(response->data, text, len);
    response->data[len] = '\0';
    response->len = len;
    response->cap = len + 1;
    return 1;
}

static int
sync_http_request(const char *method, const char *url, const char *body,
                  const char *const *headers, int header_count,
                  SyncBuffer *response, long *status)
{
    struct android_app *app = GetAndroidApp();
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jmethodID method_id;
    jstring jmethod = NULL;
    jstring jurl = NULL;
    jstring jbody = NULL;
    jobjectArray jheaders = NULL;
    jstring result = NULL;
    const char *result_text = NULL;
    const char *newline;
    int attached = 0;
    int ok = 0;

    if(status != NULL)
        *status = 0;
    if(app == NULL || app->activity == NULL || app->activity->vm == NULL ||
       app->activity->clazz == NULL || method == NULL || url == NULL)
        return 0;

    jvm = app->activity->vm;
    activity = app->activity->clazz;
    if((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK || env == NULL)
            return 0;
        attached = 1;
    }

    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method_id = (*env)->GetMethodID(env, activity_class, "syncHttpRequest",
                                    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;)Ljava/lang/String;");
    if(method_id == NULL)
        goto done;

    jmethod = (*env)->NewStringUTF(env, method);
    jurl = (*env)->NewStringUTF(env, url);
    jbody = (*env)->NewStringUTF(env, body != NULL ? body : "");
    jclass string_class = (*env)->FindClass(env, "java/lang/String");
    if(jmethod == NULL || jurl == NULL || jbody == NULL || string_class == NULL)
        goto done;
    jheaders = (*env)->NewObjectArray(env, header_count, string_class, NULL);
    if(jheaders == NULL)
        goto done;
    for(int i = 0; i < header_count; i++) {
        jstring value = (*env)->NewStringUTF(env, headers[i] != NULL ? headers[i] : "");
        if(value == NULL)
            goto done;
        (*env)->SetObjectArrayElement(env, jheaders, i, value);
        (*env)->DeleteLocalRef(env, value);
    }

    result = (jstring)(*env)->CallObjectMethod(env, activity, method_id,
                                              jmethod, jurl, jbody, jheaders);
    if((*env)->ExceptionCheck(env) || result == NULL)
        goto done;
    result_text = (*env)->GetStringUTFChars(env, result, NULL);
    if(result_text == NULL)
        goto done;
    newline = strchr(result_text, '\n');
    if(newline == NULL)
        goto done;
    if(status != NULL)
        *status = strtol(result_text, NULL, 10);
    ok = sync_buffer_set(response, newline + 1);

done:
    if(result_text != NULL && result != NULL)
        (*env)->ReleaseStringUTFChars(env, result, result_text);
    if(result != NULL)
        (*env)->DeleteLocalRef(env, result);
    if(jheaders != NULL)
        (*env)->DeleteLocalRef(env, jheaders);
    if(jbody != NULL)
        (*env)->DeleteLocalRef(env, jbody);
    if(jurl != NULL)
        (*env)->DeleteLocalRef(env, jurl);
    if(jmethod != NULL)
        (*env)->DeleteLocalRef(env, jmethod);
    if((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
    if(attached)
        (*jvm)->DetachCurrentThread(jvm);
    return ok;
}

static int
sync_android_websocket_wait(const char *url, const char *const *headers, int header_count,
                            SyncBuffer *response, long *status)
{
    struct android_app *app = GetAndroidApp();
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jclass string_class = NULL;
    jmethodID method_id;
    jstring jurl = NULL;
    jobjectArray jheaders = NULL;
    jstring result = NULL;
    const char *result_text = NULL;
    const char *newline;
    int attached = 0;
    int ok = 0;

    if(status != NULL)
        *status = 0;
    if(app == NULL || app->activity == NULL || app->activity->vm == NULL ||
       app->activity->clazz == NULL || url == NULL)
        return 0;

    jvm = app->activity->vm;
    activity = app->activity->clazz;
    if((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK || env == NULL)
            return 0;
        attached = 1;
    }

    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    method_id = (*env)->GetMethodID(env, activity_class, "syncWebSocketWait",
                                    "(Ljava/lang/String;[Ljava/lang/String;)Ljava/lang/String;");
    if(method_id == NULL)
        goto done;

    jurl = (*env)->NewStringUTF(env, url);
    string_class = (*env)->FindClass(env, "java/lang/String");
    if(jurl == NULL || string_class == NULL)
        goto done;
    jheaders = (*env)->NewObjectArray(env, header_count, string_class, NULL);
    if(jheaders == NULL)
        goto done;
    for(int i = 0; i < header_count; i++) {
        jstring value = (*env)->NewStringUTF(env, headers[i] != NULL ? headers[i] : "");
        if(value == NULL)
            goto done;
        (*env)->SetObjectArrayElement(env, jheaders, i, value);
        (*env)->DeleteLocalRef(env, value);
    }

    result = (jstring)(*env)->CallObjectMethod(env, activity, method_id, jurl, jheaders);
    if((*env)->ExceptionCheck(env) || result == NULL)
        goto done;
    result_text = (*env)->GetStringUTFChars(env, result, NULL);
    if(result_text == NULL)
        goto done;
    newline = strchr(result_text, '\n');
    if(newline == NULL)
        goto done;
    if(status != NULL)
        *status = strtol(result_text, NULL, 10);
    ok = sync_buffer_set(response, newline + 1);

done:
    if(result_text != NULL && result != NULL)
        (*env)->ReleaseStringUTFChars(env, result, result_text);
    if(result != NULL)
        (*env)->DeleteLocalRef(env, result);
    if(jheaders != NULL)
        (*env)->DeleteLocalRef(env, jheaders);
    if(jurl != NULL)
        (*env)->DeleteLocalRef(env, jurl);
    if((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
    if(attached)
        (*jvm)->DetachCurrentThread(jvm);
    return ok;
}
#elif defined(__EMSCRIPTEN__)
EM_ASYNC_JS(int, sync_web_http_request,
            (const char *method_ptr, const char *url_ptr, const char *body_ptr,
             const char *headers_ptr, char *response_ptr, int response_size,
             long *status_ptr), {
    const method = UTF8ToString(method_ptr);
    const url = UTF8ToString(url_ptr);
    const body = body_ptr ? UTF8ToString(body_ptr) : "";
    const headerLines = headers_ptr ? UTF8ToString(headers_ptr) : "";
    const headers = {};

    for(const line of headerLines.split("\n")) {
        if(!line) continue;
        const colon = line.indexOf(":");
        if(colon <= 0) continue;
        headers[line.slice(0, colon).trim()] = line.slice(colon + 1).trim();
    }

    try {
        const response = await fetch(url, {
            method,
            headers,
            body: method === "GET" ? undefined : body,
            credentials: "omit",
            redirect: "manual"
        });
        const text = await response.text();
        setValue(status_ptr, response.status, "i32");
        stringToUTF8(text, response_ptr, response_size);
        return 1;
    } catch(e) {
        console.error("Inbe sync HTTP failed:", e);
        setValue(status_ptr, 0, "i32");
        stringToUTF8("", response_ptr, response_size);
        return 0;
    }
});

EM_JS(int, sync_web_fetch_start_js,
      (int request_id, const char *method_ptr, const char *url_ptr,
       const char *body_ptr, const char *headers_ptr), {
    const method = UTF8ToString(method_ptr);
    const url = UTF8ToString(url_ptr);
    const body = body_ptr ? UTF8ToString(body_ptr) : "";
    const headerLines = headers_ptr ? UTF8ToString(headers_ptr) : "";
    const headers = {};

    if(!Module.__inbeSyncFetches)
        Module.__inbeSyncFetches = {};
    Module.__inbeSyncFetches[request_id] = {state: 0, status: 0, text: ""};

    for(const line of headerLines.split("\n")) {
        if(!line) continue;
        const colon = line.indexOf(":");
        if(colon <= 0) continue;
        headers[line.slice(0, colon).trim()] = line.slice(colon + 1).trim();
    }

    fetch(url, {
        method,
        headers,
        body: method === "GET" ? undefined : body,
        credentials: "omit",
        redirect: "manual"
    }).then(async response => {
        const text = await response.text();
        Module.__inbeSyncFetches[request_id] = {
            state: 1,
            status: response.status,
            text
        };
    }).catch(error => {
        console.error("Inbe sync HTTP failed:", error);
        Module.__inbeSyncFetches[request_id] = {
            state: 2,
            status: 0,
            text: ""
        };
    });
    return 1;
});

EM_JS(int, sync_web_fetch_poll_js,
      (int request_id, char *response_ptr, int response_size, long *status_ptr), {
    const requests = Module.__inbeSyncFetches || {};
    const request = requests[request_id];
    if(!request)
        return 2;
    if(request.state === 0)
        return 0;
    setValue(status_ptr, request.status || 0, "i32");
    stringToUTF8(request.text || "", response_ptr, response_size);
    delete requests[request_id];
    return request.state === 1 ? 1 : 2;
});

EM_JS(int, sync_websocket_start_js, (const char *url_ptr), {
    const url = UTF8ToString(url_ptr);
    const now = Date.now();
    function scheduleRetry() {
        const failures = Math.min((Module.__inbeSyncWebSocketFailures || 0) + 1, 8);
        Module.__inbeSyncWebSocketFailures = failures;
        Module.__inbeSyncWebSocketRetryAt = Date.now() +
            Math.min(300000, 2000 * Math.pow(2, failures - 1));
    }
    function logSocketError(event) {
        const logAt = Module.__inbeSyncWebSocketLogAt || 0;
        if(Date.now() < logAt)
            return;
        Module.__inbeSyncWebSocketLogAt = Date.now() + 60000;
        console.warn("Inbe sync WebSocket unavailable; remote sync events will retry in the background.", event);
    }
    if(Module.__inbeSyncWebSocket &&
       Module.__inbeSyncWebSocketUrl === url &&
       (Module.__inbeSyncWebSocket.readyState === WebSocket.OPEN ||
        Module.__inbeSyncWebSocket.readyState === WebSocket.CONNECTING))
        return 1;
    if(Module.__inbeSyncWebSocketRetryAt && now < Module.__inbeSyncWebSocketRetryAt)
        return 1;
    if(Module.__inbeSyncWebSocket) {
        try { Module.__inbeSyncWebSocket.close(); } catch(e) {}
        Module.__inbeSyncWebSocket = null;
    }
    Module.__inbeSyncWebSocketUrl = url;
    try {
        const ws = new WebSocket(url);
        Module.__inbeSyncWebSocket = ws;
        ws.onopen = function() {
            Module.__inbeSyncWebSocketRetryAt = 0;
            Module.__inbeSyncWebSocketFailures = 0;
            Module.__inbeSyncWebSocketLogAt = 0;
            console.info("Inbe sync WebSocket connected");
        };
        ws.onmessage = function(event) {
            try {
                const message = JSON.parse(String(event.data || ""));
                if(message.type === "sync_ready" || message.type === "sync_changed")
                    Module.__inbeSyncWebSocketEvent = 1;
            } catch(e) {
                if(String(event.data || "").indexOf("sync_changed") >= 0)
                    Module.__inbeSyncWebSocketEvent = 1;
            }
        };
        ws.onclose = function(event) {
            if(Module.__inbeSyncWebSocket === ws)
                Module.__inbeSyncWebSocket = null;
            scheduleRetry();
            if(ws.__inbeHadError)
                logSocketError(event);
        };
        ws.onerror = function(event) {
            ws.__inbeHadError = true;
        };
        return 1;
    } catch(e) {
        scheduleRetry();
        logSocketError(e);
        return 0;
    }
});

EM_JS(int, sync_websocket_poll_js, (void), {
    const event = Module.__inbeSyncWebSocketEvent ? 1 : 0;
    Module.__inbeSyncWebSocketEvent = 0;
    return event;
});

static int
sync_http_request(const char *method, const char *url, const char *body,
                  const char *const *headers, int header_count,
                  SyncBuffer *response, long *status)
{
    SyncBuffer header_blob = {0};
    char *response_text;
    int ok;

    if(status != NULL)
        *status = 0;
    if(response == NULL || method == NULL || url == NULL)
        return 0;
    for(int i = 0; i < header_count; i++) {
        if(headers[i] != NULL &&
           (!sync_buffer_append(&header_blob, headers[i], strlen(headers[i])) ||
            !sync_buffer_append(&header_blob, "\n", 1))) {
            free(header_blob.data);
            return 0;
        }
    }

    response_text = (char *)calloc(1, INBE_SYNC_WEB_RESPONSE_MAX);
    if(response_text == NULL) {
        free(header_blob.data);
        return 0;
    }
    ok = sync_web_http_request(method, url, body != NULL ? body : "",
                               header_blob.data != NULL ? header_blob.data : "",
                               response_text, INBE_SYNC_WEB_RESPONSE_MAX, status);
    free(header_blob.data);
    if(!ok) {
        free(response_text);
        return 0;
    }
    response->data = response_text;
    response->len = strlen(response_text);
    response->cap = INBE_SYNC_WEB_RESPONSE_MAX;
    return 1;
}

typedef enum WebSyncState {
    WEB_SYNC_IDLE,
    WEB_SYNC_WAIT_CHALLENGE,
    WEB_SYNC_WAIT_LOGIN,
    WEB_SYNC_WAIT_SYNC
} WebSyncState;

typedef struct WebSyncJob {
    WebSyncState state;
    int request_id;
    char base_url[512];
    InbeSyncAccount account;
    SyncBuffer login_body;
    char *payload;
    char *response_text;
    long status;
    InbeSyncClientResult result;
    int retried_auth;
} WebSyncJob;

static WebSyncJob g_web_sync = {0};
static int g_web_sync_next_request_id = 1;

static void
web_sync_reset(void)
{
    free(g_web_sync.login_body.data);
    if(g_web_sync.payload != NULL)
        storage_free_sync_payload_json(g_web_sync.payload);
    free(g_web_sync.response_text);
    memset(&g_web_sync, 0, sizeof(g_web_sync));
}

static int
web_sync_start_fetch(const char *method, const char *url, const char *body,
                     const char *const *headers, int header_count,
                     WebSyncState wait_state)
{
    SyncBuffer header_blob = {0};

    free(g_web_sync.response_text);
    g_web_sync.response_text = NULL;
    g_web_sync.status = 0;

    for(int i = 0; i < header_count; i++) {
        if(headers[i] != NULL &&
           (!sync_buffer_append(&header_blob, headers[i], strlen(headers[i])) ||
            !sync_buffer_append(&header_blob, "\n", 1))) {
            free(header_blob.data);
            return 0;
        }
    }

    g_web_sync.response_text = (char *)calloc(1, INBE_SYNC_WEB_RESPONSE_MAX);
    if(g_web_sync.response_text == NULL) {
        free(header_blob.data);
        return 0;
    }
    g_web_sync.request_id = g_web_sync_next_request_id++;
    if(g_web_sync_next_request_id <= 0)
        g_web_sync_next_request_id = 1;
    g_web_sync.state = wait_state;
    if(!sync_web_fetch_start_js(g_web_sync.request_id, method, url,
                                body != NULL ? body : "",
                                header_blob.data != NULL ? header_blob.data : "")) {
        free(header_blob.data);
        return 0;
    }
    free(header_blob.data);
    return 1;
}

static int
web_sync_poll_fetch(SyncBuffer *response)
{
    int poll;

    if(response == NULL || g_web_sync.response_text == NULL)
        return 2;
    poll = sync_web_fetch_poll_js(g_web_sync.request_id,
                                  g_web_sync.response_text,
                                  INBE_SYNC_WEB_RESPONSE_MAX,
                                  &g_web_sync.status);
    if(poll != 1)
        return poll;
    response->data = g_web_sync.response_text;
    response->len = strlen(g_web_sync.response_text);
    response->cap = INBE_SYNC_WEB_RESPONSE_MAX;
    g_web_sync.response_text = NULL;
    return 1;
}

static int
web_sync_start_challenge(void)
{
    char url[768];

    sync_join_url(url, sizeof(url), g_web_sync.base_url, INBE_CHALLENGE_PATH);
    if(strlen(url) + strlen(g_web_sync.account.public_id) + 10 >= sizeof(url))
        return 0;
    strncat(url, "?user_id=", sizeof(url) - strlen(url) - 1);
    strncat(url, g_web_sync.account.public_id, sizeof(url) - strlen(url) - 1);
    return web_sync_start_fetch("GET", url, NULL, NULL, 0, WEB_SYNC_WAIT_CHALLENGE);
}

static int
web_sync_start_login(const char *nonce_hex)
{
    char message[256];
    char signature_hex[5000];
    char url[768];
    char user_header[96];
    char signature_header[5050];
    const char *headers[3];

    if(!sync_build_message("POST", INBE_LOGIN_PATH, nonce_hex,
                           g_web_sync.login_body.data, message, sizeof(message)))
        return 0;
    if(!sync_account_sign_hex((const uint8_t *)message, strlen(message),
                                   signature_hex, sizeof(signature_hex)))
        return 0;
    sync_join_url(url, sizeof(url), g_web_sync.base_url, INBE_LOGIN_PATH);
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s",
             g_web_sync.account.public_id);
    snprintf(signature_header, sizeof(signature_header), "X-Inbe-Signature: %s",
             signature_hex);
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = signature_header;
    return web_sync_start_fetch("POST", url, g_web_sync.login_body.data,
                                headers, 3, WEB_SYNC_WAIT_LOGIN);
}

static int
web_sync_start_sync(void)
{
    char token[4096];
    char url[768];
    char user_header[96];
    char auth_header[4200];
    const char *headers[3];

    if(!sync_load_valid_auth_token(token, sizeof(token)))
        return 0;
    if(g_web_sync.payload == NULL) {
        g_web_sync.payload = storage_build_sync_payload_json(g_web_sync.account.public_id,
                                                             g_web_sync.account.public_key_hex);
        if(g_web_sync.payload == NULL)
            return 0;
    }
    sync_join_url(url, sizeof(url), g_web_sync.base_url, INBE_SYNC_PATH);
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s",
             g_web_sync.account.public_id);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = auth_header;
    return web_sync_start_fetch("POST", url, g_web_sync.payload,
                                headers, 3, WEB_SYNC_WAIT_SYNC);
}

static int
web_sync_build_login_body(void)
{
    if(g_web_sync.login_body.data != NULL)
        return 1;
    return sync_buffer_append(&g_web_sync.login_body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) &&
           sync_buffer_append_json_string(&g_web_sync.login_body, g_web_sync.account.public_id) &&
           sync_buffer_append(&g_web_sync.login_body, ",\"client_id\":", strlen(",\"client_id\":")) &&
           sync_buffer_append_json_string(&g_web_sync.login_body, storage_sync_client_id()) &&
           sync_buffer_append(&g_web_sync.login_body, ",\"public_key\":", strlen(",\"public_key\":")) &&
           sync_buffer_append_json_string(&g_web_sync.login_body, g_web_sync.account.public_key_hex) &&
           sync_buffer_append(&g_web_sync.login_body, "}", 1);
}

int
sync_client_web_sync_start(const char *base_url)
{
    char token[4096];

    if(g_web_sync.state != WEB_SYNC_IDLE)
        return 0;
    if(!sync_client_url_valid(base_url))
        return 0;
    if(!sync_account_load(&g_web_sync.account))
        return 0;
    snprintf(g_web_sync.base_url, sizeof(g_web_sync.base_url), "%s", base_url);
    g_web_sync.result = INBE_SYNC_CLIENT_OK;

    if(sync_load_valid_auth_token(token, sizeof(token)))
        return web_sync_start_sync();

    if(!web_sync_build_login_body()) {
        web_sync_reset();
        return 0;
    }
    if(!web_sync_start_challenge()) {
        web_sync_reset();
        return 0;
    }
    return 1;
}

int
sync_client_web_sync_poll(InbeSyncClientResult *result, int *changed)
{
    SyncBuffer response = {0};
    int poll;

    if(result != NULL)
        *result = INBE_SYNC_CLIENT_OK;
    if(changed != NULL)
        *changed = 0;
    if(g_web_sync.state == WEB_SYNC_IDLE)
        return 0;

    poll = web_sync_poll_fetch(&response);
    if(poll == 0)
        return 0;
    if(poll == 2) {
        if(result != NULL)
            *result = INBE_SYNC_CLIENT_REQUEST_FAILED;
        web_sync_reset();
        return 1;
    }

    if(g_web_sync.state == WEB_SYNC_WAIT_CHALLENGE) {
        char nonce_hex[65];
        if(g_web_sync.status != 200 ||
           !sync_find_json_string(response.data, "nonce", nonce_hex, sizeof(nonce_hex)) ||
           strlen(nonce_hex) != 64) {
            sync_log_http_failure("challenge", g_web_sync.status, response.data);
            if(result != NULL)
                *result = g_web_sync.status == 401
                              ? INBE_SYNC_CLIENT_AUTH_FAILED
                              : INBE_SYNC_CLIENT_CHALLENGE_FAILED;
            free(response.data);
            web_sync_reset();
            return 1;
        }
        free(response.data);
        if(!web_sync_start_login(nonce_hex)) {
            if(result != NULL)
                *result = INBE_SYNC_CLIENT_SIGN_FAILED;
            web_sync_reset();
            return 1;
        }
        return 0;
    }

    if(g_web_sync.state == WEB_SYNC_WAIT_LOGIN) {
        char token[4096];
        long long expires_in;
        long long expires_at;

        if(g_web_sync.status == 401) {
            sync_log_http_failure("login auth", g_web_sync.status, response.data);
            if(result != NULL)
                *result = INBE_SYNC_CLIENT_AUTH_FAILED;
            free(response.data);
            web_sync_reset();
            return 1;
        }
        if(g_web_sync.status < 200 || g_web_sync.status >= 300) {
            sync_log_http_failure("login", g_web_sync.status, response.data);
            if(result != NULL)
                *result = INBE_SYNC_CLIENT_REQUEST_FAILED;
            free(response.data);
            web_sync_reset();
            return 1;
        }
        expires_in = sync_find_json_int64(response.data, "expires_in_seconds", 3600);
        if(!sync_find_json_string(response.data, "auth_token", token, sizeof(token))) {
            sync_log_http_failure("login payload", g_web_sync.status, response.data);
            if(result != NULL)
                *result = INBE_SYNC_CLIENT_PAYLOAD_FAILED;
            free(response.data);
            web_sync_reset();
            return 1;
        }
        expires_at = (long long)time(NULL) + expires_in - 30;
        if(expires_at < (long long)time(NULL))
            expires_at = (long long)time(NULL);
        {
            char text[32];
            snprintf(text, sizeof(text), "%lld", expires_at);
            storage_set_setting_text(INBE_SYNC_AUTH_TOKEN_KEY, token);
            storage_set_setting_text(INBE_SYNC_AUTH_TOKEN_EXPIRES_KEY, text);
        }
        free(response.data);
        if(!web_sync_start_sync()) {
            if(result != NULL)
                *result = INBE_SYNC_CLIENT_PAYLOAD_FAILED;
            web_sync_reset();
            return 1;
        }
        return 0;
    }

    if(g_web_sync.state == WEB_SYNC_WAIT_SYNC) {
        if(g_web_sync.status == 401) {
            sync_client_clear_auth_token();
            sync_log_http_failure("sync auth", g_web_sync.status, response.data);
            free(response.data);
            if(!g_web_sync.retried_auth) {
                g_web_sync.retried_auth = 1;
                if(web_sync_build_login_body() && web_sync_start_challenge())
                    return 0;
            }
            if(result != NULL)
                *result = INBE_SYNC_CLIENT_AUTH_FAILED;
            web_sync_reset();
            return 1;
        }
        if(g_web_sync.status < 200 || g_web_sync.status >= 300) {
            sync_log_http_failure("sync", g_web_sync.status, response.data);
            if(result != NULL)
                *result = INBE_SYNC_CLIENT_REQUEST_FAILED;
            free(response.data);
            web_sync_reset();
            return 1;
        }
        if(!storage_apply_sync_response_json(response.data)) {
            sync_log_http_failure("sync payload", g_web_sync.status, response.data);
            if(result != NULL)
                *result = INBE_SYNC_CLIENT_PAYLOAD_FAILED;
            free(response.data);
            web_sync_reset();
            return 1;
        }
        if(changed != NULL)
            *changed = storage_last_sync_changed();
        free(response.data);
        storage_purge_synced_deleted_data();
        if(result != NULL)
            *result = INBE_SYNC_CLIENT_OK;
        web_sync_reset();
        return 1;
    }

    web_sync_reset();
    if(result != NULL)
        *result = INBE_SYNC_CLIENT_REQUEST_FAILED;
    return 1;
}

int
sync_client_web_start_remote_events(const char *base_url)
{
    InbeSyncAccount account;
    char token[4096];
    char ws_url[6000];

    if(!sync_client_url_valid(base_url))
        return 0;
    if(!sync_account_load(&account))
        return 0;
    if(!sync_load_valid_auth_token(token, sizeof(token)))
        return 0;
    if(!sync_join_ws_url(ws_url, sizeof(ws_url), base_url, INBE_SYNC_WS_PATH))
        return 0;
    if(strlen(ws_url) + strlen(token) + strlen("?token=") >= sizeof(ws_url))
        return 0;
    strncat(ws_url, "?token=", sizeof(ws_url) - strlen(ws_url) - 1);
    strncat(ws_url, token, sizeof(ws_url) - strlen(ws_url) - 1);
    return sync_websocket_start_js(ws_url);
}

int
sync_client_web_poll_remote_event(void)
{
    return sync_websocket_poll_js();
}
#endif

static InbeSyncClientResult
sync_fetch_challenge(const char *base_url, const char *user_id, char nonce_hex[65])
{
    char url[768];
    SyncBuffer response = {0};
    long status = 0;
    int ok;

    nonce_hex[0] = '\0';
    sync_join_url(url, sizeof(url), base_url, INBE_CHALLENGE_PATH);
    if(strlen(url) + strlen(user_id) + 10 >= sizeof(url))
        return INBE_SYNC_CLIENT_INVALID_URL;
    strncat(url, "?user_id=", sizeof(url) - strlen(url) - 1);
    strncat(url, user_id, sizeof(url) - strlen(url) - 1);
    ok = sync_http_request("GET", url, NULL, NULL, 0, &response, &status);
    if(!ok || status != 200 ||
       !sync_find_json_string(response.data, "nonce", nonce_hex, 65) ||
       strlen(nonce_hex) != 64) {
        sync_log_http_failure("challenge", status, response.data);
        free(response.data);
        return status == 401 ? INBE_SYNC_CLIENT_AUTH_FAILED : INBE_SYNC_CLIENT_CHALLENGE_FAILED;
    }
    free(response.data);
    return INBE_SYNC_CLIENT_OK;
}

static InbeSyncClientResult
sync_login(const char *base_url, const InbeSyncAccount *account)
{
    char nonce_hex[65];
    char message[256];
    char signature_hex[5000];
    char url[768];
    char user_header[96];
    char signature_header[5050];
    const char *headers[3];
    SyncBuffer body = {0};
    SyncBuffer response = {0};
    long status = 0;
    InbeSyncClientResult challenge_result;
    int ok;
    char token[4096];
    long long expires_in;
    long long expires_at;

    if(account == NULL)
        return INBE_SYNC_CLIENT_NO_ACCOUNT;
    if(!sync_buffer_append(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !sync_buffer_append_json_string(&body, account->public_id) ||
       !sync_buffer_append(&body, ",\"client_id\":", strlen(",\"client_id\":")) ||
       !sync_buffer_append_json_string(&body, storage_sync_client_id()) ||
       !sync_buffer_append(&body, ",\"public_key\":", strlen(",\"public_key\":")) ||
       !sync_buffer_append_json_string(&body, account->public_key_hex) ||
       !sync_buffer_append(&body, "}", 1)) {
        free(body.data);
        return INBE_SYNC_CLIENT_PAYLOAD_FAILED;
    }

    challenge_result = sync_fetch_challenge(base_url, account->public_id, nonce_hex);
    if(challenge_result != INBE_SYNC_CLIENT_OK)
    {
        free(body.data);
        return challenge_result;
    }
    if(!sync_build_message("POST", INBE_LOGIN_PATH, nonce_hex, body.data, message, sizeof(message))) {
        free(body.data);
        return INBE_SYNC_CLIENT_SIGN_FAILED;
    }
    if(!sync_account_sign_hex((const uint8_t *)message, strlen(message),
                                   signature_hex, sizeof(signature_hex))) {
        free(body.data);
        return INBE_SYNC_CLIENT_SIGN_FAILED;
    }
    sync_join_url(url, sizeof(url), base_url, INBE_LOGIN_PATH);
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", account->public_id);
    snprintf(signature_header, sizeof(signature_header), "X-Inbe-Signature: %s", signature_hex);
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = signature_header;
    ok = sync_http_request("POST", url, body.data, headers, 3, &response, &status);
    free(body.data);
    if(!ok) {
        sync_log_http_failure("login request", status, response.data);
        return INBE_SYNC_CLIENT_REQUEST_FAILED;
    }
    if(status == 401) {
        sync_log_http_failure("login auth", status, response.data);
        free(response.data);
        return INBE_SYNC_CLIENT_AUTH_FAILED;
    }
    if(status < 200 || status >= 300) {
        sync_log_http_failure("login", status, response.data);
        free(response.data);
        return INBE_SYNC_CLIENT_REQUEST_FAILED;
    }
    expires_in = sync_find_json_int64(response.data, "expires_in_seconds", 3600);
    if(!sync_find_json_string(response.data, "auth_token", token, sizeof(token))) {
        sync_log_http_failure("login payload", status, response.data);
        free(response.data);
        return INBE_SYNC_CLIENT_PAYLOAD_FAILED;
    }
    expires_at = (long long)time(NULL) + expires_in - 30;
    if(expires_at < (long long)time(NULL))
        expires_at = (long long)time(NULL);
    {
        char text[32];
        snprintf(text, sizeof(text), "%lld", expires_at);
        storage_set_setting_text(INBE_SYNC_AUTH_TOKEN_KEY, token);
        storage_set_setting_text(INBE_SYNC_AUTH_TOKEN_EXPIRES_KEY, text);
    }
    free(response.data);
    return INBE_SYNC_CLIENT_OK;
}

static int
sync_load_valid_auth_token(char *out, size_t out_size)
{
    const char *token;
    const char *expires_text;
    char token_copy[4096];
    long long expires_at;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    token = storage_get_setting_text(INBE_SYNC_AUTH_TOKEN_KEY);
    snprintf(token_copy, sizeof(token_copy), "%s", token != NULL ? token : "");
    expires_text = storage_get_setting_text(INBE_SYNC_AUTH_TOKEN_EXPIRES_KEY);
    expires_at = expires_text != NULL ? atoll(expires_text) : 0;
    if(token_copy[0] == '\0' || expires_at <= (long long)time(NULL))
        return 0;
    snprintf(out, out_size, "%s", token_copy);
    return out[0] != '\0';
}

void
sync_client_clear_auth_token(void)
{
    storage_set_setting_text(INBE_SYNC_AUTH_TOKEN_KEY, "");
    storage_set_setting_text(INBE_SYNC_AUTH_TOKEN_EXPIRES_KEY, "");
}

static InbeSyncClientResult
sync_send_bearer(const char *base_url, const char *user_id, const char *body, const char *token)
{
    char url[768];
    char user_header[96];
    char auth_header[4200];
    const char *headers[3];
    SyncBuffer response = {0};
    long status = 0;
    int ok;

    sync_join_url(url, sizeof(url), base_url, INBE_SYNC_PATH);
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", user_id);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token != NULL ? token : "");
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = auth_header;
    ok = sync_http_request("POST", url, body, headers, 3, &response, &status);
    if(!ok) {
        sync_log_http_failure("sync request", status, response.data);
        return INBE_SYNC_CLIENT_REQUEST_FAILED;
    }
    if(status == 401) {
        sync_log_http_failure("sync auth", status, response.data);
        free(response.data);
        return INBE_SYNC_CLIENT_AUTH_FAILED;
    }
    if(status < 200 || status >= 300) {
        sync_log_http_failure("sync", status, response.data);
        free(response.data);
        return INBE_SYNC_CLIENT_REQUEST_FAILED;
    }
    if(!storage_apply_sync_response_json(response.data)) {
        sync_log_http_failure("sync payload", status, response.data);
        free(response.data);
        return INBE_SYNC_CLIENT_PAYLOAD_FAILED;
    }
    free(response.data);
    return INBE_SYNC_CLIENT_OK;
}

InbeSyncClientResult
sync_client_sync(const char *base_url)
{
    InbeSyncAccount account;
    char *payload;
    InbeSyncClientResult result;
    char token[4096];

    if(!sync_client_url_valid(base_url))
        return INBE_SYNC_CLIENT_INVALID_URL;
    if(!sync_account_load(&account))
        return INBE_SYNC_CLIENT_NO_ACCOUNT;
    if(!sync_load_valid_auth_token(token, sizeof(token))) {
        result = sync_login(base_url, &account);
        if(result != INBE_SYNC_CLIENT_OK)
            return result;
        if(!sync_load_valid_auth_token(token, sizeof(token)))
            return INBE_SYNC_CLIENT_AUTH_FAILED;
    }
    payload = storage_build_sync_payload_json(account.public_id, account.public_key_hex);
    if(payload == NULL)
        return INBE_SYNC_CLIENT_PAYLOAD_FAILED;
    result = sync_send_bearer(base_url, account.public_id, payload, token);
    if(result == INBE_SYNC_CLIENT_AUTH_FAILED) {
        sync_client_clear_auth_token();
        result = sync_login(base_url, &account);
        if(result == INBE_SYNC_CLIENT_OK && sync_load_valid_auth_token(token, sizeof(token)))
            result = sync_send_bearer(base_url, account.public_id, payload, token);
        else if(result == INBE_SYNC_CLIENT_OK)
            result = INBE_SYNC_CLIENT_AUTH_FAILED;
    }
    storage_free_sync_payload_json(payload);
    if(result == INBE_SYNC_CLIENT_OK)
        storage_purge_synced_deleted_data();
    return result;
}

InbeSyncClientResult
sync_client_register_alias(const char *base_url, const char *alias)
{
#if defined(__EMSCRIPTEN__)
    (void)base_url;
    (void)alias;
    return INBE_SYNC_CLIENT_REQUEST_FAILED;
#else
    InbeSyncAccount account;
    char token[4096];
    char url[768];
    char user_header[96];
    char auth_header[4200];
    const char *headers[3];
    SyncBuffer body = {0};
    SyncBuffer response = {0};
    long status = 0;
    InbeSyncClientResult result;
    int ok;
    char saved_alias[40];

    if(!sync_client_url_valid(base_url))
        return INBE_SYNC_CLIENT_INVALID_URL;
    if(alias == NULL || alias[0] == '\0')
        return INBE_SYNC_CLIENT_PAYLOAD_FAILED;
    if(!sync_account_load(&account))
        return INBE_SYNC_CLIENT_NO_ACCOUNT;
    if(!sync_load_valid_auth_token(token, sizeof(token))) {
        result = sync_login(base_url, &account);
        if(result != INBE_SYNC_CLIENT_OK)
            return result;
        if(!sync_load_valid_auth_token(token, sizeof(token)))
            return INBE_SYNC_CLIENT_AUTH_FAILED;
    }
    if(!sync_buffer_append(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !sync_buffer_append_json_string(&body, account.public_id) ||
       !sync_buffer_append(&body, ",\"alias\":", strlen(",\"alias\":")) ||
       !sync_buffer_append_json_string(&body, alias) ||
       !sync_buffer_append(&body, "}", 1)) {
        free(body.data);
        return INBE_SYNC_CLIENT_PAYLOAD_FAILED;
    }
    sync_join_url(url, sizeof(url), base_url, INBE_ACCOUNT_ALIAS_PATH);
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", account.public_id);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = auth_header;
    ok = sync_http_request("POST", url, body.data, headers, 3, &response, &status);
    free(body.data);
    if(!ok) {
        sync_log_http_failure("alias request", status, response.data);
        return INBE_SYNC_CLIENT_REQUEST_FAILED;
    }
    if(status == 401) {
        sync_log_http_failure("alias auth", status, response.data);
        free(response.data);
        return INBE_SYNC_CLIENT_AUTH_FAILED;
    }
    if(status < 200 || status >= 300) {
        sync_log_http_failure("alias", status, response.data);
        free(response.data);
        return INBE_SYNC_CLIENT_REQUEST_FAILED;
    }
    if(sync_find_json_string(response.data, "alias", saved_alias, sizeof(saved_alias)))
        storage_set_setting_text("sync_account_alias", saved_alias);
    free(response.data);
    return INBE_SYNC_CLIENT_OK;
#endif
}

InbeSyncClientResult
sync_client_wait_for_remote_event(const char *base_url)
{
#if ANDROID_BUILD
    InbeSyncAccount account;
    char token[4096];
    char ws_url[6000];
    char auth_header[4200];
    const char *headers[1];
    SyncBuffer response = {0};
    long status = 0;

    if(!sync_client_url_valid(base_url))
        return INBE_SYNC_CLIENT_INVALID_URL;
    if(!sync_account_load(&account))
        return INBE_SYNC_CLIENT_NO_ACCOUNT;
    if(!sync_load_valid_auth_token(token, sizeof(token)))
        return INBE_SYNC_CLIENT_AUTH_FAILED;
    if(!sync_join_ws_url(ws_url, sizeof(ws_url), base_url, INBE_SYNC_WS_PATH))
        return INBE_SYNC_CLIENT_INVALID_URL;
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);
    headers[0] = auth_header;

    if(!sync_android_websocket_wait(ws_url, headers, 1, &response, &status)) {
        TraceLog(LOG_WARNING, "SYNC: websocket connect failed http=%ld url=%s", status, ws_url);
        if(status == 401)
            sync_client_clear_auth_token();
        free(response.data);
        return INBE_SYNC_CLIENT_REQUEST_FAILED;
    }
    if(status != 101) {
        TraceLog(LOG_WARNING, "SYNC: websocket connect failed http=%ld url=%s", status, ws_url);
        if(status == 401)
            sync_client_clear_auth_token();
        free(response.data);
        return INBE_SYNC_CLIENT_REQUEST_FAILED;
    }
    if(response.data != NULL && strstr(response.data, "\"type\":\"sync_changed\"") != NULL) {
        free(response.data);
        return INBE_SYNC_CLIENT_OK;
    }
    free(response.data);
    return INBE_SYNC_CLIENT_REQUEST_FAILED;
#elif !defined(__EMSCRIPTEN__)
    InbeSyncAccount account;
    char token[4096];
    char ws_url[6000];
    CURL *curl;
    struct curl_slist *curl_headers = NULL;
    CURLcode code;
    long status = 0;
    char auth_header[4200];
    char message[2048];
    size_t message_len = 0;

    if(!sync_client_url_valid(base_url))
        return INBE_SYNC_CLIENT_INVALID_URL;
    if(!sync_account_load(&account))
        return INBE_SYNC_CLIENT_NO_ACCOUNT;
    if(!sync_load_valid_auth_token(token, sizeof(token)))
        return INBE_SYNC_CLIENT_AUTH_FAILED;
    if(!sync_join_ws_url(ws_url, sizeof(ws_url), base_url, INBE_SYNC_WS_PATH))
        return INBE_SYNC_CLIENT_INVALID_URL;
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl == NULL)
        return INBE_SYNC_CLIENT_REQUEST_FAILED;
    curl_headers = curl_slist_append(curl_headers, auth_header);
    curl_easy_setopt(curl, CURLOPT_URL, ws_url);
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "inbe-sync/1");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);

    code = curl_easy_perform(curl);
    if(code != CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        TraceLog(LOG_WARNING, "SYNC: websocket connect failed code=%d http=%ld url=%s", (int)code,
                 status, ws_url);
        if(status == 401)
            sync_client_clear_auth_token();
        curl_slist_free_all(curl_headers);
        curl_easy_cleanup(curl);
        return INBE_SYNC_CLIENT_REQUEST_FAILED;
    }
    curl_slist_free_all(curl_headers);

    for(;;) {
        char buffer[512];
        size_t got = 0;
        const struct curl_ws_frame *meta = NULL;

        code = curl_ws_recv(curl, buffer, sizeof(buffer) - 1, &got, &meta);
        if(code == CURLE_AGAIN) {
#if defined(_WIN32)
            Sleep(100);
#else
            struct timespec delay;
            delay.tv_sec = 0;
            delay.tv_nsec = 100000000L;
            nanosleep(&delay, NULL);
#endif
            continue;
        }
        if(code == CURLE_GOT_NOTHING) {
            curl_easy_cleanup(curl);
            return INBE_SYNC_CLIENT_REQUEST_FAILED;
        }
        if(code != CURLE_OK) {
            TraceLog(LOG_WARNING, "SYNC: websocket recv failed code=%d", (int)code);
            curl_easy_cleanup(curl);
            return INBE_SYNC_CLIENT_REQUEST_FAILED;
        }
        if(meta != NULL && (meta->flags & CURLWS_CLOSE)) {
            curl_easy_cleanup(curl);
            return INBE_SYNC_CLIENT_REQUEST_FAILED;
        }
        if(meta != NULL && !(meta->flags & CURLWS_TEXT))
            continue;
        if(got > 0) {
            if(got > sizeof(message) - 1 - message_len)
                got = sizeof(message) - 1 - message_len;
            memcpy(message + message_len, buffer, got);
            message_len += got;
            message[message_len] = '\0';
        }
        if(meta == NULL || meta->bytesleft != 0)
            continue;
        if(strstr(message, "\"type\":\"sync_changed\"") != NULL) {
            curl_easy_cleanup(curl);
            return INBE_SYNC_CLIENT_OK;
        }
        message_len = 0;
        message[0] = '\0';
    }
#else
    (void)base_url;
    return INBE_SYNC_CLIENT_REQUEST_FAILED;
#endif
}

InbeSyncClientResult
sync_client_delete_account(const char *base_url)
{
    InbeSyncAccount account;
    char url[768];
    char exported_key[8200];
    SyncBuffer body = {0};
    SyncBuffer response = {0};
    const char *headers[1] = {"Content-Type: application/json"};
    long status = 0;
    int len;
    int ok;

    if(!sync_client_url_valid(base_url))
        return INBE_SYNC_CLIENT_INVALID_URL;
    if(!sync_account_load(&account))
        return INBE_SYNC_CLIENT_NO_ACCOUNT;

    len = snprintf(exported_key, sizeof(exported_key),
                   "inbe-sync-key-v1\nalgorithm=ML-DSA-44\npublic_id=%s\npublic_key=%s\nprivate_key=%s\n",
                   account.public_id, account.public_key_hex, account.private_key_hex);
    if(len <= 0 || (size_t)len >= sizeof(exported_key))
        return INBE_SYNC_CLIENT_PAYLOAD_FAILED;

    if(!sync_buffer_append(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !sync_buffer_append_json_string(&body, account.public_id) ||
       !sync_buffer_append(&body, ",\"exported_key\":", strlen(",\"exported_key\":")) ||
       !sync_buffer_append_json_string(&body, exported_key) ||
       !sync_buffer_append(&body, "}", 1)) {
        free(body.data);
        return INBE_SYNC_CLIENT_PAYLOAD_FAILED;
    }

    sync_join_url(url, sizeof(url), base_url, INBE_ACCOUNT_DELETE_WITH_KEY_PATH);
    ok = sync_http_request("POST", url, body.data, headers, 1, &response, &status);
    free(body.data);
    free(response.data);
    if(!ok)
        return INBE_SYNC_CLIENT_REQUEST_FAILED;
    if(status == 401 || status == 403)
        return INBE_SYNC_CLIENT_AUTH_FAILED;
    return status >= 200 && status < 300 ? INBE_SYNC_CLIENT_OK : INBE_SYNC_CLIENT_REQUEST_FAILED;
}
