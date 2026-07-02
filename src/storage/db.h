#ifndef INBE_STORAGE_DB_H
#define INBE_STORAGE_DB_H

#include "storage.h"

#include <sqlite3.h>
#include <stddef.h>

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

extern StorageState g_storage;

long long now_seconds(void);
int bind_text(sqlite3_stmt *stmt, int index, const char *text);
int storage_join_path(char *out, size_t out_size, const char *root, const char *name);
int path_exists(const char *path);
int ensure_dir_local(const char *path);
int exec_sql(const char *sql);
int table_has_column(const char *table, const char *column);
int table_exists(const char *table);
int schema_create(void);
int migrate_schema(void);
int load_or_create_user(void);
int meta_equals(const char *key, const char *value);
const char *get_meta_text(const char *key);
void set_meta(const char *key, const char *value);
long long get_meta_int64(const char *key, long long fallback);
void set_meta_int64(const char *key, long long value);

void storage_schedule_persist(void);
void storage_enqueue_all_sync_state(void);
int storage_enqueue_sync_habit(const char *habit_id);
int storage_enqueue_sync_habit_day(const char *habit_id, int local_date);
int storage_enqueue_sync_session(const char *session_id);
int storage_has_sync_account(void);
int storage_materialize_session_habit_days(void);
int storage_migrate_default_habit_ids(void);
int storage_migrate_habit_ids_to_uuid(void);
int storage_sync_review_write_json(const char *json);
void storage_sync_review_delete_json(void);
int insert_session_at_ex(long long started_at, int local_date, const int *round_times,
                         int round_count, int topic, int activity, const char *source,
                         char *out_id, size_t out_id_size);

#endif
