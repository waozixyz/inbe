#include "habits.h"

/* Habit edit functions */
void
habit_edit_begin_new(InbeApp *app)
{
    if(app == NULL)
        return;

    app->habit_edit = (HabitEditState){
        .active = 1,
        .is_new = 1,
        .index = -1,
        .color = {99, 196, 165, 255},
        .sync_mode = INBE_HABIT_SYNC_NONE
    };
    snprintf(app->habit_edit.text, sizeof(app->habit_edit.text), "%s", locale_get("habit_new_default_name"));
    app->habit_edit.cursor = (int)strlen(app->habit_edit.text);
    app->inbe.screen = InbeScreenHabitEdit;
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
    app->inbe.screen = InbeScreenHabitEdit;
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
habit_edit_section_label(int x, int y, const char *label)
{
    int font = flint_ui_font_small();
    int label_w = flint_text_measure(label, font);
    int icon_d = flint_px(18);

    flint_text_draw(label, x, y, font, flint_darken(theme_get_text(), 34));
    return ui_draw_info_button(x + label_w + flint_px(16),
                               y + font / 2 + flint_px(1), icon_d);
}

static int
habit_edit_info_modal(const char *title, const char *message)
{
    FlintUIPanelFrame frame;
    int y;
    int button_w = flint_px(112);
    int button_h = flint_px(36);
    int hover = 0;
    int result = 0;

    frame = ui_draw_modal_frame(flint_px(320), flint_px(196), title,
                                (Texture2D){0}, (Texture2D){0});
    y = frame.content_y;
    flint_ui_paragraph_draw((FlintUIParagraph){
        .text = message,
        .width = frame.content_w,
        .font = flint_ui_font(),
        .line_gap = flint_px(4),
        .color = theme_get_text()
    }, frame.content_x, &y);

    if(ui_draw_generic_button(frame.x + (frame.w - button_w) / 2,
                              frame.y + frame.h - button_h - flint_px(16),
                              button_w, button_h, locale_get("ok_button"),
                              UI_BUTTON_STYLE_PRIMARY, 0, &hover))
        result = 1;
    if(frame.right_clicked)
        result = 1;
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

static void
draw_habit_edit_field(InbeApp *app, int x, int y, int w, int h, int font)
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

    if(app == NULL)
        return;

    flint_ui_text_field((FlintUITextField){
        .bounds = {(float)x, (float)y, (float)w, (float)h},
        .text = app->habit_edit.text,
        .text_size = sizeof(app->habit_edit.text),
        .cursor_position = &app->habit_edit.cursor,
        .focused = &app->habit_edit.focused,
        .max_codepoints = INBE_HABIT_NAME_SIZE - 1,
        .font = font,
        .style = style
    });
    habit_edit_clamp_cursor(app);
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
        app->inbe.screen = InbeScreenHabits;
        return;
    }

    title = app->habit_edit.is_new ? locale_get("habit_new_title") : locale_get("habit_edit_title");

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

    flint_centered_column(max_w, flint_page_side_padding(), &content_x, &content_w);

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

    flint_clip_begin((int)app->camera.offset.x,
                     (int)(app->camera.offset.y + top_h * app->camera.zoom),
                     (int)(view_width * app->camera.zoom),
                     (int)((view_height - top_h - bottom_reserved) * app->camera.zoom));

    flint_text_draw(locale_get("habit_name_label"), content_x, y, label_font, flint_darken(theme_get_text(), 34));
    y += flint_px(22);
    habit_edit_handle_keyboard(app);
    if(!app->habit_edit.active) {
        flint_clip_end();
        return;
    }
    draw_habit_edit_field(app, content_x, y, content_w, field_h, font);
    y += field_h + flint_px(24);

    flint_text_draw(locale_get("habit_underline_label"), content_x, y, label_font, flint_darken(theme_get_text(), 34));
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

    if(habit_edit_section_label(content_x, y, locale_get("habit_practice_list_title"))) {
        app->modal.active = 1;
        app->modal.type = UIModalHabitPracticeListInfo;
        app->modal.selected_button = 0;
    }
    y += flint_px(24);
    for(int i = 0; i < EXERCISE_COUNT; i++) {
        int enabled = (app->habit_edit.sync_activity & habit_activity_mask_for(i)) != 0;
        if(ui_draw_checkbox_toggle(content_x, y, activity_options[i], &enabled)) {
            if(enabled)
                app->habit_edit.sync_activity |= habit_activity_mask_for(i);
            else
                app->habit_edit.sync_activity &= ~habit_activity_mask_for(i);
            app->habit_edit.sync_mode = app->habit_edit.sync_activity != 0
                                            ? INBE_HABIT_SYNC_ACTIVITIES
                                            : INBE_HABIT_SYNC_NONE;
        }
        y += flint_px(42);
    }

    y += flint_px(4);
    if(habit_edit_section_label(content_x, y, locale_get("habit_counting_title"))) {
        app->modal.active = 1;
        app->modal.type = UIModalHabitCountingInfo;
        app->modal.selected_button = 0;
    }
    y += flint_px(24);
    if(ui_draw_checkbox_toggle(content_x, y, locale_get("habit_multiple_counts_label"),
                               &app->habit_edit.counter_enabled)) {
        app->habit_edit.counter_enabled = app->habit_edit.counter_enabled != 0;
    }
    y += flint_px(42);

    if(!app->habit_edit.is_new) {
        int delete_w = flint_px(160);
        int delete_h = flint_px(38);
        int hover_delete = 0;
        y += flint_px(10);
        if(ui_draw_generic_button(content_x, y, delete_w, delete_h,
                                  locale_get("habit_delete_button"), UI_BUTTON_STYLE_DANGER,
                                  0, &hover_delete)) {
            app->modal.active = 1;
            app->modal.type = UIModalConfirmDeleteHabit;
            app->modal.selected_button = 0;
        }
    }

    flint_clip_end();
}
