#include "practice_screen.h"
#include "app.h"
#include "practices/practice_registry.h"
#include "flint_locale.h"
#include "flint_ui.h"
#include "theme.h"
#include <stdio.h>

extern int view_width;
extern int view_height;

enum {
    PRACTICE_GUIDE_STEPS = 4
};

typedef struct PracticeGuideStep {
    Rectangle anchor;
    const char *text;
} PracticeGuideStep;

int
practice_activity_count_for_tab(int tab)
{
    (void)tab;
    return practice_count();
}

int
practice_activity_for_tab(int tab, int index)
{
    (void)tab;
    if(index < 0)
        return EXERCISE_WIM_HOF;
    if(index >= practice_count())
        return practice_count() - 1;
    return index;
}

const char *
practice_activity_label(int exercise)
{
    return practice_label(exercise);
}

int
practice_activity_index_for_tab(int tab, int exercise)
{
    (void)tab;
    return practice_clamp_id(exercise);
}

void
practice_clamp_activity_to_tab(InbeApp *app)
{
    if(app == NULL)
        return;
    app->exercise_type = practice_clamp_id(app->exercise_type);
}

static int
practice_screen_draw_tab_bar(InbeApp *app, int y)
{
    FlintUISubtab tabs[PRACTICE_TAB_COUNT];
    int tab_h = flint_px(PRACTICE_CATEGORY_TAB_H);

    if(app == NULL)
        return -1;

    tabs[PRACTICE_TAB_PLAY] = (FlintUISubtab){
        .icon = app->icons[UI_ICON_TYPE_SUN],
        .icon_size = flint_px(20),
        .disabled = app->modal.active
    };
    tabs[PRACTICE_TAB_MANUAL] = (FlintUISubtab){
        .icon = app->icons[UI_ICON_TYPE_MANUAL],
        .icon_size = flint_px(20),
        .disabled = app->modal.active
    };
    tabs[PRACTICE_TAB_CONFIG] = (FlintUISubtab){
        .icon = app->icons[UI_ICON_TYPE_WRENCH],
        .icon_size = flint_px(20),
        .disabled = app->modal.active
    };

    if(app->practice_tab < 0 || app->practice_tab >= PRACTICE_TAB_COUNT)
        app->practice_tab = PRACTICE_TAB_PLAY;

    return ui_draw_subtab_bar((FlintUISubtabBar){
        .bounds = {0, (float)y, (float)view_width, (float)tab_h},
        .tabs = tabs,
        .count = PRACTICE_TAB_COUNT,
        .selected_index = app->practice_tab,
        .font = flint_ui_font()
    });
}

void
practice_screen_open_tab(InbeApp *app, int tab)
{
    if(app == NULL || app->modal.active)
        return;

    if(tab == PRACTICE_TAB_PLAY) {
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        app->practice_tab = PRACTICE_TAB_PLAY;
        app_switch_screen(app, InbeScreenStart);
    } else if(tab == PRACTICE_TAB_MANUAL) {
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        app->practice_tab = PRACTICE_TAB_MANUAL;
        app->tutorial_step = 0;
        app->manual_scroll = 0;
        app_switch_screen(app, InbeScreenStart);
    } else if(tab == PRACTICE_TAB_CONFIG) {
        if(app->practice_tab != PRACTICE_TAB_CONFIG) {
            reset_settings_preview(app);
            app->settings_scroll = 0;
            app->practice_config_tab = 0;
        }
        app->practice_tab = PRACTICE_TAB_CONFIG;
        app_switch_screen(app, InbeScreenStart);
    }
}

void
practice_screen_draw_top_bar(InbeApp *app, int draw_menu)
{
    const char *exercise_options[EXERCISE_COUNT];
    int exercise_values[EXERCISE_COUNT];
    int activity_count;
    int activity_index;

    if(app == NULL)
        return;

    activity_count = EXERCISE_COUNT;
    for(int i = 0; i < activity_count; i++) {
        exercise_values[i] = i;
        exercise_options[i] = practice_activity_label(exercise_values[i]);
    }
    activity_index = clampi(app->exercise_type, 0, EXERCISE_COUNT - 1);

    if(draw_menu) {
        FlintUIToolbarResult menu_result = ui_draw_toolbar_header((FlintUIToolbarHeader){
            .toolbar = (FlintUIToolbar){
                .id = 300,
                .draw_menu = 1,
                .options = exercise_options,
                .option_count = activity_count,
                .selected_index = &activity_index
            }
        }).toolbar;
        if(!app->modal.active && menu_result.selected_menu_item >= 0) {
            if(app->practice_tab == PRACTICE_TAB_CONFIG)
                app_leave_practice_config(app);
            app->exercise_type = exercise_values[activity_index];
            app->manual_scroll = 0;
            app->settings_scroll = 0;
            app->tutorial_step = 0;
            app->practice_config_tab = 0;
            save_settings(app);
        }
        return;
    }

    (void)ui_draw_toolbar_header((FlintUIToolbarHeader){
        .leading_icon = (Texture2D){0},
        .toolbar = (FlintUIToolbar){
            .id = 300,
            .height = app_toolbar_height(),
            .options = app->modal.active ? NULL : exercise_options,
            .option_count = app->modal.active ? 0 : activity_count,
            .selected_index = &activity_index,
            .dropdown_min_width = flint_px(160),
            .dropdown_max_width = flint_px(230),
            .dropdown_height = flint_px(36),
            .side_padding = flint_px(12)
        }
    });

    {
        int clicked = practice_screen_draw_tab_bar(app, app_toolbar_height());
        if(clicked >= 0 && clicked != app->practice_tab)
            practice_screen_open_tab(app, clicked);
    }
}

static Rectangle
practice_screen_dropdown_anchor(void)
{
    int side_padding = flint_px(12);
    int dropdown_h = flint_px(36);
    int dropdown_w = view_width - side_padding * 2;

    if(dropdown_w > flint_px(230))
        dropdown_w = flint_px(230);
    if(dropdown_w < flint_px(160))
        dropdown_w = view_width - side_padding * 2;
    if(dropdown_w < 1)
        dropdown_w = 1;

    return (Rectangle){
        (float)side_padding,
        (float)((app_toolbar_height() - dropdown_h) / 2),
        (float)dropdown_w,
        (float)dropdown_h
    };
}

static Rectangle
practice_screen_tab_anchor(int tab)
{
    int tab_h = flint_px(PRACTICE_CATEGORY_TAB_H);
    int tab_w = view_width / PRACTICE_TAB_COUNT;
    int x = tab * tab_w;
    int w = tab == PRACTICE_TAB_COUNT - 1 ? view_width - x : tab_w;

    return (Rectangle){
        (float)x,
        (float)app_toolbar_height(),
        (float)w,
        (float)tab_h
    };
}

static void
practice_screen_finish_first_run_guide(InbeApp *app)
{
    if(app == NULL)
        return;
    app->tutorial_seen = 1;
    app->tutorial_step = 0;
    save_settings(app);
}

void
practice_screen_prepare_first_run_guide(InbeApp *app)
{
    int step;

    if(app == NULL || app->tutorial_seen || app->inbe.screen != InbeScreenStart)
        return;
    if(app->modal.active)
        return;

    step = clampi(app->tutorial_step, 0, PRACTICE_GUIDE_STEPS - 1);
    if(step == 1)
        app->practice_tab = PRACTICE_TAB_MANUAL;
    else
        app->practice_tab = PRACTICE_TAB_PLAY;
}

static void
practice_screen_draw_guide_arrow(Rectangle tip, Rectangle anchor)
{
    int anchor_cx = (int)(anchor.x + anchor.width / 2);
    int anchor_cy = (int)(anchor.y + anchor.height / 2);
    int tip_left = (int)tip.x;
    int tip_right = (int)(tip.x + tip.width);
    int tip_top = (int)tip.y;
    int tip_bottom = (int)(tip.y + tip.height);
    int point_x = anchor_cx;
    int point_y = anchor_cy;
    int base_x = clampi(anchor_cx, tip_left + flint_px(18), tip_right - flint_px(18));
    Color color = theme_get_button();

    if(anchor_cy < tip_top) {
        DrawTriangle((Vector2){(float)base_x, (float)tip_top},
                     (Vector2){(float)(base_x - flint_px(8)), (float)(tip_top - flint_px(12))},
                     (Vector2){(float)(base_x + flint_px(8)), (float)(tip_top - flint_px(12))},
                     color);
        point_y = tip_top - flint_px(12);
    } else if(anchor_cy > tip_bottom) {
        DrawTriangle((Vector2){(float)base_x, (float)tip_bottom},
                     (Vector2){(float)(base_x + flint_px(8)), (float)(tip_bottom + flint_px(12))},
                     (Vector2){(float)(base_x - flint_px(8)), (float)(tip_bottom + flint_px(12))},
                     color);
        point_y = tip_bottom + flint_px(12);
    } else if(anchor_cx < tip_left) {
        DrawTriangle((Vector2){(float)tip_left, (float)anchor_cy},
                     (Vector2){(float)(tip_left - flint_px(12)), (float)(anchor_cy + flint_px(8))},
                     (Vector2){(float)(tip_left - flint_px(12)), (float)(anchor_cy - flint_px(8))},
                     color);
        point_x = tip_left - flint_px(12);
    } else {
        DrawTriangle((Vector2){(float)tip_right, (float)anchor_cy},
                     (Vector2){(float)(tip_right + flint_px(12)), (float)(anchor_cy - flint_px(8))},
                     (Vector2){(float)(tip_right + flint_px(12)), (float)(anchor_cy + flint_px(8))},
                     color);
        point_x = tip_right + flint_px(12);
    }

    DrawLineEx((Vector2){(float)point_x, (float)point_y},
               (Vector2){(float)anchor_cx, (float)anchor_cy},
               (float)flint_px(2), theme_get_text());
}

static Rectangle
practice_screen_guide_tip_bounds(Rectangle anchor, int w, int h)
{
    int margin = flint_px(12);
    int gap = flint_px(20);
    int x = (int)(anchor.x + anchor.width / 2) - w / 2;
    int y;

    if(x < margin)
        x = margin;
    if(x + w > view_width - margin)
        x = view_width - margin - w;
    if(x < margin)
        x = margin;

    if(anchor.y + anchor.height + gap + h < view_height - ui_bottom_nav_height())
        y = (int)(anchor.y + anchor.height + gap);
    else
        y = (int)(anchor.y - gap - h);

    if(y < app_toolbar_height() + flint_px(PRACTICE_CATEGORY_TAB_H))
        y = app_toolbar_height() + flint_px(PRACTICE_CATEGORY_TAB_H) + margin;
    if(y + h > view_height - ui_bottom_nav_height() - margin)
        y = view_height - ui_bottom_nav_height() - margin - h;
    if(y < margin)
        y = margin;

    return (Rectangle){(float)x, (float)y, (float)w, (float)h};
}

void
practice_screen_draw_first_run_guide(InbeApp *app)
{
    PracticeGuideStep steps[PRACTICE_GUIDE_STEPS];
    int step;
    int margin = flint_px(12);
    int tip_w = view_width - margin * 2;
    int pad = flint_px(12);
    int button_size = flint_px(34);
    int font = flint_ui_font();
    int page_font = FLINT_TEXT_12;
    char page_text[16];
    FlintUIParagraph paragraph;
    int paragraph_h;
    int tip_h;
    Rectangle tip;
    int y;
    int finish;

    if(app == NULL || app->tutorial_seen || app->modal.active)
        return;
    if(app->inbe.screen != InbeScreenStart)
        return;

    steps[0] = (PracticeGuideStep){
        practice_screen_dropdown_anchor(),
        locale_get("practice_guide_dropdown")
    };
    steps[1] = (PracticeGuideStep){
        practice_screen_tab_anchor(PRACTICE_TAB_MANUAL),
        locale_get("practice_guide_manual")
    };
    steps[2] = (PracticeGuideStep){
        practice_screen_tab_anchor(PRACTICE_TAB_PLAY),
        locale_get("practice_guide_sun")
    };
    steps[3] = (PracticeGuideStep){
        practice_screen_tab_anchor(PRACTICE_TAB_CONFIG),
        locale_get("practice_guide_config")
    };

    step = clampi(app->tutorial_step, 0, PRACTICE_GUIDE_STEPS - 1);
    app->tutorial_step = step;
    if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) {
        if(step >= PRACTICE_GUIDE_STEPS - 1)
            practice_screen_finish_first_run_guide(app);
        else
            app->tutorial_step = step + 1;
        return;
    }
    if(IsKeyPressed(KEY_LEFT) && step > 0) {
        app->tutorial_step = step - 1;
        return;
    }
    if(IsKeyPressed(KEY_ESCAPE)) {
        practice_screen_finish_first_run_guide(app);
        return;
    }

    if(tip_w > flint_px(300))
        tip_w = flint_px(300);
    paragraph = (FlintUIParagraph){
        .text = steps[step].text,
        .icon = (Texture2D){0},
        .icon_type = UI_ICON_TYPE_NONE,
        .icon_size = 0,
        .width = tip_w - pad * 2,
        .font = font,
        .line_gap = flint_px(6),
        .color = theme_get_text(),
    };
    paragraph_h = flint_ui_paragraph_height(paragraph);
    tip_h = pad + paragraph_h + flint_px(12) + button_size + pad;
    if(tip_h < flint_px(112))
        tip_h = flint_px(112);
    tip = practice_screen_guide_tip_bounds(steps[step].anchor, tip_w, tip_h);

    DrawRectangle(0, 0, view_width, view_height, (Color){0, 0, 0, 86});
    DrawRectangleLinesEx(steps[step].anchor, (float)flint_px(2), theme_get_text());
    DrawRectangleRounded(tip, 0.08f, 8, theme_get_button());
    DrawRectangleRoundedLines(tip, 0.08f, 8,
                              flint_darken(theme_get_button(), 35));
    practice_screen_draw_guide_arrow(tip, steps[step].anchor);

    y = (int)tip.y + pad;
    flint_ui_paragraph_draw(paragraph, (int)tip.x + pad, &y);

    snprintf(page_text, sizeof(page_text), "%d/%d", step + 1, PRACTICE_GUIDE_STEPS);
    flint_text_draw(page_text, (int)tip.x + pad,
                    (int)tip.y + (int)tip.height - pad - button_size +
                        (button_size - page_font) / 2,
                    page_font, theme_get_text());

    finish = step >= PRACTICE_GUIDE_STEPS - 1;
    if(step > 0) {
        if(flint_ui_icon_button((FlintUIIconButton){
               .bounds = {
                   tip.x + tip.width - pad - button_size * 2 - flint_px(8),
                   tip.y + tip.height - pad - button_size,
                   (float)button_size,
                   (float)button_size
               },
               .icon = app->icons[UI_ICON_TYPE_BACKWARD],
               .icon_size = flint_px(19),
               .icon_padding = flint_px(7),
               .background = theme_get_button(),
               .hover_background = theme_get_button_hover(),
               .icon_color = theme_get_text(),
               .border = flint_darken(theme_get_button(), 35)
           })) {
            app->tutorial_step = step - 1;
        }
    }
    if(flint_ui_icon_button((FlintUIIconButton){
           .bounds = {
               tip.x + tip.width - pad - button_size,
               tip.y + tip.height - pad - button_size,
               (float)button_size,
               (float)button_size
           },
           .icon = app->icons[finish ? UI_ICON_TYPE_CHECK : UI_ICON_TYPE_FORWARD],
           .icon_size = flint_px(19),
           .icon_padding = flint_px(7),
           .background = theme_get_button(),
           .hover_background = theme_get_button_hover(),
           .icon_color = theme_get_text(),
           .border = flint_darken(theme_get_button(), 35)
       })) {
        if(finish)
            practice_screen_finish_first_run_guide(app);
        else
            app->tutorial_step = step + 1;
    }
}
