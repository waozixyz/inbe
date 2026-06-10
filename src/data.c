#include "data.h"
#include "miniz.h"
#include "locale.h"
#include "version.h"

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#include "android_share.h"
#endif

#include "raylib.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FS_PATH_MAX 512

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

static char g_data_root[FS_PATH_MAX] = "";
static char g_today_dir[FS_PATH_MAX] = "";

static int __attribute__((unused))
int_from_count(const char src[4])
{
    int a = (src[0] >= '0' && src[0] <= '9') ? src[0] - '0' : 0;
    int b = (src[1] >= '0' && src[1] <= '9') ? src[1] - '0' : 0;
    int c = (src[2] >= '0' && src[2] <= '9') ? src[2] - '0' : 0;
    return a * 100 + b * 10 + c;
}

static int
ensure_dir(const char *path)
{
    if(path == NULL || path[0] == '\0')
        return 0;

    TraceLog(LOG_INFO, "DATA: ensure_dir: %s", path);

    if(DirectoryExists(path)) {
        TraceLog(LOG_INFO, "DATA: Directory already exists: %s", path);
        return 1;
    }

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    TraceLog(LOG_INFO, "DATA: Creating final directory: %s", path);
    if(MakeDirectory(path))
        return 1;

    if(DirectoryExists(path)) {
        TraceLog(LOG_WARNING, "DATA: MakeDirectory failed but directory exists: %s", path);
        return 1;
    }

    TraceLog(LOG_ERROR, "DATA: failed to create directory: %s", path);
    return 0;
#else
    char temp[FS_PATH_MAX];
    char *p;

    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    p = temp;
    if(p[0] == '/') p++;

    while((p = strchr(p, '/')) != NULL) {
        *p = '\0';
        if(!DirectoryExists(temp)) {
            TraceLog(LOG_INFO, "DATA: Creating directory: %s", temp);
            if(!MakeDirectory(temp)) {
                if(!DirectoryExists(temp)) {
                    TraceLog(LOG_ERROR, "DATA: Failed to create directory: %s", temp);
                    *p = '/';
                    return 0;
                } else {
                    TraceLog(LOG_WARNING, "DATA: Directory already exists: %s", temp);
                }
            }
        }
        *p = '/';
        p++;
    }

    TraceLog(LOG_INFO, "DATA: Creating final directory: %s", path);
    if(MakeDirectory(path))
        return 1;

    if(DirectoryExists(path)) {
        TraceLog(LOG_WARNING, "DATA: MakeDirectory failed but directory exists: %s", path);
        return 1;
    }

    TraceLog(LOG_ERROR, "DATA: failed to create directory: %s", path);
    return 0;
#endif
}

static int
is_session_file(const char *path)
{
    const char *filename;

    if(path == NULL)
        return 0;

    filename = GetFileName(path);

    return strncmp(filename, "inbe-", 5) == 0;
}

static long long
get_file_size(const char *path)
{
#if defined(PLATFORM_WEB)
    int size = 0;
    EM_ASM({
        try {
            var path = UTF8ToString($0);
            if(FS.analyzePath(path).exists) {
                var stat = FS.stat(path);
                setValue($1, stat.size, 'i64');
                setValue($2, 1, 'i32');
            }
        } catch(e) {}
    }, path, &size, &size);
    return (long long)size;
#else
    struct stat st;
    if(stat(path, &st) == 0)
        return (long long)st.st_size;
    return -1;
#endif
}

static int
read_session_file(const char *path, int *round_times, int max_rounds)
{
    char *content;
    int round_count = 0;

    content = LoadFileText(path);
    if(content == NULL)
        return -1;

    char *line = content;
    char *next_line;
    while(line != NULL && round_count < max_rounds) {
        next_line = strchr(line, '\n');
        if(next_line != NULL) {
            *next_line = '\0';
            next_line++;
        }

        while(*line == ' ' || *line == '\t')
            line++;

        if(*line != '\0') {
            int seconds = atoi(line);
            if(seconds > 0 && seconds <= 999) {
                round_times[round_count++] = seconds;
            }
        }

        line = next_line;
    }

    UnloadFileText(content);
    return round_count;
}

static int
analyze_session_file(const char *path, int *best)
{
    int round_times[MaxRounds];
    int round_count;

    round_count = read_session_file(path, round_times, MaxRounds);
    if(round_count <= 0) {
        *best = 0;
        return 0;
    }

    *best = 0;
    for(int i = 0; i < round_count; i++) {
        if(round_times[i] > *best)
            *best = round_times[i];
    }

    return round_count;
}

typedef int (*SessionFileCallbackEx)(const char *path, int year, int month, int day, void *user_data);
typedef int (*SessionFileCallback)(const char *path, void *user_data);

static int
iterate_session_dates(SessionFileCallback callback, void *user_data)
{
    int year, month, day;
    FilePathList files;

    if(callback == NULL)
        return 0;

    for(year = 1970; year <= 2100; year++) {
        char year_path[FS_PATH_MAX];
        snprintf(year_path, sizeof(year_path), "%s/%04d", data_root(), year);

        if(!DirectoryExists(year_path))
            continue;

        for(month = 1; month <= 12; month++) {
            char month_path[FS_PATH_MAX];
            snprintf(month_path, sizeof(month_path), "%s/%02d", year_path, month);

            if(!DirectoryExists(month_path))
                continue;

            for(day = 1; day <= 31; day++) {
                char day_path[FS_PATH_MAX];
                snprintf(day_path, sizeof(day_path), "%s/%02d", month_path, day);

                if(!DirectoryExists(day_path))
                    continue;

                files = LoadDirectoryFiles(day_path);
                for(unsigned int i = 0; i < files.count; i++) {
                    if(is_session_file(files.paths[i])) {
                        if(!callback(files.paths[i], user_data)) {
                            UnloadDirectoryFiles(files);
                            return 0;
                        }
                    }
                }
                UnloadDirectoryFiles(files);
            }
        }
    }

    return 1;
}

static int
iterate_session_dates_ex(SessionFileCallbackEx callback, void *user_data)
{
    int year, month, day;
    FilePathList files;

    if(callback == NULL)
        return 0;

    for(year = 1970; year <= 2100; year++) {
        char year_path[FS_PATH_MAX];
        snprintf(year_path, sizeof(year_path), "%s/%04d", data_root(), year);

        if(!DirectoryExists(year_path))
            continue;

        for(month = 1; month <= 12; month++) {
            char month_path[FS_PATH_MAX];
            snprintf(month_path, sizeof(month_path), "%s/%02d", year_path, month);

            if(!DirectoryExists(month_path))
                continue;

            for(day = 1; day <= 31; day++) {
                char day_path[FS_PATH_MAX];
                snprintf(day_path, sizeof(day_path), "%s/%02d", month_path, day);

                if(!DirectoryExists(day_path))
                    continue;

                files = LoadDirectoryFiles(day_path);
                for(unsigned int i = 0; i < files.count; i++) {
                    if(is_session_file(files.paths[i])) {
                        if(!callback(files.paths[i], year, month, day, user_data)) {
                            UnloadDirectoryFiles(files);
                            return 0;
                        }
                    }
                }
                UnloadDirectoryFiles(files);
            }
        }
    }

    return 1;
}

void
data_init(void)
{
    (void)data_root();
}

const char *
data_root(void)
{
    if(g_data_root[0] != '\0')
        return g_data_root;

#if defined(PLATFORM_WEB)
    snprintf(g_data_root, sizeof(g_data_root), "/home/lotus");
    EM_ASM({
        try {
            FS.mkdir('/home');
            FS.mkdir('/home/lotus');
        } catch(e) {}
    });
#elif defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    snprintf(g_data_root, sizeof(g_data_root), "/data/data/xyz.waozi.inbe/files/lotus");
#else
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");

    if(xdg != NULL && xdg[0] != '\0')
        snprintf(g_data_root, sizeof(g_data_root), "%s/lotus", xdg);
    else if(home != NULL && home[0] != '\0')
        snprintf(g_data_root, sizeof(g_data_root), "%s/.local/share/lotus", home);
    else
        snprintf(g_data_root, sizeof(g_data_root), ".local/lotus");
#endif

    ensure_dir(g_data_root);

    TraceLog(LOG_INFO, "DATA: root directory: %s", g_data_root);
    return g_data_root;
}

const char *
data_today_dir(void)
{
    time_t now;
    struct tm *tm;
    char dir_year[FS_PATH_MAX];
    char dir_month[FS_PATH_MAX];

    now = time(NULL);
    tm = localtime(&now);
    if(tm == NULL)
        return data_root();

    snprintf(dir_year, sizeof(dir_year), "%s/%04d",
             data_root(), tm->tm_year + 1900);
    snprintf(dir_month, sizeof(dir_month), "%s/%02d",
             dir_year, tm->tm_mon + 1);
    snprintf(g_today_dir, sizeof(g_today_dir), "%s/%02d",
             dir_month, tm->tm_mday);

    return g_today_dir;
}

int
data_save_session_path(const int *round_times, int round_count, char *out_path, size_t out_path_size)
{
    time_t now;
    struct tm *tm;
    char dir_year[FS_PATH_MAX];
    char dir_month[FS_PATH_MAX];
    char dir_day[FS_PATH_MAX];
    char path[FS_PATH_MAX];
    char text[MaxRounds * 8];
    int offset = 0;
    int saved_round_count = 0;

    if(round_times == NULL || round_count <= 0 || round_count > MaxRounds)
        return 0;

    for(int i = 0; i < round_count; i++) {
        if(round_times[i] > 0)
            saved_round_count++;
    }
    if(saved_round_count <= 0)
        return 0;

    now = time(NULL);
    tm = localtime(&now);
    if(tm == NULL)
        return 0;

    snprintf(dir_year, sizeof(dir_year), "%s/%04d",
             data_root(), tm->tm_year + 1900);
    if(!ensure_dir(dir_year))
        return 0;

    snprintf(dir_month, sizeof(dir_month), "%s/%02d", dir_year, tm->tm_mon + 1);
    if(!ensure_dir(dir_month))
        return 0;

    snprintf(dir_day, sizeof(dir_day), "%s/%02d", dir_month, tm->tm_mday);
    if(!ensure_dir(dir_day))
        return 0;

    snprintf(path, sizeof(path), "%s/inbe-%02d%02d%02d",
             dir_day, tm->tm_hour, tm->tm_min, tm->tm_sec);

    for(int i = 0; i < round_count; i++) {
        if(round_times[i] <= 0)
            continue;
        if(offset >= (int)sizeof(text) - 8)
            break;
        offset += snprintf(text + offset, sizeof(text) - (size_t)offset,
                           "%d\n", round_times[i]);
    }

    TraceLog(LOG_INFO, "DATA: saving session to %s", path);
    if(SaveFileText(path, text)) {
        TraceLog(LOG_INFO, "DATA: saved session to %s", path);
        if(out_path != NULL && out_path_size > 0)
            snprintf(out_path, out_path_size, "%s", path);
#if defined(PLATFORM_WEB)
        EM_ASM({
            try {
                FS.syncfs(false, function(err) {
                    if(err) console.error("syncfs error:", err);
                });
            } catch(e) {}
        });
#endif
        return 1;
    } else {
        TraceLog(LOG_ERROR, "DATA: failed to save session to %s", path);
        return 0;
    }
}

int
data_save_session(const int *round_times, int round_count)
{
    return data_save_session_path(round_times, round_count, NULL, 0);
}

int
data_replace_session(const char *path, const int *round_times, int round_count)
{
    char text[MaxRounds * 8];
    int offset = 0;
    int saved_round_count = 0;

    if(path == NULL || path[0] == '\0' || round_times == NULL || round_count < 0 || round_count > MaxRounds)
        return 0;

    for(int i = 0; i < round_count; i++) {
        if(round_times[i] > 0)
            saved_round_count++;
    }

    if(saved_round_count <= 0)
        return data_delete_session(path);

    for(int i = 0; i < round_count; i++) {
        if(round_times[i] <= 0)
            continue;
        if(offset >= (int)sizeof(text) - 8)
            break;
        offset += snprintf(text + offset, sizeof(text) - (size_t)offset,
                           "%d\n", round_times[i]);
    }

    if(SaveFileText(path, text)) {
#if defined(PLATFORM_WEB)
        EM_ASM({
            try {
                FS.syncfs(false, function(err) {
                    if(err) console.error("syncfs error:", err);
                });
            } catch(e) {}
        });
#endif
        TraceLog(LOG_INFO, "DATA: replaced session %s", path);
        return 1;
    }

    TraceLog(LOG_ERROR, "DATA: failed to replace session %s", path);
    return 0;
}

int
data_rename_session(const char *old_path, const char *new_path)
{
    if(old_path == NULL || old_path[0] == '\0' ||
       new_path == NULL || new_path[0] == '\0')
        return 0;

    if(strcmp(old_path, new_path) == 0)
        return 1;

    if(!FileExists(old_path) || FileExists(new_path))
        return 0;

#if defined(PLATFORM_WEB)
    int ok = 0;
    EM_ASM({
        try {
            FS.rename(UTF8ToString($0), UTF8ToString($1));
            FS.syncfs(false, function(err) {
                if(err) console.error("syncfs error:", err);
            });
            setValue($2, 1, 'i32');
        } catch(e) {}
    }, old_path, new_path, &ok);
    if(!ok)
        return 0;
#else
    if(rename(old_path, new_path) != 0)
        return 0;
#endif

    TraceLog(LOG_INFO, "DATA: renamed session %s to %s", old_path, new_path);
    return 1;
}

int
data_delete_session(const char *path)
{
    if(path == NULL || path[0] == '\0')
        return 0;

    if(!FileExists(path))
        return 1;

#if defined(PLATFORM_WEB)
    EM_ASM({
        try {
            FS.unlink(UTF8ToString($0));
            FS.syncfs(false, function(err) {
                if(err) console.error("syncfs error:", err);
            });
        } catch(e) {}
    }, path);
#else
    if(remove(path) != 0)
        return 0;
#endif

    TraceLog(LOG_INFO, "DATA: deleted session %s", path);
    return 1;
}

static int
has_any_callback(const char *path, void *user_data)
{
    (void)path;
    int *found = user_data;
    *found = 1;
    return 0;
}

int
data_has_any(void)
{
    int found = 0;
    iterate_session_dates(has_any_callback, &found);
    return found;
}

static int
total_size_callback(const char *path, void *user_data)
{
    long long *total = user_data;
    long long size = get_file_size(path);
    if(size > 0)
        *total += size;
    return 1;
}

long long
data_get_total_size(void)
{
    long long total = 0;
    iterate_session_dates(total_size_callback, &total);
    return total;
}

static int
count_callback(const char *path, void *user_data)
{
    (void)path;
    int *count = user_data;
    (*count)++;
    return 1;
}

int
data_get_session_count(void)
{
    int count = 0;
    iterate_session_dates(count_callback, &count);
    return count;
}

static int
delete_callback(const char *path, void *user_data)
{
    long long *deleted = user_data;
    if(FileExists(path)) {
#if defined(PLATFORM_WEB)
        EM_ASM({
            try {
                FS.unlink(UTF8ToString($0));
            } catch(e) {}
        }, path);
#else
        remove(path);
#endif
        (*deleted)++;
    }
    return 1;
}

long long
data_delete_all(void)
{
    long long deleted = 0;
    iterate_session_dates(delete_callback, &deleted);

    if(deleted > 0)
        TraceLog(LOG_INFO, "DATA: deleted %lld session files", deleted);

    return deleted;
}

typedef struct {
    mz_zip_archive *archive;
} ExportContext;

static int
export_callback_ex(const char *path, int year, int month, int day, void *user_data)
{
    ExportContext *ctx = user_data;
    char zip_path[FS_PATH_MAX];
    const char *filename = GetFileName(path);

    snprintf(zip_path, sizeof(zip_path),
             "lotus-data/sessions/%04d/%02d/%02d/%s",
             year, month, day, filename);

    char *content = LoadFileText(path);
    if(content != NULL) {
        size_t size = strlen(content);
        if(!mz_zip_writer_add_mem(ctx->archive, zip_path, content, size, MZ_NO_COMPRESSION)) {
            TraceLog(LOG_WARNING, "DATA: failed to add file: %s", path);
        }
        UnloadFileText(content);
    }

    return 1;
}

int
data_export(const char *path)
{
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    (void)path;
    return android_share_export("inbe-export.zip");
#else
    mz_zip_archive archive;
    FILE *fp;
    void *zip_data;
    size_t zip_size;
    int session_count = 0;
    char metadata[512];
    time_t now;
    struct tm *tm;
    char date_str[64];
    ExportContext export_ctx;

    if(path == NULL || path[0] == '\0') {
        TraceLog(LOG_ERROR, "DATA: export path is empty");
        return 0;
    }

    if(!data_has_any()) {
        TraceLog(LOG_WARNING, "DATA: no data to export");
        return 0;
    }

    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_writer_init_heap(&archive, 0, 0)) {
        TraceLog(LOG_ERROR, "DATA: failed to initialize ZIP archive");
        return 0;
    }

    session_count = data_get_session_count();

    now = time(NULL);
    tm = localtime(&now);
    if(tm != NULL) {
        snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d %02d:%02d:%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else {
        strcpy(date_str, "Unknown");
    }

    {
        char metadata_header[128];
        char metadata_version[128];
        char metadata_date[128];
        char metadata_count[128];

        locale_format(metadata_header, sizeof(metadata_header), "export_metadata_header");
        locale_format(metadata_version, sizeof(metadata_version), "export_metadata_version", INBE_VERSION_STRING);
        locale_format(metadata_date, sizeof(metadata_date), "export_metadata_date", date_str);
        locale_format(metadata_count, sizeof(metadata_count), "export_metadata_count", session_count);
        snprintf(metadata, sizeof(metadata), "%s\n%s\n%s\n%s\n",
                 metadata_header,
                 metadata_version,
                 metadata_date,
                 metadata_count);
    }

    if(!mz_zip_writer_add_mem(&archive, "lotus-data/metadata.txt", metadata, strlen(metadata), MZ_NO_COMPRESSION)) {
        TraceLog(LOG_ERROR, "DATA: failed to write metadata");
        mz_zip_writer_end(&archive);
        return 0;
    }

    export_ctx.archive = &archive;
    iterate_session_dates_ex(export_callback_ex, &export_ctx);

    if(!mz_zip_writer_finalize_heap_archive(&archive, &zip_data, &zip_size)) {
        TraceLog(LOG_ERROR, "DATA: failed to finalize ZIP archive");
        mz_zip_writer_end(&archive);
        return 0;
    }

    if(zip_data == NULL || zip_size == 0) {
        TraceLog(LOG_ERROR, "DATA: failed to get ZIP data");
        mz_zip_writer_end(&archive);
        return 0;
    }

    fp = fopen(path, "wb");
    if(fp == NULL) {
        TraceLog(LOG_ERROR, "DATA: failed to open export file: %s", path);
        mz_zip_writer_end(&archive);
        return 0;
    }

    if(fwrite(zip_data, 1, zip_size, fp) != zip_size) {
        TraceLog(LOG_ERROR, "DATA: failed to write ZIP data");
        fclose(fp);
        mz_zip_writer_end(&archive);
        return 0;
    }

    fclose(fp);
    mz_zip_writer_end(&archive);

    TraceLog(LOG_INFO, "DATA: exported %d sessions to %s", session_count, path);
    return 1;
#endif
}

static int
validate_metadata(const char *metadata, int *session_count)
{
    char *dup;
    char *line;
    int line_count = 0;

    if(metadata == NULL || session_count == NULL)
        return 0;

    dup = strdup(metadata);
    if(dup == NULL)
        return 0;

    line = strtok(dup, "\n");
    while(line != NULL && line_count < 4) {
        // Trim whitespace
        while(*line == ' ' || *line == '\t' || *line == '\r')
            line++;

        line_count++;
        if(line_count == 4) {
            // Fourth line should contain session count
            *session_count = atoi(line);
            free(dup);
            return (*session_count >= 0);
        }
        line = strtok(NULL, "\n");
    }

    free(dup);
    return 0;
}

static int
parse_session_path(const char *zip_path, int *year, int *month, int *day, char *filename)
{
    int y, m, d;
    const char *p;

    if(zip_path == NULL || year == NULL || month == NULL || day == NULL || filename == NULL)
        return 0;

    // Expected format: lotus-data/sessions/YYYY/MM/DD/inbe-HHMMSS.txt
    if(strncmp(zip_path, "lotus-data/sessions/", 19) != 0)
        return 0;

    p = zip_path + 19;

    // Parse year
    if(sscanf(p, "%04d", &y) != 1 || y < 1970 || y > 2100)
        return 0;
    p += 5;

    // Parse month
    if(sscanf(p, "%02d", &m) != 1 || m < 1 || m > 12)
        return 0;
    p += 3;

    // Parse day
    if(sscanf(p, "%02d", &d) != 1 || d < 1 || d > 31)
        return 0;
    p += 3;

    // Expect filename starting with "inbe-"
    if(strncmp(p, "/inbe-", 6) != 0)
        return 0;
    p += 6;

    // Copy filename
    const char *filename_start = p;
    if(strlen(filename_start) >= FS_PATH_MAX - 1)
        return 0;

    strcpy(filename, filename_start);

    *year = y;
    *month = m;
    *day = d;

    return 1;
}

static int
validate_session_content(const char *content, int *round_count)
{
    char *dup;
    char *line;
    int count = 0;
    int valid = 1;

    if(content == NULL || round_count == NULL)
        return 0;

    dup = strdup(content);
    if(dup == NULL)
        return 0;

    line = strtok(dup, "\n");
    while(line != NULL) {
        // Trim whitespace
        while(*line == ' ' || *line == '\t' || *line == '\r')
            line++;

        if(*line != '\0') {
            int seconds = atoi(line);
            if(seconds > 0 && seconds <= 999) {
                count++;
            } else {
                valid = 0;
                break;
            }
        }

        line = strtok(NULL, "\n");
    }

    free(dup);

    if(valid && count > 0) {
        *round_count = count;
        return 1;
    }

    return 0;
}

static int
import_session_file(const char *zip_path, const char *local_path, mz_zip_archive *archive)
{
    char *content = NULL;
    size_t size;
    FILE *fp;
    int round_count;

    if(zip_path == NULL || local_path == NULL || archive == NULL)
        return 0;

    // Extract file from ZIP
    size = 0;
    content = (char *)mz_zip_reader_extract_file_to_heap(archive, zip_path, &size, 0);
    if(content == NULL || size == 0) {
        TraceLog(LOG_WARNING, "DATA: failed to extract file from ZIP: %s", zip_path);
        return 0;
    }

    // Validate session content
    if(!validate_session_content(content, &round_count)) {
        TraceLog(LOG_WARNING, "DATA: invalid session content: %s", zip_path);
        free(content);
        return 0;
    }

    // Create directory structure if needed
    char dir_path[FS_PATH_MAX];
    strncpy(dir_path, local_path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';

    char *last_slash = strrchr(dir_path, '/');
    if(last_slash != NULL) {
        *last_slash = '\0';
        if(!ensure_dir(dir_path)) {
            TraceLog(LOG_ERROR, "DATA: failed to create directory: %s", dir_path);
            free(content);
            return 0;
        }
    }

    // Write session file
    fp = fopen(local_path, "w");
    if(fp == NULL) {
        TraceLog(LOG_ERROR, "DATA: failed to open file for writing: %s", local_path);
        free(content);
        return 0;
    }

    if(fwrite(content, 1, size, fp) != size) {
        TraceLog(LOG_ERROR, "DATA: failed to write session file: %s", local_path);
        fclose(fp);
        free(content);
        return 0;
    }

    fclose(fp);
    free(content);

    TraceLog(LOG_INFO, "DATA: imported session: %s (%d rounds)", local_path, round_count);
    return 1;
}

// Validate import file before attempting to read ZIP
int data_validate_import_file(const char *path) {
    if(!path || path[0] == '\0')
        return 0;

    if(!FileExists(path))
        return 0;

    // Check if file is readable
    FILE *test = fopen(path, "rb");
    if(!test)
        return 0;

    fclose(test);
    return 1;
}

int
data_import(const char *path)
{
    mz_zip_archive archive;
    char *metadata = NULL;
    size_t metadata_size;
    int session_count = 0;
    int imported_count = 0;
    int failed_count = 0;
    int total_files;

    if(path == NULL || path[0] == '\0') {
        TraceLog(LOG_ERROR, "DATA: import path is empty");
        return 0;
    }

    // Validate import file before attempting to read ZIP
    if(!data_validate_import_file(path)) {
        TraceLog(LOG_ERROR, "DATA: import file validation failed: %s", path);
        return 0;
    }

    // Initialize ZIP reader
    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_reader_init_file(&archive, path, 0)) {
        TraceLog(LOG_ERROR, "DATA: failed to open ZIP file: %s", path);
        return 0;
    }

    // Extract and validate metadata
    metadata_size = 0;
    metadata = (char *)mz_zip_reader_extract_file_to_heap(&archive, "lotus-data/metadata.txt", &metadata_size, 0);
    if(metadata == NULL || metadata_size == 0) {
        TraceLog(LOG_ERROR, "DATA: failed to extract metadata from ZIP");
        mz_zip_reader_end(&archive);
        return 0;
    }

    if(!validate_metadata(metadata, &session_count)) {
        TraceLog(LOG_ERROR, "DATA: invalid metadata format");
        free(metadata);
        mz_zip_reader_end(&archive);
        return 0;
    }

    free(metadata);
    TraceLog(LOG_INFO, "DATA: metadata validated, expecting %d sessions", session_count);

    // Process session files
    total_files = (int)mz_zip_reader_get_num_files(&archive);

    for(int i = 0; i < total_files; i++) {
        mz_zip_archive_file_stat file_stat;
        char zip_path[FS_PATH_MAX];
        char local_path[FS_PATH_MAX];
        int year, month, day;
        char filename[FS_PATH_MAX];

        if(!mz_zip_reader_file_stat(&archive, i, &file_stat))
            continue;

        if(file_stat.m_is_directory)
            continue;

        strncpy(zip_path, file_stat.m_filename, sizeof(zip_path) - 1);
        zip_path[sizeof(zip_path) - 1] = '\0';

        // Check if this is a session file
        if(strncmp(zip_path, "lotus-data/sessions/", 19) != 0)
            continue;

        if(!parse_session_path(zip_path, &year, &month, &day, filename)) {
            TraceLog(LOG_WARNING, "DATA: invalid session path: %s", zip_path);
            failed_count++;
            continue;
        }

        // Construct local path
        snprintf(local_path, sizeof(local_path), "%s/%04d/%02d/%02d/%s",
                 data_root(), year, month, day, filename);

        // Import session file
        if(import_session_file(zip_path, local_path, &archive)) {
            imported_count++;
        } else {
            failed_count++;
        }
    }

    mz_zip_reader_end(&archive);

    if(imported_count > 0) {
        TraceLog(LOG_INFO, "DATA: import completed: %d imported, %d failed", imported_count, failed_count);
        return 1;
    } else {
        TraceLog(LOG_WARNING, "DATA: no sessions imported");
        return 0;
    }
}

typedef struct {
    data_session_callback user_callback;
    void *user_data;
} ListSessionContext;

static int
list_sessions_callback_ex(const char *path, int year, int month, int day, void *user_data)
{
    ListSessionContext *ctx = user_data;
    char time_str[16] = "00:00:00";
    int best = 0;
    int rounds;
    char date_str[16];

    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", year, month, day);

    const char *filename = GetFileName(path);
    if(strlen(filename) >= 10) {
        snprintf(time_str, sizeof(time_str), "%c%c:%c%c:%c%c",
                 filename[5], filename[6],
                 filename[7], filename[8],
                 filename[9], filename[10]);
    }

    rounds = analyze_session_file(path, &best);
    ctx->user_callback(date_str, time_str, rounds, best, ctx->user_data);
    return 1;
}

void
data_list_sessions(data_session_callback callback, void *user)
{
    ListSessionContext ctx = {callback, user};
    if(callback != NULL)
        iterate_session_dates_ex(list_sessions_callback_ex, &ctx);
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
