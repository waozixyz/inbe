#include "practice_screen.h"
#include "app.h"
#include "screens/manual_screen.h"
#include "practices/meditation/meditation_music.h"
#include "practices/practice_registry.h"
#include "sync_account.h"
#include "flint_locale.h"
#include "flint_theme.h"
#include "flint_ui.h"

extern int view_width;
extern int view_height;

enum {
    PRACTICE_GUIDE_STEPS = 4
};

int
practice_visible_mask_all(void)
{
    return (1 << EXERCISE_COUNT) - 1;
}

static int
practice_sanitize_visible_mask(int mask)
{
    mask &= practice_visible_mask_all();
    return mask != 0 ? mask : practice_visible_mask_all();
}

int
practice_is_visible(const InbeApp *app, int exercise)
{
    int mask;

    if(exercise < 0 || exercise >= EXERCISE_COUNT)
        return 0;
    mask = practice_sanitize_visible_mask(app != NULL ? app->practice_visible_mask : 0);
    return (mask & (1 << exercise)) != 0;
}

static int
practice_first_visible(const InbeApp *app)
{
    for(int i = 0; i < EXERCISE_COUNT; i++) {
        if(practice_is_visible(app, i))
            return i;
    }
    return EXERCISE_WIM_HOF;
}

void
practice_set_visible(InbeApp *app, int exercise, int visible)
{
    int mask;

    if(app == NULL || exercise < 0 || exercise >= EXERCISE_COUNT)
        return;
    mask = practice_sanitize_visible_mask(app->practice_visible_mask);
    if(visible)
        mask |= 1 << exercise;
    else if((mask & ~(1 << exercise)) != 0)
        mask &= ~(1 << exercise);
    app->practice_visible_mask = practice_sanitize_visible_mask(mask);
    practice_clamp_activity_to_tab(app);
    save_settings(app);
}

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

static Color
practice_card_color_top(int exercise)
{
    switch(practice_clamp_id(exercise)) {
    case EXERCISE_MEDITATION:
        return (Color){78, 126, 164, 255};
    case EXERCISE_SUN_SALUTATION:
        return (Color){213, 134, 72, 255};
    case EXERCISE_WIM_HOF:
    default:
        return (Color){72, 142, 128, 255};
    }
}

static Color
practice_card_color_bottom(int exercise)
{
    switch(practice_clamp_id(exercise)) {
    case EXERCISE_MEDITATION:
        return (Color){52, 74, 122, 255};
    case EXERCISE_SUN_SALUTATION:
        return (Color){177, 87, 85, 255};
    case EXERCISE_WIM_HOF:
    default:
        return (Color){43, 93, 118, 255};
    }
}

static void
practice_draw_card_banner(Texture2D texture, Rectangle dest)
{
    float src_w;
    float src_h;
    float scale;
    Rectangle src;

    if(texture.id == 0 || texture.width <= 0 || texture.height <= 0 ||
       dest.width <= 0 || dest.height <= 0)
        return;

    src_w = (float)texture.width;
    src_h = (float)texture.height;
    scale = dest.width / src_w;
    if(src_h * scale < dest.height)
        scale = dest.height / src_h;
    src.width = dest.width / scale;
    src.height = dest.height / scale;
    src.x = (src_w - src.width) / 2.0f;
    src.y = (src_h - src.height) / 2.0f;

    DrawTexturePro(texture, src, dest, (Vector2){0}, 0, WHITE);
}

static Texture2D
practice_card_banner(const InbeApp *app, int exercise)
{
    if(app == NULL)
        return (Texture2D){0};

    switch(practice_clamp_id(exercise)) {
    case EXERCISE_WIM_HOF:
        return app->whm.banner;
    case EXERCISE_SUN_SALUTATION:
        return app->sun_salutation.banner;
    case EXERCISE_MEDITATION:
        return app->meditation.banner;
    default:
        return (Texture2D){0};
    }
}

static void
practice_draw_card_label(Rectangle card, const char *title, const char *subtitle)
{
    int title_font = flint_ui_font();
    int subtitle_font = flint_ui_font_small();
    int pad_x = flint_px(12);
    int pad_y = flint_px(6);
    int gap = flint_px(4);
    int title_w = flint_text_measure(title != NULL ? title : "", title_font);
    int subtitle_w = subtitle != NULL ? flint_text_measure(subtitle, subtitle_font) : 0;
    int content_w = title_w > subtitle_w ? title_w : subtitle_w;
    int max_label_w = ((int)card.width * 58) / 100;
    int label_w = content_w + pad_x * 2;
    int label_h = flint_px(34) + pad_y * 2;
    int inner_w;
    int label_x;
    int label_y;
    Rectangle label;
    Rectangle title_rect;

    if(subtitle != NULL)
        label_h += flint_px(18) + gap;
    if(max_label_w < flint_px(136))
        max_label_w = flint_px(136);
    if(label_w > max_label_w)
        label_w = max_label_w;
    if(label_w < flint_px(104))
        label_w = flint_px(104);
    inner_w = label_w - pad_x * 2;
    if(inner_w < flint_px(40))
        inner_w = flint_px(40);

    label_x = (int)card.x + ((int)card.width - label_w) / 2;
    label_y = (int)card.y + ((int)card.height - label_h) / 2;
    label = (Rectangle){(float)label_x, (float)label_y,
                        (float)label_w, (float)label_h};

    DrawRectangleRec(label, Fade(BLACK, 0.30f));
    DrawRectangleLinesEx(label, flint_px(1), Fade(WHITE, 0.42f));
    title_rect = (Rectangle){(float)(label_x + pad_x),
                             (float)(label_y + pad_y),
                             (float)inner_w,
                             (float)flint_px(34)};
    ui_draw_fitted_text_in_rect(title != NULL ? title : "", title_rect,
                                title_font, FLINT_TEXT_8, WHITE);
    if(subtitle != NULL) {
        Rectangle subtitle_rect = {
            (float)(label_x + pad_x),
            (float)(label_y + pad_y + flint_px(39) + gap),
            (float)inner_w,
            (float)flint_px(18)
        };
        ui_draw_fitted_text_in_rect(subtitle, subtitle_rect,
                                    subtitle_font, FLINT_TEXT_8,
                                    Fade(WHITE, 0.88f));
    }
}

static int
practice_next_visible(InbeApp *app, int dir)
{
    int current;

    if(app == NULL)
        return EXERCISE_WIM_HOF;
    current = practice_clamp_id(app->exercise_type);
    for(int step = 1; step <= EXERCISE_COUNT; step++) {
        int id = (current + dir * step + EXERCISE_COUNT * 2) % EXERCISE_COUNT;
        if(practice_is_visible(app, id))
            return id;
    }
    return current;
}

static void
practice_select_home_card(InbeApp *app, int exercise)
{
    if(app == NULL)
        return;
    if(app->practice_tab == PRACTICE_TAB_CONFIG)
        app_leave_practice_config(app);
    app->exercise_type = practice_clamp_id(exercise);
    app->practice_tab = PRACTICE_TAB_PLAY;
    app->manual_scroll = 0;
    app->settings_scroll = 0;
    app->tutorial_step = 0;
    app->practice_config_tab = 0;
    save_settings(app);
}

static int
practice_music_content_height(int content_w, void *user_data)
{
    InbeApp *app = user_data;

    return meditation_music_measure_settings(app, content_w, 1, 1) + flint_px(24);
}

static void
practice_screen_draw_music_modal(InbeApp *app)
{
    int title_h = flint_ui_title_bar_height();
    int scroll_y = title_h + flint_px(16);
    int scroll_h = view_height - scroll_y - flint_px(8);

    if(flint_ui_return_title_bar(app->icons[UI_ICON_TYPE_RETURN], locale_get("practice_music_title"), title_h))
        app_close_modal(app);

    if(scroll_h < 0)
        scroll_h = 0;
    {
        FlintUIScrollPage page = ui_scroll_page_begin((FlintUIScrollPageSpec){
            .y = scroll_y,
            .height = scroll_h,
            .max_content_width = flint_px(CONTENT_MAX_W),
            .min_content_width = flint_px(320),
            .scroll_offset = &app->settings_scroll,
            .content_height = practice_music_content_height,
            .user_data = app
        });
        int y = page.content_y;

        meditation_music_draw_settings(app, page.content_x, page.content_w,
                                       &y, 1, 1);
        ui_scroll_page_end(page);
    }
    meditation_music_draw_dropdown_menu(app);
}

void
practice_screen_draw_home(InbeApp *app)
{
    const PracticeDefinition *practice;
    int top = app_content_top_reserved(app) + flint_px(14);
    int bottom_reserved = app_content_bottom_reserved(app);
    int content_w = view_width - flint_px(32);
    int max_w = flint_px(480);
    int x;
    int y;
    int card_w;
    int card_h;
    int gap = flint_px(12);
    int btn_h = flint_px(42);
    int hover = 0;
    int arrow = flint_px(44);
    Rectangle card;
    Texture2D banner;

    if(app == NULL)
        return;

    practice_clamp_activity_to_tab(app);
    flint_ui_title_bar(locale_get("practice_title"), app_content_top_reserved(app));
    practice = practice_get(app->exercise_type);
    if(content_w > max_w)
        content_w = max_w;
    if(content_w < flint_px(260))
        content_w = view_width - flint_px(20);
    x = (view_width - content_w) / 2;
    y = top;

    if(ui_draw_generic_button(x, y, content_w, btn_h, locale_get("practice_music_title"),
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
        app->settings_scroll = 0;
        app_open_modal(app, UIModalPracticeMusic);
    }
    y += btn_h + flint_px(18);

    card_w = content_w;
    card_h = view_height - y - bottom_reserved - flint_px(176);
    if(card_h < flint_px(180))
        card_h = flint_px(180);
    if(card_h > flint_px(300))
        card_h = flint_px(300);
    card = (Rectangle){(float)x, (float)y, (float)card_w, (float)card_h};
    banner = practice_card_banner(app, app->exercise_type);
    if(banner.id != 0) {
        practice_draw_card_banner(banner, card);
    } else {
        DrawRectangleGradientV(x, y, card_w, card_h,
                               practice_card_color_top(app->exercise_type),
                               practice_card_color_bottom(app->exercise_type));
    }
    DrawRectangleLinesEx(card, flint_px(2), flint_lighten(flint_theme_get_bg(), 55));

    practice_draw_card_label(
        card,
        practice->label_key != NULL ? practice_label(app->exercise_type) : "",
        app->exercise_type == EXERCISE_SUN_SALUTATION
            ? locale_get("sun_salutation_work_in_progress")
            : NULL);

    if(practice_count() > 1) {
        int arrow_y = y + (card_h - arrow) / 2;
        int left_x = x + flint_px(10);
        int right_x = x + card_w - arrow - flint_px(10);
        if(ui_draw_overlay_button((FlintUIOverlayButton){
            .bounds = {(float)left_x, (float)arrow_y, (float)arrow, (float)arrow},
            .label = "<",
            .background = Fade(BLACK, 0.22f),
            .hover_background = Fade(WHITE, 0.32f),
            .border = Fade(WHITE, 0.42f),
            .hover_border = Fade(WHITE, 0.74f),
            .text = WHITE
        }))
            practice_select_home_card(app, practice_next_visible(app, -1));
        if(ui_draw_overlay_button((FlintUIOverlayButton){
            .bounds = {(float)right_x, (float)arrow_y, (float)arrow, (float)arrow},
            .label = ">",
            .background = Fade(BLACK, 0.22f),
            .hover_background = Fade(WHITE, 0.32f),
            .border = Fade(WHITE, 0.42f),
            .hover_border = Fade(WHITE, 0.74f),
            .text = WHITE
        }))
            practice_select_home_card(app, practice_next_visible(app, 1));
    }

    y += card_h + flint_px(18);

    if(ui_draw_generic_button(x, y, content_w, btn_h, locale_get("practice_start_button"),
                              UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
        if(practice->start != NULL)
            practice->start(app);
    }
    y += btn_h + gap;
    if(practice->draw_manual != NULL &&
       ui_draw_generic_button(x, y, content_w, btn_h, locale_get("practice_manual_button"),
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover))
        practice_screen_open_tab(app, PRACTICE_TAB_MANUAL);
    y += btn_h + gap;
    if(practice->draw_config != NULL &&
       ui_draw_generic_button(x, y, content_w, btn_h, locale_get("practice_configure_button"),
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover))
        practice_screen_open_tab(app, PRACTICE_TAB_CONFIG);
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
    app->practice_visible_mask = practice_sanitize_visible_mask(app->practice_visible_mask);
    app->exercise_type = practice_clamp_id(app->exercise_type);
    if(!practice_is_visible(app, app->exercise_type))
        app->exercise_type = practice_first_visible(app);
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
    int values[EXERCISE_COUNT];
    int tab_count = 0;
    int selected_index = 0;
    int clicked;

    if(app == NULL)
        return -1;

    for(int i = 0; i < EXERCISE_COUNT; i++) {
        if(!practice_is_visible(app, i))
            continue;
        if(i == app->exercise_type)
            selected_index = tab_count;
        values[tab_count] = i;
        tabs[tab_count++] = (FlintUITab){
            .label = practice_activity_label(i),
            .icon = (Texture2D){0},
            .icon_size = 0,
            .disabled = app->modal.active
        };
    }
    if(tab_count <= 0)
        return -1;

    clicked = ui_draw_tab_bar((FlintUITabBar){
        .bounds = {0, (float)y, (float)view_width, (float)ui_tab_bar_height()},
        .tabs = tabs,
        .count = tab_count,
        .selected_index = selected_index,
        .min_tab_width = flint_px(120),
        .max_tab_width = flint_px(180)
    });
    return clicked >= 0 && clicked < tab_count ? values[clicked] : -1;
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

    practice_clamp_activity_to_tab(app);
    activity_count = 0;
    activity_index = 0;
    for(int i = 0; i < EXERCISE_COUNT; i++) {
        if(!practice_is_visible(app, i))
            continue;
        if(i == app->exercise_type)
            activity_index = activity_count;
        exercise_values[activity_count] = i;
        exercise_options[activity_count] = practice_activity_label(i);
        activity_count++;
    }
    if(activity_count <= 0)
        return;

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
       app->modal.type != UIModalPracticeMusic &&
       app->modal.type != UIModalEditProgressiveStartSpeed)
        return;

    DrawRectangle(0, 0, view_width, view_height, flint_theme_get_bg());
    if(app->modal.type == UIModalPracticeMusic) {
        practice_screen_draw_music_modal(app);
        return;
    }
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

static void
practice_home_layout(InbeApp *app, Rectangle *music, Rectangle *card, Rectangle *start,
                     Rectangle *manual, Rectangle *config)
{
    int top = app_content_top_reserved(app) + flint_px(14);
    int bottom_reserved = ui_bottom_nav_height();
    int content_w = view_width - flint_px(32);
    int max_w = flint_px(480);
    int x;
    int y;
    int card_h;
    int btn_h = flint_px(42);
    int gap = flint_px(12);

    if(content_w > max_w)
        content_w = max_w;
    if(content_w < flint_px(260))
        content_w = view_width - flint_px(20);
    x = (view_width - content_w) / 2;
    y = top;

    if(music != NULL)
        *music = (Rectangle){(float)x, (float)y, (float)content_w, (float)btn_h};
    y += btn_h + flint_px(18);

    card_h = view_height - y - bottom_reserved - flint_px(176);
    if(card_h < flint_px(180))
        card_h = flint_px(180);
    if(card_h > flint_px(300))
        card_h = flint_px(300);
    if(card != NULL)
        *card = (Rectangle){(float)x, (float)y, (float)content_w, (float)card_h};
    y += card_h + flint_px(18);

    if(start != NULL)
        *start = (Rectangle){(float)x, (float)y, (float)content_w, (float)btn_h};
    y += btn_h + gap;

    if(manual != NULL)
        *manual = (Rectangle){(float)x, (float)y, (float)content_w, (float)btn_h};
    y += btn_h + gap;

    if(config != NULL)
        *config = (Rectangle){(float)x, (float)y, (float)content_w, (float)btn_h};
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
    InbeSyncAccount account;

    return app != NULL && !app->tutorial_seen && !app->modal.active &&
           app->exercise_type != EXERCISE_SUN_SALUTATION &&
           app->inbe.screen == InbeScreenStart &&
           !sync_account_load(&account);
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
    Rectangle music;
    Rectangle card;
    Rectangle start;
    Rectangle manual;
    Rectangle config;

    if(!practice_screen_first_run_guide_active(app))
        return;

    practice_home_layout(app, &music, &card, &start, &manual, &config);
    steps[0] = (FlintUIGuideStep){
        music,
        locale_get("practice_guide_music")
    };
    steps[1] = (FlintUIGuideStep){
        card,
        locale_get("practice_guide_carousel")
    };
    steps[2] = (FlintUIGuideStep){
        start,
        locale_get("practice_guide_start")
    };
    steps[3] = (FlintUIGuideStep){
        (Rectangle){
            manual.x,
            manual.y,
            manual.width,
            (config.y + config.height) - manual.y
        },
        locale_get("practice_guide_manual_config")
    };

    result = flint_ui_draw_guide_overlay((FlintUIGuideOverlay){
        .steps = steps,
        .count = PRACTICE_GUIDE_STEPS,
        .step = &app->tutorial_step,
        .view_width = view_width,
        .view_height = view_height,
        .reserved_top = app_content_top_reserved(app),
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
