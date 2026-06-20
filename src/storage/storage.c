#include "storage.h"

#include "db.h"
#include "screens/habits_screen.h"
#include "breath_engine.h"

#include "raylib.h"
#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

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

void
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

void
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

int
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

int
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
storage_save_session_at_for_activity(int local_date, int hour, int minute, int second,
                                     const int *round_times, int round_count,
                                     int topic, int activity,
                                     char *out_id, size_t out_id_size)
{
    struct tm tm;
    time_t started_at;
    int saved[MaxRounds];
    int saved_count = 0;

    if(local_date <= 0)
        return 0;
    for(int i = 0; i < round_count && i < MaxRounds; i++) {
        if(round_times[i] > 0)
            saved[saved_count++] = round_times[i];
    }
    if(saved_count <= 0)
        return 0;

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = local_date / 10000 - 1900;
    tm.tm_mon = (local_date / 100) % 100 - 1;
    tm.tm_mday = local_date % 100;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    tm.tm_isdst = -1;
    started_at = mktime(&tm);
    if(started_at == (time_t)-1)
        return 0;

    return insert_session_at_ex((long long)started_at, local_date, saved, saved_count,
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
                         sqlite3_column_int(stmt, 7) != 0 ? 1 : 0,
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
        "       CAST(COALESCE(json_extract(value,'$.counter_enabled'),"
        "                     (SELECT counter_enabled FROM habits WHERE id=COALESCE(json_extract(value,'$.id'),'')),"
        "                     0) AS INTEGER),"
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
        "WHERE excluded.updated_at > habits.updated_at "
        "OR (excluded.updated_at = habits.updated_at AND NOT EXISTS ("
        " SELECT 1 FROM sync_outbox "
        " WHERE entity_type='habit' AND entity_id=habits.id AND local_date=0"
        "))";
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
    g_storage.pending_sync_outbox_seq = 0;
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
        habit->counter_enabled = sqlite3_column_int(stmt, 7) != 0;
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
                              "updated_at=excluded.updated_at "
                              "WHERE habits.user_id<>excluded.user_id OR habits.name<>excluded.name OR "
                              "habits.color_r<>excluded.color_r OR habits.color_g<>excluded.color_g OR habits.color_b<>excluded.color_b OR "
                              "habits.sync_mode<>excluded.sync_mode OR habits.sync_activity<>excluded.sync_activity OR "
                              "habits.counter_enabled<>excluded.counter_enabled OR habits.sort_order<>excluded.sort_order OR "
                              "habits.deleted_at<>0",
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
        if(sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(g_storage.db) > 0)
            storage_enqueue_sync_habit(habit->id);
        sqlite3_finalize(stmt);
        stmt = NULL;
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

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
