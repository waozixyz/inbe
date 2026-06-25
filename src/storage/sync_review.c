#include "storage.h"
#include "db.h"

#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STORAGE_SYNC_PENDING_REVIEW_KEY "sync_pending_review_pending"
#define STORAGE_SYNC_APPLY_REVIEW_KEY "sync_apply_pending_review"
#define STORAGE_SYNC_FULL_REPLACE_KEY "sync_full_replace_requested"
#define STORAGE_SYNC_LAST_SERVER_HASH_KEY "sync_last_server_state_hash"

typedef struct ReviewText {
    char *data;
    size_t len;
    size_t cap;
    int ok;
} ReviewText;

static int
review_reserve(ReviewText *text, size_t extra)
{
    char *next;
    size_t next_cap;

    if(text == NULL || !text->ok)
        return 0;
    if(extra <= text->cap - text->len)
        return 1;
    next_cap = text->cap > 0 ? text->cap : 2048;
    while(extra > next_cap - text->len)
        next_cap *= 2;
    next = (char *)realloc(text->data, next_cap);
    if(next == NULL) {
        text->ok = 0;
        return 0;
    }
    text->data = next;
    text->cap = next_cap;
    return 1;
}

static void
review_append(ReviewText *text, const char *value)
{
    size_t len;

    if(text == NULL || value == NULL)
        return;
    len = strlen(value);
    if(!review_reserve(text, len + 1))
        return;
    memcpy(text->data + text->len, value, len);
    text->len += len;
    text->data[text->len] = '\0';
}

static void
review_appendf(ReviewText *text, const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int needed;

    if(text == NULL || fmt == NULL || !text->ok)
        return;
    va_start(args, fmt);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if(needed < 0) {
        text->ok = 0;
        va_end(args);
        return;
    }
    if(review_reserve(text, (size_t)needed + 1)) {
        vsnprintf(text->data + text->len, text->cap - text->len, fmt, args);
        text->len += (size_t)needed;
    }
    va_end(args);
}

static void
storage_sync_review_path(char *out, size_t out_size)
{
    const char *name = "sync-review.json";
    size_t root_len;
    size_t name_len;

    if(out == NULL || out_size == 0)
        return;
    root_len = strlen(g_storage.root);
    name_len = strlen(name);
    if(root_len + 1 + name_len + 1 > out_size) {
        out[0] = '\0';
        return;
    }
    snprintf(out, out_size, "%s/%s", g_storage.root, name);
}

int
storage_sync_review_write_json(const char *json)
{
    char path[INBE_STORAGE_PATH_SIZE];
    FILE *file;
    size_t len;

    if(json == NULL)
        return 0;
    storage_sync_review_path(path, sizeof(path));
    if(path[0] == '\0')
        return 0;
    file = fopen(path, "wb");
    if(file == NULL)
        return 0;
    len = strlen(json);
    if(fwrite(json, 1, len, file) != len) {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

static char *
storage_sync_review_read_json(void)
{
    char path[INBE_STORAGE_PATH_SIZE];
    FILE *file;
    long size;
    char *data;

    storage_sync_review_path(path, sizeof(path));
    if(path[0] == '\0')
        return NULL;
    file = fopen(path, "rb");
    if(file == NULL)
        return NULL;
    if(fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if(size < 0 || size > 8 * 1024 * 1024) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    data = (char *)malloc((size_t)size + 1);
    if(data == NULL) {
        fclose(file);
        return NULL;
    }
    if(fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return NULL;
    }
    data[size] = '\0';
    fclose(file);
    return data;
}

void
storage_sync_review_delete_json(void)
{
    char path[INBE_STORAGE_PATH_SIZE];
    storage_sync_review_path(path, sizeof(path));
    if(path[0] != '\0')
        remove(path);
}

int
storage_sync_review_pending(void)
{
    return get_meta_int64(STORAGE_SYNC_PENDING_REVIEW_KEY, 0) != 0;
}

static const char *
review_activity_name(int activity)
{
    switch(activity) {
    case 0: return "Wim Hof";
    case 1: return "Meditation";
    default: break;
    }
    return "Unknown";
}

static void
review_append_activity_heading(ReviewText *out, int activity)
{
    review_append(out, review_activity_name(activity));
    if(activity != 0 && activity != 1)
        review_appendf(out, " %d", activity);
    review_append(out, "\n");
}

static void
review_append_local_rounds(ReviewText *out, const char *session_id)
{
    sqlite3_stmt *stmt = NULL;
    int first = 1;

    if(g_storage.db == NULL || session_id == NULL)
        return;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT seconds FROM session_rounds WHERE session_id=?1 ORDER BY round_index",
                          -1, &stmt, NULL) != SQLITE_OK)
        return;
    bind_text(stmt, 1, session_id);
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        if(!first)
            review_append(out, ",");
        first = 0;
        review_appendf(out, "%ds", sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);
    if(first)
        review_append(out, "none");
}

static void
review_append_local(ReviewText *out)
{
    sqlite3_stmt *stmt = NULL;
    int rows = 0;
    int last_activity = -1000000;

    review_append(out, "Sessions\n");
    if(g_storage.db != NULL &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT id,local_date,strftime('%H:%M',started_at,'unixepoch','localtime'),activity,deleted_at "
                          "FROM sessions WHERE user_id=?1 ORDER BY activity,local_date DESC,started_at DESC,id",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, g_storage.user_id);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            const char *id = (const char *)sqlite3_column_text(stmt, 0);
            int local_date = sqlite3_column_int(stmt, 1);
            const char *time_text = (const char *)sqlite3_column_text(stmt, 2);
            int activity = sqlite3_column_int(stmt, 3);
            if(activity != last_activity) {
                if(rows > 0)
                    review_append(out, "\n");
                review_append_activity_heading(out, activity);
                last_activity = activity;
            }
            review_appendf(out, "%04d-%02d-%02d %s\n",
                           local_date / 10000, (local_date / 100) % 100,
                           local_date % 100, time_text != NULL ? time_text : "--:--");
            review_append(out, "rounds ");
            review_append_local_rounds(out, id);
            if(sqlite3_column_int64(stmt, 4) != 0)
                review_append(out, " deleted");
            review_append(out, "\n");
            rows++;
        }
        sqlite3_finalize(stmt);
    }
    if(rows == 0)
        review_append(out, "No sessions\n");

    rows = 0;
    review_append(out, "\nHabit days\n");
    if(g_storage.db != NULL &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT h.name,hd.local_date,hd.count "
                          "FROM habit_days hd JOIN habits h ON h.id=hd.habit_id "
                          "WHERE h.user_id=?1 AND (hd.completed!=0 OR hd.count>0 OR hd.session_count>0) "
                          "ORDER BY h.name,hd.local_date DESC",
                          -1, &stmt, NULL) == SQLITE_OK) {
        char last_name[256] = "";
        bind_text(stmt, 1, g_storage.user_id);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            int local_date = sqlite3_column_int(stmt, 1);
            const char *name = (const char *)sqlite3_column_text(stmt, 0);
            const char *safe_name = name != NULL && name[0] != '\0' ? name : "(unnamed)";
            if(strcmp(last_name, safe_name) != 0) {
                if(rows > 0)
                    review_append(out, "\n");
                review_append(out, safe_name);
                review_append(out, "\n");
                snprintf(last_name, sizeof(last_name), "%s", safe_name);
            }
            review_appendf(out, "%04d-%02d-%02d",
                           local_date / 10000, (local_date / 100) % 100,
                           local_date % 100);
            if(sqlite3_column_int(stmt, 2) > 0)
                review_appendf(out, " count %d", sqlite3_column_int(stmt, 2));
            review_append(out, "\n");
            rows++;
        }
        sqlite3_finalize(stmt);
    }
    if(rows == 0)
        review_append(out, "No habit days\n");
}

static void
review_append_remote(ReviewText *out, const char *json)
{
    sqlite3_stmt *stmt = NULL;
    int rows = 0;
    int last_activity = -1000000;

    review_append(out, "Sessions\n");
    if(g_storage.db != NULL && json != NULL &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT CAST(COALESCE(json_extract(s.value,'$.local_date'),0) AS INTEGER),"
                          "       COALESCE(strftime('%H:%M',json_extract(s.value,'$.started_at'),'localtime'),'--:--'),"
                          "       CAST(COALESCE(json_extract(s.value,'$.activity'),0) AS INTEGER),"
                          "       CAST(COALESCE(json_extract(s.value,'$.deleted_at'),0) AS INTEGER),"
                          "       COALESCE((SELECT group_concat(CAST(COALESCE(json_extract(r.value,'$.hold_seconds'),0) AS INTEGER) || 's', ',') FROM json_each(s.value,'$.rounds') AS r),'none') "
                          "FROM json_each(?1,'$.changes.sessions') AS s ORDER BY 3,1 DESC,2 DESC",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, json);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            int local_date = sqlite3_column_int(stmt, 0);
            const char *time_text = (const char *)sqlite3_column_text(stmt, 1);
            int activity = sqlite3_column_int(stmt, 2);
            const char *rounds = (const char *)sqlite3_column_text(stmt, 4);
            if(activity != last_activity) {
                if(rows > 0)
                    review_append(out, "\n");
                review_append_activity_heading(out, activity);
                last_activity = activity;
            }
            review_appendf(out, "%04d-%02d-%02d %s\n",
                           local_date / 10000, (local_date / 100) % 100,
                           local_date % 100, time_text != NULL ? time_text : "--:--");
            review_appendf(out, "rounds %s", rounds != NULL ? rounds : "none");
            if(sqlite3_column_int64(stmt, 3) != 0)
                review_append(out, " deleted");
            review_append(out, "\n");
            rows++;
        }
        sqlite3_finalize(stmt);
    }
    if(rows == 0)
        review_append(out, "No sessions\n");

    rows = 0;
    review_append(out, "\nHabit days\n");
    if(g_storage.db != NULL && json != NULL &&
       sqlite3_prepare_v2(g_storage.db,
                          "SELECT COALESCE((SELECT COALESCE(json_extract(h.value,'$.name'),'') FROM json_each(?1,'$.changes.habits') AS h WHERE COALESCE(json_extract(h.value,'$.id'),'')=COALESCE(json_extract(d.value,'$.habit_id'),'')),COALESCE(json_extract(d.value,'$.habit_id'),'')),"
                          "       CAST(COALESCE(json_extract(d.value,'$.local_date'),0) AS INTEGER),"
                          "       CAST(COALESCE(json_extract(d.value,'$.count'),0) AS INTEGER) "
                          "FROM json_each(?1,'$.changes.habit_days') AS d "
                          "WHERE json_extract(d.value,'$.completed') OR CAST(COALESCE(json_extract(d.value,'$.count'),0) AS INTEGER)>0 "
                          "ORDER BY 1,2 DESC",
                          -1, &stmt, NULL) == SQLITE_OK) {
        char last_name[256] = "";
        bind_text(stmt, 1, json);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            int local_date = sqlite3_column_int(stmt, 1);
            const char *name = (const char *)sqlite3_column_text(stmt, 0);
            const char *safe_name = name != NULL && name[0] != '\0' ? name : "(unknown)";
            if(strcmp(last_name, safe_name) != 0) {
                if(rows > 0)
                    review_append(out, "\n");
                review_append(out, safe_name);
                review_append(out, "\n");
                snprintf(last_name, sizeof(last_name), "%s", safe_name);
            }
            review_appendf(out, "%04d-%02d-%02d",
                           local_date / 10000, (local_date / 100) % 100,
                           local_date % 100);
            if(sqlite3_column_int(stmt, 2) > 0)
                review_appendf(out, " count %d", sqlite3_column_int(stmt, 2));
            review_append(out, "\n");
            rows++;
        }
        sqlite3_finalize(stmt);
    }
    if(rows == 0)
        review_append(out, "No habit days\n");
}

int
storage_sync_review_details(char **local_out, char **remote_out)
{
    char *json = storage_sync_review_read_json();
    ReviewText local = {0};
    ReviewText remote = {0};

    if(local_out != NULL)
        *local_out = NULL;
    if(remote_out != NULL)
        *remote_out = NULL;
    local.ok = 1;
    remote.ok = 1;
    review_append_local(&local);
    review_append_remote(&remote, json);
    free(json);
    if(!local.ok || !remote.ok) {
        free(local.data);
        free(remote.data);
        return 0;
    }
    if(local_out != NULL)
        *local_out = local.data != NULL ? local.data : strdup("");
    else
        free(local.data);
    if(remote_out != NULL)
        *remote_out = remote.data != NULL ? remote.data : strdup("");
    else
        free(remote.data);
    return (local_out == NULL || *local_out != NULL) &&
           (remote_out == NULL || *remote_out != NULL);
}

int
storage_apply_pending_sync_review(int use_remote)
{
    char *json = storage_sync_review_read_json();
    char *copy;
    int ok;

    if(json == NULL || json[0] == '\0') {
        free(json);
        return 0;
    }
    if(!use_remote) {
        set_meta(STORAGE_SYNC_PENDING_REVIEW_KEY, "");
        storage_sync_review_delete_json();
        set_meta(STORAGE_SYNC_LAST_SERVER_HASH_KEY, "");
        storage_reset_sync_state();
        set_meta_int64(STORAGE_SYNC_FULL_REPLACE_KEY, 1);
        free(json);
        return 1;
    }
    copy = strdup(json);
    free(json);
    if(copy == NULL)
        return 0;
    if(!exec_sql("BEGIN IMMEDIATE") ||
       !exec_sql("DELETE FROM session_rounds") ||
       !exec_sql("DELETE FROM sessions") ||
       !exec_sql("DELETE FROM habit_days") ||
       !exec_sql("DELETE FROM habits") ||
       !exec_sql("DELETE FROM sync_outbox") ||
       !exec_sql("COMMIT")) {
        exec_sql("ROLLBACK");
        free(copy);
        return 0;
    }
    set_meta_int64(STORAGE_SYNC_APPLY_REVIEW_KEY, 1);
    ok = storage_apply_sync_response_json(copy);
    free(copy);
    if(ok) {
        exec_sql("DELETE FROM sync_outbox");
        g_storage.pending_sync_outbox_seq = 0;
        set_meta(STORAGE_SYNC_PENDING_REVIEW_KEY, "");
        storage_sync_review_delete_json();
        storage_schedule_persist();
    }
    return ok;
}
