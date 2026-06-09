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

static const int TUTORIAL_LINE_SPACING[] = { 28, 24, 24, 24, 28, 28 };

static int
draw_tutorial_footer_button(InbeApp *app, int x, int y, int w, int h, const char *label, int *hover)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int font = flint_ui_font();
    int text_w;
    int text_x;
    int text_y;
    int pressed = 0;

    if(CheckCollisionPointRec(mouse_world, bounds)) {
        DrawRectangle(x, y, w, h, c_button_hover);
        ui_draw_bevel(x, y, w, h, flint_darken(c_button_hover, 40), flint_lighten(c_button_hover, 40));
        *hover = 1;
        app->cursor_clickable = 1;
        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            ui_draw_bevel(x, y, w, h, flint_lighten(c_button_hover, 40), flint_darken(c_button_hover, 40));
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !ui_dropdown_captures_click(mouse_world))
            pressed = 1;
    } else {
        DrawRectangle(x, y, w, h, c_button);
        ui_draw_bevel(x, y, w, h, flint_lighten(c_button, 40), flint_darken(c_button, 40));
        *hover = 0;
    }

    text_w = MeasureText(label, font);
    text_x = x + (w - text_w) / 2;
    text_y = flint_ui_text_y(label, y, h, font);
    DrawText(label, text_x, text_y, font, c_text);

    return pressed;
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

    /* Content width responds to window size - with DPI scaling consideration */
    /* On high-DPI Android, view_width is already large, so we use a reasonable percentage */
    int responsive_max_w = (int)(view_width * 0.96f);
    int max_content_w = flint_px(CONTENT_MAX_W);  /* DPI-scaled max width */
    if(responsive_max_w > max_content_w)
        responsive_max_w = max_content_w;
    int min_width = flint_px(280);
    if(responsive_max_w < min_width)
        responsive_max_w = min_width;

    int side_padding = flint_page_side_padding();
    flint_centered_column(responsive_max_w, side_padding, &content_x, &content_w);

    /* Auto-reflow every frame - like HTML, no tracking needed */
    if(!app->tutorial_layouts_initialized) {
        init_tutorial_layouts(app, body_font);
    }

    /* Reflow all layouts for current container width - automatic like CSS */
    for(int i = 0; i < (int)TUTORIAL_STEPS_COUNT; i++) {
        flint_text_layout_reflow(app->tutorial_layouts[i], content_w, body_font, flint_px(TUTORIAL_LINE_SPACING[i]));
    }

    close_clicked = ui_draw_screen_header(app, title, 1);
    if(close_clicked)
        manual_tab_close_tutorial(app, 1);

    /* Calculate actual content height based on current step and text layouts */
    /* Don't include padding in this calculation - it's handled separately */
    int actual_content_h = 0;  /* Content only, no padding */
    if(step == 0) {
        actual_content_h += flint_px(200) + flint_px(22) + flint_text_layout_get_height(app->tutorial_layouts[0]);
        /* Image height must match drawing code which uses flint_px(200), not flint_px(224) */
    } else if(step == 1) {
        /* Step 1 uses a fresh gear_layout for drawing - calculate height from it */
        FlintTextLayout temp_gear = flint_text_layout_parse(locale_get(TUTORIAL_KEYS[1]), app->gear_icon, FLINT_ICON_TYPE_GEAR, flint_px(14));
        flint_text_layout_reflow(&temp_gear, content_w, body_font, flint_px(24));
        actual_content_h += flint_text_layout_get_height(&temp_gear);
        flint_text_layout_free(&temp_gear);
    } else if(step == 2) {
        actual_content_h += flint_text_layout_get_height(app->tutorial_layouts[2]) + flint_px(68);
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
        actual_content_h += flint_text_layout_get_height(app->tutorial_layouts[4]);
    } else {
        actual_content_h += flint_px(234) + flint_px(22) + flint_text_layout_get_height(app->tutorial_layouts[5]);
    }
    int available_content_space = viewport_h - footer_content_pad - flint_px(16) - flint_px(16);
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
    int content_area_h = viewport_h - footer_content_pad;

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
            int img_h = flint_px(200);
            ui_draw_tutorial_image(app->angel_image, "angel.jpg", content_x, y, content_w, img_h);
            y += img_h + flint_px(22);

            if(app->tutorial_layouts_initialized && app->tutorial_layouts[0] != NULL) {
                flint_text_layout_draw(app->tutorial_layouts[0], content_x, &y, body_font, c_text);
            }
        } else if(step == 1) {
            /* Step 1 text with gear icon - parse fresh each frame for proper icon binding */
            int icon_size = flint_px(14);
            FlintTextLayout gear_layout = flint_text_layout_parse(locale_get(TUTORIAL_KEYS[1]), app->gear_icon, FLINT_ICON_TYPE_GEAR, icon_size);
            flint_text_layout_reflow(&gear_layout, content_w, body_font, flint_px(24));
            flint_text_layout_draw(&gear_layout, content_x, &y, body_font, c_text);
            flint_text_layout_free(&gear_layout);
        } else if(step == 2) {
            if(app->tutorial_layouts_initialized && app->tutorial_layouts[2] != NULL) {
                flint_text_layout_draw(app->tutorial_layouts[2], content_x, &y, body_font, c_text);
            }
            y += flint_px(18);
            {
                int progressive_speed = app->inbe.progressive_speed;
                int toggle_w = flint_px(56);
                int toggle_h = flint_px(30);
                DrawText(locale_get("progressive_speed_label"), content_x, y,
                         flint_ui_font(), c_text);
                y += flint_px(26);
                if(ui_draw_toggle_switch(app, content_x, y, toggle_w, toggle_h,
                                         &progressive_speed, locale_get("toggle_off"),
                                         locale_get("toggle_on"))) {
                    app->inbe.progressive_speed = progressive_speed;
                    app->settings_preview.progressive_speed = progressive_speed;
                    app->settings_dirty = 1;
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

            if(ui_draw_slider(app, 10, content_x, y, content_w, locale_get("speed_label"), SETTINGS_SPEED_MIN,
                           SETTINGS_SPEED_MAX, &speed, "")) {
                apply_settings(&app->inbe, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                apply_settings(&app->settings_preview, speed, app->inbe.max_rounds,
                               int_from_count(app->inbe.maxbreaths), app->inbe.pause_seconds);
                app->settings_preview.progressive_speed = 0;
                app->settings_dirty = 1;
            }
        } else if(step == 4) {
            if(app->tutorial_layouts_initialized && app->tutorial_layouts[4] != NULL) {
                flint_text_layout_draw(app->tutorial_layouts[4], content_x, &y, body_font, c_text);
            }
        } else {
            int img_h = flint_px(234);
            ui_draw_tutorial_image(app->begin_image, "begin.jpg", content_x, y, content_w, img_h);
            y += img_h + flint_px(22);
            if(app->tutorial_layouts_initialized && app->tutorial_layouts[5] != NULL) {
                flint_text_layout_draw(app->tutorial_layouts[5], content_x, &y, body_font, c_text);
            }
        }
    EndScissorMode();

    /* Draw scrollbar if content overflows */
    if(max_scroll > 0) {
        int scrollbar_x = content_x + content_w + flint_px(4);  /* Reduced spacing */
        int scrollbar_viewport = available_content_space;  /* Scrollable area only */
        ui_draw_scrollbar(app, scrollbar_x, title_h + flint_px(16), scrollbar_viewport, actual_content_h,
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


    DrawText(page_label,
             view_width / 2 - MeasureText(page_label, page_font) / 2,
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
