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
#define STORAGE_SYNC_AUTO_APPLY_VISIBLE_CHANGE_LIMIT 2

typedef struct ReviewText {
    char *data;
    size_t len;
    size_t cap;
    int ok;
} ReviewText;

static void review_append_local(ReviewText *out);
static void review_append_remote(ReviewText *out, const char *json);
static void review_append_unique_lines(ReviewText *out, const char *prefix,
                                       const char *lines, const char *other);
static int review_has_local_activity(void);
static int review_has_pending_outbox(void);
static int review_pending_outbox_sessions_only(void);
static int review_has_unknown_activity(const char *json);
static int review_small_session_only_change(const char *diff);
static int review_visible_change_count(const char *diff);

static int
review_sql_bool(const char *sql, const char *text_arg, int fallback)
{
    sqlite3_stmt *stmt = NULL;
    int result = fallback;

    if(g_storage.db == NULL || sql == NULL)
        return fallback;
    if(sqlite3_prepare_v2(g_storage.db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return fallback;
    if(text_arg != NULL)
        bind_text(stmt, 1, text_arg);
    if(sqlite3_step(stmt) == SQLITE_ROW)
        result = sqlite3_column_int(stmt, 0) != 0;
    sqlite3_finalize(stmt);
    return result;
}

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
    if(!storage_join_path(out, out_size, g_storage.root, "sync-review.json") &&
       out != NULL && out_size > 0)
        out[0] = '\0';
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

static int
storage_sync_review_diff_for_json(const char *json, int include_empty_message,
                                  char **diff_out)
{
    ReviewText local = {0};
    ReviewText remote = {0};
    ReviewText diff = {0};

    if(diff_out != NULL)
        *diff_out = NULL;
    if(diff_out == NULL)
        return 0;
    local.ok = 1;
    remote.ok = 1;
    diff.ok = 1;
    review_append_local(&local);
    review_append_remote(&remote, json);
    if(!local.ok || !remote.ok) {
        free(local.data);
        free(remote.data);
        return 0;
    }
    review_append_unique_lines(&diff, "- ", local.data != NULL ? local.data : "",
                               remote.data != NULL ? remote.data : "");
    review_append_unique_lines(&diff, "+ ", remote.data != NULL ? remote.data : "",
                               local.data != NULL ? local.data : "");
    if(diff.len == 0 && include_empty_message)
        review_append(&diff, "No visible differences.\n");
    free(local.data);
    free(remote.data);
    if(!diff.ok) {
        free(diff.data);
        return 0;
    }
    *diff_out = diff.data != NULL ? diff.data : strdup("");
    return *diff_out != NULL;
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
    case 2: return "Sun Salutation";
    default: break;
    }
    return NULL;
}

static int
review_activity_uses_duration(int activity)
{
    return activity == 1;
}

static void
review_append_activity_heading(ReviewText *out, int activity)
{
    const char *name = review_activity_name(activity);
    if(name == NULL)
        return;
    review_append(out, name);
    review_append(out, "\n");
}

static void
review_append_date(ReviewText *out, int local_date)
{
    review_appendf(out, "%04d-%02d-%02d",
                   local_date / 10000, (local_date / 100) % 100,
                   local_date % 100);
}

static void
review_append_date_time(ReviewText *out, int local_date, const char *time_text)
{
    review_append_date(out, local_date);
    review_appendf(out, " %s\n", time_text != NULL ? time_text : "--:--");
}

static void
review_append_habit_day(ReviewText *out, char *last_name, size_t last_name_size,
                        int *rows, const char *name, int local_date, int count)
{
    const char *safe_name = name != NULL && name[0] != '\0' ? name : "(unnamed)";

    if(strcmp(last_name, safe_name) != 0) {
        if(*rows > 0)
            review_append(out, "\n");
        review_append(out, safe_name);
        review_append(out, "\n");
        snprintf(last_name, last_name_size, "%s", safe_name);
    }
    review_append_date(out, local_date);
    if(count > 0)
        review_appendf(out, " count %d", count);
    review_append(out, "\n");
    (*rows)++;
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
review_append_local_duration(ReviewText *out, const char *session_id)
{
    sqlite3_stmt *stmt = NULL;
    int total = 0;

    if(g_storage.db == NULL || session_id == NULL)
        return;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT COALESCE(SUM(seconds),0) FROM session_rounds WHERE session_id=?1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return;
    bind_text(stmt, 1, session_id);
    if(sqlite3_step(stmt) == SQLITE_ROW)
        total = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    review_appendf(out, "duration %ds", total);
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
                          "FROM sessions WHERE user_id=?1 AND deleted_at=0 ORDER BY activity,local_date DESC,started_at DESC,id",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, g_storage.user_id);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            const char *id = (const char *)sqlite3_column_text(stmt, 0);
            int local_date = sqlite3_column_int(stmt, 1);
            const char *time_text = (const char *)sqlite3_column_text(stmt, 2);
            int activity = sqlite3_column_int(stmt, 3);
            if(review_activity_name(activity) == NULL)
                continue;
            if(activity != last_activity) {
                if(rows > 0)
                    review_append(out, "\n");
                review_append_activity_heading(out, activity);
                last_activity = activity;
            }
            review_append_date_time(out, local_date, time_text);
            if(activity == 2) {
                review_append(out, "session 1");
            } else if(review_activity_uses_duration(activity)) {
                review_append_local_duration(out, id);
            } else {
                review_append(out, "rounds ");
                review_append_local_rounds(out, id);
            }
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
            review_append_habit_day(out, last_name, sizeof(last_name), &rows,
                                    name, local_date, sqlite3_column_int(stmt, 2));
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
                          "       COALESCE((SELECT group_concat(CAST(COALESCE(json_extract(r.value,'$.hold_seconds'),0) AS INTEGER) || 's', ',') FROM json_each(s.value,'$.rounds') AS r),'none'),"
                          "       CAST(COALESCE((SELECT SUM(CAST(COALESCE(json_extract(r.value,'$.hold_seconds'),0) AS INTEGER)) FROM json_each(s.value,'$.rounds') AS r),0) AS INTEGER) "
                          "FROM json_each(?1,'$.changes.sessions') AS s "
                          "WHERE CAST(COALESCE(json_extract(s.value,'$.deleted_at'),0) AS INTEGER)=0 "
                          "ORDER BY 3,1 DESC,2 DESC",
                          -1, &stmt, NULL) == SQLITE_OK) {
        bind_text(stmt, 1, json);
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            int local_date = sqlite3_column_int(stmt, 0);
            const char *time_text = (const char *)sqlite3_column_text(stmt, 1);
            int activity = sqlite3_column_int(stmt, 2);
            const char *rounds = (const char *)sqlite3_column_text(stmt, 4);
            int duration = sqlite3_column_int(stmt, 5);
            if(review_activity_name(activity) == NULL)
                continue;
            if(activity != last_activity) {
                if(rows > 0)
                    review_append(out, "\n");
                review_append_activity_heading(out, activity);
                last_activity = activity;
            }
            review_append_date_time(out, local_date, time_text);
            if(activity == 2)
                review_append(out, "session 1");
            else if(review_activity_uses_duration(activity))
                review_appendf(out, "duration %ds", duration);
            else
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
                          "SELECT COALESCE((SELECT COALESCE(json_extract(h.value,'$.name'),'') FROM json_each(?1,'$.changes.habits') AS h WHERE COALESCE(json_extract(h.value,'$.id'),'')=COALESCE(json_extract(d.value,'$.habit_id'),'')),"
                          "                (SELECT name FROM habits WHERE id=COALESCE(json_extract(d.value,'$.habit_id'),'') LIMIT 1),"
                          "                ''),"
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
            if(name == NULL || name[0] == '\0')
                continue;
            review_append_habit_day(out, last_name, sizeof(last_name), &rows,
                                    name, local_date, sqlite3_column_int(stmt, 2));
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

static int
review_line_equal(const char *line, size_t line_len, const char *other)
{
    const char *p = other;
    const char *end;
    size_t len;

    if(line == NULL || other == NULL)
        return 0;
    while(*p != '\0') {
        end = strchr(p, '\n');
        len = end != NULL ? (size_t)(end - p) : strlen(p);
        if(len == line_len && strncmp(p, line, line_len) == 0)
            return 1;
        if(end == NULL)
            break;
        p = end + 1;
    }
    return 0;
}

static void
review_append_unique_lines(ReviewText *out, const char *prefix,
                           const char *lines, const char *other)
{
    const char *p = lines;
    const char *end;
    size_t len;

    if(out == NULL || prefix == NULL || lines == NULL)
        return;
    while(*p != '\0') {
        end = strchr(p, '\n');
        len = end != NULL ? (size_t)(end - p) : strlen(p);
        if(len > 0 && !review_line_equal(p, len, other)) {
            review_append(out, prefix);
            if(!review_reserve(out, len + 2))
                return;
            memcpy(out->data + out->len, p, len);
            out->len += len;
            out->data[out->len++] = '\n';
            out->data[out->len] = '\0';
        }
        if(end == NULL)
            break;
        p = end + 1;
    }
}

int
storage_sync_review_diff(char **diff_out)
{
    char *json = storage_sync_review_read_json();
    int ok = storage_sync_review_diff_for_json(json, 1, diff_out);

    free(json);
    return ok;
}

int
storage_sync_review_has_visible_diff(void)
{
    char *json = storage_sync_review_read_json();
    int result = storage_sync_review_json_has_visible_diff(json);

    free(json);
    return result;
}

int
storage_sync_review_clear_if_no_visible_diff(void)
{
    if(!storage_sync_review_pending())
        return 0;
    if(storage_sync_review_has_visible_diff())
        return 0;
    set_meta(STORAGE_SYNC_PENDING_REVIEW_KEY, "");
    storage_sync_review_delete_json();
    set_meta_int64(STORAGE_SYNC_APPLY_REVIEW_KEY, 0);
    storage_schedule_persist();
    return 1;
}

int
storage_sync_review_apply_remote_if_local_empty(void)
{
    if(!storage_sync_review_pending())
        return 0;
    if(review_has_local_activity())
        return 0;
    return storage_apply_pending_sync_review(1);
}

int
storage_sync_review_json_has_visible_diff(const char *json)
{
    char *diff = NULL;
    int has_diff;

    if(!storage_sync_review_diff_for_json(json, 0, &diff)) {
        free(diff);
        return 1;
    }
    has_diff = diff != NULL && diff[0] != '\0' &&
               (review_has_pending_outbox() || review_has_unknown_activity(json));
    free(diff);
    return has_diff;
}

int
storage_sync_review_json_should_auto_apply_remote(const char *json)
{
    char *diff = NULL;
    int result;

    if(review_has_unknown_activity(json))
        return 0;
    if(!storage_sync_review_diff_for_json(json, 0, &diff)) {
        free(diff);
        return 0;
    }
    if(diff == NULL || diff[0] == '\0') {
        free(diff);
        return 1;
    }
    if(!review_has_local_activity()) {
        free(diff);
        return 1;
    }
    if(!review_has_pending_outbox()) {
        free(diff);
        return 1;
    }
    result = review_small_session_only_change(diff);
    free(diff);
    return result;
}

static int
review_has_local_activity(void)
{
    return review_sql_bool("SELECT EXISTS("
                           " SELECT 1 FROM sessions WHERE deleted_at=0"
                           " UNION ALL"
                           " SELECT 1 FROM habit_days "
                           " WHERE completed!=0 OR count>0 OR session_count>0"
                           " LIMIT 1"
                           ")",
                           NULL, 1);
}

static int
review_has_pending_outbox(void)
{
    return review_sql_bool("SELECT EXISTS(SELECT 1 FROM sync_outbox LIMIT 1)",
                           NULL, 1);
}

static int
review_pending_outbox_sessions_only(void)
{
    return review_sql_bool(
        "SELECT EXISTS(SELECT 1 FROM sync_outbox LIMIT 1) "
        "AND NOT EXISTS(SELECT 1 FROM sync_outbox WHERE entity_type<>'session')",
        NULL, 0);
}

static int
review_has_unknown_activity(const char *json)
{
    if(review_sql_bool("SELECT EXISTS(SELECT 1 FROM sessions "
                       "WHERE deleted_at=0 AND activity NOT IN (0,1,2))",
                       NULL, 1))
        return 1;

    if(json == NULL)
        return 0;
    return review_sql_bool(
        "SELECT EXISTS("
        " SELECT 1 FROM json_each(?1,'$.changes.sessions') AS s "
        " WHERE CAST(COALESCE(json_extract(s.value,'$.deleted_at'),0) AS INTEGER)=0 "
        " AND CAST(COALESCE(json_extract(s.value,'$.activity'),0) AS INTEGER) NOT IN (0,1,2)"
        ")",
        json, 1);
}

static int
review_small_session_only_change(const char *diff)
{
    int count;

    if(!review_pending_outbox_sessions_only())
        return 0;
    count = review_visible_change_count(diff);
    return count > 0 && count <= STORAGE_SYNC_AUTO_APPLY_VISIBLE_CHANGE_LIMIT;
}

static int
review_line_starts_with_date(const char *line, size_t len)
{
    if(line == NULL || len < 10)
        return 0;
    return line[0] >= '0' && line[0] <= '9' &&
           line[1] >= '0' && line[1] <= '9' &&
           line[2] >= '0' && line[2] <= '9' &&
           line[3] >= '0' && line[3] <= '9' &&
           line[4] == '-' &&
           line[5] >= '0' && line[5] <= '9' &&
           line[6] >= '0' && line[6] <= '9' &&
           line[7] == '-' &&
           line[8] >= '0' && line[8] <= '9' &&
           line[9] >= '0' && line[9] <= '9';
}

static int
review_line_equals_literal(const char *line, size_t len, const char *literal)
{
    size_t literal_len;

    if(line == NULL || literal == NULL)
        return 0;
    literal_len = strlen(literal);
    return len == literal_len && strncmp(line, literal, len) == 0;
}

static int
review_visible_change_count(const char *diff)
{
    const char *p = diff;
    const char *line;
    const char *end;
    size_t len;
    int date_lines = 0;
    int changed_lines = 0;

    if(diff == NULL)
        return 0;
    while(*p != '\0') {
        end = strchr(p, '\n');
        len = end != NULL ? (size_t)(end - p) : strlen(p);
        if(len > 2 && (p[0] == '-' || p[0] == '+') && p[1] == ' ') {
            line = p + 2;
            len -= 2;
            if(review_line_starts_with_date(line, len))
                date_lines++;
            else if(!review_line_equals_literal(line, len, "Sessions") &&
                    !review_line_equals_literal(line, len, "Habit days"))
                changed_lines++;
        }
        if(end == NULL)
            break;
        p = end + 1;
    }
    return date_lines > 0 ? date_lines : changed_lines;
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
        set_meta_int64(STORAGE_SYNC_APPLY_REVIEW_KEY, 0);
        set_meta_int64(STORAGE_SYNC_FULL_REPLACE_KEY, 0);
        exec_sql("DELETE FROM sync_outbox");
        g_storage.pending_sync_outbox_seq = 0;
        storage_schedule_persist();
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
