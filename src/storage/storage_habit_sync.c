#include "storage.h"

#include "db.h"
#include "screens/habits_screen.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#define STORAGE_DEFAULT_HABIT_ID_MIGRATION_KEY "default_habit_id_migration_v1_done"
#define STORAGE_HABIT_UUID_MIGRATION_KEY "habit_uuid_migration_v1_done"
#define STORAGE_ACTIVITY_SUN_SALUTATION_MASK (1 << 2)

static int
storage_is_uuid(const char *id)
{
    if(id == NULL || strlen(id) != 36)
        return 0;
    for(int i = 0; i < 36; i++) {
        char ch = id[i];
        if(i == 8 || i == 13 || i == 18 || i == 23) {
            if(ch != '-')
                return 0;
        } else if(!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') ||
                    (ch >= 'A' && ch <= 'F'))) {
            return 0;
        }
    }
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

int
storage_reconcile_remote_habit_ids(const char *response_json)
{
    static const char *remote_map_sql =
        "INSERT OR IGNORE INTO inbe_remote_habit_id_map(old_id,new_id) "
        "SELECT h.id,COALESCE(json_extract(r.value,'$.id'),'') "
        "FROM (SELECT value FROM json_each(?1,'$.changes.habits') "
        "      UNION ALL SELECT value FROM json_each(?1,'$.data.habits')) AS r "
        "JOIN habits h ON h.name=COALESCE(json_extract(r.value,'$.name'),'') "
        "WHERE h.deleted_at=0 "
        "  AND CAST(COALESCE(json_extract(r.value,'$.deleted_at'),0) AS INTEGER)=0 "
        "  AND COALESCE(json_extract(r.value,'$.id'),'')<>'' "
        "  AND h.id<>COALESCE(json_extract(r.value,'$.id'),'')";

    if(response_json == NULL)
        return 0;
    if(!exec_sql("DROP TABLE IF EXISTS temp.inbe_remote_habit_id_map;"
                 "CREATE TEMP TABLE inbe_remote_habit_id_map("
                 " old_id TEXT PRIMARY KEY,"
                 " new_id TEXT NOT NULL"
                 ")"))
        return 0;
    if(!storage_exec_json_user_sql(remote_map_sql, response_json))
        return 0;
    if(!exec_sql("UPDATE habit_days "
                 "SET completed=MAX(completed,COALESCE((SELECT d.completed FROM habit_days d "
                 "    JOIN inbe_remote_habit_id_map m ON m.old_id=d.habit_id "
                 "    WHERE m.new_id=habit_days.habit_id "
                 "      AND d.local_date=habit_days.local_date),0)),"
                 " count=MAX(count,COALESCE((SELECT d.count FROM habit_days d "
                 "    JOIN inbe_remote_habit_id_map m ON m.old_id=d.habit_id "
                 "    WHERE m.new_id=habit_days.habit_id "
                 "      AND d.local_date=habit_days.local_date),0)),"
                 " session_count=MAX(session_count,COALESCE((SELECT d.session_count FROM "
                 "habit_days d "
                 "    JOIN inbe_remote_habit_id_map m ON m.old_id=d.habit_id "
                 "    WHERE m.new_id=habit_days.habit_id "
                 "      AND d.local_date=habit_days.local_date),0)),"
                 " updated_at=MAX(updated_at,COALESCE((SELECT d.updated_at FROM habit_days d "
                 "    JOIN inbe_remote_habit_id_map m ON m.old_id=d.habit_id "
                 "    WHERE m.new_id=habit_days.habit_id "
                 "      AND d.local_date=habit_days.local_date),0)) "
                 "WHERE EXISTS (SELECT 1 FROM inbe_remote_habit_id_map m "
                 "    WHERE m.new_id=habit_days.habit_id)"))
        return 0;
    if(!exec_sql("INSERT INTO habit_days(habit_id,local_date,completed,count,session_count,"
                 "updated_at) "
                 "SELECT m.new_id,d.local_date,d.completed,d.count,d.session_count,d.updated_at "
                 "FROM habit_days d JOIN inbe_remote_habit_id_map m ON m.old_id=d.habit_id "
                 "WHERE NOT EXISTS (SELECT 1 FROM habit_days k "
                 "    WHERE k.habit_id=m.new_id AND k.local_date=d.local_date)"))
        return 0;
    if(!exec_sql("UPDATE OR IGNORE sync_outbox SET entity_id=(SELECT m.new_id FROM "
                 "inbe_remote_habit_id_map m WHERE m.old_id=sync_outbox.entity_id) "
                 "WHERE entity_type IN ('habit','habit_day') "
                 "AND EXISTS (SELECT 1 FROM inbe_remote_habit_id_map m "
                 "    WHERE m.old_id=sync_outbox.entity_id);"
                 "DELETE FROM sync_outbox WHERE entity_type IN ('habit','habit_day') "
                 "AND EXISTS (SELECT 1 FROM inbe_remote_habit_id_map m "
                 "    WHERE m.old_id=sync_outbox.entity_id);"
                 "UPDATE OR IGNORE sync_ops SET entity_id=(SELECT m.new_id FROM "
                 "inbe_remote_habit_id_map m WHERE m.old_id=sync_ops.entity_id) "
                 "WHERE entity_type IN ('habit','habit_day') "
                 "AND EXISTS (SELECT 1 FROM inbe_remote_habit_id_map m "
                 "    WHERE m.old_id=sync_ops.entity_id);"
                 "UPDATE settings SET value=(SELECT m.new_id FROM inbe_remote_habit_id_map m "
                 "    WHERE m.old_id=settings.value),updated_at=strftime('%s','now') "
                 "WHERE key='habits_selected_id' "
                 "AND EXISTS (SELECT 1 FROM inbe_remote_habit_id_map m "
                 "    WHERE m.old_id=settings.value);"
                 "DELETE FROM habit_days WHERE EXISTS (SELECT 1 FROM inbe_remote_habit_id_map m "
                 "    WHERE m.old_id=habit_days.habit_id);"
                 "DELETE FROM habits WHERE EXISTS (SELECT 1 FROM inbe_remote_habit_id_map m "
                 "    WHERE m.old_id=habits.id);"))
        return 0;
    return exec_sql("DROP TABLE IF EXISTS temp.inbe_remote_habit_id_map");
}

static int
storage_habit_exists(const char *habit_id)
{
    sqlite3_stmt *stmt = NULL;
    int exists = 0;

    if(habit_id == NULL || habit_id[0] == '\0')
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT 1 FROM habits WHERE id=?1 LIMIT 1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, habit_id);
    exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}

static int
storage_enqueue_habit_days_for_sync(const char *habit_id)
{
    sqlite3_stmt *stmt = NULL;
    int ok = 1;

    if(habit_id == NULL || habit_id[0] == '\0')
        return 1;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT local_date FROM habit_days WHERE habit_id=?1 "
                          "AND (completed!=0 OR count>0 OR session_count>0)",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, habit_id);
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        if(!storage_enqueue_sync_habit_day(habit_id, sqlite3_column_int(stmt, 0)))
            ok = 0;
    }
    sqlite3_finalize(stmt);
    return ok;
}

int
storage_migrate_default_habit_ids(void)
{
    sqlite3_stmt *stmt = NULL;
    int sync_activity = 0;
    int deleted_at = 0;
    int should_migrate;
    int has_canonical;
    int ok = 1;

    if(g_storage.db == NULL)
        return 0;
    if(get_meta_int64(STORAGE_DEFAULT_HABIT_ID_MIGRATION_KEY, 0))
        return 1;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT sync_activity,deleted_at FROM habits WHERE id='yoga' LIMIT 1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    if(sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        set_meta_int64(STORAGE_DEFAULT_HABIT_ID_MIGRATION_KEY, 1);
        return 1;
    }
    sync_activity = sqlite3_column_int(stmt, 0);
    deleted_at = sqlite3_column_int(stmt, 1);
    sqlite3_finalize(stmt);

    should_migrate = deleted_at == 0 &&
                     (sync_activity & STORAGE_ACTIVITY_SUN_SALUTATION_MASK) != 0;
    if(!should_migrate) {
        set_meta_int64(STORAGE_DEFAULT_HABIT_ID_MIGRATION_KEY, 1);
        return 1;
    }

    has_canonical = storage_habit_exists("sun-salutation");
    if(!exec_sql("BEGIN IMMEDIATE"))
        return 0;
    if(has_canonical) {
        ok = storage_merge_habit_into("sun-salutation", "yoga");
    } else {
        ok = exec_sql("UPDATE habits SET id='sun-salutation' WHERE id='yoga';"
                      "UPDATE habit_days SET habit_id='sun-salutation' WHERE habit_id='yoga';");
    }
    if(ok) {
        exec_sql("UPDATE OR IGNORE sync_outbox SET entity_id='sun-salutation' "
                 "WHERE entity_id='yoga' AND entity_type IN ('habit','habit_day');"
                 "DELETE FROM sync_outbox WHERE entity_id='yoga' "
                 "AND entity_type IN ('habit','habit_day');");
        set_meta_int64(STORAGE_DEFAULT_HABIT_ID_MIGRATION_KEY, 1);
        ok = exec_sql("COMMIT");
    } else {
        exec_sql("ROLLBACK");
        return 0;
    }
    if(!ok)
        return 0;

    storage_enqueue_sync_habit("sun-salutation");
    storage_enqueue_habit_days_for_sync("sun-salutation");
    return 1;
}

int
storage_migrate_default_meditation_activity_mask(void)
{
    static const char *sql =
        "BEGIN IMMEDIATE;"
        "UPDATE habits SET sync_activity="
        "11,updated_at=CAST(strftime('%s','now') AS INTEGER) "
        "WHERE deleted_at=0 AND sync_mode=1 AND lower(name)='meditation' "
        "AND color_r=126 AND color_g=183 AND color_b=230 "
        "AND sync_activity IN (2,3);"
        "INSERT INTO sync_outbox(entity_type,entity_id,local_date,queued_at) "
        "SELECT 'habit',id,0,CAST(strftime('%s','now') AS INTEGER) FROM habits "
        "WHERE deleted_at=0 AND sync_mode=1 AND lower(name)='meditation' "
        "AND color_r=126 AND color_g=183 AND color_b=230 "
        "AND sync_activity=11 AND changes()>0 "
        "ON CONFLICT(entity_type,entity_id,local_date) DO UPDATE SET "
        "queued_at=excluded.queued_at;"
        "COMMIT;";

    if(g_storage.db == NULL)
        return 0;
    return exec_sql(sql);
}

int
storage_migrate_habit_ids_to_uuid(void)
{
    sqlite3_stmt *stmt = NULL;
    struct {
        char old_id[INBE_STORAGE_ID_SIZE];
        char new_id[37];
    } rows[INBE_HABIT_MAX];
    int count = 0;
    int ok = 1;

    if(g_storage.db == NULL)
        return 0;
    if(get_meta_int64(STORAGE_HABIT_UUID_MIGRATION_KEY, 0))
        return 1;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id FROM habits WHERE id<>'' ORDER BY sort_order,id",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    while(sqlite3_step(stmt) == SQLITE_ROW && count < INBE_HABIT_MAX) {
        const char *id = (const char *)sqlite3_column_text(stmt, 0);
        if(id != NULL && !storage_is_uuid(id)) {
            snprintf(rows[count].old_id, sizeof(rows[count].old_id), "%s", id);
            storage_make_uuid(rows[count].new_id);
            count++;
        }
    }
    sqlite3_finalize(stmt);
    if(count <= 0) {
        set_meta_int64(STORAGE_HABIT_UUID_MIGRATION_KEY, 1);
        return 1;
    }
    if(!exec_sql("BEGIN IMMEDIATE"))
        return 0;
    for(int i = 0; i < count; i++) {
        const char *sqls[] = {
            "UPDATE habits SET id=?2,updated_at=?3 WHERE id=?1",
            "UPDATE habit_days SET habit_id=?2 WHERE habit_id=?1",
            "UPDATE OR IGNORE sync_outbox SET entity_id=?2 "
            "WHERE entity_id=?1 AND entity_type IN ('habit','habit_day')",
            "DELETE FROM sync_outbox WHERE entity_id=?1 "
            "AND entity_type IN ('habit','habit_day')",
            "UPDATE settings SET value=?2,updated_at=?3 "
            "WHERE key='habits_selected_id' AND value=?1"};
        long long changed_at = storage_next_change_time();

        for(size_t j = 0; j < sizeof(sqls) / sizeof(sqls[0]); j++) {
            if(sqlite3_prepare_v2(g_storage.db, sqls[j], -1, &stmt, NULL) != SQLITE_OK) {
                ok = 0;
                break;
            }
            bind_text(stmt, 1, rows[i].old_id);
            bind_text(stmt, 2, rows[i].new_id);
            sqlite3_bind_int64(stmt, 3, changed_at);
            ok = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
            stmt = NULL;
            if(!ok)
                break;
        }
        if(!ok)
            break;
        storage_enqueue_sync_habit(rows[i].new_id);
        storage_enqueue_habit_days_for_sync(rows[i].new_id);
    }
    if(ok) {
        set_meta_int64(STORAGE_HABIT_UUID_MIGRATION_KEY, 1);
        ok = exec_sql("COMMIT");
    } else {
        sqlite3_finalize(stmt);
        exec_sql("ROLLBACK");
    }
    return ok;
}

int
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
storage_apply_sync_habits_json(const char *response_json)
{
    static const char *habits_sql =
        "INSERT INTO "
        "habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,"
        "counter_enabled,sort_order,deleted_at,updated_at,weekdays,reminder_hour) "
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
        "INTEGER), "
        "       CAST(COALESCE(json_extract(value,'$.weekdays'),"
        "                     (SELECT weekdays FROM habits WHERE "
        "id=COALESCE(json_extract(value,'$.id'),'')),0) AS INTEGER),"
        "       CAST(COALESCE(json_extract(value,'$.reminder_hour'),"
        "                     (SELECT reminder_hour FROM habits WHERE "
        "id=COALESCE(json_extract(value,'$.id'),'')),-1) AS INTEGER) "
        "FROM (SELECT value FROM json_each(?1,'$.changes.habits') "
        "      UNION ALL SELECT value FROM json_each(?1,'$.data.habits')) "
        "WHERE COALESCE(json_extract(value,'$.id'),'')<>'' "
        "ON CONFLICT(id) DO UPDATE SET "
        " user_id=excluded.user_id,name=excluded.name,color_r=excluded.color_r,"
        "color_g=excluded.color_g,"
        " color_b=excluded.color_b,sync_mode=excluded.sync_mode,sync_activity="
        "excluded.sync_activity,"
        " counter_enabled=excluded.counter_enabled,sort_order=excluded.sort_"
        "order,deleted_at=excluded.deleted_at,"
        " updated_at=excluded.updated_at,"
        " weekdays=excluded.weekdays,reminder_hour=excluded.reminder_hour "
        "WHERE excluded.updated_at > habits.updated_at "
        "OR (excluded.updated_at = habits.updated_at AND NOT EXISTS ("
        " SELECT 1 FROM sync_outbox "
        " WHERE entity_type='habit' AND entity_id=habits.id AND local_date=0"
        "))";

    return storage_exec_json_user_sql(habits_sql, response_json);
}

int
storage_apply_sync_habit_days_json(const char *response_json)
{
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
        "FROM (SELECT value FROM json_each(?1,'$.changes.habit_days') "
        "      UNION ALL SELECT value FROM json_each(?1,'$.data.habit_days')) "
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

    return storage_exec_json_user_sql(habit_days_sql, response_json);
}
