#include "storage.h"
#include "screens/habits_screen.h"
#include "breath_engine.h"
#include "miniz.h"
#include "raylib.h"
#include <sqlite3.h>

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int g_failures = 0;
static int g_seen_topic = -1;
static int g_seen_activity = -1;
static int g_seen_round_count = -1;
static int g_seen_first_round = -1;

void
data_init(void)
{
}

static void
check_int(const char *label, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "FAIL %s: got %d, want %d\n", label, got, want);
    g_failures++;
}

static void
check_true(const char *label, int ok)
{
    if(ok)
        return;
    fprintf(stderr, "FAIL %s\n", label);
    g_failures++;
}

static void
check_str(const char *label, const char *got, const char *want)
{
    if(got == NULL && want == NULL)
        return;
    if(got != NULL && want != NULL && strcmp(got, want) == 0)
        return;
    fprintf(stderr, "FAIL %s: got %s, want %s\n",
            label, got != NULL ? got : "(null)", want != NULL ? want : "(null)");
    g_failures++;
}

static int
ascii_equal_ci(const char *a, const char *b)
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

static InbeHabit *
find_habit_ci(InbeHabits *habits, const char *name)
{
    if(habits == NULL || name == NULL)
        return NULL;
    for(int i = 0; i < habits->count; i++) {
        if(ascii_equal_ci(habits->items[i].name, name))
            return &habits->items[i];
    }
    return NULL;
}

static int
ensure_dir(const char *path)
{
    struct stat st;
    if(stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return 1;
    return mkdir(path, 0700) == 0;
}

static void
make_path(char *out, size_t out_size, const char *root, const char *leaf)
{
    snprintf(out, out_size, "%s/%s", root, leaf);
}

static void
make_nested_dir(const char *root, const char *a, const char *b, const char *c)
{
    char path[512];

    make_path(path, sizeof(path), root, a);
    check_true("create nested dir a", ensure_dir(path));
    snprintf(path, sizeof(path), "%s/%s/%s", root, a, b);
    check_true("create nested dir b", ensure_dir(path));
    snprintf(path, sizeof(path), "%s/%s/%s/%s", root, a, b, c);
    check_true("create nested dir c", ensure_dir(path));
}

static void
remove_tree(const char *path)
{
    DIR *dir = opendir(path);
    struct dirent *entry;

    if(dir == NULL) {
        remove(path);
        return;
    }
    while((entry = readdir(dir)) != NULL) {
        char child[1024];
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        remove_tree(child);
    }
    closedir(dir);
    rmdir(path);
}

static void
make_clean_root(char *out, size_t out_size, const char *name)
{
    snprintf(out, out_size, "/tmp/inbe-storage-test-%ld-%s", (long)getpid(), name);
    remove_tree(out);
    check_true("create test root", ensure_dir(out));
}

static void
write_source_database(const char *root)
{
    int rounds[] = {45, 60, 75};
    InbeHabits habits;

    check_true("init source db", inbe_storage_init(root));
    check_true("save source session", inbe_storage_save_session(rounds, 3, NULL, 0));
    memset(&habits, 0, sizeof(habits));
    inbe_habits_add_default_set(&habits);
    inbe_habit_set_day(&habits, 0, 20260613, 1);
    check_int("source sessions", inbe_storage_session_count(), 1);
    inbe_storage_close();
}

static void
insert_raw_habit_day(const char *root, const char *habit_id, int local_date, int completed)
{
    char db_path[512];
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;

    make_path(db_path, sizeof(db_path), root, "inbe.db");
    check_true("open raw habit day db", sqlite3_open(db_path, &db) == SQLITE_OK);
    if(db == NULL)
        return;
    if(sqlite3_prepare_v2(db,
                          "INSERT OR REPLACE INTO habit_days(habit_id,local_date,completed,updated_at) "
                          "VALUES(?1,?2,?3,0)",
                          -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, habit_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, local_date);
        sqlite3_bind_int(stmt, 3, completed ? 1 : 0);
        check_true("insert raw habit day", sqlite3_step(stmt) == SQLITE_DONE);
    } else {
        check_true("prepare raw habit day", 0);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

static int
read_raw_habit_day_count(const char *root, const char *habit_id, int local_date)
{
    char db_path[512];
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    int count = -1;

    make_path(db_path, sizeof(db_path), root, "inbe.db");
    check_true("open raw count db", sqlite3_open(db_path, &db) == SQLITE_OK);
    if(db == NULL)
        return count;
    if(sqlite3_prepare_v2(db,
                          "SELECT count FROM habit_days WHERE habit_id=?1 AND local_date=?2",
                          -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, habit_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, local_date);
        if(sqlite3_step(stmt) == SQLITE_ROW)
            count = sqlite3_column_int(stmt, 0);
    } else {
        check_true("prepare raw count", 0);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return count;
}

static void
test_sync_payload_omits_uploaded_state_after_upload_marker(void)
{
    char root[512];
    InbeHabits habits;
    char *payload;

    make_clean_root(root, sizeof(root), "sync-full-local-state");
    check_true("init sync watermark db", inbe_storage_init(root));
    memset(&habits, 0, sizeof(habits));
    inbe_habits_add_default_set(&habits);
    inbe_habit_set_day_count(&habits, 0, 20260612, 12);
    inbe_habits_save(&habits);

    inbe_storage_close();

    {
        char db_path[512];
        sqlite3 *db = NULL;
        sqlite3_stmt *stmt = NULL;
        long long updated_at = 0;
        make_path(db_path, sizeof(db_path), root, "inbe.db");
        check_true("open sync watermark raw db", sqlite3_open(db_path, &db) == SQLITE_OK);
        if(db != NULL) {
            if(sqlite3_prepare_v2(db,
                                  "SELECT updated_at FROM habit_days WHERE local_date=20260612",
                                  -1, &stmt, NULL) == SQLITE_OK &&
               sqlite3_step(stmt) == SQLITE_ROW)
                updated_at = sqlite3_column_int64(stmt, 0);
            if(stmt != NULL)
                sqlite3_finalize(stmt);
            check_true("set sync watermark raw meta",
                       updated_at > 0 &&
                       sqlite3_exec(db,
                                    "INSERT OR REPLACE INTO meta(key,value) VALUES('sync_full_upload_done','1');",
                                    NULL, NULL, NULL) == SQLITE_OK);
            if(updated_at > 0) {
                char sql[192];
                snprintf(sql, sizeof(sql),
                         "INSERT OR REPLACE INTO meta(key,value) VALUES('sync_last_upload_at','%lld');",
                         updated_at);
                check_true("set sync same second upload marker",
                           sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK);
            }
            sqlite3_close(db);
        }
    }

    check_true("reopen sync watermark db", inbe_storage_init(root));
    payload = inbe_storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("habit day omitted from sync payload after upload marker",
               payload != NULL &&
               strstr(payload, "\"local_date\":20260612") == NULL &&
               strstr(payload, "\"count\":12") == NULL);
    inbe_storage_free_sync_payload_json(payload);
    inbe_storage_close();
    remove_tree(root);
}

static void
test_sync_payload_includes_queued_current_edits(void)
{
    char root[512];
    InbeHabits habits;
    char *payload;

    make_clean_root(root, sizeof(root), "sync-queued-edit");
    check_true("init queued sync db", inbe_storage_init(root));
    memset(&habits, 0, sizeof(habits));
    inbe_habits_add_default_set(&habits);
    inbe_habit_set_day_count(&habits, 0, 20260617, 7);
    inbe_habits_save(&habits);

    payload = inbe_storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("queued habit day included in sync payload",
               payload != NULL &&
               strstr(payload, "\"local_date\":20260617") != NULL &&
               strstr(payload, "\"count\":7") != NULL);
    inbe_storage_free_sync_payload_json(payload);

    inbe_storage_close();
    remove_tree(root);
}

static void
test_sync_outbox_preserves_edits_after_snapshot(void)
{
    char root[512];
    InbeHabits habits;
    char *payload;
    const char *empty_response =
        "{\"server_version\":1,\"changes\":{\"habits\":[],\"habit_days\":[],\"sessions\":[]}}";

    make_clean_root(root, sizeof(root), "sync-outbox-snapshot");
    check_true("init outbox snapshot db", inbe_storage_init(root));
    memset(&habits, 0, sizeof(habits));
    inbe_habits_add_default_set(&habits);
    inbe_habit_set_day_count(&habits, 0, 20260618, 1);
    inbe_habits_save(&habits);

    payload = inbe_storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("first queued edit in payload",
               payload != NULL && strstr(payload, "\"local_date\":20260618") != NULL);
    inbe_storage_free_sync_payload_json(payload);

    inbe_habit_set_day_count(&habits, 0, 20260619, 2);
    inbe_habits_save(&habits);
    check_true("apply response clears only snapshotted outbox",
               inbe_storage_apply_sync_response_json(empty_response));

    payload = inbe_storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("later edit remains queued after snapshot clear",
               payload != NULL &&
               strstr(payload, "\"local_date\":20260619") != NULL &&
               strstr(payload, "\"count\":2") != NULL);
    check_true("earlier edit was cleared after snapshot success",
               payload != NULL && strstr(payload, "\"local_date\":20260618") == NULL);
    inbe_storage_free_sync_payload_json(payload);

    inbe_storage_close();
    remove_tree(root);
}

static void
test_sync_apply_preserves_counter_counts(void)
{
    char root[512];
    const char *habit_id = "counter-habit";
    const char *lower_count_response =
        "{\"server_version\":1,\"changes\":{\"habits\":[],\"habit_days\":["
        "{\"habit_id\":\"counter-habit\",\"local_date\":20260619,"
        "\"completed\":true,\"count\":1,\"updated_at\":\"2026-06-19T21:00:00Z\"}"
        "],\"sessions\":[]}}";
    const char *higher_count_response =
        "{\"server_version\":2,\"changes\":{\"habits\":[],\"habit_days\":["
        "{\"habit_id\":\"counter-habit\",\"local_date\":20260619,"
        "\"completed\":true,\"count\":5,\"updated_at\":\"2026-06-19T21:00:00Z\"}"
        "],\"sessions\":[]}}";

    make_clean_root(root, sizeof(root), "sync-counter");
    check_true("init sync counter db", inbe_storage_init(root));
    inbe_storage_close();

    {
        char db_path[512];
        sqlite3 *db = NULL;
        make_path(db_path, sizeof(db_path), root, "inbe.db");
        check_true("open sync counter raw db", sqlite3_open(db_path, &db) == SQLITE_OK);
        if(db != NULL) {
            check_true("insert sync counter habit",
                       sqlite3_exec(db,
                                    "INSERT INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_enabled,sort_order,deleted_at,updated_at) "
                                    "VALUES('counter-habit','default','Counter',255,255,255,0,0,1,0,0,1781902800);"
                                    "INSERT INTO habit_days(habit_id,local_date,completed,count,updated_at) "
                                    "VALUES('counter-habit',20260619,1,4,1781902800);",
                                    NULL, NULL, NULL) == SQLITE_OK);
            sqlite3_close(db);
        }
    }

    check_true("reopen sync counter db", inbe_storage_init(root));
    check_true("apply lower equal counter", inbe_storage_apply_sync_response_json(lower_count_response));
    inbe_storage_close();
    check_int("lower equal sync does not reset counter",
              read_raw_habit_day_count(root, habit_id, 20260619), 4);

    check_true("reopen sync counter db for repair", inbe_storage_init(root));
    check_true("apply higher equal counter", inbe_storage_apply_sync_response_json(higher_count_response));
    inbe_storage_close();
    check_int("higher equal sync repairs counter",
              read_raw_habit_day_count(root, habit_id, 20260619), 5);

    remove_tree(root);
}

static void
test_session_linked_counts_materialize_for_sync(void)
{
    char root[512];
    char first_id[INBE_STORAGE_ID_SIZE + 4];
    char second_id[INBE_STORAGE_ID_SIZE + 4];
    int first_rounds[] = {30};
    int second_rounds[] = {45};
    int today = inbe_habits_today_index();
    InbeHabits habits;
    char *payload;

    make_clean_root(root, sizeof(root), "session-linked-count");
    check_true("init linked counter db", inbe_storage_init(root));
    memset(&habits, 0, sizeof(habits));
    inbe_habits_add_default_set(&habits);

    check_true("save linked session one",
               inbe_storage_save_session_for_activity(first_rounds, 1, 0, 1,
                                                      first_id, sizeof(first_id)));
    check_true("save linked session two",
               inbe_storage_save_session_for_activity(second_rounds, 1, 0, 1,
                                                      second_id, sizeof(second_id)));

    memset(&habits, 0, sizeof(habits));
    check_true("load linked counter habits", inbe_storage_habits_load(&habits));
    check_int("linked sessions materialize count",
              inbe_habit_day_count(&habits.items[0], today), 2);

    payload = inbe_storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("linked session count is in sync payload",
               payload != NULL && strstr(payload, "\"count\":2") != NULL);
    inbe_storage_free_sync_payload_json(payload);

    check_true("delete linked session one", inbe_storage_delete_session(first_id));
    memset(&habits, 0, sizeof(habits));
    check_true("reload linked counter after delete", inbe_storage_habits_load(&habits));
    check_int("linked session delete lowers derived count",
              inbe_habit_day_count(&habits.items[0], today), 1);

    inbe_habit_set_day_count(&habits, 0, today, 4);
    inbe_habits_save(&habits);
    check_true("delete linked session two", inbe_storage_delete_session(second_id));
    memset(&habits, 0, sizeof(habits));
    check_true("reload linked counter after manual override", inbe_storage_habits_load(&habits));
    check_int("linked session delete preserves manual count",
              inbe_habit_day_count(&habits.items[0], today), 4);

    inbe_storage_close();
    remove_tree(root);
}

static void
test_existing_sessions_materialize_after_habit_save(void)
{
    char root[512];
    int rounds[] = {30};
    int today = inbe_habits_today_index();
    InbeHabits habits;
    char *payload;

    make_clean_root(root, sizeof(root), "existing-session-linked-count");
    check_true("init existing linked counter db", inbe_storage_init(root));
    check_true("save existing linked session",
               inbe_storage_save_session_for_activity(rounds, 1, 0, 1, NULL, 0));

    memset(&habits, 0, sizeof(habits));
    inbe_habits_add_default_set(&habits);
    memset(&habits, 0, sizeof(habits));
    check_true("load existing linked counter habits", inbe_storage_habits_load(&habits));
    check_int("existing linked session materializes after habit save",
              inbe_habit_day_count(&habits.items[0], today), 1);

    payload = inbe_storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("existing linked session count is in sync payload",
               payload != NULL && strstr(payload, "\"count\":1") != NULL);
    inbe_storage_free_sync_payload_json(payload);

    inbe_storage_close();
    remove_tree(root);
}

static void
metadata_history_callback(const char *id, int year, int month, int day,
                          int hour, int minute, int second,
                          int topic, int activity,
                          const int *rounds, int round_count, void *user)
{
    (void)id;
    (void)year;
    (void)month;
    (void)day;
    (void)hour;
    (void)minute;
    (void)second;
    (void)rounds;
    (void)round_count;
    (void)user;
    g_seen_topic = topic;
    g_seen_activity = activity;
}

static void
legacy_history_callback(const char *id, int year, int month, int day,
                        int hour, int minute, int second,
                        int topic, int activity,
                        const int *rounds, int round_count, void *user)
{
    (void)id;
    (void)year;
    (void)month;
    (void)day;
    (void)hour;
    (void)minute;
    (void)second;
    (void)topic;
    (void)activity;
    (void)user;
    g_seen_round_count = round_count;
    g_seen_first_round = round_count > 0 ? rounds[0] : -1;
}

static void
test_session_metadata(void)
{
    char root[512];
    int rounds[] = {30, 45};

    make_clean_root(root, sizeof(root), "metadata");
    check_true("init metadata db", inbe_storage_init(root));
    check_true("save metadata session",
               inbe_storage_save_session_for_activity(rounds, 2, 2, 3, NULL, 0));
    g_seen_topic = -1;
    g_seen_activity = -1;
    inbe_storage_list_session_records(metadata_history_callback, NULL);
    check_int("metadata topic", g_seen_topic, 2);
    check_int("metadata activity", g_seen_activity, 3);
    inbe_storage_close();
    remove_tree(root);
}

static void
assert_imported_database(const char *root)
{
    InbeHabits habits;

    check_true("init imported db", inbe_storage_init(root));
    check_int("imported sessions", inbe_storage_session_count(), 1);
    memset(&habits, 0, sizeof(habits));
    check_true("imported habits load", inbe_storage_habits_load(&habits));
    check_int("imported habit count", habits.count, 1);
    check_true("imported habit day", inbe_habit_completed_day(&habits.items[0], 20260613));
    inbe_storage_close();
}

static void
test_raw_db_import(void)
{
    char source[512], dest[512], db_path[512];

    make_clean_root(source, sizeof(source), "raw-source");
    make_clean_root(dest, sizeof(dest), "raw-dest");
    write_source_database(source);
    make_path(db_path, sizeof(db_path), source, "inbe.db");

    check_true("init raw import dest", inbe_storage_init(dest));
    check_true("raw db import", inbe_storage_import_zip(db_path));
    inbe_storage_close();
    assert_imported_database(dest);

    remove_tree(source);
    remove_tree(dest);
}

static void
test_zip_db_import(void)
{
    char source[512], dest[512], zip_path[512];

    make_clean_root(source, sizeof(source), "zip-source");
    make_clean_root(dest, sizeof(dest), "zip-dest");
    write_source_database(source);
    make_path(zip_path, sizeof(zip_path), source, "export.zip");

    check_true("init export source", inbe_storage_init(source));
    check_true("export zip", inbe_storage_export_zip(zip_path));
    inbe_storage_close();

    check_true("init zip import dest", inbe_storage_init(dest));
    check_true("zip db import", inbe_storage_import_zip(zip_path));
    inbe_storage_close();
    assert_imported_database(dest);

    remove_tree(source);
    remove_tree(dest);
}

static void
test_habit_name_merge_import(void)
{
    char source[512], dest[512], zip_path[512];
    InbeHabits habits;
    InbeHabit *habit;

    make_clean_root(source, sizeof(source), "habit-name-source");
    make_clean_root(dest, sizeof(dest), "habit-name-dest");
    make_path(zip_path, sizeof(zip_path), source, "export.zip");

    check_true("init habit merge source", inbe_storage_init(source));
    memset(&habits, 0, sizeof(habits));
    check_int("add imported meditation",
              inbe_habits_add_custom(&habits, "meditation",
                                      (Color){224, 124, 104, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              0);
    inbe_habit_set_day(&habits, 0, 20260613, 1);
    check_int("add imported push ups",
              inbe_habits_add_custom(&habits, "Push ups",
                                      (Color){180, 132, 220, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              1);
    inbe_habit_set_day(&habits, 1, 20260614, 1);
    check_int("add imported cold shower",
              inbe_habits_add_custom(&habits, "Cold Shower",
                                      (Color){99, 196, 165, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              2);
    inbe_habit_set_day(&habits, 2, 20260615, 1);
    check_true("habit merge export", inbe_storage_export_zip(zip_path));
    inbe_storage_close();

    check_true("init habit merge dest", inbe_storage_init(dest));
    memset(&habits, 0, sizeof(habits));
    inbe_habits_add_default_set(&habits);
    inbe_habit_set_day(&habits, 0, 20260612, 1);
    check_true("habit merge import", inbe_storage_import_zip(zip_path));
    memset(&habits, 0, sizeof(habits));
    check_true("habit merge load", inbe_storage_habits_load(&habits));
    check_int("habit merge count", habits.count, 3);
    habit = find_habit_ci(&habits, "Meditation");
    check_true("habit merge meditation exists", habit != NULL);
    check_true("habit merge preserves local case",
               habit != NULL && strcmp(habit->name, "Meditation") == 0);
    check_true("habit merge keeps local day",
               habit != NULL && inbe_habit_completed_day(habit, 20260612));
    check_true("habit merge imports day",
               habit != NULL && inbe_habit_completed_day(habit, 20260613));
    habit = find_habit_ci(&habits, "Push ups");
    check_true("habit merge push ups exists", habit != NULL);
    check_true("habit merge push ups day",
               habit != NULL && inbe_habit_completed_day(habit, 20260614));
    habit = find_habit_ci(&habits, "Cold Shower");
    check_true("habit merge cold shower exists", habit != NULL);
    check_true("habit merge cold shower day",
               habit != NULL && inbe_habit_completed_day(habit, 20260615));
    inbe_storage_close();

    remove_tree(source);
    remove_tree(dest);
}

static void
test_import_conflict_prefers_data_over_empty(void)
{
    char source[512], dest[512], zip_path[512];
    InbeHabits habits;
    InbeHabit *habit;

    make_clean_root(source, sizeof(source), "conflict-source");
    make_clean_root(dest, sizeof(dest), "conflict-dest");
    make_path(zip_path, sizeof(zip_path), source, "conflict-export.zip");

    check_true("init conflict source", inbe_storage_init(source));
    memset(&habits, 0, sizeof(habits));
    check_int("add conflict source habit",
              inbe_habits_add_custom(&habits, "Meditation",
                                      (Color){224, 124, 104, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              0);
    inbe_storage_set_setting_text("language", "");
    inbe_storage_close();
    insert_raw_habit_day(source, "habit-1", 20260618, 0);
    check_true("reopen conflict source", inbe_storage_init(source));
    check_true("conflict source export", inbe_storage_export_zip(zip_path));
    inbe_storage_close();

    check_true("init conflict dest", inbe_storage_init(dest));
    memset(&habits, 0, sizeof(habits));
    check_int("add conflict dest habit",
              inbe_habits_add_custom(&habits, "Meditation",
                                      (Color){126, 183, 230, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              0);
    inbe_habit_set_day(&habits, 0, 20260618, 1);
    inbe_storage_set_setting_text("language", "en");
    check_true("conflict import",
               inbe_storage_import_zip_ex(zip_path, INBE_STORAGE_IMPORT_DATA_AND_SETTINGS));
    memset(&habits, 0, sizeof(habits));
    check_true("conflict habits load", inbe_storage_habits_load(&habits));
    habit = find_habit_ci(&habits, "Meditation");
    check_true("conflict keeps completed day",
               habit != NULL && inbe_habit_completed_day(habit, 20260618));
    check_str("conflict keeps non-empty setting",
              inbe_storage_get_setting_text("language"), "en");
    inbe_storage_close();

    remove_tree(source);
    remove_tree(dest);
}

static void
test_delete_all_resets_habits_to_empty_storage(void)
{
    char root[512];
    InbeHabits habits;
    long long deleted;

    make_clean_root(root, sizeof(root), "delete-all");
    check_true("init delete all db", inbe_storage_init(root));
    memset(&habits, 0, sizeof(habits));
    check_int("add delete all habit",
              inbe_habits_add_custom(&habits, "Work out",
                                      (Color){99, 196, 165, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              0);
    inbe_habit_set_day(&habits, 0, 20260618, 1);
    check_int("delete all habit count before", inbe_storage_habit_count(), 1);
    check_true("delete all sees habit-only data", inbe_storage_has_any());

    deleted = inbe_storage_delete_all_sessions();
    check_true("delete all removed habit data", deleted >= 2);
    check_int("delete all habit count after", inbe_storage_habit_count(), 0);
    check_true("delete all hides active data", !inbe_storage_has_any());

    inbe_habits_add_default_set(&habits);
    memset(&habits, 0, sizeof(habits));
    check_true("load default habit after delete all", inbe_storage_habits_load(&habits));
    check_int("default habit count after delete all", habits.count, 1);
    check_true("default meditation after delete all",
               strcmp(habits.items[0].name, "Meditation") == 0);
    check_true("old workout day removed",
               !inbe_habit_completed_day(&habits.items[0], 20260618));
    inbe_storage_close();

    remove_tree(root);
}

static void
write_multi_habit_source_database(const char *root, const char *zip_path)
{
    int rounds[] = {77};
    InbeHabits habits;

    check_true("init multi habit source", inbe_storage_init(root));
    check_true("save multi habit source session",
               inbe_storage_save_session_for_activity(rounds, 1, 0, 1, NULL, 0));
    memset(&habits, 0, sizeof(habits));
    check_int("add meditation habit",
              inbe_habits_add_custom(&habits, "Meditation",
                                      (Color){224, 124, 104, 255},
                                      INBE_HABIT_SYNC_ACTIVITIES,
                                      (1 << 0) | (1 << 1)),
              0);
    check_int("add push ups habit",
              inbe_habits_add_custom(&habits, "Push ups",
                                      (Color){180, 132, 220, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              1);
    check_int("add cold shower habit",
              inbe_habits_add_custom(&habits, "Cold Shower",
                                      (Color){99, 196, 165, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              2);
    inbe_habit_set_day(&habits, 0, 20260617, 1);
    inbe_habit_set_day(&habits, 1, 20260617, 1);
    inbe_habit_set_day(&habits, 2, 20260617, 1);
    inbe_storage_set_setting_int("speed", 7);
    inbe_storage_set_setting_text("language", "en");
    inbe_storage_set_setting_text("future_unknown_key", "ignore-me");
    check_true("export multi habit source", inbe_storage_export_zip(zip_path));
    inbe_storage_close();
}

static void
assert_multi_habits_imported(const char *root, int want_speed)
{
    InbeHabits habits;
    InbeHabit *habit;

    check_true("init multi habit import db", inbe_storage_init(root));
    check_int("multi habit imported sessions", inbe_storage_session_count(), 1);
    memset(&habits, 0, sizeof(habits));
    check_true("multi habit load", inbe_storage_habits_load(&habits));
    check_int("multi habit count", habits.count, 3);
    habit = find_habit_ci(&habits, "Meditation");
    check_true("multi meditation exists", habit != NULL);
    check_int("multi meditation sync mode",
              habit != NULL ? habit->sync_mode : -1,
              INBE_HABIT_SYNC_ACTIVITIES);
    check_int("multi meditation sync activity",
              habit != NULL ? habit->sync_activity : -1,
              (1 << 0) | (1 << 1));
    check_true("multi meditation day",
               habit != NULL && inbe_habit_completed_day(habit, 20260617));
    habit = find_habit_ci(&habits, "Push ups");
    check_true("multi push ups day",
               habit != NULL && inbe_habit_completed_day(habit, 20260617));
    habit = find_habit_ci(&habits, "Cold Shower");
    check_true("multi cold shower day",
               habit != NULL && inbe_habit_completed_day(habit, 20260617));
    check_int("multi import speed setting",
              inbe_storage_get_setting_int("speed", -1), want_speed);
    check_str("multi import unknown setting",
              inbe_storage_get_setting_text("future_unknown_key"), NULL);
    inbe_storage_close();
}

static void
test_import_modes_preserve_habits_and_settings_choice(void)
{
    char source[512], dest_data[512], dest_settings[512], zip_path[512];
    InbeStorageImportInfo info;

    make_clean_root(source, sizeof(source), "multi-source");
    make_clean_root(dest_data, sizeof(dest_data), "multi-dest-data");
    make_clean_root(dest_settings, sizeof(dest_settings), "multi-dest-settings");
    make_path(zip_path, sizeof(zip_path), source, "multi-export.zip");
    write_multi_habit_source_database(source, zip_path);

    check_true("init inspect dest", inbe_storage_init(dest_data));
    memset(&info, 0, sizeof(info));
    check_true("inspect multi export", inbe_storage_inspect_import(zip_path, &info));
    check_true("inspect valid", info.valid);
    check_true("inspect sessions", info.has_sessions);
    check_true("inspect habits", info.has_habits);
    check_true("inspect settings", info.has_settings);
    check_int("inspect habit count", info.habit_count, 3);
    inbe_storage_set_setting_int("speed", 1);
    check_true("multi data only import",
               inbe_storage_import_zip_ex(zip_path, INBE_STORAGE_IMPORT_DATA_ONLY));
    inbe_storage_close();
    assert_multi_habits_imported(dest_data, 1);

    check_true("init settings import dest", inbe_storage_init(dest_settings));
    inbe_storage_set_setting_int("speed", 1);
    check_true("multi data settings import",
               inbe_storage_import_zip_ex(zip_path, INBE_STORAGE_IMPORT_DATA_AND_SETTINGS));
    inbe_storage_close();
    assert_multi_habits_imported(dest_settings, 7);

    remove_tree(source);
    remove_tree(dest_data);
    remove_tree(dest_settings);
}

static void
write_legacy_zip(const char *path, const char *prefix)
{
    mz_zip_archive archive;
    char archive_name[256];
    const char rounds[] = "31\n35\n39\n27\n";

    memset(&archive, 0, sizeof(archive));
    snprintf(archive_name, sizeof(archive_name),
             "%s/sessions/2026/06/13/inbe-010203", prefix);
    check_true("create legacy zip", mz_zip_writer_init_file(&archive, path, 0));
    check_true("add legacy metadata",
               mz_zip_writer_add_mem(&archive, "lotus-data/metadata.txt",
                                     "Legacy Inbe export\n", 19, MZ_NO_COMPRESSION));
    check_true("add legacy session",
               mz_zip_writer_add_mem(&archive, archive_name, rounds,
                                     sizeof(rounds) - 1, MZ_BEST_COMPRESSION));
    check_true("finalize legacy zip", mz_zip_writer_finalize_archive(&archive));
    mz_zip_writer_end(&archive);
}

static void
test_legacy_zip_import(void)
{
    char source[512], dest[512], zip_path[512];

    make_clean_root(source, sizeof(source), "legacy-source");
    make_clean_root(dest, sizeof(dest), "legacy-dest");
    make_path(zip_path, sizeof(zip_path), source, "legacy.zip");
    write_legacy_zip(zip_path, "custom-root");

    check_true("init legacy import dest", inbe_storage_init(dest));
    check_true("legacy zip import", inbe_storage_import_zip(zip_path));
    check_int("legacy imported sessions", inbe_storage_session_count(), 1);
    g_seen_round_count = -1;
    g_seen_first_round = -1;
    inbe_storage_list_session_records(legacy_history_callback, NULL);
    check_int("legacy round count", g_seen_round_count, 4);
    check_int("legacy first round", g_seen_first_round, 31);
    inbe_storage_close();

    remove_tree(source);
    remove_tree(dest);
}

static void
write_text_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");

    check_true("open text file", fp != NULL);
    if(fp == NULL)
        return;
    check_true("write text file", fwrite(text, 1, strlen(text), fp) == strlen(text));
    fclose(fp);
}

static void
test_legacy_file_startup_migration(void)
{
    char root[512];
    char session_path[512];

    make_clean_root(root, sizeof(root), "legacy-files");
    make_nested_dir(root, "2026", "06", "13");
    make_path(session_path, sizeof(session_path), root, "2026/06/13/inbe-010203");
    write_text_file(session_path, "31\n35\n39\n27\n");

    check_true("init legacy file migration db", inbe_storage_init(root));
    check_int("legacy file migrated sessions", inbe_storage_session_count(), 1);
    g_seen_round_count = -1;
    g_seen_first_round = -1;
    inbe_storage_list_session_records(legacy_history_callback, NULL);
    check_int("legacy file round count", g_seen_round_count, 4);
    check_int("legacy file first round", g_seen_first_round, 31);
    inbe_storage_close();

    check_true("reopen migrated db", inbe_storage_init(root));
    check_int("legacy file migration one session", inbe_storage_session_count(), 1);
    inbe_storage_close();
    remove_tree(root);
}

static void
write_tickmate_database(const char *path)
{
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    char *error = NULL;

    check_true("open tickmate db", sqlite3_open(path, &db) == SQLITE_OK);
    if(db == NULL)
        return;
    check_true("create tickmate db",
               sqlite3_exec(db,
                            "CREATE TABLE tracks(_id integer primary key autoincrement,"
                            "name text not null,description text not null,icon text not null,"
                            "enabled integer not null,multiple_entries_per_day integer DEFAULT 0,"
                            "color integer DEFAULT 0,\"order\" integer DEFAULT -1);"
                            "CREATE TABLE ticks(_id integer primary key autoincrement,"
                            "_track_id integer,year integer,month integer,day integer,"
                            "hour integer,minute integer,second integer,has_time_info integer DEFAULT 0);"
                            "INSERT INTO tracks(_id,name,description,icon,enabled,multiple_entries_per_day,color,\"order\") "
                            "VALUES(1,'Meditation','Silenced my mind','',1,1,8925,0);"
                            "INSERT INTO ticks(_track_id,year,month,day,hour,minute,second,has_time_info) "
                            "VALUES(1,2026,0,1,0,0,0,0),"
                            "(1,2026,1,2,0,0,0,0),"
                            "(1,2026,2,3,0,0,0,0),"
                            "(1,2026,3,4,0,0,0,0),"
                            "(1,2026,4,5,0,0,0,0),"
                            "(1,2026,5,6,0,0,0,0),"
                            "(1,2026,6,7,0,0,0,0),"
                            "(1,2026,7,8,0,0,0,0),"
                            "(1,2026,8,9,0,0,0,0),"
                            "(1,2026,9,10,0,0,0,0),"
                            "(1,2026,10,11,0,0,0,0),"
                            "(1,2026,11,12,0,0,0,0),"
                            "(1,2025,0,1,0,0,0,0),"
                            "(1,2025,0,1,0,0,0,0);",
                            NULL, NULL, &error) == SQLITE_OK);
    if(error != NULL) {
        fprintf(stderr, "tickmate setup SQL error: %s\n", error);
        sqlite3_free(error);
    }
    check_true("prepare large tickmate insert",
               sqlite3_prepare_v2(db,
                                  "INSERT INTO ticks(_track_id,year,month,day,hour,minute,second,has_time_info) "
                                  "VALUES(1,?1,?2,?3,0,0,0,0)",
                                  -1, &stmt, NULL) == SQLITE_OK);
    if(stmt != NULL) {
        struct tm day;

        check_true("begin large tickmate insert", sqlite3_exec(db, "BEGIN", NULL, NULL, NULL) == SQLITE_OK);
        memset(&day, 0, sizeof(day));
        day.tm_year = 2025 - 1900;
        day.tm_mon = 0;
        day.tm_mday = 1;
        day.tm_hour = 12;
        mktime(&day);
        for(int i = 0; i < 12000; i++) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            sqlite3_bind_int(stmt, 1, day.tm_year + 1900);
            sqlite3_bind_int(stmt, 2, day.tm_mon);
            sqlite3_bind_int(stmt, 3, day.tm_mday);
            check_true("large tickmate insert row", sqlite3_step(stmt) == SQLITE_DONE);
            day.tm_mday++;
            mktime(&day);
        }
        check_true("commit large tickmate insert", sqlite3_exec(db, "COMMIT", NULL, NULL, NULL) == SQLITE_OK);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
}

static void
test_tickmate_db_import(void)
{
    char source[512], dest[512], db_path[512];
    InbeHabits habits;

    make_clean_root(source, sizeof(source), "tickmate-source");
    make_clean_root(dest, sizeof(dest), "tickmate-dest");
    make_path(db_path, sizeof(db_path), source, "tickmate.db");
    write_tickmate_database(db_path);

    check_true("init tickmate import dest", inbe_storage_init(dest));
    check_true("tickmate db import", inbe_storage_import_zip(db_path));
    memset(&habits, 0, sizeof(habits));
    check_true("tickmate habits load", inbe_storage_habits_load(&habits));
    check_int("tickmate habit count", habits.count, 1);
    check_true("tickmate january day", inbe_habit_completed_day(&habits.items[0], 20260101));
    check_true("tickmate february day", inbe_habit_completed_day(&habits.items[0], 20260202));
    check_true("tickmate march day", inbe_habit_completed_day(&habits.items[0], 20260303));
    check_true("tickmate april day", inbe_habit_completed_day(&habits.items[0], 20260404));
    check_true("tickmate may day", inbe_habit_completed_day(&habits.items[0], 20260505));
    check_true("tickmate june day", inbe_habit_completed_day(&habits.items[0], 20260606));
    check_true("tickmate july day", inbe_habit_completed_day(&habits.items[0], 20260707));
    check_true("tickmate august day", inbe_habit_completed_day(&habits.items[0], 20260808));
    check_true("tickmate september day", inbe_habit_completed_day(&habits.items[0], 20260909));
    check_true("tickmate october day", inbe_habit_completed_day(&habits.items[0], 20261010));
    check_true("tickmate november day", inbe_habit_completed_day(&habits.items[0], 20261111));
    check_true("tickmate december day", inbe_habit_completed_day(&habits.items[0], 20261212));
    check_true("tickmate loads thousands of days",
               inbe_habit_completed_day(&habits.items[0], 20571108));
    check_true("tickmate habit name", strcmp(habits.items[0].name, "Meditation") == 0);
    check_int("tickmate enables counter habit", habits.items[0].counter_enabled, 1);
    check_int("tickmate imports count", inbe_habit_day_count(&habits.items[0], 20250101), 3);
    inbe_storage_close();

    remove_tree(source);
    remove_tree(dest);
}

static void
test_tickmate_reimport_recovers_counter_data(void)
{
    char source[512], dest[512], db_path[512];
    InbeHabits habits;

    make_clean_root(source, sizeof(source), "tickmate-reimport-source");
    make_clean_root(dest, sizeof(dest), "tickmate-reimport-dest");
    make_path(db_path, sizeof(db_path), source, "tickmate.db");
    write_tickmate_database(db_path);

    check_true("init tickmate reimport dest", inbe_storage_init(dest));
    memset(&habits, 0, sizeof(habits));
    check_int("add old boolean meditation",
              inbe_habits_add_custom(&habits, "Meditation",
                                      (Color){99, 196, 165, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              0);
    inbe_habit_set_day(&habits, 0, 20250101, 1);
    check_true("tickmate reimport", inbe_storage_import_zip(db_path));
    memset(&habits, 0, sizeof(habits));
    check_true("tickmate reimport load", inbe_storage_habits_load(&habits));
    check_int("tickmate reimport habit count", habits.count, 1);
    check_int("tickmate reimport enables counter", habits.items[0].counter_enabled, 1);
    check_int("tickmate reimport restores count",
              inbe_habit_day_count(&habits.items[0], 20250101), 3);
    inbe_storage_close();

    remove_tree(source);
    remove_tree(dest);
}

static void
test_external_tickmate_db_import(void)
{
    const char *db_path = getenv("INBE_TICKMATE_IMPORT_FIXTURE");
    char dest[512];
    InbeStorageImportInfo info;
    InbeHabits habits;

    if(db_path == NULL || db_path[0] == '\0')
        return;

    make_clean_root(dest, sizeof(dest), "external-tickmate-dest");
    check_true("init external tickmate import dest", inbe_storage_init(dest));
    memset(&info, 0, sizeof(info));
    check_true("inspect external tickmate db", inbe_storage_inspect_import(db_path, &info));
    check_true("external tickmate db valid", info.valid);
    check_true("external tickmate db has habits", info.has_habits);
    check_true("external tickmate db import", inbe_storage_import_zip(db_path));
    memset(&habits, 0, sizeof(habits));
    check_true("external tickmate habits load", inbe_storage_habits_load(&habits));
    check_true("external tickmate habit count", habits.count > 0);
    inbe_storage_close();

    remove_tree(dest);
}

int
main(void)
{
    test_raw_db_import();
    test_zip_db_import();
    test_habit_name_merge_import();
    test_import_conflict_prefers_data_over_empty();
    test_delete_all_resets_habits_to_empty_storage();
    test_import_modes_preserve_habits_and_settings_choice();
    test_legacy_zip_import();
    test_legacy_file_startup_migration();
    test_tickmate_db_import();
    test_tickmate_reimport_recovers_counter_data();
    test_external_tickmate_db_import();
    test_sync_payload_omits_uploaded_state_after_upload_marker();
    test_sync_payload_includes_queued_current_edits();
    test_sync_outbox_preserves_edits_after_snapshot();
    test_sync_apply_preserves_counter_counts();
    test_session_linked_counts_materialize_for_sync();
    test_existing_sessions_materialize_after_habit_save();
    test_session_metadata();

    if(g_failures != 0) {
        fprintf(stderr, "%d storage import test failure(s)\n", g_failures);
        return 1;
    }
    printf("storage import tests passed\n");
    return 0;
}

void
TraceLog(int logLevel, const char *text, ...)
{
    va_list args;
    (void)logLevel;
    va_start(args, text);
    vfprintf(stderr, text, args);
    fputc('\n', stderr);
    va_end(args);
}

bool
FileExists(const char *fileName)
{
    struct stat st;
    return fileName != NULL && stat(fileName, &st) == 0 && S_ISREG(st.st_mode);
}

const char *
GetFileName(const char *filePath)
{
    const char *slash;
    if(filePath == NULL)
        return "";
    slash = strrchr(filePath, '/');
    return slash != NULL ? slash + 1 : filePath;
}

const char *
GetWorkingDirectory(void)
{
    static char cwd[1024];
    if(getcwd(cwd, sizeof(cwd)) == NULL)
        snprintf(cwd, sizeof(cwd), ".");
    return cwd;
}

FilePathList
LoadDirectoryFiles(const char *dirPath)
{
    FilePathList files = {0};
    DIR *dir;
    struct dirent *entry;

    dir = opendir(dirPath);
    if(dir == NULL)
        return files;
    while((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        files.count++;
    }
    rewinddir(dir);
    files.paths = calloc(files.count, sizeof(char *));
    if(files.paths == NULL) {
        files.count = 0;
        closedir(dir);
        return files;
    }
    for(unsigned int i = 0; i < files.count && (entry = readdir(dir)) != NULL;) {
        char path[1024];
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", dirPath, entry->d_name);
        files.paths[i] = strdup(path);
        if(files.paths[i] != NULL)
            i++;
    }
    closedir(dir);
    return files;
}

void
UnloadDirectoryFiles(FilePathList files)
{
    for(unsigned int i = 0; i < files.count; i++)
        free(files.paths[i]);
    free(files.paths);
}
