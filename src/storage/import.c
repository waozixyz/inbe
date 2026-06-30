#include "import.h"

#include "db.h"
#include "screens/habits_screen.h"
#include "breath_engine.h"
#include "miniz.h"
#include "version.h"

#include "raylib.h"
#include <sqlite3.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static int
source_table_has_column(sqlite3 *db, const char *table, const char *column)
{
    sqlite3_stmt *stmt = NULL;
    char sql[128];
    int found = 0;

    if(db == NULL || table == NULL || column == NULL)
        return 0;
    snprintf(sql, sizeof(sql), "PRAGMA table_info(%s)", table);
    if(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        if(name != NULL && strcmp(name, column) == 0) {
            found = 1;
            break;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

static int
source_table_exists(sqlite3 *db, const char *table)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(db == NULL || table == NULL)
        return 0;
    if(sqlite3_prepare_v2(db,
                          "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1 LIMIT 1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(stmt, 1, table, -1, SQLITE_TRANSIENT);
    found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

static int
source_count_rows(sqlite3 *db, const char *sql)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;

    if(db == NULL || sql == NULL)
        return 0;
    if(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    if(sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

static const char *const importable_setting_keys[] = {
    "speed",
    "max_rounds",
    "max_breaths",
    "pause_seconds",
    "sound_volume",
    "tutorial_seen",
    "exercise_manual_seen_mask",
    "theme",
    "dark_mode",
    "theme_mode",
    "orientation_mode",
    "main_tab",
    "fullscreen",
    "on_screen_keyboard",
    "progressive_speed",
    "progressive_start_speed",
    "breath_animation",
    "double_tap_to_breathe",
    "advanced_session_controls",
    "hold_display_mode",
    "exercise_type",
    "meditation_duration_mode",
    "meditation_custom_minutes",
    "meditation_show_extend_controls",
    "meditation_music_enabled",
    "meditation_music_shuffle",
    "meditation_music_track",
    "play_in_background",
    "language",
    "practice_category_tab",
    "practice_visible_mask"
};

static int
setting_key_importable(const char *key)
{
    if(key == NULL || key[0] == '\0')
        return 0;
    for(size_t i = 0; i < sizeof(importable_setting_keys) / sizeof(importable_setting_keys[0]); i++) {
        if(strcmp(key, importable_setting_keys[i]) == 0)
            return 1;
    }
    return 0;
}

static int
source_count_importable_settings(sqlite3 *db)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;

    if(db == NULL || !source_table_exists(db, "settings"))
        return 0;
    if(sqlite3_prepare_v2(db, "SELECT DISTINCT key FROM settings", -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *key = (const char *)sqlite3_column_text(stmt, 0);
        if(setting_key_importable(key))
            count++;
    }
    sqlite3_finalize(stmt);
    return count;
}

static int
local_habit_id_by_name(const char *name, char *out, size_t out_size)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(name == NULL || name[0] == '\0' || out == NULL || out_size == 0 ||
       g_storage.db == NULL)
        return 0;

    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT id FROM habits WHERE name=?1 COLLATE NOCASE AND deleted_at=0 ORDER BY sort_order,id LIMIT 1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, name);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *id = (const char *)sqlite3_column_text(stmt, 0);
        if(id != NULL && id[0] != '\0') {
            snprintf(out, out_size, "%s", id);
            found = out[0] != '\0';
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

static int
local_habit_id_exists(const char *id)
{
    sqlite3_stmt *stmt = NULL;
    int found = 0;

    if(id == NULL || id[0] == '\0' || g_storage.db == NULL)
        return 0;
    if(sqlite3_prepare_v2(g_storage.db,
                          "SELECT 1 FROM habits WHERE id=?1 LIMIT 1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    bind_text(stmt, 1, id);
    found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

static void
make_import_habit_id(char *out, size_t out_size)
{
    static unsigned int counter = 0;

    if(out == NULL || out_size == 0)
        return;
    do {
        snprintf(out, out_size, "import-%lld-%u", now_seconds(), counter++);
    } while(local_habit_id_exists(out));
}

static int
resolve_import_habit_id(const char *import_id, const char *name,
                        char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';

    if(local_habit_id_by_name(name, out, out_size))
        return 1;

    if(import_id != NULL && import_id[0] != '\0' && !local_habit_id_exists(import_id)) {
        snprintf(out, out_size, "%s", import_id);
        return out[0] != '\0';
    }

    make_import_habit_id(out, out_size);
    return out[0] != '\0';
}

int
storage_export_zip(const char *path)
{
    mz_zip_archive archive;
    FILE *fp;
    char *buf;
    long size;
    char metadata[512];

    if(path == NULL || path[0] == '\0' || g_storage.db == NULL)
        return 0;
    sqlite3_exec(g_storage.db, "PRAGMA wal_checkpoint(FULL)", NULL, NULL, NULL);
    fp = fopen(g_storage.db_path, "rb");
    if(fp == NULL)
        return 0;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if(size <= 0) {
        fclose(fp);
        return 0;
    }
    buf = malloc((size_t)size);
    if(buf == NULL) {
        fclose(fp);
        return 0;
    }
    if(fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_writer_init_file(&archive, path, 0)) {
        free(buf);
        return 0;
    }
    snprintf(metadata, sizeof(metadata),
             "{\n\"format\":\"inbe-data-sqlite\",\n\"format_version\":1,\n\"app_version\":\"%s\",\n\"user_id\":\"%s\",\n\"session_count\":%d,\n\"habit_count\":%d\n}\n",
             INBE_VERSION_STRING, g_storage.user_id, storage_session_count(), storage_habit_count());
    mz_zip_writer_add_mem(&archive, "inbe-data/metadata.json", metadata, strlen(metadata), MZ_NO_COMPRESSION);
    mz_zip_writer_add_mem(&archive, "inbe-data/inbe.db", buf, (size_t)size, MZ_BEST_COMPRESSION);
    free(buf);
    mz_zip_writer_finalize_archive(&archive);
    mz_zip_writer_end(&archive);
    return 1;
}

static Color
tickmate_color_from_int(int value, int index)
{
    static const Color fallback[] = {
        {99, 196, 165, 255},
        {94, 166, 232, 255},
        {210, 180, 72, 255},
        {224, 124, 104, 255},
        {180, 132, 220, 255},
        {216, 116, 164, 255}
    };
    Color color = fallback[index % (int)(sizeof(fallback) / sizeof(fallback[0]))];

    if(value != 0) {
        color.r = (unsigned char)((value >> 16) & 0xff);
        color.g = (unsigned char)((value >> 8) & 0xff);
        color.b = (unsigned char)(value & 0xff);
        color.a = 255;
        if(color.r == 0 && color.g == 0 && color.b == 0)
            color = fallback[index % (int)(sizeof(fallback) / sizeof(fallback[0]))];
    }
    return color;
}

static int
parse_legacy_rounds(const char *text, size_t size, int *rounds, int max_rounds)
{
    int count = 0;
    size_t pos = 0;

    if(text == NULL || rounds == NULL || max_rounds <= 0)
        return 0;
    while(pos < size && count < max_rounds) {
        int value = 0;
        int seen = 0;
        while(pos < size && (text[pos] == ' ' || text[pos] == '\t' ||
                             text[pos] == '\r' || text[pos] == '\n'))
            pos++;
        while(pos < size && text[pos] >= '0' && text[pos] <= '9') {
            seen = 1;
            value = value * 10 + (text[pos] - '0');
            pos++;
        }
        if(seen && value > 0)
            rounds[count++] = value;
        while(pos < size && text[pos] != '\n')
            pos++;
    }
    return count;
}

static long long
legacy_session_started_at(int year, int month, int day, int hour, int minute, int second)
{
    struct tm tm_value;

    memset(&tm_value, 0, sizeof(tm_value));
    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = month - 1;
    tm_value.tm_mday = day;
    tm_value.tm_hour = hour;
    tm_value.tm_min = minute;
    tm_value.tm_sec = second;
    tm_value.tm_isdst = -1;
    return (long long)mktime(&tm_value);
}

static int
parse_legacy_session_filename(const char *filename, int *year, int *month, int *day,
                              int *hour, int *minute, int *second)
{
    const char *p;

    if(filename == NULL)
        return 0;
    for(p = filename; *p != '\0'; p++) {
        if(sscanf(p, "sessions/%4d/%2d/%2d/inbe-%2d%2d%2d",
                  year, month, day, hour, minute, second) == 6)
            return 1;
        if(sscanf(p, "%4d/%2d/%2d/inbe-%2d%2d%2d",
                  year, month, day, hour, minute, second) == 6)
            return 1;
    }
    return 0;
}

static int
import_legacy_session_bytes(const char *name, const char *bytes, size_t size)
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int rounds[MaxRounds];
    int round_count;
    int local_date;
    long long started_at;

    if(!parse_legacy_session_filename(name, &year, &month, &day, &hour, &minute, &second))
        return 0;
    round_count = parse_legacy_rounds(bytes, size, rounds, MaxRounds);
    if(round_count <= 0) {
        TraceLog(LOG_WARNING, "DATA: legacy import ignored empty session %s", name);
        return 0;
    }
    local_date = year * 10000 + month * 100 + day;
    started_at = legacy_session_started_at(year, month, day, hour, minute, second);
    if(started_at <= 0) {
        TraceLog(LOG_WARNING, "DATA: legacy import invalid date in %s", name);
        return 0;
    }
    return insert_session_at_ex(started_at, local_date, rounds, round_count,
                                0, 0, "legacy-file-import", NULL, 0);
}

static int
import_legacy_session_zip(mz_zip_archive *archive)
{
    mz_uint file_count;
    int imported = 0;

    if(archive == NULL)
        return 0;
    g_storage.materialize_defer++;
    file_count = mz_zip_reader_get_num_files(archive);
    for(mz_uint i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat stat;
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        size_t text_size = 0;
        char *text;

        if(!mz_zip_reader_file_stat(archive, i, &stat))
            continue;
        if(stat.m_is_directory)
            continue;
        if(!parse_legacy_session_filename(stat.m_filename,
                                          &year, &month, &day,
                                          &hour, &minute, &second))
            continue;

        text = mz_zip_reader_extract_to_heap(archive, i, &text_size, 0);
        if(text == NULL) {
            TraceLog(LOG_WARNING, "DATA: legacy import failed to extract %s", stat.m_filename);
            continue;
        }
        if(import_legacy_session_bytes(stat.m_filename, text, text_size))
            imported++;
        free(text);
    }
    g_storage.materialize_defer--;
    if(g_storage.materialize_defer == 0 && g_storage.materialize_needed) {
        g_storage.materialize_needed = 0;
        storage_materialize_session_habit_days();
    }
    if(imported <= 0)
        TraceLog(LOG_WARNING, "DATA: no legacy sessions found in archive");
    return imported > 0;
}

static char *
read_file_heap(const char *path, size_t *out_size)
{
    FILE *fp;
    long size;
    char *buf;

    if(out_size != NULL)
        *out_size = 0;
    if(path == NULL || path[0] == '\0')
        return NULL;
    fp = fopen(path, "rb");
    if(fp == NULL)
        return NULL;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if(size <= 0) {
        fclose(fp);
        return NULL;
    }
    buf = malloc((size_t)size);
    if(buf == NULL) {
        fclose(fp);
        return NULL;
    }
    if(fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    if(out_size != NULL)
        *out_size = (size_t)size;
    return buf;
}

typedef struct {
    char **paths;
    int count;
    int capacity;
} ImportedFiles;

static void
imported_files_init(ImportedFiles *files)
{
    if(files == NULL)
        return;
    files->paths = NULL;
    files->count = 0;
    files->capacity = 0;
}

static void
imported_files_add(ImportedFiles *files, const char *path)
{
    char *copy;
    char **new_paths;
    int new_capacity;

    if(files == NULL || path == NULL)
        return;
    if(files->count >= files->capacity) {
        new_capacity = files->capacity == 0 ? 64 : files->capacity * 2;
        new_paths = realloc(files->paths, (size_t)new_capacity * sizeof(char *));
        if(new_paths == NULL)
            return;
        files->paths = new_paths;
        files->capacity = new_capacity;
    }
    copy = strdup(path);
    if(copy == NULL)
        return;
    files->paths[files->count++] = copy;
}

static void
imported_files_cleanup(ImportedFiles *files)
{
    int i;

    if(files == NULL)
        return;
    if(files->paths != NULL) {
        for(i = 0; i < files->count; i++) {
            if(files->paths[i] != NULL)
                free(files->paths[i]);
        }
        free(files->paths);
    }
    files->paths = NULL;
    files->count = 0;
    files->capacity = 0;
}

static void
delete_imported_legacy_files(ImportedFiles *files)
{
    int i;

    if(files == NULL)
        return;
    for(i = 0; i < files->count; i++) {
        if(files->paths[i] != NULL) {
            if(remove(files->paths[i]) == 0)
                TraceLog(LOG_INFO, "DATA: deleted migrated legacy file %s", files->paths[i]);
            else
                TraceLog(LOG_WARNING, "DATA: failed to delete %s", files->paths[i]);
        }
    }
}

static int
migrate_legacy_file_sessions_in_dir(const char *dir_path, ImportedFiles *imported)
{
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    if(dir_path == NULL || dir_path[0] == '\0')
        return 0;
    dir = opendir(dir_path);
    if(dir == NULL)
        return 0;
    while((entry = readdir(dir)) != NULL) {
        char child[INBE_STORAGE_PATH_SIZE];
        struct stat st;

        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(child, sizeof(child), "%s/%s", dir_path, entry->d_name);
        if(stat(child, &st) != 0)
            continue;
        if(S_ISDIR(st.st_mode)) {
            count += migrate_legacy_file_sessions_in_dir(child, imported);
        } else if(S_ISREG(st.st_mode)) {
            int year;
            int month;
            int day;
            int hour;
            int minute;
            int second;
            if(parse_legacy_session_filename(child, &year, &month, &day,
                                             &hour, &minute, &second)) {
                size_t size = 0;
                char *bytes = read_file_heap(child, &size);
                if(bytes != NULL) {
                    if(import_legacy_session_bytes(child, bytes, size)) {
                        count++;
                        imported_files_add(imported, child);
                    }
                    free(bytes);
                } else {
                    TraceLog(LOG_WARNING, "DATA: legacy file migration could not read %s", child);
                }
            }
        }
    }
    closedir(dir);
    return count;
}

void
migrate_legacy_file_sessions_once(void)
{
    ImportedFiles imported;

    if(g_storage.db == NULL || g_storage.root[0] == '\0')
        return;
    if(meta_equals("legacy_file_sessions_migrated", "1"))
        return;

    imported_files_init(&imported);
    migrate_legacy_file_sessions_in_dir(g_storage.root, &imported);
    set_meta("legacy_file_sessions_migrated", "1");
    storage_schedule_persist();

    if(imported.count > 0) {
        TraceLog(LOG_INFO, "DATA: deleting %d migrated legacy file(s)", imported.count);
        delete_imported_legacy_files(&imported);
    }
    imported_files_cleanup(&imported);
}

static int
import_tickmate_db(sqlite3 *src)
{
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt *write_stmt = NULL;
    int ok = 0;
    int has_multiple_entries;
    const char *track_sql;
    long long imported_at = now_seconds();

    if(src == NULL || g_storage.db == NULL)
        return 0;
    if(!source_table_has_column(src, "tracks", "name") ||
       !source_table_has_column(src, "ticks", "_track_id"))
        return 0;

    has_multiple_entries = source_table_has_column(src, "tracks", "multiple_entries_per_day");
    track_sql = has_multiple_entries
                    ? "SELECT _id,name,color,\"order\",multiple_entries_per_day,"
                      "EXISTS(SELECT 1 FROM ticks WHERE ticks._track_id=tracks._id "
                      "GROUP BY year,month,day HAVING COUNT(*)>1 LIMIT 1) "
                      "FROM tracks WHERE enabled!=0 ORDER BY \"order\",_id"
                    : "SELECT _id,name,color,\"order\",0,"
                      "EXISTS(SELECT 1 FROM ticks WHERE ticks._track_id=tracks._id "
                      "GROUP BY year,month,day HAVING COUNT(*)>1 LIMIT 1) "
                      "FROM tracks WHERE enabled!=0 ORDER BY \"order\",_id";

    if(sqlite3_prepare_v2(src, track_sql, -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    exec_sql("BEGIN IMMEDIATE");
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        char import_habit_id[64];
        char local_habit_id[INBE_STORAGE_ID_SIZE];
        const char *name = (const char *)sqlite3_column_text(stmt, 1);
        int track_id = sqlite3_column_int(stmt, 0);
        int sort_order = sqlite3_column_int(stmt, 3);
        int counter_enabled = sqlite3_column_int(stmt, 4) != 0 ||
                              sqlite3_column_int(stmt, 5) != 0;
        Color color = tickmate_color_from_int(sqlite3_column_int(stmt, 2), track_id);

        if(name == NULL || name[0] == '\0')
            continue;
        snprintf(import_habit_id, sizeof(import_habit_id), "tickmate-%d", track_id);
        if(!resolve_import_habit_id(import_habit_id, name, local_habit_id,
                                    sizeof(local_habit_id)))
            continue;
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT OR REPLACE INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_enabled,sort_order,deleted_at,updated_at) "
                              "VALUES(?1,?2,COALESCE((SELECT name FROM habits WHERE id=?1),?3),?4,?5,?6,?7,?8,"
                              "CASE WHEN ?9!=0 THEN 1 ELSE COALESCE((SELECT counter_enabled FROM habits WHERE id=?1),0) END,?10,0,?11)",
                              -1, &write_stmt, NULL) != SQLITE_OK)
            continue;
        bind_text(write_stmt, 1, local_habit_id);
        bind_text(write_stmt, 2, g_storage.user_id);
        bind_text(write_stmt, 3, name);
        sqlite3_bind_int(write_stmt, 4, color.r);
        sqlite3_bind_int(write_stmt, 5, color.g);
        sqlite3_bind_int(write_stmt, 6, color.b);
        sqlite3_bind_int(write_stmt, 7, INBE_HABIT_SYNC_NONE);
        sqlite3_bind_int(write_stmt, 8, 0);
        sqlite3_bind_int(write_stmt, 9, counter_enabled ? 1 : 0);
        sqlite3_bind_int(write_stmt, 10, sort_order);
        sqlite3_bind_int64(write_stmt, 11, imported_at);
        if(sqlite3_step(write_stmt) == SQLITE_DONE)
            ok = 1;
        sqlite3_finalize(write_stmt);
        write_stmt = NULL;
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if(sqlite3_prepare_v2(src,
                          "SELECT ticks._track_id,tracks.name,ticks.year,ticks.month,ticks.day,COUNT(*) "
                          "FROM ticks JOIN tracks ON tracks._id=ticks._track_id "
                          "WHERE tracks.enabled!=0 "
                          "GROUP BY ticks._track_id,ticks.year,ticks.month,ticks.day",
                          -1, &stmt, NULL) == SQLITE_OK) {
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT INTO habit_days(habit_id,local_date,completed,count,updated_at) "
                              "VALUES(?1,?2,1,?3,?4) "
                              "ON CONFLICT(habit_id,local_date) DO UPDATE SET "
                              "count=CASE WHEN habit_days.count>excluded.count THEN habit_days.count ELSE excluded.count END,"
                              "completed=CASE WHEN habit_days.count>0 OR excluded.count>0 OR habit_days.completed!=0 THEN 1 ELSE 0 END,"
                              "updated_at=CASE WHEN excluded.count>habit_days.count THEN excluded.updated_at ELSE habit_days.updated_at END",
                              -1, &write_stmt, NULL) != SQLITE_OK) {
            sqlite3_finalize(stmt);
            stmt = NULL;
            goto finish;
        }
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            char import_habit_id[64];
            char local_habit_id[INBE_STORAGE_ID_SIZE];
            int track_id = sqlite3_column_int(stmt, 0);
            const char *name = (const char *)sqlite3_column_text(stmt, 1);
            int year = sqlite3_column_int(stmt, 2);
            int month = sqlite3_column_int(stmt, 3);
            int day = sqlite3_column_int(stmt, 4);
            int count = sqlite3_column_int(stmt, 5);
            int local_date;

            if(name == NULL || name[0] == '\0')
                continue;
            /* Tickmate stores Android Calendar.MONTH values: January is 0. */
            if(year <= 0 || month < 0 || month > 11 || day <= 0 || day > 31)
                continue;
            local_date = year * 10000 + (month + 1) * 100 + day;
            snprintf(import_habit_id, sizeof(import_habit_id), "tickmate-%d", track_id);
            if(!resolve_import_habit_id(import_habit_id, name, local_habit_id,
                                        sizeof(local_habit_id)))
                continue;
            sqlite3_reset(write_stmt);
            sqlite3_clear_bindings(write_stmt);
            bind_text(write_stmt, 1, local_habit_id);
            sqlite3_bind_int(write_stmt, 2, local_date);
            sqlite3_bind_int(write_stmt, 3, count > 0 ? count : 1);
            sqlite3_bind_int64(write_stmt, 4, imported_at);
            if(sqlite3_step(write_stmt) == SQLITE_DONE)
                ok = 1;
        }
    }
finish:
    sqlite3_finalize(stmt);
    sqlite3_finalize(write_stmt);
    exec_sql("COMMIT");
    return ok;
}

static int
import_settings_from_source(sqlite3 *src)
{
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt *write_stmt = NULL;
    int imported = 0;
    long long imported_at = now_seconds();

    if(src == NULL || g_storage.db == NULL || !source_table_exists(src, "settings"))
        return 0;
    if(sqlite3_prepare_v2(src, "SELECT key,value FROM settings", -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    exec_sql("BEGIN IMMEDIATE");
    while(sqlite3_step(stmt) == SQLITE_ROW) {
        const char *key = (const char *)sqlite3_column_text(stmt, 0);
        const char *value = (const char *)sqlite3_column_text(stmt, 1);

        if(!setting_key_importable(key))
            continue;
        if(sqlite3_prepare_v2(g_storage.db,
                              "INSERT INTO settings(user_id,key,value,updated_at) VALUES(?1,?2,?3,?4) "
                              "ON CONFLICT(user_id,key) DO UPDATE SET "
                              "value=CASE WHEN excluded.value!='' OR settings.value='' THEN excluded.value ELSE settings.value END,"
                              "updated_at=CASE WHEN excluded.value!='' OR settings.value='' THEN excluded.updated_at ELSE settings.updated_at END",
                              -1, &write_stmt, NULL) != SQLITE_OK)
            continue;
        bind_text(write_stmt, 1, g_storage.user_id);
        bind_text(write_stmt, 2, key);
        bind_text(write_stmt, 3, value != NULL ? value : "");
        sqlite3_bind_int64(write_stmt, 4, imported_at);
        if(sqlite3_step(write_stmt) == SQLITE_DONE)
            imported++;
        sqlite3_finalize(write_stmt);
        write_stmt = NULL;
    }
    sqlite3_finalize(stmt);
    sqlite3_finalize(write_stmt);
    exec_sql("COMMIT");
    return imported;
}

static int
inspect_sqlite_db_file(const char *db_path, InbeStorageImportInfo *info)
{
    sqlite3 *src = NULL;
    int ok = 0;

    if(info != NULL)
        memset(info, 0, sizeof(*info));
    if(db_path == NULL || db_path[0] == '\0' || info == NULL)
        return 0;
    if(sqlite3_open(db_path, &src) != SQLITE_OK)
        goto done;

    if(source_table_exists(src, "sessions") && source_table_exists(src, "session_rounds")) {
        info->session_count = source_count_rows(src, "SELECT COUNT(*) FROM sessions WHERE deleted_at=0");
        info->habit_count = source_table_exists(src, "habits")
                                ? source_count_rows(src, "SELECT COUNT(*) FROM habits WHERE deleted_at=0")
                                : 0;
        info->setting_count = source_count_importable_settings(src);
        info->has_sessions = info->session_count > 0;
        info->has_habits = info->habit_count > 0;
        info->has_settings = info->setting_count > 0;
        info->valid = info->has_sessions || info->has_habits || info->has_settings;
        ok = info->valid;
        goto done;
    }

    if(source_table_has_column(src, "tracks", "name") &&
       source_table_has_column(src, "ticks", "_track_id")) {
        info->habit_count = source_count_rows(src, "SELECT COUNT(*) FROM tracks WHERE enabled!=0");
        info->has_habits = info->habit_count > 0;
        info->valid = info->has_habits;
        ok = info->valid;
    }

done:
    if(src != NULL)
        sqlite3_close(src);
    return ok;
}

static int
import_sqlite_db_file(const char *db_path, InbeStorageImportMode mode)
{
    sqlite3 *src = NULL;
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt *hstmt = NULL;
    int ok = 0;
    int imported_settings = 0;
    int deferred_materialize = 0;

    if(db_path == NULL || db_path[0] == '\0')
        return 0;
    if(sqlite3_open(db_path, &src) != SQLITE_OK) {
        TraceLog(LOG_WARNING, "DATA: sqlite import could not open %s", db_path);
        goto done;
    }
    g_storage.materialize_defer++;
    deferred_materialize = 1;
    if(sqlite3_prepare_v2(src,
                          "SELECT id,started_at,local_date,topic,activity,source FROM sessions WHERE deleted_at=0",
                          -1, &stmt, NULL) != SQLITE_OK) {
        goto try_tickmate;
    }

    while(sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_stmt *rstmt = NULL;
        int rounds[MaxRounds];
        int count = 0;
        const char *sid = (const char *)sqlite3_column_text(stmt, 0);
        long long started_at = sqlite3_column_int64(stmt, 1);
        int local_date = sqlite3_column_int(stmt, 2);
        int topic = sqlite3_column_int(stmt, 3);
        int activity = sqlite3_column_int(stmt, 4);

        if(sqlite3_prepare_v2(src,
                              "SELECT seconds FROM session_rounds WHERE session_id=?1 ORDER BY round_index",
                              -1, &rstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(rstmt, 1, sid != NULL ? sid : "", -1, SQLITE_TRANSIENT);
            while(count < MaxRounds && sqlite3_step(rstmt) == SQLITE_ROW)
                rounds[count++] = sqlite3_column_int(rstmt, 0);
        }
        sqlite3_finalize(rstmt);
        if(count > 0) {
            insert_session_at_ex(started_at, local_date, rounds, count,
                                 topic, activity, "sqlite-import", NULL, 0);
            ok = 1;
        }
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    if(source_table_exists(src, "habits")) {
        int has_sync_activity = source_table_has_column(src, "habits", "sync_activity");
        int has_counter_enabled = source_table_has_column(src, "habits", "counter_enabled");
        int has_day_count = source_table_has_column(src, "habit_days", "count");
        const char *habit_sql =
            has_sync_activity && has_counter_enabled
                ? "SELECT id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_enabled,sort_order FROM habits WHERE deleted_at=0 ORDER BY sort_order,id"
                : has_sync_activity
                    ? "SELECT id,name,color_r,color_g,color_b,sync_mode,sync_activity,0,sort_order FROM habits WHERE deleted_at=0 ORDER BY sort_order,id"
                    : has_counter_enabled
                        ? "SELECT id,name,color_r,color_g,color_b,sync_mode,0,counter_enabled,sort_order FROM habits WHERE deleted_at=0 ORDER BY sort_order,id"
                        : "SELECT id,name,color_r,color_g,color_b,sync_mode,0,0,sort_order FROM habits WHERE deleted_at=0 ORDER BY sort_order,id";
        if(sqlite3_prepare_v2(src, habit_sql, -1, &stmt, NULL) != SQLITE_OK)
            goto after_habits;
        exec_sql("BEGIN IMMEDIATE");
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            const char *import_habit_id = (const char *)sqlite3_column_text(stmt, 0);
            const char *name = (const char *)sqlite3_column_text(stmt, 1);
            char local_habit_id[INBE_STORAGE_ID_SIZE];
            int sync_activity = sqlite3_column_int(stmt, 6);
            int counter_enabled = sqlite3_column_int(stmt, 7) != 0;
            int sort_order = sqlite3_column_int(stmt, 8);

            if(import_habit_id == NULL || import_habit_id[0] == '\0' ||
               name == NULL || name[0] == '\0')
                continue;
            if(!resolve_import_habit_id(import_habit_id, name, local_habit_id,
                                        sizeof(local_habit_id)))
                continue;
            if(sqlite3_prepare_v2(g_storage.db,
                                  "INSERT OR REPLACE INTO habits(id,user_id,name,color_r,color_g,color_b,sync_mode,sync_activity,counter_enabled,sort_order,deleted_at,updated_at) "
                                  "VALUES(?1,?2,COALESCE((SELECT name FROM habits WHERE id=?1),?3),?4,?5,?6,?7,?8,"
                                  "CASE WHEN ?9!=0 THEN 1 ELSE COALESCE((SELECT counter_enabled FROM habits WHERE id=?1),0) END,?10,0,?11)",
                                  -1, &hstmt, NULL) != SQLITE_OK)
                continue;
            bind_text(hstmt, 1, local_habit_id);
            bind_text(hstmt, 2, g_storage.user_id);
            bind_text(hstmt, 3, name);
            sqlite3_bind_int(hstmt, 4, sqlite3_column_int(stmt, 2));
            sqlite3_bind_int(hstmt, 5, sqlite3_column_int(stmt, 3));
            sqlite3_bind_int(hstmt, 6, sqlite3_column_int(stmt, 4));
            sqlite3_bind_int(hstmt, 7, sqlite3_column_int(stmt, 5));
            sqlite3_bind_int(hstmt, 8, sync_activity);
            sqlite3_bind_int(hstmt, 9, counter_enabled ? 1 : 0);
            sqlite3_bind_int(hstmt, 10, sort_order);
            sqlite3_bind_int64(hstmt, 11, now_seconds());
            if(sqlite3_step(hstmt) == SQLITE_DONE)
                ok = 1;
            sqlite3_finalize(hstmt);
            hstmt = NULL;

            if(sqlite3_prepare_v2(src,
                                  has_day_count
                                      ? "SELECT local_date,completed,count FROM habit_days WHERE habit_id=?1"
                                      : "SELECT local_date,completed,CASE WHEN completed!=0 THEN 1 ELSE 0 END FROM habit_days WHERE habit_id=?1",
                                  -1, &hstmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(hstmt, 1, import_habit_id, -1, SQLITE_TRANSIENT);
                while(sqlite3_step(hstmt) == SQLITE_ROW) {
                    sqlite3_stmt *day_stmt = NULL;
                    if(sqlite3_prepare_v2(g_storage.db,
                                          "INSERT INTO habit_days(habit_id,local_date,completed,count,updated_at) "
                                          "VALUES(?1,?2,?3,?4,?5) "
                                          "ON CONFLICT(habit_id,local_date) DO UPDATE SET "
                                          "count=CASE WHEN habit_days.count>excluded.count THEN habit_days.count ELSE excluded.count END,"
                                          "completed=CASE WHEN habit_days.count>0 OR excluded.count>0 OR habit_days.completed!=0 OR excluded.completed!=0 THEN 1 ELSE 0 END,"
                                          "updated_at=CASE WHEN excluded.count>habit_days.count THEN excluded.updated_at ELSE habit_days.updated_at END",
                                          -1, &day_stmt, NULL) != SQLITE_OK)
                        continue;
                    bind_text(day_stmt, 1, local_habit_id);
                    sqlite3_bind_int(day_stmt, 2, sqlite3_column_int(hstmt, 0));
                    sqlite3_bind_int(day_stmt, 3, sqlite3_column_int(hstmt, 1) != 0);
                    sqlite3_bind_int(day_stmt, 4, sqlite3_column_int(hstmt, 2));
                    sqlite3_bind_int64(day_stmt, 5, now_seconds());
                    sqlite3_step(day_stmt);
                    sqlite3_finalize(day_stmt);
                }
            }
            sqlite3_finalize(hstmt);
            hstmt = NULL;
        }
        exec_sql("COMMIT");
    }
after_habits:

    if(mode == INBE_STORAGE_IMPORT_DATA_AND_SETTINGS)
        imported_settings = import_settings_from_source(src);
    if(imported_settings > 0)
        ok = 1;

    goto done;

try_tickmate:
    sqlite3_finalize(stmt);
    stmt = NULL;
    ok = import_tickmate_db(src);
    if(!ok)
        TraceLog(LOG_WARNING, "DATA: sqlite import was neither Inbe nor supported Tickmate schema");

done:
    if(deferred_materialize && g_storage.materialize_defer > 0) {
        g_storage.materialize_defer--;
        if(g_storage.materialize_defer == 0 && g_storage.materialize_needed) {
            g_storage.materialize_needed = 0;
            storage_materialize_session_habit_days();
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_finalize(hstmt);
    if(src != NULL)
        sqlite3_close(src);
    if(ok) {
        storage_enqueue_all_sync_state();
        storage_schedule_persist();
    }
    return ok;
}

int
storage_import_zip(const char *path)
{
    return storage_import_zip_ex(path, INBE_STORAGE_IMPORT_DATA_ONLY);
}

int
storage_import_zip_ex(const char *path, InbeStorageImportMode mode)
{
    mz_zip_archive archive;
    int ok = 0;

    if(path == NULL || path[0] == '\0') {
        TraceLog(LOG_ERROR, "DATA: import path is empty");
        return 0;
    }
    if(!path_exists(path)) {
        TraceLog(LOG_ERROR, "DATA: import path does not exist: %s", path);
        return 0;
    }
    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_reader_init_file(&archive, path, 0)) {
        return import_sqlite_db_file(path, mode);
    }
    if(mz_zip_reader_locate_file(&archive, "inbe-data/inbe.db", NULL, 0) >= 0) {
        char *db_bytes;
        size_t db_size = 0;
        char temp_path[INBE_STORAGE_PATH_SIZE];
        FILE *fp;
        db_bytes = mz_zip_reader_extract_file_to_heap(&archive, "inbe-data/inbe.db", &db_size, 0);
        if(!storage_join_path(temp_path, sizeof(temp_path), g_storage.root, "import-inbe.db")) {
            TraceLog(LOG_ERROR, "DATA: import temp path is too long");
            free(db_bytes);
            mz_zip_reader_end(&archive);
            return 0;
        }
        fp = fopen(temp_path, "wb");
        if(db_bytes != NULL && fp != NULL && fwrite(db_bytes, 1, db_size, fp) == db_size) {
            fclose(fp);
            fp = NULL;
            ok = import_sqlite_db_file(temp_path, mode);
            remove(temp_path);
            if(!ok)
                TraceLog(LOG_ERROR, "DATA: archive contained inbe-data/inbe.db but sqlite import failed");
        } else {
            TraceLog(LOG_ERROR, "DATA: failed to extract inbe-data/inbe.db from archive");
        }
        if(fp != NULL)
            fclose(fp);
        free(db_bytes);
    } else {
        ok = import_legacy_session_zip(&archive);
    }
    mz_zip_reader_end(&archive);
    if(!ok)
        TraceLog(LOG_ERROR, "DATA: import failed for %s", path);
    else
        storage_enqueue_all_sync_state();
    return ok;
}

int
storage_inspect_import(const char *path, InbeStorageImportInfo *info)
{
    mz_zip_archive archive;
    int ok = 0;

    if(info != NULL)
        memset(info, 0, sizeof(*info));
    if(path == NULL || path[0] == '\0' || info == NULL || !path_exists(path))
        return 0;

    memset(&archive, 0, sizeof(archive));
    if(!mz_zip_reader_init_file(&archive, path, 0))
        return inspect_sqlite_db_file(path, info);

    if(mz_zip_reader_locate_file(&archive, "inbe-data/inbe.db", NULL, 0) >= 0) {
        char *db_bytes;
        size_t db_size = 0;
        char temp_path[INBE_STORAGE_PATH_SIZE];
        FILE *fp;
        db_bytes = mz_zip_reader_extract_file_to_heap(&archive, "inbe-data/inbe.db", &db_size, 0);
        if(!storage_join_path(temp_path, sizeof(temp_path), g_storage.root,
                              "import-inspect-inbe.db")) {
            free(db_bytes);
            mz_zip_reader_end(&archive);
            return 0;
        }
        fp = fopen(temp_path, "wb");
        if(db_bytes != NULL && fp != NULL && fwrite(db_bytes, 1, db_size, fp) == db_size) {
            fclose(fp);
            fp = NULL;
            ok = inspect_sqlite_db_file(temp_path, info);
            remove(temp_path);
        }
        if(fp != NULL)
            fclose(fp);
        free(db_bytes);
    } else {
        mz_uint file_count = mz_zip_reader_get_num_files(&archive);
        for(mz_uint i = 0; i < file_count; i++) {
            mz_zip_archive_file_stat stat;
            int year;
            int month;
            int day;
            int hour;
            int minute;
            int second;

            if(!mz_zip_reader_file_stat(&archive, i, &stat) || stat.m_is_directory)
                continue;
            if(parse_legacy_session_filename(stat.m_filename, &year, &month, &day,
                                             &hour, &minute, &second))
                info->session_count++;
        }
        info->has_sessions = info->session_count > 0;
        info->valid = info->has_sessions;
        ok = info->valid;
    }
    mz_zip_reader_end(&archive);
    return ok;
}
