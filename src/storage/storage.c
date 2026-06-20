#include "storage.h"

#include "screens/habits_screen.h"
#include "breath_engine.h"
#include "miniz.h"
#include "version.h"

#include "raylib.h"
#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>
#include <unistd.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#if defined(_WIN32)
#define INBE_MKDIR(path) mkdir(path)
#else
#define INBE_MKDIR(path) mkdir(path, 0700)
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
    char text_value[8192];
    int last_sync_changed;
    int materialize_defer;
    int materialize_needed;
    long long pending_sync_outbox_seq;
} StorageState;

static StorageState g_storage;

static long long now_seconds(void);
static int bind_text(sqlite3_stmt *stmt, int index, const char *text);
static long long storage_max_sync_outbox_seq(void);

static int
fill_random_bytes(unsigned char *data, size_t len)
{
    FILE *file;

    if(data == NULL || len == 0)
        return 0;
    file = fopen("/dev/urandom", "rb");
    if(file != NULL) {
        size_t n = fread(data, 1, len, file);
        fclose(file);
        if(n == len)
            return 1;
    }
    for(size_t i = 0; i < len; i++)
        data[i] = (unsigned char)((rand() >> ((i % sizeof(int)) * 8)) & 0xff);
    return 1;
}

static void
make_client_uuid(char out[37])
{
    unsigned char bytes[16];

    fill_random_bytes(bytes, sizeof(bytes));
    bytes[6] = (unsigned char)((bytes[6] & 0x0f) | 0x40);
    bytes[8] = (unsigned char)((bytes[8] & 0x3f) | 0x80);
    snprintf(out,
             37,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0],
             bytes[1],
             bytes[2],
             bytes[3],
             bytes[4],
             bytes[5],
             bytes[6],
             bytes[7],
             bytes[8],
             bytes[9],
             bytes[10],
             bytes[11],
             bytes[12],
             bytes[13],
             bytes[14],
             bytes[15]);
}

typedef struct JsonBuilder {
    char *data;
    size_t len;
    size_t cap;
    int ok;
} JsonBuilder;

static int
json_reserve(JsonBuilder *json, size_t extra)
{
    char *next;
    size_t next_cap;

    if(json == NULL || !json->ok)
        return 0;
    if(extra <= json->cap - json->len)
        return 1;
    next_cap = json->cap > 0 ? json->cap : 1024;
    while(extra > next_cap - json->len)
        next_cap *= 2;
    next = (char *)realloc(json->data, next_cap);
    if(next == NULL) {
        json->ok = 0;
        return 0;
    }
    json->data = next;
    json->cap = next_cap;
    return 1;
}

static void
json_append_len(JsonBuilder *json, const char *text, size_t len)
{
    if(text == NULL || len == 0)
        return;
    if(!json_reserve(json, len + 1))
        return;
    memcpy(json->data + json->len, text, len);
    json->len += len;
    json->data[json->len] = '\0';
}

static void
json_append(JsonBuilder *json, const char *text)
{
    if(text != NULL)
        json_append_len(json, text, strlen(text));
}

static void
json_appendf(JsonBuilder *json, const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int needed;

    if(json == NULL || fmt == NULL || !json->ok)
        return;
    va_start(args, fmt);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if(needed < 0) {
        json->ok = 0;
        va_end(args);
        return;
    }
    if(json_reserve(json, (size_t)needed + 1)) {
        vsnprintf(json->data + json->len, json->cap - json->len, fmt, args);
        json->len += (size_t)needed;
    }
    va_end(args);
}

static void
json_append_string(JsonBuilder *json, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text != NULL ? text : "");

    json_append(json, "\"");
    while(*p != '\0') {
        char escaped[8];
        if(*p == '"' || *p == '\\') {
            escaped[0] = '\\';
            escaped[1] = (char)*p;
            json_append_len(json, escaped, 2);
        } else if(*p == '\b') {
            json_append(json, "\\b");
        } else if(*p == '\f') {
            json_append(json, "\\f");
        } else if(*p == '\n') {
            json_append(json, "\\n");
        } else if(*p == '\r') {
            json_append(json, "\\r");
        } else if(*p == '\t') {
            json_append(json, "\\t");
        } else if(*p < 0x20) {
            snprintf(escaped, sizeof(escaped), "\\u%04x", *p);
            json_append(json, escaped);
        } else {
            json_append_len(json, (const char *)p, 1);
        }
        p++;
    }
    json_append(json, "\"");
}

static void
json_append_key_string(JsonBuilder *json, const char *key, const char *value)
{
    json_append_string(json, key);
    json_append(json, ":");
    json_append_string(json, value);
}

static void
json_append_epoch(JsonBuilder *json, long long seconds)
{
    char formatted[32];
    struct tm tm_value;
    time_t value = (time_t)seconds;

    if(seconds <= 0)
        value = time(NULL);
#if defined(_WIN32)
    gmtime_s(&tm_value, &value);
#else
    gmtime_r(&value, &tm_value);
#endif
    strftime(formatted, sizeof(formatted), "%Y-%m-%dT%H:%M:%SZ", &tm_value);
    json_append_string(json, formatted);
}

static void
storage_schedule_persist(void)
{
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        if(typeof Module.__inbeScheduleStorageSync === 'function')
            Module.__inbeScheduleStorageSync(120, false);
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
        if(temp[0] != '\0' && !dir_exists_local(temp) && INBE_MKDIR(temp) != 0 && !dir_exists_local(temp)) {
            *p = '/';
            return 0;
        }
        *p = '/';
        p++;
    }

    return INBE_MKDIR(path) == 0 || dir_exists_local(path);
}

static int
exec_sql(const char *sql)
{
    char *error = NULL;
    int rc = SQLITE_OK;

    for(int attempt = 0; attempt < 20; attempt++) {
        rc = sqlite3_exec(g_storage.db, sql, NULL, NULL, &error);
        if(rc == SQLITE_OK)
            return 1;
        if(rc != SQLITE_BUSY && rc != SQLITE_LOCKED)
            break;
        sqlite3_free(error);
        error = NULL;
#if !defined(__EMSCRIPTEN__)
        usleep(50000);
#endif
    }
    if(rc != SQLITE_OK) {
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
table_exists(const char *table)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(table == NULL || g_storage.db == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1 LIMIT 1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, table);
    found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

static int
storage_enqueue_sync_entity(const char *entity_type, const char *entity_id, int local_date)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if(g_storage.db == NULL || entity_type == NULL || entity_id == NULL || entity_id[0] == '\0')
        return 0;
    if(sqlite3_prepare_v2(
           g_storage.db,
           "INSERT INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
           "VALUES(?1,?2,?3,?4) "
           "ON CONFLICT(entity_type,entity_id,local_date) DO UPDATE SET queued_at=excluded.queued_at",
           -1,
           &stmt,
           NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, entity_type);
    bind_text(stmt, 2, entity_id);
    sqlite3_bind_int(stmt, 3, local_date);
    sqlite3_bind_int64(stmt, 4, now_seconds());
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

static int
storage_enqueue_sync_habit(const char *habit_id)
{
    return storage_enqueue_sync_entity("habit", habit_id, 0);
}

static int
storage_enqueue_sync_habit_day(const char *habit_id, int local_date)
{
    return storage_enqueue_sync_entity("habit_day", habit_id, local_date);
}

static int
storage_enqueue_sync_session(const char *session_id)
{
    return storage_enqueue_sync_entity("session", session_id, 0);
}

static void
storage_enqueue_all_sync_state(void)
{
    if(g_storage.db == NULL)
        return;
    if(!exec_sql("BEGIN IMMEDIATE"))
        return;
    exec_sql("INSERT INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
             "SELECT 'habit',id,0,strftime('%s','now') FROM habits "
             "WHERE user_id=(SELECT id FROM users LIMIT 1) "
             "ON CONFLICT(entity_type,entity_id,local_date) DO UPDATE SET queued_at=excluded.queued_at");
    exec_sql("INSERT INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
             "SELECT 'habit_day',hd.habit_id,hd.local_date,strftime('%s','now') "
             "FROM habit_days hd JOIN habits h ON h.id=hd.habit_id "
             "WHERE h.user_id=(SELECT id FROM users LIMIT 1) "
             "ON CONFLICT(entity_type,entity_id,local_date) DO UPDATE SET queued_at=excluded.queued_at");
    exec_sql("INSERT INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
             "SELECT 'session',id,0,strftime('%s','now') FROM sessions "
             "WHERE user_id=(SELECT id FROM users LIMIT 1) "
             "ON CONFLICT(entity_type,entity_id,local_date) DO UPDATE SET queued_at=excluded.queued_at");
    exec_sql("COMMIT");
}

static int
migrate_schema(void)
{
    long long now = now_seconds();
    int had_outbox = table_exists("sync_outbox");

    if(table_has_column("habits", "sync_topic")) {
        if(!exec_sql(
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
            " deleted_at INTEGER NOT NULL DEFAULT 0,"
            " updated_at INTEGER NOT NULL DEFAULT 0"
            ");"
            "INSERT INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,sort_order,deleted_at,updated_at)"
            " SELECT id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,sort_order,deleted_at,0"
            " FROM habits_with_sync_topic;"
            "DROP TABLE habits_with_sync_topic;"
            "COMMIT;"))
            return 0;
    }
    if(!table_has_column("habits", "counter_enabled") &&
       !exec_sql("ALTER TABLE habits ADD COLUMN counter_enabled INTEGER NOT NULL DEFAULT 0"))
        return 0;
    if(!table_has_column("habit_days", "count") &&
       !exec_sql("ALTER TABLE habit_days ADD COLUMN count INTEGER NOT NULL DEFAULT 0;"
                 "UPDATE habit_days SET count=CASE WHEN completed!=0 THEN 1 ELSE 0 END WHERE count=0"))
        return 0;
    if(!table_has_column("habit_days", "session_count") &&
       !exec_sql("ALTER TABLE habit_days ADD COLUMN session_count INTEGER NOT NULL DEFAULT 0"))
        return 0;
    if(!table_has_column("habits", "updated_at")) {
        char sql[160];
        snprintf(sql, sizeof(sql), "ALTER TABLE habits ADD COLUMN updated_at INTEGER NOT NULL DEFAULT %lld", now);
        if(!exec_sql(sql))
            return 0;
    }
    if(!table_has_column("sessions", "updated_at")) {
        char sql[160];
        snprintf(sql, sizeof(sql), "ALTER TABLE sessions ADD COLUMN updated_at INTEGER NOT NULL DEFAULT %lld", now);
        if(!exec_sql(sql))
            return 0;
    }
    if(!exec_sql(
           "CREATE TABLE IF NOT EXISTS sync_outbox("
           " seq INTEGER PRIMARY KEY AUTOINCREMENT,"
           " entity_type TEXT NOT NULL,"
           " entity_id TEXT NOT NULL,"
           " local_date INTEGER NOT NULL DEFAULT 0,"
           " queued_at INTEGER NOT NULL,"
           " UNIQUE(entity_type,entity_id,local_date)"
           ");"))
        return 0;
    if(!had_outbox) {
        if(!exec_sql(
               "INSERT OR IGNORE INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
               "SELECT 'habit',id,0,updated_at FROM habits;"
               "INSERT OR IGNORE INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
               "SELECT 'habit_day',habit_id,local_date,updated_at FROM habit_days;"
               "INSERT OR IGNORE INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
               "SELECT 'session',id,0,updated_at FROM sessions;"))
            return 0;
    }
    return 1;
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

static int
source_table_exists(sqlite3 *db, const char *table)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(db == NULL || table == NULL)
        return 0;
    if(sqlite3_prepare_v2(db,
                          "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1 LIMIT 1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(stmt, 1, table, -1, SQLITE_TRANSIENT);
    found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

static int
source_count_rows(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;

    if(db == NULL || sql == NULL)
        return 0;
    if(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    if(sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

static const char *const importable_setting_keys[] = {
    "speed",
    "max_rounds",
    "max_breaths",
    "pause_seconds",
    "sound_volume",
    "tutorial_seen",
    "exercise_manual_seen_mask",
    "theme",
    "dark_mode",
    "theme_mode",
    "orientation_mode",
    "main_tab",
    "fullscreen",
    "on_screen_keyboard",
    "progressive_speed",
    "progressive_start_speed",
    "breath_animation",
    "advanced_session_controls",
    "hold_display_mode",
    "exercise_type",
    "meditation_music_enabled",
    "meditation_music_shuffle",
    "meditation_music_track",
    "play_in_background",
    "language",
    "practice_category_tab"
};

static int
setting_key_importable(const char *key)
{
    if(key == NULL || key[0] == '\0')
        return 0;
    for(size_t i = 0; i < sizeof(importable_setting_keys) / sizeof(importable_setting_keys[0]); i++) {
        if(strcmp(key, importable_setting_keys[i]) == 0)
            return 1;
    }
    return 0;
}

static int
source_count_importable_settings(sqlite3 *db)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;

    if(db == NULL || !source_table_exists(db, "settings"))
        return 0;
    if(sqlite3_prepare_v2(db, "SELECT DISTINCT key FROM settings", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *key = (const char *)sqlite3_column_text(stmt, 0);
        if(setting_key_importable(key))
            count++;
    }
    sqlite3_finalize(stmt);
    return count;
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
local_habit_id_by_name(const char *name, char *out, size_t out_size)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(name == NULL || name[0] == '\0' || out == NULL || out_size == 0 ||
       g_storage.db == NULL)
        return 0;

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id FROM habits WHERE name=?1 COLLATE NOCASE AND deleted_at=0 ORDER BY sort_order,id LIMIT 1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, name);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *id = (const char *)sqlite3_column_text(stmt, 0);
        if(id != NULL && id[0] != '\0') {
            snprintf(out, out_size, "%s", id);
            found = out[0] != '\0';
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

static int
local_habit_id_exists(const char *id)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(id == NULL || id[0] == '\0' || g_storage.db == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT 1 FROM habits WHERE id=?1 LIMIT 1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, id);
    found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

static void
make_import_habit_id(char *out, size_t out_size)
{
    static unsigned int counter = 0;

    if(out == NULL || out_size == 0)
        return;
    do {
        snprintf(out, out_size, "import-%lld-%u", now_seconds(), counter++);
    } while(local_habit_id_exists(out));
}

static int
resolve_import_habit_id(const char *import_id, const char *name,
                        char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';

    if(local_habit_id_by_name(name, out, out_size))
        return 1;

    if(import_id != NULL && import_id[0] != '\0' && !local_habit_id_exists(import_id)) {
        snprintf(out, out_size, "%s", import_id);
        return out[0] != '\0';
    }

    make_import_habit_id(out, out_size);
    return out[0] != '\0';
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
        " updated_at INTEGER NOT NULL DEFAULT 0,"
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
        " counter_enabled INTEGER NOT NULL DEFAULT 0,"
        " sort_order INTEGER NOT NULL,"
        " deleted_at INTEGER NOT NULL DEFAULT 0,"
        " updated_at INTEGER NOT NULL DEFAULT 0"
        ");"
        "CREATE TABLE IF NOT EXISTS habit_days("
        " habit_id TEXT NOT NULL,"
        " local_date INTEGER NOT NULL,"
        " completed INTEGER NOT NULL,"
        " count INTEGER NOT NULL DEFAULT 0,"
        " session_count INTEGER NOT NULL DEFAULT 0,"
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
        "CREATE TABLE IF NOT EXISTS sync_outbox("
        " seq INTEGER PRIMARY KEY AUTOINCREMENT,"
        " entity_type TEXT NOT NULL,"
        " entity_id TEXT NOT NULL,"
        " local_date INTEGER NOT NULL DEFAULT 0,"
        " queued_at INTEGER NOT NULL,"
        " UNIQUE(entity_type,entity_id,local_date)"
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

static long long
get_meta_int64(const char *key, long long fallback)
{
    sqlite3_stmt *stmt = NULL;
    long long value = fallback;

    if(g_storage.db == NULL || key == NULL)
        return fallback;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT value FROM meta WHERE key=?1", -1, &stmt, NULL) != SQLITE_OK)
        return fallback;
    bind_text(stmt, 1, key);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *text = (const char *)sqlite3_column_text(stmt, 0);
        if(text != NULL && text[0] != '\0')
            value = atoll(text);
    }
    sqlite3_finalize(stmt);
    return value;
}

static void
set_meta_int64(const char *key, long long value)
{
    char text[32];
    snprintf(text, sizeof(text), "%lld", value);
    set_meta(key, text);
}

void
storage_reset_sync_state(void)
{
    if(g_storage.db == NULL)
        return;
    set_meta_int64("sync_last_server_version", 0);
    set_meta_int64("sync_last_upload_at", 0);
    set_meta_int64("sync_full_upload_done", 0);
    exec_sql("DELETE FROM sync_outbox");
    storage_enqueue_all_sync_state();
    storage_schedule_persist();
}

const char *
storage_sync_client_id(void)
{
    static char client_id[64];
    const char *stored;

    client_id[0] = '\0';
    stored = NULL;
    if(g_storage.db != NULL) {
        sqlite3_stmt *stmt = NULL;
        if(sqlite3_prepare_v2(g_storage.db, "SELECT value FROM meta WHERE key='sync_client_id'", -1, &stmt, NULL) == SQLITE_OK &&
           sqlite3_step(stmt) == SQLITE_ROW) {
            stored = (const char *)sqlite3_column_text(stmt, 0);
            if(stored != NULL)
                snprintf(client_id, sizeof(client_id), "%s", stored);
        }
        sqlite3_finalize(stmt);
    }
    if(client_id[0] == '\0') {
        make_client_uuid(client_id);
        set_meta("sync_client_id", client_id);
        storage_schedule_persist();
    }
    return client_id;
}

int
storage_settings_empty(void)
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
storage_get_setting_int(const char *key, int fallback)
{
    const char *text = storage_get_setting_text(key);
    return text != NULL && text[0] != '\0' ? atoi(text) : fallback;
}

const char *
storage_get_setting_text(const char *key)
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
storage_set_setting_text(const char *key, const char *value)
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
storage_settings_begin_write(void)
{
    if(g_storage.db == NULL)
        return;
    exec_sql("BEGIN IMMEDIATE");
}

void
storage_settings_end_write(void)
{
    if(g_storage.db == NULL)
        return;
    exec_sql("COMMIT");
    storage_schedule_persist();
}

void
storage_set_setting_int(const char *key, int value)
{
    char text[32];
    snprintf(text, sizeof(text), "%d", value);
    storage_set_setting_text(key, text);
}

static int
storage_build_session_habit_counts(void)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if(!exec_sql("DROP TABLE IF EXISTS temp.inbe_session_habit_counts;"
                 "CREATE TEMP TABLE inbe_session_habit_counts("
                 " habit_id TEXT NOT NULL,"
                 " local_date INTEGER NOT NULL,"
                 " session_count INTEGER NOT NULL,"
                 " PRIMARY KEY(habit_id,local_date)"
                 ");"))
        return 0;

    if(sqlite3_prepare_v2(
           g_storage.db,
           "INSERT INTO inbe_session_habit_counts(habit_id,local_date,session_count) "
           "SELECT h.id,s.local_date,COUNT(*) "
           "FROM habits h JOIN sessions s ON s.user_id=h.user_id "
           "WHERE h.user_id=?1 AND h.deleted_at=0 AND s.deleted_at=0 "
           "  AND h.sync_mode=?2 AND h.sync_activity<>0 "
           "  AND s.local_date>0 AND s.activity>=0 AND s.activity<30 "
           "  AND (h.sync_activity & (1 << s.activity))<>0 "
           "GROUP BY h.id,s.local_date",
           -1,
           &stmt,
           NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    sqlite3_bind_int(stmt, 2, INBE_HABIT_SYNC_ACTIVITIES);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

static int
storage_insert_habit_day_count(const char *habit_id, int local_date,
                               int count, long long updated_at)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if(sqlite3_prepare_v2(
           g_storage.db,
           "INSERT INTO habit_days(habit_id,local_date,completed,count,session_count,updated_at) "
           "VALUES(?1,?2,?3,?4,?4,?5)",
           -1,
           &stmt,
           NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, habit_id);
    sqlite3_bind_int(stmt, 2, local_date);
    sqlite3_bind_int(stmt, 3, count > 0 ? 1 : 0);
    sqlite3_bind_int(stmt, 4, count);
    sqlite3_bind_int64(stmt, 5, updated_at);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if(rc == SQLITE_DONE)
        storage_enqueue_sync_habit_day(habit_id, local_date);
    return rc == SQLITE_DONE;
}

static int
storage_update_habit_day_count(const char *habit_id, int local_date,
                               int completed, int count, int session_count,
                               long long updated_at)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if(sqlite3_prepare_v2(
           g_storage.db,
           "UPDATE habit_days "
           "SET completed=?3,count=?4,session_count=?5,updated_at=?6 "
           "WHERE habit_id=?1 AND local_date=?2",
           -1,
           &stmt,
           NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, habit_id);
    sqlite3_bind_int(stmt, 2, local_date);
    sqlite3_bind_int(stmt, 3, completed);
    sqlite3_bind_int(stmt, 4, count);
    sqlite3_bind_int(stmt, 5, session_count);
    sqlite3_bind_int64(stmt, 6, updated_at);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if(rc == SQLITE_DONE && sqlite3_changes(g_storage.db) > 0)
        storage_enqueue_sync_habit_day(habit_id, local_date);
    return rc == SQLITE_DONE;
}

static int
storage_update_habit_day_session_count(const char *habit_id, int local_date,
                                       int session_count)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE habit_days SET session_count=?3 "
                          "WHERE habit_id=?1 AND local_date=?2",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, habit_id);
    sqlite3_bind_int(stmt, 2, local_date);
    sqlite3_bind_int(stmt, 3, session_count);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

static int
storage_apply_session_habit_count(const char *habit_id, int local_date,
                                  int session_count, long long changed_at)
{
    sqlite3_stmt *stmt = NULL;
    int rc;
    int old_completed = 0;
    int old_count = 0;
    int old_session_count = 0;
    int next_count;
    int next_completed;

    if(habit_id == NULL || habit_id[0] == '\0' || local_date <= 0 || session_count <= 0)
        return 1;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT completed,count,session_count FROM habit_days "
                          "WHERE habit_id=?1 AND local_date=?2",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, habit_id);
    sqlite3_bind_int(stmt, 2, local_date);
    rc = sqlite3_step(stmt);
    if(rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return storage_insert_habit_day_count(habit_id, local_date, session_count, changed_at);
    }
    if(rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 0;
    }
    old_completed = sqlite3_column_int(stmt, 0);
    old_count = sqlite3_column_int(stmt, 1);
    old_session_count = sqlite3_column_int(stmt, 2);
    sqlite3_finalize(stmt);

    next_count = old_count;
    if(old_count <= old_session_count || session_count > old_count)
        next_count = session_count;
    next_completed = next_count > 0 ? 1 : 0;
    if(next_completed == old_completed &&
       next_count == old_count &&
       session_count == old_session_count)
        return 1;
    if(next_completed == old_completed && next_count == old_count)
        return storage_update_habit_day_session_count(habit_id, local_date, session_count);
    return storage_update_habit_day_count(habit_id, local_date, next_completed,
                                          next_count, session_count, changed_at);
}

static int
storage_clear_stale_session_habit_counts(long long changed_at)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 1;

    if(sqlite3_prepare_v2(
           g_storage.db,
           "SELECT hd.habit_id,hd.local_date,hd.count,hd.session_count "
           "FROM habit_days hd JOIN habits h ON h.id=hd.habit_id "
           "WHERE h.user_id=?1 AND h.deleted_at=0 AND h.sync_mode=?2 AND h.sync_activity<>0 "
           "  AND hd.session_count>0 "
           "  AND NOT EXISTS (SELECT 1 FROM inbe_session_habit_counts c "
           "                  WHERE c.habit_id=hd.habit_id AND c.local_date=hd.local_date)",
           -1,
           &stmt,
           NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    sqlite3_bind_int(stmt, 2, INBE_HABIT_SYNC_ACTIVITIES);
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *habit_id = (const char *)sqlite3_column_text(stmt, 0);
        int local_date = sqlite3_column_int(stmt, 1);
        int count = sqlite3_column_int(stmt, 2);
        int session_count = sqlite3_column_int(stmt, 3);
        char habit_id_copy[INBE_STORAGE_ID_SIZE];

        snprintf(habit_id_copy, sizeof(habit_id_copy), "%s", habit_id != NULL ? habit_id : "");
        if(count <= session_count) {
            ok = storage_update_habit_day_count(habit_id_copy, local_date, 0, 0, 0, changed_at);
        } else {
            ok = storage_update_habit_day_session_count(habit_id_copy, local_date, 0);
        }
        if(!ok)
            break;
    }
    sqlite3_finalize(stmt);
    return ok;
}

static int
storage_apply_session_habit_counts(long long changed_at)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 1;

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT habit_id,local_date,session_count "
                          "FROM inbe_session_habit_counts",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *habit_id = (const char *)sqlite3_column_text(stmt, 0);
        int local_date = sqlite3_column_int(stmt, 1);
        int session_count = sqlite3_column_int(stmt, 2);
        char habit_id_copy[INBE_STORAGE_ID_SIZE];

        snprintf(habit_id_copy, sizeof(habit_id_copy), "%s", habit_id != NULL ? habit_id : "");
        ok = storage_apply_session_habit_count(habit_id_copy, local_date, session_count, changed_at);
        if(!ok)
            break;
    }
    sqlite3_finalize(stmt);
    return ok && storage_clear_stale_session_habit_counts(changed_at);
}

static int
storage_materialize_session_habit_days(void)
{
    long long changed_at;
    int ok;

    if(g_storage.db == NULL || g_storage.user_id[0] == '\0')
        return 0;
    if(g_storage.materialize_defer > 0) {
        g_storage.materialize_needed = 1;
        return 1;
    }

    changed_at = now_seconds();
    if(!exec_sql("BEGIN IMMEDIATE"))
        return 0;
    ok = storage_build_session_habit_counts() &&
         storage_apply_session_habit_counts(changed_at) &&
         exec_sql("DROP TABLE IF EXISTS temp.inbe_session_habit_counts;");
    if(ok) {
        exec_sql("COMMIT");
    } else {
        TraceLog(LOG_WARNING, "STORAGE: failed to materialize session habit counts: %s",
                 sqlite3_errmsg(g_storage.db));
        exec_sql("ROLLBACK");
    }
    return ok;
}

static int
insert_session_at_ex(long long started_at, int local_date, const int *round_times,
                     int round_count, int topic, int activity, const char *source,
                     char *out_id, size_t out_id_size)
{
    sqlite3_stmt *stmt = NULL;
    char id[INBE_STORAGE_ID_SIZE];
    int rc;
    int inserted;
    unsigned int rhash;

    if(g_storage.db == NULL || round_times == NULL || round_count <= 0 || round_count > MaxRounds)
        return 0;

    rhash = hash_rounds(round_times, round_count);
    make_session_id(started_at, round_times, round_count, id, sizeof(id));

    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT OR IGNORE INTO sessions(id,user_id,started_at,local_date,topic,activity,source,imported_at,rounds_hash,updated_at) "
                          "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)",
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
    sqlite3_bind_int64(stmt, 10, now_seconds());
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if(rc != SQLITE_DONE)
        return 0;

    inserted = sqlite3_changes(g_storage.db) > 0;
    if(inserted) {
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
        storage_enqueue_sync_session(id);
    }

    if(out_id != NULL && out_id_size > 0)
        snprintf(out_id, out_id_size, "db:%s", id);
    if(inserted)
        storage_materialize_session_habit_days();
    storage_schedule_persist();
    return 1;
}

int
storage_save_session_for_activity(const int *round_times, int round_count,
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
storage_save_session(const int *round_times, int round_count, char *out_id, size_t out_id_size)
{
    return storage_save_session_for_activity(round_times, round_count, 0, 0,
                                                  out_id, out_id_size);
}

int
storage_load_session(const char *path_or_id, int *round_times, int max_rounds,
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
storage_replace_session(const char *path_or_id, const int *round_times, int round_count)
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
        return storage_delete_session(path_or_id);

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
    if(sqlite3_prepare_v2(g_storage.db, "UPDATE sessions SET rounds_hash=?2,updated_at=?3 WHERE id=?1", -1, &stmt, NULL) != SQLITE_OK)
        goto fail;
    bind_text(stmt, 1, id);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)hash_rounds(saved, saved_count));
    sqlite3_bind_int64(stmt, 3, now_seconds());
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    storage_enqueue_sync_session(id);
    exec_sql("COMMIT");
    storage_materialize_session_habit_days();
    storage_schedule_persist();
    return 1;

fail:
    if(stmt != NULL)
        sqlite3_finalize(stmt);
    exec_sql("ROLLBACK");
    return 0;
}

int
storage_rename_session_time(const char *path_or_id, int hour, int minute)
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
    if(sqlite3_prepare_v2(g_storage.db, "UPDATE sessions SET started_at=?2,updated_at=?3 WHERE id=?1", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, id);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)t);
    sqlite3_bind_int64(stmt, 3, now_seconds());
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    storage_enqueue_sync_session(id);
    storage_materialize_session_habit_days();
    storage_schedule_persist();
    return 1;
}

int
storage_delete_session(const char *path_or_id)
{
    sqlite3_stmt *stmt = NULL;
    char id[INBE_STORAGE_ID_SIZE];
    if(!parse_db_id(path_or_id, id, sizeof(id)))
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "UPDATE sessions SET deleted_at=?2,updated_at=?2 WHERE id=?1", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, id);
    sqlite3_bind_int64(stmt, 2, now_seconds());
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    storage_enqueue_sync_session(id);
    storage_materialize_session_habit_days();
    storage_schedule_persist();
    return 1;
}

static void
storage_append_habits_json(JsonBuilder *json, long long through_seq)
{
    sqlite3_stmt *stmt = NULL;
    int first = 1;

    json_append(json, "\"habits\":[");
    if(g_storage.db != NULL &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_enabled,sort_order,deleted_at,updated_at "
                          "FROM habits WHERE user_id=?1 AND id IN ("
                          " SELECT entity_id FROM sync_outbox WHERE entity_type='habit' AND seq<=?2"
                          ") ORDER BY updated_at,sort_order,id",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, g_storage.user_id);
        sqlite3_bind_int64(stmt, 2, through_seq);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            if(!first)
                json_append(json, ",");
            first = 0;
            json_append(json, "{");
            json_append_key_string(json, "id", (const char *)sqlite3_column_text(stmt, 0));
            json_append(json, ",");
            json_append_key_string(json, "name", (const char *)sqlite3_column_text(stmt, 1));
            json_appendf(json, ",\"color_r\":%d,\"color_g\":%d,\"color_b\":%d",
                         sqlite3_column_int(stmt, 2),
                         sqlite3_column_int(stmt, 3),
                         sqlite3_column_int(stmt, 4));
            json_appendf(json, ",\"sync_mode\":%d,\"sync_activity\":%d,\"counter_enabled\":%d,\"sort_order\":%d,\"deleted_at\":%lld",
                         sqlite3_column_int(stmt, 5),
                         sqlite3_column_int(stmt, 6),
                         (sqlite3_column_int(stmt, 7) != 0 ||
                          sqlite3_column_int(stmt, 6) != 0) ? 1 : 0,
                         sqlite3_column_int(stmt, 8),
                         sqlite3_column_int64(stmt, 9));
            json_append(json, ",\"updated_at\":");
            json_append_epoch(json, sqlite3_column_int64(stmt, 10));
            json_append(json, "}");
        }
    }
    if(stmt != NULL)
        sqlite3_finalize(stmt);
    json_append(json, "]");
}

static void
storage_append_habit_days_json(JsonBuilder *json, long long through_seq)
{
    sqlite3_stmt *stmt = NULL;
    int first = 1;

    json_append(json, "\"habit_days\":[");
    if(g_storage.db != NULL &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT hd.habit_id,hd.local_date,hd.completed,hd.count,hd.updated_at "
                          "FROM habit_days hd JOIN habits h ON h.id=hd.habit_id "
                          "WHERE h.user_id=?1 AND EXISTS ("
                          " SELECT 1 FROM sync_outbox o WHERE o.entity_type='habit_day' "
                          " AND o.entity_id=hd.habit_id AND o.local_date=hd.local_date AND o.seq<=?2"
                          ") "
                          "ORDER BY hd.updated_at,hd.habit_id,hd.local_date",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, g_storage.user_id);
        sqlite3_bind_int64(stmt, 2, through_seq);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            if(!first)
                json_append(json, ",");
            first = 0;
            json_append(json, "{");
            json_append_key_string(json, "habit_id", (const char *)sqlite3_column_text(stmt, 0));
            json_appendf(json, ",\"local_date\":%d,\"completed\":%s,\"count\":%d,\"updated_at\":",
                         sqlite3_column_int(stmt, 1),
                         sqlite3_column_int(stmt, 2) != 0 ? "true" : "false",
                         sqlite3_column_int(stmt, 3));
            json_append_epoch(json, sqlite3_column_int64(stmt, 4));
            json_append(json, "}");
        }
    }
    if(stmt != NULL)
        sqlite3_finalize(stmt);
    json_append(json, "]");
}

static void
storage_append_session_rounds_json(JsonBuilder *json, const char *session_id)
{
    sqlite3_stmt *stmt = NULL;
    int first = 1;

    json_append(json, "\"rounds\":[");
    if(g_storage.db != NULL && session_id != NULL &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT round_index,seconds FROM session_rounds "
                          "WHERE session_id=?1 ORDER BY round_index",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, session_id);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            int seconds = sqlite3_column_int(stmt, 1);
            if(!first)
                json_append(json, ",");
            first = 0;
            json_appendf(json, "{\"round_index\":%d,\"breaths\":0,\"hold_seconds\":%d}",
                         sqlite3_column_int(stmt, 0), seconds);
        }
    }
    if(stmt != NULL)
        sqlite3_finalize(stmt);
    json_append(json, "]");
}

static void
storage_append_sessions_json(JsonBuilder *json, long long through_seq)
{
    sqlite3_stmt *stmt = NULL;
    int first = 1;

    json_append(json, "\"sessions\":[");
    if(g_storage.db != NULL &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT id,started_at,local_date,topic,activity,source,rounds_hash,deleted_at,updated_at "
                          "FROM sessions WHERE user_id=?1 AND id IN ("
                          " SELECT entity_id FROM sync_outbox WHERE entity_type='session' AND seq<=?2"
                          ") "
                          "ORDER BY updated_at,started_at,id",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, g_storage.user_id);
        sqlite3_bind_int64(stmt, 2, through_seq);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            const char *id = (const char *)sqlite3_column_text(stmt, 0);
            long long started_at = sqlite3_column_int64(stmt, 1);
            long long updated_at = sqlite3_column_int64(stmt, 8);
            if(!first)
                json_append(json, ",");
            first = 0;
            json_append(json, "{");
            json_append_key_string(json, "id", id);
            json_append(json, ",\"started_at\":");
            json_append_epoch(json, started_at);
            json_appendf(json, ",\"local_date\":%d,\"topic\":\"%d\",\"activity\":%d,",
                         sqlite3_column_int(stmt, 2),
                         sqlite3_column_int(stmt, 3),
                         sqlite3_column_int(stmt, 4));
            json_append_key_string(json, "source", (const char *)sqlite3_column_text(stmt, 5));
            json_append(json, ",");
            json_append_key_string(json, "rounds_hash", (const char *)sqlite3_column_text(stmt, 6));
            json_appendf(json, ",\"deleted_at\":%lld,\"updated_at\":",
                         sqlite3_column_int64(stmt, 7));
            json_append_epoch(json, updated_at > 0 ? updated_at : started_at);
            json_append(json, ",");
            storage_append_session_rounds_json(json, id);
            json_append(json, "}");
        }
    }
    if(stmt != NULL)
        sqlite3_finalize(stmt);
    json_append(json, "]");
}

char *
storage_build_sync_payload_json(const char *user_id_hash, const char *public_key_hex)
{
    JsonBuilder json = {0};
    long long since_server_version;
    long long through_seq;
    int full_upload_done;

    if(g_storage.db == NULL || user_id_hash == NULL || user_id_hash[0] == '\0')
        return NULL;
    if(!storage_materialize_session_habit_days())
        return NULL;
    since_server_version = get_meta_int64("sync_last_server_version", 0);
    full_upload_done = get_meta_int64("sync_full_upload_done", 0) != 0;
    through_seq = storage_max_sync_outbox_seq();
    g_storage.pending_sync_outbox_seq = through_seq;
    json.ok = 1;
    json_append(&json, "{");
    json_append_key_string(&json, "user_id_hash", user_id_hash);
    json_append(&json, ",");
    json_append_key_string(&json, "client_id", storage_sync_client_id());
    json_appendf(&json, ",\"since_server_version\":%lld", since_server_version);
    if(since_server_version <= 0 || !full_upload_done)
        json_append(&json, ",\"bootstrap\":true");
    if(public_key_hex != NULL && public_key_hex[0] != '\0') {
        json_append(&json, ",");
        json_append_key_string(&json, "public_key", public_key_hex);
    }
    json_append(&json, ",\"preferences\":[],");
    storage_append_habits_json(&json, through_seq);
    json_append(&json, ",");
    storage_append_habit_days_json(&json, through_seq);
    json_append(&json, ",");
    storage_append_sessions_json(&json, through_seq);
    json_append(&json, "}");

    if(!json.ok || json.data == NULL) {
        free(json.data);
        return NULL;
    }
    return json.data;
}

void
storage_free_sync_payload_json(char *payload)
{
    free(payload);
}

static int
storage_json_valid(const char *json)
{
    sqlite3_stmt *stmt = NULL;
    int valid = 0;

    if(g_storage.db == NULL || json == NULL || json[0] == '\0')
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT json_valid(?1)", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, json);
    if(sqlite3_step(stmt) == SQLITE_ROW)
        valid = sqlite3_column_int(stmt, 0) != 0;
    sqlite3_finalize(stmt);
    return valid;
}

static long long
storage_json_extract_int64(const char *json, const char *path, long long fallback)
{
    sqlite3_stmt *stmt = NULL;
    long long value = fallback;

    if(g_storage.db == NULL || json == NULL || path == NULL)
        return fallback;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT CAST(COALESCE(json_extract(?1,?2),?3) AS INTEGER)", -1, &stmt, NULL) != SQLITE_OK)
        return fallback;
    bind_text(stmt, 1, json);
    bind_text(stmt, 2, path);
    sqlite3_bind_int64(stmt, 3, fallback);
    if(sqlite3_step(stmt) == SQLITE_ROW)
        value = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return value;
}

static long long
storage_max_sync_outbox_seq(void)
{
    sqlite3_stmt *stmt = NULL;
    long long seq = 0;

    if(g_storage.db == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COALESCE(MAX(seq),0) FROM sync_outbox", -1,
                          &stmt, NULL) != SQLITE_OK)
        return 0;
    if(sqlite3_step(stmt) == SQLITE_ROW)
        seq = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return seq;
}

static void
storage_clear_uploaded_outbox(long long through_seq)
{
    sqlite3_stmt *stmt = NULL;

    if(g_storage.db == NULL || through_seq <= 0)
        return;
    if(sqlite3_prepare_v2(g_storage.db, "DELETE FROM sync_outbox WHERE seq<=?1", -1,
                          &stmt, NULL) != SQLITE_OK)
        return;
    sqlite3_bind_int64(stmt, 1, through_seq);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static int
storage_json_array_has_items(const char *json, const char *path)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;

    if(g_storage.db == NULL || json == NULL || path == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT COALESCE(json_array_length(json_extract(?1,?2)),0)",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, json);
    bind_text(stmt, 2, path);
    if(sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count > 0;
}

static int
storage_sync_response_has_changes(const char *json)
{
    return storage_json_array_has_items(json, "$.changes.habits") ||
           storage_json_array_has_items(json, "$.changes.habit_days") ||
           storage_json_array_has_items(json, "$.changes.sessions") ||
           storage_json_array_has_items(json, "$.changes.meditation_logs");
}

static int
storage_exec_json_user_sql(const char *sql, const char *json)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if(g_storage.db == NULL || sql == NULL || json == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        TraceLog(LOG_WARNING, "SYNC: changes SQL prepare failed: %s", sqlite3_errmsg(g_storage.db));
        return 0;
    }
    bind_text(stmt, 1, json);
    if(sqlite3_bind_parameter_count(stmt) >= 2)
        bind_text(stmt, 2, g_storage.user_id);
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE) {
        TraceLog(LOG_WARNING, "SYNC: changes SQL step failed: %s", sqlite3_errmsg(g_storage.db));
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    return 1;
}

int
storage_apply_sync_response_json(const char *response_json)
{
    static const char *habits_sql =
        "INSERT INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_enabled,sort_order,deleted_at,updated_at) "
        "SELECT COALESCE(json_extract(value,'$.id'),''),?2,"
        "       COALESCE(json_extract(value,'$.name'),''),"
        "       CAST(COALESCE(json_extract(value,'$.color_r'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.color_g'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.color_b'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.sync_mode'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.sync_activity'),0) AS INTEGER),"
        "       CASE WHEN CAST(COALESCE(json_extract(value,'$.sync_activity'),0) AS INTEGER)<>0 THEN 1 ELSE 0 END,"
        "       CAST(COALESCE(json_extract(value,'$.sort_order'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.deleted_at'),0) AS INTEGER),"
        "       CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),'0') AS INTEGER) "
        "FROM json_each(?1,'$.changes.habits') "
        "WHERE COALESCE(json_extract(value,'$.id'),'')<>'' "
        "ON CONFLICT(id) DO UPDATE SET "
        " user_id=excluded.user_id,name=excluded.name,color_r=excluded.color_r,color_g=excluded.color_g,"
        " color_b=excluded.color_b,sync_mode=excluded.sync_mode,sync_activity=excluded.sync_activity,"
        " counter_enabled=excluded.counter_enabled,sort_order=excluded.sort_order,deleted_at=excluded.deleted_at,"
        " updated_at=excluded.updated_at "
        "WHERE excluded.updated_at >= habits.updated_at";
    static const char *habit_days_sql =
        "INSERT INTO habit_days(habit_id,local_date,completed,count,updated_at) "
        "SELECT COALESCE(json_extract(value,'$.habit_id'),''),"
        "       CAST(COALESCE(json_extract(value,'$.local_date'),0) AS INTEGER),"
        "       CASE WHEN json_extract(value,'$.completed') THEN 1 ELSE 0 END,"
        "       CAST(COALESCE(json_extract(value,'$.count'),CASE WHEN json_extract(value,'$.completed') THEN 1 ELSE 0 END) AS INTEGER),"
        "       CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),'0') AS INTEGER) "
        "FROM json_each(?1,'$.changes.habit_days') "
        "WHERE COALESCE(json_extract(value,'$.habit_id'),'')<>'' "
        "  AND CAST(COALESCE(json_extract(value,'$.local_date'),0) AS INTEGER)>0 "
        "ON CONFLICT(habit_id,local_date) DO UPDATE SET "
        " completed=excluded.completed,count=excluded.count,updated_at=excluded.updated_at "
        "WHERE excluded.updated_at > habit_days.updated_at "
        "OR (excluded.updated_at = habit_days.updated_at AND excluded.count > habit_days.count)";
    static const char *sessions_sql =
        "INSERT INTO sessions(id,user_id,started_at,local_date,topic,activity,source,imported_at,rounds_hash,deleted_at,updated_at) "
        "SELECT COALESCE(json_extract(value,'$.id'),''),?2,"
        "       CAST(COALESCE(strftime('%s',json_extract(value,'$.started_at')),'0') AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.local_date'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.topic'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.activity'),0) AS INTEGER),"
        "       COALESCE(json_extract(value,'$.source'),''),"
        "       CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),strftime('%s',json_extract(value,'$.started_at')),'0') AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.rounds_hash'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.deleted_at'),0) AS INTEGER),"
        "       CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),strftime('%s',json_extract(value,'$.started_at')),'0') AS INTEGER) "
        "FROM json_each(?1,'$.changes.sessions') "
        "WHERE COALESCE(json_extract(value,'$.id'),'')<>'' "
        "ON CONFLICT(id) DO UPDATE SET "
        " user_id=excluded.user_id,started_at=excluded.started_at,local_date=excluded.local_date,"
        " topic=excluded.topic,activity=excluded.activity,source=excluded.source,"
        " imported_at=excluded.imported_at,rounds_hash=excluded.rounds_hash,deleted_at=excluded.deleted_at,"
        " updated_at=excluded.updated_at "
        "WHERE excluded.updated_at >= sessions.updated_at";
    static const char *delete_rounds_sql =
        "DELETE FROM session_rounds WHERE session_id IN ("
        " SELECT COALESCE(json_extract(value,'$.id'),'') "
        " FROM json_each(?1,'$.changes.sessions') "
        " WHERE CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),strftime('%s',json_extract(value,'$.started_at')),'0') AS INTEGER) "
        "       >= COALESCE((SELECT updated_at FROM sessions WHERE id=COALESCE(json_extract(value,'$.id'),'')),0)"
        ")";
    static const char *rounds_sql =
        "INSERT OR REPLACE INTO session_rounds(session_id,round_index,seconds) "
        "SELECT COALESCE(json_extract(s.value,'$.id'),''),"
        "       CAST(COALESCE(json_extract(r.value,'$.round_index'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(r.value,'$.hold_seconds'),0) AS INTEGER) "
        "FROM json_each(?1,'$.changes.sessions') AS s, json_each(s.value,'$.rounds') AS r "
        "WHERE COALESCE(json_extract(s.value,'$.id'),'')<>''";
    long long server_version;
    long long old_server_version;

    if(g_storage.db == NULL || response_json == NULL || response_json[0] == '\0')
        return 0;
    g_storage.last_sync_changed = 0;
    if(!storage_json_valid(response_json))
        return 0;
    old_server_version = get_meta_int64("sync_last_server_version", 0);
    if(!exec_sql("BEGIN IMMEDIATE"))
        return 0;
    if(!storage_exec_json_user_sql(habits_sql, response_json) ||
       !storage_exec_json_user_sql(habit_days_sql, response_json) ||
       !storage_exec_json_user_sql(sessions_sql, response_json) ||
       !storage_exec_json_user_sql(delete_rounds_sql, response_json) ||
       !storage_exec_json_user_sql(rounds_sql, response_json)) {
        exec_sql("ROLLBACK");
        return 0;
    }
    if(!exec_sql("COMMIT")) {
        exec_sql("ROLLBACK");
        return 0;
    }
    storage_materialize_session_habit_days();
    server_version = storage_json_extract_int64(response_json, "$.server_version", 0);
    if(server_version > old_server_version && storage_sync_response_has_changes(response_json))
        g_storage.last_sync_changed = 1;
    if(server_version > 0)
        set_meta_int64("sync_last_server_version", server_version);
    storage_clear_uploaded_outbox(g_storage.pending_sync_outbox_seq);
    set_meta_int64("sync_full_upload_done", 1);
    storage_mark_habits_initialized();
    storage_schedule_persist();
    return 1;
}

int
storage_last_sync_changed(void)
{
    return g_storage.last_sync_changed;
}

void
storage_purge_synced_deleted_data(void)
{
    static const char *const sqls[] = {
        "DELETE FROM session_rounds WHERE session_id IN "
        "(SELECT id FROM sessions WHERE deleted_at>0 AND id NOT IN "
        " (SELECT entity_id FROM sync_outbox WHERE entity_type='session'))",
        "DELETE FROM sessions WHERE deleted_at>0 AND id NOT IN "
        "(SELECT entity_id FROM sync_outbox WHERE entity_type='session')",
        "DELETE FROM habit_days WHERE completed=0 AND count=0 AND session_count=0 "
        "AND NOT EXISTS (SELECT 1 FROM sync_outbox o WHERE o.entity_type='habit_day' "
        "AND o.entity_id=habit_days.habit_id AND o.local_date=habit_days.local_date)",
        "DELETE FROM habits WHERE deleted_at>0 AND id NOT IN "
        "(SELECT entity_id FROM sync_outbox WHERE entity_type='habit')"
    };

    if(g_storage.db == NULL || get_meta_int64("sync_full_upload_done", 0) == 0)
        return;
    if(!exec_sql("BEGIN IMMEDIATE"))
        return;
    for(size_t i = 0; i < sizeof(sqls) / sizeof(sqls[0]); i++) {
        if(!exec_sql(sqls[i]))
            goto fail;
    }
    exec_sql("COMMIT");
    storage_schedule_persist();
    return;

fail:
    exec_sql("ROLLBACK");
}

void
storage_list_session_records(InbeStorageSessionRecordCallback callback, void *user)
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
        count = storage_load_session(dbid, rounds, MaxRounds, NULL, NULL, NULL, NULL, NULL, NULL);
        if(count > 0)
            callback(dbid, y, m, d, hh, mm, ss, topic, activity, rounds, count, user);
    }
    sqlite3_finalize(stmt);
}

int
storage_has_any(void)
{
    sqlite3_stmt *stmt = NULL;
    int count = storage_session_count();

    if(count > 0)
        return 1;
    if(g_storage.db == NULL)
        return 0;
    count = storage_habit_count();
    if(count > 0)
        return 1;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT COUNT(*) FROM habit_days WHERE completed!=0 OR count>0",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    if(sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count > 0;
}

int
storage_session_count(void)
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
storage_total_size(void)
{
    struct stat st;
    if(!storage_has_any())
        return 0;
    if(g_storage.db_path[0] != '\0' && stat(g_storage.db_path, &st) == 0)
        return (long long)st.st_size;
    return 0;
}

long long
storage_delete_all_sessions(void)
{
    sqlite3_stmt *stmt = NULL;
    int count = storage_session_count();
    int habit_day_count = 0;
    int habit_count = storage_habit_count();
    long long deleted_at = now_seconds();
    int changed = 0;

    if(g_storage.db == NULL)
        return 0;

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT COUNT(*) FROM habit_days WHERE completed!=0 OR count>0",
                          -1, &stmt, NULL) == SQLITE_OK) {
        if(sqlite3_step(stmt) == SQLITE_ROW)
            habit_day_count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if(count <= 0 && habit_day_count <= 0 && habit_count <= 0)
        return 0;

    if(!exec_sql("BEGIN IMMEDIATE"))
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id FROM sessions WHERE user_id=?1 AND deleted_at=0",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, g_storage.user_id);
        while(sqlite3_step(stmt) == SQLITE_ROW)
            storage_enqueue_sync_session((const char *)sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        stmt = NULL;
    }
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT habit_id,local_date FROM habit_days "
                          "WHERE habit_id IN (SELECT id FROM habits WHERE user_id=?1) "
                          "AND (completed!=0 OR count>0 OR session_count>0)",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, g_storage.user_id);
        while(sqlite3_step(stmt) == SQLITE_ROW)
            storage_enqueue_sync_habit_day((const char *)sqlite3_column_text(stmt, 0),
                                           sqlite3_column_int(stmt, 1));
        sqlite3_finalize(stmt);
        stmt = NULL;
    }
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id FROM habits WHERE user_id=?1 AND deleted_at=0",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, g_storage.user_id);
        while(sqlite3_step(stmt) == SQLITE_ROW)
            storage_enqueue_sync_habit((const char *)sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        stmt = NULL;
    }
    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE sessions SET deleted_at=?1,updated_at=?1 "
                          "WHERE user_id=?2 AND deleted_at=0",
                          -1, &stmt, NULL) != SQLITE_OK)
        goto fail;
    sqlite3_bind_int64(stmt, 1, deleted_at);
    bind_text(stmt, 2, g_storage.user_id);
    if(sqlite3_step(stmt) != SQLITE_DONE)
        goto fail;
    changed += sqlite3_changes(g_storage.db);
    sqlite3_finalize(stmt);
    stmt = NULL;

    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE habit_days SET completed=0,count=0,session_count=0,updated_at=?1 "
                          "WHERE habit_id IN (SELECT id FROM habits WHERE user_id=?2) "
                          "  AND (completed!=0 OR count>0 OR session_count>0)",
                          -1, &stmt, NULL) != SQLITE_OK)
        goto fail;
    sqlite3_bind_int64(stmt, 1, deleted_at);
    bind_text(stmt, 2, g_storage.user_id);
    if(sqlite3_step(stmt) != SQLITE_DONE)
        goto fail;
    changed += sqlite3_changes(g_storage.db);
    sqlite3_finalize(stmt);
    stmt = NULL;

    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE habits SET deleted_at=?1,updated_at=?1 "
                          "WHERE user_id=?2 AND deleted_at=0",
                          -1, &stmt, NULL) != SQLITE_OK)
        goto fail;
    sqlite3_bind_int64(stmt, 1, deleted_at);
    bind_text(stmt, 2, g_storage.user_id);
    if(sqlite3_step(stmt) != SQLITE_DONE)
        goto fail;
    changed += sqlite3_changes(g_storage.db);
    sqlite3_finalize(stmt);
    stmt = NULL;

    if(!exec_sql("COMMIT")) {
        exec_sql("ROLLBACK");
        return 0;
    }
    storage_schedule_persist();
    return changed > 0 ? count + habit_day_count + habit_count : 0;

fail:
    if(stmt != NULL)
        sqlite3_finalize(stmt);
    exec_sql("ROLLBACK");
    return 0;
}

int
storage_habits_empty(void)
{
    return storage_habit_count() == 0;
}

int
storage_habit_count(void)
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
storage_habits_load(void *habits_ptr)
{
    InbeHabits *habits = habits_ptr;
    sqlite3_stmt *stmt = NULL;
    int index = 0;

    if(habits == NULL || g_storage.db == NULL)
        return 0;
    habits_free(habits);
    memset(habits, 0, sizeof(*habits));
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_enabled "
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
        habit->counter_enabled = sqlite3_column_int(stmt, 7) != 0 ||
                                 (habit->sync_mode == INBE_HABIT_SYNC_ACTIVITIES &&
                                  habit->sync_activity != 0);
        index++;
    }
    sqlite3_finalize(stmt);
    habits->count = index;

    for(int i = 0; i < habits->count; i++) {
        if(sqlite3_prepare_v2(g_storage.db,
                              "SELECT local_date,completed,count FROM habit_days WHERE habit_id=?1 ORDER BY local_date",
                              -1, &stmt, NULL) != SQLITE_OK)
            continue;
        bind_text(stmt, 1, habits->items[i].id);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            int d = habits->items[i].day_count++;
            if(!habit_reserve_days(&habits->items[i], habits->items[i].day_count)) {
                habits->items[i].day_count--;
                break;
            }
            habits->items[i].days[d].day_index = sqlite3_column_int(stmt, 0);
            habits->items[i].days[d].completed = sqlite3_column_int(stmt, 1) != 0;
            habits->items[i].days[d].count = sqlite3_column_int(stmt, 2);
            if(habits->items[i].days[d].count <= 0 &&
               habits->items[i].days[d].completed)
                habits->items[i].days[d].count = 1;
        }
        sqlite3_finalize(stmt);
    }
    habits->loaded = 1;
    return habits->count > 0 || meta_equals("habits_initialized", "true");
}

void
storage_mark_habits_initialized(void)
{
    if(g_storage.db != NULL)
        set_meta("habits_initialized", "true");
}

void
storage_habits_save(const void *habits_ptr)
{
    const InbeHabits *habits = habits_ptr;
    sqlite3_stmt *stmt = NULL;
    long long changed_at = now_seconds();
    if(habits == NULL || g_storage.db == NULL)
        return;
    storage_mark_habits_initialized();
    exec_sql("BEGIN IMMEDIATE");
    exec_sql("CREATE TEMP TABLE IF NOT EXISTS sync_seen_habits(id TEXT PRIMARY KEY);"
             "DELETE FROM sync_seen_habits;");
    for(int i = 0; i < habits->count; i++) {
        const InbeHabit *habit = &habits->items[i];
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_enabled,sort_order,deleted_at,updated_at) "
                              "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,0,?11) "
                              "ON CONFLICT(id) DO UPDATE SET "
                              "user_id=excluded.user_id,"
                              "name=excluded.name,"
                              "color_r=excluded.color_r,"
                              "color_g=excluded.color_g,"
                              "color_b=excluded.color_b,"
                              "sync_mode=excluded.sync_mode,"
                              "sync_activity=excluded.sync_activity,"
                              "counter_enabled=excluded.counter_enabled,"
                              "sort_order=excluded.sort_order,"
                              "deleted_at=0,"
                              "updated_at=CASE WHEN habits.user_id<>excluded.user_id OR habits.name<>excluded.name OR "
                              "habits.color_r<>excluded.color_r OR habits.color_g<>excluded.color_g OR habits.color_b<>excluded.color_b OR "
                              "habits.sync_mode<>excluded.sync_mode OR habits.sync_activity<>excluded.sync_activity OR "
                              "habits.counter_enabled<>excluded.counter_enabled OR habits.sort_order<>excluded.sort_order OR "
                              "habits.deleted_at<>0 THEN excluded.updated_at ELSE habits.updated_at END",
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
        sqlite3_bind_int(stmt, 9, habit->counter_enabled ? 1 : 0);
        sqlite3_bind_int(stmt, 10, i);
        sqlite3_bind_int64(stmt, 11, changed_at);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        stmt = NULL;
        storage_enqueue_sync_habit(habit->id);
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT OR IGNORE INTO sync_seen_habits(id) VALUES(?1)",
                              -1, &stmt, NULL) == SQLITE_OK) {
            bind_text(stmt, 1, habit->id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            stmt = NULL;
        }
        for(int d = 0; d < habit->day_count; d++) {
            if(sqlite3_prepare_v2(g_storage.db,
                                  "INSERT INTO habit_days(habit_id,local_date,completed,count,updated_at) VALUES(?1,?2,?3,?4,?5) "
                                  "ON CONFLICT(habit_id,local_date) DO UPDATE SET "
                                  "completed=excluded.completed,"
                                  "count=excluded.count,"
                                  "updated_at=CASE WHEN habit_days.completed<>excluded.completed OR habit_days.count<>excluded.count "
                                  "THEN excluded.updated_at ELSE habit_days.updated_at END",
                                  -1, &stmt, NULL) != SQLITE_OK)
                continue;
            bind_text(stmt, 1, habit->id);
            sqlite3_bind_int(stmt, 2, habit->days[d].day_index);
            sqlite3_bind_int(stmt, 3, habit->days[d].count > 0 ||
                                      habit->days[d].completed ? 1 : 0);
            sqlite3_bind_int(stmt, 4, habit->days[d].count > 0
                                      ? habit->days[d].count
                                      : (habit->days[d].completed ? 1 : 0));
            sqlite3_bind_int64(stmt, 5, changed_at);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            stmt = NULL;
            storage_enqueue_sync_habit_day(habit->id, habit->days[d].day_index);
        }
    }
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id FROM habits WHERE user_id=?1 AND deleted_at=0 "
                          "AND id NOT IN (SELECT id FROM sync_seen_habits)",
                          -1, &stmt, NULL) == SQLITE_OK) {
        char deleted_ids[INBE_HABIT_MAX][INBE_STORAGE_ID_SIZE];
        int deleted_count = 0;
        bind_text(stmt, 1, g_storage.user_id);
        while(deleted_count < INBE_HABIT_MAX && sqlite3_step(stmt) == SQLITE_ROW) {
            snprintf(deleted_ids[deleted_count], sizeof(deleted_ids[deleted_count]), "%s",
                     (const char *)sqlite3_column_text(stmt, 0));
            deleted_count++;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
        for(int i = 0; i < deleted_count; i++) {
            if(sqlite3_prepare_v2(g_storage.db,
                                  "UPDATE habits SET deleted_at=?2,updated_at=?2 WHERE id=?1",
                                  -1, &stmt, NULL) != SQLITE_OK)
                continue;
            bind_text(stmt, 1, deleted_ids[i]);
            sqlite3_bind_int64(stmt, 2, changed_at);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            stmt = NULL;
            storage_enqueue_sync_habit(deleted_ids[i]);
        }
    }
    exec_sql("DELETE FROM sync_seen_habits;");
    exec_sql("COMMIT");
    storage_materialize_session_habit_days();
    storage_schedule_persist();
}

int
storage_export_zip(const char *path)
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
             INBE_VERSION_STRING, g_storage.user_id, storage_session_count(), storage_habit_count());
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
    g_storage.materialize_defer++;
    file_count = mz_zip_reader_get_num_files(archive);
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
    g_storage.materialize_defer--;
    if(g_storage.materialize_defer == 0 && g_storage.materialize_needed) {
        g_storage.materialize_needed = 0;
        storage_materialize_session_habit_days();
    }
    if(imported <= 0)
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
    if(g_storage.db == NULL || g_storage.root[0] == '\0')
        return;
    if(meta_equals("legacy_file_sessions_migrated", "1"))
        return;
    migrate_legacy_file_sessions_in_dir(g_storage.root);
    set_meta("legacy_file_sessions_migrated", "1");
    storage_schedule_persist();
}

static int
import_tickmate_db(sqlite3 *src)
{
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt *write_stmt = NULL;
    int ok = 0;
    int has_multiple_entries;
    const char *track_sql;
    long long imported_at = now_seconds();

    if(src == NULL || g_storage.db == NULL)
        return 0;
    if(!source_table_has_column(src, "tracks", "name") ||
       !source_table_has_column(src, "ticks", "_track_id"))
        return 0;

    has_multiple_entries = source_table_has_column(src, "tracks", "multiple_entries_per_day");
    track_sql = has_multiple_entries
                    ? "SELECT _id,name,color,\"order\",multiple_entries_per_day,"
                      "EXISTS(SELECT 1 FROM ticks WHERE ticks._track_id=tracks._id "
                      "GROUP BY year,month,day HAVING COUNT(*)>1 LIMIT 1) "
                      "FROM tracks WHERE enabled!=0 ORDER BY \"order\",_id"
                    : "SELECT _id,name,color,\"order\",0,"
                      "EXISTS(SELECT 1 FROM ticks WHERE ticks._track_id=tracks._id "
                      "GROUP BY year,month,day HAVING COUNT(*)>1 LIMIT 1) "
                      "FROM tracks WHERE enabled!=0 ORDER BY \"order\",_id";

    if(sqlite3_prepare_v2(src, track_sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    exec_sql("BEGIN IMMEDIATE");
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        char import_habit_id[64];
        char local_habit_id[INBE_STORAGE_ID_SIZE];
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        int track_id = sqlite3_column_int(stmt, 0);
        int sort_order = sqlite3_column_int(stmt, 3);
        int counter_enabled = sqlite3_column_int(stmt, 4) != 0 ||
                              sqlite3_column_int(stmt, 5) != 0;
        Color color = tickmate_color_from_int(sqlite3_column_int(stmt, 2), track_id);

        if(name == NULL || name[0] == '\0')
            continue;
        snprintf(import_habit_id, sizeof(import_habit_id), "tickmate-%d", track_id);
        if(!resolve_import_habit_id(import_habit_id, name, local_habit_id,
                                    sizeof(local_habit_id)))
            continue;
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT OR REPLACE INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_enabled,sort_order,deleted_at,updated_at) "
                              "VALUES(?1,?2,COALESCE((SELECT name FROM habits WHERE id=?1),?3),?4,?5,?6,?7,?8,"
                              "CASE WHEN ?9!=0 THEN 1 ELSE COALESCE((SELECT counter_enabled FROM habits WHERE id=?1),0) END,?10,0,?11)",
                              -1, &write_stmt, NULL) != SQLITE_OK)
            continue;
        bind_text(write_stmt, 1, local_habit_id);
        bind_text(write_stmt, 2, g_storage.user_id);
        bind_text(write_stmt, 3, name);
        sqlite3_bind_int(write_stmt, 4, color.r);
        sqlite3_bind_int(write_stmt, 5, color.g);
        sqlite3_bind_int(write_stmt, 6, color.b);
        sqlite3_bind_int(write_stmt, 7, INBE_HABIT_SYNC_NONE);
        sqlite3_bind_int(write_stmt, 8, 0);
        sqlite3_bind_int(write_stmt, 9, counter_enabled ? 1 : 0);
        sqlite3_bind_int(write_stmt, 10, sort_order);
        sqlite3_bind_int64(write_stmt, 11, imported_at);
        if(sqlite3_step(write_stmt) == SQLITE_DONE)
            ok = 1;
        sqlite3_finalize(write_stmt);
        write_stmt = NULL;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if(sqlite3_prepare_v2(src,
                          "SELECT ticks._track_id,tracks.name,ticks.year,ticks.month,ticks.day,COUNT(*) "
                          "FROM ticks JOIN tracks ON tracks._id=ticks._track_id "
                          "WHERE tracks.enabled!=0 "
                          "GROUP BY ticks._track_id,ticks.year,ticks.month,ticks.day",
                          -1, &stmt, NULL) == SQLITE_OK) {
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT INTO habit_days(habit_id,local_date,completed,count,updated_at) "
                              "VALUES(?1,?2,1,?3,?4) "
                              "ON CONFLICT(habit_id,local_date) DO UPDATE SET "
                              "count=CASE WHEN habit_days.count>excluded.count THEN habit_days.count ELSE excluded.count END,"
                              "completed=CASE WHEN habit_days.count>0 OR excluded.count>0 OR habit_days.completed!=0 THEN 1 ELSE 0 END,"
                              "updated_at=CASE WHEN excluded.count>habit_days.count THEN excluded.updated_at ELSE habit_days.updated_at END",
                              -1, &write_stmt, NULL) != SQLITE_OK) {
            sqlite3_finalize(stmt);
            stmt = NULL;
            goto finish;
        }
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            char import_habit_id[64];
            char local_habit_id[INBE_STORAGE_ID_SIZE];
            int track_id = sqlite3_column_int(stmt, 0);
            const char *name = (const char *)sqlite3_column_text(stmt, 1);
            int year = sqlite3_column_int(stmt, 2);
            int month = sqlite3_column_int(stmt, 3);
            int day = sqlite3_column_int(stmt, 4);
            int count = sqlite3_column_int(stmt, 5);
            int local_date;

            if(name == NULL || name[0] == '\0')
                continue;
            /* Tickmate stores Android Calendar.MONTH values: January is 0. */
            if(year <= 0 || month < 0 || month > 11 || day <= 0 || day > 31)
                continue;
            local_date = year * 10000 + (month + 1) * 100 + day;
            snprintf(import_habit_id, sizeof(import_habit_id), "tickmate-%d", track_id);
            if(!resolve_import_habit_id(import_habit_id, name, local_habit_id,
                                        sizeof(local_habit_id)))
                continue;
            sqlite3_reset(write_stmt);
            sqlite3_clear_bindings(write_stmt);
            bind_text(write_stmt, 1, local_habit_id);
            sqlite3_bind_int(write_stmt, 2, local_date);
            sqlite3_bind_int(write_stmt, 3, count > 0 ? count : 1);
            sqlite3_bind_int64(write_stmt, 4, imported_at);
            if(sqlite3_step(write_stmt) == SQLITE_DONE)
                ok = 1;
        }
    }
finish:
    sqlite3_finalize(stmt);
    sqlite3_finalize(write_stmt);
    exec_sql("COMMIT");
    return ok;
}

static int
import_settings_from_source(sqlite3 *src)
{
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt *write_stmt = NULL;
    int imported = 0;
    long long imported_at = now_seconds();

    if(src == NULL || g_storage.db == NULL || !source_table_exists(src, "settings"))
        return 0;
    if(sqlite3_prepare_v2(src, "SELECT key,value FROM settings", -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    exec_sql("BEGIN IMMEDIATE");
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *key = (const char *)sqlite3_column_text(stmt, 0);
        const char *value = (const char *)sqlite3_column_text(stmt, 1);

        if(!setting_key_importable(key))
            continue;
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT INTO settings(user_id,key,value,updated_at) VALUES(?1,?2,?3,?4) "
                              "ON CONFLICT(user_id,key) DO UPDATE SET "
                              "value=CASE WHEN excluded.value!='' OR settings.value='' THEN excluded.value ELSE settings.value END,"
                              "updated_at=CASE WHEN excluded.value!='' OR settings.value='' THEN excluded.updated_at ELSE settings.updated_at END",
                              -1, &write_stmt, NULL) != SQLITE_OK)
            continue;
        bind_text(write_stmt, 1, g_storage.user_id);
        bind_text(write_stmt, 2, key);
        bind_text(write_stmt, 3, value != NULL ? value : "");
        sqlite3_bind_int64(write_stmt, 4, imported_at);
        if(sqlite3_step(write_stmt) == SQLITE_DONE)
            imported++;
        sqlite3_finalize(write_stmt);
        write_stmt = NULL;
    }
    sqlite3_finalize(stmt);
    sqlite3_finalize(write_stmt);
    exec_sql("COMMIT");
    return imported;
}

static int
inspect_sqlite_db_file(const char *db_path, InbeStorageImportInfo *info)
{
    sqlite3 *src = NULL;
    int ok = 0;

    if(info != NULL)
        memset(info, 0, sizeof(*info));
    if(db_path == NULL || db_path[0] == '\0' || info == NULL)
        return 0;
    if(sqlite3_open(db_path, &src) != SQLITE_OK)
        goto done;

    if(source_table_exists(src, "sessions") && source_table_exists(src, "session_rounds")) {
        info->session_count = source_count_rows(src, "SELECT COUNT(*) FROM sessions WHERE deleted_at=0");
        info->habit_count = source_table_exists(src, "habits")
                                ? source_count_rows(src, "SELECT COUNT(*) FROM habits WHERE deleted_at=0")
                                : 0;
        info->setting_count = source_count_importable_settings(src);
        info->has_sessions = info->session_count > 0;
        info->has_habits = info->habit_count > 0;
        info->has_settings = info->setting_count > 0;
        info->valid = info->has_sessions || info->has_habits || info->has_settings;
        ok = info->valid;
        goto done;
    }

    if(source_table_has_column(src, "tracks", "name") &&
       source_table_has_column(src, "ticks", "_track_id")) {
        info->habit_count = source_count_rows(src, "SELECT COUNT(*) FROM tracks WHERE enabled!=0");
        info->has_habits = info->habit_count > 0;
        info->valid = info->has_habits;
        ok = info->valid;
    }

done:
    if(src != NULL)
        sqlite3_close(src);
    return ok;
}

static int
import_sqlite_db_file(const char *db_path, InbeStorageImportMode mode)
{
    sqlite3 *src = NULL;
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt *hstmt = NULL;
    int ok = 0;
    int imported_settings = 0;
    int deferred_materialize = 0;

    if(db_path == NULL || db_path[0] == '\0')
        return 0;
    if(sqlite3_open(db_path, &src) != SQLITE_OK) {
        TraceLog(LOG_WARNING, "DATA: sqlite import could not open %s", db_path);
        goto done;
    }
    g_storage.materialize_defer++;
    deferred_materialize = 1;
    if(sqlite3_prepare_v2(src,
                          "SELECT id,started_at,local_date,topic,activity,source FROM sessions WHERE deleted_at=0",
                          -1, &stmt, NULL) != SQLITE_OK) {
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

    if(source_table_exists(src, "habits")) {
        int has_sync_activity = source_table_has_column(src, "habits", "sync_activity");
        int has_counter_enabled = source_table_has_column(src, "habits", "counter_enabled");
        int has_day_count = source_table_has_column(src, "habit_days", "count");
        const char *habit_sql =
            has_sync_activity && has_counter_enabled
                ? "SELECT id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_enabled,sort_order FROM habits WHERE deleted_at=0 ORDER BY sort_order,id"
                : has_sync_activity
                    ? "SELECT id,name,color_r,color_g,color_b,sync_mode,sync_activity,0,sort_order FROM habits WHERE deleted_at=0 ORDER BY sort_order,id"
                    : has_counter_enabled
                        ? "SELECT id,name,color_r,color_g,color_b,sync_mode,0,counter_enabled,sort_order FROM habits WHERE deleted_at=0 ORDER BY sort_order,id"
                        : "SELECT id,name,color_r,color_g,color_b,sync_mode,0,0,sort_order FROM habits WHERE deleted_at=0 ORDER BY sort_order,id";
        if(sqlite3_prepare_v2(src, habit_sql, -1, &stmt, NULL) != SQLITE_OK)
            goto after_habits;
        exec_sql("BEGIN IMMEDIATE");
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            const char *import_habit_id = (const char *)sqlite3_column_text(stmt, 0);
            const char *name = (const char *)sqlite3_column_text(stmt, 1);
            char local_habit_id[INBE_STORAGE_ID_SIZE];
            int sync_activity = sqlite3_column_int(stmt, 6);
            int counter_enabled = sqlite3_column_int(stmt, 7) != 0;
            int sort_order = sqlite3_column_int(stmt, 8);
            if(sync_activity != 0)
                counter_enabled = 1;

            if(import_habit_id == NULL || import_habit_id[0] == '\0' ||
               name == NULL || name[0] == '\0')
                continue;
            if(!resolve_import_habit_id(import_habit_id, name, local_habit_id,
                                        sizeof(local_habit_id)))
                continue;
            if(sqlite3_prepare_v2(g_storage.db,
                                  "INSERT OR REPLACE INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_enabled,sort_order,deleted_at,updated_at) "
                                  "VALUES(?1,?2,COALESCE((SELECT name FROM habits WHERE id=?1),?3),?4,?5,?6,?7,?8,"
                                  "CASE WHEN ?9!=0 THEN 1 ELSE COALESCE((SELECT counter_enabled FROM habits WHERE id=?1),0) END,?10,0,?11)",
                                  -1, &hstmt, NULL) != SQLITE_OK)
                continue;
            bind_text(hstmt, 1, local_habit_id);
            bind_text(hstmt, 2, g_storage.user_id);
            bind_text(hstmt, 3, name);
            sqlite3_bind_int(hstmt, 4, sqlite3_column_int(stmt, 2));
            sqlite3_bind_int(hstmt, 5, sqlite3_column_int(stmt, 3));
            sqlite3_bind_int(hstmt, 6, sqlite3_column_int(stmt, 4));
            sqlite3_bind_int(hstmt, 7, sqlite3_column_int(stmt, 5));
            sqlite3_bind_int(hstmt, 8, sync_activity);
            sqlite3_bind_int(hstmt, 9, counter_enabled ? 1 : 0);
            sqlite3_bind_int(hstmt, 10, sort_order);
            sqlite3_bind_int64(hstmt, 11, now_seconds());
            if(sqlite3_step(hstmt) == SQLITE_DONE)
                ok = 1;
            sqlite3_finalize(hstmt);
            hstmt = NULL;

            if(sqlite3_prepare_v2(src,
                                  has_day_count
                                      ? "SELECT local_date,completed,count FROM habit_days WHERE habit_id=?1"
                                      : "SELECT local_date,completed,CASE WHEN completed!=0 THEN 1 ELSE 0 END FROM habit_days WHERE habit_id=?1",
                                  -1, &hstmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(hstmt, 1, import_habit_id, -1, SQLITE_TRANSIENT);
                while(sqlite3_step(hstmt) == SQLITE_ROW) {
                    sqlite3_stmt *day_stmt = NULL;
                    if(sqlite3_prepare_v2(g_storage.db,
                                          "INSERT INTO habit_days(habit_id,local_date,completed,count,updated_at) "
                                          "VALUES(?1,?2,?3,?4,?5) "
                                          "ON CONFLICT(habit_id,local_date) DO UPDATE SET "
                                          "count=CASE WHEN habit_days.count>excluded.count THEN habit_days.count ELSE excluded.count END,"
                                          "completed=CASE WHEN habit_days.count>0 OR excluded.count>0 OR habit_days.completed!=0 OR excluded.completed!=0 THEN 1 ELSE 0 END,"
                                          "updated_at=CASE WHEN excluded.count>habit_days.count THEN excluded.updated_at ELSE habit_days.updated_at END",
                                          -1, &day_stmt, NULL) != SQLITE_OK)
                        continue;
                    bind_text(day_stmt, 1, local_habit_id);
                    sqlite3_bind_int(day_stmt, 2, sqlite3_column_int(hstmt, 0));
                    sqlite3_bind_int(day_stmt, 3, sqlite3_column_int(hstmt, 1) != 0);
                    sqlite3_bind_int(day_stmt, 4, sqlite3_column_int(hstmt, 2));
                    sqlite3_bind_int64(day_stmt, 5, now_seconds());
                    sqlite3_step(day_stmt);
                    sqlite3_finalize(day_stmt);
                }
            }
            sqlite3_finalize(hstmt);
            hstmt = NULL;
        }
        exec_sql("COMMIT");
    }
after_habits:

    if(mode == INBE_STORAGE_IMPORT_DATA_AND_SETTINGS)
        imported_settings = import_settings_from_source(src);
    if(imported_settings > 0)
        ok = 1;

    goto done;

try_tickmate:
    sqlite3_finalize(stmt);
    stmt = NULL;
    ok = import_tickmate_db(src);
    if(!ok)
        TraceLog(LOG_WARNING, "DATA: sqlite import was neither Inbe nor supported Tickmate schema");

done:
    if(deferred_materialize && g_storage.materialize_defer > 0) {
        g_storage.materialize_defer--;
        if(g_storage.materialize_defer == 0 && g_storage.materialize_needed) {
            g_storage.materialize_needed = 0;
            storage_materialize_session_habit_days();
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_finalize(hstmt);
    if(src != NULL)
        sqlite3_close(src);
    if(ok) {
        storage_enqueue_all_sync_state();
        storage_schedule_persist();
    }
    return ok;
}

int
storage_import_zip(const char *path)
{
    return storage_import_zip_ex(path, INBE_STORAGE_IMPORT_DATA_ONLY);
}

int
storage_import_zip_ex(const char *path, InbeStorageImportMode mode)
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
    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_reader_init_file(&archive, path, 0)) {
        return import_sqlite_db_file(path, mode);
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
            ok = import_sqlite_db_file(temp_path, mode);
            remove(temp_path);
            if(!ok)
                TraceLog(LOG_ERROR, "DATA: archive contained inbe-data/inbe.db but sqlite import failed");
        } else {
            TraceLog(LOG_ERROR, "DATA: failed to extract inbe-data/inbe.db from archive");
        }
        if(fp != NULL)
            fclose(fp);
        free(db_bytes);
    } else {
        ok = import_legacy_session_zip(&archive);
    }
    mz_zip_reader_end(&archive);
    if(!ok)
        TraceLog(LOG_ERROR, "DATA: import failed for %s", path);
    else
        storage_enqueue_all_sync_state();
    return ok;
}

int
storage_inspect_import(const char *path, InbeStorageImportInfo *info)
{
    mz_zip_archive archive;
    int ok = 0;

    if(info != NULL)
        memset(info, 0, sizeof(*info));
    if(path == NULL || path[0] == '\0' || info == NULL || !path_exists(path))
        return 0;

    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_reader_init_file(&archive, path, 0))
        return inspect_sqlite_db_file(path, info);

    if(mz_zip_reader_locate_file(&archive, "inbe-data/inbe.db", NULL, 0) >= 0) {
        char *db_bytes;
        size_t db_size = 0;
        char temp_path[INBE_STORAGE_PATH_SIZE];
        FILE *fp;
        db_bytes = mz_zip_reader_extract_file_to_heap(&archive, "inbe-data/inbe.db", &db_size, 0);
        snprintf(temp_path, sizeof(temp_path), "%s/import-inspect-inbe.db", g_storage.root);
        fp = fopen(temp_path, "wb");
        if(db_bytes != NULL && fp != NULL && fwrite(db_bytes, 1, db_size, fp) == db_size) {
            fclose(fp);
            fp = NULL;
            ok = inspect_sqlite_db_file(temp_path, info);
            remove(temp_path);
        }
        if(fp != NULL)
            fclose(fp);
        free(db_bytes);
    } else {
        mz_uint file_count = mz_zip_reader_get_num_files(&archive);
        for(mz_uint i = 0; i < file_count; i++) {
            mz_zip_archive_file_stat stat;
            int year;
            int month;
            int day;
            int hour;
            int minute;
            int second;

            if(!mz_zip_reader_file_stat(&archive, i, &stat) || stat.m_is_directory)
                continue;
            if(parse_legacy_session_filename(stat.m_filename, &year, &month, &day,
                                             &hour, &minute, &second))
                info->session_count++;
        }
        info->has_sessions = info->session_count > 0;
        info->valid = info->has_sessions;
        ok = info->valid;
    }
    mz_zip_reader_end(&archive);
    return ok;
}

int
storage_init(const char *root)
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
    sqlite3_busy_timeout(g_storage.db, 1000);
    if(!schema_create() || !migrate_schema() || !load_or_create_user())
        return 0;
    storage_materialize_session_habit_days();
    migrate_legacy_file_sessions_once();
    return 1;
}

void
storage_close(void)
{
    if(g_storage.db != NULL) {
        sqlite3_close(g_storage.db);
        g_storage.db = NULL;
    }
}

const char *
storage_db_path(void)
{
    return g_storage.db_path;
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
