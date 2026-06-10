#include "manual_tab.h"
#include "app.h"
#include "locale.h"
#include "flint_ui.h"
#include "flint_text_layout.h"
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
    "tutorial_step_progressive_speed",
    "tutorial_step_breathe",
    "tutorial_step_exhale_hold",
    "tutorial_step_inhale_hold"
};

#define TUTORIAL_STEPS_COUNT (sizeof(TUTORIAL_KEYS) / sizeof(TUTORIAL_KEYS[0]))
#define TUTORIAL_LINE_SPACING flint_px(8)  /* Normal readable line spacing */

/*
 * Wrapper for compatibility - using Flint's unified button utility
 * This function is deprecated - use ui_draw_generic_button() directly
 */
static int
draw_tutorial_footer_button(InbeApp *app, int x, int y, int w, int h, const char *label, int *hover)
{
    (void)app;
    return ui_draw_generic_button(x, y, w, h, label, UI_BUTTON_STYLE_PRIMARY, hover);
}

static void
draw_tutorial_hold_preview(InbeApp *app, int center_x, int center_y, int radius)
{
    int seconds = (app->inbe.frame / 60) % 60;
    char text[CountSize];
    int font = flint_px(16);
    int thickness = flint_px(5);

    count_from_int(text, seconds);
    if(thickness < 3)
        thickness = 3;

    if(app->hold_display_mode == HOLD_DISPLAY_CIRCLE) {
        float sweep = 360.0f * (float)((app->inbe.frame % (60 * 60))) / (float)(60 * 60);

        DrawRing((Vector2){center_x, center_y},
                 (float)(radius + flint_px(8) - thickness / 2),
                 (float)(radius + flint_px(8) + thickness / 2),
                 -90.0f, -90.0f + sweep, 96, c_text);
    } else {
        DrawCircle(center_x, center_y, radius, c_circle);
        DrawCircleLines(center_x, center_y, radius, c_text);
    }

    flint_ui_draw_text_centered(text, center_x, center_y, font, c_text);
}

static void
init_tutorial_layouts(InbeApp *app, int body_font)
{
    if(!app->tutorial_layouts_initialized) {
        for(int i = 0; i < (int)TUTORIAL_STEPS_COUNT; i++) {
            app->tutorial_layouts[i] = calloc(1, sizeof(FlintTextLayout));
            *app->tutorial_layouts[i] = flint_text_layout_parse(locale_get(TUTORIAL_KEYS[i]), (Texture2D){0}, FLINT_ICON_TYPE_NONE, body_font);
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
            flint_text_layout_free(app->tutorial_layouts[i]);
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
    int tab_h = flint_px(56);
    int viewport_h = view_height - title_h - tab_h;
    int body_font = flint_ui_font();
    int footer_content_pad = flint_ui_font() / 2;
    int content_area_h = viewport_h - footer_content_pad;  /* For scissor mode and scroll calculations */
    int previous_step;
    int content_x;
    int content_w;
    const char *title = locale_get("tutorial_title");
    char page_label[32];
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
    case 2: title = locale_get("tutorial_progressive_speed_title"); break;
    case 3: title = locale_get("tutorial_step1_title"); break;
    case 4: title = locale_get("tutorial_step2_title"); break;
    case 5: title = locale_get("tutorial_step3_title"); break;
    default: break;
    }

    /* Calculate responsive content width */
    int responsive_max_w = (int)(view_width * 0.96f);
    int max_content_w = flint_px(CONTENT_MAX_W);
    if(responsive_max_w > max_content_w)
        responsive_max_w = max_content_w;
    int min_width = flint_px(280);
    if(responsive_max_w < min_width)
        responsive_max_w = min_width;

    int side_padding = flint_page_side_padding();
    flint_centered_column(responsive_max_w, side_padding, &content_x, &content_w);

    /* Auto-reflow every frame */
    if(!app->tutorial_layouts_initialized) {
        init_tutorial_layouts(app, body_font);
    }

    /* Reflow all layouts for current container width */
    for(int i = 0; i < (int)TUTORIAL_STEPS_COUNT; i++) {
        flint_text_layout_reflow(app->tutorial_layouts[i], content_w, body_font, TUTORIAL_LINE_SPACING);
    }

    close_clicked = ui_draw_screen_header(title, 1);
    if(close_clicked)
        manual_tab_close_tutorial(app, 1);

    /* Calculate actual content height based on current step and text layouts */
    /* Don't include padding in this calculation - it's handled separately */
    int actual_content_h = 0;  /* Content only, no padding */
    if(step == 0) {
        actual_content_h += flint_px(170) + flint_px(22) + flint_text_layout_get_height(app->tutorial_layouts[0]);
    } else if(step == 1) {
        /* Step 1 uses a fresh gear_layout for drawing - calculate height from it */
        FlintTextLayout temp_gear = flint_text_layout_parse(locale_get(TUTORIAL_KEYS[1]), app->gear_icon, FLINT_ICON_TYPE_GEAR, flint_px(14));
        flint_text_layout_reflow(&temp_gear, content_w, body_font, TUTORIAL_LINE_SPACING);
        actual_content_h += flint_text_layout_get_height(&temp_gear);
        flint_text_layout_free(&temp_gear);
    } else if(step == 2) {
        actual_content_h += flint_text_layout_get_height(app->tutorial_layouts[2]) + flint_px(68);
        if(app->inbe.progressive_speed)
            actual_content_h += flint_px(66);
    } else if(step == 3) {
        actual_content_h += flint_text_layout_get_height(app->tutorial_layouts[3]) + flint_px(20);
        /* Circle preview height - calculate actual rmax based on content_w */
        /* update_preview_bounds uses min(content_w, 132)/2 clamped to 60-120 for rmax */
        int preview_span = (content_w < flint_px(132)) ? content_w : flint_px(132);
        int preview_rmax = preview_span / 2;
        if(preview_rmax < flint_px(60)) preview_rmax = flint_px(60);
        if(preview_rmax > flint_px(120)) preview_rmax = flint_px(120);
        /* Circle is drawn at y+flint_px(40) with radius 0.72*preview_rmax */
        actual_content_h += flint_px(40) + (int)((float)preview_rmax * 0.72f) + flint_px(14);
        int slider_h = flint_px(40);
        actual_content_h += slider_h + flint_px(8);
    } else if(step == 4) {
        int hold_preview_radius = flint_px(54);
        int hold_preview_extent = hold_preview_radius + flint_px(8) + flint_px(5);
        actual_content_h += flint_px(26) + flint_px(36) + flint_px(20);
        actual_content_h += hold_preview_extent * 2 + flint_px(24);
        actual_content_h += flint_text_layout_get_height(app->tutorial_layouts[4]);
    } else {
        actual_content_h += flint_px(200) + flint_px(22) + flint_text_layout_get_height(app->tutorial_layouts[5]);
    }
    /* Calculate scroll range for virtual content space */
    int top_padding = flint_px(16);
    int total_content_h = actual_content_h + top_padding;
    int viewport_content_h = content_area_h;
    int max_scroll = total_content_h - viewport_content_h;
    if(max_scroll < 0)
        max_scroll = 0;

    /* Reset scroll position if content now fits in viewport */
    if(max_scroll == 0) {
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
                     (int)(content_area_h * app->camera.zoom));
        int y = title_h + flint_px(16) - app->manual_scroll;
        if(step == 0) {
            int img_h = flint_px(170);
            ui_draw_tutorial_image(app->angel_image, "angel.jpg", content_x, y, content_w, img_h);
            y += img_h + flint_px(22);

            if(app->tutorial_layouts_initialized && app->tutorial_layouts[0] != NULL) {
                flint_text_layout_draw(app->tutorial_layouts[0], content_x, &y, body_font, c_text);
            }
        } else if(step == 1) {
            /* Step 1 text with gear icon - parse fresh each frame for proper icon binding */
            int icon_size = flint_px(14);
            FlintTextLayout gear_layout = flint_text_layout_parse(locale_get(TUTORIAL_KEYS[1]), app->gear_icon, FLINT_ICON_TYPE_GEAR, icon_size);
            flint_text_layout_reflow(&gear_layout, content_w, body_font, TUTORIAL_LINE_SPACING);
            flint_text_layout_draw(&gear_layout, content_x, &y, body_font, c_text);
            flint_text_layout_free(&gear_layout);
        } else if(step == 2) {
            if(app->tutorial_layouts_initialized && app->tutorial_layouts[2] != NULL) {
                flint_text_layout_draw(app->tutorial_layouts[2], content_x, &y, body_font, c_text);
            }
            y += flint_px(18);
            {
                int progressive_speed = app->inbe.progressive_speed;
                int progressive_start_speed = app->inbe.progressive_start_speed;
                int toggle_w = flint_px(56);
                int toggle_h = flint_px(30);
                flint_text_draw(locale_get("progressive_speed_label"), content_x, y,
                         flint_ui_font(), c_text);
                y += flint_px(26);
                if(ui_draw_toggle_switch(content_x, y, toggle_w, toggle_h,
                                         &progressive_speed, locale_get("toggle_off"),
                                         locale_get("toggle_on"))) {
                    app->inbe.progressive_speed = progressive_speed;
                    app->settings_preview.progressive_speed = progressive_speed;
                    app->settings_dirty = 1;
                }
                y += toggle_h + flint_px(20);
                if(app->inbe.progressive_speed) {
                    int max_speed = app->inbe.speed_level;
                    progressive_start_speed = clampi(progressive_start_speed, SETTINGS_SPEED_MIN, max_speed);
                    if(ui_draw_slider(11, content_x, y, content_w,
                                      locale_get("progressive_start_speed_label"),
                                      SETTINGS_SPEED_MIN, max_speed,
                                      &progressive_start_speed, "")) {
                        app->inbe.progressive_start_speed = progressive_start_speed;
                        app->settings_preview.progressive_start_speed = progressive_start_speed;
                        app->settings_dirty = 1;
                    }
                }
            }
        } else if(step == 3) {
            int speed = app->inbe.speed_level;
            if(app->tutorial_layouts_initialized && app->tutorial_layouts[3] != NULL) {
                flint_text_layout_draw(app->tutorial_layouts[3], content_x, &y, body_font, c_text);
            }
            y += flint_px(20);  /* Increased spacing between text and circle */

            update_preview_bounds(&app->settings_preview, content_w, flint_px(132));
            apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                           int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
            app->settings_preview.progressive_speed = 0;
            inbestep(&app->settings_preview);
            if(app->settings_preview.phase != InbePhaseBreathe) {
                reset_settings_preview(app);
                update_preview_bounds(&app->settings_preview, content_w, flint_px(132));
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                app->settings_preview.progressive_speed = 0;
            }
            draw_preview_inbe(&app->settings_preview, content_x + content_w / 2, y + flint_px(40));
            y += (int)(app->settings_preview.rmax * 0.72f) + flint_px(54);

            if(ui_draw_slider(10, content_x, y, content_w, locale_get("speed_label"), SETTINGS_SPEED_MIN,
                           SETTINGS_SPEED_MAX, &speed, "")) {
                apply_settings(&app->inbe, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                app->settings_preview.progressive_speed = 0;
                app->settings_dirty = 1;
            }
        } else if(step == 4) {
            int hold_preview_radius = flint_px(54);
            int hold_preview_extent = hold_preview_radius + flint_px(8) + flint_px(5);
            flint_text_draw(locale_get("hold_display_label"), content_x, y, flint_ui_font(), c_text);
            y += flint_px(26);
            draw_hold_display_mode_selector(app, content_x, y, content_w);
            y += flint_px(36) + flint_px(20) + hold_preview_extent;
            draw_tutorial_hold_preview(app, content_x + content_w / 2, y, hold_preview_radius);
            y += hold_preview_extent + flint_px(24);
            if(app->tutorial_layouts_initialized && app->tutorial_layouts[4] != NULL) {
                flint_text_layout_draw(app->tutorial_layouts[4], content_x, &y, body_font, c_text);
            }
        } else {
            int img_h = flint_px(200);
            ui_draw_tutorial_image(app->begin_image, "begin.jpg", content_x, y, content_w, img_h);
            y += img_h + flint_px(22);
            if(app->tutorial_layouts_initialized && app->tutorial_layouts[5] != NULL) {
                flint_text_layout_draw(app->tutorial_layouts[5], content_x, &y, body_font, c_text);
            }
        }
    EndScissorMode();

    /* Draw scrollbar if content overflows */
    if(max_scroll > 0) {
        /* Transform world coordinates to screen coordinates for scrollbar function */
        int scrollbar_x = (int)(app->camera.offset.x + (content_x + content_w + flint_px(4)) * app->camera.zoom);
        int scrollbar_y = (int)(app->camera.offset.y + title_h * app->camera.zoom);
        int scrollbar_viewport = (int)(content_area_h * app->camera.zoom);  /* Scaled by camera zoom */
        int total_content_screen_h = (int)(total_content_h * app->camera.zoom);  /* Also scaled for ratio calculations */
        ui_draw_scrollbar(scrollbar_x, scrollbar_y, scrollbar_viewport, total_content_screen_h,
                          &app->manual_scroll, max_scroll);
    }

    locale_format(page_label, sizeof(page_label), "tutorial_page_label",
                  step + 1, (int)TUTORIAL_STEPS_COUNT);

    int left_hover = 0;
    int right_hover = 0;
    const char *left_label = step == 0 ? locale_get("tutorial_skip_button") : locale_get("tutorial_back_button");
    const char *right_label = step == (int)TUTORIAL_STEPS_COUNT - 1 ? locale_get("tutorial_finish_button") : locale_get("tutorial_next_button");
    int footer_gap = flint_px(10);
    int page_font = flint_ui_font();
    int button_h = flint_px(34);
    int button_w = (content_w - footer_gap) / 2;
    int footer_y = view_height - flint_px(38);
    int counter_gap = flint_px(6);


    flint_text_draw(page_label,
             view_width / 2 - flint_text_measure(page_label, page_font) / 2,
             footer_y - page_font - counter_gap, page_font, c_text);

    if(step == 0) {
        if(draw_tutorial_footer_button(app, content_x, footer_y, button_w, button_h, left_label, &left_hover))
            manual_tab_close_tutorial(app, 1);
    } else {
        if(draw_tutorial_footer_button(app, content_x, footer_y, button_w, button_h, left_label, &left_hover)) {
            app->tutorial_step--;
            app->manual_scroll = 0;
        }
    }

    if(draw_tutorial_footer_button(app, content_x + button_w + footer_gap, footer_y, button_w, button_h, right_label, &right_hover)) {
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
