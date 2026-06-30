#include "storage.h"

#include "db.h"
#include "screens/habits_screen.h"

#include "raylib.h"
#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

static long long
storage_max_sync_outbox_seq(void);
static int
storage_has_orphan_habit_days(void);
static int
storage_json_array_count(const char *json, const char *path);
static int
storage_apply_remote_full_snapshot(const char *response_json);
static int
storage_clear_local_sync_data(void);
static int
storage_json_valid(const char *json);

#define STORAGE_SYNC_BACKFILL_KEY "sync_backfill_v2_done"
#define STORAGE_SYNC_HABIT_NAME_REPAIR_KEY "sync_habit_name_repair_v1_done"
#define STORAGE_SYNC_PUBLIC_ID_KEY "sync_public_id"
#define STORAGE_SYNC_PUBLIC_KEY_KEY "sync_public_key"
#define STORAGE_SYNC_PRIVATE_KEY_KEY "sync_private_key"
#define STORAGE_SYNC_LAST_SERVER_HASH_KEY "sync_last_server_state_hash"
#define STORAGE_SYNC_SERVER_CLOCK_KEY "sync_server_clock"
#define STORAGE_SYNC_PENDING_REVIEW_KEY "sync_pending_review_pending"
#define STORAGE_SYNC_APPLY_REVIEW_KEY "sync_apply_pending_review"
#define STORAGE_SYNC_FULL_REPLACE_KEY "sync_full_replace_requested"
#define STORAGE_SYNC_ACCOUNT_ALIAS_KEY "sync_account_alias"
#define STORAGE_SYNC_ZERO_HABIT_DAY_REPAIR_KEY "sync_zero_habit_day_repair_v1_done"

static long long
storage_next_change_time(void)
{
    sqlite3_stmt *stmt = NULL;
    long long now = now_seconds();
    long long latest = 0;

    if(g_storage.db == NULL)
        return now;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT MAX(updated_at) FROM ("
                          " SELECT COALESCE(MAX(updated_at),0) AS updated_at FROM habits"
                          " UNION ALL SELECT COALESCE(MAX(updated_at),0) FROM habit_days"
                          " UNION ALL SELECT COALESCE(MAX(updated_at),0) FROM sessions"
                          " UNION ALL SELECT COALESCE(MAX(updated_at),0) FROM social_cache"
                          ")",
                          -1, &stmt, NULL) == SQLITE_OK) {
        if(sqlite3_step(stmt) == SQLITE_ROW)
            latest = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return latest >= now ? latest + 1 : now;
}

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
    snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
             bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
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
    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
                          "VALUES(?1,?2,?3,?4) "
                          "ON CONFLICT(entity_type,entity_id,local_date) DO UPDATE SET "
                          "queued_at=excluded.queued_at",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, entity_type);
    bind_text(stmt, 2, entity_id);
    sqlite3_bind_int(stmt, 3, local_date);
    sqlite3_bind_int64(stmt, 4, now_seconds());
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

int
storage_enqueue_sync_habit(const char *habit_id)
{
    return storage_enqueue_sync_entity("habit", habit_id, 0);
}

int
storage_enqueue_sync_habit_day(const char *habit_id, int local_date)
{
    return storage_enqueue_sync_entity("habit_day", habit_id, local_date);
}

int
storage_enqueue_sync_session(const char *session_id)
{
    return storage_enqueue_sync_entity("session", session_id, 0);
}

void
storage_enqueue_all_sync_state(void)
{
    int owns_transaction;

    if(g_storage.db == NULL)
        return;
    owns_transaction = sqlite3_get_autocommit(g_storage.db) != 0;
    if(owns_transaction && !exec_sql("BEGIN IMMEDIATE"))
        return;
    exec_sql("INSERT INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
             "SELECT 'habit',id,0,strftime('%s','now') FROM habits "
             "WHERE user_id=(SELECT id FROM users LIMIT 1) "
             "ON CONFLICT(entity_type,entity_id,local_date) DO UPDATE SET "
             "queued_at=excluded.queued_at");
    exec_sql("INSERT INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
             "SELECT 'habit_day',hd.habit_id,hd.local_date,strftime('%s','now') "
             "FROM habit_days hd JOIN habits h ON h.id=hd.habit_id "
             "WHERE h.user_id=(SELECT id FROM users LIMIT 1) "
             "ON CONFLICT(entity_type,entity_id,local_date) DO UPDATE SET "
             "queued_at=excluded.queued_at");
    exec_sql("INSERT INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
             "SELECT 'session',id,0,strftime('%s','now') FROM sessions "
             "WHERE user_id=(SELECT id FROM users LIMIT 1) "
             "ON CONFLICT(entity_type,entity_id,local_date) DO UPDATE SET "
             "queued_at=excluded.queued_at");
    if(owns_transaction)
        exec_sql("COMMIT");
}

void
storage_reset_sync_state(void)
{
    if(g_storage.db == NULL)
        return;
    set_meta_int64("sync_last_server_version", 0);
    set_meta_int64("sync_last_upload_at", 0);
    set_meta_int64("sync_full_upload_done", 0);
    set_meta(STORAGE_SYNC_LAST_SERVER_HASH_KEY, "");
    set_meta(STORAGE_SYNC_PENDING_REVIEW_KEY, "");
    set_meta_int64(STORAGE_SYNC_FULL_REPLACE_KEY, 0);
    storage_sync_review_delete_json();
    set_meta_int64(STORAGE_SYNC_BACKFILL_KEY, 0);
    set_meta_int64(STORAGE_SYNC_HABIT_NAME_REPAIR_KEY, 0);
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
        if(sqlite3_prepare_v2(g_storage.db, "SELECT value FROM meta WHERE key='sync_client_id'", -1,
                              &stmt, NULL) == SQLITE_OK &&
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
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COUNT(*) FROM settings WHERE user_id=?1", -1, &stmt,
                          NULL) != SQLITE_OK)
        return 1;
    bind_text(stmt, 1, g_storage.user_id);
    if(sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count == 0;
}

int
storage_get_social_cache_json(const char *kind, char *out, size_t out_size)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(g_storage.db == NULL || kind == NULL || kind[0] == '\0')
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT json FROM social_cache WHERE user_id=?1 AND kind=?2",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, kind);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *json = (const char *)sqlite3_column_text(stmt, 0);
        snprintf(out, out_size, "%s", json != NULL ? json : "");
        found = out[0] != '\0';
    }
    sqlite3_finalize(stmt);
    return found;
}

int
storage_set_social_cache_json(const char *kind, const char *json)
{
    sqlite3_stmt *stmt = NULL;
    long long updated_at;
    int rc;
    int same = 0;

    if(g_storage.db == NULL || kind == NULL || kind[0] == '\0' ||
       json == NULL || json[0] == '\0' || !storage_json_valid(json))
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT json=?3 FROM social_cache WHERE user_id=?1 AND kind=?2",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, g_storage.user_id);
        bind_text(stmt, 2, kind);
        bind_text(stmt, 3, json);
        if(sqlite3_step(stmt) == SQLITE_ROW)
            same = sqlite3_column_int(stmt, 0) != 0;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;
    if(same)
        return 1;
    updated_at = storage_next_change_time();
    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT INTO social_cache(user_id,kind,json,updated_at) "
                          "VALUES(?1,?2,?3,?4) "
                          "ON CONFLICT(user_id,kind) DO UPDATE SET "
                          "json=excluded.json,updated_at=excluded.updated_at",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, kind);
    bind_text(stmt, 3, json);
    sqlite3_bind_int64(stmt, 4, updated_at);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if(rc != SQLITE_DONE)
        return 0;
    storage_schedule_persist();
    return 2;
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
    if(sqlite3_prepare_v2(g_storage.db, "SELECT value FROM settings WHERE user_id=?1 AND key=?2",
                          -1, &stmt, NULL) != SQLITE_OK)
        return NULL;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, key);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *text = (const char *)sqlite3_column_text(stmt, 0);
        snprintf(g_storage.text_value, sizeof(g_storage.text_value), "%s",
                 text != NULL ? text : "");
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
                          "INSERT INTO settings(user_id,key,value,updated_at) "
                          "VALUES(?1,?2,?3,?4) "
                          "ON CONFLICT(user_id,key) DO UPDATE SET "
                          "value=excluded.value,updated_at=excluded.updated_at",
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

    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT INTO "
                          "inbe_session_habit_counts(habit_id,local_date,session_count) "
                          "SELECT h.id,s.local_date,COUNT(*) "
                          "FROM habits h JOIN sessions s ON s.user_id=h.user_id "
                          "WHERE h.user_id=?1 AND h.deleted_at=0 AND s.deleted_at=0 "
                          "  AND h.sync_mode=?2 AND h.sync_activity<>0 "
                          "  AND s.local_date>0 AND s.activity>=0 AND s.activity<30 "
                          "  AND (h.sync_activity & (1 << s.activity))<>0 "
                          "GROUP BY h.id,s.local_date",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    sqlite3_bind_int(stmt, 2, INBE_HABIT_SYNC_ACTIVITIES);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

static int
storage_insert_habit_day_count(const char *habit_id, int local_date, int count,
                               long long updated_at)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT INTO "
                          "habit_days(habit_id,local_date,completed,count,"
                          "session_count,updated_at) "
                          "VALUES(?1,?2,?3,?4,?4,?5)",
                          -1, &stmt, NULL) != SQLITE_OK)
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
storage_update_habit_day_count(const char *habit_id, int local_date, int completed, int count,
                               int session_count, long long updated_at)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE habit_days "
                          "SET completed=?3,count=?4,session_count=?5,updated_at=?6 "
                          "WHERE habit_id=?1 AND local_date=?2",
                          -1, &stmt, NULL) != SQLITE_OK)
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
storage_update_habit_day_session_count(const char *habit_id, int local_date, int session_count)
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
storage_apply_session_habit_count(const char *habit_id, int local_date, int session_count,
                                  long long changed_at)
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

    if(old_count <= 0 && !old_completed && old_session_count > 0) {
        if(session_count == old_session_count)
            return 1;
        return storage_update_habit_day_session_count(habit_id, local_date, session_count);
    }

    next_count = old_count;
    if(old_count <= old_session_count || session_count > old_count)
        next_count = session_count;
    next_completed = next_count > 0 ? 1 : 0;
    if(next_completed == old_completed && next_count == old_count &&
       session_count == old_session_count)
        return 1;
    if(next_completed == old_completed && next_count == old_count)
        return storage_update_habit_day_session_count(habit_id, local_date, session_count);
    return storage_update_habit_day_count(habit_id, local_date, next_completed, next_count,
                                          session_count, changed_at);
}

static int
storage_clear_stale_session_habit_counts(long long changed_at)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 1;

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT hd.habit_id,hd.local_date,hd.count,hd.session_count "
                          "FROM habit_days hd JOIN habits h ON h.id=hd.habit_id "
                          "WHERE h.user_id=?1 AND h.deleted_at=0 AND h.sync_mode=?2 AND "
                          "h.sync_activity<>0 "
                          "  AND hd.session_count>0 "
                          "  AND NOT EXISTS (SELECT 1 FROM inbe_session_habit_counts c "
                          "                  WHERE c.habit_id=hd.habit_id AND "
                          "c.local_date=hd.local_date)",
                          -1, &stmt, NULL) != SQLITE_OK)
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
        ok =
            storage_apply_session_habit_count(habit_id_copy, local_date, session_count, changed_at);
        if(!ok)
            break;
    }
    sqlite3_finalize(stmt);
    return ok && storage_clear_stale_session_habit_counts(changed_at);
}

static int
storage_snapshot_habit_day_visible_state(void)
{
    return exec_sql("DROP TABLE IF EXISTS temp.inbe_habit_day_visible_before;"
                    "CREATE TEMP TABLE inbe_habit_day_visible_before("
                    " habit_id TEXT NOT NULL,"
                    " local_date INTEGER NOT NULL,"
                    " completed INTEGER NOT NULL,"
                    " count INTEGER NOT NULL,"
                    " PRIMARY KEY(habit_id,local_date)"
                    ");"
                    "INSERT INTO inbe_habit_day_visible_before("
                    "habit_id,local_date,completed,count) "
                    "SELECT hd.habit_id,hd.local_date,hd.completed,hd.count "
                    "FROM habit_days hd JOIN habits h ON h.id=hd.habit_id "
                    "WHERE h.user_id=(SELECT id FROM users LIMIT 1)");
}

static int
storage_enqueue_materialized_habit_day_changes(void)
{
    return exec_sql("INSERT INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
                    "SELECT 'habit_day',hd.habit_id,hd.local_date,strftime('%s','now') "
                    "FROM habit_days hd JOIN habits h ON h.id=hd.habit_id "
                    "LEFT JOIN inbe_habit_day_visible_before b "
                    "ON b.habit_id=hd.habit_id AND b.local_date=hd.local_date "
                    "WHERE h.user_id=(SELECT id FROM users LIMIT 1) "
                    "AND (b.habit_id IS NULL OR b.completed<>hd.completed OR b.count<>hd.count) "
                    "ON CONFLICT(entity_type,entity_id,local_date) DO UPDATE SET "
                    "queued_at=excluded.queued_at;"
                    "DROP TABLE IF EXISTS temp.inbe_habit_day_visible_before");
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
    ok = storage_snapshot_habit_day_visible_state() &&
         storage_build_session_habit_counts() && storage_apply_session_habit_counts(changed_at) &&
         storage_enqueue_materialized_habit_day_changes() &&
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

static void
storage_append_habits_json(JsonBuilder *json, long long through_seq)
{
    sqlite3_stmt *stmt = NULL;
    int first = 1;

    json_append(json, "\"habits\":[");
    if(g_storage.db != NULL &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT "
                          "id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_"
                          "enabled,sort_order,deleted_at,updated_at "
                          "FROM habits WHERE user_id=?1 AND id IN ("
                          " SELECT entity_id FROM sync_outbox WHERE entity_type='habit' AND "
                          "seq<=?2"
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
                         sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
                         sqlite3_column_int(stmt, 4));
            json_appendf(json,
                         ",\"sync_mode\":%d,\"sync_activity\":%d,\"counter_enabled\":"
                         "%d,\"sort_order\":%d,\"deleted_at\":%lld",
                         sqlite3_column_int(stmt, 5), sqlite3_column_int(stmt, 6),
                         sqlite3_column_int(stmt, 7) != 0 ? 1 : 0, sqlite3_column_int(stmt, 8),
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
                          "SELECT "
                          "hd.habit_id,hd.local_date,hd.completed,hd.count,hd.updated_at "
                          "FROM habit_days hd JOIN habits h ON h.id=hd.habit_id "
                          "WHERE h.user_id=?1 AND EXISTS ("
                          " SELECT 1 FROM sync_outbox o WHERE o.entity_type='habit_day' "
                          " AND o.entity_id=hd.habit_id AND o.local_date=hd.local_date AND "
                          "o.seq<=?2"
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
    if(g_storage.db != NULL && sqlite3_prepare_v2(g_storage.db,
                                                  "SELECT "
                                                  "id,started_at,local_date,topic,activity,source,"
                                                  "rounds_hash,deleted_at,updated_at "
                                                  "FROM sessions WHERE user_id=?1 AND id IN ("
                                                  " SELECT entity_id FROM sync_outbox WHERE "
                                                  "entity_type='session' AND seq<=?2"
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
                         sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
                         sqlite3_column_int(stmt, 4));
            json_append_key_string(json, "source", (const char *)sqlite3_column_text(stmt, 5));
            json_append(json, ",");
            json_append_key_string(json, "rounds_hash", (const char *)sqlite3_column_text(stmt, 6));
            json_appendf(json,
                         ",\"deleted_at\":%lld,\"updated_at\":", sqlite3_column_int64(stmt, 7));
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

static int
storage_append_habit_payload_json(JsonBuilder *json, const char *habit_id)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(g_storage.db == NULL || habit_id == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT "
                          "id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_"
                          "enabled,sort_order,deleted_at,updated_at "
                          "FROM habits WHERE user_id=?1 AND id=?2",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, habit_id);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        found = 1;
        json_append(json, "{");
        json_append_key_string(json, "id", (const char *)sqlite3_column_text(stmt, 0));
        json_append(json, ",");
        json_append_key_string(json, "name", (const char *)sqlite3_column_text(stmt, 1));
        json_appendf(json, ",\"color_r\":%d,\"color_g\":%d,\"color_b\":%d",
                     sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
                     sqlite3_column_int(stmt, 4));
        json_appendf(json,
                     ",\"sync_mode\":%d,\"sync_activity\":%d,\"counter_enabled\":%"
                     "d,\"sort_order\":%d,\"deleted_at\":%lld",
                     sqlite3_column_int(stmt, 5), sqlite3_column_int(stmt, 6),
                     sqlite3_column_int(stmt, 7) != 0 ? 1 : 0, sqlite3_column_int(stmt, 8),
                     sqlite3_column_int64(stmt, 9));
        json_append(json, ",\"updated_at\":");
        json_append_epoch(json, sqlite3_column_int64(stmt, 10));
        json_append(json, "}");
    }
    sqlite3_finalize(stmt);
    return found;
}

static int
storage_append_habit_day_payload_json(JsonBuilder *json, const char *habit_id, int local_date)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(g_storage.db == NULL || habit_id == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT "
                          "hd.habit_id,hd.local_date,hd.completed,hd.count,hd.updated_at "
                          "FROM habit_days hd JOIN habits h ON h.id=hd.habit_id "
                          "WHERE h.user_id=?1 AND hd.habit_id=?2 AND hd.local_date=?3",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, habit_id);
    sqlite3_bind_int(stmt, 3, local_date);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        found = 1;
        json_append(json, "{");
        json_append_key_string(json, "habit_id", (const char *)sqlite3_column_text(stmt, 0));
        json_appendf(json, ",\"local_date\":%d,\"completed\":%s,\"count\":%d,\"updated_at\":",
                     sqlite3_column_int(stmt, 1),
                     sqlite3_column_int(stmt, 2) != 0 ? "true" : "false",
                     sqlite3_column_int(stmt, 3));
        json_append_epoch(json, sqlite3_column_int64(stmt, 4));
        json_append(json, "}");
    }
    sqlite3_finalize(stmt);
    return found;
}

static int
storage_append_session_payload_json(JsonBuilder *json, const char *session_id)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(g_storage.db == NULL || session_id == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT "
                          "id,started_at,local_date,topic,activity,source,"
                          "rounds_hash,deleted_at,updated_at "
                          "FROM sessions WHERE user_id=?1 AND id=?2",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, session_id);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *id = (const char *)sqlite3_column_text(stmt, 0);
        long long started_at = sqlite3_column_int64(stmt, 1);
        long long updated_at = sqlite3_column_int64(stmt, 8);
        found = 1;
        json_append(json, "{");
        json_append_key_string(json, "id", id);
        json_append(json, ",\"started_at\":");
        json_append_epoch(json, started_at);
        json_appendf(json, ",\"local_date\":%d,\"topic\":\"%d\",\"activity\":%d,",
                     sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
                     sqlite3_column_int(stmt, 4));
        json_append_key_string(json, "source", (const char *)sqlite3_column_text(stmt, 5));
        json_append(json, ",");
        json_append_key_string(json, "rounds_hash", (const char *)sqlite3_column_text(stmt, 6));
        json_appendf(json, ",\"deleted_at\":%lld,\"updated_at\":", sqlite3_column_int64(stmt, 7));
        json_append_epoch(json, updated_at > 0 ? updated_at : started_at);
        json_append(json, ",");
        storage_append_session_rounds_json(json, id);
        json_append(json, "}");
    }
    sqlite3_finalize(stmt);
    return found;
}

static int
storage_sync_op_is_delete(const char *entity_type, const char *entity_id, int local_date)
{
    sqlite3_stmt *stmt = NULL;
    int deleted = 0;

    if(g_storage.db == NULL || entity_type == NULL || entity_id == NULL)
        return 1;
    if(strcmp(entity_type, "habit") == 0) {
        if(sqlite3_prepare_v2(g_storage.db,
                              "SELECT deleted_at FROM habits WHERE user_id=?1 AND id=?2", -1, &stmt,
                              NULL) != SQLITE_OK)
            return 0;
        bind_text(stmt, 1, g_storage.user_id);
        bind_text(stmt, 2, entity_id);
        if(sqlite3_step(stmt) == SQLITE_ROW)
            deleted = sqlite3_column_int64(stmt, 0) > 0;
        else
            deleted = 1;
    } else if(strcmp(entity_type, "session") == 0) {
        if(sqlite3_prepare_v2(g_storage.db,
                              "SELECT deleted_at FROM sessions WHERE user_id=?1 AND id=?2", -1,
                              &stmt, NULL) != SQLITE_OK)
            return 0;
        bind_text(stmt, 1, g_storage.user_id);
        bind_text(stmt, 2, entity_id);
        if(sqlite3_step(stmt) == SQLITE_ROW)
            deleted = sqlite3_column_int64(stmt, 0) > 0;
        else
            deleted = 1;
    } else if(strcmp(entity_type, "habit_day") == 0) {
        if(sqlite3_prepare_v2(g_storage.db,
                              "SELECT completed,count FROM habit_days WHERE "
                              "habit_id=?1 AND local_date=?2",
                              -1, &stmt, NULL) != SQLITE_OK)
            return 0;
        bind_text(stmt, 1, entity_id);
        sqlite3_bind_int(stmt, 2, local_date);
        if(sqlite3_step(stmt) != SQLITE_ROW)
            deleted = 1;
    }
    sqlite3_finalize(stmt);
    return deleted;
}

static void
storage_append_sync_op_payload(JsonBuilder *json, const char *entity_type, const char *entity_id,
                               int local_date)
{
    int found = 0;

    if(strcmp(entity_type, "habit") == 0)
        found = storage_append_habit_payload_json(json, entity_id);
    else if(strcmp(entity_type, "habit_day") == 0)
        found = storage_append_habit_day_payload_json(json, entity_id, local_date);
    else if(strcmp(entity_type, "session") == 0)
        found = storage_append_session_payload_json(json, entity_id);
    if(!found)
        json_append(json, "{}");
}

static void
storage_append_sync_ops_json(JsonBuilder *json, long long through_seq)
{
    sqlite3_stmt *stmt = NULL;
    const char *client_id = storage_sync_client_id();
    int first = 1;

    json_append(json, "\"ops\":[");
    if(g_storage.db != NULL &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT seq,entity_type,entity_id,local_date,queued_at "
                          "FROM sync_outbox WHERE seq<=?1 ORDER BY seq",
                          -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, through_seq);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            long long seq = sqlite3_column_int64(stmt, 0);
            const char *entity_type = (const char *)sqlite3_column_text(stmt, 1);
            const char *entity_id = (const char *)sqlite3_column_text(stmt, 2);
            int local_date = sqlite3_column_int(stmt, 3);
            long long queued_at = sqlite3_column_int64(stmt, 4);
            int is_delete = storage_sync_op_is_delete(entity_type, entity_id, local_date);
            char op_id[160];

            if(entity_type == NULL || entity_id == NULL || entity_id[0] == '\0')
                continue;
            snprintf(op_id, sizeof(op_id), "%s:%lld", client_id, seq);
            if(!first)
                json_append(json, ",");
            first = 0;
            json_append(json, "{");
            json_append(json, "\"op_id\":");
            json_append_string(json, op_id);
            json_append(json, ",\"client_id\":");
            json_append_string(json, client_id);
            json_appendf(json, ",\"seq\":%lld", seq);
            json_append(json, ",\"entity_type\":");
            json_append_string(json, entity_type);
            json_append(json, ",\"entity_id\":");
            json_append_string(json, entity_id);
            if(local_date > 0)
                json_appendf(json, ",\"local_date\":%d", local_date);
            json_append(json, ",\"op_type\":");
            json_append_string(json, is_delete ? "delete" : "upsert");
            json_append(json, ",\"payload\":");
            storage_append_sync_op_payload(json, entity_type, entity_id, local_date);
            json_append(json, ",\"created_at\":");
            json_append_epoch(json, queued_at);
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
    int force_zero_day_repair;

    if(g_storage.db == NULL || user_id_hash == NULL || user_id_hash[0] == '\0')
        return NULL;
    if(!migrate_schema())
        return NULL;
    if(!storage_materialize_session_habit_days())
        return NULL;
    if(!get_meta_int64(STORAGE_SYNC_BACKFILL_KEY, 0))
        storage_enqueue_all_sync_state();
    since_server_version = get_meta_int64("sync_last_server_version", 0);
    full_upload_done = get_meta_int64("sync_full_upload_done", 0) != 0;
    if(!get_meta_int64(STORAGE_SYNC_HABIT_NAME_REPAIR_KEY, 0) || storage_has_orphan_habit_days())
        since_server_version = 0;
    force_zero_day_repair = get_meta_int64(STORAGE_SYNC_ZERO_HABIT_DAY_REPAIR_KEY, 0) == 0;
    if(force_zero_day_repair)
        since_server_version = 0;
    through_seq = storage_max_sync_outbox_seq();
    g_storage.pending_sync_outbox_seq = through_seq;
    json.ok = 1;
    json_append(&json, "{");
    json_append(&json, "\"protocol_version\":2,");
    json_append_key_string(&json, "user_id_hash", user_id_hash);
    json_append(&json, ",");
    json_append_key_string(&json, "client_id", storage_sync_client_id());
    json_appendf(&json, ",\"since_server_version\":%lld", since_server_version);
    json_appendf(&json, ",\"client_clock\":%lld", get_meta_int64(STORAGE_SYNC_SERVER_CLOCK_KEY, 0));
    if(get_meta_int64(STORAGE_SYNC_FULL_REPLACE_KEY, 0) != 0)
        json_append(&json, ",\"full_sync_requested\":true");
    {
        const char *last_server_hash = get_meta_text(STORAGE_SYNC_LAST_SERVER_HASH_KEY);
        if(!force_zero_day_repair && last_server_hash != NULL && last_server_hash[0] != '\0') {
            json_append(&json, ",");
            json_append_key_string(&json, "last_server_state_hash", last_server_hash);
        }
    }
    if(since_server_version <= 0 || !full_upload_done)
        json_append(&json, ",\"bootstrap\":true");
    if(public_key_hex != NULL && public_key_hex[0] != '\0') {
        json_append(&json, ",");
        json_append_key_string(&json, "public_key", public_key_hex);
    }
    json_append(&json, ",");
    storage_append_habits_json(&json, through_seq);
    json_append(&json, ",");
    storage_append_habit_days_json(&json, through_seq);
    json_append(&json, ",");
    storage_append_sessions_json(&json, through_seq);
    json_append(&json, ",");
    storage_append_sync_ops_json(&json, through_seq);
    json_append(&json, "}");

    if(!json.ok || json.data == NULL) {
        free(json.data);
        return NULL;
    }
    TraceLog(LOG_INFO,
             "SYNC: payload since=%lld bootstrap=%d outbox_through=%lld "
             "habits=%d habit_days=%d sessions=%d",
             since_server_version, since_server_version <= 0 || !full_upload_done, through_seq,
             storage_json_array_count(json.data, "$.habits"),
             storage_json_array_count(json.data, "$.habit_days"),
             storage_json_array_count(json.data, "$.sessions"));
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
    if(sqlite3_prepare_v2(g_storage.db, "SELECT CAST(COALESCE(json_extract(?1,?2),?3) AS INTEGER)",
                          -1, &stmt, NULL) != SQLITE_OK)
        return fallback;
    bind_text(stmt, 1, json);
    bind_text(stmt, 2, path);
    sqlite3_bind_int64(stmt, 3, fallback);
    if(sqlite3_step(stmt) == SQLITE_ROW)
        value = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return value;
}

static int
storage_json_extract_text(const char *json, const char *path, char *out, size_t out_size)
{
    sqlite3_stmt *stmt = NULL;
    const unsigned char *text;
    int ok = 0;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(g_storage.db == NULL || json == NULL || path == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COALESCE(json_extract(?1,?2),'')", -1, &stmt,
                          NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, json);
    bind_text(stmt, 2, path);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        text = sqlite3_column_text(stmt, 0);
        if(text != NULL) {
            snprintf(out, out_size, "%s", (const char *)text);
            ok = out[0] != '\0';
        }
    }
    sqlite3_finalize(stmt);
    return ok;
}

static long long
storage_max_sync_outbox_seq(void)
{
    sqlite3_stmt *stmt = NULL;
    long long seq = 0;

    if(g_storage.db == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COALESCE(MAX(seq),0) FROM sync_outbox", -1, &stmt,
                          NULL) != SQLITE_OK)
        return 0;
    if(sqlite3_step(stmt) == SQLITE_ROW)
        seq = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return seq;
}

static int
storage_has_orphan_habit_days(void)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(g_storage.db == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT 1 FROM habit_days hd "
                          "WHERE hd.habit_id<>'' "
                          "AND NOT EXISTS (SELECT 1 FROM habits h WHERE h.id=hd.habit_id) "
                          "LIMIT 1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

int
storage_has_sync_account(void)
{
    const char *public_id = storage_get_setting_text(STORAGE_SYNC_PUBLIC_ID_KEY);
    const char *public_key = storage_get_setting_text(STORAGE_SYNC_PUBLIC_KEY_KEY);
    const char *private_key = storage_get_setting_text(STORAGE_SYNC_PRIVATE_KEY_KEY);

    return public_id != NULL && public_id[0] != '\0' && public_key != NULL &&
           public_key[0] != '\0' && private_key != NULL && private_key[0] != '\0';
}

static void
storage_clear_uploaded_outbox(long long through_seq)
{
    sqlite3_stmt *stmt = NULL;

    if(g_storage.db == NULL || through_seq <= 0)
        return;
    if(sqlite3_prepare_v2(g_storage.db, "DELETE FROM sync_outbox WHERE seq<=?1", -1, &stmt, NULL) !=
       SQLITE_OK)
        return;
    sqlite3_bind_int64(stmt, 1, through_seq);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static int
storage_json_array_has_items(const char *json, const char *path)
{
    return storage_json_array_count(json, path) > 0;
}

static int
storage_json_array_count(const char *json, const char *path)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;

    if(g_storage.db == NULL || json == NULL || path == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COALESCE(json_array_length(json_extract(?1,?2)),0)",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, json);
    bind_text(stmt, 2, path);
    if(sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

static int
storage_sync_response_has_changes(const char *json)
{
    return storage_json_array_has_items(json, "$.changes.habits") ||
           storage_json_array_has_items(json, "$.changes.habit_days") ||
           storage_json_array_has_items(json, "$.changes.sessions") ||
           storage_json_array_has_items(json, "$.changes.meditation_logs") ||
           storage_json_array_has_items(json, "$.changes.social_cache");
}

static int
storage_exec_json_user_sql(const char *sql, const char *json)
{
    sqlite3_stmt *stmt = NULL;
    int rc;
    int attempt;

    if(g_storage.db == NULL || sql == NULL || json == NULL)
        return 0;
    for(attempt = 0; attempt < 2; attempt++) {
        if(sqlite3_prepare_v2(g_storage.db, sql, -1, &stmt, NULL) == SQLITE_OK)
            break;
        if(attempt == 0 && migrate_schema())
            continue;
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

static int
storage_ascii_equal_ci(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;

    if(a == NULL || b == NULL)
        return 0;
    while(*a != '\0' && *b != '\0') {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if(ca >= 'A' && ca <= 'Z')
            ca = (unsigned char)(ca - 'A' + 'a');
        if(cb >= 'A' && cb <= 'Z')
            cb = (unsigned char)(cb - 'A' + 'a');
        if(ca != cb)
            return 0;
    }
    return *a == '\0' && *b == '\0';
}

static int
storage_merge_habit_into(const char *keeper_id, const char *duplicate_id)
{
    sqlite3_stmt *stmt = NULL;
    static const char *const sqls[] = {
        "UPDATE habit_days "
        "SET completed=MAX(completed,COALESCE((SELECT d.completed FROM "
        "habit_days d "
        "    WHERE d.habit_id=?2 AND d.local_date=habit_days.local_date),0)),"
        " count=MAX(count,COALESCE((SELECT d.count FROM habit_days d "
        "    WHERE d.habit_id=?2 AND d.local_date=habit_days.local_date),0)),"
        " session_count=MAX(session_count,COALESCE((SELECT d.session_count FROM "
        "habit_days d "
        "    WHERE d.habit_id=?2 AND d.local_date=habit_days.local_date),0)),"
        " updated_at=MAX(updated_at,COALESCE((SELECT d.updated_at FROM "
        "habit_days d "
        "    WHERE d.habit_id=?2 AND d.local_date=habit_days.local_date),0)) "
        "WHERE habit_id=?1 AND EXISTS (SELECT 1 FROM habit_days d "
        "    WHERE d.habit_id=?2 AND d.local_date=habit_days.local_date)",
        "INSERT INTO "
        "habit_days(habit_id,local_date,completed,count,session_count,updated_at)"
        " "
        "SELECT ?1,d.local_date,d.completed,d.count,d.session_count,d.updated_at "
        "FROM habit_days d WHERE d.habit_id=?2 "
        "AND NOT EXISTS (SELECT 1 FROM habit_days k "
        "    WHERE k.habit_id=?1 AND k.local_date=d.local_date)",
        "DELETE FROM habit_days WHERE habit_id=?2", "DELETE FROM habits WHERE id=?2"};
    int ok = 1;

    if(keeper_id == NULL || duplicate_id == NULL || keeper_id[0] == '\0' ||
       duplicate_id[0] == '\0' || strcmp(keeper_id, duplicate_id) == 0)
        return 1;

    for(size_t i = 0; i < sizeof(sqls) / sizeof(sqls[0]); i++) {
        if(sqlite3_prepare_v2(g_storage.db, sqls[i], -1, &stmt, NULL) != SQLITE_OK)
            return 0;
        bind_text(stmt, 1, keeper_id);
        bind_text(stmt, 2, duplicate_id);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        stmt = NULL;
        if(!ok)
            return 0;
    }
    return ok;
}

static int
storage_merge_duplicate_habit_names(void)
{
    sqlite3_stmt *stmt = NULL;
    struct {
        char id[INBE_STORAGE_ID_SIZE];
        char name[INBE_HABIT_NAME_SIZE];
        int merged;
    } rows[64];
    int count = 0;
    int ok = 1;

    if(g_storage.db == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id,name FROM habits WHERE user_id=?1 AND deleted_at=0 "
                          "ORDER BY sort_order,id",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    while(count < (int)(sizeof(rows) / sizeof(rows[0])) && sqlite3_step(stmt) == SQLITE_ROW) {
        const char *id = (const char *)sqlite3_column_text(stmt, 0);
        const char *name = (const char *)sqlite3_column_text(stmt, 1);

        snprintf(rows[count].id, sizeof(rows[count].id), "%s", id != NULL ? id : "");
        snprintf(rows[count].name, sizeof(rows[count].name), "%s", name != NULL ? name : "");
        rows[count].merged = 0;
        count++;
    }
    sqlite3_finalize(stmt);

    for(int i = 0; i < count && ok; i++) {
        if(rows[i].merged || rows[i].name[0] == '\0')
            continue;
        for(int j = i + 1; j < count; j++) {
            if(rows[j].merged)
                continue;
            if(!storage_ascii_equal_ci(rows[i].name, rows[j].name))
                continue;
            ok = storage_merge_habit_into(rows[i].id, rows[j].id);
            rows[j].merged = 1;
            if(!ok)
                break;
        }
    }
    return ok;
}

int
storage_apply_sync_response_json(const char *response_json)
{
    static const char *habits_sql =
        "INSERT INTO "
        "habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,"
        "counter_enabled,sort_order,deleted_at,updated_at) "
        "SELECT COALESCE(json_extract(value,'$.id'),''),?2,"
        "       COALESCE(json_extract(value,'$.name'),''),"
        "       CAST(COALESCE(json_extract(value,'$.color_r'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.color_g'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.color_b'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.sync_mode'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.sync_activity'),0) AS "
        "INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.counter_enabled'),"
        "                     (SELECT counter_enabled FROM habits WHERE "
        "id=COALESCE(json_extract(value,'$.id'),'')),"
        "                     0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.sort_order'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.deleted_at'),0) AS INTEGER),"
        "       "
        "CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),'0') AS "
        "INTEGER) "
        "FROM json_each(?1,'$.changes.habits') "
        "WHERE COALESCE(json_extract(value,'$.id'),'')<>'' "
        "ON CONFLICT(id) DO UPDATE SET "
        " user_id=excluded.user_id,name=excluded.name,color_r=excluded.color_r,"
        "color_g=excluded.color_g,"
        " color_b=excluded.color_b,sync_mode=excluded.sync_mode,sync_activity="
        "excluded.sync_activity,"
        " counter_enabled=excluded.counter_enabled,sort_order=excluded.sort_"
        "order,deleted_at=excluded.deleted_at,"
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
        "       CAST(COALESCE(json_extract(value,'$.count'),CASE WHEN "
        "json_extract(value,'$.completed') THEN 1 ELSE 0 END) AS INTEGER),"
        "       "
        "CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),'0') AS "
        "INTEGER) "
        "FROM json_each(?1,'$.changes.habit_days') "
        "WHERE COALESCE(json_extract(value,'$.habit_id'),'')<>'' "
        "  AND CAST(COALESCE(json_extract(value,'$.local_date'),0) AS INTEGER)>0 "
        "ON CONFLICT(habit_id,local_date) DO UPDATE SET "
        " completed=excluded.completed,count=excluded.count,updated_at=excluded."
        "updated_at "
        "WHERE excluded.updated_at > habit_days.updated_at "
        "OR (excluded.updated_at = habit_days.updated_at AND NOT EXISTS ("
        "     SELECT 1 FROM sync_outbox "
        "     WHERE entity_type='habit_day' AND entity_id=habit_days.habit_id "
        "       AND local_date=habit_days.local_date"
        "    ))";
    static const char *sessions_sql =
        "INSERT INTO "
        "sessions(id,user_id,started_at,local_date,topic,activity,source,"
        "imported_at,rounds_hash,deleted_at,updated_at) "
        "SELECT COALESCE(json_extract(value,'$.id'),''),?2,"
        "       "
        "CAST(COALESCE(strftime('%s',json_extract(value,'$.started_at')),'0') AS "
        "INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.local_date'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.topic'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.activity'),0) AS INTEGER),"
        "       COALESCE(json_extract(value,'$.source'),''),"
        "       "
        "CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),"
        "strftime('%s',json_extract(value,'$.started_at')),'0') AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.rounds_hash'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.deleted_at'),0) AS INTEGER),"
        "       "
        "CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),"
        "strftime('%s',json_extract(value,'$.started_at')),'0') AS INTEGER) "
        "FROM json_each(?1,'$.changes.sessions') "
        "WHERE COALESCE(json_extract(value,'$.id'),'')<>'' "
        "ON CONFLICT(id) DO UPDATE SET "
        " user_id=excluded.user_id,started_at=excluded.started_at,local_date="
        "excluded.local_date,"
        " topic=excluded.topic,activity=excluded.activity,source=excluded.source,"
        " imported_at=excluded.imported_at,rounds_hash=excluded.rounds_hash,"
        "deleted_at=excluded.deleted_at,"
        " updated_at=excluded.updated_at "
        "WHERE excluded.updated_at > sessions.updated_at "
        "OR (excluded.updated_at = sessions.updated_at AND NOT EXISTS ("
        " SELECT 1 FROM sync_outbox "
        " WHERE entity_type='session' AND entity_id=sessions.id AND local_date=0"
        "))";
    static const char *delete_rounds_sql =
        "DELETE FROM session_rounds WHERE session_id IN ("
        " SELECT COALESCE(json_extract(value,'$.id'),'') "
        " FROM json_each(?1,'$.changes.sessions') "
        " WHERE COALESCE(json_extract(value,'$.id'),'')<>'' "
        " AND ("
        "       NOT EXISTS (SELECT 1 FROM sessions WHERE "
        "id=COALESCE(json_extract(value,'$.id'),'')) "
        "       OR "
        "CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),"
        "strftime('%s',json_extract(value,'$.started_at')),'0') AS INTEGER) "
        "          > COALESCE((SELECT updated_at FROM sessions WHERE "
        "id=COALESCE(json_extract(value,'$.id'),'')),0) "
        "       OR "
        "(CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),"
        "strftime('%s',json_extract(value,'$.started_at')),'0') AS INTEGER) "
        "           = COALESCE((SELECT updated_at FROM sessions WHERE "
        "id=COALESCE(json_extract(value,'$.id'),'')),0) "
        "           AND NOT EXISTS (SELECT 1 FROM sync_outbox "
        "               WHERE entity_type='session' AND "
        "entity_id=COALESCE(json_extract(value,'$.id'),'') AND local_date=0))"
        " )"
        ")";
    static const char *rounds_sql =
        "INSERT OR REPLACE INTO session_rounds(session_id,round_index,seconds) "
        "SELECT COALESCE(json_extract(s.value,'$.id'),''),"
        "       CAST(COALESCE(json_extract(r.value,'$.round_index'),0) AS "
        "INTEGER),"
        "       CAST(COALESCE(json_extract(r.value,'$.hold_seconds'),0) AS "
        "INTEGER) "
        "FROM json_each(?1,'$.changes.sessions') AS s, "
        "json_each(s.value,'$.rounds') AS r "
        "WHERE COALESCE(json_extract(s.value,'$.id'),'')<>'' "
        "AND "
        "(CAST(COALESCE(strftime('%s',json_extract(s.value,'$.updated_at')),"
        "strftime('%s',json_extract(s.value,'$.started_at')),'0') AS INTEGER) "
        "       > COALESCE((SELECT updated_at FROM sessions WHERE "
        "id=COALESCE(json_extract(s.value,'$.id'),'')),0) "
        "OR "
        "(CAST(COALESCE(strftime('%s',json_extract(s.value,'$.updated_at')),"
        "strftime('%s',json_extract(s.value,'$.started_at')),'0') AS INTEGER) "
        "       = COALESCE((SELECT updated_at FROM sessions WHERE "
        "id=COALESCE(json_extract(s.value,'$.id'),'')),0) "
        "    AND NOT EXISTS (SELECT 1 FROM sync_outbox "
        "        WHERE entity_type='session' AND "
        "entity_id=COALESCE(json_extract(s.value,'$.id'),'') AND local_date=0)))";
    static const char *social_cache_sql =
        "INSERT INTO social_cache(user_id,kind,json,updated_at) "
        "SELECT ?2,"
        "       COALESCE(json_extract(value,'$.kind'),''),"
        "       json(COALESCE(json_extract(value,'$.json'),'{}')),"
        "       CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),'0') AS INTEGER) "
        "FROM json_each(?1,'$.changes.social_cache') "
        "WHERE COALESCE(json_extract(value,'$.kind'),'')<>'' "
        "ON CONFLICT(user_id,kind) DO UPDATE SET "
        " json=excluded.json,updated_at=excluded.updated_at";
    long long server_version;
    long long server_clock;
    long long old_server_version;
    char server_hash[80];
    char account_alias[40];

    if(g_storage.db == NULL || response_json == NULL || response_json[0] == '\0')
        return 0;
    if(!migrate_schema())
        return 0;
    g_storage.last_sync_changed = 0;
    if(!storage_json_valid(response_json))
        return 0;
    if(storage_json_extract_text(response_json, "$.account_alias", account_alias,
                                 sizeof(account_alias)))
        storage_set_setting_text(STORAGE_SYNC_ACCOUNT_ALIAS_KEY, account_alias);
    if(storage_json_extract_int64(response_json, "$.full_snapshot_required", 0) != 0 &&
       get_meta_int64(STORAGE_SYNC_APPLY_REVIEW_KEY, 0) == 0) {
        if(!storage_has_any() ||
           storage_sync_review_json_should_auto_apply_remote(response_json))
            return storage_apply_remote_full_snapshot(response_json);
        if(!storage_sync_review_write_json(response_json))
            return 0;
        set_meta(STORAGE_SYNC_PENDING_REVIEW_KEY, "1");
        g_storage.pending_sync_outbox_seq = 0;
        storage_schedule_persist();
        TraceLog(LOG_WARNING, "SYNC: full snapshot review required");
        return 1;
    }
    old_server_version = get_meta_int64("sync_last_server_version", 0);
    if(!exec_sql("BEGIN IMMEDIATE"))
        return 0;
    storage_clear_uploaded_outbox(g_storage.pending_sync_outbox_seq);
    if(!storage_exec_json_user_sql(habits_sql, response_json) ||
       !storage_exec_json_user_sql(habit_days_sql, response_json) ||
       !storage_merge_duplicate_habit_names() ||
       !storage_exec_json_user_sql(delete_rounds_sql, response_json) ||
       !storage_exec_json_user_sql(rounds_sql, response_json) ||
       !storage_exec_json_user_sql(sessions_sql, response_json) ||
       !storage_exec_json_user_sql(social_cache_sql, response_json)) {
        exec_sql("ROLLBACK");
        return 0;
    }
    if(!exec_sql("COMMIT")) {
        exec_sql("ROLLBACK");
        return 0;
    }
    storage_materialize_session_habit_days();
    server_version = storage_json_extract_int64(response_json, "$.server_version", 0);
    server_clock = storage_json_extract_int64(response_json, "$.server_clock", 0);
    TraceLog(LOG_INFO,
             "SYNC: response server_version=%lld old_server_version=%lld "
             "habits=%d habit_days=%d sessions=%d",
             server_version, old_server_version,
             storage_json_array_count(response_json, "$.changes.habits"),
             storage_json_array_count(response_json, "$.changes.habit_days"),
             storage_json_array_count(response_json, "$.changes.sessions"));
    if(server_version > old_server_version && storage_sync_response_has_changes(response_json))
        g_storage.last_sync_changed = 1;
    if(server_version > 0)
        set_meta_int64("sync_last_server_version", server_version);
    if(server_clock > 0)
        set_meta_int64(STORAGE_SYNC_SERVER_CLOCK_KEY, server_clock);
    if(storage_json_extract_text(response_json, "$.server_state_hash", server_hash,
                                 sizeof(server_hash)))
        set_meta(STORAGE_SYNC_LAST_SERVER_HASH_KEY, server_hash);
    set_meta(STORAGE_SYNC_PENDING_REVIEW_KEY, "");
    storage_sync_review_delete_json();
    set_meta_int64(STORAGE_SYNC_APPLY_REVIEW_KEY, 0);
    set_meta_int64(STORAGE_SYNC_FULL_REPLACE_KEY, 0);
    g_storage.pending_sync_outbox_seq = 0;
    set_meta_int64("sync_full_upload_done", 1);
    set_meta_int64(STORAGE_SYNC_BACKFILL_KEY, 1);
    set_meta_int64(STORAGE_SYNC_HABIT_NAME_REPAIR_KEY, 1);
    set_meta_int64(STORAGE_SYNC_ZERO_HABIT_DAY_REPAIR_KEY, 1);
    storage_mark_habits_initialized();
    storage_schedule_persist();
    return 1;
}

static int
storage_apply_remote_full_snapshot(const char *response_json)
{
    char *copy;
    int ok;

    if(response_json == NULL)
        return 0;
    copy = strdup(response_json);
    if(copy == NULL)
        return 0;
    if(!storage_clear_local_sync_data()) {
        free(copy);
        return 0;
    }
    set_meta_int64(STORAGE_SYNC_APPLY_REVIEW_KEY, 1);
    ok = storage_apply_sync_response_json(copy);
    free(copy);
    return ok;
}

static int
storage_clear_local_sync_data(void)
{
    static const char *const sqls[] = {"DELETE FROM session_rounds", "DELETE FROM sessions",
                                       "DELETE FROM habit_days", "DELETE FROM habits",
                                       "DELETE FROM social_cache",
                                       "DELETE FROM sync_outbox"};

    if(g_storage.db == NULL)
        return 0;
    if(!exec_sql("BEGIN IMMEDIATE"))
        return 0;
    for(size_t i = 0; i < sizeof(sqls) / sizeof(sqls[0]); i++) {
        if(!exec_sql(sqls[i])) {
            exec_sql("ROLLBACK");
            return 0;
        }
    }
    if(!exec_sql("COMMIT")) {
        exec_sql("ROLLBACK");
        return 0;
    }
    return 1;
}

int
storage_last_sync_changed(void)
{
    return g_storage.last_sync_changed;
}

int
storage_sync_status(InbeStorageSyncStatus *status)
{
    sqlite3_stmt *stmt = NULL;

    if(status == NULL)
        return 0;
    memset(status, 0, sizeof(*status));
    if(g_storage.db == NULL)
        return 0;

    status->has_account = storage_has_sync_account();
    status->review_pending = get_meta_int64(STORAGE_SYNC_PENDING_REVIEW_KEY, 0) != 0;
    status->repair_pending = get_meta_int64(STORAGE_SYNC_ZERO_HABIT_DAY_REPAIR_KEY, 0) == 0;
    status->full_upload_done = get_meta_int64("sync_full_upload_done", 0) != 0;
    status->server_version = get_meta_int64("sync_last_server_version", 0);
    status->server_clock = get_meta_int64(STORAGE_SYNC_SERVER_CLOCK_KEY, 0);
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COUNT(*) FROM sync_outbox", -1, &stmt, NULL) ==
       SQLITE_OK) {
        if(sqlite3_step(stmt) == SQLITE_ROW)
            status->queued_changes = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return 1;
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
        "DELETE FROM habit_days WHERE completed=0 AND count=0 AND "
        "session_count=0 "
        "AND NOT EXISTS (SELECT 1 FROM sync_outbox o WHERE "
        "o.entity_type='habit_day' "
        "AND o.entity_id=habit_days.habit_id AND "
        "o.local_date=habit_days.local_date)",
        "DELETE FROM habits WHERE deleted_at>0 AND id NOT IN "
        "(SELECT entity_id FROM sync_outbox WHERE entity_type='habit')"};

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

static int
storage_profile_day_offset(int local_date, int today_date)
{
    struct tm day_tm = {0};
    struct tm today_tm = {0};
    time_t day_time;
    time_t today_time;
    double diff_days;

    if(local_date <= 0 || today_date <= 0)
        return -1;
    day_tm.tm_year = local_date / 10000 - 1900;
    day_tm.tm_mon = (local_date / 100) % 100 - 1;
    day_tm.tm_mday = local_date % 100;
    day_tm.tm_hour = 12;
    today_tm.tm_year = today_date / 10000 - 1900;
    today_tm.tm_mon = (today_date / 100) % 100 - 1;
    today_tm.tm_mday = today_date % 100;
    today_tm.tm_hour = 12;
    day_time = mktime(&day_tm);
    today_time = mktime(&today_tm);
    diff_days = difftime(today_time, day_time) / 86400.0;
    if(diff_days < -0.5 || diff_days > 370.5)
        return -1;
    return (int)(diff_days + 0.5);
}

int
storage_profile_activity_stats(int activity, int today_date,
                               int *streak_out, long *avg_hold_out)
{
    sqlite3_stmt *stmt = NULL;
    int seen[371] = {0};
    int streak = 0;
    long avg_value = 0;

    if(streak_out != NULL)
        *streak_out = 0;
    if(avg_hold_out != NULL)
        *avg_hold_out = 0;
    if(g_storage.db == NULL || activity < 0 || activity >= 30)
        return 0;

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT local_date FROM sessions "
                          "WHERE user_id=?1 AND deleted_at=0 AND activity=?2 "
                          "  AND local_date>0",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    sqlite3_bind_int(stmt, 2, activity);
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        int offset = storage_profile_day_offset(sqlite3_column_int(stmt, 0), today_date);
        if(offset >= 0 && offset <= 370)
            seen[offset] = 1;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    while(streak <= 370 && seen[streak])
        streak++;

    if(activity == 0) {
        if(sqlite3_prepare_v2(g_storage.db,
                              "SELECT COALESCE(AVG(sr.seconds),0) "
                              "FROM sessions s JOIN session_rounds sr ON sr.session_id=s.id "
                              "WHERE s.user_id=?1 AND s.deleted_at=0 AND s.activity=?2 "
                              "  AND sr.seconds>0",
                              -1, &stmt, NULL) == SQLITE_OK) {
            bind_text(stmt, 1, g_storage.user_id);
            sqlite3_bind_int(stmt, 2, activity);
            if(sqlite3_step(stmt) == SQLITE_ROW)
                avg_value = (long)sqlite3_column_double(stmt, 0);
            sqlite3_finalize(stmt);
        }
    } else if(activity == 1) {
        if(sqlite3_prepare_v2(g_storage.db,
                              "SELECT COALESCE(AVG(total_seconds),0) "
                              "FROM ("
                              "  SELECT SUM(sr.seconds) AS total_seconds "
                              "  FROM sessions s JOIN session_rounds sr ON sr.session_id=s.id "
                              "  WHERE s.user_id=?1 AND s.deleted_at=0 AND s.activity=?2 "
                              "    AND sr.seconds>0 "
                              "  GROUP BY s.id"
                              ")",
                              -1, &stmt, NULL) == SQLITE_OK) {
            bind_text(stmt, 1, g_storage.user_id);
            sqlite3_bind_int(stmt, 2, activity);
            if(sqlite3_step(stmt) == SQLITE_ROW)
                avg_value = (long)sqlite3_column_double(stmt, 0);
            sqlite3_finalize(stmt);
        }
    }

    if(streak_out != NULL)
        *streak_out = streak;
    if(avg_hold_out != NULL)
        *avg_hold_out = avg_value;
    return 1;
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
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COUNT(*) FROM habits WHERE deleted_at=0", -1, &stmt,
                          NULL) != SQLITE_OK)
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
                          "SELECT "
                          "id,name,description,color_r,color_g,color_b,sync_mode,sync_activity,counter_"
                          "enabled "
                          "FROM habits WHERE deleted_at=0 ORDER BY sort_order,id LIMIT 10",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    while(index < INBE_HABIT_MAX && sqlite3_step(stmt) == SQLITE_ROW) {
        InbeHabit *habit = &habits->items[index];
        snprintf(habit->id, sizeof(habit->id), "%s", (const char *)sqlite3_column_text(stmt, 0));
        snprintf(habits->loaded_ids[index], sizeof(habits->loaded_ids[index]), "%s", habit->id);
        snprintf(habit->name, sizeof(habit->name), "%s",
                 (const char *)sqlite3_column_text(stmt, 1));
        snprintf(habit->description, sizeof(habit->description), "%s",
                 (const char *)sqlite3_column_text(stmt, 2));
        habit->color = (Color){(unsigned char)sqlite3_column_int(stmt, 3),
                               (unsigned char)sqlite3_column_int(stmt, 4),
                               (unsigned char)sqlite3_column_int(stmt, 5), 255};
        habit->sync_mode = sqlite3_column_int(stmt, 6);
        habit->sync_activity = sqlite3_column_int(stmt, 7);
        habit->counter_enabled = sqlite3_column_int(stmt, 8) != 0;
        index++;
    }
    sqlite3_finalize(stmt);
    habits->count = index;
    habits->loaded_count = index;

    for(int i = 0; i < habits->count; i++) {
        if(sqlite3_prepare_v2(g_storage.db,
                              "SELECT local_date,completed,count FROM habit_days "
                              "WHERE habit_id=?1 ORDER BY local_date",
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
            if(habits->items[i].days[d].count <= 0 && habits->items[i].days[d].completed)
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
    long long changed_at = storage_next_change_time();
    if(habits == NULL || g_storage.db == NULL)
        return;
    storage_mark_habits_initialized();
    exec_sql("BEGIN IMMEDIATE");
    exec_sql("CREATE TEMP TABLE IF NOT EXISTS sync_seen_habits(id TEXT PRIMARY KEY);"
             "CREATE TEMP TABLE IF NOT EXISTS sync_loaded_habits(id TEXT PRIMARY KEY);"
             "DELETE FROM sync_seen_habits;"
             "DELETE FROM sync_loaded_habits;");
    for(int i = 0; i < habits->loaded_count && i < INBE_HABIT_MAX; i++) {
        if(habits->loaded_ids[i][0] == '\0')
            continue;
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT OR IGNORE INTO sync_loaded_habits(id) VALUES(?1)", -1, &stmt,
                              NULL) != SQLITE_OK)
            continue;
        bind_text(stmt, 1, habits->loaded_ids[i]);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        stmt = NULL;
    }
    for(int i = 0; i < habits->count; i++) {
        const InbeHabit *habit = &habits->items[i];
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT INTO "
                              "habits(id,user_id,name,description,color_r,color_g,color_b,sync_mode,sync_"
                              "activity,counter_enabled,sort_order,deleted_at,updated_at) "
                              "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,0,?12) "
                              "ON CONFLICT(id) DO UPDATE SET "
                              "user_id=excluded.user_id,"
                              "name=excluded.name,"
                              "description=excluded.description,"
                              "color_r=excluded.color_r,"
                              "color_g=excluded.color_g,"
                              "color_b=excluded.color_b,"
                              "sync_mode=excluded.sync_mode,"
                              "sync_activity=excluded.sync_activity,"
                              "counter_enabled=excluded.counter_enabled,"
                              "sort_order=excluded.sort_order,"
                              "deleted_at=0,"
                              "updated_at=excluded.updated_at "
                              "WHERE habits.user_id<>excluded.user_id OR "
                              "habits.name<>excluded.name OR "
                              "habits.color_r<>excluded.color_r OR "
                              "habits.color_g<>excluded.color_g OR "
                              "habits.color_b<>excluded.color_b OR "
                              "habits.sync_mode<>excluded.sync_mode OR "
                              "habits.sync_activity<>excluded.sync_activity OR "
                              "habits.counter_enabled<>excluded.counter_enabled OR "
                              "habits.sort_order<>excluded.sort_order OR "
                              "habits.deleted_at<>0",
                              -1, &stmt, NULL) != SQLITE_OK)
            continue;
        bind_text(stmt, 1, habit->id);
        bind_text(stmt, 2, g_storage.user_id);
        bind_text(stmt, 3, habit->name);
        bind_text(stmt, 4, habit->description);
        sqlite3_bind_int(stmt, 5, habit->color.r);
        sqlite3_bind_int(stmt, 6, habit->color.g);
        sqlite3_bind_int(stmt, 7, habit->color.b);
        sqlite3_bind_int(stmt, 8, habit->sync_mode);
        sqlite3_bind_int(stmt, 9, habit->sync_activity);
        sqlite3_bind_int(stmt, 10, habit->counter_enabled ? 1 : 0);
        sqlite3_bind_int(stmt, 11, i);
        sqlite3_bind_int64(stmt, 12, changed_at);
        if(sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(g_storage.db) > 0)
            storage_enqueue_sync_habit(habit->id);
        sqlite3_finalize(stmt);
        stmt = NULL;
        if(sqlite3_prepare_v2(g_storage.db,
                              "UPDATE habits SET description=?2 "
                              "WHERE id=?1 AND description<>?2",
                              -1, &stmt, NULL) == SQLITE_OK) {
            bind_text(stmt, 1, habit->id);
            bind_text(stmt, 2, habit->description);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            stmt = NULL;
        }
        if(sqlite3_prepare_v2(g_storage.db, "INSERT OR IGNORE INTO sync_seen_habits(id) VALUES(?1)",
                              -1, &stmt, NULL) == SQLITE_OK) {
            bind_text(stmt, 1, habit->id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            stmt = NULL;
        }
        for(int d = 0; d < habit->day_count; d++) {
            if(sqlite3_prepare_v2(g_storage.db,
                                  "INSERT INTO "
                                  "habit_days(habit_id,local_date,completed,count,updated_at) "
                                  "VALUES(?1,?2,?3,?4,?5) "
                                  "ON CONFLICT(habit_id,local_date) DO UPDATE SET "
                                  "completed=excluded.completed,"
                                  "count=excluded.count,"
                                  "updated_at=CASE WHEN habit_days.completed<>excluded.completed "
                                  "OR habit_days.count<>excluded.count "
                                  "THEN excluded.updated_at ELSE habit_days.updated_at END",
                                  -1, &stmt, NULL) != SQLITE_OK)
                continue;
            bind_text(stmt, 1, habit->id);
            sqlite3_bind_int(stmt, 2, habit->days[d].day_index);
            sqlite3_bind_int(stmt, 3, habit->days[d].count > 0 || habit->days[d].completed ? 1 : 0);
            sqlite3_bind_int(stmt, 4,
                             habit->days[d].count > 0 ? habit->days[d].count
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
                          "AND id IN (SELECT id FROM sync_loaded_habits) "
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
                                  "SELECT local_date FROM habit_days "
                                  "WHERE habit_id=?1 AND (completed!=0 OR count>0 "
                                  "OR session_count>0)",
                                  -1, &stmt, NULL) == SQLITE_OK) {
                bind_text(stmt, 1, deleted_ids[i]);
                while(sqlite3_step(stmt) == SQLITE_ROW)
                    storage_enqueue_sync_habit_day(deleted_ids[i], sqlite3_column_int(stmt, 0));
                sqlite3_finalize(stmt);
                stmt = NULL;
            }
            if(sqlite3_prepare_v2(g_storage.db,
                                  "UPDATE habit_days SET "
                                  "completed=0,count=0,session_count=0,updated_at=?2 "
                                  "WHERE habit_id=?1 AND (completed!=0 OR count>0 OR "
                                  "session_count>0)",
                                  -1, &stmt, NULL) == SQLITE_OK) {
                bind_text(stmt, 1, deleted_ids[i]);
                sqlite3_bind_int64(stmt, 2, changed_at);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                stmt = NULL;
            }
            if(sqlite3_prepare_v2(g_storage.db,
                                  "UPDATE habits SET deleted_at=?2,updated_at=?2 WHERE id=?1", -1,
                                  &stmt, NULL) != SQLITE_OK)
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
    exec_sql("DELETE FROM sync_loaded_habits;");
    exec_sql("COMMIT");
    storage_materialize_session_habit_days();
    storage_schedule_persist();
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
