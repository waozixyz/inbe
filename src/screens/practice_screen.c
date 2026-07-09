#include "practice_screen.h"
#include "app.h"
#include "screens/manual_screen.h"
#include "practices/meditation/meditation_music.h"
#include "practices/practice_registry.h"
#include "sync_account.h"
#include "locale.h"
#include "theme.h"
#include "ui.h"

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

int
practice_screen_subscreen_integrated(const InbeApp *app, int modal_type)
{
    return app != NULL &&
           app->inbe.screen == InbeScreenStart &&
           (int)app->modal.type != modal_type;
}

void
practice_screen_config_layout(InbeApp *app, int modal_type, int content_top_gap,
                              PracticeSubscreenLayout *layout)
{
    int integrated = practice_screen_subscreen_integrated(app, modal_type);
    int title_h = integrated ? app_content_top_reserved(app) : GetUITitleBarHeight();
    int scroll_y = title_h + content_top_gap;
    int bottom_reserved = integrated ? app_content_bottom_reserved(app) : 0;
    int scroll_h = view_height - scroll_y - bottom_reserved -
                   (integrated ? ScaleUIPx(8) : 0);

    if(layout == NULL)
        return;
    if(scroll_h < 0)
        scroll_h = 0;
    *layout = (PracticeSubscreenLayout){
        integrated,
        title_h,
        scroll_y,
        scroll_h
    };
}

void
practice_screen_manual_layout(InbeApp *app, int modal_type, int page_count,
                              int content_top_gap, int content_bottom_gap,
                              int min_content_h, PracticeManualLayout *layout)
{
    int integrated = practice_screen_subscreen_integrated(app, modal_type);
    int title_h = integrated ? app_content_top_reserved(app) : GetUITitleBarHeight();
    int nav_h = page_count > 1 ? manual_screen_guide_nav_height() : 0;
    int bottom_reserved = integrated ? app_content_bottom_reserved(app) : 0;
    int nav_y = view_height - bottom_reserved - nav_h;
    int content_y = title_h + content_top_gap;
    int content_h = nav_y - content_y - content_bottom_gap;

    if(layout == NULL)
        return;
    if(content_h < min_content_h)
        content_h = min_content_h;
    *layout = (PracticeManualLayout){
        integrated,
        title_h,
        nav_y,
        nav_h,
        content_y,
        content_h
    };
}

int
practice_screen_handle_config_title(InbeApp *app, const char *title, int modal_type,
                                    void (*leave_config)(InbeApp *app))
{
    int title_h;

    if(app == NULL)
        return 0;
    title_h = practice_screen_subscreen_integrated(app, modal_type)
                  ? app_content_top_reserved(app)
                  : GetUITitleBarHeight();
    if(!DrawUIReturnTitleBar(app->icons[UI_ICON_TYPE_RETURN], title, title_h))
        return 0;

    if(app->modal.active && (int)app->modal.type == modal_type) {
        app_close_modal(app);
    } else {
        if(app->settings_dirty)
            save_settings(app);
        if(leave_config != NULL)
            leave_config(app);
        app->settings_scroll = 0;
        app->practice_tab = PRACTICE_TAB_PLAY;
        app_switch_screen(app, InbeScreenStart);
    }
    return 1;
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
    int title_font = GetUIFontSize();
    int subtitle_font = GetUISmallFontSize();
    int pad_x = ScaleUIPx(12);
    int pad_y = ScaleUIPx(6);
    int gap = ScaleUIPx(4);
    int title_w = MeasureUIText(title != NULL ? title : "", title_font);
    int subtitle_w = subtitle != NULL ? MeasureUIText(subtitle, subtitle_font) : 0;
    int content_w = title_w > subtitle_w ? title_w : subtitle_w;
    int max_label_w = ((int)card.width * 58) / 100;
    int label_w = content_w + pad_x * 2;
    int label_h = ScaleUIPx(34) + pad_y * 2;
    int inner_w;
    int label_x;
    int label_y;
    Rectangle label;
    Rectangle title_rect;

    if(subtitle != NULL)
        label_h += ScaleUIPx(18) + gap;
    if(max_label_w < ScaleUIPx(136))
        max_label_w = ScaleUIPx(136);
    if(label_w > max_label_w)
        label_w = max_label_w;
    if(label_w < ScaleUIPx(104))
        label_w = ScaleUIPx(104);
    inner_w = label_w - pad_x * 2;
    if(inner_w < ScaleUIPx(40))
        inner_w = ScaleUIPx(40);

    label_x = (int)card.x + ((int)card.width - label_w) / 2;
    label_y = (int)card.y + ((int)card.height - label_h) / 2;
    label = (Rectangle){(float)label_x, (float)label_y,
                        (float)label_w, (float)label_h};

    DrawRectangleRec(label, Fade(BLACK, 0.30f));
    DrawRectangleLinesEx(label, ScaleUIPx(1), Fade(WHITE, 0.42f));
    title_rect = (Rectangle){(float)(label_x + pad_x),
                             (float)(label_y + pad_y),
                             (float)inner_w,
                             (float)ScaleUIPx(34)};
    DrawFittedUITextInRect(title != NULL ? title : "", title_rect,
                                title_font, UI_TEXT_8, WHITE);
    if(subtitle != NULL) {
        Rectangle subtitle_rect = {
            (float)(label_x + pad_x),
            (float)(label_y + pad_y + ScaleUIPx(39) + gap),
            (float)inner_w,
            (float)ScaleUIPx(18)
        };
        DrawFittedUITextInRect(subtitle, subtitle_rect,
                                    subtitle_font, UI_TEXT_8,
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
    app->practice_home_scroll = 0;
    app->tutorial_step = 0;
    app->practice_config_tab = 0;
    save_settings(app);
}

static int
practice_music_content_height(int content_w, void *user_data)
{
    InbeApp *app = user_data;

    return meditation_music_measure_settings(app, content_w, 1, 1) + ScaleUIPx(24);
}

static void
practice_screen_draw_music_modal(InbeApp *app)
{
    int title_h = GetUITitleBarHeight();
    int scroll_y = title_h + ScaleUIPx(16);
    int scroll_h = view_height - scroll_y - ScaleUIPx(8);

    if(DrawUIReturnTitleBar(app->icons[UI_ICON_TYPE_RETURN], GetLocaleText("practice_music_title"), title_h))
        app_close_modal(app);

    if(scroll_h < 0)
        scroll_h = 0;
    {
        UIScrollPage page = BeginUIScrollPage((UIScrollPageSpec){
            .y = scroll_y,
            .height = scroll_h,
            .max_content_width = ScaleUIPx(CONTENT_MAX_W),
            .min_content_width = ScaleUIPx(320),
            .scroll_offset = &app->settings_scroll,
            .content_height = practice_music_content_height,
            .user_data = app
        });
        int y = page.content_y;

        meditation_music_draw_settings(app, page.content_x, page.content_w,
                                       &y, 1, 1);
        EndUIScrollPage(page);
    }
    meditation_music_draw_dropdown_menu(app);
}

static int
practice_home_card_height(int y, int bottom_reserved)
{
    int card_h = view_height - y - bottom_reserved - ScaleUIPx(176);

    if(card_h < ScaleUIPx(180))
        card_h = ScaleUIPx(180);
    if(card_h > ScaleUIPx(300))
        card_h = ScaleUIPx(300);
    return card_h;
}

static int
practice_home_action_gap(int content_w)
{
    return content_w < ScaleUIPx(340) ? ScaleUIPx(6) : ScaleUIPx(8);
}

static int
practice_home_action_row_widths(int content_w, int *manual_w, int *config_w)
{
    int font = GetUISmallFontSize();
    int pad = ScaleUIPx(8);
    int gap = practice_home_action_gap(content_w);
    int manual_min = MeasureUIText(GetLocaleText("practice_manual_button"), font) + pad * 2;
    int config_min = MeasureUIText(GetLocaleText("practice_configure_button"), font) + pad * 2;
    int half_w;
    int available_w;

    if(content_w < manual_min + config_min + gap)
        return 0;

    available_w = content_w - gap;
    half_w = available_w / 2;
    if(half_w >= manual_min && available_w - half_w >= config_min) {
        *manual_w = half_w;
        *config_w = available_w - half_w;
        return 1;
    }

    *manual_w = manual_min;
    *config_w = available_w - *manual_w;
    if(*config_w < config_min) {
        *config_w = config_min;
        *manual_w = available_w - *config_w;
    }
    return *manual_w >= manual_min && *config_w >= config_min;
}

static int
practice_home_actions_share_row(const PracticeDefinition *practice, int content_w)
{
    int manual_w;
    int config_w;

    return practice != NULL &&
           practice->draw_manual != NULL &&
           practice->draw_config != NULL &&
           practice_home_action_row_widths(content_w, &manual_w, &config_w);
}

static int
practice_home_content_height(int content_w, void *user_data)
{
    InbeApp *app = user_data;
    const PracticeDefinition *practice;
    int scroll_y = app_content_top_reserved(app);
    int bottom_reserved = app_content_bottom_reserved(app);
    int y = ScaleUIPx(14);
    int btn_h = ScaleUIPx(42);
    int gap = practice_home_action_gap(content_w);

    (void)content_w;
    if(app == NULL)
        return 0;

    practice = practice_get(app->exercise_type);
    y += btn_h + ScaleUIPx(18);
    y += practice_home_card_height(scroll_y + y, bottom_reserved) + ScaleUIPx(18);
    y += btn_h;
    if(practice_home_actions_share_row(practice, content_w)) {
        y += gap + btn_h;
    } else {
        if(practice->draw_manual != NULL)
            y += gap + btn_h;
        if(practice->draw_config != NULL)
            y += gap + btn_h;
    }
    return y + ScaleUIPx(14);
}

void
practice_screen_draw_home(InbeApp *app)
{
    const PracticeDefinition *practice;
    int scroll_y;
    int scroll_h;
    int bottom_reserved = app_content_bottom_reserved(app);
    UIScrollPage page = {0};
    int use_scroll = 0;
    int content_w;
    int x;
    int y;
    int card_w;
    int card_h;
    int gap = ScaleUIPx(12);
    int btn_h = ScaleUIPx(42);
    int hover = 0;
    int arrow = ScaleUIPx(44);
    Rectangle card;
    Texture2D banner;

    if(app == NULL)
        return;

    practice_clamp_activity_to_tab(app);
    DrawUITitleBar(GetLocaleText("practice_title"), app_content_top_reserved(app));
    scroll_y = app_content_top_reserved(app);
    scroll_h = view_height - scroll_y - bottom_reserved;
    if(scroll_h < 0)
        scroll_h = 0;
    practice = practice_get(app->exercise_type);
    content_w = view_width - ScaleUIPx(24);
    if(content_w > ScaleUIPx(480))
        content_w = ScaleUIPx(480);
    if(content_w < ScaleUIPx(260))
        content_w = view_width - ScaleUIPx(16);
    x = (view_width - content_w) / 2;
    y = scroll_y + ScaleUIPx(14);

    if(practice_home_content_height(content_w, app) > scroll_h) {
        use_scroll = 1;
        page = BeginUIScrollPage((UIScrollPageSpec){
            .y = scroll_y,
            .height = scroll_h,
            .max_content_width = ScaleUIPx(480),
            .min_content_width = ScaleUIPx(260),
            .scroll_offset = &app->practice_home_scroll,
            .content_height = practice_home_content_height,
            .user_data = app
        });
        content_w = page.content_w;
        x = page.content_x;
        y = page.content_y + ScaleUIPx(14);
    } else {
        app->practice_home_scroll = 0;
    }

    if(DrawUIGenericButton(x, y, content_w, btn_h, GetLocaleText("practice_music_title"),
                              UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
        app->settings_scroll = 0;
        app_open_modal(app, UIModalPracticeMusic);
    }
    y += btn_h + ScaleUIPx(18);

    card_w = content_w;
    card_h = practice_home_card_height(y, bottom_reserved);
    card = (Rectangle){(float)x, (float)y, (float)card_w, (float)card_h};
    banner = practice_card_banner(app, app->exercise_type);
    if(banner.id != 0) {
        practice_draw_card_banner(banner, card);
    } else {
        DrawRectangleGradientV(x, y, card_w, card_h,
                               practice_card_color_top(app->exercise_type),
                               practice_card_color_bottom(app->exercise_type));
    }
    DrawRectangleLinesEx(card, ScaleUIPx(2), LightenUIColor(GetThemeBackground(), 55));

    practice_draw_card_label(
        card,
        practice->label_key != NULL ? practice_label(app->exercise_type) : "",
        app->exercise_type == EXERCISE_SUN_SALUTATION
            ? GetLocaleText("sun_salutation_work_in_progress")
            : NULL);

    if(practice_count() > 1) {
        int arrow_y = y + (card_h - arrow) / 2;
        int left_x = x + ScaleUIPx(10);
        int right_x = x + card_w - arrow - ScaleUIPx(10);
        if(DrawUIOverlayButton((UIOverlayButton){
            .bounds = {(float)left_x, (float)arrow_y, (float)arrow, (float)arrow},
            .label = "<",
            .background = Fade(BLACK, 0.22f),
            .hover_background = Fade(WHITE, 0.32f),
            .border = Fade(WHITE, 0.42f),
            .hover_border = Fade(WHITE, 0.74f),
            .text = WHITE
        }))
            practice_select_home_card(app, practice_next_visible(app, -1));
        if(DrawUIOverlayButton((UIOverlayButton){
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

    y += card_h + ScaleUIPx(18);

    if(DrawUIGenericButton(x, y, content_w, btn_h, GetLocaleText("practice_start_button"),
                              UI_BUTTON_STYLE_PRIMARY, 0, &hover)) {
        if(practice->start != NULL)
            practice->start(app);
    }
    y += btn_h + gap;
    if(practice_home_actions_share_row(practice, content_w)) {
        int row_gap = practice_home_action_gap(content_w);
        int manual_w;
        int config_w;
        practice_home_action_row_widths(content_w, &manual_w, &config_w);
        if(DrawUIGenericButton(x, y, manual_w, btn_h, GetLocaleText("practice_manual_button"),
                                  UI_BUTTON_STYLE_SECONDARY, 0, &hover))
            practice_screen_open_tab(app, PRACTICE_TAB_MANUAL);
        if(DrawUIGenericButton(x + manual_w + row_gap, y, config_w,
                                  btn_h, GetLocaleText("practice_configure_button"),
                                  UI_BUTTON_STYLE_SECONDARY, 0, &hover))
            practice_screen_open_tab(app, PRACTICE_TAB_CONFIG);
    } else {
        if(practice->draw_manual != NULL &&
           DrawUIGenericButton(x, y, content_w, btn_h, GetLocaleText("practice_manual_button"),
                                  UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
            practice_screen_open_tab(app, PRACTICE_TAB_MANUAL);
        }
        if(practice->draw_manual != NULL)
            y += btn_h + gap;
        if(practice->draw_config != NULL &&
           DrawUIGenericButton(x, y, content_w, btn_h, GetLocaleText("practice_configure_button"),
                                  UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
            practice_screen_open_tab(app, PRACTICE_TAB_CONFIG);
        }
    }

    if(use_scroll)
        EndUIScrollPage(page);
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
        app_switch_screen(app, InbeScreenStart);
    }
}

static int
practice_screen_draw_desktop_tab_bar(InbeApp *app, int y)
{
    UITab tabs[EXERCISE_COUNT];
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
        tabs[tab_count++] = (UITab){
            .label = practice_activity_label(i),
            .icon = (Texture2D){0},
            .icon_size = 0,
            .disabled = app->modal.active
        };
    }
    if(tab_count <= 0)
        return -1;

    clicked = DrawUITabBar((UITabBar){
        .bounds = {0, (float)y, (float)view_width, (float)GetUITabBarHeight()},
        .tabs = tabs,
        .count = tab_count,
        .selected_index = selected_index,
        .min_tab_width = ScaleUIPx(120),
        .max_tab_width = ScaleUIPx(180)
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
        UIToolbarResult menu_result = DrawUIToolbarHeader((UIToolbarHeader){
            .toolbar = (UIToolbar){
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
            app->practice_home_scroll = 0;
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
            app->practice_home_scroll = 0;
            app->tutorial_step = 0;
            app->practice_config_tab = 0;
            save_settings(app);
        }
    } else {
        // Mobile mode: keep existing dropdown
        (void)DrawUIToolbarHeader((UIToolbarHeader){
            .leading_icon = (Texture2D){0},
            .toolbar = (UIToolbar){
                .id = 300,
                .height = GetUITabBarHeight(),
                .options = app->modal.active ? NULL : exercise_options,
                .option_count = app->modal.active ? 0 : activity_count,
                .selected_index = &activity_index,
                .dropdown_min_width = ScaleUIPx(160),
                .dropdown_height = ScaleUIPx(36),
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
    int icon_size = ScaleUIPx(22);
    int icon_padding = ScaleUIPx(9);
    int button_w = icon_size + icon_padding * 2;
    int gap = ScaleUIPx(10);
    int margin = ScaleUIPx(14);
    int y = app_content_top_reserved(app) + ScaleUIPx(10);
    int x = view_width - margin;

    if(app == NULL || app->modal.active || app->inbe.screen != InbeScreenStart)
        return;
    practice = practice_get(app->exercise_type);
    if(practice->draw_config != NULL) {
        int hover = 0;
        x -= button_w;
        if(DrawUIPaddedIconBtn(x, y, icon_size, icon_padding,
                                   app->icons[UI_ICON_TYPE_WRENCH], &hover))
            practice_screen_open_tab(app, PRACTICE_TAB_CONFIG);
        x -= gap;
    }
    if(practice->draw_manual != NULL) {
        int hover = 0;
        x -= button_w;
        if(DrawUIPaddedIconBtn(x, y, icon_size, icon_padding,
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

    DrawRectangle(0, 0, view_width, view_height, GetThemeBackground());
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
    int top = app_content_top_reserved(app) + ScaleUIPx(14);
    int bottom_reserved = GetUIBottomNavHeight();
    int content_w = view_width - ScaleUIPx(24);
    int max_w = ScaleUIPx(480);
    const PracticeDefinition *practice = practice_get(app->exercise_type);
    int x;
    int y;
    int card_h;
    int btn_h = ScaleUIPx(42);
    int gap;

    if(content_w > max_w)
        content_w = max_w;
    if(content_w < ScaleUIPx(260))
        content_w = view_width - ScaleUIPx(16);
    gap = practice_home_action_gap(content_w);
    x = (view_width - content_w) / 2;
    y = top;

    if(music != NULL)
        *music = (Rectangle){(float)x, (float)y, (float)content_w, (float)btn_h};
    y += btn_h + ScaleUIPx(18);

    card_h = view_height - y - bottom_reserved - ScaleUIPx(176);
    if(card_h < ScaleUIPx(180))
        card_h = ScaleUIPx(180);
    if(card_h > ScaleUIPx(300))
        card_h = ScaleUIPx(300);
    if(card != NULL)
        *card = (Rectangle){(float)x, (float)y, (float)content_w, (float)card_h};
    y += card_h + ScaleUIPx(18);

    if(start != NULL)
        *start = (Rectangle){(float)x, (float)y, (float)content_w, (float)btn_h};
    y += btn_h + gap;

    if(practice_home_actions_share_row(practice, content_w)) {
        int row_gap = practice_home_action_gap(content_w);
        int manual_w;
        int config_w;
        practice_home_action_row_widths(content_w, &manual_w, &config_w);
        if(manual != NULL)
            *manual = (Rectangle){(float)x, (float)y, (float)manual_w, (float)btn_h};
        if(config != NULL)
            *config = (Rectangle){(float)(x + manual_w + row_gap), (float)y,
                                  (float)config_w, (float)btn_h};
    } else {
        if(manual != NULL)
            *manual = (Rectangle){(float)x, (float)y, (float)content_w, (float)btn_h};
        y += btn_h + gap;
        if(config != NULL)
            *config = (Rectangle){(float)x, (float)y, (float)content_w, (float)btn_h};
    }
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
    UIGuideStep steps[PRACTICE_GUIDE_STEPS];
    UIGuideResult result;
    Rectangle music;
    Rectangle card;
    Rectangle start;
    Rectangle manual;
    Rectangle config;

    if(!practice_screen_first_run_guide_active(app))
        return;

    practice_home_layout(app, &music, &card, &start, &manual, &config);
    steps[0] = (UIGuideStep){
        music,
        GetLocaleText("practice_guide_music")
    };
    steps[1] = (UIGuideStep){
        card,
        GetLocaleText("practice_guide_carousel")
    };
    steps[2] = (UIGuideStep){
        start,
        GetLocaleText("practice_guide_start")
    };
    steps[3] = (UIGuideStep){
        (Rectangle){
            manual.x,
            manual.y,
            manual.width,
            (config.y + config.height) - manual.y
        },
        GetLocaleText("practice_guide_manual_config")
    };

    result = DrawUIGuideOverlay((UIGuideOverlay){
        .steps = steps,
        .count = PRACTICE_GUIDE_STEPS,
        .step = &app->tutorial_step,
        .view_width = view_width,
        .view_height = view_height,
        .reserved_top = app_content_top_reserved(app),
        .reserved_bottom = GetUIBottomNavHeight(),
        .max_width = ScaleUIPx(300),
        .close_icon = app->icons[UI_ICON_TYPE_X],
        .back_icon = app->icons[UI_ICON_TYPE_BACKWARD],
        .next_icon = app->icons[UI_ICON_TYPE_FORWARD],
        .done_icon = app->icons[UI_ICON_TYPE_CHECK]
    });
    if(result.closed || result.finished)
        practice_screen_finish_first_run_guide(app);
}
