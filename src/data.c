#include "data.h"

#include "storage.h"

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include "android_share.h"
#endif

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DATA_PATH_MAX 512

static char g_data_root[DATA_PATH_MAX] = "";
static char g_today_dir[DATA_PATH_MAX] = "";
static int g_data_ready = 0;

static int
ensure_dir(const char *path)
{
    char temp[DATA_PATH_MAX];
    char *p;

    if(path == NULL || path[0] == '\0')
        return 0;
    if(DirectoryExists(path))
        return 1;

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    return MakeDirectory(path) || DirectoryExists(path);
#else
    snprintf(temp, sizeof(temp), "%s", path);
    p = temp;
    if(*p == '/')
        p++;
    while((p = strchr(p, '/')) != NULL) {
        *p = '\0';
        if(temp[0] != '\0' && !DirectoryExists(temp) && !MakeDirectory(temp) && !DirectoryExists(temp)) {
            *p = '/';
            return 0;
        }
        *p = '/';
        p++;
    }
    return MakeDirectory(path) || DirectoryExists(path);
#endif
}

const char *
data_root(void)
{
    if(g_data_root[0] != '\0')
        return g_data_root;

#if defined(PLATFORM_WEB)
    snprintf(g_data_root, sizeof(g_data_root), "/home/inbe");
#elif defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    snprintf(g_data_root, sizeof(g_data_root), "%s/inbe", GetWorkingDirectory());
#elif defined(_WIN32)
    {
        const char *local = getenv("LOCALAPPDATA");
        const char *roaming = getenv("APPDATA");
        if(local != NULL && local[0] != '\0')
            snprintf(g_data_root, sizeof(g_data_root), "%s/Inbe", local);
        else if(roaming != NULL && roaming[0] != '\0')
            snprintf(g_data_root, sizeof(g_data_root), "%s/Inbe", roaming);
        else
            snprintf(g_data_root, sizeof(g_data_root), "Inbe");
    }
#else
    {
        const char *xdg = getenv("XDG_DATA_HOME");
        const char *home = getenv("HOME");
        if(xdg != NULL && xdg[0] != '\0')
            snprintf(g_data_root, sizeof(g_data_root), "%s/inbe", xdg);
        else if(home != NULL && home[0] != '\0')
            snprintf(g_data_root, sizeof(g_data_root), "%s/.local/share/inbe", home);
        else
            snprintf(g_data_root, sizeof(g_data_root), ".local/inbe");
    }
#endif

    ensure_dir(g_data_root);
    TraceLog(LOG_INFO, "DATA: root directory: %s", g_data_root);
    return g_data_root;
}

void
data_init(void)
{
    if(g_data_ready)
        return;
    g_data_ready = inbe_storage_init(data_root());
}

const char *
data_today_dir(void)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    if(tm == NULL)
        return data_root();
    snprintf(g_today_dir, sizeof(g_today_dir), "%s/%04d/%02d/%02d",
             data_root(), tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    return g_today_dir;
}

int
data_save_session_path(const int *round_times, int round_count, char *out_path, size_t out_path_size)
{
    data_init();
    return inbe_storage_save_session(round_times, round_count, out_path, out_path_size);
}

int
data_save_session_path_for_activity(const int *round_times, int round_count,
                                    int topic, int activity,
                                    char *out_path, size_t out_path_size)
{
    data_init();
    return inbe_storage_save_session_for_activity(round_times, round_count, topic, activity,
                                                  out_path, out_path_size);
}

int
data_save_session(const int *round_times, int round_count)
{
    return data_save_session_path(round_times, round_count, NULL, 0);
}

int
data_replace_session(const char *path, const int *round_times, int round_count)
{
    data_init();
    return inbe_storage_replace_session(path, round_times, round_count);
}

int
data_rename_session(const char *old_path, const char *new_path)
{
    const char *name;
    int hour = 0;
    int minute = 0;
    int second = 0;

    data_init();
    name = GetFileName(new_path);
    if(name == NULL || sscanf(name, "inbe-%2d%2d%2d", &hour, &minute, &second) != 3)
        return 0;
    (void)second;
    return inbe_storage_rename_session_time(old_path, hour, minute);
}

int
data_delete_session(const char *path)
{
    data_init();
    return inbe_storage_delete_session(path);
}

int
data_has_any(void)
{
    data_init();
    return inbe_storage_has_any();
}

long long
data_get_total_size(void)
{
    data_init();
    return inbe_storage_total_size();
}

int
data_get_session_count(void)
{
    data_init();
    return inbe_storage_session_count();
}

long long
data_delete_all(void)
{
    data_init();
    return inbe_storage_delete_all_sessions();
}

void
data_default_export_filename(char *out, size_t out_size)
{
    time_t now;

    if(out == NULL || out_size == 0)
        return;

    now = time(NULL);
    if(now > 0)
        snprintf(out, out_size, "inbe-%lld.zip", (long long)now);
    else
        snprintf(out, out_size, "inbe.zip");
}

int
data_export(const char *path)
{
    data_init();
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    char filename[64];
    if(path != NULL && path[0] != '\0')
        return android_share_export(path);
    data_default_export_filename(filename, sizeof(filename));
    return android_share_export(filename);
#else
    return inbe_storage_export_zip(path);
#endif
}

int
data_validate_import_file(const char *path)
{
    FILE *fp;
    if(path == NULL || path[0] == '\0' || !FileExists(path))
        return 0;
    fp = fopen(path, "rb");
    if(fp == NULL)
        return 0;
    fclose(fp);
    return 1;
}

int
data_import(const char *path)
{
    data_init();
    return inbe_storage_import_zip(path);
}

typedef struct DbListSessionContext {
    data_session_callback callback;
    void *user;
} DbListSessionContext;

static void
db_list_session_callback(const char *path, int year, int month, int day,
                         int hour, int minute, int second,
                         int topic, int activity,
                         const int *round_times, int round_count, void *user)
{
    DbListSessionContext *ctx = user;
    char date[16];
    char time_text[16];
    int best = 0;

    (void)path;
    (void)topic;
    (void)activity;
    for(int i = 0; i < round_count; i++) {
        if(round_times[i] > best)
            best = round_times[i];
    }
    snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);
    snprintf(time_text, sizeof(time_text), "%02d:%02d:%02d", hour, minute, second);
    ctx->callback(date, time_text, round_count, best, ctx->user);
}

void
data_list_sessions(data_session_callback callback, void *user)
{
    DbListSessionContext ctx = {callback, user};
    if(callback == NULL)
        return;
    data_list_session_records(db_list_session_callback, &ctx);
}

void
data_list_session_records(data_session_record_callback callback, void *user)
{
    data_init();
    if(callback != NULL)
        inbe_storage_list_session_records((InbeStorageSessionRecordCallback)callback, user);
}

int
data_load_session(const char *path, int *round_times, int max_rounds,
                  int *year, int *month, int *day,
                  int *hour, int *minute, int *second)
{
    data_init();
    return inbe_storage_load_session(path, round_times, max_rounds,
                                     year, month, day, hour, minute, second);
}
