#include "sqlite3.h"

#include <stdlib.h>
#include <string.h>

static const char plan9_sqlite_stub_error[] = "sqlite unavailable on Plan 9 test build";
static const unsigned char plan9_sqlite_empty_text[] = "";

int
sqlite3_open(const char *filename, sqlite3 **ppDb)
{
    sqlite3 *db;

    (void)filename;
    if(ppDb == NULL)
        return SQLITE_MISUSE;
    db = (sqlite3 *)calloc(1, 1);
    if(db == NULL) {
        *ppDb = NULL;
        return SQLITE_NOMEM;
    }
    *ppDb = db;
    return SQLITE_OK;
}

int
sqlite3_close(sqlite3 *db)
{
    free(db);
    return SQLITE_OK;
}

int
sqlite3_busy_timeout(sqlite3 *db, int ms)
{
    (void)db;
    (void)ms;
    return SQLITE_OK;
}

int
sqlite3_exec(sqlite3 *db, const char *sql,
             int (*callback)(void *, int, char **, char **), void *arg,
             char **errmsg)
{
    (void)db;
    (void)sql;
    (void)callback;
    (void)arg;
    if(errmsg != NULL)
        *errmsg = NULL;
    return SQLITE_OK;
}

int
sqlite3_prepare_v2(sqlite3 *db, const char *sql, int nByte,
                   sqlite3_stmt **ppStmt, const char **pzTail)
{
    sqlite3_stmt *stmt;

    (void)db;
    if(ppStmt == NULL)
        return SQLITE_MISUSE;
    stmt = (sqlite3_stmt *)calloc(1, 1);
    if(stmt == NULL) {
        *ppStmt = NULL;
        return SQLITE_NOMEM;
    }
    *ppStmt = stmt;
    if(pzTail != NULL)
        *pzTail = sql != NULL ? sql + (nByte >= 0 ? nByte : strlen(sql)) : NULL;
    return SQLITE_OK;
}

int
sqlite3_finalize(sqlite3_stmt *stmt)
{
    free(stmt);
    return SQLITE_OK;
}

int
sqlite3_step(sqlite3_stmt *stmt)
{
    (void)stmt;
    return SQLITE_DONE;
}

int
sqlite3_reset(sqlite3_stmt *stmt)
{
    (void)stmt;
    return SQLITE_OK;
}

int
sqlite3_clear_bindings(sqlite3_stmt *stmt)
{
    (void)stmt;
    return SQLITE_OK;
}

int
sqlite3_bind_parameter_count(sqlite3_stmt *stmt)
{
    (void)stmt;
    return 0;
}

int
sqlite3_bind_int(sqlite3_stmt *stmt, int index, int value)
{
    (void)stmt;
    (void)index;
    (void)value;
    return SQLITE_OK;
}

int
sqlite3_bind_int64(sqlite3_stmt *stmt, int index, sqlite3_int64 value)
{
    (void)stmt;
    (void)index;
    (void)value;
    return SQLITE_OK;
}

int
sqlite3_bind_text(sqlite3_stmt *stmt, int index, const char *value, int len,
                  void (*destructor)(void *))
{
    (void)stmt;
    (void)index;
    (void)value;
    (void)len;
    (void)destructor;
    return SQLITE_OK;
}

int
sqlite3_column_int(sqlite3_stmt *stmt, int column)
{
    (void)stmt;
    (void)column;
    return 0;
}

sqlite3_int64
sqlite3_column_int64(sqlite3_stmt *stmt, int column)
{
    (void)stmt;
    (void)column;
    return 0;
}

double
sqlite3_column_double(sqlite3_stmt *stmt, int column)
{
    (void)stmt;
    (void)column;
    return 0.0;
}

const unsigned char *
sqlite3_column_text(sqlite3_stmt *stmt, int column)
{
    (void)stmt;
    (void)column;
    return plan9_sqlite_empty_text;
}

int
sqlite3_changes(sqlite3 *db)
{
    (void)db;
    return 0;
}

int
sqlite3_db_cacheflush(sqlite3 *db)
{
    (void)db;
    return SQLITE_OK;
}

int
sqlite3_get_autocommit(sqlite3 *db)
{
    (void)db;
    return 1;
}

const char *
sqlite3_errmsg(sqlite3 *db)
{
    (void)db;
    return plan9_sqlite_stub_error;
}

void
sqlite3_free(void *p)
{
    free(p);
}
