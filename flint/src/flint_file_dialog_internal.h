#ifndef FLINT_FILE_DIALOG_INTERNAL_H
#define FLINT_FILE_DIALOG_INTERNAL_H

#include "flint_file_dialog.h"
#include <dirent.h>
#include <sys/stat.h>

#define FILE_DIALOG_ITEM_HEIGHT 28

typedef struct {
    char name[256];
    int is_dir;
    long size;
} DirEntry;

typedef struct {
    char current_dir[PATH_MAX];
    char selected_file[256];
    char filename_input[256];
    int file_count;
    DirEntry *entries;
    int scroll_offset;
    int hover_index;
    int focus_area;
    Rectangle breadcrumb_rect;
    Rectangle file_list_rect;
    Rectangle filename_rect;
    Rectangle button_ok_rect;
    Rectangle button_cancel_rect;
    Rectangle show_hidden_checkbox_rect;
    Rectangle scrollbar_rect;
    float last_click_time;
    float cursor_blink_time;
    int cursor_visible;
    int dialog_width;  /* Dynamic width */
    int dialog_height;  /* Dynamic height */
    int show_hidden_files;  /* 0 = hide, 1 = show */
} FlintFileDialogInternal;

/* File system operations */
static int scan_directory(FlintFileDialogInternal *internal);
static int compare_entries(const void *a, const void *b);
static void navigate_up(FlintFileDialogInternal *internal);
static void navigate_into(FlintFileDialogInternal *internal, const char *dirname);
static void navigate_home(FlintFileDialogInternal *internal);

/* Rendering functions */
static void render_file_dialog(FlintFileDialog *dlg, Vector2 screen_size);
static void render_header(FlintFileDialog *dlg, Rectangle dialog_rect);
static void render_breadcrumb(FlintFileDialog *dlg, Rectangle dialog_rect);
static void render_file_list(FlintFileDialog *dlg, Rectangle dialog_rect);
static void render_filename_input(FlintFileDialog *dlg, Rectangle dialog_rect);
static void render_buttons(FlintFileDialog *dlg, Rectangle dialog_rect);
static void render_scrollbar(FlintFileDialog *dlg, Rectangle dialog_rect);

/* Event handling */
static int handle_mouse_input(FlintFileDialog *dlg);
static int handle_keyboard_input(FlintFileDialog *dlg);
static void confirm_dialog(FlintFileDialog *dlg);
static void cancel_dialog(FlintFileDialog *dlg);

#endif
