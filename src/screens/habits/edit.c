#include "habits.h"

/* Habit edit functions */
void
habit_edit_begin_new(InbeApp *app)
{
    const char *default_name;
    Color default_color = {99, 196, 165, 255};
    int created;

    if(app == NULL)
        return;

    default_name = locale_get("habit_new_default_name");
    created = habits_add_custom(&app->habits, default_name, default_color,
                                INBE_HABIT_SYNC_NONE, 0);
    if(created < 0)
        return;

    app_auto_sync(app);
    habit_edit_begin(app, created);
}

void
habit_edit_begin(InbeApp *app, int index)
{
    if(app == NULL || index < 0 || index >= app->habits.count)
        return;

    app->habit_edit = (HabitEditState){
        .active = 1,
        .index = index,
        .color = app->habits.items[index].color,
        .sync_mode = app->habits.items[index].sync_mode,
        .sync_activity = app->habits.items[index].sync_activity,
        .counter_enabled = app->habits.items[index].counter_enabled
    };
    snprintf(app->habit_edit.text, sizeof(app->habit_edit.text), "%s",
             app->habits.items[index].name);
    app->habit_edit.cursor = (int)strlen(app->habit_edit.text);
    app->habits.tab = HABIT_TAB_EDIT;
    app->inbe.screen = InbeScreenHabits;
}

void
habit_edit_cancel(InbeApp *app)
{
    if(app == NULL)
        return;

    app->habit_edit = (HabitEditState){
        .index = -1,
        .color = {99, 196, 165, 255},
        .sync_mode = INBE_HABIT_SYNC_NONE
    };
    ui_focus_set_text_input_active(0);
}

static const char *
habit_edit_trimmed_text(InbeApp *app)
{
    char *start;
    char *end;

    if(app == NULL)
        return "";

    start = app->habit_edit.text;
    while(*start == ' ' || *start == '\t')
        start++;
    end = start + strlen(start);
    while(end > start && (end[-1] == ' ' || end[-1] == '\t'))
        end--;
    *end = '\0';
    return start;
}

static int
habit_edit_info_modal(const char *title, const char *message)
{
    static int info_scroll = 0;
    FlintUIPanelFrame frame;
    FlintUIParagraph paragraph;
    FlintUIScrollView scroll_view;
    FlintUIScrollArea scroll_area;
    int y;
    int modal_h;
    int paragraph_h;
    int button_w = flint_px(112);
    int button_h = flint_px(36);
    int button_y;
    int text_h;
    int hover = 0;
    int result = 0;

    paragraph = (FlintUIParagraph){
        .text = message,
        .width = flint_px(320) - flint_px(36),
        .font = flint_ui_font(),
        .line_gap = flint_px(4),
        .color = theme_get_text()
    };
    modal_h = ui_paragraph_modal_height((FlintUIParagraphModalMeasure){
        .message = message,
        .width = flint_px(320),
        .button_h = button_h,
        .line_gap = flint_px(4),
        .extra_lines = 2,
        .min_height = flint_px(196),
        .font = flint_ui_font()
    });

    frame = ui_draw_modal_frame(flint_px(320), modal_h, title,
                                (Texture2D){0}, (Texture2D){0});
    paragraph.width = frame.content_w;
    paragraph_h = flint_ui_paragraph_height(paragraph);
    button_y = frame.y + frame.h - button_h - flint_px(16);
    text_h = button_y - frame.content_y - flint_px(16);
    if(text_h < flint_px(32))
        text_h = flint_px(32);

    scroll_area = (FlintUIScrollArea){
        .bounds = {
            (float)frame.content_x,
            (float)frame.content_y,
            (float)frame.content_w,
            (float)text_h
        },
        .content_height = paragraph_h,
        .content_x = frame.content_x,
        .content_width = frame.content_w,
        .scroll_offset = &info_scroll
    };
    scroll_view = ui_scroll_container_begin(scroll_area);
    y = scroll_view.content_y;
    flint_ui_paragraph_draw(paragraph, scroll_view.content_x, &y);
    ui_scroll_container_end(scroll_area, scroll_view);

    if(ui_draw_generic_button(frame.x + (frame.w - button_w) / 2,
                              button_y,
                              button_w, button_h, locale_get("ok_button"),
                              UI_BUTTON_STYLE_PRIMARY, 0, &hover))
        result = 1;
    if(frame.right_clicked)
        result = 1;
    if(result)
        info_scroll = 0;
    return result;
}

void
habit_edit_commit(InbeApp *app)
{
    const char *text;
    int index;

    if(app == NULL || !app->habit_edit.active)
        return;

    index = app->habit_edit.index;
    if(!app->habit_edit.is_new && (index < 0 || index >= app->habits.count)) {
        habit_edit_cancel(app);
        return;
    }

    text = habit_edit_trimmed_text(app);
    if(text[0] != '\0') {
        if(app->habit_edit.sync_activity != 0)
            app->habit_edit.sync_mode = INBE_HABIT_SYNC_ACTIVITIES;
        else
            app->habit_edit.sync_mode = INBE_HABIT_SYNC_NONE;
        if(app->habit_edit.is_new) {
            int created = habits_add_custom(&app->habits, text, app->habit_edit.color,
                                                 app->habit_edit.sync_mode,
                                                 app->habit_edit.sync_activity);
            if(created >= 0 && created < app->habits.count) {
                app->habits.items[created].counter_enabled = app->habit_edit.counter_enabled != 0;
                habits_save(&app->habits);
            }
        } else {
            snprintf(app->habits.items[index].name,
                     sizeof(app->habits.items[index].name), "%s", text);
            app->habits.items[index].color = app->habit_edit.color;
            app->habits.items[index].color.a = 255;
            app->habits.items[index].sync_mode = app->habit_edit.sync_mode;
            app->habits.items[index].sync_activity = app->habit_edit.sync_activity;
            app->habits.items[index].counter_enabled = app->habit_edit.counter_enabled != 0;
            app->habits.selected = index;
            habits_save(&app->habits);
        }
        app_auto_sync(app);
    }
    habit_edit_cancel(app);
    app->habits.tab = app->habits.view_mode == HABIT_VIEW_WEEKLY
                          ? HABIT_TAB_WEEKLY
                          : HABIT_TAB_MONTHLY;
    app->inbe.screen = InbeScreenHabits;
}

static void
habit_edit_clamp_cursor(InbeApp *app)
{
    int len;

    if(app == NULL)
        return;

    len = (int)strlen(app->habit_edit.text);
    if(app->habit_edit.cursor < 0)
        app->habit_edit.cursor = 0;
    if(app->habit_edit.cursor > len)
        app->habit_edit.cursor = len;
}

static void
habit_edit_handle_keyboard(InbeApp *app)
{
    if(app == NULL || !app->habit_edit.active)
        return;
    if(IsKeyPressed(KEY_ESCAPE)) {
        habit_edit_cancel(app);
        return;
    }
}

static FlintUITextField
habit_edit_text_field(InbeApp *app, int font)
{
    FlintUITextInputStyle style = {
        .background = flint_darken(theme_get_bg(), 4),
        .border = theme_get_button(),
        .focus_border = theme_get_button_hover(),
        .text = theme_get_text(),
        .cursor = theme_get_text(),
        .radius = 0.08f,
        .padding_x = flint_px(10)
    };

    return (FlintUITextField){
        .bounds = {0},
        .text = app->habit_edit.text,
        .text_size = sizeof(app->habit_edit.text),
        .cursor_position = &app->habit_edit.cursor,
        .focused = &app->habit_edit.focused,
        .max_codepoints = INBE_HABIT_NAME_SIZE - 1,
        .font = font,
        .style = style
    };
}


static int
habit_color_button(InbeApp *app, int x, int y, Color color, int selected)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int radius = flint_px(13);
    Rectangle bounds = {
        (float)(x - radius - flint_px(6)),
        (float)(y - radius - flint_px(6)),
        (float)(radius * 2 + flint_px(12)),
        (float)(radius * 2 + flint_px(12))
    };
    int active = CheckCollisionPointRec(mouse_world, bounds);
    int hovered = active && ui_hover_effects_enabled();

    DrawCircle(x, y, radius, color);
    DrawCircleLines(x, y, radius + flint_px(2),
                    selected ? theme_get_text() : flint_darken(theme_get_bg(), 42));
    if(active) {
        app->cursor_clickable = 1;
        if(hovered)
            DrawCircleLines(x, y, radius + flint_px(5), theme_get_button_hover());
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            return 1;
    }
    return 0;
}

static int
habit_edit_scroll_content_height(int content_w, void *user_data)
{
    InbeApp *app = user_data;
    int height = flint_px(18);

    (void)content_w;
    height += ui_label_text_field_height((FlintUILabelTextField){
        .label = locale_get("habit_name_label"),
        .field_h = flint_px(40)
    });
    height += flint_px(32) + flint_px(34);
    height += ui_section_label_height((FlintUISectionLabel){0});
    for(int i = 0; i < EXERCISE_COUNT; i++)
        height += ui_checkbox_row_height((FlintUICheckboxRow){0});
    height += flint_px(4);
    height += ui_section_label_height((FlintUISectionLabel){0});
    height += ui_checkbox_row_height((FlintUICheckboxRow){0});
    height += flint_px(10);
    height += ui_button_row_height((FlintUIButtonRow){.height = flint_px(40)});
    height += flint_px(24);
    (void)app;
    return height;
}

void
draw_habit_edit_screen(InbeApp *app)
{
    const char *title;
    const char *activity_options[] = {
        "Wim Hof Breathing",
        "Meditation",
        "Sun Salutation",
        "7-Minute Workout"
    };
    Color color_options[6];
    int top_h = flint_px(58);
    int bottom_reserved;
    int content_x;
    int content_w;
    int max_w = flint_px(CONTENT_MAX_W);
    int y = top_h + flint_px(18);
    int font = flint_ui_font();
    int label_font = flint_ui_font_small();
    int field_h = flint_px(40);
    int hover = 0;
    int title_font;
    int title_w;

    if(app == NULL)
        return;
    bottom_reserved = app_content_bottom_reserved(app);

    if(!app->habit_edit.active) {
        app->habits.tab = app->habits.view_mode == HABIT_VIEW_WEEKLY
                              ? HABIT_TAB_WEEKLY
                              : HABIT_TAB_MONTHLY;
        app->inbe.screen = InbeScreenHabits;
        return;
    }

    title = app->habit_edit.is_new ? locale_get("habit_new_title") : locale_get("habit_edit_title");

    if(app->inbe.screen == InbeScreenHabitEdit) {
        DrawRectangle(0, 0, view_width, top_h, theme_get_bg());
        DrawLine(0, top_h - 1, view_width, top_h - 1, flint_darken(theme_get_button(), 18));
        if(!app->modal.active &&
           ui_draw_icon_btn_padded(flint_px(12), flint_px(12), flint_px(24),
                                   flint_px(8), app->icons[UI_ICON_TYPE_RETURN], &hover)) {
            habit_edit_cancel(app);
            app->inbe.screen = InbeScreenHabits;
            return;
        }
        title_font = flint_ui_title_font(title, view_width - flint_px(120));
        title_w = flint_text_measure(title, title_font);
        flint_text_draw(title, (view_width - title_w) / 2,
                        flint_ui_text_y(title, 0, top_h, title_font),
                        title_font, theme_get_text());
        if(!app->modal.active &&
           ui_draw_icon_btn_padded(view_width - flint_px(52), flint_px(12),
                                   flint_px(24), flint_px(8), app->icons[UI_ICON_TYPE_CHECK], &hover)) {
            habit_edit_commit(app);
            return;
        }
    } else {
        top_h = app_content_top_reserved(app) + flint_px(40);
        y = top_h + flint_px(18);
    }

    if(app->modal.active && app->modal.type == UIModalHabitPracticeListInfo) {
        if(habit_edit_info_modal(locale_get("habit_practice_list_title"),
                                 locale_get("habit_practice_list_info"))) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
        }
        return;
    }

    if(app->modal.active && app->modal.type == UIModalHabitCountingInfo) {
        if(habit_edit_info_modal(locale_get("habit_counting_title"),
                                 locale_get("habit_counting_info"))) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
        }
        return;
    }

    if(app->modal.active && app->modal.type == UIModalConfirmDeleteHabit) {
        int modal_result = ui_draw_modal(locale_get("habit_delete_title"),
                                         locale_get("habit_delete_message"),
                                         locale_get("cancel_button"),
                                         locale_get("delete_button"));
        if(modal_result == 1) {
            app->modal.active = 0;
            app->modal.type = UIModalNone;
        } else if(modal_result == 2) {
            int index = app->habit_edit.index;
            app->modal.active = 0;
            app->modal.type = UIModalNone;
            habit_edit_cancel(app);
            if(index >= 0 && index < app->habits.count) {
                habits_delete(&app->habits, index);
                app_auto_sync(app);
            }
            app->inbe.screen = InbeScreenHabits;
        }
        return;
    }

    {
    int viewport_h = view_height - top_h - bottom_reserved;
    FlintUIScrollPage page;

    if(viewport_h < 0)
        viewport_h = 0;
    page = ui_scroll_page_begin((FlintUIScrollPageSpec){
        .y = top_h,
        .height = viewport_h,
        .max_content_width = max_w,
        .scroll_offset = &app->habits.scroll,
        .content_height = habit_edit_scroll_content_height,
        .user_data = app
    });
    content_x = page.content_x;
    content_w = page.content_w;
    y = page.content_y + flint_px(18);

    habit_edit_handle_keyboard(app);
    if(!app->habit_edit.active) {
        ui_scroll_page_end(page);
        return;
    }
    (void)ui_draw_label_text_field((FlintUILabelTextField){
        .label = locale_get("habit_name_label"),
        .field = habit_edit_text_field(app, font),
        .field_h = field_h,
        .label_font = label_font,
        .label_color = flint_darken(theme_get_text(), 34)
    }, content_x, y, content_w);
    habit_edit_clamp_cursor(app);
    y += ui_label_text_field_height((FlintUILabelTextField){.field_h = field_h});

    flint_text_draw(locale_get("habit_underline_label"), content_x, y,
                    label_font, flint_darken(theme_get_text(), 34));
    y += flint_px(32);
    color_options[0] = (Color){94, 166, 232, 255};
    color_options[1] = (Color){99, 196, 165, 255};
    color_options[2] = (Color){210, 180, 72, 255};
    color_options[3] = (Color){224, 124, 104, 255};
    color_options[4] = (Color){180, 132, 220, 255};
    color_options[5] = (Color){216, 116, 164, 255};
    for(int i = 0; i < 6; i++) {
        int cx = content_x + flint_px(18) + i * flint_px(42);
        int selected = app->habit_edit.color.r == color_options[i].r &&
                       app->habit_edit.color.g == color_options[i].g &&
                       app->habit_edit.color.b == color_options[i].b;
        if(habit_color_button(app, cx, y, color_options[i], selected))
            app->habit_edit.color = color_options[i];
    }
    y += flint_px(34);

    if(ui_draw_section_label((FlintUISectionLabel){
        .label = locale_get("habit_practice_list_title"),
        .info_button = 1,
        .color = flint_darken(theme_get_text(), 34)
    }, content_x, y)) {
        app->modal.active = 1;
        app->modal.type = UIModalHabitPracticeListInfo;
        app->modal.selected_button = 0;
    }
    y += ui_section_label_height((FlintUISectionLabel){0});
    for(int i = 0; i < EXERCISE_COUNT; i++) {
        int enabled = (app->habit_edit.sync_activity & habit_activity_mask_for(i)) != 0;
        if(ui_draw_checkbox_row((FlintUICheckboxRow){
            .label = activity_options[i],
            .value = &enabled
        }, content_x, y)) {
            if(enabled)
                app->habit_edit.sync_activity |= habit_activity_mask_for(i);
            else
                app->habit_edit.sync_activity &= ~habit_activity_mask_for(i);
            app->habit_edit.sync_mode = app->habit_edit.sync_activity != 0
                                            ? INBE_HABIT_SYNC_ACTIVITIES
                                            : INBE_HABIT_SYNC_NONE;
        }
        y += ui_checkbox_row_height((FlintUICheckboxRow){0});
    }

    y += flint_px(4);
    if(ui_draw_section_label((FlintUISectionLabel){
        .label = locale_get("habit_counting_title"),
        .info_button = 1,
        .color = flint_darken(theme_get_text(), 34)
    }, content_x, y)) {
        app->modal.active = 1;
        app->modal.type = UIModalHabitCountingInfo;
        app->modal.selected_button = 0;
    }
    y += ui_section_label_height((FlintUISectionLabel){0});
    if(ui_draw_checkbox_row((FlintUICheckboxRow){
        .label = locale_get("habit_multiple_counts_label"),
        .value = &app->habit_edit.counter_enabled
    }, content_x, y)) {
        app->habit_edit.counter_enabled = app->habit_edit.counter_enabled != 0;
    }
    y += ui_checkbox_row_height((FlintUICheckboxRow){0});

    y += flint_px(10);
    {
        FlintUIButtonRowItem actions[2];
        int action_count = app->habit_edit.is_new ? 1 : 2;
        int clicked_action;

        if(app->habit_edit.is_new) {
            actions[0] = (FlintUIButtonRowItem){
                .label = locale_get("save_button"),
                .style = UI_BUTTON_STYLE_PRIMARY
            };
        } else {
            actions[0] = (FlintUIButtonRowItem){
                .label = locale_get("habit_delete_button"),
                .style = UI_BUTTON_STYLE_DANGER
            };
            actions[1] = (FlintUIButtonRowItem){
                .label = locale_get("save_button"),
                .style = UI_BUTTON_STYLE_PRIMARY
            };
        }
        clicked_action = ui_draw_button_row((FlintUIButtonRow){
            .x = content_x,
            .y = y,
            .width = content_w,
            .height = flint_px(40),
            .items = actions,
            .count = action_count
        });
        if(!app->habit_edit.is_new && clicked_action == 0) {
            app->modal.active = 1;
            app->modal.type = UIModalConfirmDeleteHabit;
            app->modal.selected_button = 0;
        } else if((app->habit_edit.is_new && clicked_action == 0) ||
                  (!app->habit_edit.is_new && clicked_action == 1)) {
            ui_scroll_page_end(page);
            habit_edit_commit(app);
            return;
        }
    }

    ui_scroll_page_end(page);
    }
}
