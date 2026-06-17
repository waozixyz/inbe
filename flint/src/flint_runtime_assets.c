#include "flint_runtime_assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#include <wininet.h>
#define FLINT_MKDIR(path) _mkdir(path)
#else
#include <errno.h>
#include <unistd.h>
#define FLINT_MKDIR(path) mkdir(path, 0755)
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/fetch.h>
#endif

#if defined(FLINT_HAS_LIBCURL) && !defined(__EMSCRIPTEN__)
#include <curl/curl.h>
#include <pthread.h>
#endif

static FlintRuntimeAssetDownloadBackend g_download_backend = NULL;

static int
path_is_dir(const char *path)
{
    struct stat st;
    if(path == NULL || path[0] == '\0')
        return 0;
    return stat(path, &st) == 0 && (st.st_mode & S_IFDIR) != 0;
}

int
flint_runtime_asset_ensure_dir(const char *path)
{
    char tmp[512];
    char *p;

    if(path == NULL || path[0] == '\0')
        return 0;
    if(path_is_dir(path))
        return 1;

    snprintf(tmp, sizeof(tmp), "%s", path);
    p = tmp;
    if(p[0] == '/')
        p++;

    while((p = strchr(p, '/')) != NULL) {
        *p = '\0';
        if(tmp[0] != '\0' && !path_is_dir(tmp)) {
            if(FLINT_MKDIR(tmp) != 0 && !path_is_dir(tmp))
                return 0;
        }
        *p = '/';
        p++;
    }

    if(FLINT_MKDIR(tmp) != 0 && !path_is_dir(tmp))
        return 0;
    return 1;
}

int
flint_runtime_asset_cache_root(const char *app_id, char *out, size_t out_size)
{
    const char *id = (app_id != NULL && app_id[0] != '\0') ? app_id : "flint";

    if(out == NULL || out_size == 0)
        return 0;

#if defined(__EMSCRIPTEN__)
    snprintf(out, out_size, "/persistent_data/%s", id);
#elif defined(_WIN32)
    {
        const char *local = getenv("LOCALAPPDATA");
        if(local != NULL && local[0] != '\0')
            snprintf(out, out_size, "%s/%s/runtime-assets", local, id);
        else
            snprintf(out, out_size, "%s/runtime-assets", id);
    }
#else
    {
        const char *xdg = getenv("XDG_DATA_HOME");
        const char *home = getenv("HOME");
        if(xdg != NULL && xdg[0] != '\0')
            snprintf(out, out_size, "%s/%s/runtime-assets", xdg, id);
        else if(home != NULL && home[0] != '\0')
            snprintf(out, out_size, "%s/.local/share/%s/runtime-assets", home, id);
        else
            snprintf(out, out_size, ".local/%s/runtime-assets", id);
    }
#endif

    return flint_runtime_asset_ensure_dir(out);
}

int
flint_runtime_assets_init(const char *app_id)
{
    char root[512];

    if(!flint_runtime_asset_cache_root(app_id, root, sizeof(root)))
        return 0;

#if defined(__EMSCRIPTEN__)
    EM_ASM({
        var root = UTF8ToString($0);
        var parts = root.split('/').filter(Boolean);
        var path = "";
        try {
            for(var i = 0; i < parts.length; i++) {
                path += '/' + parts[i];
                if(!FS.analyzePath(path).exists)
                    FS.mkdir(path);
            }
            if(!Module.__flintRuntimeAssetsMounted) {
                FS.mount(IDBFS, {}, '/persistent_data');
                Module.__flintRuntimeAssetsMounted = true;
            }
            FS.syncfs(true, function(err) {
                if(err) console.error('flint runtime asset sync failed', err);
            });
        } catch(e) {
            console.error('flint runtime asset init failed', e);
        }
    }, root);
#endif

    return 1;
}

const char *
flint_runtime_asset_status_text(FlintRuntimeAssetStatus status)
{
    switch(status) {
    case FLINT_RUNTIME_ASSET_IDLE: return "idle";
    case FLINT_RUNTIME_ASSET_DOWNLOADING: return "downloading";
    case FLINT_RUNTIME_ASSET_READY: return "ready";
    case FLINT_RUNTIME_ASSET_ERROR: return "error";
    case FLINT_RUNTIME_ASSET_UNSUPPORTED: return "unsupported";
    default: return "unknown";
    }
}

void
flint_runtime_asset_set_download_backend(FlintRuntimeAssetDownloadBackend backend)
{
    g_download_backend = backend;
}

#if defined(__EMSCRIPTEN__)
static void
fetch_done(emscripten_fetch_t *fetch)
{
    FlintRuntimeAssetDownload *download = (FlintRuntimeAssetDownload *)fetch->userData;
    FILE *file;

    if(download == NULL) {
        emscripten_fetch_close(fetch);
        return;
    }

    download->http_status = fetch->status;
    download->bytes = (size_t)fetch->numBytes;
    file = fopen(download->path, "wb");
    if(file == NULL) {
        snprintf(download->error, sizeof(download->error), "failed to open %s", download->path);
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        emscripten_fetch_close(fetch);
        return;
    }

    if(fwrite(fetch->data, 1, (size_t)fetch->numBytes, file) != (size_t)fetch->numBytes) {
        snprintf(download->error, sizeof(download->error), "failed to write %s", download->path);
        fclose(file);
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        emscripten_fetch_close(fetch);
        return;
    }

    fclose(file);
    download->status = FLINT_RUNTIME_ASSET_READY;
    EM_ASM({
        FS.syncfs(false, function(err) {
            if(err) console.error('flint runtime asset save failed', err);
        });
    });
    emscripten_fetch_close(fetch);
}

static void
fetch_failed(emscripten_fetch_t *fetch)
{
    FlintRuntimeAssetDownload *download = (FlintRuntimeAssetDownload *)fetch->userData;

    if(download != NULL) {
        download->http_status = fetch->status;
        snprintf(download->error, sizeof(download->error), "HTTP %d", fetch->status);
        download->status = FLINT_RUNTIME_ASSET_ERROR;
    }
    emscripten_fetch_close(fetch);
}
#endif

#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
typedef struct WindowsDownloadContext {
    FlintRuntimeAssetDownload *download;
} WindowsDownloadContext;

static void
windows_set_last_error(FlintRuntimeAssetDownload *download, const char *prefix)
{
    DWORD err = GetLastError();

    if(download == NULL)
        return;
    snprintf(download->error, sizeof(download->error), "%s (%lu)",
             prefix != NULL ? prefix : "Windows download failed",
             (unsigned long)err);
}

static DWORD WINAPI
windows_download_thread_main(void *user_data)
{
    WindowsDownloadContext *ctx = (WindowsDownloadContext *)user_data;
    FlintRuntimeAssetDownload *download = ctx != NULL ? ctx->download : NULL;
    HINTERNET internet = NULL;
    HINTERNET request = NULL;
    FILE *file = NULL;
    char status_buf[32];
    DWORD status_len = sizeof(status_buf);
    DWORD status_index = 0;
    char length_buf[64];
    DWORD length_len = sizeof(length_buf);
    DWORD length_index = 0;
    BYTE buffer[32768];
    DWORD bytes_read = 0;
    BOOL read_ok;

    free(ctx);
    if(download == NULL)
        return 0;

    file = fopen(download->path, "wb");
    if(file == NULL) {
        snprintf(download->error, sizeof(download->error), "failed to open %.200s", download->path);
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        return 0;
    }

    internet = InternetOpenA("flint-runtime-assets/1",
                             INTERNET_OPEN_TYPE_PRECONFIG,
                             NULL, NULL, 0);
    if(internet == NULL) {
        fclose(file);
        windows_set_last_error(download, "failed to initialize Windows networking");
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        return 0;
    }

    request = InternetOpenUrlA(internet, download->url, NULL, 0,
                               INTERNET_FLAG_RELOAD |
                               INTERNET_FLAG_NO_CACHE_WRITE |
                               INTERNET_FLAG_PRAGMA_NOCACHE,
                               0);
    if(request == NULL) {
        fclose(file);
        InternetCloseHandle(internet);
        remove(download->path);
        windows_set_last_error(download, "Windows download failed");
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        return 0;
    }

    if(HttpQueryInfoA(request, HTTP_QUERY_STATUS_CODE, status_buf, &status_len, &status_index)) {
        status_buf[sizeof(status_buf) - 1] = '\0';
        download->http_status = strtol(status_buf, NULL, 10);
    }
    if(download->http_status < 200 || download->http_status >= 300) {
        snprintf(download->error, sizeof(download->error), "HTTP %ld", download->http_status);
        fclose(file);
        InternetCloseHandle(request);
        InternetCloseHandle(internet);
        remove(download->path);
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        return 0;
    }

    if(HttpQueryInfoA(request, HTTP_QUERY_CONTENT_LENGTH, length_buf, &length_len, &length_index)) {
        length_buf[sizeof(length_buf) - 1] = '\0';
        download->total_bytes = (size_t)strtoull(length_buf, NULL, 10);
    }

    while((read_ok = InternetReadFile(request, buffer, sizeof(buffer), &bytes_read)) && bytes_read > 0) {
        if(fwrite(buffer, 1, bytes_read, file) != bytes_read) {
            snprintf(download->error, sizeof(download->error), "failed to write %.200s", download->path);
            fclose(file);
            InternetCloseHandle(request);
            InternetCloseHandle(internet);
            remove(download->path);
            download->status = FLINT_RUNTIME_ASSET_ERROR;
            return 0;
        }
        download->bytes += (size_t)bytes_read;
    }

    if(!read_ok) {
        fclose(file);
        InternetCloseHandle(request);
        InternetCloseHandle(internet);
        remove(download->path);
        windows_set_last_error(download, "Windows download failed");
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        return 0;
    }

    fclose(file);
    InternetCloseHandle(request);
    InternetCloseHandle(internet);
    if(download->total_bytes == 0)
        download->total_bytes = download->bytes;
    download->status = FLINT_RUNTIME_ASSET_READY;
    return 0;
}
#endif

#if defined(FLINT_HAS_LIBCURL) && !defined(__EMSCRIPTEN__)
typedef struct NativeDownloadContext {
    FlintRuntimeAssetDownload *download;
} NativeDownloadContext;

static size_t
curl_write_file(void *ptr, size_t size, size_t nmemb, void *stream)
{
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

static int
curl_progress(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
              curl_off_t ultotal, curl_off_t ulnow)
{
    FlintRuntimeAssetDownload *download = (FlintRuntimeAssetDownload *)clientp;
    (void)ultotal;
    (void)ulnow;

    if(download == NULL)
        return 0;

    download->bytes = dlnow > 0 ? (size_t)dlnow : 0;
    download->total_bytes = dltotal > 0 ? (size_t)dltotal : 0;
    return 0;
}

static void *
curl_thread_main(void *user_data)
{
    NativeDownloadContext *ctx = (NativeDownloadContext *)user_data;
    FlintRuntimeAssetDownload *download = ctx != NULL ? ctx->download : NULL;
    CURL *curl;
    FILE *file;
    CURLcode res;
    curl_off_t downloaded = 0;

    free(ctx);
    if(download == NULL)
        return NULL;

    file = fopen(download->path, "wb");
    if(file == NULL) {
        snprintf(download->error, sizeof(download->error), "failed to open %.200s", download->path);
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        return NULL;
    }

    curl = curl_easy_init();
    if(curl == NULL) {
        fclose(file);
        snprintf(download->error, sizeof(download->error), "failed to initialize curl");
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        return NULL;
    }

    curl_easy_setopt(curl, CURLOPT_URL, download->url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "flint-runtime-assets/1");
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, download);

    res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &download->http_status);
    if(curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &downloaded) == CURLE_OK &&
       downloaded > 0)
        download->bytes = (size_t)downloaded;
    if(download->total_bytes == 0 && downloaded > 0)
        download->total_bytes = (size_t)downloaded;
    curl_easy_cleanup(curl);
    fclose(file);

    if(res != CURLE_OK) {
        snprintf(download->error, sizeof(download->error), "%s", curl_easy_strerror(res));
        remove(download->path);
        download->status = FLINT_RUNTIME_ASSET_ERROR;
        return NULL;
    }

    download->status = FLINT_RUNTIME_ASSET_READY;
    return NULL;
}
#endif

int
flint_runtime_asset_download(FlintRuntimeAssetDownload *download,
                             const char *url,
                             const char *path)
{
    char dir[512];
    char *slash;

    if(download == NULL || url == NULL || url[0] == '\0' || path == NULL || path[0] == '\0')
        return 0;
    if(download->status == FLINT_RUNTIME_ASSET_DOWNLOADING)
        return 0;

    memset(download, 0, sizeof(*download));
    snprintf(download->url, sizeof(download->url), "%s", url);
    snprintf(download->path, sizeof(download->path), "%s", path);

    snprintf(dir, sizeof(dir), "%s", path);
    slash = strrchr(dir, '/');
    if(slash != NULL) {
        *slash = '\0';
        if(!flint_runtime_asset_ensure_dir(dir)) {
            snprintf(download->error, sizeof(download->error), "failed to create %.200s", dir);
            download->status = FLINT_RUNTIME_ASSET_ERROR;
            return 0;
        }
    }

    download->status = FLINT_RUNTIME_ASSET_DOWNLOADING;

    if(g_download_backend != NULL) {
        if(g_download_backend(download, download->url, download->path))
            return 1;
        if(download->status == FLINT_RUNTIME_ASSET_DOWNLOADING) {
            snprintf(download->error, sizeof(download->error), "runtime asset backend failed");
            download->status = FLINT_RUNTIME_ASSET_ERROR;
        }
        return 0;
    }

#if defined(__EMSCRIPTEN__)
    {
        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.userData = download;
        attr.onsuccess = fetch_done;
        attr.onerror = fetch_failed;
        emscripten_fetch(&attr, download->url);
        return 1;
    }
#elif defined(_WIN32)
    {
        HANDLE thread;
        WindowsDownloadContext *ctx = (WindowsDownloadContext *)calloc(1, sizeof(*ctx));
        if(ctx == NULL) {
            snprintf(download->error, sizeof(download->error), "out of memory");
            download->status = FLINT_RUNTIME_ASSET_ERROR;
            return 0;
        }
        ctx->download = download;
        thread = CreateThread(NULL, 0, windows_download_thread_main, ctx, 0, NULL);
        if(thread == NULL) {
            free(ctx);
            windows_set_last_error(download, "failed to create download thread");
            download->status = FLINT_RUNTIME_ASSET_ERROR;
            return 0;
        }
        CloseHandle(thread);
        return 1;
    }
#elif defined(FLINT_HAS_LIBCURL)
    {
        pthread_t thread;
        NativeDownloadContext *ctx = (NativeDownloadContext *)calloc(1, sizeof(*ctx));
        if(ctx == NULL) {
            snprintf(download->error, sizeof(download->error), "out of memory");
            download->status = FLINT_RUNTIME_ASSET_ERROR;
            return 0;
        }
        ctx->download = download;
        if(pthread_create(&thread, NULL, curl_thread_main, ctx) != 0) {
            free(ctx);
            snprintf(download->error, sizeof(download->error), "failed to create download thread");
            download->status = FLINT_RUNTIME_ASSET_ERROR;
            return 0;
        }
        pthread_detach(thread);
        return 1;
    }
#else
    snprintf(download->error, sizeof(download->error), "runtime asset downloads are not enabled for this build");
    download->status = FLINT_RUNTIME_ASSET_UNSUPPORTED;
    return 0;
#endif
}
