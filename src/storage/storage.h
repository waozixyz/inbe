#ifndef INBE_STORAGE_H
#define INBE_STORAGE_H

#include "raylib.h"
#include <stddef.h>

enum {
    INBE_STORAGE_PATH_SIZE = 512,
    INBE_STORAGE_ID_SIZE = 64
};

typedef void (*InbeStorageSessionRecordCallback)(const char *id,
                                                 int year, int month, int day,
                                                 int hour, int minute, int second,
                                                 int topic, int activity,
                                                 const int *rounds, int round_count,
                                                 void *user);

typedef enum InbeStorageImportMode {
    INBE_STORAGE_IMPORT_DATA_ONLY = 0,
    INBE_STORAGE_IMPORT_DATA_AND_SETTINGS = 1
} InbeStorageImportMode;

typedef struct InbeStorageImportInfo {
    int valid;
    int has_sessions;
    int has_habits;
    int has_settings;
    int session_count;
    int habit_count;
    int setting_count;
} InbeStorageImportInfo;

int storage_init(const char *root);
void storage_close(void);
const char *storage_db_path(void);

int storage_get_setting_int(const char *key, int fallback);
const char *storage_get_setting_text(const char *key);
void storage_set_setting_int(const char *key, int value);
void storage_set_setting_text(const char *key, const char *value);
void storage_settings_begin_write(void);
void storage_settings_end_write(void);
int storage_settings_empty(void);

int storage_save_session(const int *round_times, int round_count,
                              char *out_id, size_t out_id_size);
int storage_save_session_for_activity(const int *round_times, int round_count,
                                           int topic, int activity,
                                           char *out_id, size_t out_id_size);
int storage_save_session_at_for_activity(int local_date, int hour, int minute, int second,
                                         const int *round_times, int round_count,
                                         int topic, int activity,
                                         char *out_id, size_t out_id_size);
int storage_replace_session(const char *id, const int *round_times, int round_count);
int storage_rename_session_time(const char *id, int hour, int minute);
int storage_delete_session(const char *id);
int storage_load_session(const char *id, int *round_times, int max_rounds,
                              int *year, int *month, int *day,
                              int *hour, int *minute, int *second);
void storage_list_session_records(InbeStorageSessionRecordCallback callback, void *user);
int storage_has_any(void);
int storage_session_count(void);
int storage_habit_count(void);
long long storage_total_size(void);
long long storage_delete_all_sessions(void);
char *storage_build_sync_payload_json(const char *user_id_hash,
                                           const char *public_key_hex);
void storage_free_sync_payload_json(char *payload);
int storage_apply_sync_response_json(const char *response_json);
int storage_last_sync_changed(void);
int storage_sync_review_pending(void);
int storage_sync_review_details(char **local_out, char **remote_out);
int storage_sync_review_diff(char **diff_out);
int storage_apply_pending_sync_review(int use_remote);
void storage_purge_synced_deleted_data(void);
const char *storage_sync_client_id(void);
void storage_reset_sync_state(void);

int storage_habits_empty(void);
int storage_habits_load(void *habits);
void storage_habits_save(const void *habits);
void storage_mark_habits_initialized(void);

int storage_export_zip(const char *path);
int storage_import_zip(const char *path);
int storage_import_zip_ex(const char *path, InbeStorageImportMode mode);
int storage_inspect_import(const char *path, InbeStorageImportInfo *info);

#endif
