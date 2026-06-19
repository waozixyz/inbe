#include "sync_client.h"

#include "storage.h"
#include "sync_account.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define INBE_SYNC_CLIENT_HAS_ANDROID 1
#include <android_native_app_glue.h>
#include <jni.h>
extern struct android_app *GetAndroidApp(void);
#elif defined(FLINT_HAS_LIBCURL) && !defined(__EMSCRIPTEN__)
#define INBE_SYNC_CLIENT_HAS_CURL 1
#include <curl/curl.h>
#endif

#if defined(INBE_SYNC_CLIENT_HAS_CURL) || defined(INBE_SYNC_CLIENT_HAS_ANDROID)
#define INBE_SYNC_CLIENT_HAS_HTTP 1
#endif

#define INBE_SYNC_PATH "/api/v1/sync"
#define INBE_DELETE_PATH "/api/v1/account"
#define INBE_CHALLENGE_PATH "/api/v1/sync/challenge"

typedef struct SyncBuffer {
    char *data;
    size_t len;
    size_t cap;
} SyncBuffer;

static int
sync_url_valid(const char *url)
{
    return url != NULL &&
           (strncmp(url, "https://", 8) == 0 ||
            strncmp(url, "http://127.0.0.1", 16) == 0 ||
            strncmp(url, "http://localhost", 16) == 0 ||
            strncmp(url, "http://10.0.2.2", 15) == 0);
}

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

#if defined(INBE_SYNC_CLIENT_HAS_CURL)
static size_t
sync_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    SyncBuffer *buffer = (SyncBuffer *)userdata;
    size_t bytes = size * nmemb;
    char *next;
    size_t next_cap;

    if(buffer == NULL || bytes == 0)
        return bytes;
    if(bytes > buffer->cap - buffer->len - 1) {
        next_cap = buffer->cap > 0 ? buffer->cap : 1024;
        while(bytes > next_cap - buffer->len - 1)
            next_cap *= 2;
        next = (char *)realloc(buffer->data, next_cap);
        if(next == NULL)
            return 0;
        buffer->data = next;
        buffer->cap = next_cap;
    }
    memcpy(buffer->data + buffer->len, ptr, bytes);
    buffer->len += bytes;
    buffer->data[buffer->len] = '\0';
    return bytes;
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
        free(response.data);
        return status == 401 ? INBE_SYNC_CLIENT_AUTH_FAILED : INBE_SYNC_CLIENT_CHALLENGE_FAILED;
    }
    free(response.data);
    return INBE_SYNC_CLIENT_OK;
}

static InbeSyncClientResult
sync_send_signed(const char *base_url, const char *path, const char *method,
                 const char *user_id, const char *body)
{
    char nonce_hex[65];
    char message[256];
    char signature_hex[5000];
    char url[768];
    char user_header[96];
    char signature_header[5050];
    const char *headers[3];
    SyncBuffer response = {0};
    long status = 0;
    InbeSyncClientResult challenge_result;
    int ok;

    challenge_result = sync_fetch_challenge(base_url, user_id, nonce_hex);
    if(challenge_result != INBE_SYNC_CLIENT_OK)
        return challenge_result;
    if(!sync_build_message(method, path, nonce_hex, body, message, sizeof(message)))
        return INBE_SYNC_CLIENT_SIGN_FAILED;
    if(!inbe_sync_account_sign_hex((const uint8_t *)message, strlen(message),
                                   signature_hex, sizeof(signature_hex)))
        return INBE_SYNC_CLIENT_SIGN_FAILED;
    sync_join_url(url, sizeof(url), base_url, path);
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", user_id);
    snprintf(signature_header, sizeof(signature_header), "X-Inbe-Signature: %s", signature_hex);
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = signature_header;
    ok = sync_http_request(method, url, body, headers, 3, &response, &status);
    free(response.data);
    if(!ok)
        return INBE_SYNC_CLIENT_REQUEST_FAILED;
    if(status == 401)
        return INBE_SYNC_CLIENT_AUTH_FAILED;
    return status >= 200 && status < 300 ? INBE_SYNC_CLIENT_OK : INBE_SYNC_CLIENT_REQUEST_FAILED;
}
#endif

InbeSyncClientResult
inbe_sync_client_sync(const char *base_url)
{
#if defined(INBE_SYNC_CLIENT_HAS_HTTP)
    InbeSyncAccount account;
    char *payload;
    InbeSyncClientResult result;

    if(!sync_url_valid(base_url))
        return INBE_SYNC_CLIENT_INVALID_URL;
    if(!inbe_sync_account_load(&account))
        return INBE_SYNC_CLIENT_NO_ACCOUNT;
    payload = inbe_storage_build_sync_payload_json(account.public_id, account.public_key_hex);
    if(payload == NULL)
        return INBE_SYNC_CLIENT_PAYLOAD_FAILED;
    result = sync_send_signed(base_url, INBE_SYNC_PATH, "POST", account.public_id, payload);
    inbe_storage_free_sync_payload_json(payload);
    return result;
#else
    (void)base_url;
    return INBE_SYNC_CLIENT_UNAVAILABLE;
#endif
}

InbeSyncClientResult
inbe_sync_client_delete_remote(const char *base_url)
{
#if defined(INBE_SYNC_CLIENT_HAS_HTTP)
    InbeSyncAccount account;
    char body[128];

    if(!sync_url_valid(base_url))
        return INBE_SYNC_CLIENT_INVALID_URL;
    if(!inbe_sync_account_load(&account))
        return INBE_SYNC_CLIENT_NO_ACCOUNT;
    snprintf(body, sizeof(body), "{\"user_id_hash\":\"%s\"}", account.public_id);
    return sync_send_signed(base_url, INBE_DELETE_PATH, "DELETE", account.public_id, body);
#else
    (void)base_url;
    return INBE_SYNC_CLIENT_UNAVAILABLE;
#endif
}
