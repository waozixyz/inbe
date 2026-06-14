#ifndef DATA_H
#define DATA_H

#include <stddef.h>
#include "inbe.h"

void data_init(void);
const char *data_root(void);
const char *data_today_dir(void);
int data_save_session(const int *round_times, int round_count);
int data_save_session_path(const int *round_times, int round_count, char *out_path, size_t out_path_size);
int data_save_session_path_for_activity(const int *round_times, int round_count,
                                        int topic, int activity,
                                        char *out_path, size_t out_path_size);
int data_replace_session(const char *path, const int *round_times, int round_count);
int data_rename_session(const char *old_path, const char *new_path);
int data_delete_session(const char *path);
int data_has_any(void);
long long data_get_total_size(void);
int data_get_session_count(void);
long long data_delete_all(void);
int data_export(const char *path);
int data_import(const char *path);
int data_validate_import_file(const char *path);
typedef void (*data_session_callback)(const char *date, const char *time,
                                       int rounds, int best, void *user);
typedef void (*data_history_callback)(const char *path,
                                      int year, int month, int day,
                                      int hour, int minute, int second,
                                      int topic, int activity,
                                      const int *round_times, int round_count,
                                      void *user);

void data_list_sessions(data_session_callback callback, void *user);
void data_list_history(data_history_callback callback, void *user);
int data_load_session(const char *path, int *round_times, int max_rounds,
                      int *year, int *month, int *day,
                      int *hour, int *minute, int *second);

#endif
