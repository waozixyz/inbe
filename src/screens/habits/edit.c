#include "habits.h"

/* Habit edit functions */
void
habit_edit_begin_new(InbeApp *app)
{
    const char *default_name;
    Color default_color = {99, 196, 165, 255};

    if(app == NULL)
        return;

    default_name = GetLocaleText("habit_new_default_name");
    app->habit_edit = (HabitEditState){
        .active = 1,
        .is_new = 1,
        .index = -1,
        .color = default_color,
        .sync_mode = INBE_HABIT_SYNC_NONE
    };
    snprintf(app->habit_edit.text, sizeof(app->habit_edit.text), "%s", default_name);
    app->habit_edit.cursor = (int)strlen(app->habit_edit.text);
    app->habit_edit.focused = 1;
    app->habits.tab = HABIT_TAB_EDIT;
    app_switch_screen(app, InbeScreenHabits);
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
    snprintf(app->habit_edit.description, sizeof(app->habit_edit.description), "%s",
             app->habits.items[index].description);
    app->habit_edit.cursor = (int)strlen(app->habit_edit.text);
    app->habit_edit.description_cursor = (int)strlen(app->habit_edit.description);
    app->habits.tab = HABIT_TAB_EDIT;
    app_switch_screen(app, InbeScreenHabits);
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
    SetUIFocusTextInputActive(0);
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
    UIPanelFrame frame;
    UIParagraph paragraph;
    UIScrollView scroll_view;
    UIScrollArea scroll_area;
    int y;
    int modal_h;
    int modal_w;
    int paragraph_h;
    int button_w = ScaleUIPx(112);
    int button_h = ScaleUIPx(36);
    int button_y;
    int text_h;
    int hover = 0;
    int result = 0;

    modal_w = view_width - ScaleUIPx(36);
    if(modal_w > ScaleUIPx(420))
        modal_w = ScaleUIPx(420);
    if(modal_w < ScaleUIPx(320))
        modal_w = ScaleUIPx(320);
    if(modal_w > view_width - ScaleUIPx(24))
        modal_w = view_width - ScaleUIPx(24);
    if(modal_w < ScaleUIPx(240))
        modal_w = ScaleUIPx(240);

    paragraph = (UIParagraph){
        .text = message,
        .width = modal_w - ScaleUIPx(36),
        .font = GetUIFontSize(),
        .line_gap = ScaleUIPx(4)
    };
    modal_h = GetUIParagraphModalHeight((UIParagraphModalMeasure){
        .message = message,
        .width = modal_w,
        .button_h = button_h,
        .extra_lines = 2,
        .min_height = ScaleUIPx(196)
    });

    frame = DrawUIModalFrame(modal_w, modal_h, title,
                                (Texture2D){0}, (Texture2D){0});
    paragraph.width = frame.content_w;
    paragraph_h = GetUIParagraphHeight(paragraph);
    button_y = frame.y + frame.h - button_h - ScaleUIPx(16);
    text_h = button_y - frame.content_y - ScaleUIPx(16);
    if(text_h < ScaleUIPx(32))
        text_h = ScaleUIPx(32);

    scroll_area = (UIScrollArea){
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
    scroll_view = BeginUIScrollContainer(scroll_area);
    y = scroll_view.content_y;
    DrawUIParagraph(paragraph, scroll_view.content_x, &y);
    EndUIScrollContainer(scroll_area, scroll_view);

    if(DrawUIGenericButton(frame.x + (frame.w - button_w) / 2,
                              button_y,
                              button_w, button_h, GetLocaleText("ok_button"),
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
                snprintf(app->habits.items[created].description,
                         sizeof(app->habits.items[created].description), "%s",
                         app->habit_edit.description);
                habits_save(&app->habits);
            }
        } else {
            // Check for duplicate names when editing existing habit
            if(habits_name_exists(&app->habits, text, index)) {
                // Show error modal or prevent the update
                // For now, just return without saving
                habit_edit_cancel(app);
                return;
            }
            snprintf(app->habits.items[index].name,
                     sizeof(app->habits.items[index].name), "%s", text);
            snprintf(app->habits.items[index].description,
                     sizeof(app->habits.items[index].description), "%s",
                     app->habit_edit.description);
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
    app_switch_screen(app, InbeScreenHabits);
}

static void
habit_edit_clamp_cursor(char *text, int *cursor)
{
    int len;

    if(text == NULL || cursor == NULL)
        return;

    len = (int)strlen(text);
    if(*cursor < 0)
        *cursor = 0;
    if(*cursor > len)
        *cursor = len;
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

static UITextField
habit_edit_text_field(char *text, size_t text_size, int *cursor, int *focused,
                      int max_codepoints, int font)
{
    UITextInputStyle style = {
        .background = DarkenUIColor(GetThemeBackground(), 4),
        .border = GetThemeButton(),
        .focus_border = GetThemeButtonHover(),
        .text = GetThemeText(),
        .cursor = GetThemeText(),
        .radius = 0.08f,
        .padding_x = ScaleUIPx(10)
    };

    return (UITextField){
        .bounds = {0},
        .text = text,
        .text_size = text_size,
        .cursor_position = cursor,
        .focused = focused,
        .max_codepoints = max_codepoints,
        .font = font,
        .style = style
    };
}


static int
habit_color_button(InbeApp *app, int x, int y, Color color, int selected)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int radius = ScaleUIPx(13);
    Rectangle bounds = {
        (float)(x - radius - ScaleUIPx(6)),
        (float)(y - radius - ScaleUIPx(6)),
        (float)(radius * 2 + ScaleUIPx(12)),
        (float)(radius * 2 + ScaleUIPx(12))
    };
    int active = CheckCollisionPointRec(mouse_world, bounds);
    int hovered = active && UIHoverEffectsEnabled();

    DrawCircle(x, y, radius, color);
    DrawCircleLines(x, y, radius + ScaleUIPx(2),
                    selected ? GetThemeText() : DarkenUIColor(GetThemeBackground(), 42));
    if(active) {
        MarkUIClickable();
        if(hovered)
            DrawCircleLines(x, y, radius + ScaleUIPx(5), GetThemeButtonHover());
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            return 1;
    }
    return 0;
}

static int
habit_edit_scroll_content_height(int content_w, void *user_data)
{
    InbeApp *app = user_data;
    int height = ScaleUIPx(18);

    (void)content_w;
    height += GetUILabelTextFieldHeight((UILabelTextField){
        .label = GetLocaleText("habit_name_label"),
        .field_h = ScaleUIPx(40)
    });
    height += GetUILabelTextFieldHeight((UILabelTextField){
        .label = GetLocaleText("habit_description_label"),
        .field_h = ScaleUIPx(40)
    });
    height += ScaleUIPx(32) + ScaleUIPx(76);
    height += GetUISectionLabelHeight((UISectionLabel){0});
    for(int i = 0; i < EXERCISE_COUNT; i++)
        height += GetUICheckboxRowHeight((UICheckboxRow){0});
    height += ScaleUIPx(4);
    height += GetUISectionLabelHeight((UISectionLabel){0});
    height += GetUICheckboxRowHeight((UICheckboxRow){0});
    height += ScaleUIPx(10);
    height += GetUIButtonRowHeight((UIButtonRow){.height = ScaleUIPx(40)});
    height += ScaleUIPx(24);
    (void)app;
    return height;
}

void
draw_habit_edit_screen(InbeApp *app)
{
    const char *title;
    const char *activity_options[EXERCISE_COUNT];
    Color color_options[12];
    int top_h = ScaleUIPx(58);
    int bottom_reserved;
    int content_x;
    int content_w;
    int max_w = ScaleUIPx(CONTENT_MAX_W);
    int y = top_h + ScaleUIPx(18);
    int font = GetUIFontSize();
    int label_font = GetUISmallFontSize();
    int field_h = ScaleUIPx(40);
    int hover = 0;
    int title_font;
    int title_w;

    if(app == NULL)
        return;
    bottom_reserved = app_content_bottom_reserved(app);
    for(int i = 0; i < EXERCISE_COUNT; i++)
        activity_options[i] = practice_activity_label(i);

    if(!app->habit_edit.active) {
        app->habits.tab = app->habits.view_mode == HABIT_VIEW_WEEKLY
                              ? HABIT_TAB_WEEKLY
                              : HABIT_TAB_MONTHLY;
        app_switch_screen(app, InbeScreenHabits);
        return;
    }

    title = app->habit_edit.is_new ? GetLocaleText("habit_new_title") : GetLocaleText("habit_edit_title");

    if(app->inbe.screen == InbeScreenHabitEdit) {
        DrawRectangle(0, 0, view_width, top_h, GetThemeBackground());
        DrawLine(0, top_h - 1, view_width, top_h - 1, DarkenUIColor(GetThemeButton(), 18));
        if(!app->modal.active &&
           DrawUIPaddedIconBtn(view_width - ScaleUIPx(52), ScaleUIPx(12),
                                   ScaleUIPx(24), ScaleUIPx(8),
                                   app->icons[UI_ICON_TYPE_X], &hover)) {
            habit_edit_cancel(app);
            app_switch_screen(app, InbeScreenHabits);
            return;
        }
        title_font = GetUITitleFontSize(title, view_width - ScaleUIPx(120));
        title_w = MeasureUIText(title, title_font);
        DrawUIText(title, (view_width - title_w) / 2,
                        GetUIControlTextY(title, 0, top_h, title_font),
                        title_font, GetThemeText());
        if(!app->modal.active &&
           DrawUIPaddedIconBtn(view_width - ScaleUIPx(100), ScaleUIPx(12),
                                   ScaleUIPx(24), ScaleUIPx(8), app->icons[UI_ICON_TYPE_CHECK], &hover)) {
            habit_edit_commit(app);
            return;
        }
    } else {
        top_h = habits_screen_top_reserved(app);
        y = top_h + ScaleUIPx(18);
    }

    if(app->modal.active && app->modal.type == UIModalHabitPracticeListInfo) {
        if(habit_edit_info_modal(GetLocaleText("habit_practice_list_title"),
                                 GetLocaleText("habit_practice_list_info"))) {
            app_close_modal(app);
        }
        return;
    }

    if(app->modal.active && app->modal.type == UIModalHabitCountingInfo) {
        if(habit_edit_info_modal(GetLocaleText("habit_counting_title"),
                                 GetLocaleText("habit_counting_info"))) {
            app_close_modal(app);
        }
        return;
    }

    if(app->modal.active && app->modal.type == UIModalConfirmDeleteHabit) {
        int modal_result = DrawUIModal(GetLocaleText("habit_delete_title"),
                                         GetLocaleText("habit_delete_message"),
                                         GetLocaleText("cancel_button"),
                                         GetLocaleText("delete_button"));
        if(modal_result == 1) {
            app_close_modal(app);
        } else if(modal_result == 2) {
            int index = app->habit_edit.index;
            app_close_modal(app);
            habit_edit_cancel(app);
            if(index >= 0 && index < app->habits.count) {
                habits_delete(&app->habits, index);
                app_auto_sync(app);
            }
            app->habits.screen_mode = HABITS_SCREEN_OVERVIEW;
            app->habits.scroll = 0;
            save_settings(app);
            app_switch_screen(app, InbeScreenHabits);
        }
        return;
    }

    {
    int viewport_h = view_height - top_h - bottom_reserved;
    UIScrollPage page;

    if(viewport_h < 0)
        viewport_h = 0;
    page = BeginUIScrollPage((UIScrollPageSpec){
        .y = top_h,
        .height = viewport_h,
        .max_content_width = max_w,
        .scroll_offset = &app->habits.scroll,
        .content_height = habit_edit_scroll_content_height,
        .user_data = app
    });
    content_x = page.content_x;
    content_w = page.content_w;
    y = page.content_y + ScaleUIPx(18);

    habit_edit_handle_keyboard(app);
    if(!app->habit_edit.active) {
        EndUIScrollPage(page);
        return;
    }
    (void)DrawUILabelTextField((UILabelTextField){
        .label = GetLocaleText("habit_name_label"),
        .field = habit_edit_text_field(app->habit_edit.text,
                                       sizeof(app->habit_edit.text),
                                       &app->habit_edit.cursor,
                                       &app->habit_edit.focused,
                                       INBE_HABIT_NAME_SIZE - 1, font),
        .field_h = field_h,
        .label_font = label_font,
        .label_color = DarkenUIColor(GetThemeText(), 34)
    }, content_x, y, content_w);
    habit_edit_clamp_cursor(app->habit_edit.text, &app->habit_edit.cursor);
    y += GetUILabelTextFieldHeight((UILabelTextField){.field_h = field_h});

    (void)DrawUILabelTextField((UILabelTextField){
        .label = GetLocaleText("habit_description_label"),
        .field = habit_edit_text_field(app->habit_edit.description,
                                       sizeof(app->habit_edit.description),
                                       &app->habit_edit.description_cursor,
                                       &app->habit_edit.description_focused,
                                       INBE_HABIT_DESCRIPTION_SIZE - 1, font),
        .field_h = field_h,
        .label_font = label_font,
        .label_color = DarkenUIColor(GetThemeText(), 34)
    }, content_x, y, content_w);
    habit_edit_clamp_cursor(app->habit_edit.description,
                            &app->habit_edit.description_cursor);
    y += GetUILabelTextFieldHeight((UILabelTextField){.field_h = field_h});

    DrawUIText(GetLocaleText("habit_underline_label"), content_x, y,
                    label_font, DarkenUIColor(GetThemeText(), 34));
    y += ScaleUIPx(32);
    color_options[0] = (Color){224, 92, 92, 255};
    color_options[1] = (Color){224, 124, 104, 255};
    color_options[2] = (Color){232, 150, 74, 255};
    color_options[3] = (Color){210, 180, 72, 255};
    color_options[4] = (Color){160, 196, 82, 255};
    color_options[5] = (Color){99, 196, 165, 255};
    color_options[6] = (Color){74, 186, 198, 255};
    color_options[7] = (Color){94, 166, 232, 255};
    color_options[8] = (Color){116, 132, 224, 255};
    color_options[9] = (Color){180, 132, 220, 255};
    color_options[10] = (Color){216, 116, 164, 255};
    color_options[11] = (Color){198, 96, 118, 255};
    for(int i = 0; i < 12; i++) {
        int row = i / 6;
        int col = i % 6;
        int cx = content_x + ScaleUIPx(18) + col * ScaleUIPx(42);
        int cy = y + row * ScaleUIPx(42);
        int selected = app->habit_edit.color.r == color_options[i].r &&
                       app->habit_edit.color.g == color_options[i].g &&
                       app->habit_edit.color.b == color_options[i].b;
        if(habit_color_button(app, cx, cy, color_options[i], selected))
            app->habit_edit.color = color_options[i];
    }
    y += ScaleUIPx(76);

    if(DrawUISectionLabel((UISectionLabel){
        .label = GetLocaleText("habit_practice_list_title"),
        .info_button = 1,
        .color = DarkenUIColor(GetThemeText(), 34)
    }, content_x, y)) {
        app_open_modal(app, UIModalHabitPracticeListInfo);
    }
    y += GetUISectionLabelHeight((UISectionLabel){0});
    for(int i = 0; i < EXERCISE_COUNT; i++) {
        int enabled = (app->habit_edit.sync_activity & habit_activity_mask_for(i)) != 0;
        if(DrawUICheckboxRow((UICheckboxRow){
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
        y += GetUICheckboxRowHeight((UICheckboxRow){0});
    }

    y += ScaleUIPx(4);
    if(DrawUISectionLabel((UISectionLabel){
        .label = GetLocaleText("habit_counting_title"),
        .info_button = 1,
        .color = DarkenUIColor(GetThemeText(), 34)
    }, content_x, y)) {
        app_open_modal(app, UIModalHabitCountingInfo);
    }
    y += GetUISectionLabelHeight((UISectionLabel){0});
    if(DrawUICheckboxRow((UICheckboxRow){
        .label = GetLocaleText("habit_multiple_counts_label"),
        .value = &app->habit_edit.counter_enabled
    }, content_x, y)) {
        app->habit_edit.counter_enabled = app->habit_edit.counter_enabled != 0;
    }
    y += GetUICheckboxRowHeight((UICheckboxRow){0});

    y += ScaleUIPx(10);
    {
        UIButtonRowItem actions[2];
        int action_count = app->habit_edit.is_new ? 1 : 2;
        int clicked_action;

        if(app->habit_edit.is_new) {
            actions[0] = (UIButtonRowItem){
                .label = GetLocaleText("save_button"),
                .style = UI_BUTTON_STYLE_PRIMARY
            };
        } else {
            actions[0] = (UIButtonRowItem){
                .label = GetLocaleText("habit_delete_button"),
                .style = UI_BUTTON_STYLE_DANGER
            };
            actions[1] = (UIButtonRowItem){
                .label = GetLocaleText("save_button"),
                .style = UI_BUTTON_STYLE_PRIMARY
            };
        }
        clicked_action = DrawUIButtonRow((UIButtonRow){
            .x = content_x,
            .y = y,
            .width = content_w,
            .height = ScaleUIPx(40),
            .items = actions,
            .count = action_count
        });
        if(!app->habit_edit.is_new && clicked_action == 0) {
            app_open_modal(app, UIModalConfirmDeleteHabit);
        } else if((app->habit_edit.is_new && clicked_action == 0) ||
                  (!app->habit_edit.is_new && clicked_action == 1)) {
            EndUIScrollPage(page);
            habit_edit_commit(app);
            return;
        }
    }

    EndUIScrollPage(page);
    }
}
