#ifndef INBE_STORAGE_IMPORT_H
#define INBE_STORAGE_IMPORT_H

#include "storage.h"

int storage_export_zip(const char *path);
int storage_import_zip(const char *path);
int storage_import_zip_ex(const char *path, InbeStorageImportMode mode);
int storage_inspect_import(const char *path, InbeStorageImportInfo *info);
void migrate_legacy_file_sessions_once(void);

#endif
