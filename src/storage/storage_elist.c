#include "storage.h"

#include "db.h"
#include "screens/elist_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
storage_elist_load(void *value)
{
    EListState *state = value;
    sqlite3_stmt *statement = NULL;

    if(g_storage.db == NULL || state == NULL)
        return 0;
    state->list_count = 0;
    state->item_count = 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id,title,sort_order,deleted_at,updated_at "
                          "FROM elist_lists WHERE user_id=?1 AND deleted_at=0 "
                          "ORDER BY sort_order,updated_at,id",
                          -1, &statement, NULL) != SQLITE_OK)
        return 0;
    bind_text(statement, 1, g_storage.user_id);
    while(state->list_count < ELIST_LIST_MAX &&
          sqlite3_step(statement) == SQLITE_ROW) {
        EListList *list = &state->lists[state->list_count++];

        snprintf(list->id, sizeof(list->id), "%s",
                 sqlite3_column_text(statement, 0));
        snprintf(list->title, sizeof(list->title), "%s",
                 sqlite3_column_text(statement, 1));
        list->sort_order = sqlite3_column_int(statement, 2);
        list->deleted_at = sqlite3_column_int64(statement, 3);
        list->updated_at = sqlite3_column_int64(statement, 4);
    }
    sqlite3_finalize(statement);

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT i.id,i.list_id,i.title,i.comment,i.done,"
                          "i.sort_order,i.deleted_at,i.updated_at "
                          "FROM elist_items i JOIN elist_lists l ON l.id=i.list_id "
                          "WHERE l.user_id=?1 AND l.deleted_at=0 AND i.deleted_at=0 "
                          "ORDER BY i.list_id,i.sort_order,i.updated_at,i.id",
                          -1, &statement, NULL) != SQLITE_OK)
        return 0;
    bind_text(statement, 1, g_storage.user_id);
    while(state->item_count < ELIST_ITEM_MAX &&
          sqlite3_step(statement) == SQLITE_ROW) {
        EListItem *item = &state->items[state->item_count++];

        snprintf(item->id, sizeof(item->id), "%s",
                 sqlite3_column_text(statement, 0));
        snprintf(item->list_id, sizeof(item->list_id), "%s",
                 sqlite3_column_text(statement, 1));
        snprintf(item->title, sizeof(item->title), "%s",
                 sqlite3_column_text(statement, 2));
        snprintf(item->comment, sizeof(item->comment), "%s",
                 sqlite3_column_text(statement, 3));
        item->done = sqlite3_column_int(statement, 4) != 0;
        item->sort_order = sqlite3_column_int(statement, 5);
        item->deleted_at = sqlite3_column_int64(statement, 6);
        item->updated_at = sqlite3_column_int64(statement, 7);
    }
    sqlite3_finalize(statement);
    if(state->selected_list >= state->list_count)
        state->selected_list = state->list_count - 1;
    if(state->selected_list < 0 && state->list_count > 0)
        state->selected_list = 0;
    return 1;
}

int
storage_elist_create_list(const char *title, char out_id[37])
{
    sqlite3_stmt *statement = NULL;
    char id[37];
    long long changed_at = storage_next_change_time();
    int result;

    if(g_storage.db == NULL || title == NULL || title[0] == '\0')
        return 0;
    storage_make_uuid(id);
    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT INTO elist_lists(id,user_id,title,sort_order,"
                          "deleted_at,updated_at) VALUES(?1,?2,?3,"
                          "(SELECT COALESCE(MAX(sort_order),-1)+1 FROM elist_lists "
                          "WHERE user_id=?2),0,?4)",
                          -1, &statement, NULL) != SQLITE_OK)
        return 0;
    bind_text(statement, 1, id);
    bind_text(statement, 2, g_storage.user_id);
    bind_text(statement, 3, title);
    sqlite3_bind_int64(statement, 4, changed_at);
    result = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if(result)
        result = storage_enqueue_sync_elist_list(id);
    if(result && out_id != NULL)
        snprintf(out_id, 37, "%s", id);
    return result;
}

int
storage_elist_update_list(const char *id, const char *title, int sort_order)
{
    sqlite3_stmt *statement = NULL;
    int result;

    if(g_storage.db == NULL || id == NULL || title == NULL || title[0] == '\0')
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE elist_lists SET title=?3,sort_order=?4,"
                          "updated_at=?5 WHERE id=?1 AND user_id=?2 AND deleted_at=0",
                          -1, &statement, NULL) != SQLITE_OK)
        return 0;
    bind_text(statement, 1, id);
    bind_text(statement, 2, g_storage.user_id);
    bind_text(statement, 3, title);
    sqlite3_bind_int(statement, 4, sort_order);
    sqlite3_bind_int64(statement, 5, storage_next_change_time());
    result = sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(g_storage.db) > 0;
    sqlite3_finalize(statement);
    return result && storage_enqueue_sync_elist_list(id);
}

int
storage_elist_delete_list(const char *id)
{
    sqlite3_stmt *statement = NULL;
    long long changed_at = storage_next_change_time();
    int result;

    if(g_storage.db == NULL || id == NULL)
        return 0;
    if(!exec_sql("BEGIN IMMEDIATE"))
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE elist_lists SET deleted_at=?3,updated_at=?3 "
                          "WHERE id=?1 AND user_id=?2 AND deleted_at=0",
                          -1, &statement, NULL) != SQLITE_OK)
        goto rollback;
    bind_text(statement, 1, id);
    bind_text(statement, 2, g_storage.user_id);
    sqlite3_bind_int64(statement, 3, changed_at);
    result = sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(g_storage.db) > 0;
    sqlite3_finalize(statement);
    statement = NULL;
    if(!result)
        goto rollback;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id FROM elist_items WHERE user_id=?1 AND list_id=?2 "
                          "AND deleted_at=0", -1, &statement, NULL) != SQLITE_OK)
        goto rollback;
    bind_text(statement, 1, g_storage.user_id);
    bind_text(statement, 2, id);
    while(sqlite3_step(statement) == SQLITE_ROW) {
        if(!storage_enqueue_sync_elist_item(
               (const char *)sqlite3_column_text(statement, 0)))
            goto rollback;
    }
    sqlite3_finalize(statement);
    statement = NULL;
    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE elist_items SET deleted_at=?3,updated_at=?3 "
                          "WHERE user_id=?1 AND list_id=?2 AND deleted_at=0",
                          -1, &statement, NULL) != SQLITE_OK)
        goto rollback;
    bind_text(statement, 1, g_storage.user_id);
    bind_text(statement, 2, id);
    sqlite3_bind_int64(statement, 3, changed_at);
    result = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    statement = NULL;
    if(!result || !storage_enqueue_sync_elist_list(id) || !exec_sql("COMMIT"))
        goto rollback;
    return 1;

rollback:
    if(statement != NULL)
        sqlite3_finalize(statement);
    exec_sql("ROLLBACK");
    return 0;
}

int
storage_elist_create_item(const char *list_id, const char *title,
                          const char *comment, char out_id[37])
{
    sqlite3_stmt *statement = NULL;
    char id[37];
    long long changed_at = storage_next_change_time();
    int result;

    if(g_storage.db == NULL || list_id == NULL || title == NULL || title[0] == '\0')
        return 0;
    storage_make_uuid(id);
    if(sqlite3_prepare_v2(g_storage.db,
                          "INSERT INTO elist_items(id,user_id,list_id,title,comment,done,"
                          "sort_order,deleted_at,updated_at) VALUES(?1,?2,?3,?4,?5,0,"
                          "(SELECT COALESCE(MAX(sort_order),-1)+1 FROM elist_items "
                          "WHERE list_id=?3),0,?6)",
                          -1, &statement, NULL) != SQLITE_OK)
        return 0;
    bind_text(statement, 1, id);
    bind_text(statement, 2, g_storage.user_id);
    bind_text(statement, 3, list_id);
    bind_text(statement, 4, title);
    bind_text(statement, 5, comment != NULL ? comment : "");
    sqlite3_bind_int64(statement, 6, changed_at);
    result = sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if(result)
        result = storage_enqueue_sync_elist_item(id);
    if(result && out_id != NULL)
        snprintf(out_id, 37, "%s", id);
    return result;
}

int
storage_elist_update_item(const char *id, const char *title,
                          const char *comment, int done, int sort_order)
{
    sqlite3_stmt *statement = NULL;
    int result;

    if(g_storage.db == NULL || id == NULL || title == NULL || title[0] == '\0')
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE elist_items SET title=?3,comment=?4,done=?5,"
                          "sort_order=?6,updated_at=?7 "
                          "WHERE id=?1 AND user_id=?2 AND deleted_at=0",
                          -1, &statement, NULL) != SQLITE_OK)
        return 0;
    bind_text(statement, 1, id);
    bind_text(statement, 2, g_storage.user_id);
    bind_text(statement, 3, title);
    bind_text(statement, 4, comment != NULL ? comment : "");
    sqlite3_bind_int(statement, 5, done != 0);
    sqlite3_bind_int(statement, 6, sort_order);
    sqlite3_bind_int64(statement, 7, storage_next_change_time());
    result = sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(g_storage.db) > 0;
    sqlite3_finalize(statement);
    return result && storage_enqueue_sync_elist_item(id);
}

int
storage_elist_delete_item(const char *id)
{
    sqlite3_stmt *statement = NULL;
    long long changed_at = storage_next_change_time();
    int result;

    if(g_storage.db == NULL || id == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "UPDATE elist_items SET deleted_at=?3,updated_at=?3 "
                          "WHERE id=?1 AND user_id=?2 AND deleted_at=0",
                          -1, &statement, NULL) != SQLITE_OK)
        return 0;
    bind_text(statement, 1, id);
    bind_text(statement, 2, g_storage.user_id);
    sqlite3_bind_int64(statement, 3, changed_at);
    result = sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(g_storage.db) > 0;
    sqlite3_finalize(statement);
    return result && storage_enqueue_sync_elist_item(id);
}

static int
storage_apply_elist_json(const char *response_json, const char *array_name,
                         const char *sql)
{
    char wrapped_sql[4096];

    if(response_json == NULL || array_name == NULL || sql == NULL)
        return 0;
    snprintf(wrapped_sql, sizeof(wrapped_sql), sql, array_name, array_name);
    return storage_exec_json_user_sql(wrapped_sql, response_json);
}

int
storage_apply_sync_elist_lists_json(const char *response_json)
{
    static const char *sql =
        "INSERT INTO elist_lists(id,user_id,title,sort_order,deleted_at,updated_at) "
        "SELECT json_extract(value,'$.id'),?2,COALESCE(json_extract(value,'$.title'),''),"
        "CAST(COALESCE(json_extract(value,'$.sort_order'),0) AS INTEGER),"
        "CAST(COALESCE(json_extract(value,'$.deleted_at'),0) AS INTEGER),"
        "CAST(COALESCE(json_extract(value,'$.updated_at'),0) AS INTEGER) "
        "FROM (SELECT value FROM json_each(?1,'$.changes.%s') UNION ALL "
        "SELECT value FROM json_each(?1,'$.data.%s')) WHERE json_extract(value,'$.id')<>'' "
        "ON CONFLICT(id) DO UPDATE SET title=excluded.title,sort_order=excluded.sort_order,"
        "deleted_at=excluded.deleted_at,updated_at=excluded.updated_at "
        "WHERE excluded.updated_at>=elist_lists.updated_at";

    return storage_apply_elist_json(response_json, "elist_lists", sql);
}

int
storage_apply_sync_elist_items_json(const char *response_json)
{
    static const char *sql =
        "INSERT INTO elist_items(id,user_id,list_id,title,comment,done,sort_order,"
        "deleted_at,updated_at) SELECT json_extract(value,'$.id'),?2,"
        "COALESCE(json_extract(value,'$.list_id'),''),"
        "COALESCE(json_extract(value,'$.title'),''),"
        "COALESCE(json_extract(value,'$.comment'),''),"
        "CAST(COALESCE(json_extract(value,'$.done'),0) AS INTEGER),"
        "CAST(COALESCE(json_extract(value,'$.sort_order'),0) AS INTEGER),"
        "CAST(COALESCE(json_extract(value,'$.deleted_at'),0) AS INTEGER),"
        "CAST(COALESCE(json_extract(value,'$.updated_at'),0) AS INTEGER) "
        "FROM (SELECT value FROM json_each(?1,'$.changes.%s') UNION ALL "
        "SELECT value FROM json_each(?1,'$.data.%s')) WHERE json_extract(value,'$.id')<>'' "
        "ON CONFLICT(id) DO UPDATE SET list_id=excluded.list_id,title=excluded.title,"
        "comment=excluded.comment,done=excluded.done,sort_order=excluded.sort_order,"
        "deleted_at=excluded.deleted_at,updated_at=excluded.updated_at "
        "WHERE excluded.updated_at>=elist_items.updated_at";

    return storage_apply_elist_json(response_json, "elist_items", sql);
}

int
storage_elist_import_onelist(const char *path)
{
    FILE *file;
    char *json;
    long size;
    sqlite3_stmt *statement = NULL;
    char list_id[37];
    int item_order = 0;
    int result = 0;

    if(g_storage.db == NULL || path == NULL)
        return 0;
    file = fopen(path, "rb");
    if(file == NULL || fseek(file, 0, SEEK_END) != 0) {
        if(file != NULL)
            fclose(file);
        return 0;
    }
    size = ftell(file);
    if(size <= 0 || size > 4 * 1024 * 1024 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    json = malloc((size_t)size + 1);
    if(json == NULL) {
        fclose(file);
        return 0;
    }
    if(fread(json, 1, (size_t)size, file) != (size_t)size) {
        free(json);
        fclose(file);
        return 0;
    }
    fclose(file);
    json[size] = '\0';

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT COALESCE(json_extract(?1,'$.title'),''),"
                          "json_valid(?1)", -1, &statement, NULL) != SQLITE_OK)
        goto done;
    bind_text(statement, 1, json);
    if(sqlite3_step(statement) != SQLITE_ROW || sqlite3_column_int(statement, 1) == 0 ||
       sqlite3_column_text(statement, 0)[0] == '\0')
        goto done;
    if(!exec_sql("BEGIN IMMEDIATE"))
        goto done;
    if(!storage_elist_create_list((const char *)sqlite3_column_text(statement, 0), list_id)) {
        exec_sql("ROLLBACK");
        goto done;
    }
    sqlite3_finalize(statement);
    statement = NULL;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT COALESCE(json_extract(value,'$.title'),''),"
                          "COALESCE(json_extract(value,'$.comment'),''),"
                          "CAST(COALESCE(json_extract(value,'$.done'),0) AS INTEGER) "
                          "FROM json_each(?1,'$.items') ORDER BY key",
                          -1, &statement, NULL) != SQLITE_OK) {
        exec_sql("ROLLBACK");
        goto done;
    }
    bind_text(statement, 1, json);
    while(sqlite3_step(statement) == SQLITE_ROW) {
        const char *title = (const char *)sqlite3_column_text(statement, 0);
        const char *comment = (const char *)sqlite3_column_text(statement, 1);
        char item_id[37];

        if(title[0] == '\0')
            continue;
        if(!storage_elist_create_item(list_id, title, comment, item_id) ||
           (sqlite3_column_int(statement, 2) != 0 &&
            !storage_elist_update_item(item_id, title, comment, 1, item_order))) {
            exec_sql("ROLLBACK");
            goto done;
        }
        item_order++;
    }
    result = exec_sql("COMMIT");

done:
    if(statement != NULL)
        sqlite3_finalize(statement);
    free(json);
    return result;
}
