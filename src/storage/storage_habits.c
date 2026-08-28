#include "storage.h"

#include "db.h"
#include "screens/habits_screen.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

int
storage_habits_empty(void)
{
    return storage_habit_count() == 0;
}

int
storage_habit_count(void)
{
    return db_select_int("SELECT COUNT(*) FROM habits WHERE deleted_at=0", 0);
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
                          "enabled,weekdays,reminder_hour "
                          "FROM habits WHERE deleted_at=0 ORDER BY sort_order,id LIMIT 32",
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
        habit->weekdays = sqlite3_column_int(stmt, 9);
        habit->reminder_hour = sqlite3_column_int(stmt, 10);
        if(habit->reminder_hour > 23)
            habit->reminder_hour = -1;
        index++;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;
    habits->count = index;
    habits->loaded_count = index;

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT local_date,completed,count,session_count FROM habit_days "
                          "WHERE habit_id=?1 ORDER BY local_date",
                          -1, &stmt, NULL) == SQLITE_OK) {
        for(int i = 0; i < habits->count; i++) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
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
                habits->items[i].days[d].session_count = sqlite3_column_int(stmt, 3);
                if(habits->items[i].days[d].count <= 0 && habits->items[i].days[d].completed)
                    habits->items[i].days[d].count = 1;
            }
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
    sqlite3_stmt *loaded_stmt = NULL;
    sqlite3_stmt *habit_stmt = NULL;
    sqlite3_stmt *desc_stmt = NULL;
    sqlite3_stmt *seen_stmt = NULL;
    sqlite3_stmt *day_stmt = NULL;
    long long changed_at = storage_next_change_time();
    if(habits == NULL || g_storage.db == NULL)
        return;
    storage_mark_habits_initialized();
    exec_sql("BEGIN IMMEDIATE");
    exec_sql("CREATE TEMP TABLE IF NOT EXISTS sync_seen_habits(id TEXT PRIMARY KEY);"
             "CREATE TEMP TABLE IF NOT EXISTS sync_loaded_habits(id TEXT PRIMARY KEY);"
             "DELETE FROM sync_seen_habits;"
             "DELETE FROM sync_loaded_habits;");
    sqlite3_prepare_v2(g_storage.db, "INSERT OR IGNORE INTO sync_loaded_habits(id) VALUES(?1)",
                       -1, &loaded_stmt, NULL);
    sqlite3_prepare_v2(g_storage.db,
                       "INSERT INTO "
                       "habits(id,user_id,name,description,color_r,color_g,color_b,sync_mode,sync_"
                       "activity,counter_enabled,sort_order,deleted_at,updated_at,"
                       "weekdays,reminder_hour) "
                       "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,0,?12,?13,?14) "
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
                       "updated_at=excluded.updated_at,"
                       "weekdays=excluded.weekdays,"
                       "reminder_hour=excluded.reminder_hour "
                       "WHERE habits.user_id<>excluded.user_id OR "
                       "habits.name<>excluded.name OR "
                       "habits.color_r<>excluded.color_r OR "
                       "habits.color_g<>excluded.color_g OR "
                       "habits.color_b<>excluded.color_b OR "
                       "habits.sync_mode<>excluded.sync_mode OR "
                       "habits.sync_activity<>excluded.sync_activity OR "
                       "habits.counter_enabled<>excluded.counter_enabled OR "
                       "habits.sort_order<>excluded.sort_order OR "
                       "habits.deleted_at<>0 OR "
                       "habits.weekdays<>excluded.weekdays OR "
                       "habits.reminder_hour<>excluded.reminder_hour",
                       -1, &habit_stmt, NULL);
    sqlite3_prepare_v2(g_storage.db,
                       "UPDATE habits SET description=?2 "
                       "WHERE id=?1 AND description<>?2",
                       -1, &desc_stmt, NULL);
    sqlite3_prepare_v2(g_storage.db, "INSERT OR IGNORE INTO sync_seen_habits(id) VALUES(?1)",
                       -1, &seen_stmt, NULL);
    sqlite3_prepare_v2(g_storage.db,
                       "INSERT INTO "
                       "habit_days(habit_id,local_date,completed,count,updated_at) "
                       "VALUES(?1,?2,?3,?4,?5) "
                       "ON CONFLICT(habit_id,local_date) DO UPDATE SET "
                       "completed=excluded.completed,"
                       "count=excluded.count,"
                       "updated_at=CASE WHEN habit_days.completed<>excluded.completed "
                       "OR habit_days.count<>excluded.count "
                       "THEN excluded.updated_at ELSE habit_days.updated_at END",
                       -1, &day_stmt, NULL);
    for(int i = 0; i < habits->loaded_count && i < INBE_HABIT_MAX; i++) {
        if(habits->loaded_ids[i][0] == '\0')
            continue;
        if(loaded_stmt == NULL)
            continue;
        sqlite3_reset(loaded_stmt);
        sqlite3_clear_bindings(loaded_stmt);
        bind_text(loaded_stmt, 1, habits->loaded_ids[i]);
        sqlite3_step(loaded_stmt);
    }
    for(int i = 0; i < habits->count; i++) {
        const InbeHabit *habit = &habits->items[i];
        if(habit_stmt == NULL)
            continue;
        sqlite3_reset(habit_stmt);
        sqlite3_clear_bindings(habit_stmt);
        bind_text(habit_stmt, 1, habit->id);
        bind_text(habit_stmt, 2, g_storage.user_id);
        bind_text(habit_stmt, 3, habit->name);
        bind_text(habit_stmt, 4, habit->description);
        sqlite3_bind_int(habit_stmt, 5, habit->color.r);
        sqlite3_bind_int(habit_stmt, 6, habit->color.g);
        sqlite3_bind_int(habit_stmt, 7, habit->color.b);
        sqlite3_bind_int(habit_stmt, 8, habit->sync_mode);
        sqlite3_bind_int(habit_stmt, 9, habit->sync_activity);
        sqlite3_bind_int(habit_stmt, 10, habit->counter_enabled ? 1 : 0);
        sqlite3_bind_int(habit_stmt, 11, i);
        sqlite3_bind_int64(habit_stmt, 12, changed_at);
        sqlite3_bind_int(habit_stmt, 13, habit->weekdays & 0x7f);
        sqlite3_bind_int(habit_stmt, 14, habit->reminder_hour >= 0 &&
                                       habit->reminder_hour <= 23
                                       ? habit->reminder_hour : -1);
        if(sqlite3_step(habit_stmt) == SQLITE_DONE && sqlite3_changes(g_storage.db) > 0)
            storage_enqueue_sync_habit(habit->id);
        if(desc_stmt != NULL) {
            sqlite3_reset(desc_stmt);
            sqlite3_clear_bindings(desc_stmt);
            bind_text(desc_stmt, 1, habit->id);
            bind_text(desc_stmt, 2, habit->description);
            sqlite3_step(desc_stmt);
        }
        if(seen_stmt != NULL) {
            sqlite3_reset(seen_stmt);
            sqlite3_clear_bindings(seen_stmt);
            bind_text(seen_stmt, 1, habit->id);
            sqlite3_step(seen_stmt);
        }
        for(int d = 0; d < habit->day_count; d++) {
            if(day_stmt == NULL)
                continue;
            sqlite3_reset(day_stmt);
            sqlite3_clear_bindings(day_stmt);
            bind_text(day_stmt, 1, habit->id);
            sqlite3_bind_int(day_stmt, 2, habit->days[d].day_index);
            sqlite3_bind_int(day_stmt, 3,
                             habit->days[d].count > 0 || habit->days[d].completed ? 1 : 0);
            sqlite3_bind_int(day_stmt, 4,
                             habit->days[d].count > 0 ? habit->days[d].count
                                                      : (habit->days[d].completed ? 1 : 0));
            sqlite3_bind_int64(day_stmt, 5, changed_at);
            sqlite3_step(day_stmt);
            storage_enqueue_sync_habit_day(habit->id, habit->days[d].day_index);
        }
    }
    sqlite3_finalize(loaded_stmt);
    sqlite3_finalize(habit_stmt);
    sqlite3_finalize(desc_stmt);
    sqlite3_finalize(seen_stmt);
    sqlite3_finalize(day_stmt);
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

int
storage_habit_day_save(const char *habit_id, int local_date, int completed, int count)
{
    sqlite3_stmt *stmt = NULL;
    long long changed_at;
    int rc;
    int changed;

    if(g_storage.db == NULL || habit_id == NULL || habit_id[0] == '\0' || local_date <= 0)
        return 0;
    if(count < 0)
        count = 0;
    completed = completed || count > 0;
    changed_at = storage_next_change_time();
    storage_mark_habits_initialized();

    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT INTO "
                          "habit_days(habit_id,local_date,completed,count,session_count,updated_at) "
                          "VALUES(?1,?2,?3,?4,0,?5) "
                          "ON CONFLICT(habit_id,local_date) DO UPDATE SET "
                          "completed=excluded.completed,"
                          "count=excluded.count,"
                          "updated_at=CASE WHEN habit_days.completed<>excluded.completed "
                          "OR habit_days.count<>excluded.count "
                          "THEN excluded.updated_at ELSE habit_days.updated_at END",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    bind_text(stmt, 1, habit_id);
    sqlite3_bind_int(stmt, 2, local_date);
    sqlite3_bind_int(stmt, 3, completed ? 1 : 0);
    sqlite3_bind_int(stmt, 4, count);
    sqlite3_bind_int64(stmt, 5, changed_at);
    rc = sqlite3_step(stmt);
    changed = sqlite3_changes(g_storage.db) > 0;
    sqlite3_finalize(stmt);

    if(rc != SQLITE_DONE)
        return 0;
    if(changed)
        storage_enqueue_sync_habit_day(habit_id, local_date);
    storage_schedule_persist();
    return 1;
}
