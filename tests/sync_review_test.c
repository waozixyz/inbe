#include "storage.h"

#include <dirent.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_failures = 0;
static char g_data_root[1024] = "";

void
data_init(void)
{
}

const char *
data_root(void)
{
    return g_data_root;
}

void
TraceLog(int log_level, const char *text, ...)
{
    (void)log_level;
    (void)text;
}

void
sync_client_clear_auth_token(void)
{
}

char *
LoadFileText(const char *file_name)
{
    FILE *file;
    long size;
    char *text;

    file = fopen(file_name, "rb");
    if(file == NULL)
        return NULL;
    if(fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if(size < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    text = (char *)malloc((size_t)size + 1);
    if(text == NULL) {
        fclose(file);
        return NULL;
    }
    if(fread(text, 1, (size_t)size, file) != (size_t)size) {
        free(text);
        fclose(file);
        return NULL;
    }
    text[size] = '\0';
    fclose(file);
    return text;
}

void
UnloadFileText(char *text)
{
    free(text);
}

bool
SaveFileData(const char *file_name, const void *data, int data_size)
{
    FILE *file;
    int ok;

    file = fopen(file_name, "wb");
    if(file == NULL)
        return 0;
    ok = fwrite(data, 1, (size_t)data_size, file) == (size_t)data_size;
    fclose(file);
    return ok;
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
check_false(const char *label, int ok)
{
    if(!ok)
        return;
    fprintf(stderr, "FAIL %s\n", label);
    g_failures++;
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
check_contains(const char *label, const char *text, const char *needle)
{
    if(text != NULL && strstr(text, needle) != NULL)
        return;
    fprintf(stderr, "FAIL %s: missing %s in %s\n",
            label, needle, text != NULL ? text : "(null)");
    g_failures++;
}

static void
check_not_contains(const char *label, const char *text, const char *needle)
{
    if(text == NULL || strstr(text, needle) == NULL)
        return;
    fprintf(stderr, "FAIL %s: unexpected %s in %s\n", label, needle, text);
    g_failures++;
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
    snprintf(out, out_size, "/tmp/inbe-sync-review-test-%ld-%s",
             (long)getpid(), name);
    remove_tree(out);
    check_true("create test root", ensure_dir(out));
    snprintf(g_data_root, sizeof(g_data_root), "%s", out);
}

static int
exec_db_sql(const char *root, const char *sql)
{
    char db_path[1024];
    sqlite3 *db = NULL;
    char *err = NULL;
    int ok;

    snprintf(db_path, sizeof(db_path), "%s/inbe.db", root);
    if(sqlite3_open(db_path, &db) != SQLITE_OK || db == NULL)
        return 0;
    ok = sqlite3_exec(db, sql, NULL, NULL, &err) == SQLITE_OK;
    if(!ok && err != NULL) {
        fprintf(stderr, "SQL error: %s\n", err);
        sqlite3_free(err);
    }
    sqlite3_close(db);
    return ok;
}

static int
read_db_count(const char *root, const char *sql)
{
    char db_path[1024];
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    int count = -1;

    snprintf(db_path, sizeof(db_path), "%s/inbe.db", root);
    if(sqlite3_open(db_path, &db) != SQLITE_OK || db == NULL)
        return count;
    if(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK &&
       sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return count;
}

static void
seed_local_data(const char *root)
{
    check_true("seed local data",
               exec_db_sql(root,
                           "INSERT INTO habits(id,user_id,name,color_r,color_g,color_b,"
                           "sync_mode,sync_activity,counter_enabled,sort_order,deleted_at,updated_at)"
                           "VALUES('local-habit',(SELECT id FROM users LIMIT 1),'Local Breath',10,20,30,0,0,1,1,0,1782300000);"
                           "INSERT INTO habit_days(habit_id,local_date,completed,count,session_count,updated_at)"
                           "VALUES('local-habit',20260624,1,4,0,1782300000);"
                           "INSERT INTO sessions(id,user_id,started_at,local_date,topic,activity,source,"
                           "imported_at,rounds_hash,deleted_at,updated_at)"
                           "VALUES('local-session',(SELECT id FROM users LIMIT 1),1782300000,20260624,1,1,'test',1782300000,101,0,1782300000);"
                           "INSERT INTO session_rounds(session_id,round_index,seconds)"
                           "VALUES('local-session',0,40);"));
}

static const char *
remote_snapshot_response(void)
{
    return "{"
           "\"server_version\":12,"
           "\"server_state_hash\":\"remote-hash-001\","
           "\"full_snapshot_required\":true,"
           "\"changes_complete\":false,"
           "\"changes\":{"
           "\"habits\":[{\"id\":\"remote-habit\",\"name\":\"Remote Breath\","
           "\"color_r\":90,\"color_g\":80,\"color_b\":70,\"sync_mode\":0,"
           "\"sync_activity\":0,\"counter_enabled\":1,\"sort_order\":1,"
           "\"deleted_at\":0,\"updated_at\":\"2026-06-24T10:00:00Z\"}],"
           "\"habit_days\":[{\"habit_id\":\"remote-habit\",\"local_date\":20260624,"
           "\"completed\":true,\"count\":7,\"updated_at\":\"2026-06-24T10:00:00Z\"}],"
           "\"sessions\":[{\"id\":\"remote-session\",\"started_at\":\"2026-06-24T10:00:00Z\","
           "\"local_date\":20260624,\"topic\":2,\"activity\":3,\"source\":\"lyra-test\","
           "\"rounds_hash\":202,\"deleted_at\":0,\"updated_at\":\"2026-06-24T10:00:00Z\","
           "\"rounds\":[{\"round_index\":0,\"hold_seconds\":55}]}],"
           "\"meditation_logs\":[]"
           "}"
           "}";
}

static void
test_full_snapshot_waits_for_review(void)
{
    char root[1024];
    char *local_detail = NULL;
    char *remote_detail = NULL;

    make_clean_root(root, sizeof(root), "pending");
    check_true("init pending db", storage_init(root));
    seed_local_data(root);

    check_true("apply review response",
               storage_apply_sync_response_json(remote_snapshot_response()));
    check_true("review pending", storage_sync_review_pending());
    check_int("local session preserved",
              read_db_count(root, "SELECT COUNT(*) FROM sessions WHERE id='local-session'"), 1);
    check_int("remote session not applied yet",
              read_db_count(root, "SELECT COUNT(*) FROM sessions WHERE id='remote-session'"), 0);
    check_int("local habit preserved",
              read_db_count(root, "SELECT COUNT(*) FROM habits WHERE id='local-habit'"), 1);
    check_int("remote habit not applied yet",
              read_db_count(root, "SELECT COUNT(*) FROM habits WHERE id='remote-habit'"), 0);

    check_true("review detail builds",
               storage_sync_review_details(&local_detail, &remote_detail));
    check_contains("local detail session line", local_detail,
                   "2026-06-24 08:20 Meditation rounds 40s");
    check_contains("local detail habit line", local_detail,
                   "Local Breath activity Wim Hof counter");
    check_contains("local detail habit day line", local_detail,
                   "2026-06-24 Local Breath completed=1 count=4 sessions=0");
    check_contains("remote detail session line", remote_detail,
                   "2026-06-24 07:00 Unknown 3 rounds 55s");
    check_contains("remote detail habit line", remote_detail,
                   "Remote Breath activity Wim Hof counter");
    check_contains("remote detail habit day line", remote_detail,
                   "2026-06-24 Remote Breath completed=1 count=7");
    free(local_detail);
    free(remote_detail);

    storage_close();
    remove_tree(root);
}

static void
test_keep_local_requests_full_replace(void)
{
    char root[1024];
    char *payload;

    make_clean_root(root, sizeof(root), "keep-local");
    check_true("init keep local db", storage_init(root));
    seed_local_data(root);
    check_true("apply keep local review response",
               storage_apply_sync_response_json(remote_snapshot_response()));

    check_true("keep local review choice", storage_apply_pending_sync_review(0));
    check_false("review cleared after keep local", storage_sync_review_pending());
    check_int("local session still present",
              read_db_count(root, "SELECT COUNT(*) FROM sessions WHERE id='local-session'"), 1);
    check_int("remote session still absent",
              read_db_count(root, "SELECT COUNT(*) FROM sessions WHERE id='remote-session'"), 0);

    payload = storage_build_sync_payload_json("test-public-id", "test-public-key");
    check_contains("keep local full replace flag", payload, "\"full_sync_requested\":true");
    check_contains("keep local payload has local session", payload, "local-session");
    check_contains("keep local payload has local habit", payload, "local-habit");
    check_not_contains("keep local cleared stale server hash", payload, "last_server_state_hash");
    storage_free_sync_payload_json(payload);

    storage_close();
    remove_tree(root);
}

static void
test_use_remote_replaces_local_data(void)
{
    char root[1024];
    char *payload;

    make_clean_root(root, sizeof(root), "use-remote");
    check_true("init use remote db", storage_init(root));
    seed_local_data(root);
    check_true("apply use remote review response",
               storage_apply_sync_response_json(remote_snapshot_response()));

    check_true("use remote review choice", storage_apply_pending_sync_review(1));
    check_false("review cleared after use remote", storage_sync_review_pending());
    check_int("local session removed",
              read_db_count(root, "SELECT COUNT(*) FROM sessions WHERE id='local-session'"), 0);
    check_int("remote session applied",
              read_db_count(root, "SELECT COUNT(*) FROM sessions WHERE id='remote-session'"), 1);
    check_int("local habit removed",
              read_db_count(root, "SELECT COUNT(*) FROM habits WHERE id='local-habit'"), 0);
    check_int("remote habit applied",
              read_db_count(root, "SELECT COUNT(*) FROM habits WHERE id='remote-habit'"), 1);

    payload = storage_build_sync_payload_json("test-public-id", "test-public-key");
    check_not_contains("use remote no full replace flag", payload, "full_sync_requested");
    check_contains("use remote remembers server hash", payload,
                   "\"last_server_state_hash\":\"remote-hash-001\"");
    storage_free_sync_payload_json(payload);

    storage_close();
    remove_tree(root);
}

static void
test_normal_response_records_server_hash(void)
{
    char root[1024];
    char *payload;
    const char *response =
        "{"
        "\"server_version\":5,"
        "\"server_state_hash\":\"normal-hash-001\","
        "\"account_alias\":\"waozi\","
        "\"changes\":{\"habits\":[],\"habit_days\":[],\"sessions\":[],\"meditation_logs\":[]}"
        "}";

    make_clean_root(root, sizeof(root), "normal");
    check_true("init normal db", storage_init(root));
    check_true("apply normal response", storage_apply_sync_response_json(response));
    check_false("normal response no review", storage_sync_review_pending());
    check_contains("normal response saves alias",
                   storage_get_setting_text("sync_account_alias"), "waozi");

    payload = storage_build_sync_payload_json("test-public-id", "test-public-key");
    check_contains("normal response hash in next payload", payload,
                   "\"last_server_state_hash\":\"normal-hash-001\"");
    check_not_contains("normal response no full replace", payload, "full_sync_requested");
    storage_free_sync_payload_json(payload);

    storage_close();
    remove_tree(root);
}

int
main(void)
{
    test_full_snapshot_waits_for_review();
    test_keep_local_requests_full_replace();
    test_use_remote_replaces_local_data();
    test_normal_response_records_server_hash();

    if(g_failures != 0) {
        fprintf(stderr, "%d failures\n", g_failures);
        return 1;
    }
    printf("sync review tests passed\n");
    return 0;
}
