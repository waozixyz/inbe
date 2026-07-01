#include "db.h"

#include "import.h"

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(_WIN32)
#define INBE_MKDIR(path) mkdir(path)
#else
#define INBE_MKDIR(path) mkdir(path, 0700)
#endif

StorageState g_storage;

static void
make_user_id(char *out, size_t out_size)
{
    unsigned int r = (unsigned int)rand();
    snprintf(out, out_size, "local-%lld-%u", now_seconds(), r);
}

int
storage_join_path(char *out, size_t out_size, const char *root, const char *name)
{
    size_t root_len;
    size_t name_len;

    if(out == NULL || out_size == 0 || root == NULL || root[0] == '\0' || name == NULL)
        return 0;
    root_len = strlen(root);
    name_len = strlen(name);
    if(root_len + 1 + name_len + 1 > out_size)
        return 0;
    memcpy(out, root, root_len);
    out[root_len] = '/';
    memcpy(out + root_len + 1, name, name_len + 1);
    return 1;
}

int
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

int
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

int
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

int
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

int
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

long long
now_seconds(void)
{
    return (long long)time(NULL);
}

int
bind_text(sqlite3_stmt *stmt, int index, const char *text)
{
    return sqlite3_bind_text(stmt, index, text != NULL ? text : "", -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

int
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
        "CREATE TABLE IF NOT EXISTS social_cache("
        " user_id TEXT NOT NULL,"
        " kind TEXT NOT NULL,"
        " json TEXT NOT NULL,"
        " updated_at INTEGER NOT NULL,"
        " PRIMARY KEY(user_id,kind)"
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
        " description TEXT NOT NULL DEFAULT '',"
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
        "CREATE TABLE IF NOT EXISTS sync_ops("
        " op_id TEXT PRIMARY KEY,"
        " client_id TEXT NOT NULL,"
        " seq INTEGER NOT NULL,"
        " entity_type TEXT NOT NULL,"
        " entity_id TEXT NOT NULL,"
        " local_date INTEGER NOT NULL DEFAULT 0,"
        " op_type TEXT NOT NULL,"
        " payload_json TEXT NOT NULL DEFAULT '',"
        " created_at INTEGER NOT NULL,"
        " sent_at INTEGER NOT NULL DEFAULT 0,"
        " acked_at INTEGER NOT NULL DEFAULT 0"
        ");"
        "INSERT OR IGNORE INTO meta(key,value) VALUES('schema_version','1');");
}

int
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

int
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

const char *
get_meta_text(const char *key)
{
    sqlite3_stmt *stmt = NULL;
    g_storage.text_value[0] = '\0';

    if(g_storage.db == NULL || key == NULL)
        return NULL;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT value FROM meta WHERE key=?1", -1, &stmt, NULL) != SQLITE_OK)
        return NULL;
    bind_text(stmt, 1, key);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *text = (const char *)sqlite3_column_text(stmt, 0);
        snprintf(g_storage.text_value, sizeof(g_storage.text_value), "%s", text != NULL ? text : "");
    }
    sqlite3_finalize(stmt);
    return g_storage.text_value[0] != '\0' ? g_storage.text_value : NULL;
}

void
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

long long
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

void
set_meta_int64(const char *key, long long value)
{
    char text[32];
    snprintf(text, sizeof(text), "%lld", value);
    set_meta(key, text);
}

int
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
            " description TEXT NOT NULL DEFAULT '',"
            " color_r INTEGER NOT NULL,"
            " color_g INTEGER NOT NULL,"
            " color_b INTEGER NOT NULL,"
            " sync_mode INTEGER NOT NULL,"
            " sync_activity INTEGER NOT NULL,"
            " sort_order INTEGER NOT NULL,"
            " deleted_at INTEGER NOT NULL DEFAULT 0,"
            " updated_at INTEGER NOT NULL DEFAULT 0"
            ");"
            "INSERT INTO habits(id,user_id,name,description,color_r,color_g,color_b,sync_mode,sync_activity,sort_order,deleted_at,updated_at)"
            " SELECT id,user_id,name,'',color_r,color_g,color_b,sync_mode,sync_activity,sort_order,deleted_at,0"
            " FROM habits_with_sync_topic;"
            "DROP TABLE habits_with_sync_topic;"
            "COMMIT;"))
            return 0;
    }
    if(!table_has_column("habits", "description") &&
       !exec_sql("ALTER TABLE habits ADD COLUMN description TEXT NOT NULL DEFAULT ''"))
        return 0;
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
           ");"
           "CREATE TABLE IF NOT EXISTS sync_ops("
           " op_id TEXT PRIMARY KEY,"
           " client_id TEXT NOT NULL,"
           " seq INTEGER NOT NULL,"
           " entity_type TEXT NOT NULL,"
           " entity_id TEXT NOT NULL,"
           " local_date INTEGER NOT NULL DEFAULT 0,"
           " op_type TEXT NOT NULL,"
           " payload_json TEXT NOT NULL DEFAULT '',"
           " created_at INTEGER NOT NULL,"
           " sent_at INTEGER NOT NULL DEFAULT 0,"
           " acked_at INTEGER NOT NULL DEFAULT 0"
           ");"
           "CREATE TABLE IF NOT EXISTS social_cache("
           " user_id TEXT NOT NULL,"
           " kind TEXT NOT NULL,"
           " json TEXT NOT NULL,"
           " updated_at INTEGER NOT NULL,"
           " PRIMARY KEY(user_id,kind)"
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

int
storage_init(const char *root)
{
    if(root == NULL || root[0] == '\0')
        return 0;
    g_storage.pending_sync_outbox_seq = 0;
    g_storage.last_sync_changed = 0;
    snprintf(g_storage.root, sizeof(g_storage.root), "%s", root);
    ensure_dir_local(g_storage.root);
    if(!storage_join_path(g_storage.db_path, sizeof(g_storage.db_path),
                          g_storage.root, "inbe.db")) {
        TraceLog(LOG_ERROR, "STORAGE: database path is too long");
        return 0;
    }
    if(sqlite3_open(g_storage.db_path, &g_storage.db) != SQLITE_OK) {
        TraceLog(LOG_ERROR, "STORAGE: failed to open %s", g_storage.db_path);
        return 0;
    }
    sqlite3_busy_timeout(g_storage.db, 1000);
    if(!schema_create() || !migrate_schema() || !load_or_create_user())
        return 0;
    storage_migrate_default_habit_ids();
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
    g_storage.pending_sync_outbox_seq = 0;
    g_storage.last_sync_changed = 0;
}

const char *
storage_db_path(void)
{
    return g_storage.db_path;
}
