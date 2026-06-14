#include "storage.h"

#include "habits/habits.h"
#include "inbe.h"
#include "miniz.h"
#include "version.h"
#include "../vendor/rini/src/rini.h"

#include "raylib.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

typedef struct StorageState {
    sqlite3 *db;
    char root[INBE_STORAGE_PATH_SIZE];
    char db_path[INBE_STORAGE_PATH_SIZE];
    char user_id[INBE_STORAGE_ID_SIZE];
    char text_value[256];
} StorageState;

static StorageState g_storage;

static int
path_exists(const char *path)
{
    struct stat st;
    return path != NULL && stat(path, &st) == 0;
}

static int
dir_exists_local(const char *path)
{
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int
ensure_dir_local(const char *path)
{
    char temp[INBE_STORAGE_PATH_SIZE];
    char *p;

    if(path == NULL || path[0] == '\0')
        return 0;
    if(dir_exists_local(path))
        return 1;

    snprintf(temp, sizeof(temp), "%s", path);
    p = temp;
    if(*p == '/')
        p++;
    while((p = strchr(p, '/')) != NULL) {
        *p = '\0';
        if(temp[0] != '\0' && !dir_exists_local(temp) && mkdir(temp, 0700) != 0 && !dir_exists_local(temp)) {
            *p = '/';
            return 0;
        }
        *p = '/';
        p++;
    }

    return mkdir(path, 0700) == 0 || dir_exists_local(path);
}

static int
exec_sql(const char *sql)
{
    char *error = NULL;
    if(sqlite3_exec(g_storage.db, sql, NULL, NULL, &error) != SQLITE_OK) {
        TraceLog(LOG_ERROR, "STORAGE: SQL failed: %s", error != NULL ? error : "unknown");
        sqlite3_free(error);
        return 0;
    }
    return 1;
}

static long long
now_seconds(void)
{
    return (long long)time(NULL);
}

static unsigned int
hash_rounds(const int *round_times, int round_count)
{
    unsigned int h = 2166136261u;
    for(int i = 0; i < round_count; i++) {
        unsigned int v = (unsigned int)round_times[i];
        h ^= v & 0xffu;
        h *= 16777619u;
        h ^= (v >> 8) & 0xffu;
        h *= 16777619u;
    }
    return h;
}

static void
make_user_id(char *out, size_t out_size)
{
    unsigned int r = (unsigned int)rand();
    snprintf(out, out_size, "local-%lld-%u", now_seconds(), r);
}

static void
make_session_id(long long started_at, const int *round_times, int round_count,
                char *out, size_t out_size)
{
    snprintf(out, out_size, "s-%lld-%08x", started_at, hash_rounds(round_times, round_count));
}

static int
parse_db_id(const char *path_or_id, char *out, size_t out_size)
{
    if(path_or_id == NULL || path_or_id[0] == '\0')
        return 0;
    if(strncmp(path_or_id, "db:", 3) == 0)
        snprintf(out, out_size, "%s", path_or_id + 3);
    else
        snprintf(out, out_size, "%s", path_or_id);
    return out[0] != '\0';
}

static int
bind_text(sqlite3_stmt *stmt, int index, const char *text)
{
    return sqlite3_bind_text(stmt, index, text != NULL ? text : "", -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

static int
schema_create(void)
{
    return exec_sql(
        "PRAGMA journal_mode=DELETE;"
        "PRAGMA foreign_keys=ON;"
        "CREATE TABLE IF NOT EXISTS meta("
        " key TEXT PRIMARY KEY,"
        " value TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS users("
        " id TEXT PRIMARY KEY,"
        " created_at INTEGER NOT NULL,"
        " kind TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS settings("
        " user_id TEXT NOT NULL,"
        " key TEXT NOT NULL,"
        " value TEXT NOT NULL,"
        " updated_at INTEGER NOT NULL,"
        " PRIMARY KEY(user_id,key)"
        ");"
        "CREATE TABLE IF NOT EXISTS sessions("
        " id TEXT PRIMARY KEY,"
        " user_id TEXT NOT NULL,"
        " started_at INTEGER NOT NULL,"
        " local_date INTEGER NOT NULL,"
        " topic INTEGER NOT NULL DEFAULT 0,"
        " activity INTEGER NOT NULL DEFAULT 0,"
        " source TEXT NOT NULL,"
        " imported_at INTEGER NOT NULL,"
        " rounds_hash INTEGER NOT NULL,"
        " deleted_at INTEGER NOT NULL DEFAULT 0,"
        " UNIQUE(user_id,started_at,rounds_hash)"
        ");"
        "CREATE TABLE IF NOT EXISTS session_rounds("
        " session_id TEXT NOT NULL,"
        " round_index INTEGER NOT NULL,"
        " seconds INTEGER NOT NULL,"
        " PRIMARY KEY(session_id,round_index)"
        ");"
        "CREATE TABLE IF NOT EXISTS habits("
        " id TEXT PRIMARY KEY,"
        " user_id TEXT NOT NULL,"
        " name TEXT NOT NULL,"
        " color_r INTEGER NOT NULL,"
        " color_g INTEGER NOT NULL,"
        " color_b INTEGER NOT NULL,"
        " sync_mode INTEGER NOT NULL,"
        " sync_topic INTEGER NOT NULL,"
        " sync_activity INTEGER NOT NULL,"
        " sort_order INTEGER NOT NULL,"
        " deleted_at INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS habit_days("
        " habit_id TEXT NOT NULL,"
        " local_date INTEGER NOT NULL,"
        " completed INTEGER NOT NULL,"
        " updated_at INTEGER NOT NULL,"
        " PRIMARY KEY(habit_id,local_date)"
        ");"
        "CREATE TABLE IF NOT EXISTS migration_sources("
        " path TEXT PRIMARY KEY,"
        " source_kind TEXT NOT NULL,"
        " migrated_at INTEGER NOT NULL,"
        " status TEXT NOT NULL,"
        " detail TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS imports("
        " id TEXT PRIMARY KEY,"
        " imported_at INTEGER NOT NULL,"
        " format TEXT NOT NULL,"
        " source_name TEXT NOT NULL,"
        " session_count INTEGER NOT NULL,"
        " habit_count INTEGER NOT NULL"
        ");"
        "INSERT OR IGNORE INTO meta(key,value) VALUES('schema_version','1');");
}

static int
load_or_create_user(void)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if(sqlite3_prepare_v2(g_storage.db, "SELECT id FROM users WHERE kind='local' ORDER BY created_at LIMIT 1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    rc = sqlite3_step(stmt);
    if(rc == SQLITE_ROW) {
        snprintf(g_storage.user_id, sizeof(g_storage.user_id), "%s",
                 (const char *)sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        return 1;
    }
    sqlite3_finalize(stmt);

    make_user_id(g_storage.user_id, sizeof(g_storage.user_id));
    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT INTO users(id,created_at,kind) VALUES(?1,?2,'local')",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    sqlite3_bind_int64(stmt, 2, now_seconds());
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

static int
meta_equals(const char *key, const char *value)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if(sqlite3_prepare_v2(g_storage.db, "SELECT value FROM meta WHERE key=?1", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, key);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *text = (const char *)sqlite3_column_text(stmt, 0);
        ok = text != NULL && strcmp(text, value) == 0;
    }
    sqlite3_finalize(stmt);
    return ok;
}

static void
set_meta(const char *key, const char *value)
{
    sqlite3_stmt *stmt = NULL;
    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT INTO meta(key,value) VALUES(?1,?2) "
                          "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
                          -1, &stmt, NULL) != SQLITE_OK)
        return;
    bind_text(stmt, 1, key);
    bind_text(stmt, 2, value);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int
inbe_storage_settings_empty(void)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;

    if(g_storage.db == NULL)
        return 1;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COUNT(*) FROM settings WHERE user_id=?1", -1, &stmt, NULL) != SQLITE_OK)
        return 1;
    bind_text(stmt, 1, g_storage.user_id);
    if(sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count == 0;
}

int
inbe_storage_get_setting_int(const char *key, int fallback)
{
    const char *text = inbe_storage_get_setting_text(key);
    return text != NULL && text[0] != '\0' ? atoi(text) : fallback;
}

const char *
inbe_storage_get_setting_text(const char *key)
{
    sqlite3_stmt *stmt = NULL;
    g_storage.text_value[0] = '\0';

    if(g_storage.db == NULL || key == NULL)
        return NULL;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT value FROM settings WHERE user_id=?1 AND key=?2",
                          -1, &stmt, NULL) != SQLITE_OK)
        return NULL;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, key);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *text = (const char *)sqlite3_column_text(stmt, 0);
        snprintf(g_storage.text_value, sizeof(g_storage.text_value), "%s", text != NULL ? text : "");
    }
    sqlite3_finalize(stmt);
    return g_storage.text_value[0] != '\0' ? g_storage.text_value : NULL;
}

void
inbe_storage_set_setting_text(const char *key, const char *value)
{
    sqlite3_stmt *stmt = NULL;
    if(g_storage.db == NULL || key == NULL)
        return;
    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT INTO settings(user_id,key,value,updated_at) VALUES(?1,?2,?3,?4) "
                          "ON CONFLICT(user_id,key) DO UPDATE SET value=excluded.value,updated_at=excluded.updated_at",
                          -1, &stmt, NULL) != SQLITE_OK)
        return;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, key);
    bind_text(stmt, 3, value != NULL ? value : "");
    sqlite3_bind_int64(stmt, 4, now_seconds());
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void
inbe_storage_set_setting_int(const char *key, int value)
{
    char text[32];
    snprintf(text, sizeof(text), "%d", value);
    inbe_storage_set_setting_text(key, text);
}

static int
insert_session_at(long long started_at, int local_date, const int *round_times,
                  int round_count, const char *source, char *out_id, size_t out_id_size)
{
    sqlite3_stmt *stmt = NULL;
    char id[INBE_STORAGE_ID_SIZE];
    int rc;
    unsigned int rhash;

    if(g_storage.db == NULL || round_times == NULL || round_count <= 0 || round_count > MaxRounds)
        return 0;

    rhash = hash_rounds(round_times, round_count);
    make_session_id(started_at, round_times, round_count, id, sizeof(id));

    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT OR IGNORE INTO sessions(id,user_id,started_at,local_date,source,imported_at,rounds_hash) "
                          "VALUES(?1,?2,?3,?4,?5,?6,?7)",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, id);
    bind_text(stmt, 2, g_storage.user_id);
    sqlite3_bind_int64(stmt, 3, started_at);
    sqlite3_bind_int(stmt, 4, local_date);
    bind_text(stmt, 5, source != NULL ? source : "app");
    sqlite3_bind_int64(stmt, 6, now_seconds());
    sqlite3_bind_int64(stmt, 7, (sqlite3_int64)rhash);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if(rc != SQLITE_DONE)
        return 0;

    if(sqlite3_changes(g_storage.db) > 0) {
        for(int i = 0; i < round_count; i++) {
            if(sqlite3_prepare_v2(g_storage.db,
                                  "INSERT OR REPLACE INTO session_rounds(session_id,round_index,seconds) VALUES(?1,?2,?3)",
                                  -1, &stmt, NULL) != SQLITE_OK)
                return 0;
            bind_text(stmt, 1, id);
            sqlite3_bind_int(stmt, 2, i);
            sqlite3_bind_int(stmt, 3, round_times[i]);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    if(out_id != NULL && out_id_size > 0)
        snprintf(out_id, out_id_size, "db:%s", id);
    return 1;
}

int
inbe_storage_save_session(const int *round_times, int round_count, char *out_id, size_t out_id_size)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int saved[MaxRounds];
    int saved_count = 0;
    int local_date;

    if(tm == NULL)
        return 0;
    for(int i = 0; i < round_count && i < MaxRounds; i++) {
        if(round_times[i] > 0)
            saved[saved_count++] = round_times[i];
    }
    if(saved_count <= 0)
        return 0;
    local_date = (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday;
    return insert_session_at((long long)now, local_date, saved, saved_count, "app", out_id, out_id_size);
}

int
inbe_storage_load_session(const char *path_or_id, int *round_times, int max_rounds,
                          int *year, int *month, int *day,
                          int *hour, int *minute, int *second)
{
    sqlite3_stmt *stmt = NULL;
    char id[INBE_STORAGE_ID_SIZE];
    int count = 0;
    long long started_at = 0;
    int local_date = 0;

    if(!parse_db_id(path_or_id, id, sizeof(id)) || round_times == NULL || max_rounds <= 0)
        return 0;

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT started_at,local_date FROM sessions WHERE id=?1 AND deleted_at=0",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, id);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        started_at = sqlite3_column_int64(stmt, 0);
        local_date = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);
    if(started_at == 0)
        return 0;

    if(year) *year = local_date / 10000;
    if(month) *month = (local_date / 100) % 100;
    if(day) *day = local_date % 100;
    {
        time_t t = (time_t)started_at;
        struct tm *tm = localtime(&t);
        if(tm != NULL) {
            if(hour) *hour = tm->tm_hour;
            if(minute) *minute = tm->tm_min;
            if(second) *second = tm->tm_sec;
        }
    }

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT seconds FROM session_rounds WHERE session_id=?1 ORDER BY round_index",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, id);
    while(count < max_rounds && sqlite3_step(stmt) == SQLITE_ROW)
        round_times[count++] = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

int
inbe_storage_replace_session(const char *path_or_id, const int *round_times, int round_count)
{
    sqlite3_stmt *stmt = NULL;
    char id[INBE_STORAGE_ID_SIZE];
    int saved[MaxRounds];
    int saved_count = 0;

    if(!parse_db_id(path_or_id, id, sizeof(id)) || round_times == NULL || round_count < 0 || round_count > MaxRounds)
        return 0;
    for(int i = 0; i < round_count; i++) {
        if(round_times[i] > 0)
            saved[saved_count++] = round_times[i];
    }
    if(saved_count <= 0)
        return inbe_storage_delete_session(path_or_id);

    exec_sql("BEGIN IMMEDIATE");
    if(sqlite3_prepare_v2(g_storage.db, "DELETE FROM session_rounds WHERE session_id=?1", -1, &stmt, NULL) != SQLITE_OK)
        goto fail;
    bind_text(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    stmt = NULL;
    for(int i = 0; i < saved_count; i++) {
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT INTO session_rounds(session_id,round_index,seconds) VALUES(?1,?2,?3)",
                              -1, &stmt, NULL) != SQLITE_OK)
            goto fail;
        bind_text(stmt, 1, id);
        sqlite3_bind_int(stmt, 2, i);
        sqlite3_bind_int(stmt, 3, saved[i]);
        if(sqlite3_step(stmt) != SQLITE_DONE)
            goto fail;
        sqlite3_finalize(stmt);
        stmt = NULL;
    }
    if(sqlite3_prepare_v2(g_storage.db, "UPDATE sessions SET rounds_hash=?2 WHERE id=?1", -1, &stmt, NULL) != SQLITE_OK)
        goto fail;
    bind_text(stmt, 1, id);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)hash_rounds(saved, saved_count));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    exec_sql("COMMIT");
    return 1;

fail:
    if(stmt != NULL)
        sqlite3_finalize(stmt);
    exec_sql("ROLLBACK");
    return 0;
}

int
inbe_storage_rename_session_time(const char *path_or_id, int hour, int minute)
{
    sqlite3_stmt *stmt = NULL;
    char id[INBE_STORAGE_ID_SIZE];
    long long started_at = 0;
    time_t t;
    struct tm *tm;

    if(!parse_db_id(path_or_id, id, sizeof(id)) || hour < 0 || hour > 23 || minute < 0 || minute > 59)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT started_at FROM sessions WHERE id=?1", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, id);
    if(sqlite3_step(stmt) == SQLITE_ROW)
        started_at = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    if(started_at == 0)
        return 0;
    t = (time_t)started_at;
    tm = localtime(&t);
    if(tm == NULL)
        return 0;
    tm->tm_hour = hour;
    tm->tm_min = minute;
    t = mktime(tm);
    if(t == (time_t)-1)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "UPDATE sessions SET started_at=?2 WHERE id=?1", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, id);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)t);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 1;
}

int
inbe_storage_delete_session(const char *path_or_id)
{
    sqlite3_stmt *stmt = NULL;
    char id[INBE_STORAGE_ID_SIZE];
    if(!parse_db_id(path_or_id, id, sizeof(id)))
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "UPDATE sessions SET deleted_at=?2 WHERE id=?1", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, id);
    sqlite3_bind_int64(stmt, 2, now_seconds());
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 1;
}

void
inbe_storage_list_history(InbeStorageHistoryCallback callback, void *user)
{
    sqlite3_stmt *stmt = NULL;
    if(callback == NULL || g_storage.db == NULL)
        return;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id,started_at,local_date FROM sessions WHERE deleted_at=0 ORDER BY started_at DESC LIMIT 48",
                          -1, &stmt, NULL) != SQLITE_OK)
        return;
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        char dbid[INBE_STORAGE_ID_SIZE + 4];
        int rounds[MaxRounds];
        int count = 0;
        const char *id = (const char *)sqlite3_column_text(stmt, 0);
        long long started_at = sqlite3_column_int64(stmt, 1);
        int local_date = sqlite3_column_int(stmt, 2);
        int y = local_date / 10000;
        int m = (local_date / 100) % 100;
        int d = local_date % 100;
        int hh = 0, mm = 0, ss = 0;
        time_t t = (time_t)started_at;
        struct tm *tm = localtime(&t);
        if(tm != NULL) {
            hh = tm->tm_hour;
            mm = tm->tm_min;
            ss = tm->tm_sec;
        }
        snprintf(dbid, sizeof(dbid), "db:%s", id != NULL ? id : "");
        count = inbe_storage_load_session(dbid, rounds, MaxRounds, NULL, NULL, NULL, NULL, NULL, NULL);
        if(count > 0)
            callback(dbid, y, m, d, hh, mm, ss, rounds, count, user);
    }
    sqlite3_finalize(stmt);
}

int
inbe_storage_has_any(void)
{
    return inbe_storage_session_count() > 0;
}

int
inbe_storage_session_count(void)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;
    if(g_storage.db == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COUNT(*) FROM sessions WHERE deleted_at=0", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    if(sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

long long
inbe_storage_total_size(void)
{
    struct stat st;
    if(g_storage.db_path[0] != '\0' && stat(g_storage.db_path, &st) == 0)
        return (long long)st.st_size;
    return 0;
}

long long
inbe_storage_delete_all_sessions(void)
{
    int count = inbe_storage_session_count();
    exec_sql("UPDATE sessions SET deleted_at=strftime('%s','now') WHERE deleted_at=0");
    return count;
}

static int
read_legacy_session_file(const char *path, int *rounds, int max_rounds)
{
    FILE *fp = fopen(path, "r");
    int value;
    int count = 0;
    if(fp == NULL)
        return 0;
    while(count < max_rounds && fscanf(fp, "%d", &value) == 1) {
        if(value > 0 && value <= 999)
            rounds[count++] = value;
    }
    fclose(fp);
    return count;
}

static long long
legacy_started_at(int year, int month, int day, const char *filename)
{
    struct tm tmv;
    int hh = 0, mm = 0, ss = 0;
    memset(&tmv, 0, sizeof(tmv));
    if(filename == NULL || sscanf(filename, "inbe-%2d%2d%2d", &hh, &mm, &ss) != 3)
        return 0;
    tmv.tm_year = year - 1900;
    tmv.tm_mon = month - 1;
    tmv.tm_mday = day;
    tmv.tm_hour = hh;
    tmv.tm_min = mm;
    tmv.tm_sec = ss;
    tmv.tm_isdst = -1;
    return (long long)mktime(&tmv);
}

static int
archive_file(mz_zip_archive *archive, const char *root, const char *path)
{
    FILE *fp;
    char *buf;
    long size;
    const char *rel = path;
    if(strncmp(path, root, strlen(root)) == 0) {
        rel = path + strlen(root);
        if(*rel == '/')
            rel++;
    }
    fp = fopen(path, "rb");
    if(fp == NULL)
        return 0;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if(size < 0) {
        fclose(fp);
        return 0;
    }
    buf = malloc((size_t)size + 1);
    if(buf == NULL) {
        fclose(fp);
        return 0;
    }
    if(fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    mz_zip_writer_add_mem(archive, rel, buf, (size_t)size, MZ_NO_COMPRESSION);
    free(buf);
    return 1;
}

static void
record_migration_source(const char *path, const char *kind, const char *status, const char *detail)
{
    sqlite3_stmt *stmt = NULL;
    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT OR REPLACE INTO migration_sources(path,source_kind,migrated_at,status,detail) "
                          "VALUES(?1,?2,?3,?4,?5)",
                          -1, &stmt, NULL) != SQLITE_OK)
        return;
    bind_text(stmt, 1, path);
    bind_text(stmt, 2, kind);
    sqlite3_bind_int64(stmt, 3, now_seconds());
    bind_text(stmt, 4, status);
    bind_text(stmt, 5, detail);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void
migrate_settings_file(const char *path, int overwrite)
{
    rini_data data;
    static const char *keys[] = {
        "speed", "max_rounds", "max_breaths", "pause_seconds", "sound_volume",
        "tutorial_seen", "exercise_manual_seen_mask", "theme", "dark_mode",
        "fullscreen", "on_screen_keyboard", "progressive_speed", "progressive_start_speed",
        "advanced_session_controls", "hold_display_mode", "exercise_type",
        "meditation_music_enabled", "meditation_music_shuffle", "meditation_music_track",
        "play_in_background", "language", "practice_tab_enabled_mask",
        "practice_tab_mind_theme", "practice_tab_yoga_theme", "practice_tab_fitness_theme",
        "practice_category_tab"
    };

    if(!path_exists(path))
        return;
    data = rini_load(path);
    for(size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        const char *value = rini_get_value_text(data, keys[i]);
        if(value == NULL)
            continue;
        if(!overwrite && inbe_storage_get_setting_text(keys[i]) != NULL)
            continue;
        inbe_storage_set_setting_text(keys[i], value);
    }
    rini_unload(&data);
    record_migration_source(path, "settings", "migrated", "");
}

static void
migrate_habits_file(const char *path, int overwrite)
{
    rini_data data;
    int count;
    sqlite3_stmt *stmt = NULL;

    if(!path_exists(path))
        return;
    if(!overwrite && !inbe_storage_habits_empty())
        return;

    data = rini_load(path);
    count = rini_get_value_fallback(data, "count", 0);
    if(count < 0)
        count = 0;
    if(count > INBE_HABIT_MAX)
        count = INBE_HABIT_MAX;
    exec_sql("DELETE FROM habit_days; DELETE FROM habits;");
    for(int i = 0; i < count; i++) {
        char key[96];
        const char *id;
        const char *name;
        int r, g, b, sync_mode, sync_topic, sync_activity, day_count;

        snprintf(key, sizeof(key), "habit_%d_id", i);
        id = rini_get_value_text(data, key);
        snprintf(key, sizeof(key), "habit_%d_name", i);
        name = rini_get_value_text(data, key);
        snprintf(key, sizeof(key), "habit_%d_color_r", i);
        r = rini_get_value_fallback(data, key, 99);
        snprintf(key, sizeof(key), "habit_%d_color_g", i);
        g = rini_get_value_fallback(data, key, 196);
        snprintf(key, sizeof(key), "habit_%d_color_b", i);
        b = rini_get_value_fallback(data, key, 165);
        snprintf(key, sizeof(key), "habit_%d_sync_mode", i);
        sync_mode = rini_get_value_fallback(data, key, INBE_HABIT_SYNC_NONE);
        snprintf(key, sizeof(key), "habit_%d_sync_topic", i);
        sync_topic = rini_get_value_fallback(data, key, INBE_HABIT_TOPIC_MIND);
        snprintf(key, sizeof(key), "habit_%d_sync_activity", i);
        sync_activity = rini_get_value_fallback(data, key, 0);

        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT OR REPLACE INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_topic,sync_activity,sort_order,deleted_at) "
                              "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,0)",
                              -1, &stmt, NULL) != SQLITE_OK)
            continue;
        bind_text(stmt, 1, id != NULL && id[0] != '\0' ? id : "habit");
        bind_text(stmt, 2, g_storage.user_id);
        bind_text(stmt, 3, name != NULL && name[0] != '\0' ? name : "Habit");
        sqlite3_bind_int(stmt, 4, r);
        sqlite3_bind_int(stmt, 5, g);
        sqlite3_bind_int(stmt, 6, b);
        sqlite3_bind_int(stmt, 7, sync_mode);
        sqlite3_bind_int(stmt, 8, sync_topic);
        sqlite3_bind_int(stmt, 9, sync_activity);
        sqlite3_bind_int(stmt, 10, i);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        stmt = NULL;

        snprintf(key, sizeof(key), "habit_%d_day_count", i);
        day_count = rini_get_value_fallback(data, key, 0);
        if(day_count < 0)
            day_count = 0;
        if(day_count > INBE_HABIT_MAX_DAYS)
            day_count = INBE_HABIT_MAX_DAYS;
        for(int d = 0; d < day_count; d++) {
            int day_index;
            int completed;
            snprintf(key, sizeof(key), "habit_%d_day_%d_index", i, d);
            day_index = rini_get_value_fallback(data, key, 0);
            snprintf(key, sizeof(key), "habit_%d_day_%d_completed", i, d);
            completed = rini_get_value_fallback(data, key, 0);
            if(day_index <= 0)
                continue;
            if(sqlite3_prepare_v2(g_storage.db,
                                  "INSERT OR REPLACE INTO habit_days(habit_id,local_date,completed,updated_at) VALUES(?1,?2,?3,?4)",
                                  -1, &stmt, NULL) != SQLITE_OK)
                continue;
            bind_text(stmt, 1, id != NULL && id[0] != '\0' ? id : "habit");
            sqlite3_bind_int(stmt, 2, day_index);
            sqlite3_bind_int(stmt, 3, completed ? 1 : 0);
            sqlite3_bind_int64(stmt, 4, now_seconds());
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            stmt = NULL;
        }
    }
    rini_unload(&data);
    record_migration_source(path, "habits", "migrated", "");
}

static void
scan_legacy_sessions(const char *root, mz_zip_archive *backup, int cleanup)
{
    char path[INBE_STORAGE_PATH_SIZE];

    if(root == NULL || !dir_exists_local(root))
        return;
    for(int y = 1970; y <= 2100; y++) {
        char ypath[INBE_STORAGE_PATH_SIZE];
        snprintf(ypath, sizeof(ypath), "%s/%04d", root, y);
        if(!dir_exists_local(ypath))
            continue;
        for(int m = 1; m <= 12; m++) {
            char mpath[INBE_STORAGE_PATH_SIZE];
            snprintf(mpath, sizeof(mpath), "%s/%02d", ypath, m);
            if(!dir_exists_local(mpath))
                continue;
            for(int d = 1; d <= 31; d++) {
                FilePathList files;
                char dpath[INBE_STORAGE_PATH_SIZE];
                snprintf(dpath, sizeof(dpath), "%s/%02d", mpath, d);
                if(!dir_exists_local(dpath))
                    continue;
                files = LoadDirectoryFiles(dpath);
                for(unsigned int i = 0; i < files.count; i++) {
                    const char *filename = GetFileName(files.paths[i]);
                    int rounds[MaxRounds];
                    int count;
                    long long started_at;
                    if(strncmp(filename, "inbe-", 5) != 0)
                        continue;
                    count = read_legacy_session_file(files.paths[i], rounds, MaxRounds);
                    started_at = legacy_started_at(y, m, d, filename);
                    if(count > 0 && started_at > 0) {
                        insert_session_at(started_at, y * 10000 + m * 100 + d, rounds, count, root, NULL, 0);
                        record_migration_source(files.paths[i], "session", "migrated", "");
                        if(backup != NULL)
                            archive_file(backup, root, files.paths[i]);
                        if(cleanup)
                            remove(files.paths[i]);
                    }
                }
                UnloadDirectoryFiles(files);
            }
        }
    }
    snprintf(path, sizeof(path), "%s/settings.ini", root);
    if(backup != NULL && path_exists(path))
        archive_file(backup, root, path);
    snprintf(path, sizeof(path), "%s/apps/habits/habits.ini", root);
    if(backup != NULL && path_exists(path))
        archive_file(backup, root, path);
}

static void
legacy_root(char *out, size_t out_size, const char *name)
{
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    if(strcmp(name, "lotus") == 0)
        snprintf(out, out_size, "/data/data/xyz.waozi.inbe/files/lotus");
    else
        snprintf(out, out_size, "%s/%s", GetWorkingDirectory(), name);
#else
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    if(xdg != NULL && xdg[0] != '\0')
        snprintf(out, out_size, "%s/%s", xdg, name);
    else if(home != NULL && home[0] != '\0')
        snprintf(out, out_size, "%s/.local/share/%s", home, name);
    else
        snprintf(out, out_size, ".local/%s", name);
#endif
}

static void
cleanup_known_legacy_files(const char *root)
{
    char path[INBE_STORAGE_PATH_SIZE];
    if(root == NULL || !dir_exists_local(root))
        return;
    snprintf(path, sizeof(path), "%s/settings.ini", root);
    remove(path);
    snprintf(path, sizeof(path), "%s/apps/habits/habits.ini", root);
    remove(path);
}

static void
migrate_legacy_files(void)
{
    char lotus[INBE_STORAGE_PATH_SIZE];
    char inbe[INBE_STORAGE_PATH_SIZE];
    char backup_dir[INBE_STORAGE_PATH_SIZE];
    char backup_path[INBE_STORAGE_PATH_SIZE];
    mz_zip_archive backup;
    int have_backup = 0;
    time_t now;
    struct tm *tm;

    if(meta_equals("legacy_file_migration_complete", "true"))
        return;

    legacy_root(lotus, sizeof(lotus), "lotus");
    snprintf(inbe, sizeof(inbe), "%s", g_storage.root);
    snprintf(backup_dir, sizeof(backup_dir), "%s/migration-backups", g_storage.root);
    ensure_dir_local(backup_dir);
    now = time(NULL);
    tm = localtime(&now);
    if(tm != NULL) {
        snprintf(backup_path, sizeof(backup_path), "%s/legacy-files-%04d%02d%02d-%02d%02d%02d.zip",
                 backup_dir, tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                 tm->tm_hour, tm->tm_min, tm->tm_sec);
    } else {
        snprintf(backup_path, sizeof(backup_path), "%s/legacy-files.zip", backup_dir);
    }

    memset(&backup, 0, sizeof(backup));
    have_backup = mz_zip_writer_init_file(&backup, backup_path, 0);

    exec_sql("BEGIN IMMEDIATE");
    {
        char path[INBE_STORAGE_PATH_SIZE];
        snprintf(path, sizeof(path), "%s/settings.ini", lotus);
        migrate_settings_file(path, 0);
        snprintf(path, sizeof(path), "%s/settings.ini", inbe);
        migrate_settings_file(path, 1);
        snprintf(path, sizeof(path), "%s/apps/habits/habits.ini", lotus);
        migrate_habits_file(path, 0);
        snprintf(path, sizeof(path), "%s/apps/habits/habits.ini", inbe);
        migrate_habits_file(path, 1);
    }
    scan_legacy_sessions(lotus, have_backup ? &backup : NULL, 1);
    scan_legacy_sessions(inbe, have_backup ? &backup : NULL, 1);
    exec_sql("COMMIT");

    if(have_backup)
        mz_zip_writer_finalize_archive(&backup);
    if(have_backup)
        mz_zip_writer_end(&backup);

    cleanup_known_legacy_files(lotus);
    cleanup_known_legacy_files(inbe);
    set_meta("legacy_file_migration_complete", "true");
}

int
inbe_storage_habits_empty(void)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COUNT(*) FROM habits WHERE deleted_at=0", -1, &stmt, NULL) != SQLITE_OK)
        return 1;
    if(sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count == 0;
}

int
inbe_storage_habits_load(void *habits_ptr)
{
    InbeHabits *habits = habits_ptr;
    sqlite3_stmt *stmt = NULL;
    int index = 0;

    if(habits == NULL || g_storage.db == NULL)
        return 0;
    memset(habits, 0, sizeof(*habits));
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id,name,color_r,color_g,color_b,sync_mode,sync_topic,sync_activity "
                          "FROM habits WHERE deleted_at=0 ORDER BY sort_order,id LIMIT 10",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    while(index < INBE_HABIT_MAX && sqlite3_step(stmt) == SQLITE_ROW) {
        InbeHabit *habit = &habits->items[index];
        snprintf(habit->id, sizeof(habit->id), "%s", (const char *)sqlite3_column_text(stmt, 0));
        snprintf(habit->name, sizeof(habit->name), "%s", (const char *)sqlite3_column_text(stmt, 1));
        habit->color = (Color){(unsigned char)sqlite3_column_int(stmt, 2),
                               (unsigned char)sqlite3_column_int(stmt, 3),
                               (unsigned char)sqlite3_column_int(stmt, 4), 255};
        habit->sync_mode = sqlite3_column_int(stmt, 5);
        habit->sync_topic = sqlite3_column_int(stmt, 6);
        habit->sync_activity = sqlite3_column_int(stmt, 7);
        index++;
    }
    sqlite3_finalize(stmt);
    habits->count = index;

    for(int i = 0; i < habits->count; i++) {
        if(sqlite3_prepare_v2(g_storage.db,
                              "SELECT local_date,completed FROM habit_days WHERE habit_id=?1 ORDER BY local_date LIMIT 366",
                              -1, &stmt, NULL) != SQLITE_OK)
            continue;
        bind_text(stmt, 1, habits->items[i].id);
        while(habits->items[i].day_count < INBE_HABIT_MAX_DAYS && sqlite3_step(stmt) == SQLITE_ROW) {
            int d = habits->items[i].day_count++;
            habits->items[i].days[d].day_index = sqlite3_column_int(stmt, 0);
            habits->items[i].days[d].completed = sqlite3_column_int(stmt, 1) != 0;
        }
        sqlite3_finalize(stmt);
    }
    habits->loaded = 1;
    return habits->count > 0 || meta_equals("habits_initialized", "true");
}

void
inbe_storage_mark_habits_initialized(void)
{
    if(g_storage.db != NULL)
        set_meta("habits_initialized", "true");
}

void
inbe_storage_habits_save(const void *habits_ptr)
{
    const InbeHabits *habits = habits_ptr;
    sqlite3_stmt *stmt = NULL;
    if(habits == NULL || g_storage.db == NULL)
        return;
    inbe_storage_mark_habits_initialized();
    exec_sql("BEGIN IMMEDIATE");
    exec_sql("DELETE FROM habit_days; DELETE FROM habits;");
    for(int i = 0; i < habits->count; i++) {
        const InbeHabit *habit = &habits->items[i];
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_topic,sync_activity,sort_order,deleted_at) "
                              "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,0)",
                              -1, &stmt, NULL) != SQLITE_OK)
            continue;
        bind_text(stmt, 1, habit->id);
        bind_text(stmt, 2, g_storage.user_id);
        bind_text(stmt, 3, habit->name);
        sqlite3_bind_int(stmt, 4, habit->color.r);
        sqlite3_bind_int(stmt, 5, habit->color.g);
        sqlite3_bind_int(stmt, 6, habit->color.b);
        sqlite3_bind_int(stmt, 7, habit->sync_mode);
        sqlite3_bind_int(stmt, 8, habit->sync_topic);
        sqlite3_bind_int(stmt, 9, habit->sync_activity);
        sqlite3_bind_int(stmt, 10, i);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        stmt = NULL;
        for(int d = 0; d < habit->day_count; d++) {
            if(sqlite3_prepare_v2(g_storage.db,
                                  "INSERT INTO habit_days(habit_id,local_date,completed,updated_at) VALUES(?1,?2,?3,?4)",
                                  -1, &stmt, NULL) != SQLITE_OK)
                continue;
            bind_text(stmt, 1, habit->id);
            sqlite3_bind_int(stmt, 2, habit->days[d].day_index);
            sqlite3_bind_int(stmt, 3, habit->days[d].completed ? 1 : 0);
            sqlite3_bind_int64(stmt, 4, now_seconds());
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            stmt = NULL;
        }
    }
    exec_sql("COMMIT");
}

int
inbe_storage_export_zip(const char *path)
{
    mz_zip_archive archive;
    FILE *fp;
    char *buf;
    long size;
    char metadata[512];

    if(path == NULL || path[0] == '\0' || g_storage.db == NULL)
        return 0;
    sqlite3_exec(g_storage.db, "PRAGMA wal_checkpoint(FULL)", NULL, NULL, NULL);
    fp = fopen(g_storage.db_path, "rb");
    if(fp == NULL)
        return 0;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if(size <= 0) {
        fclose(fp);
        return 0;
    }
    buf = malloc((size_t)size);
    if(buf == NULL) {
        fclose(fp);
        return 0;
    }
    if(fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_writer_init_file(&archive, path, 0)) {
        free(buf);
        return 0;
    }
    snprintf(metadata, sizeof(metadata),
             "{\n\"format\":\"inbe-data-sqlite\",\n\"format_version\":1,\n\"app_version\":\"%s\",\n\"user_id\":\"%s\",\n\"session_count\":%d,\n\"habit_count\":%d\n}\n",
             INBE_VERSION_STRING, g_storage.user_id, inbe_storage_session_count(), inbe_storage_habits_empty() ? 0 : 1);
    mz_zip_writer_add_mem(&archive, "inbe-data/metadata.json", metadata, strlen(metadata), MZ_NO_COMPRESSION);
    mz_zip_writer_add_mem(&archive, "inbe-data/inbe.db", buf, (size_t)size, MZ_BEST_COMPRESSION);
    free(buf);
    mz_zip_writer_finalize_archive(&archive);
    mz_zip_writer_end(&archive);
    return 1;
}

static int
import_legacy_session_content(const char *zip_path, mz_zip_archive *archive,
                              int year, int month, int day, const char *filename)
{
    char *content;
    size_t size = 0;
    char *text;
    int rounds[MaxRounds];
    int count = 0;
    char *line;
    long long started_at;

    content = mz_zip_reader_extract_file_to_heap(archive, zip_path, &size, 0);
    if(content == NULL || size == 0)
        return 0;
    text = malloc(size + 1);
    if(text == NULL) {
        free(content);
        return 0;
    }
    memcpy(text, content, size);
    text[size] = '\0';
    free(content);
    line = strtok(text, "\n");
    while(line != NULL && count < MaxRounds) {
        int seconds = atoi(line);
        if(seconds > 0 && seconds <= 999)
            rounds[count++] = seconds;
        line = strtok(NULL, "\n");
    }
    started_at = legacy_started_at(year, month, day, filename);
    if(count > 0 && started_at > 0)
        insert_session_at(started_at, year * 10000 + month * 100 + day, rounds, count, "legacy-import", NULL, 0);
    free(text);
    return count > 0;
}

static int
import_legacy_zip(mz_zip_archive *archive)
{
    int imported = 0;
    int total = (int)mz_zip_reader_get_num_files(archive);
    for(int i = 0; i < total; i++) {
        mz_zip_archive_file_stat st;
        int y, m, d;
        char filename[INBE_STORAGE_PATH_SIZE];
        const char *p;
        if(!mz_zip_reader_file_stat(archive, i, &st) || st.m_is_directory)
            continue;
        if(strncmp(st.m_filename, "lotus-data/sessions/", 20) != 0)
            continue;
        p = st.m_filename + 20;
        if(sscanf(p, "%4d/%2d/%2d/%511s", &y, &m, &d, filename) != 4)
            continue;
        if(import_legacy_session_content(st.m_filename, archive, y, m, d, filename))
            imported++;
    }
    return imported > 0;
}

static int
import_sqlite_db_file(const char *db_path)
{
    sqlite3 *src = NULL;
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt *hstmt = NULL;
    int ok = 0;

    if(db_path == NULL || db_path[0] == '\0')
        return 0;
    if(sqlite3_open(db_path, &src) != SQLITE_OK)
        goto done;
    if(sqlite3_prepare_v2(src,
                          "SELECT id,started_at,local_date,source FROM sessions WHERE deleted_at=0",
                          -1, &stmt, NULL) != SQLITE_OK)
        goto done;

    while(sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_stmt *rstmt = NULL;
        int rounds[MaxRounds];
        int count = 0;
        const char *sid = (const char *)sqlite3_column_text(stmt, 0);
        long long started_at = sqlite3_column_int64(stmt, 1);
        int local_date = sqlite3_column_int(stmt, 2);

        if(sqlite3_prepare_v2(src,
                              "SELECT seconds FROM session_rounds WHERE session_id=?1 ORDER BY round_index",
                              -1, &rstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(rstmt, 1, sid != NULL ? sid : "", -1, SQLITE_TRANSIENT);
            while(count < MaxRounds && sqlite3_step(rstmt) == SQLITE_ROW)
                rounds[count++] = sqlite3_column_int(rstmt, 0);
        }
        sqlite3_finalize(rstmt);
        if(count > 0) {
            insert_session_at(started_at, local_date, rounds, count, "sqlite-import", NULL, 0);
            ok = 1;
        }
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if(sqlite3_prepare_v2(src,
                          "SELECT id,name,color_r,color_g,color_b,sync_mode,sync_topic,sync_activity,sort_order "
                          "FROM habits WHERE deleted_at=0 ORDER BY sort_order,id",
                          -1, &stmt, NULL) == SQLITE_OK) {
        exec_sql("BEGIN IMMEDIATE");
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            const char *habit_id = (const char *)sqlite3_column_text(stmt, 0);

            if(habit_id == NULL || habit_id[0] == '\0')
                continue;
            if(sqlite3_prepare_v2(g_storage.db,
                                  "INSERT OR REPLACE INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_topic,sync_activity,sort_order,deleted_at) "
                                  "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,0)",
                                  -1, &hstmt, NULL) != SQLITE_OK)
                continue;
            bind_text(hstmt, 1, habit_id);
            bind_text(hstmt, 2, g_storage.user_id);
            bind_text(hstmt, 3, (const char *)sqlite3_column_text(stmt, 1));
            sqlite3_bind_int(hstmt, 4, sqlite3_column_int(stmt, 2));
            sqlite3_bind_int(hstmt, 5, sqlite3_column_int(stmt, 3));
            sqlite3_bind_int(hstmt, 6, sqlite3_column_int(stmt, 4));
            sqlite3_bind_int(hstmt, 7, sqlite3_column_int(stmt, 5));
            sqlite3_bind_int(hstmt, 8, sqlite3_column_int(stmt, 6));
            sqlite3_bind_int(hstmt, 9, sqlite3_column_int(stmt, 7));
            sqlite3_bind_int(hstmt, 10, sqlite3_column_int(stmt, 8));
            if(sqlite3_step(hstmt) == SQLITE_DONE)
                ok = 1;
            sqlite3_finalize(hstmt);
            hstmt = NULL;

            if(sqlite3_prepare_v2(src,
                                  "SELECT local_date,completed FROM habit_days WHERE habit_id=?1",
                                  -1, &hstmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(hstmt, 1, habit_id, -1, SQLITE_TRANSIENT);
                while(sqlite3_step(hstmt) == SQLITE_ROW) {
                    sqlite3_stmt *day_stmt = NULL;
                    if(sqlite3_prepare_v2(g_storage.db,
                                          "INSERT OR REPLACE INTO habit_days(habit_id,local_date,completed,updated_at) "
                                          "VALUES(?1,?2,?3,?4)",
                                          -1, &day_stmt, NULL) != SQLITE_OK)
                        continue;
                    bind_text(day_stmt, 1, habit_id);
                    sqlite3_bind_int(day_stmt, 2, sqlite3_column_int(hstmt, 0));
                    sqlite3_bind_int(day_stmt, 3, sqlite3_column_int(hstmt, 1) != 0);
                    sqlite3_bind_int64(day_stmt, 4, now_seconds());
                    sqlite3_step(day_stmt);
                    sqlite3_finalize(day_stmt);
                }
            }
            sqlite3_finalize(hstmt);
            hstmt = NULL;
        }
        exec_sql("COMMIT");
    }

done:
    sqlite3_finalize(stmt);
    sqlite3_finalize(hstmt);
    if(src != NULL)
        sqlite3_close(src);
    return ok;
}

int
inbe_storage_import_zip(const char *path)
{
    mz_zip_archive archive;
    int ok = 0;

    if(path == NULL || path[0] == '\0' || !path_exists(path))
        return 0;
    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_reader_init_file(&archive, path, 0))
        return import_sqlite_db_file(path);
    if(mz_zip_reader_locate_file(&archive, "inbe-data/inbe.db", NULL, 0) >= 0) {
        char *db_bytes;
        size_t db_size = 0;
        char temp_path[INBE_STORAGE_PATH_SIZE];
        FILE *fp;
        db_bytes = mz_zip_reader_extract_file_to_heap(&archive, "inbe-data/inbe.db", &db_size, 0);
        snprintf(temp_path, sizeof(temp_path), "%s/import-inbe.db", g_storage.root);
        fp = fopen(temp_path, "wb");
        if(db_bytes != NULL && fp != NULL && fwrite(db_bytes, 1, db_size, fp) == db_size) {
            fclose(fp);
            fp = NULL;
            ok = import_sqlite_db_file(temp_path);
            remove(temp_path);
        }
        if(fp != NULL)
            fclose(fp);
        free(db_bytes);
    } else {
        ok = import_legacy_zip(&archive);
    }
    mz_zip_reader_end(&archive);
    return ok;
}

int
inbe_storage_init(const char *root)
{
    if(root == NULL || root[0] == '\0')
        return 0;
    snprintf(g_storage.root, sizeof(g_storage.root), "%s", root);
    ensure_dir_local(g_storage.root);
    snprintf(g_storage.db_path, sizeof(g_storage.db_path), "%s/inbe.db", g_storage.root);
    if(sqlite3_open(g_storage.db_path, &g_storage.db) != SQLITE_OK) {
        TraceLog(LOG_ERROR, "STORAGE: failed to open %s", g_storage.db_path);
        return 0;
    }
    if(!schema_create() || !load_or_create_user())
        return 0;
    migrate_legacy_files();
    return 1;
}

void
inbe_storage_close(void)
{
    if(g_storage.db != NULL) {
        sqlite3_close(g_storage.db);
        g_storage.db = NULL;
    }
}

const char *
inbe_storage_db_path(void)
{
    return g_storage.db_path;
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
