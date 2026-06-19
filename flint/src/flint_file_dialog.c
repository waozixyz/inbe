#include "flint_file_dialog_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <time.h>

#include "flint_scaling.h"
#include "flint_clip.h"
#include "flint_color.h"
#include "flint_theme.h"
#include "flint_ui.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

static char g_dialog_theme_scope[FLINT_THEME_NAME_SIZE] = "sky_light";

static Color dialog_theme_get(const char *key) {
    return flint_theme_get(g_dialog_theme_scope, key);
}

void flint_file_dialog_set_theme_scope(const char *scope) {
    if(scope == NULL || scope[0] == '\0') {
        snprintf(g_dialog_theme_scope, sizeof(g_dialog_theme_scope), "%s", "sky_light");
        return;
    }

    snprintf(g_dialog_theme_scope, sizeof(g_dialog_theme_scope), "%s", scope);
}

static FlintFileDialogInternal *create_internal(void) {
    FlintFileDialogInternal *internal = calloc(1, sizeof(FlintFileDialogInternal));
    if(internal != NULL) {
        internal->hover_index = -1;
        internal->last_clicked_index = -1;
    }
    return internal;
}

static int compare_entries(const void *a, const void *b) {
    const DirEntry *ea = (const DirEntry *)a;
    const DirEntry *eb = (const DirEntry *)b;

    if(ea->is_dir && !eb->is_dir) return -1;
    if(!ea->is_dir && eb->is_dir) return 1;

    return strcmp(ea->name, eb->name);
}

static void join_path(char *dst, size_t dst_size, const char *dir, const char *name);

static void copy_text(char *dst, size_t dst_size, const char *src) {
    if(dst_size == 0) return;
    if(src == NULL) src = "";
    size_t len = strlen(src);
    if(len >= dst_size)
        len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static int ascii_tolower(int c) {
    if(c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

static int extension_equal(const char *a, const char *b, size_t n) {
    for(size_t i = 0; i < n; i++) {
        if(ascii_tolower((unsigned char)a[i]) != ascii_tolower((unsigned char)b[i]))
            return 0;
    }
    return 1;
}

static int file_matches_filter(const char *name, const char *filter) {
    const char *p;
    size_t name_len;

    if(filter == NULL || filter[0] == '\0')
        return 1;
    if(name == NULL)
        return 0;

    name_len = strlen(name);
    p = filter;
    while(*p != '\0') {
        char ext[32];
        size_t ext_len = 0;

        while(*p == ' ' || *p == ',')
            p++;
        while(*p != '\0' && *p != ',' && ext_len + 1 < sizeof(ext))
            ext[ext_len++] = *p++;
        ext[ext_len] = '\0';
        while(*p != '\0' && *p != ',')
            p++;

        if(ext_len > 0 && name_len >= ext_len &&
           extension_equal(name + name_len - ext_len, ext, ext_len))
            return 1;
    }

    return 0;
}

static int should_show_entry(FlintFileDialogInternal *internal, const char *name, int *is_dir, long *size) {
    char full_path[PATH_MAX];
    struct stat st;
    int entry_is_dir = 0;
    long entry_size = 0;

    if(internal == NULL || name == NULL)
        return 0;
    if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return 0;
    if(!internal->show_hidden_files && name[0] == '.')
        return 0;

    join_path(full_path, sizeof(full_path), internal->current_dir, name);
    if(stat(full_path, &st) == 0) {
        entry_is_dir = S_ISDIR(st.st_mode);
        entry_size = st.st_size;
    }

    if(!entry_is_dir && !file_matches_filter(name, internal->extension_filter))
        return 0;

    if(is_dir != NULL)
        *is_dir = entry_is_dir;
    if(size != NULL)
        *size = entry_size;
    return 1;
}

static void join_path(char *dst, size_t dst_size, const char *dir, const char *name) {
    if(dst_size == 0) return;
    const char *separator = strcmp(dir, "/") == 0 ? "" : "/";
    size_t dir_len = strlen(dir);
    size_t sep_len = strlen(separator);
    size_t name_len = strlen(name);
    size_t pos = 0;

    if(dir_len > dst_size - 1)
        dir_len = dst_size - 1;
    memcpy(dst + pos, dir, dir_len);
    pos += dir_len;

    if(pos < dst_size - 1) {
        if(sep_len > dst_size - 1 - pos)
            sep_len = dst_size - 1 - pos;
        memcpy(dst + pos, separator, sep_len);
        pos += sep_len;
    }

    if(pos < dst_size - 1) {
        if(name_len > dst_size - 1 - pos)
            name_len = dst_size - 1 - pos;
        memcpy(dst + pos, name, name_len);
        pos += name_len;
    }

    dst[pos] = '\0';
}

static int scan_directory(FlintFileDialogInternal *internal) {
    DIR *dir = opendir(internal->current_dir);
    if(!dir) return 0;

    struct dirent *entry;
    int count = 0;

    while((entry = readdir(dir))) {
        if(!should_show_entry(internal, entry->d_name, NULL, NULL)) continue;
        count++;
    }

    if(internal->entries) {
        free(internal->entries);
        internal->entries = NULL;
    }
    if(count > 0) {
        internal->entries = malloc((size_t)count * sizeof(DirEntry));
        if(!internal->entries) {
            closedir(dir);
            return 0;
        }
    }

    rewinddir(dir);
    internal->file_count = 0;

    while((entry = readdir(dir))) {
        int is_dir = 0;
        long size = 0;
        if(!should_show_entry(internal, entry->d_name, &is_dir, &size)) continue;

        copy_text(internal->entries[internal->file_count].name,
                  sizeof(internal->entries[internal->file_count].name),
                  entry->d_name);

        internal->entries[internal->file_count].is_dir = is_dir;
        internal->entries[internal->file_count].size = size;

        internal->file_count++;
    }
    closedir(dir);

    if(internal->file_count > 0) {
        qsort(internal->entries, internal->file_count,
              sizeof(DirEntry), compare_entries);
    }
    internal->hover_index = -1;
    internal->last_clicked_index = -1;
    internal->last_click_time = 0.0f;

    return 1;
}

static void navigate_home(FlintFileDialogInternal *internal) {
    const char *home = getenv("HOME");
    if(home) {
        copy_text(internal->current_dir, sizeof(internal->current_dir), home);
        scan_directory(internal);
    }
}

static void navigate_up(FlintFileDialogInternal *internal) {
    char *last_slash = strrchr(internal->current_dir, '/');
    if(last_slash && last_slash != internal->current_dir) {
        *last_slash = '\0';
        scan_directory(internal);
    } else if(last_slash == internal->current_dir && last_slash[1] != '\0') {
        last_slash[1] = '\0';
        scan_directory(internal);
    }
}

static void navigate_into(FlintFileDialogInternal *internal, const char *dirname) {
    char new_path[PATH_MAX];
    join_path(new_path, sizeof(new_path), internal->current_dir, dirname);
    if(strlen(new_path) == sizeof(new_path) - 1)
        return;

    struct stat st;
    if(stat(new_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        copy_text(internal->current_dir, sizeof(internal->current_dir), new_path);
        scan_directory(internal);
    }
}

static void render_header(FlintFileDialog *dlg, Rectangle dialog_rect) {
    const char *title = dlg->title;
    int title_font = flint_ui_font();
    Color title_color = dialog_theme_get("text");

    int title_x = dialog_rect.x + flint_px(16);
    int title_y = dialog_rect.y + flint_px(12);

    flint_text_draw(title, title_x, title_y, title_font, title_color);
}

static void render_breadcrumb(FlintFileDialog *dlg, Rectangle dialog_rect) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    int breadcrumb_y = dialog_rect.y + flint_px(40);
    int breadcrumb_x = dialog_rect.x + flint_px(16);
    int breadcrumb_height = flint_px(24);

    internal->breadcrumb_rect = (Rectangle){
        breadcrumb_x, breadcrumb_y,
        dialog_rect.width - flint_px(32), breadcrumb_height
    };

    Color bg = dialog_theme_get("background");
    DrawRectangleRec(internal->breadcrumb_rect, bg);

    Color border = flint_darken(bg, 30);
    DrawRectangleLinesEx(internal->breadcrumb_rect, 1, border);

    Color text = dialog_theme_get("text");
    int font = flint_ui_font_small();

    char display_path[PATH_MAX];
    copy_text(display_path, sizeof(display_path), internal->current_dir);

    int text_x = breadcrumb_x + flint_px(8);
    int text_y = breadcrumb_y + flint_px(4);
    int max_text_width = (int)internal->breadcrumb_rect.width - flint_px(16);
    while(display_path[0] != '\0' && flint_text_measure(display_path, font) > max_text_width) {
        memmove(display_path, display_path + 1, strlen(display_path));
        if(strlen(display_path) > 3) {
            display_path[0] = '.';
            display_path[1] = '.';
            display_path[2] = '.';
        }
    }
    flint_text_draw(display_path, text_x, text_y, font, text);
}

static void render_file_list(FlintFileDialog *dlg, Rectangle dialog_rect) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    int list_y = dialog_rect.y + flint_px(70);
    int list_x = dialog_rect.x + flint_px(16);
    int scrollbar_w = flint_px(12);
    int list_width = dialog_rect.width - flint_px(32);
    int bottom_y = dialog_rect.y + internal->dialog_height - flint_px(112);
    int list_height = bottom_y - list_y;
    if(list_height < FILE_DIALOG_ITEM_HEIGHT)
        list_height = FILE_DIALOG_ITEM_HEIGHT;

    internal->file_list_rect = (Rectangle){
        list_x, list_y, list_width, list_height
    };

    Color bg = flint_lighten(dialog_theme_get("background"), 10);
    DrawRectangleRec(internal->file_list_rect, bg);

    Color border = flint_darken(dialog_theme_get("background"), 30);
    DrawRectangleLinesEx(internal->file_list_rect, 1, border);

    flint_clip_begin(list_x, list_y, list_width, list_height);

    int font = flint_ui_font_small();
    Color text = dialog_theme_get("text");
    Color hover = dialog_theme_get("button_hover");
    Color selected = flint_darken(dialog_theme_get("button"), 20);

    int start_idx = internal->scroll_offset;
    int visible_items = MAX(1, (int)(list_height / FILE_DIALOG_ITEM_HEIGHT));
    int end_idx = MIN(start_idx + visible_items, internal->file_count);

    for(int i = start_idx; i < end_idx; i++) {
        int item_y = list_y + (i - start_idx) * FILE_DIALOG_ITEM_HEIGHT;
        Rectangle item_rect = {list_x, item_y, list_width - scrollbar_w, FILE_DIALOG_ITEM_HEIGHT};

        if(i == internal->hover_index) {
            DrawRectangleRec(item_rect, hover);
        }

        if(strcmp(internal->entries[i].name, internal->selected_file) == 0) {
            DrawRectangleRec(item_rect, selected);
        }

        char display_name[256];
        snprintf(display_name, sizeof(display_name), "%s", internal->entries[i].name);
        int max_name_width = list_width - scrollbar_w - flint_px(16);
        while(display_name[0] != '\0' && flint_text_measure(display_name, font) > max_name_width) {
            size_t len = strlen(display_name);
            if(len <= 4)
                break;
            display_name[len - 1] = '\0';
            display_name[len - 2] = '.';
            display_name[len - 3] = '.';
            display_name[len - 4] = '.';
        }
        flint_text_draw(display_name, list_x + flint_px(8), item_y + flint_px(4), font, text);
    }

    flint_clip_end();
}

static void render_scrollbar(FlintFileDialog *dlg, Rectangle dialog_rect) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
    (void)dialog_rect;

    int list_height = internal->file_list_rect.height;
    int visible_items = MAX(1, (int)(list_height / FILE_DIALOG_ITEM_HEIGHT));

    internal->scrollbar_rect = (Rectangle){0};
    if(internal->file_count <= visible_items) return;

    int scrollbar_x = internal->file_list_rect.x + internal->file_list_rect.width - flint_px(12);
    int scrollbar_y = internal->file_list_rect.y;
    int scrollbar_height = list_height;
    int scrollbar_width = flint_px(8);

    internal->scrollbar_rect = (Rectangle){
        scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_height
    };

    Color bg = flint_darken(dialog_theme_get("background"), 40);
    DrawRectangleRec(internal->scrollbar_rect, bg);

    int max_scroll = MAX(1, internal->file_count - visible_items);
    float thumb_ratio = (float)visible_items / (float)internal->file_count;
    int thumb_height = (int)(scrollbar_height * thumb_ratio);
    if(thumb_height < flint_px(24))
        thumb_height = flint_px(24);
    if(thumb_height > scrollbar_height)
        thumb_height = scrollbar_height;
    int thumb_y = scrollbar_y + (int)((internal->scroll_offset / (float)max_scroll) * (float)(scrollbar_height - thumb_height));

    Rectangle thumb_rect = {scrollbar_x, thumb_y, scrollbar_width, thumb_height};
    Color thumb = dialog_theme_get("button");
    DrawRectangleRec(thumb_rect, thumb);
}

static void render_filename_input(FlintFileDialog *dlg, Rectangle dialog_rect) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    int input_y = dialog_rect.y + internal->dialog_height - flint_px(96);
    int input_x = dialog_rect.x + flint_px(16);
    int font = flint_ui_font_small();
    int label_width = flint_text_measure("Filename:", font) + flint_px(8);
    int input_width = dialog_rect.width - flint_px(32) - label_width;
    int input_height = flint_px(24);
    if(input_width < flint_px(24))
        input_width = flint_px(24);

    internal->filename_rect = (Rectangle){
        input_x + label_width, input_y, input_width, input_height
    };

    Color bg = dialog_theme_get("background");
    DrawRectangleRec(internal->filename_rect, bg);

    Color border = flint_darken(bg, 30);
    Color accent = dialog_theme_get("button");

    if(internal->focus_area == 2) {
        DrawRectangleLinesEx(internal->filename_rect, 2, accent);
    } else {
        DrawRectangleLinesEx(internal->filename_rect, 1, border);
    }

    Color text = dialog_theme_get("text");
    Color label_dim = flint_darken(text, 40);
    flint_text_draw("Filename:", input_x, flint_ui_text_y("Filename:", input_y, input_height, font), font, label_dim);

    char display_filename[256];
    snprintf(display_filename, sizeof(display_filename), "%s", internal->filename_input);
    int max_filename_width = input_width - flint_px(8);
    while(display_filename[0] != '\0' && flint_text_measure(display_filename, font) > max_filename_width)
        memmove(display_filename, display_filename + 1, strlen(display_filename));

    flint_text_draw(display_filename, (int)internal->filename_rect.x + flint_px(4),
            input_y + flint_px(4), font, text);

    if(internal->focus_area == 2) {
        float current_time = GetTime();
        if(current_time - internal->cursor_blink_time > 0.5) {
            internal->cursor_visible = !internal->cursor_visible;
            internal->cursor_blink_time = current_time;
        }

        if(internal->cursor_visible) {
            int cursor_x = (int)internal->filename_rect.x + flint_px(4) +
                          flint_text_measure(display_filename, font);
            DrawLine(cursor_x, input_y + flint_px(4),
                    cursor_x, input_y + input_height - flint_px(4), text);
        }
    } else {
        internal->cursor_visible = 0;
    }
}

static void render_buttons(FlintFileDialog *dlg, Rectangle dialog_rect) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    int button_y = dialog_rect.y + internal->dialog_height - flint_px(58);
    int button_width = flint_px(80);
    int button_height = flint_px(24);

    internal->button_cancel_rect = (Rectangle){
        dialog_rect.x + dialog_rect.width - flint_px(176),
        button_y, button_width, button_height
    };

    internal->button_ok_rect = (Rectangle){
        dialog_rect.x + dialog_rect.width - flint_px(88),
        button_y, button_width, button_height
    };

    Color cancel_bg = dialog_theme_get("button");
    Color cancel_text = dialog_theme_get("text");

    DrawRectangleRec(internal->button_cancel_rect, cancel_bg);
    DrawRectangleLinesEx(internal->button_cancel_rect, 1,
                        flint_darken(cancel_bg, 30));

    flint_text_draw("Cancel",
            internal->button_cancel_rect.x + flint_px(8),
            internal->button_cancel_rect.y + flint_px(4),
            flint_ui_font_small(), cancel_text);

    Color ok_bg = dialog_theme_get("button_hover");
    Color ok_text = dialog_theme_get("text");

    DrawRectangleRec(internal->button_ok_rect, ok_bg);
    DrawRectangleLinesEx(internal->button_ok_rect, 1,
                        flint_darken(ok_bg, 30));

    flint_text_draw("OK",
            internal->button_ok_rect.x + flint_px(8),
            internal->button_ok_rect.y + flint_px(4),
            flint_ui_font_small(), ok_text);

    Color text = dialog_theme_get("text");
    int checkbox_size = flint_px(16);
    int checkbox_x = dialog_rect.x + flint_px(16);
    int checkbox_y = dialog_rect.y + internal->dialog_height - flint_px(28);

    internal->show_hidden_checkbox_rect = (Rectangle){
        checkbox_x, checkbox_y, checkbox_size, checkbox_size
    };

    DrawRectangleLinesEx(internal->show_hidden_checkbox_rect, 1, text);

    if(internal->show_hidden_files) {
        DrawRectangle(checkbox_x + 2, checkbox_y + 2,
                     checkbox_size - 4, checkbox_size - 4, text);
    }

    flint_text_draw("Show hidden files", checkbox_x + checkbox_size + flint_px(8),
            flint_ui_text_y("Show hidden files", checkbox_y, checkbox_size, flint_ui_font_small()),
            flint_ui_font_small(), text);
}

static void render_file_dialog(FlintFileDialog *dlg, Vector2 screen_size) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
    Color bg_color;

    /* Use exact application width and height */
    internal->dialog_width = (int)screen_size.x;
    internal->dialog_height = (int)screen_size.y;

    Rectangle dialog_rect = {
        0,
        0,
        screen_size.x,
        screen_size.y
    };

    bg_color = dialog_theme_get("background");
    bg_color.a = 255;
    DrawRectangleRec(dialog_rect, bg_color);

    render_header(dlg, dialog_rect);
    render_breadcrumb(dlg, dialog_rect);
    render_file_list(dlg, dialog_rect);
    render_scrollbar(dlg, dialog_rect);
    render_filename_input(dlg, dialog_rect);
    render_buttons(dlg, dialog_rect);
}

static int handle_mouse_input(FlintFileDialog *dlg) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
    Vector2 mouse = GetMousePosition();

    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if(CheckCollisionPointRec(mouse, internal->button_cancel_rect)) {
            cancel_dialog(dlg);
            return 1;
        }

        if(CheckCollisionPointRec(mouse, internal->button_ok_rect)) {
            confirm_dialog(dlg);
            return 1;
        }

        if(CheckCollisionPointRec(mouse, internal->show_hidden_checkbox_rect)) {
            internal->show_hidden_files = !internal->show_hidden_files;
            scan_directory(internal);
            return 0;
        }

        if(CheckCollisionPointRec(mouse, internal->file_list_rect)) {
            int hover_idx = internal->scroll_offset +
                           (int)((mouse.y - internal->file_list_rect.y) / FILE_DIALOG_ITEM_HEIGHT);

            if(hover_idx >= 0 && hover_idx < internal->file_count) {
                float current_time = GetTime();
                if(current_time - internal->last_click_time < 0.5 &&
                   internal->last_clicked_index == hover_idx) {
                    if(internal->entries[hover_idx].is_dir) {
                        navigate_into(internal, internal->entries[hover_idx].name);
                        internal->last_clicked_index = -1;
                    } else {
                        copy_text(internal->selected_file, sizeof(internal->selected_file),
                                  internal->entries[hover_idx].name);
                        copy_text(internal->filename_input, sizeof(internal->filename_input),
                                  internal->selected_file);
                        confirm_dialog(dlg);
                        return 1;
                    }
                } else {
                    internal->hover_index = hover_idx;
                    copy_text(internal->selected_file, sizeof(internal->selected_file),
                              internal->entries[hover_idx].name);
                    copy_text(internal->filename_input, sizeof(internal->filename_input),
                              internal->selected_file);
                }

                internal->last_click_time = current_time;
                internal->last_clicked_index = hover_idx;
            }
        }

        if(CheckCollisionPointRec(mouse, internal->filename_rect)) {
            internal->focus_area = 2;
        } else if(CheckCollisionPointRec(mouse, internal->breadcrumb_rect)) {
            navigate_up(internal);
            internal->focus_area = 1;
        } else {
            internal->focus_area = 0;
        }
    }

    if(IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, internal->scrollbar_rect)) {
        int list_height = internal->file_list_rect.height;
        int visible_items = MAX(1, (int)(list_height / FILE_DIALOG_ITEM_HEIGHT));
        int max_scroll = MAX(0, internal->file_count - visible_items);
        int scroll_pos = 0;
        if(list_height > 0) {
            scroll_pos = (int)(((mouse.y - internal->scrollbar_rect.y) /
                                (float)list_height) * internal->file_count);
        }
        internal->scroll_offset = scroll_pos;
        internal->scroll_offset = MAX(0, MIN(internal->scroll_offset, max_scroll));
    }

    if(CheckCollisionPointRec(mouse, internal->file_list_rect)) {
        int hover_idx = internal->scroll_offset +
                       (int)((mouse.y - internal->file_list_rect.y) / FILE_DIALOG_ITEM_HEIGHT);
        if(hover_idx >= 0 && hover_idx < internal->file_count) {
            internal->hover_index = hover_idx;
        }
    }

    float wheel = GetMouseWheelMove();
    if(wheel != 0) {
        internal->scroll_offset -= (int)wheel;
        int visible_items = MAX(1, (int)(internal->file_list_rect.height / FILE_DIALOG_ITEM_HEIGHT));
        internal->scroll_offset = MAX(0,
            MIN(internal->scroll_offset,
                MAX(0, internal->file_count - visible_items)));
    }

    return 0;
}

static int handle_keyboard_input(FlintFileDialog *dlg) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    if(IsKeyPressed(KEY_ESCAPE)) {
        cancel_dialog(dlg);
        return 1;
    }

    if(IsKeyPressed(KEY_ENTER)) {
        confirm_dialog(dlg);
        return 1;
    }

    if(internal->focus_area == 2) {
        if(IsKeyPressed(KEY_BACKSPACE)) {
            int len = strlen(internal->filename_input);
            if(len > 0)
                internal->filename_input[len - 1] = '\0';
        }

        int key = GetCharPressed();
        while(key > 0) {
            if(key >= 32 && key <= 125) {
                int len = strlen(internal->filename_input);
                if(len < 255) {
                    internal->filename_input[len] = (char)key;
                    internal->filename_input[len + 1] = '\0';
                }
            }
            key = GetCharPressed();
        }
    }

    if(IsKeyPressed(KEY_UP)) {
        if(internal->file_count > 0 && internal->hover_index > 0) {
            internal->hover_index--;
            copy_text(internal->selected_file, sizeof(internal->selected_file),
                      internal->entries[internal->hover_index].name);
            copy_text(internal->filename_input, sizeof(internal->filename_input),
                      internal->selected_file);

            if(internal->hover_index < internal->scroll_offset) {
                internal->scroll_offset = internal->hover_index;
            }
        }
    }

    if(IsKeyPressed(KEY_DOWN)) {
        if(internal->file_count > 0 && internal->hover_index < internal->file_count - 1) {
            internal->hover_index++;
            copy_text(internal->selected_file, sizeof(internal->selected_file),
                      internal->entries[internal->hover_index].name);
            copy_text(internal->filename_input, sizeof(internal->filename_input),
                      internal->selected_file);

            int visible_items = MAX(1, (int)(internal->file_list_rect.height / FILE_DIALOG_ITEM_HEIGHT));
            if(internal->hover_index >= internal->scroll_offset + visible_items) {
                internal->scroll_offset = internal->hover_index - visible_items + 1;
            }
        }
    }

    return 0;
}

static void confirm_dialog(FlintFileDialog *dlg) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    if(internal->filename_input[0] != '\0') {
        join_path(dlg->result_path, sizeof(dlg->result_path),
                  internal->current_dir, internal->filename_input);
        dlg->confirmed = 1;
        dlg->active = 0;
    }
}

static void cancel_dialog(FlintFileDialog *dlg) {
    dlg->result_path[0] = '\0';
    dlg->confirmed = 0;
    dlg->active = 0;
}

void flint_file_dialog_init(FlintFileDialog *dlg) {
    if(!dlg) return;

    memset(dlg, 0, sizeof(FlintFileDialog));
    dlg->_internal = create_internal();
    dlg->mode = FLINT_FILE_DIALOG_LOAD;

    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    navigate_home(internal);
    if(internal->current_dir[0] == '\0') {
        copy_text(internal->current_dir, sizeof(internal->current_dir), "/");
        scan_directory(internal);
    }
}

static int
run_dialog(FlintFileDialog *dlg)
{
    while(dlg->active && !WindowShouldClose()) {
        Color bg = dialog_theme_get("background");
        bg.a = 255;
        BeginDrawing();
        ClearBackground(bg);
        flint_file_dialog_update(dlg);
        EndDrawing();
    }
    return dlg->confirmed;
}

void
flint_file_dialog_begin_load_filtered(FlintFileDialog *dlg, const char *title, const char *filter)
{
    FlintFileDialogInternal *internal;

    if(!dlg || !title) return;
    flint_file_dialog_init(dlg);
    dlg->mode = FLINT_FILE_DIALOG_LOAD;
    copy_text(dlg->title, sizeof(dlg->title), title);
    copy_text(dlg->filter, sizeof(dlg->filter), filter != NULL ? filter : "");
    dlg->active = 1;
    dlg->confirmed = 0;

    internal = (FlintFileDialogInternal *)dlg->_internal;
    copy_text(internal->extension_filter, sizeof(internal->extension_filter), dlg->filter);
    scan_directory(internal);
    internal->filename_input[0] = '\0';
    internal->selected_file[0] = '\0';
    internal->hover_index = -1;
    internal->last_clicked_index = -1;
    internal->focus_area = 0;
}

void
flint_file_dialog_begin_load(FlintFileDialog *dlg, const char *title)
{
    flint_file_dialog_begin_load_filtered(dlg, title, NULL);
}

int flint_file_dialog_load_filtered(FlintFileDialog *dlg, const char *title, const char *filter) {
    if(!dlg || !title) return 0;
    flint_file_dialog_begin_load_filtered(dlg, title, filter);
    return run_dialog(dlg);
}

int flint_file_dialog_load(FlintFileDialog *dlg, const char *title) {
    if(!dlg || !title) return 0;
    flint_file_dialog_begin_load(dlg, title);
    return run_dialog(dlg);
}

void
flint_file_dialog_begin_save(FlintFileDialog *dlg, const char *title, const char *default_filename)
{
    FlintFileDialogInternal *internal;

    if(!dlg || !title || !default_filename) return;
    flint_file_dialog_init(dlg);
    dlg->mode = FLINT_FILE_DIALOG_SAVE;
    copy_text(dlg->title, sizeof(dlg->title), title);
    copy_text(dlg->default_name, sizeof(dlg->default_name), default_filename);
    copy_text(dlg->filter, sizeof(dlg->filter), "*.zip");
    dlg->active = 1;
    dlg->confirmed = 0;

    internal = (FlintFileDialogInternal *)dlg->_internal;
    copy_text(internal->filename_input, sizeof(internal->filename_input), default_filename);
    copy_text(internal->selected_file, sizeof(internal->selected_file), default_filename);
    internal->focus_area = 2;
}

int flint_file_dialog_save(FlintFileDialog *dlg, const char *title, const char *default_filename) {
    if(!dlg || !title || !default_filename) return 0;
    flint_file_dialog_begin_save(dlg, title, default_filename);
    return run_dialog(dlg);
}

int
flint_file_dialog_update(FlintFileDialog *dlg)
{
    Vector2 screen_size;

    if(dlg == NULL)
        return 0;
    if(!dlg->active)
        return dlg->confirmed ? 1 : 0;

    screen_size = (Vector2){(float)GetScreenWidth(), (float)GetScreenHeight()};
    render_file_dialog(dlg, screen_size);
    handle_mouse_input(dlg);
    handle_keyboard_input(dlg);

    if(dlg->active)
        return -1;
    return dlg->confirmed ? 1 : 0;
}

int flint_file_dialog_select_folder(FlintFileDialog *dlg, const char *title) {
    if(!dlg || !title) return 0;

    flint_file_dialog_init(dlg);
    dlg->mode = FLINT_FILE_DIALOG_SELECT_FOLDER;
    copy_text(dlg->title, sizeof(dlg->title), title);
    dlg->active = 1;
    dlg->confirmed = 0;

    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
    internal->filename_input[0] = '\0';
    internal->selected_file[0] = '\0';
    internal->hover_index = -1;
    internal->last_clicked_index = -1;
    internal->focus_area = 0;

    if(run_dialog(dlg)) {
        copy_text(dlg->result_path, sizeof(dlg->result_path), internal->current_dir);
    }

    return dlg->confirmed;
}

const char *flint_file_dialog_get_path(FlintFileDialog *dlg) {
    if(!dlg) return NULL;
    if(dlg->confirmed && dlg->result_path[0] != '\0')
        return dlg->result_path;
    return NULL;
}

void flint_file_dialog_cleanup(FlintFileDialog *dlg) {
    if(!dlg) return;

    if(dlg->_internal) {
        FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
        if(internal->entries) free(internal->entries);
        free(internal);
        dlg->_internal = NULL;
    }
}
