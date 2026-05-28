#include "data_tab.h"
#include "data.h"
#include "app.h"
#include "ui.h"
#include "raylib.h"
#include <stdio.h>

/* Viewport dimensions - set by inbe_app_update_draw before calling draw functions */
extern int view_width;
extern int view_height;

/* Theme colors - set by ui_set_colors */
extern Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

/* ================================================================
 * DATA TAB DRAWING
 * ================================================================ */

void
data_tab_on_click(void *user_data)
{
    InbeApp *app = user_data;
    if(app != NULL) {
        app->settings_tab = SETTINGS_TAB_DATA;
        app->inbe.screen = InbeScreenSettings;
    }
}

static void
draw_stat_box(InbeApp *app __attribute__((unused)), int x, int y, int w, int h,
              const char *label, const char *value)
{
    Color c_box;

    (void)c_bg; /* unused */
    c_box = ui_darken(c_bg, 6);

    DrawRectangle(x, y, w, h, c_box);
    DrawLine(x, y, x + w, y, ui_darken(c_bg, 30));

    int font = ui_clamp_px(14, 12, 16);
    DrawText(label, x + ui_px(10), y + ui_px(8), font, ui_darken(c_text, 20));
    DrawText(value, x + ui_px(10), y + ui_px(28), font, c_text);
}

void
data_tab_draw(InbeApp *app)
{
    int center_x = view_width / 2;
    int title_h = ui_screen_header_height();
    int close_clicked = 0;

    (void)c_bg; /* unused */

    /* Draw shared header */
    close_clicked = ui_draw_screen_header(app, "Data Management", 1);
    if(close_clicked) {
        app->inbe.screen = InbeScreenStart;
        return;
    }

    /* Calculate stats */
    int session_count = data_get_session_count();
    long long total_size = data_get_total_size();
    int has_data = data_has_any();

    /* Format size for display */
    char size_str[32];
    if(total_size < 1024)
        snprintf(size_str, sizeof(size_str), "%lld B", total_size);
    else if(total_size < 1024 * 1024)
        snprintf(size_str, sizeof(size_str), "%.1f KB", total_size / 1024.0);
    else
        snprintf(size_str, sizeof(size_str), "%.1f MB", total_size / (1024.0 * 1024.0));

    /* Stats section */
    int box_x, box_w, stat_y = title_h + ui_px(20);
    ui_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &box_x, &box_w);

    int stat_box_h = ui_px(50);
    int stat_box_w = (box_w - ui_px(10)) / 2;

    DrawText("Statistics", box_x, stat_y, ui_clamp_px(18, 16, 20), c_text);
    stat_y += ui_px(30);

    /* Session count */
    char count_str[32];
    snprintf(count_str, sizeof(count_str), "%d sessions", session_count);
    draw_stat_box(app, box_x, stat_y, stat_box_w, stat_box_h, "Total Sessions", count_str);

    /* Data size */
    draw_stat_box(app, box_x + stat_box_w + ui_px(10), stat_y,
                  stat_box_w, stat_box_h, "Data Size", size_str);

    stat_y += stat_box_h + ui_px(20);

    /* Action buttons */
    int btn_y = stat_y;
    int btn_w = ui_px(140);
    int btn_h = ui_px(44);
    int hover_export = 0, hover_import = 0;

    /* Export button */
    int export_x = center_x - btn_w - ui_px(10);
    if(ui_draw_text_btn(app, export_x, btn_y, "EXPORT", &hover_export)) {
        /* TODO: Open file dialog for export */
        TraceLog(LOG_INFO, "DATA: Export button clicked (not yet implemented)");
    }

    /* Import button */
    int import_x = center_x + ui_px(10);
    if(ui_draw_text_btn(app, import_x, btn_y, "IMPORT", &hover_import)) {
        /* TODO: Open file dialog for import */
        TraceLog(LOG_INFO, "DATA: Import button clicked (not yet implemented)");
    }

    btn_y += btn_h + ui_px(30);

    /* Delete button (danger zone) */
    int hover_delete_danger = 0;
    int delete_x = center_x - btn_w / 2;

    if(!has_data) {
        /* No data to delete - show disabled button */
        Color disabled = ui_darken(c_text, 40);
        int btn_x = delete_x;
        DrawRectangle(btn_x, btn_y, btn_w, btn_h, ui_darken(c_bg, 10));
        DrawRectangleLinesEx((Rectangle){btn_x, btn_y, btn_w, btn_h}, 1,
                             ui_darken(c_bg, 20));
        int txt_w = MeasureText("NO DATA", ui_clamp_px(14, 12, 16));
        DrawText("NO DATA", btn_x + (btn_w - txt_w) / 2, btn_y + btn_h / 2 - 8,
                 ui_clamp_px(14, 12, 16), disabled);
    } else if(ui_draw_text_btn(app, delete_x, btn_y, "DELETE ALL", &hover_delete_danger)) {
        /* Show confirmation modal */
        app->modal.active = 1;
        app->modal.type = UIModalConfirmDeleteData;
        app->modal.selected_button = 0;
    }

    /* Draw modal if active */
    if(app->modal.active && app->modal.type == UIModalConfirmDeleteData) {
        int modal_result = ui_draw_modal(app, "Delete All Data?",
                                         "This will permanently delete all your session history.",
                                         "Cancel", "Delete");
        if(modal_result == 1) {
            /* Cancel */
            app->modal.active = 0;
            app->modal.type = UIModalNone;
        } else if(modal_result == 2) {
            /* Delete - actually delete the data */
            long long deleted = data_delete_all();
            TraceLog(LOG_INFO, "DATA: Deleted %lld session files", deleted);
            app->modal.active = 0;
            app->modal.type = UIModalNone;
        }
    }
}
