#include "manual_tab.h"
#include "app.h"
#include "ui.h"
#include "text_layout.h"
#include "theme_meta.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

#define CONTENT_MAX_W 800  /* Responsive width for tutorial text reflow */

extern int view_width;
extern int view_height;

extern Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

/* Tutorial text content */
static const char *tutorial_step0_text = "This breathing practice is based on the Wim Hof Method.\n\nIt can be powerful. Use it with care.\n\nPractice sitting or lying down.\nNever use it while driving,\nstanding, or in water.";

static const char *tutorial_step1_part1_text = "Simply follow 4 steps:\n\n1. Breathe rhythmically.\n2. Exhale and hold.\n3. Inhale deeply and hold.\n4. Exhale and repeat.\n\n";
static const char *tutorial_step1_part2_text = "Use the gear icon %i to adjust rounds, breaths, speed, and pauses.";

static const char *tutorial_step2_text = "Fill your lungs fully, then let the breath flow out.\n\nUse this slider to set the pace of the breathing circle.\n\nFind a rhythm that feels natural for you.\n\n";

static const char *tutorial_step3_text = "After the breathing round,\nexhale normally and hold.\n\nRelease when your body asks\nfor air. Do not force it.";

static const char *tutorial_step4_text = "Inhale fully and hold for about 15 seconds.\n\nThen exhale and begin the next round.\nOver time, each round may feel deeper.";

static void
init_tutorial_layouts(InbeApp *app, int body_font)
{
    if(!app->tutorial_layouts_initialized) {
        /* Allocate memory for layouts */
        app->tutorial_step0_layout = calloc(1, sizeof(TextLayout));
        app->tutorial_step1_layout = calloc(1, sizeof(TextLayout));
        app->tutorial_step2_layout = calloc(1, sizeof(TextLayout));
        app->tutorial_step3_layout = calloc(1, sizeof(TextLayout));
        app->tutorial_step4_layout = calloc(1, sizeof(TextLayout));

        /* Parse text into layouts - done once */
        *app->tutorial_step0_layout = ui_text_layout_parse(tutorial_step0_text, (Texture2D){0}, UI_ICON_TYPE_NONE, body_font);
        *app->tutorial_step1_layout = ui_text_layout_parse(tutorial_step1_part1_text, (Texture2D){0}, UI_ICON_TYPE_NONE, body_font);
        *app->tutorial_step2_layout = ui_text_layout_parse(tutorial_step2_text, (Texture2D){0}, UI_ICON_TYPE_NONE, body_font);
        *app->tutorial_step3_layout = ui_text_layout_parse(tutorial_step3_text, (Texture2D){0}, UI_ICON_TYPE_NONE, body_font);
        *app->tutorial_step4_layout = ui_text_layout_parse(tutorial_step4_text, (Texture2D){0}, UI_ICON_TYPE_NONE, body_font);

        app->tutorial_layouts_initialized = 1;
    }
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
    int body_font = ui_clamp_px(16, 14, 18);  /* Slightly smaller to avoid bold rendering */
    int previous_step;
    int max_scroll = 0;  /* Will be calculated based on actual content height */
    int content_x;
    int content_w;
    const char *title = "Tutorial";
    int footer_y = view_height - ui_px(38);
    int footer_mid_y = footer_y + ui_px(7);
    char page_label[16];
    int close_clicked = 0;

    app->tutorial_step = clampi(app->tutorial_step, 0, TUTORIAL_STEPS - 1);
    previous_step = app->tutorial_step;

    if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) {
        if(app->tutorial_step < TUTORIAL_STEPS - 1)
            app->tutorial_step++;
        else
            manual_tab_close_tutorial(app, 1);
    }
    if(IsKeyPressed(KEY_LEFT) && app->tutorial_step > 0)
        app->tutorial_step--;
    if(IsKeyPressed(KEY_ESCAPE))
        manual_tab_close_tutorial(app, 1);

    if(previous_step != app->tutorial_step) {
        app->manual_scroll = 0;
        previous_step = app->tutorial_step;
    }

    switch(app->tutorial_step) {
    case 1: title = "Method"; break;
    case 2: title = "Step 1: In & Out"; break;
    case 3: title = "Step 2: Exhale & Hold"; break;
    case 4: title = "Step 3: Inhale & Hold"; break;
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
    ui_text_layout_reflow(app->tutorial_step0_layout, content_w, body_font, ui_px(28));  /* Good spacing */
    ui_text_layout_reflow(app->tutorial_step1_layout, content_w, body_font, ui_px(24));  /* Good spacing */
    ui_text_layout_reflow(app->tutorial_step2_layout, content_w, body_font, ui_px(24));  /* Good spacing */
    ui_text_layout_reflow(app->tutorial_step3_layout, content_w, body_font, ui_px(28));  /* Good spacing */
    ui_text_layout_reflow(app->tutorial_step4_layout, content_w, body_font, ui_px(28));  /* Good spacing */

    close_clicked = ui_draw_screen_header(app, title, 1);
    if(close_clicked)
        manual_tab_close_tutorial(app, 1);

    /* Calculate actual content height based on current step and text layouts */
    /* Don't include padding in this calculation - it's handled separately */
    int actual_content_h = 0;  /* Content only, no padding */
    if(app->tutorial_step == 0) {
        actual_content_h += ui_px(200) + ui_px(22) + ui_text_layout_get_height(app->tutorial_step0_layout);
        /* Image height must match drawing code which uses ui_px(200), not ui_px(224) */
    } else if(app->tutorial_step == 1) {
        actual_content_h += ui_text_layout_get_height(app->tutorial_step1_layout);
        actual_content_h += ui_px(12);  /* Spacing between text parts */
        /* Calculate gear icon layout height */
        TextLayout temp_gear = ui_text_layout_parse(tutorial_step1_part2_text, app->gear_icon, UI_ICON_TYPE_GEAR, ui_px(14));
        ui_text_layout_reflow(&temp_gear, content_w, body_font, ui_px(19));
        actual_content_h += ui_text_layout_get_height(&temp_gear);
        ui_text_layout_free(&temp_gear);
    } else if(app->tutorial_step == 2) {
        actual_content_h += ui_text_layout_get_height(app->tutorial_step2_layout) + ui_px(20);
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
    } else if(app->tutorial_step == 3) {
        actual_content_h += ui_text_layout_get_height(app->tutorial_step3_layout);
    } else {
        actual_content_h += ui_px(234) + ui_px(22) + ui_text_layout_get_height(app->tutorial_step4_layout);
    }

    /* Update max_scroll based on actual content height
     * Drawing area starts at title_h + ui_px(16) and content takes actual_content_h
     * Available space is viewport_h - ui_px(16) top padding - ui_px(16) bottom padding */
    int available_content_space = viewport_h - ui_px(16) - ui_px(16);
    int old_max_scroll = max_scroll;
    max_scroll = actual_content_h - available_content_space;
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
        if(app->tutorial_step == 0) {
            int img_h = ui_px(200);
            ui_draw_tutorial_image(app->angel_image, "angel.jpg", content_x, y, content_w, img_h);
            y += img_h + ui_px(22);

            if(app->tutorial_layouts_initialized && app->tutorial_step0_layout != NULL) {
                ui_text_layout_draw(app->tutorial_step0_layout, content_x, &y, body_font, c_text);
            }
        } else if(app->tutorial_step == 1) {
            if(app->tutorial_layouts_initialized && app->tutorial_step1_layout != NULL) {
                ui_text_layout_draw(app->tutorial_step1_layout, content_x, &y, body_font, c_text);
            }
            y += ui_px(12);

            /* Step 1 part 2 with gear icon - needs to be recreated each time for proper icon binding */
            int icon_size = ui_px(14);
            TextLayout gear_layout = ui_text_layout_parse(tutorial_step1_part2_text, app->gear_icon, UI_ICON_TYPE_GEAR, icon_size);
            ui_text_layout_reflow(&gear_layout, content_w, body_font, ui_px(19));
            ui_text_layout_draw(&gear_layout, content_x, &y, body_font, c_text);
            ui_text_layout_free(&gear_layout);
        } else if(app->tutorial_step == 2) {
            int speed = app->inbe.speed_level;
            if(app->tutorial_layouts_initialized && app->tutorial_step2_layout != NULL) {
                ui_text_layout_draw(app->tutorial_step2_layout, content_x, &y, body_font, c_text);
            }
            y += ui_px(20);  /* Increased spacing between text and circle */

            update_preview_bounds(&app->settings_preview, content_w, ui_px(132));
            apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                           int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
            inbestep(&app->settings_preview);
            if(app->settings_preview.phase != InbePhaseBreathe) {
                reset_settings_preview(app);
                update_preview_bounds(&app->settings_preview, content_w, ui_px(132));
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
            }
            draw_preview_inbe(&app->settings_preview, content_x + content_w / 2, y + ui_px(40));
            y += (int)(app->settings_preview.rmax * 0.72f) + ui_px(54);

            if(ui_draw_slider(app, 10, content_x, y, content_w, "Speed", SETTINGS_SPEED_MIN,
                           SETTINGS_SPEED_MAX, &speed, "")) {
                apply_settings(&app->inbe, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                app->settings_dirty = 1;
            }
        } else if(app->tutorial_step == 3) {
            if(app->tutorial_layouts_initialized && app->tutorial_step3_layout != NULL) {
                ui_text_layout_draw(app->tutorial_step3_layout, content_x, &y, body_font, c_text);
            }
        } else {
            int img_h = ui_px(234);
            ui_draw_tutorial_image(app->begin_image, "begin.jpg", content_x, y, content_w, img_h);
            y += img_h + ui_px(22);
            if(app->tutorial_layouts_initialized && app->tutorial_step4_layout != NULL) {
                ui_text_layout_draw(app->tutorial_step4_layout, content_x, &y, body_font, c_text);
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

    snprintf(page_label, sizeof(page_label), "%d/%d", app->tutorial_step + 1, TUTORIAL_STEPS);
    DrawText(page_label,
             view_width / 2 - MeasureText(page_label, ui_clamp_px(14, 14, 16)) / 2,
             footer_mid_y, ui_clamp_px(14, 14, 16), c_text);

    int left_hover = 0;
    int right_hover = 0;
    int button_pad = ui_px(48);
    if(app->tutorial_step == 0) {
        if(ui_draw_text_btn(app, content_x + button_pad, footer_y, "SKIP", &left_hover))
            manual_tab_close_tutorial(app, 1);
    } else {
        if(ui_draw_text_btn(app, content_x + button_pad, footer_y, "BACK", &left_hover)) {
            app->tutorial_step--;
            app->manual_scroll = 0;
        }
    }

    if(ui_draw_text_btn(app, content_x + content_w - button_pad, footer_y,
               app->tutorial_step == TUTORIAL_STEPS - 1 ? "FINISH" : "NEXT", &right_hover)) {
        if(app->tutorial_step == TUTORIAL_STEPS - 1)
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

