#ifndef HISTORY_TAB_H
#define HISTORY_TAB_H

#include <stddef.h>
#include "../libinbe/inbe.h"

typedef struct InbeApp InbeApp;

/* History constants */
enum {
    HISTORY_MAX_SESSIONS = 48,
    HISTORY_PATH_SIZE = 512,
    HISTORY_TEXT_SIZE = 96,
};

/* History entry structure */
typedef struct HistoryEntry {
    char path[HISTORY_PATH_SIZE];
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int round_count;
    int best;
    int avg_seconds;
    int rounds[MaxRounds];
} HistoryEntry;

/* Draw the history tab */
void history_tab_draw(InbeApp *app);

/* Handle tab click - switch to history screen */
void history_tab_on_click(void *user_data);

/* Open the latest session in history viewer */
void history_open_latest(InbeApp *app);

/* Load a session file and populate a history entry */
void history_load_session_file(const char *path, HistoryEntry *entry);

/* Clear the record selection in history viewer */
void history_clear_record_selection(InbeApp *app);

/* Format session label for display (e.g., "14:30 5 rounds 45s") */
void history_format_session_label(const HistoryEntry *entry, char *out, size_t out_size);

#endif
