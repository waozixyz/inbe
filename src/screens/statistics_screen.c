#include "statistics_screen.h"

#include "app.h"
#include "habits/habits.h"
#include "locale.h"
#include "theme.h"
#include "ui.h"

extern int view_width;
extern int view_height;

static int
statistics_content_height(int linked_count)
{
    return ScaleUIPx(linked_count > 0 ? 710 : 420);
}

static int
statistics_day_count(const InbeHabit *habit, const HabitLinkedContext *linked_ctx,
                     int day_index)
{
    int count = habit_effective_day_count(habit, day_index, linked_ctx);

    if(count > 0)
        return count;
    return habit_completed_day(habit, day_index) ||
           habit_linked_has_day(linked_ctx, day_index)
               ? 1
               : 0;
}

static int
statistics_day_of_week(int day_index)
{
    struct tm tm_value;

    memset(&tm_value, 0, sizeof(tm_value));
    tm_value.tm_year = day_index / 10000 - 1900;
    tm_value.tm_mon = (day_index / 100) % 100 - 1;
    tm_value.tm_mday = day_index % 100;
    tm_value.tm_hour = 12;
    mktime(&tm_value);
    return tm_value.tm_wday;
}

static void
statistics_draw_metric(int x, int y, int w, int h, const char *label,
                       const char *value, Color accent)
{
    int label_font = UI_TEXT_12;
    int value_font = UI_TEXT_24;
    int value_w;

    DrawRectangle(x, y, w, h, DarkenUIColor(GetThemeBackground(), 8));
    DrawRectangle(x, y, ScaleUIPx(4), h, accent);
    DrawLine(x, y + h - 1, x + w, y + h - 1, DarkenUIColor(GetThemeBackground(), 30));
    DrawUIText(label, x + ScaleUIPx(12), y + ScaleUIPx(8),
                    label_font, DarkenUIColor(GetThemeText(), 20));
    value_w = MeasureUIText(value, value_font);
    if(value_w > w - ScaleUIPx(24))
        value_font = UI_TEXT_16;
    DrawUIText(value, x + ScaleUIPx(12), y + ScaleUIPx(30),
                    value_font, GetThemeText());
}

static int
statistics_draw_section_title(const char *title, int x, int y, int w)
{
    UIParagraph paragraph = {
        .text = title,
        .width = w,
        .font = UI_TEXT_16,
        .line_gap = ScaleUIPx(3),
        .color = GetThemeText()
    };
    int title_y = y;
    int height = GetUIParagraphHeight(paragraph);

    DrawUIParagraph(paragraph, x, &title_y);
    return height;
}

static void
statistics_format_day_count(char *out, size_t out_size, int count)
{
    FormatLocaleText(out, out_size,
                  count == 1 ? "habit_stats_day_singular"
                             : "habit_stats_day_plural",
                  count);
}

static int
statistics_entry_matches_filter(const HabitLinkedEntry *entry, int filter)
{
    if(entry == NULL)
        return 0;
    return entry->activity == filter;
}

static int
statistics_has_wim_hof_rounds(const HabitLinkedContext *linked_ctx)
{
    if(linked_ctx == NULL)
        return 0;
    for(int i = 0; i < linked_ctx->count; i++) {
        if(linked_ctx->entries[i].activity == EXERCISE_WIM_HOF &&
           linked_ctx->entries[i].round_count > 0)
            return 1;
    }
    return 0;
}

static int
statistics_day_offset_from_today(int day_index, const struct tm *today_tm)
{
    struct tm day_tm;
    struct tm today_copy;
    time_t today_time;
    time_t day_time;
    double diff_days;

    if(today_tm == NULL)
        return -1;
    today_copy = *today_tm;
    day_tm = *today_tm;
    day_tm.tm_year = day_index / 10000 - 1900;
    day_tm.tm_mon = (day_index / 100) % 100 - 1;
    day_tm.tm_mday = day_index % 100;
    day_tm.tm_hour = 12;
    day_tm.tm_min = 0;
    day_tm.tm_sec = 0;
    today_time = mktime(&today_copy);
    day_time = mktime(&day_tm);
    diff_days = difftime(today_time, day_time) / 86400.0;
    if(diff_days < -0.5 || diff_days > 30.5)
        return -1;
    return (int)(diff_days + 0.5);
}

static int
statistics_draw_hold_range_selector(InbeApp *app, int x, int y, int w)
{
    const char *labels[2] = {"1W", "1M"};
    const int values[2] = {7, 31};
    int selected = app->habits.hold_stats_range_days == 31 ? 1 : 0;
    int h = ScaleUIPx(34);
    int segment_w = w / 2;
    int clicked = 0;
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);

    for(int i = 0; i < 2; i++) {
        int segment_x = x + i * segment_w;
        int current_w = i == 1 ? x + w - segment_x : segment_w;
        Rectangle rect = {segment_x, y, current_w, h};
        int active_hit = CheckCollisionPointRec(mouse_world, rect) &&
                         !UIInputCapturesClick(mouse_world);
        int hovered = active_hit && UIHoverEffectsEnabled();
        int active = i == selected;
        Color fill = active ? GetThemeButton() : DarkenUIColor(GetThemeBackground(), 10);
        Color top = LightenUIColor(fill, 35);
        Color bottom = DarkenUIColor(fill, 45);
        int font = GetUIFontSize();
        int text_w;

        if(active_hit) {
            app->cursor_clickable = 1;
            if(hovered && !active) {
                fill = GetThemeButtonHover();
                top = DarkenUIColor(GetThemeButtonHover(), 40);
                bottom = LightenUIColor(GetThemeButtonHover(), 40);
            }
        }

        DrawRectangle(segment_x, y, current_w, h, fill);
        DrawUIBevel(segment_x, y, current_w, h, top, bottom);
        text_w = MeasureUIText(labels[i], font);
        DrawUIText(labels[i], segment_x + (current_w - text_w) / 2,
                        GetUIControlTextY(labels[i], y, h, font), font,
                        GetThemeText());

        if(active_hit && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
           !UIInputCapturesClick(mouse_world) && selected != i) {
            app->habits.hold_stats_range_days = values[i];
            clicked = 1;
        }
    }

    return clicked;
}

static void
statistics_draw_hold_graph(InbeApp *app, const HabitLinkedContext *linked_ctx,
                           int content_x, int y, int content_w,
                           const struct tm *today_tm, int filter, Color accent)
{
    enum { MAX_GRAPH_DAYS = 31, MAX_POINTS = 256 };
    typedef struct {
        int time_key;
        int round_index;
        int day_slot;
        int hold_seconds;
        int x;
        int y;
    } GraphPoint;
    GraphPoint points[MAX_POINTS];
    int point_count = 0;
    int graph_days = app->habits.hold_stats_range_days == 31 ? 31 : 7;
    int day_points[MAX_GRAPH_DAYS] = {0};
    int day_totals[MAX_GRAPH_DAYS] = {0};
    int max_hold = 1;
    int graph_h = ScaleUIPx(170);
    int axis_w = ScaleUIPx(48);
    int plot_x = content_x + axis_w;
    int plot_y = y + ScaleUIPx(8);
    int plot_w = content_w - axis_w - ScaleUIPx(10);
    int plot_h = graph_h - ScaleUIPx(36);
    char axis_label[32];

    DrawRectangle(content_x, y, content_w, graph_h, DarkenUIColor(GetThemeBackground(), 7));
    for(int i = 0; linked_ctx != NULL && i < linked_ctx->count; i++) {
        const HabitLinkedEntry *entry = &linked_ctx->entries[i];
        int day_index;
        int day_offset;

        if(!statistics_entry_matches_filter(entry, filter))
            continue;
        day_index = entry->year * 10000 + entry->month * 100 + entry->day;
        day_offset = statistics_day_offset_from_today(day_index, today_tm);
        if(day_offset < 0 || day_offset >= graph_days)
            continue;
        for(int r = 0; r < entry->round_count && point_count < MAX_POINTS; r++) {
            int day_slot = graph_days - 1 - day_offset;
            if(entry->rounds[r] > max_hold)
                max_hold = entry->rounds[r];
            day_points[day_slot]++;
            day_totals[day_slot] += entry->rounds[r];
            points[point_count++] = (GraphPoint){
                day_index * 86400 + entry->hour * 3600 + entry->minute * 60 + entry->second,
                r,
                day_slot,
                entry->rounds[r],
                0,
                0
            };
        }
    }

    for(int i = 1; i < point_count; i++) {
        GraphPoint point = points[i];
        int j = i - 1;
        while(j >= 0 &&
              (points[j].time_key > point.time_key ||
               (points[j].time_key == point.time_key &&
                points[j].round_index > point.round_index))) {
            points[j + 1] = points[j];
            j--;
        }
        points[j + 1] = point;
    }

    for(int i = 1; i < graph_days; i++) {
        if(graph_days == 7 || i % 5 == 0) {
            int x = plot_x + (plot_w * i) / graph_days;
            DrawLine(x, plot_y, x, plot_y + plot_h, DarkenUIColor(GetThemeBackground(), 22));
        }
    }
    for(int i = 0; i <= 2; i++) {
        int line_y = plot_y + (plot_h * i) / 2;
        int seconds = max_hold - (max_hold * i) / 2;
        snprintf(axis_label, sizeof(axis_label), "%ds", seconds);
        DrawLine(plot_x, line_y, plot_x + plot_w, line_y,
                 DarkenUIColor(GetThemeBackground(), 20));
        DrawUIText(axis_label, content_x + ScaleUIPx(4),
                        line_y - ScaleUIPx(8), UI_TEXT_12,
                        DarkenUIColor(GetThemeText(), 24));
    }
    DrawLine(plot_x, plot_y + plot_h, plot_x + plot_w, plot_y + plot_h,
             DarkenUIColor(GetThemeText(), 42));

    {
        int day_seen[MAX_GRAPH_DAYS] = {0};
        int day_first_x[MAX_GRAPH_DAYS];
        int day_last_x[MAX_GRAPH_DAYS];

        for(int i = 0; i < graph_days; i++) {
            day_first_x[i] = -1;
            day_last_x[i] = -1;
        }
        for(int i = 0; i < point_count; i++) {
            int day_slot = points[i].day_slot;
            int count = day_points[day_slot] > 0 ? day_points[day_slot] : 1;
            int ordinal = day_seen[day_slot]++;
            int x0 = plot_x + (plot_w * day_slot) / graph_days;
            int x1 = plot_x + (plot_w * (day_slot + 1)) / graph_days;
            points[i].x = x0 + ((ordinal + 1) * (x1 - x0)) / (count + 1);
            points[i].y = plot_y + plot_h -
                          (points[i].hold_seconds * (plot_h - ScaleUIPx(10))) / max_hold;
            if(day_first_x[day_slot] < 0 || points[i].x < day_first_x[day_slot])
                day_first_x[day_slot] = points[i].x;
            if(day_last_x[day_slot] < 0 || points[i].x > day_last_x[day_slot])
                day_last_x[day_slot] = points[i].x;
        }

        {
        int prev_x = 0;
        int prev_y = 0;
        int has_prev = 0;

        for(int i = 0; i < graph_days; i++) {
            int avg_seconds;
            int avg_y;

            if(day_points[i] <= 0)
                continue;
            avg_seconds = day_totals[i] / day_points[i];
            avg_y = plot_y + plot_h -
                    (avg_seconds * (plot_h - ScaleUIPx(10))) / max_hold;
            if(has_prev)
                DrawLine(prev_x, prev_y, day_first_x[i], avg_y, accent);
            DrawLine(day_first_x[i], avg_y, day_last_x[i], avg_y, accent);
            prev_x = day_last_x[i];
            prev_y = avg_y;
            has_prev = 1;
        }
        }
    }
    for(int i = 0; i < point_count; i++)
        DrawCircle(points[i].x, points[i].y, ScaleUIPx(3), accent);
    if(point_count <= 0) {
        const char *empty = GetLocaleText(graph_days == 31 ? "habit_stats_no_rounds_month"
                                                        : "habit_stats_no_rounds_week");
        int text_w = MeasureUIText(empty, UI_TEXT_12);
        DrawUIText(empty, content_x + (content_w - text_w) / 2,
                        y + graph_h / 2 - ScaleUIPx(8), UI_TEXT_12,
                        DarkenUIColor(GetThemeText(), 24));
    }
    for(int i = 0; i < graph_days; i++) {
        int day_offset = graph_days - 1 - i;
        int show_label = graph_days == 7 || day_offset == 0 || day_offset % 5 == 0;
        char label[8];
        int day_x;

        if(!show_label)
            continue;
        if(day_offset == 0)
            snprintf(label, sizeof(label), "%s", GetLocaleText("statistics_now_label"));
        else
            FormatLocaleText(label, sizeof(label), "statistics_day_offset_label",
                             day_offset);
        day_x = plot_x + (plot_w * i) / graph_days;
        DrawUIText(label, day_x + ScaleUIPx(2),
                        y + graph_h - ScaleUIPx(20), UI_TEXT_12,
                        DarkenUIColor(GetThemeText(), 24));
    }
}

static void
statistics_draw_view(InbeApp *app, InbeHabit *active,
                     HabitLinkedContext *linked_ctx,
                     int content_x, int content_w, int y)
{
    time_t now = time(NULL);
    struct tm today_tm;
    int current_streak = 0;
    int best_streak = 0;
    int running_streak = 0;
    int last_30_done = 0;
    int last_30_total = 30;
    int weekday_counts[7] = {0};
    int max_weekday = 1;
    int chart_h = ScaleUIPx(128);
    int metrics_gap = ScaleUIPx(8);
    int metric_h = ScaleUIPx(66);
    int metric_w;
    char value[64];
    int has_wim_hof_rounds = statistics_has_wim_hof_rounds(linked_ctx);

    if(localtime(&now) != NULL)
        today_tm = *localtime(&now);
    else
        memset(&today_tm, 0, sizeof(today_tm));
    today_tm.tm_hour = 12;
    today_tm.tm_min = 0;
    today_tm.tm_sec = 0;
    mktime(&today_tm);

    for(int i = 0; i < 365; i++) {
        struct tm day_tm = today_tm;
        int day_index;
        int count;

        day_tm.tm_mday -= i;
        mktime(&day_tm);
        day_index = habit_tm_date_index(&day_tm);
        count = statistics_day_count(active, linked_ctx, day_index);
        if(i < 30 && count > 0)
            last_30_done++;
        if(i < 56 && count > 0) {
            int weekday = statistics_day_of_week(day_index);
            if(weekday >= 0 && weekday < 7) {
                weekday_counts[weekday]++;
                if(weekday_counts[weekday] > max_weekday)
                    max_weekday = weekday_counts[weekday];
            }
        }
        if(count > 0) {
            running_streak++;
            if(running_streak > best_streak)
                best_streak = running_streak;
            if(i == current_streak)
                current_streak++;
        } else {
            running_streak = 0;
        }
    }

    metric_w = (content_w - metrics_gap) / 2;
    statistics_format_day_count(value, sizeof(value), current_streak);
    statistics_draw_metric(content_x, y, metric_w, metric_h,
                           GetLocaleText("habit_stats_current_streak"), value, active->color);
    snprintf(value, sizeof(value), "%d/%d", last_30_done, last_30_total);
    statistics_draw_metric(content_x + metric_w + metrics_gap, y,
                           content_w - metric_w - metrics_gap, metric_h,
                           GetLocaleText("habit_stats_last_30_days"), value, LightenUIColor(active->color, 24));
    y += metric_h + ScaleUIPx(22);

    if(has_wim_hof_rounds) {
        y += statistics_draw_section_title(GetLocaleText("habit_stats_wim_hof_hold_rounds"),
                                           content_x, y, content_w) + ScaleUIPx(10);
        statistics_draw_hold_range_selector(app, content_x, y, ScaleUIPx(118));
        y += ScaleUIPx(42);
        statistics_draw_hold_graph(app, linked_ctx, content_x, y, content_w,
                                   &today_tm, EXERCISE_WIM_HOF,
                                   active->color);
        y += ScaleUIPx(194);
    }

    y += statistics_draw_section_title(GetLocaleText("habit_stats_weekday_pattern"),
                                       content_x, y, content_w) + ScaleUIPx(10);
    DrawRectangle(content_x, y, content_w, chart_h, DarkenUIColor(GetThemeBackground(), 7));
    {
        const char *labels[7] = {"S", "M", "T", "W", "T", "F", "S"};
        int slot_w = content_w / 7;
        for(int i = 0; i < 7; i++) {
            int h = weekday_counts[i] > 0
                        ? (weekday_counts[i] * (chart_h - ScaleUIPx(34))) / max_weekday
                        : ScaleUIPx(3);
            int x = content_x + i * slot_w + ScaleUIPx(5);
            int w = slot_w - ScaleUIPx(10);
            int bar_y = y + chart_h - h - ScaleUIPx(22);

            if(w < ScaleUIPx(4))
                w = ScaleUIPx(4);
            DrawRectangle(x, bar_y, w, h,
                          weekday_counts[i] > 0 ? LightenUIColor(active->color, 12)
                                                 : DarkenUIColor(GetThemeBackground(), 24));
            DrawUIText(labels[i], x + (w - MeasureUIText(labels[i], UI_TEXT_12)) / 2,
                            y + chart_h - ScaleUIPx(18), UI_TEXT_12,
                            DarkenUIColor(GetThemeText(), 18));
        }
    }
    y += chart_h + ScaleUIPx(24);

    if(linked_ctx != NULL && linked_ctx->count > 0) {
        char best[32];
        char total[32];
        y += statistics_draw_section_title(GetLocaleText("habit_stats_practice_time"),
                                           content_x, y, content_w) + ScaleUIPx(10);
        habit_format_duration(linked_ctx->best_seconds, best, sizeof(best));
        habit_format_duration(linked_ctx->total_seconds, total, sizeof(total));
        statistics_draw_metric(content_x, y, metric_w, metric_h,
                               GetLocaleText("habit_stats_best_hold"), best, active->color);
        statistics_draw_metric(content_x + metric_w + metrics_gap, y,
                               content_w - metric_w - metrics_gap, metric_h,
                               GetLocaleText("habit_stats_total_hold"), total, LightenUIColor(active->color, 20));
    } else {
        statistics_format_day_count(value, sizeof(value), best_streak);
        statistics_draw_metric(content_x, y, content_w, metric_h,
                               GetLocaleText("habit_stats_best_streak"), value, active->color);
    }
}

typedef struct StatisticsScrollPageContext {
    int linked_count;
} StatisticsScrollPageContext;

static int
statistics_scroll_page_content_height(int content_w, void *user_data)
{
    StatisticsScrollPageContext *ctx = user_data;
    (void)content_w;
    return statistics_content_height(ctx->linked_count);
}

void
draw_statistics_content(InbeApp *app, int content_top)
{
    int content_bottom;
    int viewport_h;
    int scroll_y;
    int scroll_h;
    int max_w = ScaleUIPx(CONTENT_MAX_W);
    InbeHabit *active;
    HabitLinkedContext *linked_ctx = NULL;

    if(app == NULL)
        return;
    content_bottom = app_content_bottom_reserved(app);
    viewport_h = view_height - content_top - content_bottom;
    scroll_y = content_top + ScaleUIPx(4);
    scroll_h = viewport_h - ScaleUIPx(4);
    if(app->habits.count <= 0)
        return;
    if(app->habits.selected < 0 || app->habits.selected >= app->habits.count)
        app->habits.selected = 0;

    active = &app->habits.items[app->habits.selected];
    if(habit_is_linked(active)) {
        linked_ctx = calloc(1, sizeof(*linked_ctx));
        if(linked_ctx != NULL)
            habit_collect_linked_entries(active, 0, linked_ctx);
    }

    if(scroll_h < 0)
        scroll_h = 0;
    {
        StatisticsScrollPageContext page_ctx = {
            linked_ctx != NULL ? linked_ctx->count : 0
        };
        UIScrollPage page = BeginUIScrollPage((UIScrollPageSpec){
            .y = scroll_y,
            .height = scroll_h,
            .max_content_width = max_w,
            .scroll_offset = &app->habits.scroll,
            .content_height = statistics_scroll_page_content_height,
            .user_data = &page_ctx
        });
        statistics_draw_view(app, active, linked_ctx, page.content_x,
                             page.content_w, page.content_y + ScaleUIPx(8));
        EndUIScrollPage(page);
    }
    free(linked_ctx);
}
