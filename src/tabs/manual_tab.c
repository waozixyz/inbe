#include "manual_tab.h"
#include "app.h"
#include "ui.h"
#include "text_layout.h"
#include "theme_meta.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

#define CONTENT_MAX_W 800  /* Increased from 600 to make resize more obvious */
#define CONTENT_SIDE_PAD 20

extern int view_width;
extern int view_height;

extern Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;

/* Tutorial text content */
static const char *tutorial_step0_text = "This breathing practice is based on the Wim Hof Method.\n\nIt can be powerful. Use it with care.\n\nPractice sitting or lying down.\nNever use it while driving,\nstanding, or in water.";

static const char *tutorial_step1_part1_text = "Simply follow 4 steps:\n\n1. Breathe rhythmically.\n2. Exhale and hold.\n3. Inhale deeply and hold.\n4. Exhale and repeat.";
static const char *tutorial_step1_part2_text = "Use the gear icon %i to adjust rounds, breaths, speed, and pauses.";

static const char *tutorial_step2_text = "Fill your lungs fully, then let the breath flow out.\n\nUse this slider to set the pace of the breathing circle.";

static const char *tutorial_step3_text = "After the breathing round,\nexhale normally and hold.\n\nRelease when your body asks\nfor air. Do not force it.";

static const char *tutorial_step4_text = "Inhale fully and hold for about\n15 seconds.\n\nThen exhale and begin the next round.\nOver time, each round may feel deeper.";

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

        printf("=== INITIALIZING TUTORIAL LAYOUTS ===\n");
        printf("Step 1 text: '%s'\n", tutorial_step1_part1_text);

        /* Parse text into layouts - done once */
        *app->tutorial_step0_layout = ui_text_layout_parse(tutorial_step0_text, (Texture2D){0}, UI_ICON_TYPE_NONE, body_font);
        *app->tutorial_step1_layout = ui_text_layout_parse(tutorial_step1_part1_text, (Texture2D){0}, UI_ICON_TYPE_NONE, body_font);
        *app->tutorial_step2_layout = ui_text_layout_parse(tutorial_step2_text, (Texture2D){0}, UI_ICON_TYPE_NONE, body_font);
        *app->tutorial_step3_layout = ui_text_layout_parse(tutorial_step3_text, (Texture2D){0}, UI_ICON_TYPE_NONE, body_font);
        *app->tutorial_step4_layout = ui_text_layout_parse(tutorial_step4_text, (Texture2D){0}, UI_ICON_TYPE_NONE, body_font);

        printf("Step 1 layout: %d elements, %d lines\n",
               app->tutorial_step1_layout->element_count, app->tutorial_step1_layout->line_count);

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
    int content_h = 430;
    int body_font = ui_clamp_px(16, 14, 18);  /* Slightly smaller to avoid bold rendering */
    int previous_step;
    int max_scroll;
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
    case 1: title = "Method"; content_h = 275; break;
    case 2: title = "Step 1: In & Out"; content_h = 315; break;
    case 3: title = "Step 2: Exhale & Hold"; content_h = 205; break;
    case 4: title = "Step 3: Inhale & Hold"; content_h = 440; break;
    default: break;
    }

    max_scroll = ui_px(content_h) - viewport_h;
    if(max_scroll < 0)
        max_scroll = 0;
    app->manual_scroll -= (int)(GetMouseWheelMove() * 24.0f);
    app->manual_scroll = clampi(app->manual_scroll, 0, max_scroll);

    /* Content width responds to window size - no limits */
    int responsive_max_w = (int)(view_width * 0.92f);  /* Slightly more horizontal padding */
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

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        int y = title_h + ui_px(16) - app->manual_scroll;
        if(app->tutorial_step == 0) {
            int img_h = ui_px(240);
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
            int img_h = ui_px(250);
            ui_draw_tutorial_image(app->begin_image, "begin.jpg", content_x, y, content_w, img_h);
            y += img_h + ui_px(22);
            if(app->tutorial_layouts_initialized && app->tutorial_step4_layout != NULL) {
                ui_text_layout_draw(app->tutorial_step4_layout, content_x, &y, body_font, c_text);
            }
        }
    EndScissorMode();

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

