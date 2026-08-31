#include "storage.h"

#include "db.h"
#include "storage_json_builder.h"

#include "ksync_account.h"
#include "ksync_crypto.h"
#include "kryon.h"
#include <sqlite3.h>
#include <stdint.h>
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
static long long
storage_sync_outbox_batch_seq(int limit);
static int
storage_has_pending_sync_outbox(void);
static int
storage_has_pending_encrypted_shadow_migration(void);
static int
storage_sync_response_has_legacy_records(const char *response_json);
static long long
storage_encrypted_shadow_source_count(void);
static void
storage_prepare_encrypted_shadow_migration_queue(void);
static void
storage_clear_completed_encrypted_shadow_outbox_if_stale(void);
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
#define STORAGE_SYNC_SERVER_URL_KEY "sync_server_url"
#define STORAGE_SYNC_SERVER_CONNECTED_KEY "sync_server_connected"
#define STORAGE_SYNC_LAST_SERVER_HASH_KEY "sync_last_server_state_hash"
#define STORAGE_SYNC_SERVER_CLOCK_KEY "sync_server_clock"
#define STORAGE_SYNC_LATEST_PROTOCOL_KEY "sync_latest_protocol"
#define STORAGE_SYNC_PENDING_REVIEW_KEY "sync_pending_review_pending"
#define STORAGE_SYNC_APPLY_REVIEW_KEY "sync_apply_pending_review"
#define STORAGE_SYNC_FULL_REPLACE_KEY "sync_full_replace_requested"
#define STORAGE_SYNC_ACCOUNT_ALIAS_KEY "sync_account_alias"
#define STORAGE_SYNC_DATA_OWNER_PUBLIC_ID_KEY "sync_data_owner_public_id"
#define STORAGE_SYNC_ZERO_HABIT_DAY_REPAIR_KEY "sync_zero_habit_day_repair_v1_done"
#define STORAGE_SYNC_ENCRYPTED_SHADOW_KEY "sync_encrypted_shadow_v4_complete_v2"
#define STORAGE_SYNC_ENCRYPTED_SHADOW_LEGACY_SEEN_KEY "sync_encrypted_shadow_v4_legacy_seen_v2"
#define STORAGE_SYNC_ENCRYPTED_SHADOW_QUEUED_KEY "sync_encrypted_shadow_v4_queued_v2"
#define STORAGE_SYNC_ENCRYPTED_SHADOW_TOTAL_KEY "sync_encrypted_shadow_v4_total_v2"
#define STORAGE_SYNC_OP_BATCH_LIMIT 400
#define STORAGE_SYNC_RECORD_KEY_CONTEXT "inbe-ksync-record-key-v1"
#define STORAGE_SYNC_RECORD_KEY_ID "inbe-v4-main"

long long
storage_next_change_time(void)
{
    long long now = now_seconds();
    long long latest;

    latest = db_select_int64("SELECT MAX(updated_at) FROM ("
                                  " SELECT COALESCE(MAX(updated_at),0) AS updated_at FROM habits"
                                  " UNION ALL SELECT COALESCE(MAX(updated_at),0) FROM habit_days"
                                  " UNION ALL SELECT COALESCE(MAX(updated_at),0) FROM sessions"
                                  ")",
                                  0);
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

void
storage_make_uuid(char out[37])
{
    if(out == NULL)
        return;
    make_client_uuid(out);
}

void
storage_schedule_persist(void)
{
#if defined(__EMSCRIPTEN__)
    if(g_storage.db != NULL) {
        sqlite3_db_cacheflush(g_storage.db);
        if(sqlite3_get_autocommit(g_storage.db)) {
            sqlite3_exec(g_storage.db, "PRAGMA wal_checkpoint(TRUNCATE)", NULL, NULL, NULL);
            sqlite3_db_cacheflush(g_storage.db);
        }
    }
    ScheduleWebStorageSync(120, 0);
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
    set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_KEY, 0);
    set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_LEGACY_SEEN_KEY, 0);
    set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_QUEUED_KEY, 0);
    set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_TOTAL_KEY, 0);
    exec_sql("DELETE FROM sync_outbox");
    if(storage_has_sync_account())
        storage_enqueue_all_sync_state();
    storage_schedule_persist();
}

const char *
storage_sync_data_owner_public_id(void)
{
    const char *owner = get_meta_text(STORAGE_SYNC_DATA_OWNER_PUBLIC_ID_KEY);
    return owner != NULL && owner[0] != '\0' ? owner : NULL;
}

void
storage_set_sync_data_owner_public_id(const char *public_id)
{
    if(g_storage.db == NULL)
        return;
    set_meta(STORAGE_SYNC_DATA_OWNER_PUBLIC_ID_KEY,
             public_id != NULL ? public_id : "");
    storage_schedule_persist();
}

int
storage_has_local_syncable_data(void)
{
    if(g_storage.db == NULL)
        return 0;
    return db_select_int64(
               "SELECT EXISTS(SELECT 1 FROM sessions "
               "WHERE user_id=(SELECT id FROM users LIMIT 1) AND deleted_at=0 LIMIT 1) "
               "OR EXISTS(SELECT 1 FROM habits "
               "WHERE user_id=(SELECT id FROM users LIMIT 1) AND deleted_at=0 LIMIT 1) "
               "OR EXISTS(SELECT 1 FROM habit_days hd JOIN habits h ON h.id=hd.habit_id "
               "WHERE h.user_id=(SELECT id FROM users LIMIT 1) "
               "AND (hd.completed!=0 OR hd.count>0) LIMIT 1)",
               0) != 0;
}

int
storage_clear_local_syncable_data(void)
{
    int ok = storage_clear_local_sync_data();
    if(ok)
        storage_schedule_persist();
    return ok;
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
                          "SELECT json FROM social_snapshots WHERE user_id=?1 AND kind=?2",
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
                          "SELECT json=?3 FROM social_snapshots WHERE user_id=?1 AND kind=?2",
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
                          "INSERT INTO social_snapshots(user_id,kind,json,updated_at) "
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

int
storage_setting_text_equals(const char *key, const char *value)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 0;

    if(g_storage.db == NULL || key == NULL || value == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT 1 FROM settings WHERE user_id=?1 AND key=?2 AND value=?3",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, key);
    bind_text(stmt, 3, value);
    ok = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return ok;
}

int
storage_list_settings(void (*callback)(const char *key, const char *value, void *user),
                      void *user)
{
    sqlite3_stmt *stmt = NULL;

    if(g_storage.db == NULL || callback == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT key,value FROM settings WHERE user_id=?1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *key = (const char *)sqlite3_column_text(stmt, 0);
        const char *value = (const char *)sqlite3_column_text(stmt, 1);
        callback(key != NULL ? key : "", value != NULL ? value : "", user);
    }
    sqlite3_finalize(stmt);
    return 1;
}

void
storage_set_setting_text(const char *key, const char *value)
{
    sqlite3_stmt *stmt = NULL;
    int rc;

    if(g_storage.db == NULL || key == NULL)
        return;
    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT INTO settings(user_id,key,value,updated_at) "
                          "VALUES(?1,?2,?3,?4) "
                          "ON CONFLICT(user_id,key) DO UPDATE SET "
                          "value=excluded.value,updated_at=excluded.updated_at "
                          "WHERE settings.value<>excluded.value",
                          -1, &stmt, NULL) != SQLITE_OK)
        return;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, key);
    bind_text(stmt, 3, value != NULL ? value : "");
    sqlite3_bind_int64(stmt, 4, now_seconds());
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if(rc == SQLITE_DONE && sqlite3_changes(g_storage.db) > 0 &&
       g_storage.settings_write_depth <= 0)
        storage_schedule_persist();
}

void
storage_settings_begin_write(void)
{
    if(g_storage.db == NULL)
        return;
    if(g_storage.settings_write_depth == 0)
        exec_sql("BEGIN IMMEDIATE");
    g_storage.settings_write_depth++;
}

void
storage_settings_end_write(void)
{
    if(g_storage.db == NULL || g_storage.settings_write_depth <= 0)
        return;
    g_storage.settings_write_depth--;
    if(g_storage.settings_write_depth == 0) {
        exec_sql("COMMIT");
        storage_schedule_persist();
    }
}

void
storage_set_setting_int(const char *key, int value)
{
    char text[32];
    snprintf(text, sizeof(text), "%d", value);
    storage_set_setting_text(key, text);
}

static int
storage_setting_exists(const char *key)
{
    sqlite3_stmt *stmt = NULL;
    int exists = 0;

    if(g_storage.db == NULL || key == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT EXISTS(SELECT 1 FROM settings "
                          "WHERE user_id=?1 AND key=?2)",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, key);
    if(sqlite3_step(stmt) == SQLITE_ROW)
        exists = sqlite3_column_int(stmt, 0) != 0;
    sqlite3_finalize(stmt);
    return exists;
}

static int
storage_sync_url_looks_valid(const char *url)
{
    if(url == NULL || url[0] == '\0')
        return 0;
    if(strncmp(url, "https://", 8) == 0)
        return url[8] != '\0';
    if(strncmp(url, "http://", 7) == 0)
        return url[7] != '\0';
    return 0;
}

int
storage_migrate_sync_server_connected_flag(void)
{
    const char *url;

    if(g_storage.db == NULL)
        return 0;
    if(storage_setting_exists(STORAGE_SYNC_SERVER_CONNECTED_KEY))
        return 1;
    if(!storage_has_sync_account())
        return 1;
    url = storage_get_setting_text(STORAGE_SYNC_SERVER_URL_KEY);
    if(!storage_sync_url_looks_valid(url))
        return 1;
    if(get_meta_int64("sync_last_server_version", 0) <= 0 &&
       get_meta_int64("sync_full_upload_done", 0) == 0)
        return 1;

    storage_set_setting_int(STORAGE_SYNC_SERVER_CONNECTED_KEY, 1);
    TraceLog(LOG_INFO, "SYNC: migrated existing synced account to connected server state");
    return 1;
}

int
storage_sync_server_connected(void)
{
    return storage_has_sync_account() &&
           storage_get_setting_int(STORAGE_SYNC_SERVER_CONNECTED_KEY, 0) != 0;
}

void
storage_set_sync_server_connected(int connected)
{
    storage_set_setting_int(STORAGE_SYNC_SERVER_CONNECTED_KEY, connected ? 1 : 0);
}

static void
storage_append_habit_row_json(StorageJsonBuilder *json, sqlite3_stmt *stmt)
{
    storage_json_builder_append(json, "{");
    storage_json_builder_append_key_string(json, "id", (const char *)sqlite3_column_text(stmt, 0));
    storage_json_builder_append(json, ",");
    storage_json_builder_append_key_string(json, "name", (const char *)sqlite3_column_text(stmt, 1));
    storage_json_builder_appendf(json, ",\"color_r\":%d,\"color_g\":%d,\"color_b\":%d",
                 sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
                 sqlite3_column_int(stmt, 4));
    storage_json_builder_appendf(json,
                 ",\"sync_mode\":%d,\"sync_activity\":%d,\"counter_enabled\":"
                 "%d,\"sort_order\":%d,\"deleted_at\":%lld",
                 sqlite3_column_int(stmt, 5), sqlite3_column_int(stmt, 6),
                 sqlite3_column_int(stmt, 7) != 0 ? 1 : 0, sqlite3_column_int(stmt, 8),
                 sqlite3_column_int64(stmt, 9));
    storage_json_builder_append(json, ",\"updated_at\":");
    storage_json_builder_append_epoch(json, sqlite3_column_int64(stmt, 10));
    storage_json_builder_appendf(json, ",\"weekdays\":%d,\"reminder_hour\":%d",
                 sqlite3_column_int(stmt, 11), sqlite3_column_int(stmt, 12));
    storage_json_builder_append(json, "}");
}

static void
storage_append_habit_day_row_json(StorageJsonBuilder *json, sqlite3_stmt *stmt)
{
    storage_json_builder_append(json, "{");
    storage_json_builder_append_key_string(json, "habit_id", (const char *)sqlite3_column_text(stmt, 0));
    storage_json_builder_appendf(json, ",\"local_date\":%d,\"completed\":%s,\"count\":%d,\"updated_at\":",
                 sqlite3_column_int(stmt, 1),
                 sqlite3_column_int(stmt, 2) != 0 ? "true" : "false",
                 sqlite3_column_int(stmt, 3));
    storage_json_builder_append_epoch(json, sqlite3_column_int64(stmt, 4));
    storage_json_builder_append(json, "}");
}

static void
storage_append_session_rounds_json(StorageJsonBuilder *json, const char *session_id)
{
    sqlite3_stmt *stmt = NULL;
    int first = 1;

    storage_json_builder_append(json, "\"rounds\":[");
    if(g_storage.db != NULL && session_id != NULL &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT round_index,seconds FROM session_rounds "
                          "WHERE session_id=?1 ORDER BY round_index",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, session_id);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            int seconds = sqlite3_column_int(stmt, 1);
            if(!first)
                storage_json_builder_append(json, ",");
            first = 0;
            storage_json_builder_appendf(json, "{\"round_index\":%d,\"breaths\":0,\"hold_seconds\":%d}",
                         sqlite3_column_int(stmt, 0), seconds);
        }
    }
    if(stmt != NULL)
        sqlite3_finalize(stmt);
    storage_json_builder_append(json, "]");
}

static void
storage_append_session_row_json(StorageJsonBuilder *json, sqlite3_stmt *stmt)
{
    const char *id = (const char *)sqlite3_column_text(stmt, 0);
    long long started_at = sqlite3_column_int64(stmt, 1);
    long long updated_at = sqlite3_column_int64(stmt, 14);

    storage_json_builder_append(json, "{");
    storage_json_builder_append_key_string(json, "id", id);
    storage_json_builder_append(json, ",\"started_at\":");
    storage_json_builder_append_epoch(json, started_at);
    storage_json_builder_appendf(json, ",\"local_date\":%d,\"topic\":\"%d\",\"activity\":%d,",
                 sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3),
                 sqlite3_column_int(stmt, 4));
    storage_json_builder_append_key_string(json, "source", (const char *)sqlite3_column_text(stmt, 5));
    storage_json_builder_append(json, ",");
    storage_json_builder_append_key_string(json, "rounds_hash", (const char *)sqlite3_column_text(stmt, 6));
    storage_json_builder_appendf(json,
                 ",\"mood_before\":%d,\"mood_after\":%d,\"energy\":%d,"
                 "\"stress\":%d,",
                 sqlite3_column_int(stmt, 7), sqlite3_column_int(stmt, 8),
                 sqlite3_column_int(stmt, 9), sqlite3_column_int(stmt, 10));
    storage_json_builder_append_key_string(json, "note", (const char *)sqlite3_column_text(stmt, 11));
    storage_json_builder_append(json, ",");
    storage_json_builder_append_key_string(json, "tags", (const char *)sqlite3_column_text(stmt, 12));
    storage_json_builder_appendf(json, ",\"deleted_at\":%lld,\"updated_at\":", sqlite3_column_int64(stmt, 13));
    storage_json_builder_append_epoch(json, updated_at > 0 ? updated_at : started_at);
    storage_json_builder_append(json, ",");
    storage_append_session_rounds_json(json, id);
    storage_json_builder_append(json, "}");
}

typedef void (*StorageJsonRowFn)(StorageJsonBuilder *json, sqlite3_stmt *stmt);

static int
storage_append_id_payload_json(StorageJsonBuilder *json, const char *sql,
                               const char *id, StorageJsonRowFn append_row)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(g_storage.db == NULL || id == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, id);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        found = 1;
        append_row(json, stmt);
    }
    sqlite3_finalize(stmt);
    return found;
}

static int
storage_append_dated_payload_json(StorageJsonBuilder *json, const char *sql,
                                  const char *id, int local_date,
                                  StorageJsonRowFn append_row)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(g_storage.db == NULL || id == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, g_storage.user_id);
    bind_text(stmt, 2, id);
    sqlite3_bind_int(stmt, 3, local_date);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        found = 1;
        append_row(json, stmt);
    }
    sqlite3_finalize(stmt);
    return found;
}

static int
storage_append_habit_payload_json(StorageJsonBuilder *json, const char *habit_id)
{
    return storage_append_id_payload_json(
        json,
        "SELECT "
        "id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_"
        "enabled,sort_order,deleted_at,updated_at,weekdays,reminder_hour "
        "FROM habits WHERE user_id=?1 AND id=?2",
        habit_id, storage_append_habit_row_json);
}

static int
storage_append_habit_day_payload_json(StorageJsonBuilder *json, const char *habit_id, int local_date)
{
    return storage_append_dated_payload_json(
        json,
        "SELECT "
        "hd.habit_id,hd.local_date,hd.completed,hd.count,hd.updated_at "
        "FROM habit_days hd JOIN habits h ON h.id=hd.habit_id "
        "WHERE h.user_id=?1 AND hd.habit_id=?2 AND hd.local_date=?3",
        habit_id, local_date, storage_append_habit_day_row_json);
}

static int
storage_append_session_payload_json(StorageJsonBuilder *json, const char *session_id)
{
    return storage_append_id_payload_json(
        json,
        "SELECT "
        "id,started_at,local_date,topic,activity,source,"
        "rounds_hash,mood_before,mood_after,energy,stress,note,tags,"
        "deleted_at,updated_at "
        "FROM sessions WHERE user_id=?1 AND id=?2",
        session_id, storage_append_session_row_json);
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
storage_append_sync_op_payload(StorageJsonBuilder *json, const char *entity_type, const char *entity_id,
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
        storage_json_builder_append(json, "{}");
}

static void
storage_append_sync_ops_json(StorageJsonBuilder *json, long long through_seq)
{
    sqlite3_stmt *stmt = NULL;
    const char *client_id = storage_sync_client_id();
    int first = 1;

    storage_json_builder_append(json, "\"ops\":[");
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
                storage_json_builder_append(json, ",");
            first = 0;
            storage_json_builder_append(json, "{");
            storage_json_builder_append(json, "\"op_id\":");
            storage_json_builder_append_string(json, op_id);
            storage_json_builder_append(json, ",\"client_id\":");
            storage_json_builder_append_string(json, client_id);
            storage_json_builder_appendf(json, ",\"seq\":%lld", seq);
            storage_json_builder_append(json, ",\"entity_type\":");
            storage_json_builder_append_string(json, entity_type);
            storage_json_builder_append(json, ",\"entity_id\":");
            storage_json_builder_append_string(json, entity_id);
            if(local_date > 0)
                storage_json_builder_appendf(json, ",\"local_date\":%d", local_date);
            storage_json_builder_append(json, ",\"op_type\":");
            storage_json_builder_append_string(json, is_delete ? "delete" : "upsert");
            storage_json_builder_append(json, ",\"payload\":");
            storage_append_sync_op_payload(json, entity_type, entity_id, local_date);
            storage_json_builder_append(json, ",\"created_at\":");
            storage_json_builder_append_epoch(json, queued_at);
            storage_json_builder_append(json, "}");
        }
    }
    if(stmt != NULL)
        sqlite3_finalize(stmt);
    storage_json_builder_append(json, "]");
}

static const char *
storage_encrypted_collection_for_entity(const char *entity_type)
{
    if(entity_type == NULL)
        return NULL;
    if(strcmp(entity_type, "habit") == 0)
        return "inbe.habits";
    if(strcmp(entity_type, "habit_day") == 0)
        return "inbe.habit_days";
    if(strcmp(entity_type, "session") == 0)
        return "inbe.sessions";
    return NULL;
}

static int
storage_encrypted_record_id(const char *entity_type, const char *entity_id,
                            int local_date, char *out, size_t out_size)
{
    int written;

    if(out == NULL || out_size == 0 || entity_type == NULL ||
       entity_id == NULL || entity_id[0] == '\0')
        return 0;
    out[0] = '\0';
    if(strcmp(entity_type, "habit_day") == 0)
        written = snprintf(out, out_size, "%s:%d", entity_id, local_date);
    else
        written = snprintf(out, out_size, "%s", entity_id);
    return written > 0 && (size_t)written < out_size;
}

static int
storage_private_record_key(uint8_t out[32])
{
    const char *private_key_hex = storage_get_setting_text(STORAGE_SYNC_PRIVATE_KEY_KEY);
    uint8_t private_key[(KSYNC_PRIVATE_KEY_HEX_SIZE - 1) / 2];
    int ok;

    if(private_key_hex == NULL)
        return 0;
    ok = KsyncCryptoHexToBytes(private_key_hex, private_key, sizeof(private_key));
    if(ok) {
        KsyncCryptoHmacSha256(private_key, sizeof(private_key),
                              (const uint8_t *)STORAGE_SYNC_RECORD_KEY_CONTEXT,
                              strlen(STORAGE_SYNC_RECORD_KEY_CONTEXT), out);
    }
    memset(private_key, 0, sizeof(private_key));
    return ok;
}

static int
storage_append_encrypted_record_json(StorageJsonBuilder *json, const uint8_t key[32],
                                     const char *collection, const char *record_id,
                                     const char *entity_type, const char *entity_id,
                                     int local_date, long long queued_at, int deleted)
{
    StorageJsonBuilder plain = {0};
    StorageJsonBuilder aad = {0};
    uint8_t nonce[12];
    uint8_t *sealed = NULL;
    char *sealed_hex = NULL;
    char nonce_hex[25];
    size_t sealed_len;
    int ok = 0;

    if(json == NULL || key == NULL || collection == NULL || record_id == NULL)
        return 0;
    plain.ok = 1;
    storage_append_sync_op_payload(&plain, entity_type, entity_id, local_date);
    if(!plain.ok || plain.data == NULL)
        goto done;

    sealed_len = plain.len + 16;
    sealed = (uint8_t *)malloc(sealed_len);
    sealed_hex = (char *)malloc(sealed_len * 2 + 1);
    if(sealed == NULL || sealed_hex == NULL)
        goto done;

    aad.ok = 1;
    storage_json_builder_append(&aad, collection);
    storage_json_builder_append(&aad, "\n");
    storage_json_builder_append(&aad, record_id);
    storage_json_builder_append(&aad, "\n");
    storage_json_builder_append(&aad, STORAGE_SYNC_RECORD_KEY_ID);
    if(!aad.ok || aad.data == NULL)
        goto done;

    KsyncCryptoRandom(nonce, sizeof(nonce));
    if(!KsyncCryptoChaCha20Poly1305Seal(key, nonce, (const uint8_t *)plain.data,
                                        plain.len, (const uint8_t *)aad.data,
                                        aad.len, sealed))
        goto done;
    if(!KsyncCryptoBytesToHex(nonce, sizeof(nonce), nonce_hex, sizeof(nonce_hex)) ||
       !KsyncCryptoBytesToHex(sealed, sealed_len, sealed_hex, sealed_len * 2 + 1))
        goto done;

    storage_json_builder_append(json, "{");
    storage_json_builder_append_key_string(json, "collection", collection);
    storage_json_builder_append(json, ",");
    storage_json_builder_append_key_string(json, "id", record_id);
    storage_json_builder_append(json, ",");
    storage_json_builder_append_key_string(json, "key_id", STORAGE_SYNC_RECORD_KEY_ID);
    storage_json_builder_append(json, ",");
    storage_json_builder_append_key_string(json, "nonce", nonce_hex);
    storage_json_builder_append(json, ",");
    storage_json_builder_append_key_string(json, "ciphertext", sealed_hex);
    storage_json_builder_append(json, ",\"updated_at\":");
    storage_json_builder_append_epoch(json, queued_at);
    if(deleted)
        storage_json_builder_appendf(json, ",\"deleted_at\":%lld", queued_at > 0 ? queued_at : now_seconds());
    storage_json_builder_append(json, "}");
    ok = json->ok;

done:
    storage_json_builder_free(&plain);
    storage_json_builder_free(&aad);
    free(sealed);
    free(sealed_hex);
    return ok;
}

static void
storage_append_encrypted_records_json(StorageJsonBuilder *json, long long through_seq)
{
    sqlite3_stmt *stmt = NULL;
    uint8_t key[32];
    int has_key;
    int first = 1;

    storage_json_builder_append(json, "\"encrypted_records\":[");
    has_key = storage_private_record_key(key);
    if(has_key && g_storage.db != NULL &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT seq,entity_type,entity_id,local_date,queued_at "
                          "FROM sync_outbox WHERE seq<=?1 ORDER BY seq",
                          -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, through_seq);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            const char *entity_type = (const char *)sqlite3_column_text(stmt, 1);
            const char *entity_id = (const char *)sqlite3_column_text(stmt, 2);
            int local_date = sqlite3_column_int(stmt, 3);
            long long queued_at = sqlite3_column_int64(stmt, 4);
            const char *collection = storage_encrypted_collection_for_entity(entity_type);
            int is_delete = storage_sync_op_is_delete(entity_type, entity_id, local_date);
            char record_id[180];

            if(collection == NULL ||
               !storage_encrypted_record_id(entity_type, entity_id, local_date,
                                            record_id, sizeof(record_id)))
                continue;
            if(!first)
                storage_json_builder_append(json, ",");
            first = 0;
            if(!storage_append_encrypted_record_json(json, key, collection, record_id,
                                                     entity_type, entity_id, local_date,
                                                     queued_at, is_delete)) {
                json->ok = 0;
                break;
            }
        }
    }
    if(stmt != NULL)
        sqlite3_finalize(stmt);
    memset(key, 0, sizeof(key));
    storage_json_builder_append(json, "]");
}

char *
storage_build_sync_payload_json(const char *user_id_hash, const char *public_key_hex)
{
    StorageJsonBuilder json = {0};
    long long since_server_version;
    long long through_seq;
    int full_upload_done;
    int force_zero_day_repair;
    uint8_t shadow_key[32];
    int can_shadow_encrypt;

    if(g_storage.db == NULL || user_id_hash == NULL || user_id_hash[0] == '\0')
        return NULL;
    if(!migrate_schema())
        return NULL;
    if(!storage_materialize_session_habit_days())
        return NULL;
    if(!get_meta_int64(STORAGE_SYNC_BACKFILL_KEY, 0))
        storage_enqueue_all_sync_state();
    can_shadow_encrypt = storage_private_record_key(shadow_key);
    memset(shadow_key, 0, sizeof(shadow_key));
    if(can_shadow_encrypt && storage_has_pending_encrypted_shadow_migration() &&
       get_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_QUEUED_KEY, 0) == 0)
        storage_prepare_encrypted_shadow_migration_queue();
    since_server_version = get_meta_int64("sync_last_server_version", 0);
    full_upload_done = get_meta_int64("sync_full_upload_done", 0) != 0;
    if(!get_meta_int64(STORAGE_SYNC_HABIT_NAME_REPAIR_KEY, 0) || storage_has_orphan_habit_days())
        since_server_version = 0;
    force_zero_day_repair = get_meta_int64(STORAGE_SYNC_ZERO_HABIT_DAY_REPAIR_KEY, 0) == 0;
    if(force_zero_day_repair)
        since_server_version = 0;
    through_seq = storage_sync_outbox_batch_seq(STORAGE_SYNC_OP_BATCH_LIMIT);
    g_storage.pending_sync_outbox_seq = through_seq;
    json.ok = 1;
    storage_json_builder_append(&json, "{");
    storage_json_builder_appendf(&json, "\"protocol_version\":%d,", INBE_SYNC_PROTOCOL_VERSION);
    storage_json_builder_append(&json, "\"app_id\":\"inbe\",");
    storage_json_builder_append(&json, "\"client_capabilities\":[\"v4-encrypted-records\","
                       "\"v4-dual-write-transition\","
                       "\"v5-dual-read\","
                       "\"v5-legacy-encrypted-collections\"],");
    storage_json_builder_append(&json, "\"include_legacy_data\":true,");
    storage_json_builder_append_key_string(&json, "user_id_hash", user_id_hash);
    storage_json_builder_append(&json, ",");
    storage_json_builder_append_key_string(&json, "client_id", storage_sync_client_id());
    storage_json_builder_appendf(&json, ",\"since_server_version\":%lld", since_server_version);
    storage_json_builder_appendf(&json, ",\"client_clock\":%lld", get_meta_int64(STORAGE_SYNC_SERVER_CLOCK_KEY, 0));
    if(get_meta_int64(STORAGE_SYNC_FULL_REPLACE_KEY, 0) != 0)
        storage_json_builder_append(&json, ",\"full_sync_requested\":true");
    {
        const char *last_server_hash = get_meta_text(STORAGE_SYNC_LAST_SERVER_HASH_KEY);
        if(!force_zero_day_repair && last_server_hash != NULL && last_server_hash[0] != '\0') {
            storage_json_builder_append(&json, ",");
            storage_json_builder_append_key_string(&json, "last_server_state_hash", last_server_hash);
        }
    }
    if(since_server_version <= 0 || !full_upload_done)
        storage_json_builder_append(&json, ",\"bootstrap\":true");
    if(public_key_hex != NULL && public_key_hex[0] != '\0') {
        storage_json_builder_append(&json, ",");
        storage_json_builder_append_key_string(&json, "public_key", public_key_hex);
    }
    storage_json_builder_append(&json, ",\"habits\":[]");
    storage_json_builder_append(&json, ",\"habit_days\":[]");
    storage_json_builder_append(&json, ",\"sessions\":[]");
    storage_json_builder_append(&json, ",");
    storage_append_sync_ops_json(&json, through_seq);
    storage_json_builder_append(&json, ",");
    storage_append_encrypted_records_json(&json, through_seq);
    storage_json_builder_append(&json, "}");

    if(!json.ok || json.data == NULL) {
        free(json.data);
        return NULL;
    }
    TraceLog(LOG_INFO,
             "SYNC: payload since=%lld bootstrap=%d outbox_through=%lld "
             "ops=%d",
             since_server_version, since_server_version <= 0 || !full_upload_done, through_seq,
             storage_json_array_count(json.data, "$.ops"));
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
    return db_select_int64("SELECT COALESCE(MAX(seq),0) FROM sync_outbox", 0);
}

static long long
storage_sync_outbox_batch_seq(int limit)
{
    sqlite3_stmt *stmt = NULL;
    long long seq = 0;

    if(g_storage.db == NULL)
        return 0;
    if(limit <= 0)
        return storage_max_sync_outbox_seq();
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT seq FROM sync_outbox ORDER BY seq LIMIT 1 OFFSET ?1",
                          -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, limit - 1);
        if(sqlite3_step(stmt) == SQLITE_ROW)
            seq = sqlite3_column_int64(stmt, 0);
    }
    if(stmt != NULL)
        sqlite3_finalize(stmt);
    if(seq > 0)
        return seq;
    return storage_max_sync_outbox_seq();
}

static int
storage_has_pending_sync_outbox(void)
{
    return db_select_int64("SELECT EXISTS(SELECT 1 FROM sync_outbox LIMIT 1)", 0) != 0;
}

static int
storage_has_pending_encrypted_shadow_migration(void)
{
    if(!storage_has_sync_account())
        return 0;
    return get_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_KEY, 0) == 0;
}

static long long
storage_encrypted_shadow_queue_count(void)
{
    if(g_storage.db == NULL)
        return 0;
    return db_select_int64(
        "SELECT COUNT(*) FROM sync_outbox "
        "WHERE entity_type IN ('habit','habit_day','session')",
        0);
}

static long long
storage_encrypted_shadow_source_count(void)
{
    if(g_storage.db == NULL)
        return 0;
    return db_select_int64(
        "SELECT "
        "(SELECT COUNT(*) FROM habits "
        " WHERE user_id=(SELECT id FROM users LIMIT 1)) + "
        "(SELECT COUNT(*) FROM habit_days hd "
        " JOIN habits h ON h.id=hd.habit_id "
        " WHERE h.user_id=(SELECT id FROM users LIMIT 1)) + "
        "(SELECT COUNT(*) FROM sessions "
        " WHERE user_id=(SELECT id FROM users LIMIT 1))",
        0);
}

static void
storage_prepare_encrypted_shadow_migration_queue(void)
{
    long long total;
    long long queued;

    if(g_storage.db == NULL || !storage_has_pending_encrypted_shadow_migration())
        return;
    storage_enqueue_all_sync_state();
    total = storage_encrypted_shadow_source_count();
    queued = storage_encrypted_shadow_queue_count();
    if(total < queued)
        total = queued;
    set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_TOTAL_KEY, total);
    set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_QUEUED_KEY, 1);
}

static void
storage_clear_completed_encrypted_shadow_outbox_if_stale(void)
{
    long long outbox_count;
    long long shadow_count;
    long long source_count;

    if(g_storage.db == NULL ||
       get_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_KEY, 0) == 0 ||
       get_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_LEGACY_SEEN_KEY, 0) == 0)
        return;

    outbox_count = db_select_int64("SELECT COUNT(*) FROM sync_outbox", 0);
    if(outbox_count <= 0) {
        set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_LEGACY_SEEN_KEY, 0);
        set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_QUEUED_KEY, 0);
        set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_TOTAL_KEY, 0);
        return;
    }

    shadow_count = storage_encrypted_shadow_queue_count();
    source_count = storage_encrypted_shadow_source_count();
    if(outbox_count != shadow_count || shadow_count != source_count)
        return;

    if(exec_sql("DELETE FROM sync_outbox "
                "WHERE entity_type IN ('habit','habit_day','session')")) {
        set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_LEGACY_SEEN_KEY, 0);
        set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_QUEUED_KEY, 0);
        set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_TOTAL_KEY, 0);
    }
}

static int
storage_has_orphan_habit_days(void)
{
    return db_select_int64(
               "SELECT EXISTS(SELECT 1 FROM habit_days hd "
               "WHERE hd.habit_id<>'' "
               "AND NOT EXISTS (SELECT 1 FROM habits h WHERE h.id=hd.habit_id) "
               "LIMIT 1)",
               0) != 0;
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
storage_sync_response_has_legacy_records(const char *response_json)
{
    static const char *const paths[] = {
        "$.changes.habits",
        "$.changes.habit_days",
        "$.changes.sessions",
        "$.changes.session_rounds",
        "$.changes.meditation_logs",
        "$.changes.social_cache",
        "$.data.habits",
        "$.data.habit_days",
        "$.data.sessions",
        "$.data.session_rounds",
        "$.data.meditation_logs",
        "$.data.social_cache",
    };

    for(size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        if(storage_json_array_has_items(response_json, paths[i]))
            return 1;
    }
    return 0;
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

int
storage_json_array_count_path(const char *json, const char *path)
{
    return storage_json_array_count(json, path);
}

int
storage_json_array_object_text(const char *json, const char *array_path,
                               int index, const char *key,
                               char *out, size_t out_size)
{
    char path[128];

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(array_path == NULL || key == NULL || index < 0)
        return 0;
    if(snprintf(path, sizeof(path), "%s[%d].%s", array_path, index, key) >=
       (int)sizeof(path))
        return 0;
    return storage_json_extract_text(json, path, out, out_size);
}

double
storage_json_array_object_number(const char *json, const char *array_path,
                                 int index, const char *key)
{
    char path[128];
    sqlite3_stmt *stmt = NULL;
    double value = 0.0;

    if(g_storage.db == NULL || json == NULL || array_path == NULL ||
       key == NULL || index < 0)
        return 0.0;
    if(snprintf(path, sizeof(path), "%s[%d].%s", array_path, index, key) >=
       (int)sizeof(path))
        return 0.0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT CAST(COALESCE(json_extract(?1,?2),0) AS REAL)",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0.0;
    bind_text(stmt, 1, json);
    bind_text(stmt, 2, path);
    if(sqlite3_step(stmt) == SQLITE_ROW)
        value = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);
    return value;
}

static int
storage_sync_response_has_changes(const char *json)
{
    return storage_json_array_has_items(json, "$.changes.habits") ||
           storage_json_array_has_items(json, "$.changes.habit_days") ||
           storage_json_array_has_items(json, "$.changes.sessions") ||
           storage_json_array_has_items(json, "$.changes.meditation_logs") ||
           storage_json_array_has_items(json, "$.data.habits") ||
           storage_json_array_has_items(json, "$.data.habit_days") ||
           storage_json_array_has_items(json, "$.data.sessions") ||
           storage_json_array_has_items(json, "$.data.meditation_logs") ||
           storage_json_array_has_items(json, "$.data.social") ||
           storage_json_array_has_items(json, "$.changes.social_cache");
}

int
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
storage_apply_sync_sessions_json(const char *response_json)
{
    static const char *sessions_sql =
        "INSERT INTO "
        "sessions(id,user_id,started_at,local_date,topic,activity,source,"
        "imported_at,rounds_hash,mood_before,mood_after,energy,stress,note,tags,"
        "deleted_at,updated_at) "
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
        "       CAST(COALESCE(json_extract(value,'$.mood_before'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.mood_after'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.energy'),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.stress'),0) AS INTEGER),"
        "       COALESCE(json_extract(value,'$.note'),''),"
        "       COALESCE(json_extract(value,'$.tags'),''),"
        "       CAST(COALESCE(json_extract(value,'$.deleted_at'),0) AS INTEGER),"
        "       "
        "CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),"
        "strftime('%s',json_extract(value,'$.started_at')),'0') AS INTEGER) "
        "FROM (SELECT value FROM json_each(?1,'$.changes.sessions') "
        "      UNION ALL SELECT value FROM json_each(?1,'$.data.sessions')) "
        "WHERE COALESCE(json_extract(value,'$.id'),'')<>'' "
        "ON CONFLICT(id) DO UPDATE SET "
        " user_id=excluded.user_id,started_at=excluded.started_at,local_date="
        "excluded.local_date,"
        " topic=excluded.topic,activity=excluded.activity,source=excluded.source,"
        " imported_at=excluded.imported_at,rounds_hash=excluded.rounds_hash,"
        " mood_before=excluded.mood_before,mood_after=excluded.mood_after,"
        " energy=excluded.energy,stress=excluded.stress,note=excluded.note,"
        " tags=excluded.tags,"
        "deleted_at=excluded.deleted_at,"
        " updated_at=excluded.updated_at "
        "WHERE excluded.updated_at > sessions.updated_at "
        "OR (excluded.updated_at = sessions.updated_at AND NOT EXISTS ("
        " SELECT 1 FROM sync_outbox "
        " WHERE entity_type='session' AND entity_id=sessions.id AND local_date=0"
        "))";

    return storage_exec_json_user_sql(sessions_sql, response_json);
}

static int
storage_apply_sync_session_rounds_json(const char *response_json)
{
    static const char *delete_rounds_sql =
        "DELETE FROM session_rounds WHERE session_id IN ("
        " SELECT COALESCE(json_extract(value,'$.id'),'') "
        " FROM (SELECT value FROM json_each(?1,'$.changes.sessions') "
        "       UNION ALL SELECT value FROM json_each(?1,'$.data.sessions')) "
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
        "FROM (SELECT value FROM json_each(?1,'$.changes.sessions') "
        "      UNION ALL SELECT value FROM json_each(?1,'$.data.sessions')) AS s, "
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

    return storage_exec_json_user_sql(delete_rounds_sql, response_json) &&
           storage_exec_json_user_sql(rounds_sql, response_json);
}

static int
storage_apply_sync_meditation_logs_json(const char *response_json)
{
    static const char *meditation_logs_sql =
        "INSERT INTO "
        "meditation_logs(id,user_id,session_id,duration_seconds,completed_at,updated_at) "
        "SELECT COALESCE(json_extract(value,'$.id'),''),?2,"
        "       COALESCE(json_extract(value,'$.session_id'),''),"
        "       CAST(COALESCE(json_extract(value,'$.duration_seconds'),"
        "                     json_extract(value,'$.duration'),0) AS INTEGER),"
        "       CAST(COALESCE(strftime('%s',json_extract(value,'$.completed_at')),"
        "                     strftime('%s',json_extract(value,'$.timestamp')),'0') AS INTEGER),"
        "       CAST(COALESCE(strftime('%s',json_extract(value,'$.completed_at')),"
        "                     strftime('%s',json_extract(value,'$.timestamp')),'0') AS INTEGER) "
        "FROM (SELECT value FROM json_each(?1,'$.changes.meditation_logs') "
        "      UNION ALL SELECT value FROM json_each(?1,'$.data.meditation_logs')) "
        "WHERE COALESCE(json_extract(value,'$.id'),'')<>'' "
        "ON CONFLICT(id) DO UPDATE SET "
        " user_id=excluded.user_id,session_id=excluded.session_id,"
        " duration_seconds=excluded.duration_seconds,"
        " completed_at=excluded.completed_at,updated_at=excluded.updated_at "
        "WHERE excluded.updated_at > meditation_logs.updated_at";

    return storage_exec_json_user_sql(meditation_logs_sql, response_json);
}

static int
storage_apply_sync_social_json(const char *response_json)
{
    static const char *social_cache_sql =
        "INSERT INTO social_snapshots(user_id,kind,json,updated_at) "
        "SELECT ?2,"
        "       COALESCE(json_extract(value,'$.kind'),''),"
        "       json(COALESCE(json_extract(value,'$.json'),'{}')),"
        "       CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),'0') AS INTEGER) "
        "FROM json_each(?1,'$.changes.social_cache') "
        "WHERE COALESCE(json_extract(value,'$.kind'),'')<>'' "
        "ON CONFLICT(user_id,kind) DO UPDATE SET "
        " json=excluded.json,updated_at=excluded.updated_at";
    static const char *social_sql =
        "INSERT INTO social_snapshots(user_id,kind,json,updated_at) "
        "SELECT ?2,"
        "       COALESCE(json_extract(value,'$.kind'),''),"
        "       json(COALESCE(json_extract(value,'$.json'),'{}')),"
        "       CAST(COALESCE(strftime('%s',json_extract(value,'$.updated_at')),'0') AS INTEGER) "
        "FROM json_each(?1,'$.data.social') "
        "WHERE COALESCE(json_extract(value,'$.kind'),'')<>'' "
        "ON CONFLICT(user_id,kind) DO UPDATE SET "
        " json=excluded.json,updated_at=excluded.updated_at";

    return storage_exec_json_user_sql(social_sql, response_json) &&
           storage_exec_json_user_sql(social_cache_sql, response_json);
}

int
storage_apply_sync_response_json(const char *response_json)
{
    long long server_version;
    long long server_clock;
    long long old_server_version;
    long long latest_protocol;
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
    latest_protocol = storage_json_extract_int64(response_json, "$.latest_protocol", 0);
    if(latest_protocol > 0)
        set_meta_int64(STORAGE_SYNC_LATEST_PROTOCOL_KEY, latest_protocol);
    if(storage_json_extract_int64(response_json, "$.full_snapshot_required", 0) != 0 &&
       get_meta_int64(STORAGE_SYNC_APPLY_REVIEW_KEY, 0) == 0) {
        if(!storage_has_any())
            return storage_apply_remote_full_snapshot(response_json);
        if(storage_has_pending_sync_outbox()) {
            TraceLog(LOG_INFO, "SYNC: merging full snapshot with pending local edits");
        } else {
            if(storage_sync_review_json_should_auto_apply_remote(response_json))
                return storage_apply_remote_full_snapshot(response_json);
            if(!storage_sync_review_write_json(response_json))
                return 0;
            set_meta(STORAGE_SYNC_PENDING_REVIEW_KEY, "1");
            g_storage.pending_sync_outbox_seq = 0;
            storage_schedule_persist();
            TraceLog(LOG_WARNING, "SYNC: full snapshot review required");
            return 1;
        }
    }
    old_server_version = get_meta_int64("sync_last_server_version", 0);
    if(!exec_sql("SAVEPOINT inbe_sync_apply"))
        return 0;
    storage_clear_uploaded_outbox(g_storage.pending_sync_outbox_seq);
    if(!storage_reconcile_remote_habit_ids(response_json) ||
       !storage_apply_sync_habits_json(response_json) ||
       !storage_apply_sync_habit_days_json(response_json) ||
       !storage_merge_duplicate_habit_names() ||
       !storage_apply_sync_session_rounds_json(response_json) ||
       !storage_apply_sync_sessions_json(response_json) ||
       !storage_apply_sync_meditation_logs_json(response_json) ||
       !storage_apply_sync_social_json(response_json)) {
        exec_sql("ROLLBACK TO inbe_sync_apply");
        exec_sql("RELEASE inbe_sync_apply");
        return 0;
    }
    if(!exec_sql("RELEASE inbe_sync_apply")) {
        return 0;
    }
    storage_materialize_session_habit_days();
    server_version = storage_json_extract_int64(response_json, "$.server_version", 0);
    server_clock = storage_json_extract_int64(response_json, "$.server_clock", 0);
    TraceLog(LOG_INFO,
             "SYNC: response server_version=%lld old_server_version=%lld "
             "habits=%d habit_days=%d sessions=%d",
             server_version, old_server_version,
             storage_json_array_count(response_json, "$.changes.habits") +
                 storage_json_array_count(response_json, "$.data.habits"),
             storage_json_array_count(response_json, "$.changes.habit_days") +
                 storage_json_array_count(response_json, "$.data.habit_days"),
             storage_json_array_count(response_json, "$.changes.sessions") +
                 storage_json_array_count(response_json, "$.data.sessions"));
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
    if((storage_json_extract_int64(response_json, "$.latest_protocol", 0) >= 4 ||
        storage_json_extract_int64(response_json, "$.protocol_version", 0) >= 4 ||
        storage_json_array_has_items(response_json, "$.changes.encrypted_records") ||
        storage_json_array_has_items(response_json, "$.data.encrypted_records")) &&
       !storage_has_pending_sync_outbox()) {
        if(storage_sync_response_has_legacy_records(response_json) &&
           get_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_LEGACY_SEEN_KEY, 0) == 0) {
            set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_LEGACY_SEEN_KEY, 1);
            set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_QUEUED_KEY, 0);
            storage_prepare_encrypted_shadow_migration_queue();
        } else {
            set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_KEY, 1);
            set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_LEGACY_SEEN_KEY, 0);
            set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_QUEUED_KEY, 0);
            set_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_TOTAL_KEY, 0);
        }
    }
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
    static const char *const sqls[] = {"DELETE FROM meditation_logs",
                                       "DELETE FROM session_rounds",
                                       "DELETE FROM sessions",
                                       "DELETE FROM habit_days",
                                       "DELETE FROM habits",
                                       "DELETE FROM social_snapshots",
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
    if(status == NULL)
        return 0;
    memset(status, 0, sizeof(*status));
    if(g_storage.db == NULL)
        return 0;

    status->has_account = storage_has_sync_account();
    status->server_connected = storage_sync_server_connected();
    status->review_pending = get_meta_int64(STORAGE_SYNC_PENDING_REVIEW_KEY, 0) != 0;
    status->repair_pending = get_meta_int64(STORAGE_SYNC_ZERO_HABIT_DAY_REPAIR_KEY, 0) == 0;
    status->secure_migration_pending = storage_has_pending_encrypted_shadow_migration();
    status->full_upload_done = get_meta_int64("sync_full_upload_done", 0) != 0;
    status->server_version = get_meta_int64("sync_last_server_version", 0);
    status->server_clock = get_meta_int64(STORAGE_SYNC_SERVER_CLOCK_KEY, 0);
    status->latest_protocol = (int)get_meta_int64(STORAGE_SYNC_LATEST_PROTOCOL_KEY,
                                                  INBE_SYNC_PROTOCOL_VERSION);
    status->protocol_upgrade_available = status->latest_protocol > INBE_SYNC_PROTOCOL_VERSION;
    storage_clear_completed_encrypted_shadow_outbox_if_stale();
    status->queued_changes = db_select_int64("SELECT COUNT(*) FROM sync_outbox", 0);
    status->secure_migration_queued = storage_encrypted_shadow_queue_count();
    status->secure_migration_total = get_meta_int64(STORAGE_SYNC_ENCRYPTED_SHADOW_TOTAL_KEY, 0);
    if(status->secure_migration_pending) {
        if(status->secure_migration_total <= 0)
            status->secure_migration_total = storage_encrypted_shadow_source_count();
        if(status->secure_migration_total < status->secure_migration_queued)
            status->secure_migration_total = status->secure_migration_queued;
        status->secure_migration_done = status->secure_migration_total -
                                        status->secure_migration_queued;
        if(status->secure_migration_done < 0)
            status->secure_migration_done = 0;
        if(status->secure_migration_done > status->secure_migration_total)
            status->secure_migration_done = status->secure_migration_total;
    }
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

static int
storage_profile_date_from_epoch(long long epoch)
{
    time_t value = (time_t)epoch;
    struct tm *tm_value;

    if(epoch <= 0)
        return 0;
    tm_value = gmtime(&value);
    if(tm_value == NULL)
        return 0;
    return (tm_value->tm_year + 1900) * 10000 +
           (tm_value->tm_mon + 1) * 100 + tm_value->tm_mday;
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

    if(activity == 1 &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT completed_at FROM meditation_logs "
                          "WHERE user_id=?1 AND duration_seconds>0",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, g_storage.user_id);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            int local_date = storage_profile_date_from_epoch(sqlite3_column_int64(stmt, 0));
            int offset = storage_profile_day_offset(local_date, today_date);
            if(offset >= 0 && offset <= 370)
                seen[offset] = 1;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

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
                              "WITH session_totals AS ("
                              "  SELECT s.id, SUM(sr.seconds) AS seconds "
                              "  FROM sessions s JOIN session_rounds sr ON sr.session_id=s.id "
                              "  WHERE s.user_id=?1 AND s.deleted_at=0 AND s.activity=?2 "
                              "    AND sr.seconds>0 "
                              "  GROUP BY s.id"
                              "),"
                              "log_totals AS ("
                              "  SELECT ml.session_id AS id, ml.duration_seconds AS seconds "
                              "  FROM meditation_logs ml "
                              "  WHERE ml.user_id=?1 AND ml.duration_seconds>0 "
                              "    AND NOT EXISTS (SELECT 1 FROM session_totals st "
                              "                    WHERE st.id=ml.session_id)"
                              "),"
                              "all_totals AS ("
                              "  SELECT seconds FROM session_totals "
                              "  UNION ALL "
                              "  SELECT seconds FROM log_totals"
                              ") "
                              "SELECT COALESCE(AVG(seconds),0) FROM all_totals",
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
storage_profile_week_stats(int today_date, int *active_days_out,
                           int *practice_sessions_out)
{
    sqlite3_stmt *stmt = NULL;
    int active_days[7] = {0};
    int practice_sessions = 0;
    int active_count = 0;

    if(active_days_out != NULL)
        *active_days_out = 0;
    if(practice_sessions_out != NULL)
        *practice_sessions_out = 0;
    if(g_storage.db == NULL || today_date <= 0)
        return 0;

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT local_date FROM sessions "
                          "WHERE user_id=?1 AND deleted_at=0 AND local_date>0",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, g_storage.user_id);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            int offset = storage_profile_day_offset(sqlite3_column_int(stmt, 0),
                                                    today_date);
            if(offset >= 0 && offset < 7) {
                active_days[offset] = 1;
                practice_sessions++;
            }
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT DISTINCT hd.local_date FROM habit_days hd "
                          "JOIN habits h ON h.id=hd.habit_id "
                          "WHERE h.user_id=?1 AND h.deleted_at=0 "
                          "AND (hd.completed!=0 OR hd.count>0)",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, g_storage.user_id);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            int offset = storage_profile_day_offset(sqlite3_column_int(stmt, 0),
                                                    today_date);
            if(offset >= 0 && offset < 7)
                active_days[offset] = 1;
        }
        sqlite3_finalize(stmt);
    }

    for(int i = 0; i < 7; i++)
        active_count += active_days[i];
    if(active_days_out != NULL)
        *active_days_out = active_count;
    if(practice_sessions_out != NULL)
        *practice_sessions_out = practice_sessions;
    return 1;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

int
storage_export_sessions_csv(const char *path)
{
    static const char *query =
        "SELECT s.local_date, s.activity, s.source, "
        "COALESCE((SELECT SUM(r.seconds) FROM session_rounds r "
        "          WHERE r.session_id = s.id), 0), "
        "COALESCE((SELECT COUNT(*) FROM session_rounds r "
        "          WHERE r.session_id = s.id), 0), "
        "COALESCE((SELECT m.duration_seconds FROM meditation_logs m "
        "          WHERE m.session_id = s.id), 0) "
        "FROM sessions s WHERE s.deleted_at IS NULL "
        "ORDER BY s.local_date, s.started_at";
    static const char *topic_names[] = {"wim_hof", "meditation",
                                        "sun_salutation", "patterns"};
    FILE *file;
    sqlite3_stmt *stmt;
    int ok = 0;

    if(path == NULL || path[0] == '\0' || g_storage.db == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, query, -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    file = fopen(path, "wb");
    if(file == NULL) {
        sqlite3_finalize(stmt);
        return 0;
    }
    fputs("date,practice,source,rounds,total_hold_seconds,"
          "meditation_seconds\n", file);
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *date = sqlite3_column_text(stmt, 0);
        int topic = sqlite3_column_int(stmt, 1);
        const unsigned char *source = sqlite3_column_text(stmt, 2);
        const char *topic_name = topic >= 0 &&
            (size_t)topic < sizeof(topic_names) / sizeof(topic_names[0])
            ? topic_names[topic] : "other";
        if(sqlite3_column_int(stmt, 5) > 0)
            topic_name = "meditation";

        fprintf(file, "%s,%s,%s,%d,%d,%d\n",
                date != NULL ? (const char *)date : "",
                topic_name,
                source != NULL ? (const char *)source : "",
                sqlite3_column_int(stmt, 4),
                sqlite3_column_int(stmt, 3),
                sqlite3_column_int(stmt, 5));
    }
    ok = ferror(file) == 0;
    fclose(file);
    sqlite3_finalize(stmt);
    return ok;
}

/* Minimal session list for Health Connect: epoch start/end plus the
 * activity id. Duration prefers meditation_logs (activity 1) and falls
 * back to the sum of session_rounds (breathwork, sun salutation,
 * patterns). */
int
storage_export_health_connect_csv(const char *path)
{
    static const char *query =
        "SELECT s.started_at, s.activity, "
        "COALESCE((SELECT m.duration_seconds FROM meditation_logs m "
        "          WHERE m.session_id = s.id), "
        "(SELECT SUM(r.seconds) FROM session_rounds r "
        "          WHERE r.session_id = s.id), 0) "
        "FROM sessions s WHERE s.deleted_at IS NULL "
        "ORDER BY s.started_at";
    FILE *file;
    sqlite3_stmt *stmt;
    int rows = 0;

    if(path == NULL || path[0] == '\0' || g_storage.db == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, query, -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    file = fopen(path, "wb");
    if(file == NULL) {
        sqlite3_finalize(stmt);
        return 0;
    }
    fputs("start,end,practice\n", file);
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        long long start = sqlite3_column_int64(stmt, 0);
        int activity = sqlite3_column_int(stmt, 1);
        long long duration = sqlite3_column_int64(stmt, 2);

        if(start <= 0 || duration <= 0)
            continue;
        fprintf(file, "%lld,%lld,%d\n", start, start + duration, activity);
        rows++;
    }
    if(ferror(file) != 0)
        rows = 0;
    fclose(file);
    sqlite3_finalize(stmt);
    return rows > 0;
}
