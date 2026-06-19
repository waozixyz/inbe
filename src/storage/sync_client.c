#include "sync_client.h"

#include "storage.h"
#include "sync_account.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define INBE_SYNC_CLIENT_HAS_ANDROID 1
#include <android_native_app_glue.h>
#include <jni.h>
extern struct android_app *GetAndroidApp(void);
#elif defined(__EMSCRIPTEN__)
#define INBE_SYNC_CLIENT_HAS_WEB 1
#include <emscripten.h>
#elif defined(FLINT_HAS_LIBCURL) && !defined(__EMSCRIPTEN__)
#define INBE_SYNC_CLIENT_HAS_CURL 1
#include <curl/curl.h>
#endif

#if defined(INBE_SYNC_CLIENT_HAS_CURL) || defined(INBE_SYNC_CLIENT_HAS_ANDROID) || defined(INBE_SYNC_CLIENT_HAS_WEB)
#define INBE_SYNC_CLIENT_HAS_HTTP 1
#endif

#define INBE_SYNC_PATH "/api/v1/sync"
#define INBE_CHALLENGE_PATH "/api/v1/sync/challenge"
#define INBE_LOGIN_PATH "/api/v1/sync/login"
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
inbe_sync_client_result_name(InbeSyncClientResult result)
{
    switch(result) {
        case INBE_SYNC_CLIENT_OK:
            return "ok";
        case INBE_SYNC_CLIENT_UNAVAILABLE:
            return "unavailable";
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
inbe_sync_client_url_valid(const char *url)
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
inbe_sync_client_normalize_url(const char *input, char *out, size_t out_size)
{
    int len;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(!inbe_sync_client_url_valid(input))
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
inbe_sync_client_test_response_buffer(const char *first, const char *second,
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
sync_build_message(const char *method, const char *path, const char *nonce_hex,
                   const char *body, char *out, size_t out_size)
{
    char body_hash[65];
    int len;

    if(method == NULL || path == NULL || nonce_hex == NULL || body == NULL ||
       out == NULL || out_size == 0)
        return 0;
    inbe_sync_sha256_hex((const uint8_t *)body, strlen(body), body_hash);
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

#if defined(INBE_SYNC_CLIENT_HAS_CURL)
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
#elif defined(INBE_SYNC_CLIENT_HAS_ANDROID)
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
#elif defined(INBE_SYNC_CLIENT_HAS_WEB)
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
#endif

#if defined(INBE_SYNC_CLIENT_HAS_HTTP)
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
       !sync_buffer_append_json_string(&body, inbe_storage_sync_client_id()) ||
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
    if(!inbe_sync_account_sign_hex((const uint8_t *)message, strlen(message),
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
        inbe_storage_set_setting_text(INBE_SYNC_AUTH_TOKEN_KEY, token);
        inbe_storage_set_setting_text(INBE_SYNC_AUTH_TOKEN_EXPIRES_KEY, text);
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
    token = inbe_storage_get_setting_text(INBE_SYNC_AUTH_TOKEN_KEY);
    snprintf(token_copy, sizeof(token_copy), "%s", token != NULL ? token : "");
    expires_text = inbe_storage_get_setting_text(INBE_SYNC_AUTH_TOKEN_EXPIRES_KEY);
    expires_at = expires_text != NULL ? atoll(expires_text) : 0;
    if(token_copy[0] == '\0' || expires_at <= (long long)time(NULL))
        return 0;
    snprintf(out, out_size, "%s", token_copy);
    return out[0] != '\0';
}

void
inbe_sync_client_clear_auth_token(void)
{
    inbe_storage_set_setting_text(INBE_SYNC_AUTH_TOKEN_KEY, "");
    inbe_storage_set_setting_text(INBE_SYNC_AUTH_TOKEN_EXPIRES_KEY, "");
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
    if(!inbe_storage_apply_sync_response_json(response.data)) {
        sync_log_http_failure("sync payload", status, response.data);
        free(response.data);
        return INBE_SYNC_CLIENT_PAYLOAD_FAILED;
    }
    free(response.data);
    return INBE_SYNC_CLIENT_OK;
}
#endif

#if !defined(INBE_SYNC_CLIENT_HAS_HTTP)
void
inbe_sync_client_clear_auth_token(void)
{
    inbe_storage_set_setting_text(INBE_SYNC_AUTH_TOKEN_KEY, "");
    inbe_storage_set_setting_text(INBE_SYNC_AUTH_TOKEN_EXPIRES_KEY, "");
}
#endif

InbeSyncClientResult
inbe_sync_client_sync(const char *base_url)
{
#if defined(INBE_SYNC_CLIENT_HAS_HTTP)
    InbeSyncAccount account;
    char *payload;
    InbeSyncClientResult result;
    char token[4096];

    if(!inbe_sync_client_url_valid(base_url))
        return INBE_SYNC_CLIENT_INVALID_URL;
    if(!inbe_sync_account_load(&account))
        return INBE_SYNC_CLIENT_NO_ACCOUNT;
    if(!sync_load_valid_auth_token(token, sizeof(token))) {
        result = sync_login(base_url, &account);
        if(result != INBE_SYNC_CLIENT_OK)
            return result;
        if(!sync_load_valid_auth_token(token, sizeof(token)))
            return INBE_SYNC_CLIENT_AUTH_FAILED;
    }
    payload = inbe_storage_build_sync_payload_json(account.public_id, NULL);
    if(payload == NULL)
        return INBE_SYNC_CLIENT_PAYLOAD_FAILED;
    result = sync_send_bearer(base_url, account.public_id, payload, token);
    if(result == INBE_SYNC_CLIENT_AUTH_FAILED) {
        inbe_sync_client_clear_auth_token();
        result = sync_login(base_url, &account);
        if(result == INBE_SYNC_CLIENT_OK && sync_load_valid_auth_token(token, sizeof(token)))
            result = sync_send_bearer(base_url, account.public_id, payload, token);
        else if(result == INBE_SYNC_CLIENT_OK)
            result = INBE_SYNC_CLIENT_AUTH_FAILED;
    }
    inbe_storage_free_sync_payload_json(payload);
    return result;
#else
    (void)base_url;
    return INBE_SYNC_CLIENT_UNAVAILABLE;
#endif
}

InbeSyncClientResult
inbe_sync_client_delete_account(const char *base_url)
{
#if defined(INBE_SYNC_CLIENT_HAS_HTTP)
    InbeSyncAccount account;
    char url[768];
    char exported_key[8200];
    SyncBuffer body = {0};
    SyncBuffer response = {0};
    const char *headers[1] = {"Content-Type: application/json"};
    long status = 0;
    int len;
    int ok;

    if(!inbe_sync_client_url_valid(base_url))
        return INBE_SYNC_CLIENT_INVALID_URL;
    if(!inbe_sync_account_load(&account))
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
#else
    (void)base_url;
    return INBE_SYNC_CLIENT_UNAVAILABLE;
#endif
}
