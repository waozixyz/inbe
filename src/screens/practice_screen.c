#include "practice_screen.h"
#include "app.h"
#include "practices/practice_registry.h"
#include "flint_locale.h"
#include "flint_ui.h"

extern int view_width;
extern int view_height;

enum {
    PRACTICE_GUIDE_STEPS = 4
};

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
        .selected_index = app->practice_tab
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

int
practice_screen_first_run_guide_active(const InbeApp *app)
{
    return app != NULL && !app->tutorial_seen && !app->modal.active &&
           app->inbe.screen == InbeScreenStart;
}

void
practice_screen_prepare_first_run_guide(InbeApp *app)
{
    if(!practice_screen_first_run_guide_active(app))
        return;

    app->practice_tab = PRACTICE_TAB_PLAY;
}

void
practice_screen_draw_first_run_guide(InbeApp *app)
{
    FlintUIGuideStep steps[PRACTICE_GUIDE_STEPS];
    FlintUIGuideResult result;

    if(!practice_screen_first_run_guide_active(app))
        return;

    steps[0] = (FlintUIGuideStep){
        practice_screen_dropdown_anchor(),
        locale_get("practice_guide_dropdown")
    };
    steps[1] = (FlintUIGuideStep){
        practice_screen_tab_anchor(PRACTICE_TAB_MANUAL),
        locale_get("practice_guide_manual")
    };
    steps[2] = (FlintUIGuideStep){
        practice_screen_tab_anchor(PRACTICE_TAB_PLAY),
        locale_get("practice_guide_sun")
    };
    steps[3] = (FlintUIGuideStep){
        practice_screen_tab_anchor(PRACTICE_TAB_CONFIG),
        locale_get("practice_guide_config")
    };

    result = flint_ui_draw_guide_overlay((FlintUIGuideOverlay){
        .steps = steps,
        .count = PRACTICE_GUIDE_STEPS,
        .step = &app->tutorial_step,
        .view_width = view_width,
        .view_height = view_height,
        .reserved_top = app_toolbar_height() + flint_px(PRACTICE_CATEGORY_TAB_H),
        .reserved_bottom = ui_bottom_nav_height(),
        .max_width = flint_px(300),
        .close_icon = app->icons[UI_ICON_TYPE_X],
        .back_icon = app->icons[UI_ICON_TYPE_BACKWARD],
        .next_icon = app->icons[UI_ICON_TYPE_FORWARD],
        .done_icon = app->icons[UI_ICON_TYPE_CHECK]
    });
    if(result.closed || result.finished)
        practice_screen_finish_first_run_guide(app);
}
