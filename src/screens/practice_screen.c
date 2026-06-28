#include "practice_screen.h"
#include "app.h"
#include "screens/manual_screen.h"
#include "practices/practice_registry.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_ui.h"

extern int view_width;
extern int view_height;

enum {
    PRACTICE_GUIDE_STEPS = 3
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

void
practice_screen_open_tab(InbeApp *app, int tab)
{
    if(app == NULL || app->modal.active)
        return;

    if(tab == PRACTICE_TAB_PLAY) {
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        app->practice_tab = PRACTICE_TAB_PLAY;
        if(app->modal.type == UIModalPracticeManual ||
           app->modal.type == UIModalPracticeConfig)
            app_close_modal(app);
        app_switch_screen(app, InbeScreenStart);
    } else if(tab == PRACTICE_TAB_MANUAL) {
        if(practice_get(app->exercise_type)->draw_manual == NULL)
            return;
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        app->practice_tab = PRACTICE_TAB_MANUAL;
        app->tutorial_step = 0;
        app->manual_scroll = 0;
        app_open_modal(app, UIModalPracticeManual);
        app_switch_screen(app, InbeScreenStart);
    } else if(tab == PRACTICE_TAB_CONFIG) {
        if(practice_get(app->exercise_type)->draw_config == NULL)
            return;
        if(app->practice_tab != PRACTICE_TAB_CONFIG) {
            reset_settings_preview(app);
            app->settings_scroll = 0;
            app->practice_config_tab = 0;
        }
        app->practice_tab = PRACTICE_TAB_CONFIG;
        app_open_modal(app, UIModalPracticeConfig);
        app_switch_screen(app, InbeScreenStart);
    }
}

static int
practice_screen_draw_desktop_tab_bar(InbeApp *app, int y)
{
    FlintUITab tabs[EXERCISE_COUNT];
    int tab_count = 0;

    if(app == NULL)
        return -1;

    // Create tab for each exercise type
    for(int i = 0; i < EXERCISE_COUNT; i++) {
        tabs[tab_count++] = (FlintUITab){
            .label = practice_activity_label(i),
            .icon = (Texture2D){0},
            .icon_size = 0,
            .disabled = app->modal.active
        };
    }

    return ui_draw_tab_bar((FlintUITabBar){
        .bounds = {0, (float)y, (float)view_width, (float)ui_tab_bar_height()},
        .tabs = tabs,
        .count = tab_count,
        .selected_index = app->exercise_type,
        .min_tab_width = flint_px(120),
        .max_tab_width = flint_px(180)
    });
}

static int
practice_screen_selector_height(InbeApp *app)
{
    (void)app;
    return ui_tab_bar_height();
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

    // Desktop mode: use tab bar instead of dropdown
    if(app_should_use_tab_bar(app)) {
        int clicked_exercise = practice_screen_draw_desktop_tab_bar(app, 0);
        if(clicked_exercise >= 0 && clicked_exercise != app->exercise_type) {
            if(app->practice_tab == PRACTICE_TAB_CONFIG)
                app_leave_practice_config(app);
            app->exercise_type = clicked_exercise;
            app->manual_scroll = 0;
            app->settings_scroll = 0;
            app->tutorial_step = 0;
            app->practice_config_tab = 0;
            save_settings(app);
        }
    } else {
        // Mobile mode: keep existing dropdown
        (void)ui_draw_toolbar_header((FlintUIToolbarHeader){
            .leading_icon = (Texture2D){0},
            .toolbar = (FlintUIToolbar){
                .id = 300,
                .height = ui_tab_bar_height(),
                .options = app->modal.active ? NULL : exercise_options,
                .option_count = app->modal.active ? 0 : activity_count,
                .selected_index = &activity_index,
                .dropdown_min_width = flint_px(160),
                .dropdown_height = flint_px(36),
                .side_padding = -1
            }
        });
    }

    if(!app->modal.active)
        app->practice_tab = PRACTICE_TAB_PLAY;
}

void
practice_screen_draw_floating_actions(InbeApp *app)
{
    const PracticeDefinition *practice;
    int icon_size = flint_px(22);
    int icon_padding = flint_px(9);
    int button_w = icon_size + icon_padding * 2;
    int gap = flint_px(10);
    int margin = flint_px(14);
    int y = app_content_top_reserved(app) + flint_px(10);
    int x = view_width - margin;

    if(app == NULL || app->modal.active || app->inbe.screen != InbeScreenStart)
        return;
    practice = practice_get(app->exercise_type);
    if(practice->draw_config != NULL) {
        int hover = 0;
        x -= button_w;
        if(ui_draw_icon_btn_padded(x, y, icon_size, icon_padding,
                                   app->icons[UI_ICON_TYPE_WRENCH], &hover))
            practice_screen_open_tab(app, PRACTICE_TAB_CONFIG);
        x -= gap;
    }
    if(practice->draw_manual != NULL) {
        int hover = 0;
        x -= button_w;
        if(ui_draw_icon_btn_padded(x, y, icon_size, icon_padding,
                                   app->icons[UI_ICON_TYPE_MANUAL], &hover))
            practice_screen_open_tab(app, PRACTICE_TAB_MANUAL);
    }
}

void
practice_screen_draw_modal(InbeApp *app)
{
    if(app == NULL || !app->modal.active)
        return;
    if(app->modal.type != UIModalPracticeManual &&
       app->modal.type != UIModalPracticeConfig &&
       app->modal.type != UIModalEditProgressiveStartSpeed)
        return;

    DrawRectangle(0, 0, view_width, view_height, flint_theme_get_bg());
    if(app->modal.type == UIModalEditProgressiveStartSpeed) {
        const PracticeDefinition *practice = practice_get(app->exercise_type);
        if(practice->draw_config != NULL)
            practice->draw_config(app);
        return;
    }

    if(app->modal.type == UIModalPracticeManual) {
        app->practice_tab = PRACTICE_TAB_MANUAL;
        manual_screen_draw(app);
    } else {
        const PracticeDefinition *practice = practice_get(app->exercise_type);
        app->practice_tab = PRACTICE_TAB_CONFIG;
        if(practice->draw_config != NULL)
            practice->draw_config(app);
    }
}

static Rectangle
practice_screen_dropdown_anchor(void)
{
    int side_padding = 0;
    int dropdown_h = flint_px(36);
    int dropdown_w = view_width - side_padding * 2;

    if(dropdown_w < flint_px(160))
        dropdown_w = view_width - side_padding * 2;
    if(dropdown_w < 1)
        dropdown_w = 1;

    return (Rectangle){
        (float)side_padding,
        (float)((ui_tab_bar_height() - dropdown_h) / 2),
        (float)dropdown_w,
        (float)dropdown_h
    };
}

static Rectangle
practice_screen_floating_action_anchor(InbeApp *app, int tab)
{
    int icon_size = flint_px(22);
    int icon_padding = flint_px(9);
    int size = icon_size + icon_padding * 2;
    int gap = flint_px(10);
    int margin = flint_px(14);
    int x = view_width - margin;
    int y = app_content_top_reserved(app) + flint_px(10);
    const PracticeDefinition *practice;

    if(app == NULL)
        return (Rectangle){0};
    practice = practice_get(app->exercise_type);
    if(practice->draw_config != NULL) {
        x -= size;
        if(tab == PRACTICE_TAB_CONFIG)
            return (Rectangle){(float)x, (float)y, (float)size, (float)size};
        x -= gap;
    }
    if(practice->draw_manual != NULL) {
        x -= size;
        if(tab == PRACTICE_TAB_MANUAL)
            return (Rectangle){(float)x, (float)y, (float)size, (float)size};
    }

    return (Rectangle){(float)(view_width - margin - size), (float)y,
                       (float)size, (float)size};
}

static Rectangle
practice_screen_exercise_tabs_anchor(InbeApp *app)
{
    if(app == NULL)
        return (Rectangle){0};

    // Desktop mode: highlight all exercise tabs (top tab bar)
    if(app_should_use_tab_bar(app)) {
        return (Rectangle){
            0,
            0,
            (float)view_width,
            (float)ui_tab_bar_height()
        };
    }

    // Mobile mode: highlight the dropdown
    return practice_screen_dropdown_anchor();
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
           app->exercise_type != EXERCISE_SUN_SALUTATION &&
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
        practice_screen_exercise_tabs_anchor(app),
        locale_get("practice_guide_dropdown")
    };
    steps[1] = (FlintUIGuideStep){
        practice_screen_floating_action_anchor(app, PRACTICE_TAB_MANUAL),
        locale_get("practice_guide_manual")
    };
    steps[2] = (FlintUIGuideStep){
        practice_screen_floating_action_anchor(app, PRACTICE_TAB_CONFIG),
        locale_get("practice_guide_config")
    };

    result = flint_ui_draw_guide_overlay((FlintUIGuideOverlay){
        .steps = steps,
        .count = PRACTICE_GUIDE_STEPS,
        .step = &app->tutorial_step,
        .view_width = view_width,
        .view_height = view_height,
        .reserved_top = practice_screen_selector_height(app),
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
