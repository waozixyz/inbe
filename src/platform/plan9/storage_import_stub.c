#include "storage/import.h"

int
storage_export_zip(const char *path)
{
    (void)path;
    return 0;
}

int
storage_import_zip(const char *path)
{
    (void)path;
    return 0;
}

int
storage_import_zip_ex(const char *path, InbeStorageImportMode mode)
{
    (void)path;
    (void)mode;
    return 0;
}

int
storage_inspect_import(const char *path, InbeStorageImportInfo *info)
{
    (void)path;
    if(info != 0) {
        info->valid = 0;
        info->has_sessions = 0;
        info->has_habits = 0;
        info->has_settings = 0;
        info->session_count = 0;
        info->habit_count = 0;
        info->setting_count = 0;
    }
    return 0;
}

void
migrate_legacy_file_sessions_once(void)
{
}
