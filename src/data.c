#include "data.h"
#include "miniz.h"
#include "version.h"

#include "raylib.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FS_PATH_MAX 512

/* Suppress GCC format-truncation warnings - paths are safely sized in practice */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

/* ================================================================
 * INTERNAL STATE
 * ================================================================ */

static char g_data_root[FS_PATH_MAX] = "";
static char g_today_dir[FS_PATH_MAX] = "";

/* ================================================================
 * INTERNAL HELPER FUNCTIONS
 * ================================================================ */

/* Convert a 3-digit count string to integer
 * String format: "000" to "999" */
static int __attribute__((unused))
int_from_count(const char src[4])
{
    int a = (src[0] >= '0' && src[0] <= '9') ? src[0] - '0' : 0;
    int b = (src[1] >= '0' && src[1] <= '9') ? src[1] - '0' : 0;
    int c = (src[2] >= '0' && src[2] <= '9') ? src[2] - '0' : 0;
    return a * 100 + b * 10 + c;
}

/* Ensure directory exists (creates parent directories recursively)
 * Returns: 1 if directory exists or was created, 0 on failure */
static int
ensure_dir(const char *path)
{
    char temp[FS_PATH_MAX];
    char *p;

    if(path == NULL || path[0] == '\0')
        return 0;

    TraceLog(LOG_INFO, "DATA: ensure_dir: %s", path);

    if(DirectoryExists(path))
        return 1;

    /* Make a temporary copy to modify */
    strncpy(temp, path, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    /* Create parent directories first */
    p = temp;
    if(p[0] == '/') p++; /* Skip root if absolute path */

    while((p = strchr(p, '/')) != NULL) {
        *p = '\0';
        if(!DirectoryExists(temp)) {
            TraceLog(LOG_INFO, "DATA: Creating directory: %s", temp);
            if(!MakeDirectory(temp)) {
                /* Parent directory creation failed */
                TraceLog(LOG_ERROR, "DATA: Failed to create directory: %s", temp);
                *p = '/';
                return 0;
            }
        }
        *p = '/';
        p++;
    }

    /* Now create the final directory */
    TraceLog(LOG_INFO, "DATA: Creating final directory: %s", path);
    if(MakeDirectory(path))
        return 1;

    TraceLog(LOG_ERROR, "DATA: failed to create directory: %s", path);
    return 0;
}

/* Check if a path refers to a session file (starts with "inbe-")
 * Handles both filenames and full paths */
static int
is_session_file(const char *path)
{
    const char *filename;

    if(path == NULL)
        return 0;

    /* Extract filename from full path */
    filename = GetFileName(path);

    return strncmp(filename, "inbe-", 5) == 0;
}

/* Get file size in bytes
 * Returns: File size in bytes, or -1 on error */
static long long
get_file_size(const char *path)
{
#if defined(PLATFORM_WEB)
    /* Web platform: use JS to get file size */
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

/* Read a session file and parse round times
 * path: Full path to session file
 * round_times: Output array for round times
 * max_rounds: Maximum rounds to read
 * Returns: Number of rounds read, or -1 on error */
static int
read_session_file(const char *path, int *round_times, int max_rounds)
{
    char *content;
    int round_count = 0;

    content = LoadFileText(path);
    if(content == NULL)
        return -1;

    /* Parse each line as a round time */
    char *line = content;
    char *next_line;
    while(line != NULL && round_count < max_rounds) {
        next_line = strchr(line, '\n');
        if(next_line != NULL) {
            *next_line = '\0';
            next_line++;
        }

        /* Skip empty lines */
        while(*line == ' ' || *line == '\t')
            line++;

        if(*line != '\0') {
            int seconds = atoi(line);
            if(seconds >= 0 && seconds <= 999) {
                round_times[round_count++] = seconds;
            }
        }

        line = next_line;
    }

    UnloadFileText(content);
    return round_count;
}

/* Count total rounds and find best time in a session file
 * Returns: Number of rounds, sets *best to best time */
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

/* ================================================================
 * PUBLIC API
 * ================================================================ */

void
data_init(void)
{
    /* Initialize on first call to data_root() */
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
    /* On Android, use simple relative path - Raylib handles storage */
    snprintf(g_data_root, sizeof(g_data_root), "lotus");
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

    /* Ensure root directory exists */
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
data_save_session(const int *round_times, int round_count)
{
    time_t now;
    struct tm *tm;
    char dir_year[FS_PATH_MAX];
    char dir_month[FS_PATH_MAX];
    char dir_day[FS_PATH_MAX];
    char path[FS_PATH_MAX];
    char text[MaxRounds * 8];
    int offset = 0;

    if(round_times == NULL || round_count <= 0 || round_count > MaxRounds)
        return 0;

    now = time(NULL);
    tm = localtime(&now);
    if(tm == NULL)
        return 0;

    /* Create directory structure: YYYY/MM/DD */
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

    /* Create session filename: inbe-HHMMSS */
    snprintf(path, sizeof(path), "%s/inbe-%02d%02d%02d",
             dir_day, tm->tm_hour, tm->tm_min, tm->tm_sec);

    /* Build session content */
    for(int i = 0; i < round_count; i++) {
        if(offset >= (int)sizeof(text) - 8)
            break;
        offset += snprintf(text + offset, sizeof(text) - (size_t)offset,
                           "%d\n", round_times[i]);
    }

    TraceLog(LOG_INFO, "DATA: saving session to %s", path);
    if(SaveFileText(path, text)) {
        TraceLog(LOG_INFO, "DATA: saved session to %s", path);
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
data_has_any(void)
{
    int year, month, day;
    FilePathList files;
    int has_data = 0;

    /* Scan year directories (1970-2100 covers reasonable range) */
    for(year = 1970; year <= 2100; year++) {
        char year_path[FS_PATH_MAX];
        snprintf(year_path, sizeof(year_path), "%s/%04d", data_root(), year);

        if(!DirectoryExists(year_path))
            continue;

        /* Scan month directories */
        for(month = 1; month <= 12; month++) {
            char month_path[FS_PATH_MAX];
            snprintf(month_path, sizeof(month_path), "%s/%02d", year_path, month);

            if(!DirectoryExists(month_path))
                continue;

            /* Scan day directories */
            for(day = 1; day <= 31; day++) {
                char day_path[FS_PATH_MAX];
                snprintf(day_path, sizeof(day_path), "%s/%02d", month_path, day);

                if(!DirectoryExists(day_path))
                    continue;

                /* Check for session files */
                files = LoadDirectoryFiles(day_path);
                for(unsigned int i = 0; i < files.count; i++) {
                    if(is_session_file(files.paths[i])) {
                        has_data = 1;
                        break;
                    }
                }
                UnloadDirectoryFiles(files);

                if(has_data)
                    break;
            }

            if(has_data)
                break;
        }

        if(has_data)
            break;
    }

    return has_data;
}

long long
data_get_total_size(void)
{
    long long total = 0;
    int year, month, day;
    FilePathList files;

    /* Scan all year/month/day directories and sum file sizes */
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
                        long long size = get_file_size(files.paths[i]);
                        if(size > 0)
                            total += size;
                    }
                }
                UnloadDirectoryFiles(files);
            }
        }
    }

    return total;
}

int
data_get_session_count(void)
{
    int count = 0;
    int year, month, day;
    FilePathList files;

    /* Count all session files */
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
                    if(is_session_file(files.paths[i]))
                        count++;
                }
                UnloadDirectoryFiles(files);
            }
        }
    }

    return count;
}

long long
data_delete_all(void)
{
    long long deleted = 0;
    int year, month, day;
    FilePathList files;

    /* Delete all session files */
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
                        if(FileExists(files.paths[i])) {
                            /* Delete the file */
#if defined(PLATFORM_WEB)
                            EM_ASM({
                                try {
                                    FS.unlink(UTF8ToString($0));
                                } catch(e) {}
                            }, files.paths[i]);
#else
                            remove(files.paths[i]);
#endif
                            deleted++;
                        }
                    }
                }
                UnloadDirectoryFiles(files);
            }
        }
    }

    if(deleted > 0)
        TraceLog(LOG_INFO, "DATA: deleted %lld session files", deleted);

    return deleted;
}

int
data_export(const char *path)
{
    mz_zip_archive archive;
    FILE *fp;
    void *zip_data;
    size_t zip_size;
    int year, month, day;
    FilePathList files;
    int session_count = 0;
    char metadata[512];
    time_t now;
    struct tm *tm;
    char date_str[64];

    if(path == NULL || path[0] == '\0') {
        TraceLog(LOG_ERROR, "DATA: export path is empty");
        return 0;
    }

    /* Check if any data exists */
    if(!data_has_any()) {
        TraceLog(LOG_WARNING, "DATA: no data to export");
        return 0;
    }

    /* Initialize ZIP archive */
    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_writer_init_heap(&archive, 0, 0)) {
        TraceLog(LOG_ERROR, "DATA: failed to initialize ZIP archive");
        return 0;
    }

    session_count = data_get_session_count();

    /* Create metadata file */
    now = time(NULL);
    tm = localtime(&now);
    if(tm != NULL) {
        snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d %02d:%02d:%02d",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else {
        strcpy(date_str, "Unknown");
    }

    snprintf(metadata, sizeof(metadata),
             "Inner Breeze Data Export\n"
             "Version: %s\n"
             "Export Date: %s\n"
             "Session Count: %d\n",
             INBE_VERSION_STRING,
             date_str,
             session_count);

    if(!mz_zip_writer_add_mem(&archive, "lotus-data/metadata.txt", metadata, strlen(metadata), MZ_NO_COMPRESSION)) {
        TraceLog(LOG_ERROR, "DATA: failed to write metadata");
        mz_zip_writer_end(&archive);
        return 0;
    }

    /* Add all session files to ZIP */
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
                        char *content;
                        char zip_path[FS_PATH_MAX];
                        const char *filename = GetFileName(files.paths[i]);

                        /* Build path inside ZIP */
                        snprintf(zip_path, sizeof(zip_path),
                                 "lotus-data/sessions/%04d/%02d/%02d/%s",
                                 year, month, day, filename);

                        /* Read file content */
                        content = LoadFileText(files.paths[i]);
                        if(content != NULL) {
                            size_t size = strlen(content);
                            if(!mz_zip_writer_add_mem(&archive, zip_path, content, size, MZ_NO_COMPRESSION)) {
                                TraceLog(LOG_WARNING, "DATA: failed to add file: %s", files.paths[i]);
                            }
                            UnloadFileText(content);
                        }
                    }
                }
                UnloadDirectoryFiles(files);
            }
        }
    }

    /* Finalize heap archive and get buffer */
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

    /* Write to file */
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
}

int
data_import(const char *path)
{
    /* TODO: Implement ZIP import
     * For now, this is a placeholder */
    TraceLog(LOG_WARNING, "DATA: import not yet implemented");
    (void)path;
    return 0;
}

void
data_list_sessions(data_session_callback callback, void *user)
{
    int year, month, day;
    FilePathList files;

    if(callback == NULL)
        return;

    /* Scan all directories and call callback for each session */
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
                char date_str[16];
                snprintf(day_path, sizeof(day_path), "%s/%02d", month_path, day);

                if(!DirectoryExists(day_path))
                    continue;

                snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d",
                         year, month, day);

                files = LoadDirectoryFiles(day_path);
                for(unsigned int i = 0; i < files.count; i++) {
                    if(is_session_file(files.paths[i])) {
                        char time_str[16] = "00:00:00";
                        int best = 0;
                        int rounds;

                        /* Parse time from filename (inbe-HHMMSS) */
                        const char *filename = GetFileName(files.paths[i]);
                        if(strlen(filename) >= 10) {
                            snprintf(time_str, sizeof(time_str), "%c%c:%c%c:%c%c",
                                     filename[5], filename[6],
                                     filename[7], filename[8],
                                     filename[9], filename[10]);
                        }

                        /* Analyze session file */
                        rounds = analyze_session_file(files.paths[i], &best);

                        /* Call callback */
                        callback(date_str, time_str, rounds, best, user);
                    }
                }
                UnloadDirectoryFiles(files);
            }
        }
    }
}

#pragma GCC diagnostic pop
