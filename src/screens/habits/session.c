#include "habits.h"

static void
habit_session_begin_round_edit(InbeApp *app, const HabitLinkedEntry *entry, int round)
{
    if(app == NULL || entry == NULL || round < 0 || round >= entry->round_count)
        return;
    app->habit_session_edit.active = 1;
    app->habit_session_edit.kind = HABIT_SESSION_EDIT_ROUND;
    app->habit_session_edit.round = round;
    snprintf(app->habit_session_edit.path, sizeof(app->habit_session_edit.path),
             "%s", entry->path);
    snprintf(app->habit_session_edit.text, sizeof(app->habit_session_edit.text),
             "%d", entry->rounds[round]);
    app->habit_session_edit.cursor = (int)strlen(app->habit_session_edit.text);
}

static int
habit_session_text_filter(int codepoint, void *user_data)
{
    InbeApp *app = (InbeApp *)user_data;

    if(codepoint >= '0' && codepoint <= '9')
        return 1;
    (void)app;
    return 0;
}

static int
habit_session_delete_round(const HabitLinkedEntry *entry, int round)
{
    int rounds[MaxRounds];
    int out_count = 0;

    if(entry == NULL || round < 0 || round >= entry->round_count)
        return 0;
    if(entry->round_count <= 1)
        return data_delete_session(entry->path);

    for(int i = 0; i < entry->round_count; i++) {
        if(i != round)
            rounds[out_count++] = entry->rounds[i];
    }
    return data_replace_session(entry->path, rounds, out_count);
}

static int
habit_session_handle_physical_keyboard(InbeApp *app, const HabitLinkedEntry *entry)
{
    int commit_pressed = 0;

    if(app == NULL || entry == NULL || !app->habit_session_edit.active)
        return 0;

    if(IsKeyPressed(KEY_ESCAPE)) {
        habit_session_cancel_edit(app);
        return 1;
    }

    flint_ui_text_edit((FlintUITextEdit){
        .text = app->habit_session_edit.text,
        .text_size = sizeof(app->habit_session_edit.text),
        .cursor_position = &app->habit_session_edit.cursor,
        .max_codepoints = 3,
        .filter = habit_session_text_filter,
        .filter_user_data = app,
        .commit_pressed = &commit_pressed
    });

    if(commit_pressed)
        return habit_session_commit_edit(app, entry);

    return 0;
}

typedef struct HabitSessionScrollPageContext {
    InbeApp *app;
    HabitLinkedContext *linked;
    int y;
} HabitSessionScrollPageContext;

static int
habit_session_scroll_page_content_height(int content_w, void *user_data)
{
    HabitSessionScrollPageContext *ctx = user_data;

    return draw_habit_session_edit_content(ctx->app, ctx->linked,
                                           0, content_w, ctx->y, 0) -
           ctx->y;
}


/* Habit session edit screen */
void
draw_habit_session_edit_screen(InbeApp *app)
{
    HabitLinkedContext ctx;
    InbeHabit *habit;
    char date_text[32];
    int top_h = flint_px(58);
    int bottom_reserved;
    int keyboard_h;
    int viewport_h;
    int max_w = flint_px(400);
    int y = top_h + flint_px(14);
    FlintUIHeader header;

    if(app == NULL)
        return;
    bottom_reserved = app_content_bottom_reserved(app);
    if(app->habit_detail_index < 0 || app->habit_detail_index >= app->habits.count) {
        app->inbe.screen = InbeScreenHabits;
        return;
    }

    habit = &app->habits.items[app->habit_detail_index];
    habit_collect_linked_entries(habit, app->habit_detail_day, &ctx);
    if(ctx.count <= 0) {
        habit_session_cancel_edit(app);
        app->inbe.screen = InbeScreenHabits;
        return;
    }
    habit_format_date(app->habit_detail_day, date_text, sizeof(date_text));

    keyboard_h = habit_session_keyboard_height(app);
    viewport_h = view_height - top_h - bottom_reserved - keyboard_h;
    if(viewport_h < flint_px(80))
        viewport_h = flint_px(80);

    header = ui_draw_title_header(top_h, date_text,
                                  app->icons[UI_ICON_TYPE_RETURN], (Texture2D){0});
    if(header.left_clicked) {
        habit_session_cancel_edit(app);
        app->inbe.screen = InbeScreenHabits;
        return;
    }

    {
        HabitSessionScrollPageContext page_ctx = {app, &ctx, y};
        FlintUIScrollPage page = ui_scroll_page_begin((FlintUIScrollPageSpec){
            .y = y,
            .height = viewport_h,
            .max_content_width = max_w,
            .scroll_offset = &app->habit_session_edit.scroll,
            .content_height = habit_session_scroll_page_content_height,
            .user_data = &page_ctx
        });
        draw_habit_session_edit_content(app, &ctx, page.content_x, page.content_w,
                                        page.content_y, 1);
        ui_scroll_page_end(page);
    }

    if(app->habit_session_edit.active) {
        HabitLinkedEntry *active_entry = NULL;
        for(int i = 0; i < ctx.count; i++) {
            if(strcmp(app->habit_session_edit.path, ctx.entries[i].path) == 0) {
                active_entry = &ctx.entries[i];
                break;
            }
        }
        if(active_entry != NULL) {
            if(habit_session_handle_physical_keyboard(app, active_entry))
                return;
            if(habit_session_draw_keyboard(app, active_entry))
                return;
        }
    }
}

int
draw_habit_session_edit_content(InbeApp *app, HabitLinkedContext *ctx, int content_x, int content_w, int y, int draw)
{
    int row_h = flint_px(34);
    int section_gap = flint_px(10);
    int section_h = flint_px(28);
    int section_font = flint_ui_font();

    if(app == NULL || ctx == NULL)
        return y;

    if(ctx->count <= 0) {
        if(draw)
            flint_text_draw("No sessions", content_x, y, flint_ui_font(), theme_get_text());
        return y + row_h;
    }

    for(int activity = 0; activity < EXERCISE_COUNT; activity++) {
        int has_activity = 0;
        for(int i = 0; i < ctx->count; i++) {
            if(ctx->entries[i].activity == activity) {
                has_activity = 1;
                break;
            }
        }
        if(!has_activity)
            continue;

        if(draw) {
            flint_text_draw(practice_activity_label(activity), content_x, y,
                            section_font, flint_darken(theme_get_text(), 12));
        }
        y += section_h;

        for(int i = 0; i < ctx->count; i++) {
            char time_text[16];
            char summary_text[32];
            int icon_size = flint_px(18);
            int icon_padding = flint_px(6);
            int icon_w = icon_size + icon_padding * 2;
            int trash_x = content_x + content_w - icon_w;
            int row_font = flint_ui_font_small();
            int summary_x;
            int hover_trash = 0;

            if(ctx->entries[i].activity != activity)
                continue;

            snprintf(time_text, sizeof(time_text), "%02d:%02d",
                     ctx->entries[i].hour, ctx->entries[i].minute);
            if(activity == EXERCISE_MEDITATION)
                habit_format_duration(ctx->entries[i].total_seconds,
                                      summary_text, sizeof(summary_text));
            else
                locale_format(summary_text, sizeof(summary_text), "results_rounds",
                              ctx->entries[i].round_count);
            summary_x = content_x + flint_px(76);
            if(summary_x + flint_text_measure(summary_text, row_font) > trash_x - flint_px(8))
                summary_x = content_x + flint_px(62);

            if(draw) {
                flint_text_draw(time_text, content_x,
                                flint_ui_text_y(time_text, y, row_h, row_font),
                                row_font, theme_get_text());
                flint_text_draw(summary_text, summary_x,
                                flint_ui_text_y(summary_text, y, row_h, row_font),
                                row_font, flint_darken(theme_get_text(), 12));
                if(ui_draw_icon_btn_padded(trash_x, y - flint_px(4), icon_size, icon_padding,
                                           app->icons[UI_ICON_TYPE_TRASH], &hover_trash)) {
                    if(data_delete_session(ctx->entries[i].path))
                        habit_session_changed(app, ctx->count);
                    habit_session_cancel_edit(app);
                    return y;
                }
            }
            y += row_h;

            if(activity == EXERCISE_MEDITATION) {
                y += flint_px(4);
                continue;
            }

            for(int r = 0; r < ctx->entries[i].round_count; r++) {
                char round_line[64];
                int round_trash_x = content_x + content_w - icon_w;
                int round_edit_x = round_trash_x - icon_w - flint_px(4);
                int round_text_x = content_x + flint_px(16);
                int round_text_w = round_edit_x - round_text_x - flint_px(8);
                int editing_round = app->habit_session_edit.active &&
                                    app->habit_session_edit.kind == HABIT_SESSION_EDIT_ROUND &&
                                    app->habit_session_edit.round == r &&
                                    strcmp(app->habit_session_edit.path, ctx->entries[i].path) == 0;
                int hover_round_edit = 0;
                int hover_round_trash = 0;
                locale_format(round_line, sizeof(round_line), "round_result_label",
                              r + 1, ctx->entries[i].rounds[r]);
                if(draw) {
                    if(editing_round)
                        locale_format(round_line, sizeof(round_line), "round_result_label",
                                      r + 1, atoi(app->habit_session_edit.text));
                    if(round_text_w < flint_px(80))
                        round_text_w = flint_px(80);
                    flint_ui_draw_text_left_in_rect(
                        round_line,
                        (Rectangle){(float)round_text_x, (float)y,
                                    (float)round_text_w, (float)flint_px(24)},
                        row_font,
                        editing_round ? theme_get_button_hover()
                                      : theme_get_text());
                    if(ui_draw_icon_btn_padded(round_edit_x, y - flint_px(6),
                                               icon_size, icon_padding,
                                               app->icons[UI_ICON_TYPE_PENCIL], &hover_round_edit)) {
                        habit_session_begin_round_edit(app, &ctx->entries[i], r);
                        return y;
                    }
                    if(ui_draw_icon_btn_padded(round_trash_x, y - flint_px(6),
                                               icon_size, icon_padding,
                                               app->icons[UI_ICON_TYPE_TRASH], &hover_round_trash)) {
                        if(habit_session_delete_round(&ctx->entries[i], r))
                            habit_session_changed(app, ctx->count);
                        habit_session_cancel_edit(app);
                        return y;
                    }
                }
                y += flint_px(24);
            }
            y += flint_px(4);
        }
        y += section_gap;
    }

    if(ctx->day_filter > 0 &&
       app->habit_detail_index >= 0 &&
       app->habit_detail_index < app->habits.count) {
        InbeHabit *habit = &app->habits.items[app->habit_detail_index];

        if(habit_counting_enabled(habit)) {
            int minimum_count = ctx->count;
            int total_count = habit_effective_day_count(habit, ctx->day_filter, ctx);
            int button_h = flint_px(34);
            int step_w = flint_px(42);
            int gap = flint_px(8);
            int minus_x = content_x;
            int plus_x = content_x + content_w - step_w;
            int label_x = minus_x + step_w + gap;
            int label_w = plus_x - label_x - gap;
            int hover = 0;
            char total_text[64];

            y += flint_px(6);
            if(draw) {
                snprintf(total_text, sizeof(total_text), "Total count %d", total_count);
                if(ui_draw_generic_button(minus_x, y, step_w, button_h, "-",
                                          UI_BUTTON_STYLE_SECONDARY,
                                          total_count <= minimum_count, &hover)) {
                    habit_apply_count_action(app, app->habit_detail_index,
                                             ctx->day_filter, -1, minimum_count);
                    app_auto_sync(app);
                    return y;
                }
                DrawRectangle(label_x, y, label_w, button_h, flint_darken(theme_get_bg(), 5));
                flint_text_draw(total_text, label_x + flint_px(8),
                                flint_ui_text_y(total_text, y, button_h, flint_ui_font_small()),
                                flint_ui_font_small(), theme_get_text());
                if(ui_draw_generic_button(plus_x, y, step_w, button_h, "+",
                                          UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
                    habit_apply_count_action(app, app->habit_detail_index,
                                             ctx->day_filter, 1, minimum_count);
                    app_auto_sync(app);
                    return y;
                }
            }
            y += button_h + flint_px(8);
        }
    }

    return y;
}

/* Habit session keyboard functions */
int
habit_session_keyboard_height(InbeApp *app)
{
    int key_h = flint_px(48);
    int gap = flint_px(6);
    int pad = flint_px(10);

#if !(defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID))
    if(app == NULL || !app->on_screen_keyboard_enabled)
        return 0;
#endif
    if(app == NULL || !app->habit_session_edit.active)
        return 0;
    return pad * 2 + key_h * 4 + gap * 3;
}

int
habit_session_draw_keyboard(InbeApp *app, const HabitLinkedEntry *entry)
{
    const char *labels[12] = {
        "1", "2", "3",
        "4", "5", "6",
        "7", "8", "9",
        "DEL", "0", "OK"
    };
    int key_h = flint_px(48);
    int gap = flint_px(6);
    int pad = flint_px(10);
    int keyboard_h = habit_session_keyboard_height(app);
    int x = flint_page_side_padding();
    int y = view_height - keyboard_h;
    int w = view_width - x * 2;
    int key_w = (w - gap * 2) / 3;

#if !(defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID))
    if(app == NULL || !app->on_screen_keyboard_enabled)
        return 0;
#endif
    if(app == NULL || !app->habit_session_edit.active || keyboard_h <= 0)
        return 0;

    DrawRectangle(0, y, view_width, keyboard_h, flint_darken(theme_get_bg(), 10));
    DrawLine(0, y, view_width, y, flint_darken(theme_get_bg(), 42));

    for(int i = 0; i < 12; i++) {
        int col = i % 3;
        int row = i / 3;
        int key_x = x + col * (key_w + gap);
        int key_y = y + pad + row * (key_h + gap);
        if(habit_session_keyboard_key(key_x, key_y, key_w, key_h, labels[i])) {
            if(i == 9) {
                habit_session_delete_before_cursor(app);
            } else if(i == 11) {
                if(habit_session_commit_edit(app, entry))
                    return 1;
            } else {
                habit_session_insert_char(app, labels[i][0]);
            }
        }
    }

    return 0;
}

/* Habit session helper functions */
int
habit_session_keyboard_key(int x, int y, int w, int h, const char *label)
{
    int hover = 0;
    return ui_draw_generic_button(x, y, w, h, label, UI_BUTTON_STYLE_SECONDARY, 0, &hover);
}

void
habit_session_clamp_cursor(InbeApp *app)
{
    int len;

    if(app == NULL)
        return;
    len = (int)strlen(app->habit_session_edit.text);
    if(app->habit_session_edit.cursor < 0)
        app->habit_session_edit.cursor = 0;
    if(app->habit_session_edit.cursor > len)
        app->habit_session_edit.cursor = len;
}

void
habit_session_delete_before_cursor(InbeApp *app)
{
    size_t len;
    int cursor;

    if(app == NULL)
        return;
    habit_session_clamp_cursor(app);
    len = strlen(app->habit_session_edit.text);
    cursor = app->habit_session_edit.cursor;
    if(cursor <= 0 || len == 0)
        return;
    memmove(app->habit_session_edit.text + cursor - 1,
            app->habit_session_edit.text + cursor,
            len - (size_t)cursor + 1);
    app->habit_session_edit.cursor--;
}

void
habit_session_insert_char(InbeApp *app, char c)
{
    size_t len;
    int cursor;

    if(app == NULL)
        return;
    habit_session_clamp_cursor(app);
    len = strlen(app->habit_session_edit.text);
    cursor = app->habit_session_edit.cursor;

    if(len < 3) {
        memmove(app->habit_session_edit.text + cursor + 1,
                app->habit_session_edit.text + cursor,
                len - (size_t)cursor + 1);
        app->habit_session_edit.text[cursor] = c;
        app->habit_session_edit.cursor = cursor + 1;
        return;
    }

    if(cursor < (int)len) {
        app->habit_session_edit.text[cursor] = c;
        app->habit_session_edit.cursor = cursor + 1;
    }
}

int
habit_session_parse_seconds(const char *text, int *seconds)
{
    int value;
    char tail;

    if(text == NULL || sscanf(text, "%d%c", &value, &tail) != 1)
        return 0;
    if(value <= 0 || value > 999)
        return 0;
    if(seconds != NULL)
        *seconds = value;
    return 1;
}

int
habit_session_commit_edit(InbeApp *app, const HabitLinkedEntry *entry)
{
    if(app == NULL || entry == NULL || !app->habit_session_edit.active)
        return 0;

    if(app->habit_session_edit.kind == HABIT_SESSION_EDIT_ROUND) {
        int seconds;
        int rounds[MaxRounds];

        if(app->habit_session_edit.round < 0 ||
           app->habit_session_edit.round >= entry->round_count)
            return 0;
        if(!habit_session_parse_seconds(app->habit_session_edit.text, &seconds))
            return 0;
        for(int i = 0; i < entry->round_count; i++)
            rounds[i] = entry->rounds[i];
        rounds[app->habit_session_edit.round] = seconds;
        if(!data_replace_session(entry->path, rounds, entry->round_count))
            return 0;
        habit_session_changed(app, -1);
        habit_session_cancel_edit(app);
        return 1;
    }

    return 0;
}
