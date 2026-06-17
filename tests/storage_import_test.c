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
#include <unistd.h>

static int g_failures = 0;
static int g_seen_topic = -1;
static int g_seen_activity = -1;

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
write_tickmate_database(const char *path)
{
    sqlite3 *db = NULL;
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
                            "INSERT INTO tracks(_id,name,description,icon,enabled,color,\"order\") "
                            "VALUES(1,'Meditation','Silenced my mind','',1,8925,0);"
                            "INSERT INTO ticks(_track_id,year,month,day,hour,minute,second,has_time_info) "
                            "VALUES(1,2026,6,13,0,0,0,0);",
                            NULL, NULL, &error) == SQLITE_OK);
    if(error != NULL) {
        fprintf(stderr, "tickmate setup SQL error: %s\n", error);
        sqlite3_free(error);
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
    check_true("tickmate habit day", inbe_habit_completed_day(&habits.items[0], 20260613));
    check_true("tickmate habit name", strcmp(habits.items[0].name, "Meditation") == 0);
    inbe_storage_close();

    remove_tree(source);
    remove_tree(dest);
}

int
main(void)
{
    test_raw_db_import();
    test_zip_db_import();
    test_tickmate_db_import();
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
