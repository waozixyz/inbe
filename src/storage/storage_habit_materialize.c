#include "storage.h"

#include "db.h"
#include "screens/habits_screen.h"

#include "kryon.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

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
    if(!exec_sql("SAVEPOINT inbe_materialize_habit_days"))
        return 0;
    ok = storage_snapshot_habit_day_visible_state() &&
         storage_build_session_habit_counts() && storage_apply_session_habit_counts(changed_at) &&
         storage_enqueue_materialized_habit_day_changes() &&
         exec_sql("DROP TABLE IF EXISTS temp.inbe_session_habit_counts;");
    if(ok) {
        ok = exec_sql("RELEASE inbe_materialize_habit_days");
    } else {
        TraceLog(LOG_WARNING, "STORAGE: failed to materialize session habit counts: %s",
                 sqlite3_errmsg(g_storage.db));
        exec_sql("ROLLBACK TO inbe_materialize_habit_days");
        exec_sql("RELEASE inbe_materialize_habit_days");
    }
    return ok;
}
