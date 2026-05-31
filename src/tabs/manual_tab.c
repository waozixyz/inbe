#include "manual_tab.h"
#include "app.h"
#include "locale.h"
#include "ui/ui.h"
#include "ui/text_layout.h"
#include "theme_meta.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

#define CONTENT_MAX_W 800  /* Responsive width for tutorial text reflow */

extern int view_width;
extern int view_height;

extern Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

/* Tutorial text content ordered by step */
static const char *const TUTORIAL_KEYS[] = {
    "tutorial_step_intro",
    "tutorial_step_method",
    "tutorial_step_breathe",
    "tutorial_step_exhale_hold",
    "tutorial_step_inhale_hold"
};

#define TUTORIAL_STEPS_COUNT (sizeof(TUTORIAL_KEYS) / sizeof(TUTORIAL_KEYS[0]))

static const int TUTORIAL_LINE_SPACING[] = { 28, 24, 24, 28, 28 };

static void
init_tutorial_layouts(InbeApp *app, int body_font)
{
    if(!app->tutorial_layouts_initialized) {
        for(int i = 0; i < (int)TUTORIAL_STEPS_COUNT; i++) {
            app->tutorial_layouts[i] = calloc(1, sizeof(TextLayout));
            *app->tutorial_layouts[i] = ui_text_layout_parse(locale_get(TUTORIAL_KEYS[i]), (Texture2D){0}, UI_ICON_TYPE_NONE, body_font);
        }
        app->tutorial_layouts_initialized = 1;
    }
}

void
manual_tab_reset_layouts(InbeApp *app)
{
    if(app == NULL)
        return;

    for(int i = 0; i < (int)TUTORIAL_STEPS_COUNT; i++) {
        if(app->tutorial_layouts[i] != NULL) {
            ui_text_layout_free(app->tutorial_layouts[i]);
            free(app->tutorial_layouts[i]);
            app->tutorial_layouts[i] = NULL;
        }
    }
    app->tutorial_layouts_initialized = 0;
}

void
manual_tab_close_tutorial(InbeApp *app, int mark_seen)
{
    if(mark_seen && !app->tutorial_seen) {
        app->tutorial_seen = 1;
        save_settings(app);
    }
    app->tutorial_step = 0;
    app->manual_scroll = 0;
    app->inbe.screen = InbeScreenStart;
}

void
manual_tab_draw(InbeApp *app)
{
    int title_h = ui_screen_header_height();
    int tab_h = ui_clamp_px(54, 54, 66);
    int viewport_h = view_height - title_h - tab_h;
    int body_font = ui_clamp_px(16, 14, 18);
    int previous_step;
    int content_x;
    int content_w;
    const char *title = locale_get("tutorial_title");
    int footer_y = view_height - ui_px(38);
    int footer_mid_y = footer_y + ui_px(7);
    char page_label[16];
    int close_clicked = 0;
    int step = app->tutorial_step;

    step = clampi(step, 0, (int)TUTORIAL_STEPS_COUNT - 1);
    app->tutorial_step = step;
    previous_step = step;

    if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) {
        if(step < (int)TUTORIAL_STEPS_COUNT - 1)
            app->tutorial_step++;
        else
            manual_tab_close_tutorial(app, 1);
    }
    if(IsKeyPressed(KEY_LEFT) && step > 0)
        app->tutorial_step--;
    if(IsKeyPressed(KEY_ESCAPE))
        manual_tab_close_tutorial(app, 1);

    step = app->tutorial_step;
    if(previous_step != step) {
        app->manual_scroll = 0;
        previous_step = step;
    }

    switch(step) {
    case 1: title = locale_get("tutorial_method_title"); break;
    case 2: title = locale_get("tutorial_step1_title"); break;
    case 3: title = locale_get("tutorial_step2_title"); break;
    case 4: title = locale_get("tutorial_step3_title"); break;
    default: break;
    }

    /* Content width responds to window size - with DPI scaling consideration */
    /* On high-DPI Android, view_width is already large, so we use a reasonable percentage */
    int responsive_max_w = (int)(view_width * 0.92f);
    int max_content_w = ui_px(CONTENT_MAX_W);  /* DPI-scaled max width */
    if(responsive_max_w > max_content_w)
        responsive_max_w = max_content_w;
    int min_width = ui_px(280);
    if(responsive_max_w < min_width)
        responsive_max_w = min_width;

    int side_padding = ui_px(32);  /* Increased from CONTENT_SIDE_PAD=20 for more space */
    ui_centered_column(responsive_max_w, side_padding, &content_x, &content_w);

    /* Auto-reflow every frame - like HTML, no tracking needed */
    if(!app->tutorial_layouts_initialized) {
        init_tutorial_layouts(app, body_font);
    }

    /* Reflow all layouts for current container width - automatic like CSS */
    for(int i = 0; i < (int)TUTORIAL_STEPS_COUNT; i++) {
        ui_text_layout_reflow(app->tutorial_layouts[i], content_w, body_font, ui_px(TUTORIAL_LINE_SPACING[i]));
    }

    close_clicked = ui_draw_screen_header(app, title, 1);
    if(close_clicked)
        manual_tab_close_tutorial(app, 1);

    /* Calculate actual content height based on current step and text layouts */
    /* Don't include padding in this calculation - it's handled separately */
    int actual_content_h = 0;  /* Content only, no padding */
    if(step == 0) {
        actual_content_h += ui_px(200) + ui_px(22) + ui_text_layout_get_height(app->tutorial_layouts[0]);
        /* Image height must match drawing code which uses ui_px(200), not ui_px(224) */
    } else if(step == 1) {
        /* Step 1 uses a fresh gear_layout for drawing - calculate height from it */
        TextLayout temp_gear = ui_text_layout_parse(locale_get(TUTORIAL_KEYS[1]), app->gear_icon, UI_ICON_TYPE_GEAR, ui_px(14));
        ui_text_layout_reflow(&temp_gear, content_w, body_font, ui_px(24));
        actual_content_h += ui_text_layout_get_height(&temp_gear);
        ui_text_layout_free(&temp_gear);
    } else if(step == 2) {
        actual_content_h += ui_text_layout_get_height(app->tutorial_layouts[2]) + ui_px(20);
        /* Circle preview height - calculate actual rmax based on content_w */
        /* update_preview_bounds uses min(content_w, 132)/2 clamped to 60-120 for rmax */
        int preview_span = (content_w < ui_px(132)) ? content_w : ui_px(132);
        int preview_rmax = preview_span / 2;
        if(preview_rmax < ui_px(60)) preview_rmax = ui_px(60);
        if(preview_rmax > ui_px(120)) preview_rmax = ui_px(120);
        /* Circle is drawn at y+ui_px(40) with radius 0.72*preview_rmax */
        actual_content_h += ui_px(40) + (int)((float)preview_rmax * 0.72f) + ui_px(14);
        int slider_h = ui_clamp_px(36, 32, 40);
        actual_content_h += slider_h + ui_px(8);
    } else if(step == 3) {
        actual_content_h += ui_text_layout_get_height(app->tutorial_layouts[3]);
    } else {
        actual_content_h += ui_px(234) + ui_px(22) + ui_text_layout_get_height(app->tutorial_layouts[4]);
    }
    int available_content_space = viewport_h - ui_px(16) - ui_px(16);
    int old_max_scroll = 0;
    int max_scroll = actual_content_h - available_content_space;
    if(max_scroll < 0)
        max_scroll = 0;

    /* Reset scroll position if content now fits in viewport */
    if(max_scroll == 0 && old_max_scroll > 0) {
        app->manual_scroll = 0;
    }
    /* Clamp scroll position to new max */
    app->manual_scroll = clampi(app->manual_scroll, 0, max_scroll);
    app->manual_scroll -= (int)(GetMouseWheelMove() * 24.0f);
    app->manual_scroll = clampi(app->manual_scroll, 0, max_scroll);

    /* Handle content area drag scrolling */
    static int content_drag_active = 0;
    static int content_drag_start_y = 0;
    static int content_drag_start_scroll = 0;

    Vector2 mouse_pos = GetMousePosition();
    int content_area_y = title_h;
    int content_area_h = viewport_h;

    Rectangle content_bounds = {content_x, content_area_y, content_w, content_area_h};

    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if(!content_drag_active) {
            /* Start drag if clicking in content area */
            if(CheckCollisionPointRec(mouse_pos, content_bounds)) {
                content_drag_active = 1;
                content_drag_start_y = (int)mouse_pos.y;
                content_drag_start_scroll = app->manual_scroll;
            }
        } else {
            /* Continue drag */
            int dy = (int)mouse_pos.y - content_drag_start_y;
            app->manual_scroll = content_drag_start_scroll - dy;
            app->manual_scroll = clampi(app->manual_scroll, 0, max_scroll);
        }
    } else {
        content_drag_active = 0;
    }

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        int y = title_h + ui_px(16) - app->manual_scroll;
        if(step == 0) {
            int img_h = ui_px(200);
            ui_draw_tutorial_image(app->angel_image, "angel.jpg", content_x, y, content_w, img_h);
            y += img_h + ui_px(22);

            if(app->tutorial_layouts_initialized && app->tutorial_layouts[0] != NULL) {
                ui_text_layout_draw(app->tutorial_layouts[0], content_x, &y, body_font, c_text);
            }
        } else if(step == 1) {
            /* Step 1 text with gear icon - parse fresh each frame for proper icon binding */
            int icon_size = ui_px(14);
            TextLayout gear_layout = ui_text_layout_parse(locale_get(TUTORIAL_KEYS[1]), app->gear_icon, UI_ICON_TYPE_GEAR, icon_size);
            ui_text_layout_reflow(&gear_layout, content_w, body_font, ui_px(24));
            ui_text_layout_draw(&gear_layout, content_x, &y, body_font, c_text);
            ui_text_layout_free(&gear_layout);
        } else if(step == 2) {
            int speed = app->inbe.speed_level;
            if(app->tutorial_layouts_initialized && app->tutorial_layouts[2] != NULL) {
                ui_text_layout_draw(app->tutorial_layouts[2], content_x, &y, body_font, c_text);
            }
            y += ui_px(20);  /* Increased spacing between text and circle */

            update_preview_bounds(&app->settings_preview, content_w, ui_px(132));
            apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                           int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
            app->settings_preview.progressive_speed = 0;
            inbestep(&app->settings_preview);
            if(app->settings_preview.phase != InbePhaseBreathe) {
                reset_settings_preview(app);
                update_preview_bounds(&app->settings_preview, content_w, ui_px(132));
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                app->settings_preview.progressive_speed = 0;
            }
            draw_preview_inbe(&app->settings_preview, content_x + content_w / 2, y + ui_px(40));
            y += (int)(app->settings_preview.rmax * 0.72f) + ui_px(54);

            if(ui_draw_slider(app, 10, content_x, y, content_w, locale_get("speed_label"), SETTINGS_SPEED_MIN,
                           SETTINGS_SPEED_MAX, &speed, "")) {
                apply_settings(&app->inbe, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                app->settings_preview.progressive_speed = 0;
                app->settings_dirty = 1;
            }
        } else if(step == 3) {
            if(app->tutorial_layouts_initialized && app->tutorial_layouts[3] != NULL) {
                ui_text_layout_draw(app->tutorial_layouts[3], content_x, &y, body_font, c_text);
            }
        } else {
            int img_h = ui_px(234);
            ui_draw_tutorial_image(app->begin_image, "begin.jpg", content_x, y, content_w, img_h);
            y += img_h + ui_px(22);
            if(app->tutorial_layouts_initialized && app->tutorial_layouts[4] != NULL) {
                ui_text_layout_draw(app->tutorial_layouts[4], content_x, &y, body_font, c_text);
            }
        }
    EndScissorMode();

    /* Draw scrollbar if content overflows */
    if(max_scroll > 0) {
        int scrollbar_x = content_x + content_w + ui_px(4);  /* Reduced spacing */
        int scrollbar_viewport = available_content_space;  /* Scrollable area only */
        ui_draw_scrollbar(app, scrollbar_x, title_h + ui_px(16), scrollbar_viewport, actual_content_h,
                          &app->manual_scroll, max_scroll);
    }

    snprintf(page_label, sizeof(page_label), "%d/%d", step + 1, (int)TUTORIAL_STEPS_COUNT);
    DrawText(page_label,
             view_width / 2 - MeasureText(page_label, ui_clamp_px(14, 14, 16)) / 2,
             footer_mid_y, ui_clamp_px(14, 14, 16), c_text);

    int left_hover = 0;
    int right_hover = 0;
    int button_pad = ui_px(48);
    if(step == 0) {
    if(ui_draw_text_btn(app, content_x + button_pad, footer_y, locale_get("tutorial_skip_button"), &left_hover))
        manual_tab_close_tutorial(app, 1);
    } else {
        if(ui_draw_text_btn(app, content_x + button_pad, footer_y, locale_get("tutorial_back_button"), &left_hover)) {
            app->tutorial_step--;
            app->manual_scroll = 0;
        }
    }

    if(ui_draw_text_btn(app, content_x + content_w - button_pad, footer_y,
               step == (int)TUTORIAL_STEPS_COUNT - 1 ? locale_get("tutorial_finish_button") : locale_get("tutorial_next_button"), &right_hover)) {
        if(step == (int)TUTORIAL_STEPS_COUNT - 1)
            manual_tab_close_tutorial(app, 1);
        else {
            app->tutorial_step++;
            app->manual_scroll = 0;
        }
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        app->settings_drag_slider = 0;
        if(app->settings_dirty)
            save_settings(app);
    }
}
