#include "kryon.h"
#include "storage.h"
#include "data.h"
#include "storage_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

void
TraceLog(int level, const char *text, ...)
{
    va_list args;

    (void)level;
    va_start(args, text);
    vfprintf(stderr, text, args);
    va_end(args);
    fputc('\n', stderr);
}

bool
FileExists(const char *path)
{
    struct stat st;

    return path != NULL && stat(path, &st) == 0;
}

static int g_failures = 0;

static void
check_int(const char *label, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "FAIL %s: got %d want %d\n", label, got, want);
    g_failures++;
}

static void
check_str(const char *label, const char *got, const char *want)
{
    if(got != NULL && strcmp(got, want) == 0)
        return;
    fprintf(stderr, "FAIL %s: got %s want %s\n", label,
            got ? got : "(null)", want);
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

static char *
make_fixture(void)
{
    char *dir = strdup("/tmp/inbe-storage-paths-XXXXXX");

    if(mkdtemp(dir) == NULL) {
        perror("mkdtemp");
        exit(1);
    }
    return dir;
}

static void
make_dir(const char *path)
{
    if(mkdir(path, 0700) != 0 && errno != EEXIST) {
        perror("mkdir");
        exit(1);
    }
}

static void
make_dir_recursive(const char *path)
{
    char temp[512];
    char *p;

    snprintf(temp, sizeof(temp), "%s", path);
    for(p = temp + 1; *p != '\0'; p++) {
        if(*p != '/')
            continue;
        *p = '\0';
        make_dir(temp);
        *p = '/';
    }
    make_dir(temp);
}

static void
write_file(const char *dir, const char *name, const char *content)
{
    char path[512];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    fp = fopen(path, "wb");
    if(fp == NULL) {
        perror("fopen");
        exit(1);
    }
    fputs(content, fp);
    fclose(fp);
}

static int
file_exists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0;
}

/* data_root() caches its result in a static buffer, so every scenario that
   needs a different environment runs in a forked child. */
static void
run_scenario(void (*scenario)(const char *fixture), const char *fixture)
{
    pid_t pid = fork();
    int status = 0;

    if(pid < 0) {
        perror("fork");
        exit(1);
    }
    if(pid == 0) {
        scenario(fixture);
        _exit(g_failures == 0 ? 0 : 1);
    }
    if(waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        exit(1);
    }
    if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "FAIL scenario exited badly (status %d)\n", status);
        g_failures++;
    }
}

static void
scenario_override(const char *fixture)
{
    char wanted[512];

    snprintf(wanted, sizeof(wanted), "%s/override-root", fixture);
    setenv("APP_DATA_ROOT", wanted, 1);
    check_str("override root", data_root(), wanted);
    check_true("override root created", file_exists(wanted));
}

static void
scenario_xdg(const char *fixture)
{
    char wanted[512];

    unsetenv("APP_DATA_ROOT");
    setenv("XDG_DATA_HOME", fixture, 1);
    snprintf(wanted, sizeof(wanted), "%s/%s", fixture, STORAGE_EXPECT_DIR_HERE);
    check_str("xdg root", data_root(), wanted);
    check_true("xdg root created", file_exists(wanted));
}

static void
scenario_home(const char *fixture)
{
    char wanted[512];

    unsetenv("APP_DATA_ROOT");
    unsetenv("XDG_DATA_HOME");
    setenv("HOME", fixture, 1);
    snprintf(wanted, sizeof(wanted), "%s/.local/share/%s", fixture,
             STORAGE_EXPECT_DIR_HERE);
    check_str("home root", data_root(), wanted);
    check_true("home root created", file_exists(wanted));
}

static void
scenario_legacy_migrates(const char *fixture)
{
    char base[512];
    char legacy[512];
    char current[512];
    char moved_file[512];

    unsetenv("APP_DATA_ROOT");
    snprintf(base, sizeof(base), "%s/legacy-home", fixture);
    snprintf(legacy, sizeof(legacy), "%s/.local/share/%s", base,
             STORAGE_DIR_LEGACY_POSIX);
    snprintf(current, sizeof(current), "%s/.local/share/%s", base,
             STORAGE_DIR_NAME);
    make_dir_recursive(legacy);
    write_file(legacy, "sentinel.txt", "data");
    unsetenv("XDG_DATA_HOME");
    setenv("HOME", base, 1);
    check_str("legacy migrates to current", data_root(), current);
    snprintf(moved_file, sizeof(moved_file), "%s/sentinel.txt", current);
    check_true("legacy content moved", file_exists(moved_file));
    check_true("legacy dir gone", !file_exists(legacy));
}

static void
scenario_conflict_keeps_legacy_data(const char *fixture)
{
    char base[512];
    char legacy[512];
    char current[512];
    char archive[512];
    char moved_file[512];

    unsetenv("APP_DATA_ROOT");
    snprintf(base, sizeof(base), "%s/conflict-home", fixture);
    snprintf(legacy, sizeof(legacy), "%s/.local/share/%s", base,
             STORAGE_DIR_LEGACY_POSIX);
    snprintf(current, sizeof(current), "%s/.local/share/%s", base,
             STORAGE_DIR_NAME);
    snprintf(archive, sizeof(archive), "%s.before-breathing-migration-1",
             current);
    make_dir_recursive(legacy);
    write_file(legacy, "legacy.txt", "legacy");
    make_dir_recursive(current);
    write_file(current, "current.txt", "current");
    unsetenv("XDG_DATA_HOME");
    setenv("HOME", base, 1);
    check_str("conflict resolves to current", data_root(), current);
    snprintf(moved_file, sizeof(moved_file), "%s/legacy.txt", current);
    check_true("legacy data wins", file_exists(moved_file));
    check_true("previous current archived", file_exists(archive));
}

int
main(void)
{
    char *fixture;
    char filename[128];
    const char *export_entry = NULL;
    const char *historical_entry = NULL;
    int index;

    /* The proved decision table this platform build consumed. */
    check_int("fresh install uses current",
              storage_layout_decisions[0][0][0], STORAGE_ROOT_CURRENT);
    check_int("fresh install ignores move state",
              storage_layout_decisions[0][1][1], STORAGE_ROOT_CURRENT);
    check_int("successful move uses current",
              storage_layout_decisions[1][0][0], STORAGE_ROOT_CURRENT);
    check_int("failed move keeps legacy",
              storage_layout_decisions[1][0][1], STORAGE_ROOT_LEGACY);
    check_int("conflict keeps legacy",
              storage_layout_decisions[1][1][0], STORAGE_ROOT_LEGACY);
    check_int("conflict keeps legacy regardless of move",
              storage_layout_decisions[1][1][1], STORAGE_ROOT_LEGACY);

    /* The frozen compatibility vocabulary, pinned byte for byte. */
    check_str("export entry", STORAGE_EXPORT_ENTRY_DB,
              "breathing-data/breathing.db");
    check_str("export metadata entry", STORAGE_EXPORT_ENTRY_META,
              "breathing-data/metadata.json");
    check_str("export metadata format", STORAGE_EXPORT_META_FORMAT,
              "breathing-data-sqlite");
    check_str("database name", STORAGE_DB_NAME, "inbe.db");
    check_str("legacy database name", STORAGE_DB_NAME_LEGACY, "breathing.db");
    check_str("web home", STORAGE_WEB_HOME, "/home/inbe");
    check_str("web legacy home", STORAGE_WEB_HOME_LEGACY, "/home/breathing");
    check_str("posix expected dir", STORAGE_EXPECT_DIR_HERE, "inbe");
    check_str("posix legacy dir", STORAGE_DIR_LEGACY_HERE, "breathing");
    check_str("windows legacy dir", STORAGE_DIR_LEGACY_WINDOWS, "BreathSession");
    check_int("import entry count", STORAGE_IMPORT_ENTRY_COUNT, 2);
    for(index = 0; index < STORAGE_IMPORT_ENTRY_COUNT; index++) {
        if(strcmp(storage_import_entries[index], "breathing-data/breathing.db") == 0)
            export_entry = storage_import_entries[index];
        if(strcmp(storage_import_entries[index], "inbe-data/inbe.db") == 0)
            historical_entry = storage_import_entries[index];
    }
    check_true("import list has the export entry", export_entry != NULL);
    check_true("import list has the historical entry", historical_entry != NULL);

    /* The visible export filename follows the current name. */
    data_default_export_filename(filename, sizeof(filename));
    check_true("export filename uses current prefix",
               strncmp(filename, "inbe-", 5) == 0);
    check_true("export filename ends with .zip",
               strlen(filename) > 4 &&
               strcmp(filename + strlen(filename) - 4, ".zip") == 0);

    fixture = make_fixture();
    run_scenario(scenario_override, fixture);
    run_scenario(scenario_xdg, fixture);
    run_scenario(scenario_home, fixture);
    run_scenario(scenario_legacy_migrates, fixture);
    run_scenario(scenario_conflict_keeps_legacy_data, fixture);

    if(g_failures == 0) {
        printf("storage path tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d storage path check(s) failed\n", g_failures);
    return 1;
}
