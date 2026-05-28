#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

/* File dialog state */
typedef struct {
    int active;
    int mode; /* 0 = save, 1 = load */
    char path[512];
    char filename[128];
    int cursor;
} FileDialog;

/* Initialize file dialog */
void file_dialog_init(FileDialog *dlg);

/* Show save file dialog
 * dlg: Dialog state
 * title: Dialog title
 * default_filename: Suggested filename
 * Returns: 1 if user confirmed, 0 if cancelled */
int file_dialog_save(FileDialog *dlg, const char *title, const char *default_filename);

/* Get the selected path
 * dlg: Dialog state
 * Returns: Full path to selected file */
const char *file_dialog_get_path(FileDialog *dlg);

#endif
