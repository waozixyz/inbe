#include "manual_tab.h"
#include "app.h"
#include "ui.h"
#include "theme_meta.h"
#include "raylib.h"
#include <stdio.h>

#define CONTENT_MAX_W 600
#define CONTENT_SIDE_PAD 20

extern int view_width;
extern int view_height;

/* Theme colors - set by ui_set_colors */
extern Color c_text, c_bg, c_circle, c_button, c_button_hover, c_icon;


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
    int body_font = ui_clamp_px(13, 11, 14);
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

    ui_centered_column(CONTENT_MAX_W, CONTENT_SIDE_PAD, &content_x, &content_w);

    close_clicked = ui_draw_screen_header(app, title, 1);
    if(close_clicked)
        manual_tab_close_tutorial(app, 1);

    BeginScissorMode((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + title_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)(viewport_h * app->camera.zoom));
        int y = title_h + ui_px(16) - app->manual_scroll;
        if(app->tutorial_step == 0) {
            const char *lines[] = {
                "This breathing practice is based on",
                "the Wim Hof Method.",
                "",
                "It can be powerful. Use it with care.",
                "",
                "Practice sitting or lying down.",
                "Never use it while driving,",
                "standing, or in water."
            };
            int img_h = ui_px(240);
            ui_draw_tutorial_image(app->angel_image, "angel.jpg", content_x, y, content_w, img_h);
            y += img_h + ui_px(22);
            ui_draw_text_lines(lines, 8, content_x, &y, body_font, ui_px(20));
        } else if(app->tutorial_step == 1) {
            const char *lines[] = {
                "Simply follow 4 steps:",
                "",
                "1. Breathe rhythmically.",
                "2. Exhale and hold.",
                "3. Inhale deeply and hold.",
                "4. Exhale and repeat."
            };
            ui_draw_text_lines(lines, 6, content_x, &y, body_font, ui_px(19));
            y += ui_px(12);

            const char *before_gear = "Use the gear icon";
            DrawText(before_gear, content_x, y, body_font, c_text);

            if(app->gear_icon.id != 0) {
                int icon_size = ui_px(14);
                int gear_y = y - icon_size / 2 + ui_px(5);
                int gear_x = content_x + MeasureText(before_gear, body_font) + ui_px(4);
                Rectangle src = {0, 0, app->gear_icon.width, app->gear_icon.height};
                Rectangle dst = {gear_x, gear_y, icon_size, icon_size};
                DrawTexturePro(app->gear_icon, src, dst, (Vector2){0}, 0, c_icon);
            }

            const char *after_gear = " to adjust rounds,";
            DrawText(after_gear, content_x + MeasureText(before_gear, body_font) + ui_px(4) + ui_px(14) + ui_px(4), y, body_font, c_text);
            y += ui_px(19);

            const char *settings_lines2[] = {
                "breaths, speed, and pauses."
            };
            ui_draw_text_lines(settings_lines2, 1, content_x, &y, body_font, ui_px(19));
        } else if(app->tutorial_step == 2) {
            int speed = app->inbe.speed_level;
            const char *lines[] = {
                "Fill your lungs fully, then",
                "let the breath flow out.",
                "",
                "Use this slider to set the",
                "pace of the breathing circle."
            };
            ui_draw_text_lines(lines, 5, content_x, &y, body_font, ui_px(19));
            y += ui_px(8);

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
            const char *lines[] = {
                "After the breathing round,",
                "exhale normally and hold.",
                "",
                "Release when your body asks",
                "for air. Do not force it."
            };
            ui_draw_text_lines(lines, 5, content_x, &y, body_font, ui_px(20));
        } else {
            const char *lines[] = {
                "Inhale fully and hold for",
                "about 15 seconds.",
                "",
                "Then exhale and begin the",
                "next round. Over time, each",
                "round may feel deeper."
            };
            int img_h = ui_px(250);
            ui_draw_tutorial_image(app->begin_image, "begin.jpg", content_x, y, content_w, img_h);
            y += img_h + ui_px(22);
            ui_draw_text_lines(lines, 6, content_x, &y, body_font, ui_px(20));
        }
    EndScissorMode();

    ui_draw_scrollbar(app, &app->manual_scroll, ui_px(content_h), viewport_h,
                   &app->manual_drag_scrollbar, &app->manual_drag_content, &app->manual_drag_content_y);
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

