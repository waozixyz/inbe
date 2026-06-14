#ifndef INBE_STORAGE_H
#define INBE_STORAGE_H

#include "raylib.h"
#include <stddef.h>

enum {
    INBE_STORAGE_PATH_SIZE = 512,
    INBE_STORAGE_ID_SIZE = 64
};

typedef void (*InbeStorageHistoryCallback)(const char *id,
                                           int year, int month, int day,
                                           int hour, int minute, int second,
                                           int topic, int activity,
                                           const int *rounds, int round_count,
                                           void *user);

int inbe_storage_init(const char *root);
void inbe_storage_close(void);
const char *inbe_storage_db_path(void);

int inbe_storage_get_setting_int(const char *key, int fallback);
const char *inbe_storage_get_setting_text(const char *key);
void inbe_storage_set_setting_int(const char *key, int value);
void inbe_storage_set_setting_text(const char *key, const char *value);
int inbe_storage_settings_empty(void);

int inbe_storage_save_session(const int *round_times, int round_count,
                              char *out_id, size_t out_id_size);
int inbe_storage_save_session_for_activity(const int *round_times, int round_count,
                                           int topic, int activity,
                                           char *out_id, size_t out_id_size);
int inbe_storage_replace_session(const char *id, const int *round_times, int round_count);
int inbe_storage_rename_session_time(const char *id, int hour, int minute);
int inbe_storage_delete_session(const char *id);
int inbe_storage_load_session(const char *id, int *round_times, int max_rounds,
                              int *year, int *month, int *day,
                              int *hour, int *minute, int *second);
void inbe_storage_list_history(InbeStorageHistoryCallback callback, void *user);
int inbe_storage_has_any(void);
int inbe_storage_session_count(void);
long long inbe_storage_total_size(void);
long long inbe_storage_delete_all_sessions(void);

int inbe_storage_habits_empty(void);
int inbe_storage_habits_load(void *habits);
void inbe_storage_habits_save(const void *habits);
void inbe_storage_mark_habits_initialized(void);

int inbe_storage_export_zip(const char *path);
int inbe_storage_import_zip(const char *path);

#endif
