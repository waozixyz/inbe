#ifndef FLINT_FILE_DIALOG_H
#define FLINT_FILE_DIALOG_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLINT_FILE_DIALOG_PATH_MAX 512

typedef enum {
    FLINT_FILE_DIALOG_SAVE,
    FLINT_FILE_DIALOG_LOAD,
    FLINT_FILE_DIALOG_SELECT_FOLDER
} FlintFileDialogMode;

typedef struct {
    int active;
    FlintFileDialogMode mode;
    char title[128];
    char result_path[FLINT_FILE_DIALOG_PATH_MAX];
    char default_name[128];
    char filter[64];
    int confirmed;
    void *_internal;
} FlintFileDialog;

/* Initialize file dialog */
void flint_file_dialog_init(FlintFileDialog *dlg);

/* Save file dialog - returns 1 if file selected, 0 if cancelled */
int flint_file_dialog_save(FlintFileDialog *dlg, const char *title, const char *default_filename);

/* Load file dialog - returns 1 if file selected, 0 if cancelled */
int flint_file_dialog_load(FlintFileDialog *dlg, const char *title);

/* Select folder dialog - returns 1 if folder selected, 0 if cancelled */
int flint_file_dialog_select_folder(FlintFileDialog *dlg, const char *title);

/* Get selected file path */
const char *flint_file_dialog_get_path(FlintFileDialog *dlg);

/* Cleanup dialog resources */
void flint_file_dialog_cleanup(FlintFileDialog *dlg);

#ifdef __cplusplus
}
#endif

#endif /* FLINT_FILE_DIALOG_H */
