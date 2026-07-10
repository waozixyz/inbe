#include "practice_screen.h"
#include "app.h"
#include "screens/manual_screen.h"
#include "practices/practice_registry.h"
#include "sync_account.h"
#include "locale.h"
#include "theme.h"
#include "ui.h"

#include <stdio.h>

extern int view_width;
extern int view_height;

enum {
    PRACTICE_GUIDE_STEPS = 3
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
    int nav_y = title_h;
    int content_y = title_h + nav_h + content_top_gap;
    int content_h = view_height - bottom_reserved - content_y - content_bottom_gap;

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

static int
practice_visible_count(const InbeApp *app)
{
    int count = 0;

    for(int i = 0; i < EXERCISE_COUNT; i++) {
        if(practice_is_visible(app, i))
            count++;
    }
    return count;
}

static int
practice_visible_index(const InbeApp *app, int exercise)
{
    int index = 0;

    for(int i = 0; i < EXERCISE_COUNT; i++) {
        if(!practice_is_visible(app, i))
            continue;
        if(i == exercise)
            return index;
        index++;
    }
    return 0;
}

static int
practice_home_dots_height(const InbeApp *app)
{
    return practice_visible_count(app) > 1 ? ScaleUIPx(28) : 0;
}

static int
practice_home_carousel_controls_height(const InbeApp *app)
{
    return practice_visible_count(app) > 1 ? ScaleUIPx(52) : 0;
}

static void
practice_select_home_card(InbeApp *app, int exercise)
{
    AppRoute route;

    if(app == NULL)
        return;
    if(app->practice_tab == PRACTICE_TAB_CONFIG)
        app_leave_practice_config(app);
    route = app_current_route(app);
    route.screen = InbeScreenStart;
    route.exercise_type = practice_clamp_id(exercise);
    route.practice_tab = PRACTICE_TAB_PLAY;
    route.practice_config_tab = 0;
    app->manual_scroll = 0;
    app->settings_scroll = 0;
    app->practice_home_scroll = 0;
    app->tutorial_step = 0;
    app->settings_dirty = 1;
    app->settings_save_delay_ticks = 18;
    app_switch_route(app, route);
}

static void
practice_draw_home_dots(InbeApp *app, int x, int y, int w)
{
    int count = practice_visible_count(app);
    int active_index;
    int radius = ScaleUIPx(4);
    int active_radius = ScaleUIPx(5);
    int hit = ScaleUIPx(24);
    int gap = ScaleUIPx(14);
    int total_w;
    int start_x;
    int center_y;
    int dot_index = 0;
    Vector2 mouse_world;

    if(app == NULL || count <= 1)
        return;

    active_index = practice_visible_index(app, app->exercise_type);
    total_w = count * hit + (count - 1) * gap;
    start_x = x + (w - total_w) / 2 + hit / 2;
    center_y = y + ScaleUIPx(14);
    mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);

    for(int i = 0; i < EXERCISE_COUNT; i++) {
        int center_x;
        int current_radius;
        Rectangle hit_bounds;
        int active;
        int hovered;
        Color fill;
        Color stroke;

        if(!practice_is_visible(app, i))
            continue;

        center_x = start_x + dot_index * (hit + gap);
        current_radius = dot_index == active_index ? active_radius : radius;
        hit_bounds = (Rectangle){(float)(center_x - hit / 2),
                                 (float)(center_y - hit / 2),
                                 (float)hit, (float)hit};
        active = CheckCollisionPointRec(mouse_world, hit_bounds) &&
                 !UIInputCapturesClick(mouse_world);
        hovered = active && UIHoverEffectsEnabled();
        fill = dot_index == active_index ? GetThemeText()
                                         : GetThemeBackground();
        stroke = GetThemeText();

        if(hovered) {
            fill = dot_index == active_index ? LightenUIColor(fill, 18)
                                             : DarkenUIColor(GetThemeBackground(), 16);
            stroke = LightenUIColor(stroke, 18);
            MarkUIClickable();
        }
        DrawCircle(center_x, center_y, current_radius, fill);
        DrawCircleLines(center_x, center_y, current_radius + ScaleUIPx(1), stroke);
        if(active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && i != app->exercise_type)
            practice_select_home_card(app, i);

        dot_index++;
    }
}

static void
practice_draw_home_carousel_controls(InbeApp *app, int x, int y, int w)
{
    int arrow = ScaleUIPx(44);
    int row_h = practice_home_carousel_controls_height(app);
    int arrow_y = y + (row_h - arrow) / 2;
    int gap = ScaleUIPx(8);
    int dots_x = x + arrow + gap;
    int dots_w = w - (arrow + gap) * 2;
    Color arrow_bg = GetThemeButton();
    Color arrow_hover = GetThemeButtonHover();
    Color arrow_border = DarkenUIColor(GetThemeButton(), 35);
    Color arrow_icon = GetThemeText();

    if(app == NULL || row_h <= 0)
        return;

    if(dots_w < ScaleUIPx(80)) {
        dots_x = x;
        dots_w = w;
    }

    if(DrawUIIconButton((UIIconButton){
        .bounds = {(float)x, (float)arrow_y, (float)arrow, (float)arrow},
        .icon = app->icons[UI_ICON_TYPE_BACKWARD],
        .icon_size = arrow - ScaleUIPx(18),
        .icon_padding = ScaleUIPx(9),
        .background = arrow_bg,
        .hover_background = arrow_hover,
        .border = arrow_border,
        .icon_color = arrow_icon,
        .radius = 0.22f
    }))
        practice_select_home_card(app, practice_next_visible(app, -1));

    practice_draw_home_dots(app, dots_x,
                            y + (row_h - practice_home_dots_height(app)) / 2,
                            dots_w);

    if(DrawUIIconButton((UIIconButton){
        .bounds = {(float)(x + w - arrow), (float)arrow_y, (float)arrow, (float)arrow},
        .icon = app->icons[UI_ICON_TYPE_FORWARD],
        .icon_size = arrow - ScaleUIPx(18),
        .icon_padding = ScaleUIPx(9),
        .background = arrow_bg,
        .hover_background = arrow_hover,
        .border = arrow_border,
        .icon_color = arrow_icon,
        .radius = 0.22f
    }))
        practice_select_home_card(app, practice_next_visible(app, 1));
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

static const char *
practice_home_subtitle(int exercise)
{
    return exercise == EXERCISE_SUN_SALUTATION
               ? GetLocaleText("sun_salutation_work_in_progress")
               : NULL;
}

static int
practice_home_meditation_minutes(const InbeApp *app)
{
    switch(app != NULL ? app->meditation.duration_mode : 1) {
    case 0: return 5;
    case 1: return 15;
    case 2: return 30;
    case 3: return 60;
    case 4: return 120;
    default:
        return app != NULL ? clampi(app->meditation.custom_minutes, 1, 240) : 15;
    }
}

static void
practice_home_metadata(InbeApp *app, char *out, size_t out_size)
{
    if(out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    if(app == NULL)
        return;

    switch(practice_clamp_id(app->exercise_type)) {
    case EXERCISE_MEDITATION:
        FormatLocaleText(out, out_size, "practice_home_minutes",
                         practice_home_meditation_minutes(app));
        break;
    case EXERCISE_SUN_SALUTATION:
        FormatLocaleText(out, out_size, "practice_home_repetitions",
                         app->sun_salutation.repetitions);
        break;
    case EXERCISE_WIM_HOF:
    default:
        FormatLocaleText(out, out_size, "practice_home_wim_hof_metadata",
                         app->inbe.max_rounds, int_from_count(app->inbe.maxbreaths));
        break;
    }
}

static int
practice_home_title_height(const InbeApp *app)
{
    const char *subtitle = app != NULL ? practice_home_subtitle(app->exercise_type) : NULL;

    return subtitle != NULL ? ScaleUIPx(80) : ScaleUIPx(58);
}

static void
practice_draw_home_title(InbeApp *app, int x, int y, int w)
{
    const PracticeDefinition *practice;
    const char *title;
    const char *subtitle;
    int title_font;
    Rectangle title_rect;
    Rectangle metadata_rect;
    char metadata[96];

    if(app == NULL)
        return;

    practice = practice_get(app->exercise_type);
    title = practice->label_key != NULL ? practice_label(app->exercise_type) : "";
    subtitle = practice_home_subtitle(app->exercise_type);
    practice_home_metadata(app, metadata, sizeof(metadata));
    title_font = GetUITitleFontSize(title, w);
    title_rect = (Rectangle){(float)x, (float)y, (float)w, (float)ScaleUIPx(34)};
    DrawFittedUITextInRect(title, title_rect, title_font, UI_TEXT_16, GetThemeText());
    metadata_rect = (Rectangle){(float)x, (float)(y + ScaleUIPx(34)),
                                (float)w, (float)ScaleUIPx(18)};
    DrawFittedUITextInRect(metadata, metadata_rect,
                           GetUISmallFontSize(), UI_TEXT_8,
                           DarkenUIColor(GetThemeText(), 22));
    if(subtitle != NULL) {
        Rectangle subtitle_rect = {
            (float)x,
            (float)(y + ScaleUIPx(56)),
            (float)w,
            (float)ScaleUIPx(18)
        };
        DrawFittedUITextInRect(subtitle, subtitle_rect,
                               GetUISmallFontSize(), UI_TEXT_8,
                               DarkenUIColor(GetThemeText(), 22));
    }
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
    int scroll_y = 0;
    int bottom_reserved = app_content_bottom_reserved(app);
    int y = ScaleUIPx(14);
    int btn_h = ScaleUIPx(42);
    int gap = practice_home_action_gap(content_w);

    if(app == NULL)
        return 0;

    practice = practice_get(app->exercise_type);
    y += practice_home_title_height(app) + ScaleUIPx(10);
    y += practice_home_card_height(scroll_y + y, bottom_reserved);
    y += practice_home_carousel_controls_height(app) + ScaleUIPx(10);
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
    int layout_y;
    int card_w;
    int card_h;
    int gap = ScaleUIPx(12);
    int btn_h = ScaleUIPx(42);
    int hover = 0;
    int carousel_count;
    Rectangle card;
    Texture2D banner;

    if(app == NULL)
        return;

    practice_clamp_activity_to_tab(app);
    scroll_y = 0;
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
    layout_y = y;

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
        layout_y = scroll_y + ScaleUIPx(14);
    } else {
        app->practice_home_scroll = 0;
    }

    practice_draw_home_title(app, x, y, content_w);
    y += practice_home_title_height(app) + ScaleUIPx(10);
    layout_y += practice_home_title_height(app) + ScaleUIPx(10);

    card_w = content_w;
    card_h = practice_home_card_height(layout_y, bottom_reserved);
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

    carousel_count = practice_visible_count(app);
    y += card_h;
    if(carousel_count > 1) {
        practice_draw_home_carousel_controls(app, x, y, content_w);
        y += practice_home_carousel_controls_height(app);
    }
    y += ScaleUIPx(10);

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
        AppRoute route = app_current_route(app);
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        route.practice_tab = PRACTICE_TAB_PLAY;
        route.screen = InbeScreenStart;
        if(app->modal.type == UIModalPracticeManual ||
           app->modal.type == UIModalPracticeConfig)
            app_close_modal(app);
        app_switch_route(app, route);
    } else if(tab == PRACTICE_TAB_MANUAL) {
        AppRoute route = app_current_route(app);
        if(practice_get(app->exercise_type)->draw_manual == NULL)
            return;
        if(app->practice_tab == PRACTICE_TAB_CONFIG)
            app_leave_practice_config(app);
        route.practice_tab = PRACTICE_TAB_MANUAL;
        route.screen = InbeScreenStart;
        app->tutorial_step = 0;
        app->manual_scroll = 0;
        app_switch_route(app, route);
    } else if(tab == PRACTICE_TAB_CONFIG) {
        AppRoute route = app_current_route(app);
        if(practice_get(app->exercise_type)->draw_config == NULL)
            return;
        if(app->practice_tab != PRACTICE_TAB_CONFIG) {
            reset_settings_preview(app);
            app->settings_scroll = 0;
            app->practice_config_tab = 0;
        }
        route.practice_tab = PRACTICE_TAB_CONFIG;
        route.practice_config_tab = 0;
        route.screen = InbeScreenStart;
        app_switch_route(app, route);
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
            AppRoute route = app_current_route(app);
            if(app->practice_tab == PRACTICE_TAB_CONFIG)
                app_leave_practice_config(app);
            route.exercise_type = exercise_values[activity_index];
            route.practice_tab = PRACTICE_TAB_PLAY;
            route.practice_config_tab = 0;
            route.screen = InbeScreenStart;
            app->manual_scroll = 0;
            app->settings_scroll = 0;
            app->practice_home_scroll = 0;
            app->tutorial_step = 0;
            app->practice_config_tab = 0;
            app->settings_dirty = 1;
            app->settings_save_delay_ticks = 18;
            app_switch_route(app, route);
        }
        return;
    }

    // Desktop mode: use tab bar instead of dropdown
    if(app_should_use_tab_bar(app)) {
        int clicked_exercise = practice_screen_draw_desktop_tab_bar(app, 0);
        if(clicked_exercise >= 0 && clicked_exercise != app->exercise_type) {
            AppRoute route = app_current_route(app);
            if(app->practice_tab == PRACTICE_TAB_CONFIG)
                app_leave_practice_config(app);
            route.exercise_type = clicked_exercise;
            route.practice_tab = PRACTICE_TAB_PLAY;
            route.practice_config_tab = 0;
            route.screen = InbeScreenStart;
            app->manual_scroll = 0;
            app->settings_scroll = 0;
            app->practice_home_scroll = 0;
            app->tutorial_step = 0;
            app->practice_config_tab = 0;
            app->settings_dirty = 1;
            app->settings_save_delay_ticks = 18;
            app_switch_route(app, route);
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
       app->modal.type != UIModalEditProgressiveStartSpeed)
        return;

    DrawRectangle(0, 0, view_width, view_height, GetThemeBackground());
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
practice_home_layout(InbeApp *app, Rectangle *card, Rectangle *start,
                     Rectangle *manual, Rectangle *config)
{
    int top = ScaleUIPx(14);
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

    y += practice_home_title_height(app) + ScaleUIPx(10);

    card_h = view_height - y - bottom_reserved - ScaleUIPx(176);
    if(card_h < ScaleUIPx(180))
        card_h = ScaleUIPx(180);
    if(card_h > ScaleUIPx(300))
        card_h = ScaleUIPx(300);
    if(card != NULL)
        *card = (Rectangle){(float)x, (float)y, (float)content_w, (float)card_h};
    y += card_h + practice_home_carousel_controls_height(app) + ScaleUIPx(10);

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
    Rectangle card;
    Rectangle start;
    Rectangle manual;
    Rectangle config;

    if(!practice_screen_first_run_guide_active(app))
        return;

    practice_home_layout(app, &card, &start, &manual, &config);
    steps[0] = (UIGuideStep){
        card,
        GetLocaleText("practice_guide_carousel")
    };
    steps[1] = (UIGuideStep){
        start,
        GetLocaleText("practice_guide_start")
    };
    steps[2] = (UIGuideStep){
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
        .reserved_top = 0,
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
