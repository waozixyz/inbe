#include "habits.h"
#include "platform.h"

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

static const char *
habit_session_last_substring(const char *text, const char *needle)
{
    const char *match = NULL;
    const char *cursor;
    size_t needle_len;

    if(text == NULL || needle == NULL || needle[0] == '\0')
        return NULL;
    needle_len = strlen(needle);
    for(cursor = text; (cursor = strstr(cursor, needle)) != NULL; cursor += needle_len)
        match = cursor;
    return match;
}

static void
habit_session_draw_round_line(const char *line, const char *seconds_text,
                              int x, int y, int w, int h, int font,
                              int editing)
{
    const char *value_at;
    const char *suffix;
    char prefix[64];
    size_t prefix_len;
    int text_y;
    int draw_x;

    if(!editing) {
        DrawLeftUIControlTextInRect(line,
                                        (Rectangle){(float)x, (float)y, (float)w, (float)h},
                                        font, GetThemeText());
        return;
    }

    value_at = habit_session_last_substring(line, seconds_text);
    if(value_at == NULL) {
        DrawLeftUIControlTextInRect(line,
                                        (Rectangle){(float)x, (float)y, (float)w, (float)h},
                                        font, GetThemeText());
        return;
    }

    prefix_len = (size_t)(value_at - line);
    if(prefix_len >= sizeof(prefix))
        prefix_len = sizeof(prefix) - 1;
    memcpy(prefix, line, prefix_len);
    prefix[prefix_len] = '\0';
    suffix = value_at + strlen(seconds_text);
    text_y = GetUIControlTextY(line, y, h, font);
    draw_x = x;

    DrawUIText(prefix, draw_x, text_y, font, GetThemeText());
    draw_x += MeasureUIText(prefix, font);
    DrawUIText(seconds_text, draw_x, text_y, font, GetThemeButtonHover());
    draw_x += MeasureUIText(seconds_text, font);
    DrawUIText(suffix, draw_x, text_y, font, GetThemeText());
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

    EditUIText((UITextEdit){
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
    int top_h = ScaleUIPx(58);
    int bottom_reserved;
    int keyboard_h;
    int viewport_h;
    int max_w = ScaleUIPx(400);
    int y = top_h + ScaleUIPx(14);

    if(app == NULL)
        return;
    bottom_reserved = app_content_bottom_reserved(app);
    if(app->habit_detail_index < 0 || app->habit_detail_index >= app->habits.count) {
        app_switch_screen(app, InbeScreenHabits);
        return;
    }

    habit = &app->habits.items[app->habit_detail_index];
    habit_collect_linked_entries(habit, app->habit_detail_day, &ctx);
    if(ctx.count <= 0) {
        habit_session_cancel_edit(app);
        app_switch_screen(app, InbeScreenHabits);
        return;
    }
    habit_format_date(app->habit_detail_day, date_text, sizeof(date_text));

    keyboard_h = habit_session_keyboard_height(app);
    viewport_h = view_height - top_h - bottom_reserved - keyboard_h;
    if(viewport_h < ScaleUIPx(80))
        viewport_h = ScaleUIPx(80);

    if(app_draw_close_title_bar(app, date_text, top_h)) {
        habit_session_cancel_edit(app);
        app_switch_screen(app, InbeScreenHabits);
        return;
    }

    {
        HabitSessionScrollPageContext page_ctx = {app, &ctx, y};
        UIScrollPage page = BeginUIScrollPage((UIScrollPageSpec){
            .y = y,
            .height = viewport_h,
            .max_content_width = max_w,
            .scroll_offset = &app->habit_session_edit.scroll,
            .content_height = habit_session_scroll_page_content_height,
            .user_data = &page_ctx
        });
        draw_habit_session_edit_content(app, &ctx, page.content_x, page.content_w,
                                        page.content_y, 1);
        EndUIScrollPage(page);
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
    int row_h = ScaleUIPx(34);
    int section_gap = ScaleUIPx(10);
    int section_h = ScaleUIPx(28);
    int section_font = GetUIFontSize();

    if(app == NULL || ctx == NULL)
        return y;

    if(ctx->count <= 0) {
        if(draw)
            DrawUIText(GetLocaleText("no_sessions"), content_x, y,
                            GetUIFontSize(), GetThemeText());
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
            DrawUIText(practice_activity_label(activity), content_x, y,
                            section_font, DarkenUIColor(GetThemeText(), 12));
        }
        y += section_h;

        for(int i = 0; i < ctx->count; i++) {
            char time_text[16];
            char summary_text[32];
            int icon_size = ScaleUIPx(18);
            int icon_padding = ScaleUIPx(6);
            int icon_w = icon_size + icon_padding * 2;
            int trash_x = content_x + content_w - icon_w;
            int row_font = GetUISmallFontSize();
            int summary_x;
            int hover_trash = 0;

            if(ctx->entries[i].activity != activity)
                continue;

            snprintf(time_text, sizeof(time_text), "%02d:%02d",
                     ctx->entries[i].hour, ctx->entries[i].minute);
            if(activity == EXERCISE_SUN_SALUTATION)
                FormatLocaleText(summary_text, sizeof(summary_text),
                              "practice_home_repetitions",
                              ctx->entries[i].round_count > 0
                                  ? ctx->entries[i].rounds[0]
                                  : 1);
            else if(activity == EXERCISE_MEDITATION)
                habit_format_duration(ctx->entries[i].total_seconds,
                                      summary_text, sizeof(summary_text));
            else
                FormatLocaleText(summary_text, sizeof(summary_text), "results_rounds",
                              ctx->entries[i].round_count);
            summary_x = content_x + ScaleUIPx(76);
            if(summary_x + MeasureUIText(summary_text, row_font) > trash_x - ScaleUIPx(8))
                summary_x = content_x + ScaleUIPx(62);

            if(draw) {
                DrawUIText(time_text, content_x,
                                GetUIControlTextY(time_text, y, row_h, row_font),
                                row_font, GetThemeText());
                DrawUIText(summary_text, summary_x,
                                GetUIControlTextY(summary_text, y, row_h, row_font),
                                row_font, DarkenUIColor(GetThemeText(), 12));
                if(DrawUIPaddedIconBtn(trash_x, y - ScaleUIPx(4), icon_size, icon_padding,
                                           app->icons[UI_ICON_TYPE_TRASH], &hover_trash)) {
                    if(data_delete_session(ctx->entries[i].path))
                        habit_session_changed(app, ctx->count);
                    habit_session_cancel_edit(app);
                    return y;
                }
            }
            y += row_h;

            if(activity == EXERCISE_MEDITATION ||
               activity == EXERCISE_SUN_SALUTATION) {
                y += ScaleUIPx(4);
                continue;
            }

            for(int r = 0; r < ctx->entries[i].round_count; r++) {
                char round_line[64];
                char round_seconds[16];
                int round_trash_x = content_x + content_w - icon_w;
                int round_edit_x = round_trash_x - icon_w - ScaleUIPx(4);
                int round_text_x = content_x + ScaleUIPx(16);
                int round_text_w = round_edit_x - round_text_x - ScaleUIPx(8);
                int editing_round = app->habit_session_edit.active &&
                                    app->habit_session_edit.kind == HABIT_SESSION_EDIT_ROUND &&
                                    app->habit_session_edit.round == r &&
                                    strcmp(app->habit_session_edit.path, ctx->entries[i].path) == 0;
                int hover_round_edit = 0;
                int hover_round_trash = 0;
                FormatLocaleText(round_line, sizeof(round_line), "round_result_label",
                              r + 1, ctx->entries[i].rounds[r]);
                if(draw) {
                    if(editing_round)
                        FormatLocaleText(round_line, sizeof(round_line), "round_result_label",
                                      r + 1, atoi(app->habit_session_edit.text));
                    snprintf(round_seconds, sizeof(round_seconds), "%d",
                             editing_round ? atoi(app->habit_session_edit.text)
                                           : ctx->entries[i].rounds[r]);
                    if(round_text_w < ScaleUIPx(80))
                        round_text_w = ScaleUIPx(80);
                    habit_session_draw_round_line(round_line, round_seconds,
                                                  round_text_x, y, round_text_w,
                                                  ScaleUIPx(24), row_font,
                                                  editing_round);
                    if(DrawUIPaddedIconBtn(round_edit_x, y - ScaleUIPx(6),
                                               icon_size, icon_padding,
                                               app->icons[editing_round ? UI_ICON_TYPE_SAVE
                                                                        : UI_ICON_TYPE_EDIT],
                                               &hover_round_edit)) {
                        if(editing_round) {
                            if(habit_session_commit_edit(app, &ctx->entries[i]))
                                return y;
                        } else {
                            habit_session_begin_round_edit(app, &ctx->entries[i], r);
                        }
                        return y;
                    }
                    if(DrawUIPaddedIconBtn(round_trash_x, y - ScaleUIPx(6),
                                               icon_size, icon_padding,
                                               app->icons[UI_ICON_TYPE_TRASH], &hover_round_trash)) {
                        if(habit_session_delete_round(&ctx->entries[i], r))
                            habit_session_changed(app, ctx->count);
                        habit_session_cancel_edit(app);
                        return y;
                    }
                }
                y += ScaleUIPx(28);
            }
            y += ScaleUIPx(4);
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
            int button_h = ScaleUIPx(34);
            int step_w = ScaleUIPx(42);
            int gap = ScaleUIPx(8);
            int minus_x = content_x;
            int plus_x = content_x + content_w - step_w;
            int label_x = minus_x + step_w + gap;
            int label_w = plus_x - label_x - gap;
            int hover = 0;
            char total_text[64];

            y += ScaleUIPx(6);
            if(draw) {
                FormatLocaleText(total_text, sizeof(total_text),
                                 "habit_session_total_count", total_count);
                if(DrawUIGenericButton(minus_x, y, step_w, button_h, "-",
                                          UI_BUTTON_STYLE_SECONDARY,
                                          total_count <= minimum_count, &hover)) {
                    habit_apply_count_action(app, app->habit_detail_index,
                                             ctx->day_filter, -1, minimum_count);
                    app_auto_sync(app);
                    return y;
                }
                DrawRectangle(label_x, y, label_w, button_h, DarkenUIColor(GetThemeBackground(), 5));
                DrawUIText(total_text, label_x + ScaleUIPx(8),
                                GetUIControlTextY(total_text, y, button_h, GetUISmallFontSize()),
                                GetUISmallFontSize(), GetThemeText());
                if(DrawUIGenericButton(plus_x, y, step_w, button_h, "+",
                                          UI_BUTTON_STYLE_SECONDARY, 0, &hover)) {
                    habit_apply_count_action(app, app->habit_detail_index,
                                             ctx->day_filter, 1, minimum_count);
                    app_auto_sync(app);
                    return y;
                }
            }
            y += button_h + ScaleUIPx(8);
        }
    }

    return y;
}

/* Habit session keyboard functions */
int
habit_session_keyboard_height(InbeApp *app)
{
    int key_h = ScaleUIPx(48);
    int gap = ScaleUIPx(6);
    int pad = ScaleUIPx(10);

#if !ANDROID_BUILD
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
    int key_h = ScaleUIPx(48);
    int gap = ScaleUIPx(6);
    int pad = ScaleUIPx(10);
    int keyboard_h = habit_session_keyboard_height(app);
    int x = GetUIPageSidePadding();
    int y = view_height - keyboard_h;
    int w = view_width - x * 2;
    int key_w = (w - gap * 2) / 3;

#if !ANDROID_BUILD
    if(app == NULL || !app->on_screen_keyboard_enabled)
        return 0;
#endif
    if(app == NULL || !app->habit_session_edit.active || keyboard_h <= 0)
        return 0;

    DrawRectangle(0, y, view_width, keyboard_h, DarkenUIColor(GetThemeBackground(), 10));
    DrawLine(0, y, view_width, y, DarkenUIColor(GetThemeBackground(), 42));

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
    return DrawUIGenericButton(x, y, w, h, label, UI_BUTTON_STYLE_SECONDARY, 0, &hover);
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
