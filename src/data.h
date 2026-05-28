#ifndef DATA_H
#define DATA_H

#include <stddef.h>
#include "../libinbe/inbe.h"

/* Initialize data module - call once at app startup */
void data_init(void);

/* Get the root data directory path
 * Returns: Path like "/home/user/.local/share/lotus" or ".local/lotus"
 * The returned pointer is valid for the lifetime of the program */
const char *data_root(void);

/* Get today's session directory path (YYYY/MM/DD)
 * Returns: Path like data_root() + "/2025/05/28"
 * The returned pointer is valid until the next call */
const char *data_today_dir(void);

/* Save session results
 * round_times: Array of round durations in seconds
 * round_count: Number of rounds to save
 * Returns: 1 on success, 0 on failure */
int data_save_session(const int *round_times, int round_count);

/* Check if any session data exists
 * Returns: 1 if data exists, 0 otherwise */
int data_has_any(void);

/* Get total size of all session data in bytes
 * Returns: Total size in bytes, or -1 on error */
long long data_get_total_size(void);

/* Get the total number of sessions stored
 * Returns: Number of session files, or -1 on error */
int data_get_session_count(void);

/* Delete all session data
 * Returns: Number of files deleted, or -1 on error */
long long data_delete_all(void);

/* Export all data to a zip file
 * path: Destination zip file path
 * Returns: 1 on success, 0 on failure */
int data_export(const char *path);

/* Import data from a zip file
 * path: Source zip file path
 * Returns: 1 on success, 0 on failure */
int data_import(const char *path);

/* Session callback type for listing sessions
 * date: Date string "YYYY-MM-DD"
 * time: Time string "HH:MM:SS" extracted from filename
 * rounds: Number of rounds in session
 * best: Best round time in seconds
 * user: User data pointer passed to callback */
typedef void (*data_session_callback)(const char *date, const char *time,
                                       int rounds, int best, void *user);

/* List all sessions
 * callback: Function called for each session found
 * user: User data pointer passed to callback */
void data_list_sessions(data_session_callback callback, void *user);

#endif
