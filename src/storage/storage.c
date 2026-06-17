#include "storage.h"

#include "screens/habits_screen.h"
#include "breath_engine.h"
#include "miniz.h"
#include "version.h"

#include "raylib.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

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

static void
storage_schedule_persist(void)
{
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        if(typeof FS === 'undefined' || typeof FS.syncfs !== 'function')
            return;
        Module.__inbeStorageSyncPending = true;
        if(Module.__inbeStorageSyncTimer)
            clearTimeout(Module.__inbeStorageSyncTimer);
        Module.__inbeStorageSyncTimer = setTimeout(function() {
            Module.__inbeStorageSyncTimer = 0;
            Module.__inbeStorageSyncing = true;
            try {
                FS.syncfs(false, function(err) {
                    Module.__inbeStorageSyncing = false;
                    Module.__inbeStorageSyncPending = false;
                    if(err)
                        console.error("IDBFS save failed:", err);
                });
            } catch(e) {
                Module.__inbeStorageSyncing = false;
                Module.__inbeStorageSyncPending = false;
                console.error("IDBFS sync error:", e);
            }
        }, 120);
    });
#endif
}

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

static int
table_has_column(const char *table, const char *column)
{
    sqlite3_stmt *stmt = NULL;
    char sql[128];
    int found = 0;

    if(table == NULL || column == NULL || g_storage.db == NULL)
        return 0;
    snprintf(sql, sizeof(sql), "PRAGMA table_info(%s)", table);
    if(sqlite3_prepare_v2(g_storage.db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        if(name != NULL && strcmp(name, column) == 0) {
            found = 1;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

static int
migrate_schema(void)
{
    if(!table_has_column("habits", "sync_topic"))
        return 1;

    return exec_sql(
        "BEGIN IMMEDIATE;"
        "ALTER TABLE habits RENAME TO habits_with_sync_topic;"
        "CREATE TABLE habits("
        " id TEXT PRIMARY KEY,"
        " user_id TEXT NOT NULL,"
        " name TEXT NOT NULL,"
        " color_r INTEGER NOT NULL,"
        " color_g INTEGER NOT NULL,"
        " color_b INTEGER NOT NULL,"
        " sync_mode INTEGER NOT NULL,"
        " sync_activity INTEGER NOT NULL,"
        " sort_order INTEGER NOT NULL,"
        " deleted_at INTEGER NOT NULL DEFAULT 0"
        ");"
        "INSERT INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,sort_order,deleted_at)"
        " SELECT id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,sort_order,deleted_at"
        " FROM habits_with_sync_topic;"
        "DROP TABLE habits_with_sync_topic;"
        "COMMIT;");
}

static int
source_table_has_column(sqlite3 *db, const char *table, const char *column)
{
    sqlite3_stmt *stmt = NULL;
    char sql[128];
    int found = 0;

    if(db == NULL || table == NULL || column == NULL)
        return 0;
    snprintf(sql, sizeof(sql), "PRAGMA table_info(%s)", table);
    if(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        if(name != NULL && strcmp(name, column) == 0) {
            found = 1;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
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
    storage_schedule_persist();
}

void
inbe_storage_settings_begin_write(void)
{
    if(g_storage.db == NULL)
        return;
    exec_sql("BEGIN IMMEDIATE");
}

void
inbe_storage_settings_end_write(void)
{
    if(g_storage.db == NULL)
        return;
    exec_sql("COMMIT");
    storage_schedule_persist();
}

void
inbe_storage_set_setting_int(const char *key, int value)
{
    char text[32];
    snprintf(text, sizeof(text), "%d", value);
    inbe_storage_set_setting_text(key, text);
}

static int
insert_session_at_ex(long long started_at, int local_date, const int *round_times,
                     int round_count, int topic, int activity, const char *source,
                     char *out_id, size_t out_id_size)
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
                          "INSERT OR IGNORE INTO sessions(id,user_id,started_at,local_date,topic,activity,source,imported_at,rounds_hash) "
                          "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9)",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, id);
    bind_text(stmt, 2, g_storage.user_id);
    sqlite3_bind_int64(stmt, 3, started_at);
    sqlite3_bind_int(stmt, 4, local_date);
    sqlite3_bind_int(stmt, 5, topic);
    sqlite3_bind_int(stmt, 6, activity);
    bind_text(stmt, 7, source != NULL ? source : "app");
    sqlite3_bind_int64(stmt, 8, now_seconds());
    sqlite3_bind_int64(stmt, 9, (sqlite3_int64)rhash);
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
    storage_schedule_persist();
    return 1;
}

int
inbe_storage_save_session_for_activity(const int *round_times, int round_count,
                                       int topic, int activity,
                                       char *out_id, size_t out_id_size)
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
    return insert_session_at_ex((long long)now, local_date, saved, saved_count,
                                topic, activity, "app", out_id, out_id_size);
}

int
inbe_storage_save_session(const int *round_times, int round_count, char *out_id, size_t out_id_size)
{
    return inbe_storage_save_session_for_activity(round_times, round_count, 0, 0,
                                                  out_id, out_id_size);
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
    storage_schedule_persist();
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
    storage_schedule_persist();
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
    storage_schedule_persist();
    return 1;
}

void
inbe_storage_list_session_records(InbeStorageSessionRecordCallback callback, void *user)
{
    sqlite3_stmt *stmt = NULL;
    if(callback == NULL || g_storage.db == NULL)
        return;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id,started_at,local_date,topic,activity FROM sessions WHERE deleted_at=0 ORDER BY started_at DESC LIMIT 48",
                          -1, &stmt, NULL) != SQLITE_OK)
        return;
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        char dbid[INBE_STORAGE_ID_SIZE + 4];
        int rounds[MaxRounds];
        int count = 0;
        const char *id = (const char *)sqlite3_column_text(stmt, 0);
        long long started_at = sqlite3_column_int64(stmt, 1);
        int local_date = sqlite3_column_int(stmt, 2);
        int topic = sqlite3_column_int(stmt, 3);
        int activity = sqlite3_column_int(stmt, 4);
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
            callback(dbid, y, m, d, hh, mm, ss, topic, activity, rounds, count, user);
    }
    sqlite3_finalize(stmt);
}

int
inbe_storage_has_any(void)
{
    sqlite3_stmt *stmt = NULL;
    int count = inbe_storage_session_count();

    if(count > 0)
        return 1;
    if(g_storage.db == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COUNT(*) FROM habit_days", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    if(sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count > 0;
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
    if(!inbe_storage_has_any())
        return 0;
    if(g_storage.db_path[0] != '\0' && stat(g_storage.db_path, &st) == 0)
        return (long long)st.st_size;
    return 0;
}

long long
inbe_storage_delete_all_sessions(void)
{
    sqlite3_stmt *stmt = NULL;
    int count = inbe_storage_session_count();
    int habit_day_count = 0;

    if(g_storage.db == NULL)
        return 0;

    if(sqlite3_prepare_v2(g_storage.db, "SELECT COUNT(*) FROM habit_days", -1, &stmt, NULL) == SQLITE_OK) {
        if(sqlite3_step(stmt) == SQLITE_ROW)
            habit_day_count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if(count <= 0 && habit_day_count <= 0)
        return 0;

    if(!exec_sql("BEGIN IMMEDIATE"))
        return 0;
    if(!exec_sql("DELETE FROM session_rounds")) {
        exec_sql("ROLLBACK");
        return 0;
    }
    if(!exec_sql("DELETE FROM sessions")) {
        exec_sql("ROLLBACK");
        return 0;
    }
    if(!exec_sql("DELETE FROM habit_days")) {
        exec_sql("ROLLBACK");
        return 0;
    }
    if(!exec_sql("COMMIT")) {
        exec_sql("ROLLBACK");
        return 0;
    }
    exec_sql("VACUUM");
    storage_schedule_persist();
    return count + habit_day_count;
}

int
inbe_storage_habits_empty(void)
{
    return inbe_storage_habit_count() == 0;
}

int
inbe_storage_habit_count(void)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;
    if(g_storage.db == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COUNT(*) FROM habits WHERE deleted_at=0", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    if(sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
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
                          "SELECT id,name,color_r,color_g,color_b,sync_mode,sync_activity "
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
        habit->sync_activity = sqlite3_column_int(stmt, 6);
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
                              "INSERT INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,sort_order,deleted_at) "
                              "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,0)",
                              -1, &stmt, NULL) != SQLITE_OK)
            continue;
        bind_text(stmt, 1, habit->id);
        bind_text(stmt, 2, g_storage.user_id);
        bind_text(stmt, 3, habit->name);
        sqlite3_bind_int(stmt, 4, habit->color.r);
        sqlite3_bind_int(stmt, 5, habit->color.g);
        sqlite3_bind_int(stmt, 6, habit->color.b);
        sqlite3_bind_int(stmt, 7, habit->sync_mode);
        sqlite3_bind_int(stmt, 8, habit->sync_activity);
        sqlite3_bind_int(stmt, 9, i);
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
    storage_schedule_persist();
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
             INBE_VERSION_STRING, g_storage.user_id, inbe_storage_session_count(), inbe_storage_habit_count());
    mz_zip_writer_add_mem(&archive, "inbe-data/metadata.json", metadata, strlen(metadata), MZ_NO_COMPRESSION);
    mz_zip_writer_add_mem(&archive, "inbe-data/inbe.db", buf, (size_t)size, MZ_BEST_COMPRESSION);
    free(buf);
    mz_zip_writer_finalize_archive(&archive);
    mz_zip_writer_end(&archive);
    return 1;
}

static Color
tickmate_color_from_int(int value, int index)
{
    static const Color fallback[] = {
        {99, 196, 165, 255},
        {94, 166, 232, 255},
        {210, 180, 72, 255},
        {224, 124, 104, 255},
        {180, 132, 220, 255},
        {216, 116, 164, 255}
    };
    Color color = fallback[index % (int)(sizeof(fallback) / sizeof(fallback[0]))];

    if(value != 0) {
        color.r = (unsigned char)((value >> 16) & 0xff);
        color.g = (unsigned char)((value >> 8) & 0xff);
        color.b = (unsigned char)(value & 0xff);
        color.a = 255;
        if(color.r == 0 && color.g == 0 && color.b == 0)
            color = fallback[index % (int)(sizeof(fallback) / sizeof(fallback[0]))];
    }
    return color;
}

static int
parse_legacy_rounds(const char *text, size_t size, int *rounds, int max_rounds)
{
    int count = 0;
    size_t pos = 0;

    if(text == NULL || rounds == NULL || max_rounds <= 0)
        return 0;
    while(pos < size && count < max_rounds) {
        int value = 0;
        int seen = 0;
        while(pos < size && (text[pos] == ' ' || text[pos] == '\t' ||
                             text[pos] == '\r' || text[pos] == '\n'))
            pos++;
        while(pos < size && text[pos] >= '0' && text[pos] <= '9') {
            seen = 1;
            value = value * 10 + (text[pos] - '0');
            pos++;
        }
        if(seen && value > 0)
            rounds[count++] = value;
        while(pos < size && text[pos] != '\n')
            pos++;
    }
    return count;
}

static long long
legacy_session_started_at(int year, int month, int day, int hour, int minute, int second)
{
    struct tm tm_value;

    memset(&tm_value, 0, sizeof(tm_value));
    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = month - 1;
    tm_value.tm_mday = day;
    tm_value.tm_hour = hour;
    tm_value.tm_min = minute;
    tm_value.tm_sec = second;
    tm_value.tm_isdst = -1;
    return (long long)mktime(&tm_value);
}

static int
parse_legacy_session_filename(const char *filename, int *year, int *month, int *day,
                              int *hour, int *minute, int *second)
{
    const char *p;

    if(filename == NULL)
        return 0;
    for(p = filename; *p != '\0'; p++) {
        if(sscanf(p, "sessions/%4d/%2d/%2d/inbe-%2d%2d%2d",
                  year, month, day, hour, minute, second) == 6)
            return 1;
        if(sscanf(p, "%4d/%2d/%2d/inbe-%2d%2d%2d",
                  year, month, day, hour, minute, second) == 6)
            return 1;
    }
    return 0;
}

static int
import_legacy_session_bytes(const char *name, const char *bytes, size_t size)
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int rounds[MaxRounds];
    int round_count;
    int local_date;
    long long started_at;

    if(!parse_legacy_session_filename(name, &year, &month, &day, &hour, &minute, &second))
        return 0;
    round_count = parse_legacy_rounds(bytes, size, rounds, MaxRounds);
    if(round_count <= 0) {
        TraceLog(LOG_WARNING, "DATA: legacy import ignored empty session %s", name);
        return 0;
    }
    local_date = year * 10000 + month * 100 + day;
    started_at = legacy_session_started_at(year, month, day, hour, minute, second);
    if(started_at <= 0) {
        TraceLog(LOG_WARNING, "DATA: legacy import invalid date in %s", name);
        return 0;
    }
    return insert_session_at_ex(started_at, local_date, rounds, round_count,
                                0, 0, "legacy-file-import", NULL, 0);
}

static int
import_legacy_session_zip(mz_zip_archive *archive)
{
    mz_uint file_count;
    int imported = 0;

    if(archive == NULL)
        return 0;
    file_count = mz_zip_reader_get_num_files(archive);
    TraceLog(LOG_INFO, "DATA: checking legacy session archive with %u files", file_count);
    for(mz_uint i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat stat;
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        size_t text_size = 0;
        char *text;

        if(!mz_zip_reader_file_stat(archive, i, &stat))
            continue;
        if(stat.m_is_directory)
            continue;
        if(!parse_legacy_session_filename(stat.m_filename,
                                          &year, &month, &day,
                                          &hour, &minute, &second))
            continue;

        text = mz_zip_reader_extract_to_heap(archive, i, &text_size, 0);
        if(text == NULL) {
            TraceLog(LOG_WARNING, "DATA: legacy import failed to extract %s", stat.m_filename);
            continue;
        }
        if(import_legacy_session_bytes(stat.m_filename, text, text_size))
            imported++;
        free(text);
    }
    if(imported > 0)
        TraceLog(LOG_INFO, "DATA: imported %d legacy sessions", imported);
    else
        TraceLog(LOG_WARNING, "DATA: no legacy sessions found in archive");
    return imported > 0;
}

static char *
read_file_heap(const char *path, size_t *out_size)
{
    FILE *fp;
    long size;
    char *buf;

    if(out_size != NULL)
        *out_size = 0;
    if(path == NULL || path[0] == '\0')
        return NULL;
    fp = fopen(path, "rb");
    if(fp == NULL)
        return NULL;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if(size <= 0) {
        fclose(fp);
        return NULL;
    }
    buf = malloc((size_t)size);
    if(buf == NULL) {
        fclose(fp);
        return NULL;
    }
    if(fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    if(out_size != NULL)
        *out_size = (size_t)size;
    return buf;
}

static int
migrate_legacy_file_sessions_in_dir(const char *dir_path)
{
    DIR *dir;
    struct dirent *entry;
    int imported = 0;

    if(dir_path == NULL || dir_path[0] == '\0')
        return 0;
    dir = opendir(dir_path);
    if(dir == NULL)
        return 0;
    while((entry = readdir(dir)) != NULL) {
        char child[INBE_STORAGE_PATH_SIZE];
        struct stat st;

        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(child, sizeof(child), "%s/%s", dir_path, entry->d_name);
        if(stat(child, &st) != 0)
            continue;
        if(S_ISDIR(st.st_mode)) {
            imported += migrate_legacy_file_sessions_in_dir(child);
        } else if(S_ISREG(st.st_mode)) {
            int year;
            int month;
            int day;
            int hour;
            int minute;
            int second;
            if(parse_legacy_session_filename(child, &year, &month, &day,
                                             &hour, &minute, &second)) {
                size_t size = 0;
                char *bytes = read_file_heap(child, &size);
                if(bytes != NULL) {
                    if(import_legacy_session_bytes(child, bytes, size))
                        imported++;
                    free(bytes);
                } else {
                    TraceLog(LOG_WARNING, "DATA: legacy file migration could not read %s", child);
                }
            }
        }
    }
    closedir(dir);
    return imported;
}

static void
migrate_legacy_file_sessions_once(void)
{
    int imported;

    if(g_storage.db == NULL || g_storage.root[0] == '\0')
        return;
    if(meta_equals("legacy_file_sessions_migrated", "1"))
        return;
    TraceLog(LOG_INFO, "DATA: checking for legacy session files in %s", g_storage.root);
    imported = migrate_legacy_file_sessions_in_dir(g_storage.root);
    if(imported > 0)
        TraceLog(LOG_INFO, "DATA: migrated %d legacy file sessions", imported);
    else
        TraceLog(LOG_INFO, "DATA: no legacy file sessions found");
    set_meta("legacy_file_sessions_migrated", "1");
    storage_schedule_persist();
}

static int
import_tickmate_db(sqlite3 *src)
{
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt *write_stmt = NULL;
    int ok = 0;
    long long imported_at = now_seconds();

    if(src == NULL || g_storage.db == NULL)
        return 0;
    if(!source_table_has_column(src, "tracks", "name") ||
       !source_table_has_column(src, "ticks", "_track_id"))
        return 0;

    if(sqlite3_prepare_v2(src,
                          "SELECT _id,name,color,\"order\" FROM tracks WHERE enabled!=0 ORDER BY \"order\",_id",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    exec_sql("BEGIN IMMEDIATE");
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        char habit_id[64];
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        int track_id = sqlite3_column_int(stmt, 0);
        int sort_order = sqlite3_column_int(stmt, 3);
        Color color = tickmate_color_from_int(sqlite3_column_int(stmt, 2), track_id);

        if(name == NULL || name[0] == '\0')
            continue;
        snprintf(habit_id, sizeof(habit_id), "tickmate-%d", track_id);
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT OR REPLACE INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,sort_order,deleted_at) "
                              "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,0)",
                              -1, &write_stmt, NULL) != SQLITE_OK)
            continue;
        bind_text(write_stmt, 1, habit_id);
        bind_text(write_stmt, 2, g_storage.user_id);
        bind_text(write_stmt, 3, name);
        sqlite3_bind_int(write_stmt, 4, color.r);
        sqlite3_bind_int(write_stmt, 5, color.g);
        sqlite3_bind_int(write_stmt, 6, color.b);
        sqlite3_bind_int(write_stmt, 7, INBE_HABIT_SYNC_NONE);
        sqlite3_bind_int(write_stmt, 8, 0);
        sqlite3_bind_int(write_stmt, 9, sort_order);
        if(sqlite3_step(write_stmt) == SQLITE_DONE)
            ok = 1;
        sqlite3_finalize(write_stmt);
        write_stmt = NULL;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if(sqlite3_prepare_v2(src,
                          "SELECT _track_id,year,month,day FROM ticks",
                          -1, &stmt, NULL) == SQLITE_OK) {
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            char habit_id[64];
            int track_id = sqlite3_column_int(stmt, 0);
            int year = sqlite3_column_int(stmt, 1);
            int month = sqlite3_column_int(stmt, 2);
            int day = sqlite3_column_int(stmt, 3);
            int local_date;

            if(year <= 0 || month <= 0 || month > 12 || day <= 0 || day > 31)
                continue;
            local_date = year * 10000 + month * 100 + day;
            snprintf(habit_id, sizeof(habit_id), "tickmate-%d", track_id);
            if(sqlite3_prepare_v2(g_storage.db,
                                  "INSERT OR REPLACE INTO habit_days(habit_id,local_date,completed,updated_at) "
                                  "VALUES(?1,?2,1,?3)",
                                  -1, &write_stmt, NULL) != SQLITE_OK)
                continue;
            bind_text(write_stmt, 1, habit_id);
            sqlite3_bind_int(write_stmt, 2, local_date);
            sqlite3_bind_int64(write_stmt, 3, imported_at);
            if(sqlite3_step(write_stmt) == SQLITE_DONE)
                ok = 1;
            sqlite3_finalize(write_stmt);
            write_stmt = NULL;
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_finalize(write_stmt);
    exec_sql("COMMIT");
    return ok;
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
    if(sqlite3_open(db_path, &src) != SQLITE_OK) {
        TraceLog(LOG_WARNING, "DATA: sqlite import could not open %s", db_path);
        goto done;
    }
    if(sqlite3_prepare_v2(src,
                          "SELECT id,started_at,local_date,topic,activity,source FROM sessions WHERE deleted_at=0",
                          -1, &stmt, NULL) != SQLITE_OK) {
        TraceLog(LOG_INFO, "DATA: sqlite import is not new Inbe schema, trying Tickmate");
        goto try_tickmate;
    }

    while(sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_stmt *rstmt = NULL;
        int rounds[MaxRounds];
        int count = 0;
        const char *sid = (const char *)sqlite3_column_text(stmt, 0);
        long long started_at = sqlite3_column_int64(stmt, 1);
        int local_date = sqlite3_column_int(stmt, 2);
        int topic = sqlite3_column_int(stmt, 3);
        int activity = sqlite3_column_int(stmt, 4);

        if(sqlite3_prepare_v2(src,
                              "SELECT seconds FROM session_rounds WHERE session_id=?1 ORDER BY round_index",
                              -1, &rstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(rstmt, 1, sid != NULL ? sid : "", -1, SQLITE_TRANSIENT);
            while(count < MaxRounds && sqlite3_step(rstmt) == SQLITE_ROW)
                rounds[count++] = sqlite3_column_int(rstmt, 0);
        }
        sqlite3_finalize(rstmt);
        if(count > 0) {
            insert_session_at_ex(started_at, local_date, rounds, count,
                                 topic, activity, "sqlite-import", NULL, 0);
            ok = 1;
        }
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if(sqlite3_prepare_v2(src,
                          "SELECT id,name,color_r,color_g,color_b,sync_mode,sync_activity,sort_order "
                          "FROM habits WHERE deleted_at=0 ORDER BY sort_order,id",
                          -1, &stmt, NULL) == SQLITE_OK) {
        exec_sql("BEGIN IMMEDIATE");
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            const char *habit_id = (const char *)sqlite3_column_text(stmt, 0);

            if(habit_id == NULL || habit_id[0] == '\0')
                continue;
            if(sqlite3_prepare_v2(g_storage.db,
                                  "INSERT OR REPLACE INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,sort_order,deleted_at) "
                                  "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,0)",
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

    goto done;

try_tickmate:
    sqlite3_finalize(stmt);
    stmt = NULL;
    ok = import_tickmate_db(src);
    if(!ok)
        TraceLog(LOG_WARNING, "DATA: sqlite import was neither Inbe nor supported Tickmate schema");

done:
    sqlite3_finalize(stmt);
    sqlite3_finalize(hstmt);
    if(src != NULL)
        sqlite3_close(src);
    if(ok)
        storage_schedule_persist();
    return ok;
}

int
inbe_storage_import_zip(const char *path)
{
    mz_zip_archive archive;
    int ok = 0;

    if(path == NULL || path[0] == '\0') {
        TraceLog(LOG_ERROR, "DATA: import path is empty");
        return 0;
    }
    if(!path_exists(path)) {
        TraceLog(LOG_ERROR, "DATA: import path does not exist: %s", path);
        return 0;
    }
    TraceLog(LOG_INFO, "DATA: importing %s", path);
    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_reader_init_file(&archive, path, 0)) {
        TraceLog(LOG_INFO, "DATA: import is not a zip archive, trying sqlite db");
        return import_sqlite_db_file(path);
    }
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
            if(ok)
                TraceLog(LOG_INFO, "DATA: imported sqlite archive");
            else
                TraceLog(LOG_ERROR, "DATA: archive contained inbe-data/inbe.db but sqlite import failed");
        } else {
            TraceLog(LOG_ERROR, "DATA: failed to extract inbe-data/inbe.db from archive");
        }
        if(fp != NULL)
            fclose(fp);
        free(db_bytes);
    } else {
        TraceLog(LOG_INFO, "DATA: archive has no inbe-data/inbe.db, trying legacy sessions");
        ok = import_legacy_session_zip(&archive);
    }
    mz_zip_reader_end(&archive);
    if(!ok)
        TraceLog(LOG_ERROR, "DATA: import failed for %s", path);
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
    if(!schema_create() || !migrate_schema() || !load_or_create_user())
        return 0;
    migrate_legacy_file_sessions_once();
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
