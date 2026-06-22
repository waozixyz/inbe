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

    check_true("init source db", storage_init(root));
    check_true("save source session", storage_save_session(rounds, 3, NULL, 0));
    memset(&habits, 0, sizeof(habits));
    habits_add_default_set(&habits);
    habit_set_day(&habits, 0, 20260613, 1);
    habits_save(&habits);
    check_int("source sessions", storage_session_count(), 1);
    storage_close();
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
    check_true("init sync watermark db", storage_init(root));
    memset(&habits, 0, sizeof(habits));
    habits_add_default_set(&habits);
    habit_set_day_count(&habits, 0, 20260612, 12);
    habits_save(&habits);

    storage_close();

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
            check_true("set sync backfill marker raw meta",
                       sqlite3_exec(db,
                                    "INSERT OR REPLACE INTO meta(key,value) VALUES('sync_backfill_v2_done','1');",
                                    NULL, NULL, NULL) == SQLITE_OK);
            if(updated_at > 0) {
                char sql[192];
                snprintf(sql, sizeof(sql),
                         "INSERT OR REPLACE INTO meta(key,value) VALUES('sync_last_upload_at','%lld');",
                         updated_at);
                check_true("set sync same second upload marker",
                           sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK);
            }
            check_true("clear uploaded outbox raw meta",
                       sqlite3_exec(db, "DELETE FROM sync_outbox;", NULL, NULL, NULL) == SQLITE_OK);
            sqlite3_close(db);
        }
    }

    check_true("reopen sync watermark db", storage_init(root));
    payload = storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("habit day omitted from sync payload after upload marker",
               payload != NULL &&
               strstr(payload, "\"local_date\":20260612") == NULL &&
               strstr(payload, "\"count\":12") == NULL);
    storage_free_sync_payload_json(payload);
    storage_close();
    remove_tree(root);
}

static void
test_sync_backfill_includes_existing_habits(void)
{
    char root[512];
    InbeHabits habits;
    char *payload;

    make_clean_root(root, sizeof(root), "sync-existing-habit-backfill");
    check_true("init backfill sync db", storage_init(root));
    memset(&habits, 0, sizeof(habits));
    habits_add_default_set(&habits);
    habits_save(&habits);

    storage_close();

    {
        char db_path[512];
        sqlite3 *db = NULL;
        make_path(db_path, sizeof(db_path), root, "inbe.db");
        check_true("open backfill raw db", sqlite3_open(db_path, &db) == SQLITE_OK);
        if(db != NULL) {
            check_true("mark old full upload done",
                       sqlite3_exec(db,
                                    "INSERT OR REPLACE INTO meta(key,value) VALUES('sync_full_upload_done','1');",
                                    NULL, NULL, NULL) == SQLITE_OK);
            check_true("clear old outbox",
                       sqlite3_exec(db, "DELETE FROM sync_outbox;", NULL, NULL, NULL) == SQLITE_OK);
            sqlite3_close(db);
        }
    }

    check_true("reopen backfill sync db", storage_init(root));
    payload = storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("existing habit included by one-time sync backfill",
               payload != NULL &&
               strstr(payload, "\"habits\":[{") != NULL &&
               strstr(payload, "\"habit_days\"") != NULL);
    storage_free_sync_payload_json(payload);

    storage_close();
    remove_tree(root);
}

static void
test_sync_payload_excludes_local_settings(void)
{
    char root[512];
    InbeHabits habits;
    char *payload;

    make_clean_root(root, sizeof(root), "sync-local-settings");
    check_true("init local settings sync db", storage_init(root));
    storage_set_setting_text("theme", "dark");
    storage_set_setting_text("main_tab", "0");
    memset(&habits, 0, sizeof(habits));
    habits_add_default_set(&habits);

    payload = storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("settings are not present in sync payload",
               payload != NULL &&
               strstr(payload, "\"preferences\"") == NULL &&
               strstr(payload, "\"theme\"") == NULL &&
               strstr(payload, "\"main_tab\"") == NULL);
    storage_free_sync_payload_json(payload);

    storage_close();
    remove_tree(root);
}

static void
test_sync_payload_includes_queued_current_edits(void)
{
    char root[512];
    InbeHabits habits;
    char *payload;

    make_clean_root(root, sizeof(root), "sync-queued-edit");
    check_true("init queued sync db", storage_init(root));
    memset(&habits, 0, sizeof(habits));
    habits_add_default_set(&habits);
    habit_set_day_count(&habits, 0, 20260617, 7);
    habits_save(&habits);

    payload = storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("queued habit day included in sync payload",
               payload != NULL &&
               strstr(payload, "\"local_date\":20260617") != NULL &&
               strstr(payload, "\"count\":7") != NULL);
    storage_free_sync_payload_json(payload);

    storage_close();
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
    check_true("init outbox snapshot db", storage_init(root));
    memset(&habits, 0, sizeof(habits));
    habits_add_default_set(&habits);
    habit_set_day_count(&habits, 0, 20260618, 1);
    habits_save(&habits);

    payload = storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("first queued edit in payload",
               payload != NULL && strstr(payload, "\"local_date\":20260618") != NULL);
    storage_free_sync_payload_json(payload);

    habit_set_day_count(&habits, 0, 20260619, 2);
    habits_save(&habits);
    check_true("apply response clears only snapshotted outbox",
               storage_apply_sync_response_json(empty_response));

    payload = storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("later edit remains queued after snapshot clear",
               payload != NULL &&
               strstr(payload, "\"local_date\":20260619") != NULL &&
               strstr(payload, "\"count\":2") != NULL);
    check_true("earlier edit was cleared after snapshot success",
               payload != NULL && strstr(payload, "\"local_date\":20260618") == NULL);
    storage_free_sync_payload_json(payload);

    storage_close();
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
    check_true("init sync counter db", storage_init(root));
    storage_close();

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

    check_true("reopen sync counter db", storage_init(root));
    check_true("apply lower equal counter", storage_apply_sync_response_json(lower_count_response));
    storage_close();
    check_int("lower equal sync does not reset counter",
              read_raw_habit_day_count(root, habit_id, 20260619), 4);

    check_true("reopen sync counter db for repair", storage_init(root));
    check_true("apply higher equal counter", storage_apply_sync_response_json(higher_count_response));
    storage_close();
    check_int("higher equal sync repairs counter",
              read_raw_habit_day_count(root, habit_id, 20260619), 5);

    remove_tree(root);
}

static void
test_sync_apply_updates_habit_counter_enabled(void)
{
    char root[512];
    InbeHabits habits;
    const char *enabled_response =
        "{\"server_version\":1,\"changes\":{\"habits\":["
        "{\"id\":\"counter-toggle\",\"name\":\"Counter Toggle\",\"color_r\":10,\"color_g\":20,\"color_b\":30,"
        "\"sync_mode\":0,\"sync_activity\":0,\"counter_enabled\":1,\"sort_order\":0,"
        "\"deleted_at\":0,\"updated_at\":\"2026-06-19T21:00:00Z\"}"
        "],\"habit_days\":[],\"sessions\":[]}}";
    const char *disabled_response =
        "{\"server_version\":2,\"changes\":{\"habits\":["
        "{\"id\":\"counter-toggle\",\"name\":\"Counter Toggle\",\"color_r\":10,\"color_g\":20,\"color_b\":30,"
        "\"sync_mode\":0,\"sync_activity\":0,\"counter_enabled\":0,\"sort_order\":0,"
        "\"deleted_at\":0,\"updated_at\":\"2026-06-19T21:01:00Z\"}"
        "],\"habit_days\":[],\"sessions\":[]}}";

    make_clean_root(root, sizeof(root), "sync-counter-enabled");
    check_true("init sync counter enabled db", storage_init(root));
    check_true("apply counter enabled habit", storage_apply_sync_response_json(enabled_response));
    memset(&habits, 0, sizeof(habits));
    check_true("load enabled counter habit", storage_habits_load(&habits));
    check_int("remote enables multiple counts", habits.count, 1);
    check_int("counter enabled after remote enable", habits.items[0].counter_enabled, 1);

    check_true("apply counter disabled habit", storage_apply_sync_response_json(disabled_response));
    memset(&habits, 0, sizeof(habits));
    check_true("reload disabled counter habit", storage_habits_load(&habits));
    check_int("counter disabled after remote disable", habits.items[0].counter_enabled, 0);

    storage_close();
    remove_tree(root);
}

static void
test_stale_habit_save_keeps_synced_remote_habits(void)
{
    char root[512];
    InbeHabits stale_habits;
    InbeHabits loaded_habits;
    const char *remote_response =
        "{\"server_version\":1,\"changes\":{\"habits\":["
        "{\"id\":\"habit-2\",\"name\":\"Push ups\",\"color_r\":99,\"color_g\":196,\"color_b\":165,"
        "\"sync_mode\":0,\"sync_activity\":0,\"counter_enabled\":0,\"sort_order\":1,"
        "\"deleted_at\":0,\"updated_at\":\"2026-06-19T21:00:00Z\"},"
        "{\"id\":\"habit-3\",\"name\":\"Cold Shower\",\"color_r\":99,\"color_g\":196,\"color_b\":165,"
        "\"sync_mode\":0,\"sync_activity\":0,\"counter_enabled\":0,\"sort_order\":2,"
        "\"deleted_at\":0,\"updated_at\":\"2026-06-19T21:01:00Z\"}"
        "],\"habit_days\":["
        "{\"habit_id\":\"habit-2\",\"local_date\":20260619,\"completed\":true,\"count\":1,"
        "\"updated_at\":\"2026-06-19T21:00:00Z\"},"
        "{\"habit_id\":\"habit-3\",\"local_date\":20260619,\"completed\":true,\"count\":1,"
        "\"updated_at\":\"2026-06-19T21:01:00Z\"}"
        "],\"sessions\":[]}}";

    make_clean_root(root, sizeof(root), "sync-stale-habit-save");
    check_true("init stale habit save db", storage_init(root));
    memset(&stale_habits, 0, sizeof(stale_habits));
    habits_add_default_set(&stale_habits);

    check_true("apply remote habits behind stale UI",
               storage_apply_sync_response_json(remote_response));
    habits_save(&stale_habits);

    memset(&loaded_habits, 0, sizeof(loaded_habits));
    check_true("load habits after stale save", storage_habits_load(&loaded_habits));
    check_int("stale save keeps remote habit rows", loaded_habits.count, 3);
    check_true("remote push ups survived", find_habit_ci(&loaded_habits, "Push ups") != NULL);
    check_true("remote cold shower survived", find_habit_ci(&loaded_habits, "Cold Shower") != NULL);

    habits_free(&stale_habits);
    habits_free(&loaded_habits);
    storage_close();
    remove_tree(root);
}

static void
test_sync_payload_resets_cursor_for_orphan_habit_days(void)
{
    char root[512];
    char db_path[512];
    char *payload;
    sqlite3 *db = NULL;

    make_clean_root(root, sizeof(root), "sync-orphan-habit-days");
    check_true("init orphan habit day db", storage_init(root));
    storage_close();
    insert_raw_habit_day(root, "habit-2", 20260617, 1);
    insert_raw_habit_day(root, "habit-3", 20260618, 1);

    make_path(db_path, sizeof(db_path), root, "inbe.db");
    check_true("open orphan cursor raw db", sqlite3_open(db_path, &db) == SQLITE_OK);
    if(db != NULL) {
        check_true("set orphan sync cursor",
                   sqlite3_exec(db,
                                "INSERT OR REPLACE INTO meta(key,value) VALUES('sync_last_server_version','530');"
                                "INSERT OR REPLACE INTO meta(key,value) VALUES('sync_full_upload_done','1');"
                                "INSERT OR REPLACE INTO meta(key,value) VALUES('sync_backfill_v2_done','1');",
                                NULL, NULL, NULL) == SQLITE_OK);
        sqlite3_close(db);
    }

    check_true("reopen orphan habit day db", storage_init(root));
    payload = storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("orphan habit days request full snapshot",
               payload != NULL && strstr(payload, "\"since_server_version\":0") != NULL);
    storage_free_sync_payload_json(payload);

    storage_close();
    remove_tree(root);
}

static void
test_sync_payload_runs_one_time_habit_name_repair(void)
{
    char root[512];
    char db_path[512];
    char *payload;
    const char *empty_response =
        "{\"server_version\":530,\"changes\":{\"habits\":[],\"habit_days\":[],\"sessions\":[]}}";
    sqlite3 *db = NULL;

    make_clean_root(root, sizeof(root), "sync-habit-name-repair");
    check_true("init habit name repair db", storage_init(root));
    storage_close();

    make_path(db_path, sizeof(db_path), root, "inbe.db");
    check_true("open habit name repair raw db", sqlite3_open(db_path, &db) == SQLITE_OK);
    if(db != NULL) {
        check_true("set old sync cursor",
                   sqlite3_exec(db,
                                "INSERT OR REPLACE INTO meta(key,value) VALUES('sync_last_server_version','530');"
                                "INSERT OR REPLACE INTO meta(key,value) VALUES('sync_full_upload_done','1');"
                                "INSERT OR REPLACE INTO meta(key,value) VALUES('sync_backfill_v2_done','1');",
                                NULL, NULL, NULL) == SQLITE_OK);
        sqlite3_close(db);
    }

    check_true("reopen habit name repair db", storage_init(root));
    payload = storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("habit name repair requests full snapshot",
               payload != NULL && strstr(payload, "\"since_server_version\":0") != NULL);
    storage_free_sync_payload_json(payload);

    check_true("apply habit name repair response", storage_apply_sync_response_json(empty_response));
    payload = storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("habit name repair only runs once",
               payload != NULL && strstr(payload, "\"since_server_version\":530") != NULL);
    storage_free_sync_payload_json(payload);

    storage_close();
    remove_tree(root);
}

static void
test_sync_apply_merges_duplicate_habit_names(void)
{
    char root[512];
    InbeHabits habits;
    InbeHabit *merged;
    const char *response =
        "{\"server_version\":1,\"changes\":{\"habits\":["
        "{\"id\":\"habit-1\",\"name\":\"New Habit\",\"color_r\":99,\"color_g\":196,\"color_b\":165,"
        "\"sync_mode\":0,\"sync_activity\":0,\"counter_enabled\":0,\"sort_order\":0,"
        "\"deleted_at\":0,\"updated_at\":\"2026-06-19T21:00:00Z\"},"
        "{\"id\":\"habit-2\",\"name\":\"New Habit\",\"color_r\":99,\"color_g\":196,\"color_b\":165,"
        "\"sync_mode\":0,\"sync_activity\":0,\"counter_enabled\":0,\"sort_order\":1,"
        "\"deleted_at\":0,\"updated_at\":\"2026-06-19T21:01:00Z\"},"
        "{\"id\":\"meditation\",\"name\":\"Meditation\",\"color_r\":126,\"color_g\":183,\"color_b\":230,"
        "\"sync_mode\":1,\"sync_activity\":3,\"counter_enabled\":0,\"sort_order\":2,"
        "\"deleted_at\":0,\"updated_at\":\"2026-06-19T21:02:00Z\"}"
        "],\"habit_days\":["
        "{\"habit_id\":\"habit-1\",\"local_date\":20260617,\"completed\":true,\"count\":1,"
        "\"updated_at\":\"2026-06-19T21:00:00Z\"},"
        "{\"habit_id\":\"habit-2\",\"local_date\":20260618,\"completed\":true,\"count\":1,"
        "\"updated_at\":\"2026-06-19T21:01:00Z\"},"
        "{\"habit_id\":\"meditation\",\"local_date\":20260619,\"completed\":true,\"count\":1,"
        "\"updated_at\":\"2026-06-19T21:02:00Z\"}"
        "],\"sessions\":[]}}";

    make_clean_root(root, sizeof(root), "sync-duplicate-habit-names");
    check_true("init duplicate habit name db", storage_init(root));
    check_true("apply duplicate named habits", storage_apply_sync_response_json(response));
    memset(&habits, 0, sizeof(habits));
    check_true("load merged duplicate habits", storage_habits_load(&habits));
    check_int("duplicate habit names collapse to one tab", habits.count, 2);
    merged = find_habit_ci(&habits, "New Habit");
    check_true("merged habit visible", merged != NULL);
    if(merged != NULL) {
        check_true("first duplicate day kept", habit_completed_day(merged, 20260617));
        check_true("second duplicate day moved", habit_completed_day(merged, 20260618));
    }
    check_true("meditation remains visible", find_habit_ci(&habits, "Meditation") != NULL);

    habits_free(&habits);
    storage_close();
    remove_tree(root);
}

static void
test_sync_apply_preserves_queued_habit_counter_enabled(void)
{
    char root[512];
    char db_path[512];
    char sql[512];
    char response[1024];
    InbeHabits habits;
    char habit_id[INBE_STORAGE_ID_SIZE];
    char *payload;
    sqlite3 *db = NULL;

    make_clean_root(root, sizeof(root), "sync-counter-enabled-queued");
    check_true("init queued counter enabled db", storage_init(root));
    memset(&habits, 0, sizeof(habits));
    habits_add_default_set(&habits);
    snprintf(habit_id, sizeof(habit_id), "%s", habits.items[0].id);
    habits.items[0].counter_enabled = 1;
    habits_save(&habits);
    storage_close();

    make_path(db_path, sizeof(db_path), root, "inbe.db");
    check_true("open queued counter raw db", sqlite3_open(db_path, &db) == SQLITE_OK);
    if(db != NULL) {
        snprintf(sql, sizeof(sql),
                 "UPDATE habits SET updated_at=1781902800 WHERE id='%s';",
                 habit_id);
        check_true("pin queued counter updated_at", sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK);
        sqlite3_close(db);
    }

    snprintf(response, sizeof(response),
             "{\"server_version\":1,\"changes\":{\"habits\":["
             "{\"id\":\"%s\",\"name\":\"Breathe\",\"color_r\":0,\"color_g\":0,\"color_b\":0,"
             "\"sync_mode\":0,\"sync_activity\":0,\"counter_enabled\":0,\"sort_order\":0,"
             "\"deleted_at\":0,\"updated_at\":\"2026-06-19T21:00:00Z\"}"
             "],\"habit_days\":[],\"sessions\":[]}}",
             habit_id);

    check_true("reopen queued counter enabled db", storage_init(root));
    check_true("apply stale equal counter enabled habit", storage_apply_sync_response_json(response));
    memset(&habits, 0, sizeof(habits));
    check_true("reload queued counter enabled habit", storage_habits_load(&habits));
    check_int("queued local counter enabled survives equal remote", habits.items[0].counter_enabled, 1);

    payload = storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("queued local counter enabled remains in payload",
               payload != NULL && strstr(payload, "\"counter_enabled\":1") != NULL);
    storage_free_sync_payload_json(payload);

    storage_close();
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
    int today = habits_today_index();
    InbeHabits habits;
    char *payload;

    make_clean_root(root, sizeof(root), "session-linked-count");
    check_true("init linked counter db", storage_init(root));
    memset(&habits, 0, sizeof(habits));
    habits_add_default_set(&habits);

    check_true("save linked session one",
               storage_save_session_for_activity(first_rounds, 1, 0, 1,
                                                      first_id, sizeof(first_id)));
    check_true("save linked session two",
               storage_save_session_for_activity(second_rounds, 1, 0, 1,
                                                      second_id, sizeof(second_id)));

    memset(&habits, 0, sizeof(habits));
    check_true("load linked counter habits", storage_habits_load(&habits));
    check_int("linked sessions materialize count",
              habit_day_count(&habits.items[0], today), 2);

    payload = storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("linked session count is in sync payload",
               payload != NULL && strstr(payload, "\"count\":2") != NULL);
    storage_free_sync_payload_json(payload);

    check_true("delete linked session one", storage_delete_session(first_id));
    memset(&habits, 0, sizeof(habits));
    check_true("reload linked counter after delete", storage_habits_load(&habits));
    check_int("linked session delete lowers derived count",
              habit_day_count(&habits.items[0], today), 1);

    habit_set_day_count(&habits, 0, today, 4);
    habits_save(&habits);
    check_true("delete linked session two", storage_delete_session(second_id));
    memset(&habits, 0, sizeof(habits));
    check_true("reload linked counter after manual override", storage_habits_load(&habits));
    check_int("linked session delete preserves manual count",
              habit_day_count(&habits.items[0], today), 4);

    storage_close();
    remove_tree(root);
}

static void
test_existing_sessions_materialize_after_habit_save(void)
{
    char root[512];
    int rounds[] = {30};
    int today = habits_today_index();
    InbeHabits habits;
    char *payload;

    make_clean_root(root, sizeof(root), "existing-session-linked-count");
    check_true("init existing linked counter db", storage_init(root));
    check_true("save existing linked session",
               storage_save_session_for_activity(rounds, 1, 0, 1, NULL, 0));

    memset(&habits, 0, sizeof(habits));
    habits_add_default_set(&habits);
    memset(&habits, 0, sizeof(habits));
    check_true("load existing linked counter habits", storage_habits_load(&habits));
    check_int("existing linked session materializes after habit save",
              habit_day_count(&habits.items[0], today), 1);

    payload = storage_build_sync_payload_json("test-hash", "test-public-key");
    check_true("existing linked session count is in sync payload",
               payload != NULL && strstr(payload, "\"count\":1") != NULL);
    storage_free_sync_payload_json(payload);

    storage_close();
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
    check_true("init metadata db", storage_init(root));
    check_true("save metadata session",
               storage_save_session_for_activity(rounds, 2, 2, 3, NULL, 0));
    g_seen_topic = -1;
    g_seen_activity = -1;
    storage_list_session_records(metadata_history_callback, NULL);
    check_int("metadata topic", g_seen_topic, 2);
    check_int("metadata activity", g_seen_activity, 3);
    storage_close();
    remove_tree(root);
}

static void
assert_imported_database(const char *root)
{
    InbeHabits habits;

    check_true("init imported db", storage_init(root));
    check_int("imported sessions", storage_session_count(), 1);
    memset(&habits, 0, sizeof(habits));
    check_true("imported habits load", storage_habits_load(&habits));
    check_int("imported habit count", habits.count, 1);
    check_true("imported habit day", habit_completed_day(&habits.items[0], 20260613));
    storage_close();
}

static void
test_raw_db_import(void)
{
    char source[512], dest[512], db_path[512];

    make_clean_root(source, sizeof(source), "raw-source");
    make_clean_root(dest, sizeof(dest), "raw-dest");
    write_source_database(source);
    make_path(db_path, sizeof(db_path), source, "inbe.db");

    check_true("init raw import dest", storage_init(dest));
    check_true("raw db import", storage_import_zip(db_path));
    storage_close();
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

    check_true("init export source", storage_init(source));
    check_true("export zip", storage_export_zip(zip_path));
    storage_close();

    check_true("init zip import dest", storage_init(dest));
    check_true("zip db import", storage_import_zip(zip_path));
    storage_close();
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

    check_true("init habit merge source", storage_init(source));
    memset(&habits, 0, sizeof(habits));
    check_int("add imported meditation",
              habits_add_custom(&habits, "meditation",
                                      (Color){224, 124, 104, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              0);
    habit_set_day(&habits, 0, 20260613, 1);
    habits_save(&habits);
    check_int("add imported push ups",
              habits_add_custom(&habits, "Push ups",
                                      (Color){180, 132, 220, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              1);
    habit_set_day(&habits, 1, 20260614, 1);
    habits_save(&habits);
    check_int("add imported cold shower",
              habits_add_custom(&habits, "Cold Shower",
                                      (Color){99, 196, 165, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              2);
    habit_set_day(&habits, 2, 20260615, 1);
    habits_save(&habits);
    check_true("habit merge export", storage_export_zip(zip_path));
    storage_close();

    check_true("init habit merge dest", storage_init(dest));
    memset(&habits, 0, sizeof(habits));
    habits_add_default_set(&habits);
    habit_set_day(&habits, 0, 20260612, 1);
    habits_save(&habits);
    check_true("habit merge import", storage_import_zip(zip_path));
    memset(&habits, 0, sizeof(habits));
    check_true("habit merge load", storage_habits_load(&habits));
    check_int("habit merge count", habits.count, 3);
    habit = find_habit_ci(&habits, "Meditation");
    check_true("habit merge meditation exists", habit != NULL);
    check_true("habit merge preserves local case",
               habit != NULL && strcmp(habit->name, "Meditation") == 0);
    check_true("habit merge keeps local day",
               habit != NULL && habit_completed_day(habit, 20260612));
    check_true("habit merge imports day",
               habit != NULL && habit_completed_day(habit, 20260613));
    habit = find_habit_ci(&habits, "Push ups");
    check_true("habit merge push ups exists", habit != NULL);
    check_true("habit merge push ups day",
               habit != NULL && habit_completed_day(habit, 20260614));
    habit = find_habit_ci(&habits, "Cold Shower");
    check_true("habit merge cold shower exists", habit != NULL);
    check_true("habit merge cold shower day",
               habit != NULL && habit_completed_day(habit, 20260615));
    storage_close();

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

    check_true("init conflict source", storage_init(source));
    memset(&habits, 0, sizeof(habits));
    check_int("add conflict source habit",
              habits_add_custom(&habits, "Meditation",
                                      (Color){224, 124, 104, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              0);
    storage_set_setting_text("language", "");
    storage_close();
    insert_raw_habit_day(source, "habit-1", 20260618, 0);
    check_true("reopen conflict source", storage_init(source));
    check_true("conflict source export", storage_export_zip(zip_path));
    storage_close();

    check_true("init conflict dest", storage_init(dest));
    memset(&habits, 0, sizeof(habits));
    check_int("add conflict dest habit",
              habits_add_custom(&habits, "Meditation",
                                      (Color){126, 183, 230, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              0);
    habit_set_day(&habits, 0, 20260618, 1);
    habits_save(&habits);
    storage_set_setting_text("language", "en");
    check_true("conflict import",
               storage_import_zip_ex(zip_path, INBE_STORAGE_IMPORT_DATA_AND_SETTINGS));
    memset(&habits, 0, sizeof(habits));
    check_true("conflict habits load", storage_habits_load(&habits));
    habit = find_habit_ci(&habits, "Meditation");
    check_true("conflict keeps completed day",
               habit != NULL && habit_completed_day(habit, 20260618));
    check_str("conflict keeps non-empty setting",
              storage_get_setting_text("language"), "en");
    storage_close();

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
    check_true("init delete all db", storage_init(root));
    memset(&habits, 0, sizeof(habits));
    check_int("add delete all habit",
              habits_add_custom(&habits, "Work out",
                                      (Color){99, 196, 165, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              0);
    habit_set_day(&habits, 0, 20260618, 1);
    habits_save(&habits);
    check_int("delete all habit count before", storage_habit_count(), 1);
    check_true("delete all sees habit-only data", storage_has_any());

    deleted = storage_delete_all_sessions();
    check_true("delete all removed habit data", deleted >= 2);
    check_int("delete all habit count after", storage_habit_count(), 0);
    check_true("delete all hides active data", !storage_has_any());

    habits_add_default_set(&habits);
    memset(&habits, 0, sizeof(habits));
    check_true("load default habit after delete all", storage_habits_load(&habits));
    check_int("default habit count after delete all", habits.count, 1);
    check_true("default meditation after delete all",
               strcmp(habits.items[0].name, "Meditation") == 0);
    check_true("old workout day removed",
               !habit_completed_day(&habits.items[0], 20260618));
    storage_close();

    remove_tree(root);
}

static void
test_empty_initialized_habits_seed_meditation_on_startup(void)
{
    char root[512];
    char db_path[512];
    sqlite3 *db = NULL;
    InbeHabits habits;

    make_clean_root(root, sizeof(root), "empty-initialized-habits");
    check_true("init empty initialized habit db", storage_init(root));
    storage_mark_habits_initialized();
    storage_close();

    make_path(db_path, sizeof(db_path), root, "inbe.db");
    check_true("open empty initialized raw db", sqlite3_open(db_path, &db) == SQLITE_OK);
    if(db != NULL) {
        check_true("clear empty initialized habits",
                   sqlite3_exec(db, "DELETE FROM habit_days; DELETE FROM habits;",
                                NULL, NULL, NULL) == SQLITE_OK);
        sqlite3_close(db);
    }

    check_true("reopen empty initialized habit db", storage_init(root));
    memset(&habits, 0, sizeof(habits));
    habits_init(&habits);
    check_int("empty initialized startup seeds one habit", habits.count, 1);
    check_str("empty initialized startup seeds meditation", habits.items[0].name, "Meditation");
    habits_free(&habits);

    memset(&habits, 0, sizeof(habits));
    check_true("load seeded meditation from storage", storage_habits_load(&habits));
    check_int("seeded meditation persisted", habits.count, 1);
    check_str("persisted seeded habit name", habits.items[0].name, "Meditation");

    habits_free(&habits);
    storage_close();
    remove_tree(root);
}

static void
write_multi_habit_source_database(const char *root, const char *zip_path)
{
    int rounds[] = {77};
    InbeHabits habits;

    check_true("init multi habit source", storage_init(root));
    check_true("save multi habit source session",
               storage_save_session_for_activity(rounds, 1, 0, 1, NULL, 0));
    memset(&habits, 0, sizeof(habits));
    check_int("add meditation habit",
              habits_add_custom(&habits, "Meditation",
                                      (Color){224, 124, 104, 255},
                                      INBE_HABIT_SYNC_ACTIVITIES,
                                      (1 << 0) | (1 << 1)),
              0);
    check_int("add push ups habit",
              habits_add_custom(&habits, "Push ups",
                                      (Color){180, 132, 220, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              1);
    check_int("add cold shower habit",
              habits_add_custom(&habits, "Cold Shower",
                                      (Color){99, 196, 165, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              2);
    habit_set_day(&habits, 0, 20260617, 1);
    habits_save(&habits);
    habit_set_day(&habits, 1, 20260617, 1);
    habits_save(&habits);
    habit_set_day(&habits, 2, 20260617, 1);
    habits_save(&habits);
    storage_set_setting_int("speed", 7);
    storage_set_setting_text("language", "en");
    storage_set_setting_text("future_unknown_key", "ignore-me");
    check_true("export multi habit source", storage_export_zip(zip_path));
    storage_close();
}

static void
assert_multi_habits_imported(const char *root, int want_speed)
{
    InbeHabits habits;
    InbeHabit *habit;

    check_true("init multi habit import db", storage_init(root));
    check_int("multi habit imported sessions", storage_session_count(), 1);
    memset(&habits, 0, sizeof(habits));
    check_true("multi habit load", storage_habits_load(&habits));
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
               habit != NULL && habit_completed_day(habit, 20260617));
    habit = find_habit_ci(&habits, "Push ups");
    check_true("multi push ups day",
               habit != NULL && habit_completed_day(habit, 20260617));
    habit = find_habit_ci(&habits, "Cold Shower");
    check_true("multi cold shower day",
               habit != NULL && habit_completed_day(habit, 20260617));
    check_int("multi import speed setting",
              storage_get_setting_int("speed", -1), want_speed);
    check_str("multi import unknown setting",
              storage_get_setting_text("future_unknown_key"), NULL);
    storage_close();
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

    check_true("init inspect dest", storage_init(dest_data));
    memset(&info, 0, sizeof(info));
    check_true("inspect multi export", storage_inspect_import(zip_path, &info));
    check_true("inspect valid", info.valid);
    check_true("inspect sessions", info.has_sessions);
    check_true("inspect habits", info.has_habits);
    check_true("inspect settings", info.has_settings);
    check_int("inspect habit count", info.habit_count, 3);
    storage_set_setting_int("speed", 1);
    check_true("multi data only import",
               storage_import_zip_ex(zip_path, INBE_STORAGE_IMPORT_DATA_ONLY));
    storage_close();
    assert_multi_habits_imported(dest_data, 1);

    check_true("init settings import dest", storage_init(dest_settings));
    storage_set_setting_int("speed", 1);
    check_true("multi data settings import",
               storage_import_zip_ex(zip_path, INBE_STORAGE_IMPORT_DATA_AND_SETTINGS));
    storage_close();
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

    check_true("init legacy import dest", storage_init(dest));
    check_true("legacy zip import", storage_import_zip(zip_path));
    check_int("legacy imported sessions", storage_session_count(), 1);
    g_seen_round_count = -1;
    g_seen_first_round = -1;
    storage_list_session_records(legacy_history_callback, NULL);
    check_int("legacy round count", g_seen_round_count, 4);
    check_int("legacy first round", g_seen_first_round, 31);
    storage_close();

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

    check_true("init legacy file migration db", storage_init(root));
    check_int("legacy file migrated sessions", storage_session_count(), 1);
    g_seen_round_count = -1;
    g_seen_first_round = -1;
    storage_list_session_records(legacy_history_callback, NULL);
    check_int("legacy file round count", g_seen_round_count, 4);
    check_int("legacy file first round", g_seen_first_round, 31);
    storage_close();

    check_true("reopen migrated db", storage_init(root));
    check_int("legacy file migration one session", storage_session_count(), 1);
    storage_close();
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

    check_true("init tickmate import dest", storage_init(dest));
    check_true("tickmate db import", storage_import_zip(db_path));
    memset(&habits, 0, sizeof(habits));
    check_true("tickmate habits load", storage_habits_load(&habits));
    check_int("tickmate habit count", habits.count, 1);
    check_true("tickmate january day", habit_completed_day(&habits.items[0], 20260101));
    check_true("tickmate february day", habit_completed_day(&habits.items[0], 20260202));
    check_true("tickmate march day", habit_completed_day(&habits.items[0], 20260303));
    check_true("tickmate april day", habit_completed_day(&habits.items[0], 20260404));
    check_true("tickmate may day", habit_completed_day(&habits.items[0], 20260505));
    check_true("tickmate june day", habit_completed_day(&habits.items[0], 20260606));
    check_true("tickmate july day", habit_completed_day(&habits.items[0], 20260707));
    check_true("tickmate august day", habit_completed_day(&habits.items[0], 20260808));
    check_true("tickmate september day", habit_completed_day(&habits.items[0], 20260909));
    check_true("tickmate october day", habit_completed_day(&habits.items[0], 20261010));
    check_true("tickmate november day", habit_completed_day(&habits.items[0], 20261111));
    check_true("tickmate december day", habit_completed_day(&habits.items[0], 20261212));
    check_true("tickmate loads thousands of days",
               habit_completed_day(&habits.items[0], 20571108));
    check_true("tickmate habit name", strcmp(habits.items[0].name, "Meditation") == 0);
    check_int("tickmate enables counter habit", habits.items[0].counter_enabled, 1);
    check_int("tickmate imports count", habit_day_count(&habits.items[0], 20250101), 3);
    storage_close();

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

    check_true("init tickmate reimport dest", storage_init(dest));
    memset(&habits, 0, sizeof(habits));
    check_int("add old boolean meditation",
              habits_add_custom(&habits, "Meditation",
                                      (Color){99, 196, 165, 255},
                                      INBE_HABIT_SYNC_NONE, 0),
              0);
    habit_set_day(&habits, 0, 20250101, 1);
    habits_save(&habits);
    check_true("tickmate reimport", storage_import_zip(db_path));
    memset(&habits, 0, sizeof(habits));
    check_true("tickmate reimport load", storage_habits_load(&habits));
    check_int("tickmate reimport habit count", habits.count, 1);
    check_int("tickmate reimport enables counter", habits.items[0].counter_enabled, 1);
    check_int("tickmate reimport restores count",
              habit_day_count(&habits.items[0], 20250101), 3);
    storage_close();

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
    check_true("init external tickmate import dest", storage_init(dest));
    memset(&info, 0, sizeof(info));
    check_true("inspect external tickmate db", storage_inspect_import(db_path, &info));
    check_true("external tickmate db valid", info.valid);
    check_true("external tickmate db has habits", info.has_habits);
    check_true("external tickmate db import", storage_import_zip(db_path));
    memset(&habits, 0, sizeof(habits));
    check_true("external tickmate habits load", storage_habits_load(&habits));
    check_true("external tickmate habit count", habits.count > 0);
    storage_close();

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
    test_empty_initialized_habits_seed_meditation_on_startup();
    test_sync_payload_omits_uploaded_state_after_upload_marker();
    test_sync_backfill_includes_existing_habits();
    test_sync_payload_excludes_local_settings();
    test_sync_payload_includes_queued_current_edits();
    test_sync_outbox_preserves_edits_after_snapshot();
    test_sync_apply_preserves_counter_counts();
    test_sync_apply_updates_habit_counter_enabled();
    test_stale_habit_save_keeps_synced_remote_habits();
    test_sync_payload_resets_cursor_for_orphan_habit_days();
    test_sync_payload_runs_one_time_habit_name_repair();
    test_sync_apply_merges_duplicate_habit_names();
    test_sync_apply_preserves_queued_habit_counter_enabled();
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
