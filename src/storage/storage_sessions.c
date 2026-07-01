#include "storage.h"

#include "breath_engine.h"
#include "db.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

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
make_session_id(long long started_at, const int *round_times, int round_count, char *out,
                size_t out_size)
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

int
insert_session_at_ex(long long started_at, int local_date, const int *round_times, int round_count,
                     int topic, int activity, const char *source, char *out_id, size_t out_id_size)
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
                          "INSERT OR IGNORE INTO "
                          "sessions(id,user_id,started_at,local_date,topic,"
                          "activity,source,imported_at,rounds_hash,updated_at) "
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
                                  "INSERT OR REPLACE INTO "
                                  "session_rounds(session_id,round_index,seconds) VALUES(?1,?2,?3)",
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
storage_save_session_for_activity(const int *round_times, int round_count, int topic, int activity,
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
    return insert_session_at_ex((long long)now, local_date, saved, saved_count, topic, activity,
                                "app", out_id, out_id_size);
}

int
storage_save_session_at_for_activity(int local_date, int hour, int minute, int second,
                                     const int *round_times, int round_count, int topic,
                                     int activity, char *out_id, size_t out_id_size)
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

    return insert_session_at_ex((long long)started_at, local_date, saved, saved_count, topic,
                                activity, "app", out_id, out_id_size);
}

int
storage_save_session(const int *round_times, int round_count, char *out_id, size_t out_id_size)
{
    return storage_save_session_for_activity(round_times, round_count, 0, 0, out_id, out_id_size);
}

int
storage_load_session(const char *path_or_id, int *round_times, int max_rounds, int *year,
                     int *month, int *day, int *hour, int *minute, int *second)
{
    sqlite3_stmt *stmt = NULL;
    char id[INBE_STORAGE_ID_SIZE];
    int count = 0;
    long long started_at = 0;
    int local_date = 0;

    if(!parse_db_id(path_or_id, id, sizeof(id)) || round_times == NULL || max_rounds <= 0)
        return 0;

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT started_at,local_date FROM sessions WHERE "
                          "id=?1 AND deleted_at=0",
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

    if(year)
        *year = local_date / 10000;
    if(month)
        *month = (local_date / 100) % 100;
    if(day)
        *day = local_date % 100;
    {
        time_t t = (time_t)started_at;
        struct tm *tm = localtime(&t);
        if(tm != NULL) {
            if(hour)
                *hour = tm->tm_hour;
            if(minute)
                *minute = tm->tm_min;
            if(second)
                *second = tm->tm_sec;
        }
    }

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT seconds FROM session_rounds WHERE "
                          "session_id=?1 ORDER BY round_index",
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

    if(!parse_db_id(path_or_id, id, sizeof(id)) || round_times == NULL || round_count < 0 ||
       round_count > MaxRounds)
        return 0;
    for(int i = 0; i < round_count; i++) {
        if(round_times[i] > 0)
            saved[saved_count++] = round_times[i];
    }
    if(saved_count <= 0)
        return storage_delete_session(path_or_id);

    exec_sql("BEGIN IMMEDIATE");
    if(sqlite3_prepare_v2(g_storage.db, "DELETE FROM session_rounds WHERE session_id=?1", -1, &stmt,
                          NULL) != SQLITE_OK)
        goto fail;
    bind_text(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    stmt = NULL;
    for(int i = 0; i < saved_count; i++) {
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT INTO session_rounds(session_id,round_index,seconds) "
                              "VALUES(?1,?2,?3)",
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
    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE sessions SET rounds_hash=?2,updated_at=?3 WHERE id=?1", -1, &stmt,
                          NULL) != SQLITE_OK)
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

    if(!parse_db_id(path_or_id, id, sizeof(id)) || hour < 0 || hour > 23 || minute < 0 ||
       minute > 59)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT started_at FROM sessions WHERE id=?1", -1, &stmt,
                          NULL) != SQLITE_OK)
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
    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE sessions SET started_at=?2,updated_at=?3 WHERE id=?1", -1, &stmt,
                          NULL) != SQLITE_OK)
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
    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE sessions SET deleted_at=?2,updated_at=?2 WHERE id=?1", -1, &stmt,
                          NULL) != SQLITE_OK)
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

int
storage_discard_session(const char *path_or_id)
{
    sqlite3_stmt *stmt = NULL;
    char id[INBE_STORAGE_ID_SIZE];

    if(!parse_db_id(path_or_id, id, sizeof(id)))
        return 0;
    if(!exec_sql("BEGIN IMMEDIATE"))
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "DELETE FROM session_rounds WHERE session_id=?1", -1,
                          &stmt, NULL) != SQLITE_OK) {
        exec_sql("ROLLBACK");
        return 0;
    }
    bind_text(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    stmt = NULL;
    if(sqlite3_prepare_v2(g_storage.db, "DELETE FROM sessions WHERE id=?1", -1, &stmt,
                          NULL) != SQLITE_OK) {
        exec_sql("ROLLBACK");
        return 0;
    }
    bind_text(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    stmt = NULL;
    if(!exec_sql("COMMIT")) {
        exec_sql("ROLLBACK");
        return 0;
    }
    storage_materialize_session_habit_days();
    exec_sql("DELETE FROM sync_outbox");
    g_storage.pending_sync_outbox_seq = 0;
    storage_schedule_persist();
    return 1;
}

void
storage_list_session_records(InbeStorageSessionRecordCallback callback, void *user)
{
    sqlite3_stmt *stmt = NULL;
    if(callback == NULL || g_storage.db == NULL)
        return;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id,started_at,local_date,topic,activity FROM sessions WHERE "
                          "deleted_at=0 ORDER BY started_at DESC LIMIT 48",
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
                          "SELECT COUNT(*) FROM habit_days WHERE completed!=0 OR count>0", -1,
                          &stmt, NULL) != SQLITE_OK)
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
    if(sqlite3_prepare_v2(g_storage.db, "SELECT COUNT(*) FROM sessions WHERE deleted_at=0", -1,
                          &stmt, NULL) != SQLITE_OK)
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
                          "SELECT COUNT(*) FROM habit_days WHERE completed!=0 OR count>0", -1,
                          &stmt, NULL) == SQLITE_OK) {
        if(sqlite3_step(stmt) == SQLITE_ROW)
            habit_day_count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if(count <= 0 && habit_day_count <= 0 && habit_count <= 0)
        return 0;

    if(!storage_has_sync_account()) {
        if(!exec_sql("BEGIN IMMEDIATE"))
            return 0;
        if(!exec_sql("DELETE FROM session_rounds WHERE session_id IN "
                     "(SELECT id FROM sessions WHERE user_id=(SELECT id FROM users "
                     "LIMIT 1));"
                     "DELETE FROM sessions WHERE user_id=(SELECT id FROM users LIMIT 1);"
                     "DELETE FROM habit_days WHERE habit_id IN "
                     "(SELECT id FROM habits WHERE user_id=(SELECT id FROM users LIMIT "
                     "1));"
                     "DELETE FROM habits WHERE user_id=(SELECT id FROM users LIMIT 1);"
                     "DELETE FROM sync_outbox;")) {
            exec_sql("ROLLBACK");
            return 0;
        }
        set_meta_int64("sync_last_server_version", 0);
        set_meta_int64("sync_last_upload_at", 0);
        set_meta_int64("sync_full_upload_done", 0);
        set_meta_int64("sync_backfill_v2_done", 0);
        set_meta_int64("sync_habit_name_repair_v1_done", 0);
        if(!exec_sql("COMMIT")) {
            exec_sql("ROLLBACK");
            return 0;
        }
        storage_schedule_persist();
        return count + habit_day_count + habit_count;
    }

    if(!exec_sql("BEGIN IMMEDIATE"))
        return 0;
    if(sqlite3_prepare_v2(g_storage.db, "SELECT id FROM sessions WHERE user_id=?1 AND deleted_at=0",
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
    if(sqlite3_prepare_v2(g_storage.db, "SELECT id FROM habits WHERE user_id=?1 AND deleted_at=0",
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
                          "UPDATE habit_days SET "
                          "completed=0,count=0,session_count=0,updated_at=?1 "
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
