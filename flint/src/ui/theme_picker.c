#include "ui.h"
#include "flint_layout.h"
#include "flint_locale.h"


static const FlintThemeId theme_picker_order[FLINT_THEME_COUNT] = {
    FLINT_THEME_SKY,
    FLINT_THEME_OCEAN,
    FLINT_THEME_COBALT,
    FLINT_THEME_FOREST,
    FLINT_THEME_SAGE,
    FLINT_THEME_MINT,
    FLINT_THEME_SUNSET,
    FLINT_THEME_INK,
    FLINT_THEME_DAWN,
    FLINT_THEME_CHERRY,
    FLINT_THEME_LAVENDER,
    FLINT_THEME_MONO
};

typedef struct {
    int circle_size;
    int label_gap;
    int col_gap;
    int cell_w;
    int per_row;
    int row_width;
    int row_step;
    int row_count;
    int height;
} UIThemeGridLayout;

static const char *
ui_theme_label(FlintThemeId theme)
{
    const char *key = NULL;
    const char *label;

    switch(flint_theme_normalize(theme)) {
    case FLINT_THEME_SKY: key = "theme_sky"; break;
    case FLINT_THEME_OCEAN: key = "theme_ocean"; break;
    case FLINT_THEME_FOREST: key = "theme_forest"; break;
    case FLINT_THEME_SUNSET: key = "theme_sunset"; break;
    case FLINT_THEME_LAVENDER: key = "theme_lavender"; break;
    case FLINT_THEME_CHERRY: key = "theme_cherry"; break;
    case FLINT_THEME_DAWN: key = "theme_dawn"; break;
    case FLINT_THEME_SAGE: key = "theme_sage"; break;
    case FLINT_THEME_INK: key = "theme_sepia"; break;
    case FLINT_THEME_MONO: key = "theme_mono"; break;
    case FLINT_THEME_MINT: key = "theme_mint"; break;
    case FLINT_THEME_COBALT: key = "theme_cobalt"; break;
    default: break;
    }

    if(key == NULL)
        return flint_theme_label(theme);
    label = locale_get(key);
    if(label == NULL || label[0] == '\0' || strcmp(label, key) == 0)
        return flint_theme_label(theme);
    return label;
}

static UIThemeGridLayout
ui_theme_grid_layout(int w)
{
    UIThemeGridLayout layout = {0};
    int small_font = FLINT_TEXT_12;

    int row_gap = flint_px(14);
    layout.circle_size = flint_px(24);
    layout.label_gap = flint_px(6);
    layout.col_gap = flint_px(10);
    layout.cell_w = layout.circle_size;
    for(int i = 0; i < FLINT_THEME_COUNT; i++) {
        int name_w = flint_text_measure(ui_theme_label((FlintThemeId)i), small_font) + flint_px(8);
        if(name_w > layout.cell_w)
            layout.cell_w = name_w;
    }

    layout.per_row = (w + layout.col_gap) / (layout.cell_w + layout.col_gap);
    if(layout.per_row < 1)
        layout.per_row = 1;
    if(layout.per_row > 3)
        layout.per_row = 3;
    if(layout.per_row > FLINT_THEME_COUNT)
        layout.per_row = FLINT_THEME_COUNT;
    layout.row_width = layout.per_row * layout.cell_w + (layout.per_row - 1) * layout.col_gap;

    layout.row_step = layout.circle_size + layout.label_gap + small_font + row_gap;
    layout.row_count = (FLINT_THEME_COUNT + layout.per_row - 1) / layout.per_row;
    layout.height = layout.circle_size + layout.label_gap + small_font;
    if(layout.row_count > 1)
        layout.height += (layout.row_count - 1) * layout.row_step;

    return layout;
}

static int
ui_draw_theme_grid(int x, int circle_y, int w, int dark, int *theme_id)
{
    int changed = 0;
    int small_font = FLINT_TEXT_12;
    int selected = theme_id != NULL ? *theme_id : FLINT_THEME_SKY;
    UIThemeGridLayout layout = ui_theme_grid_layout(w);
    int start_x = x + (w - layout.row_width) / 2;
    Vector2 mouse_world = ui_mouse_world();

    if(selected < 0 || selected >= FLINT_THEME_COUNT)
        selected = FLINT_THEME_SKY;

    for(int i = 0; i < FLINT_THEME_COUNT; i++) {
        FlintThemeId theme = theme_picker_order[i];
        int row = i / layout.per_row;
        int col = i % layout.per_row;
        int cell_x = start_x + col * (layout.cell_w + layout.col_gap);
        int cx = cell_x + layout.cell_w / 2;
        int cy = circle_y + row * layout.row_step;
        Color theme_color = c_circle;

        if(!flint_theme_catalog_color(theme, dark != 0, "circle", &theme_color)) {
            const char *scope = flint_theme_scope_for(theme, dark != 0);
            theme_color = flint_theme_get(scope, "circle");
        }

        DrawCircle(cx, cy, layout.circle_size / 2, theme_color);
        DrawCircleLines(cx, cy, layout.circle_size / 2 + (selected == (int)theme ? flint_px(2) : flint_px(1)),
                        selected == (int)theme ? c_text : flint_darken(c_bg, 30));

        Rectangle bounds = {
            (float)(cx - layout.circle_size / 2 - flint_px(4)),
            (float)(cy - layout.circle_size / 2 - flint_px(4)),
            (float)(layout.circle_size + flint_px(8)),
            (float)(layout.circle_size + flint_px(8))
        };
        if(CheckCollisionPointRec(mouse_world, bounds) && !ui_input_captures_click(mouse_world)) {
            ui_mark_clickable();
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                selected = theme;
                if(theme_id != NULL)
                    *theme_id = theme;
                changed = 1;
            }
        }

        const char *name = ui_theme_label(theme);
        int name_w = flint_text_measure(name, small_font);
        flint_text_draw(name, cx - name_w / 2,
                        cy + layout.circle_size / 2 + layout.label_gap,
                        small_font, c_text);
    }

    return changed;
}

int
ui_draw_theme_switcher(int x, int y, int w, const char *label,
                       const char *light_label, const char *dark_label,
                       int *theme_id, int *dark_mode)
{
    int changed = 0;
    int font = flint_ui_font();
    int dark = dark_mode != NULL ? *dark_mode : 0;

    flint_text_draw(label ? label : "Theme", x, y, font, c_text);

    int light_w = flint_text_measure(light_label ? light_label : "Light", font);
    int dark_w = flint_text_measure(dark_label ? dark_label : "Dark", font);
    int max_label_w = light_w > dark_w ? light_w : dark_w;
    int toggle_w = max_label_w * 2 + flint_px(32);
    int min_toggle_w = flint_px(100);
    if(toggle_w < min_toggle_w)
        toggle_w = min_toggle_w;
    if(toggle_w > w)
        toggle_w = w;

    int toggle_h = flint_px(28);
    int toggle_x = x + w - toggle_w - flint_px(8);
    int toggle_y = y - flint_px(2);
    if(ui_draw_toggle_switch(toggle_x, toggle_y, toggle_w, toggle_h, &dark,
                             light_label ? light_label : "Light",
                             dark_label ? dark_label : "Dark")) {
        if(dark_mode != NULL)
            *dark_mode = dark;
        changed = 1;
    }

    if(ui_draw_theme_grid(x, y + flint_px(64), w, dark, theme_id))
        changed = 1;

    return changed;
}

int
ui_draw_theme_picker(int x, int y, int w, int dark_mode,
                     int *theme_id)
{
    int changed = 0;

    if(ui_draw_theme_grid(x, y + flint_px(54), w, dark_mode != 0, theme_id))
        changed = 1;

    return changed;
}

int
ui_theme_picker_height(int w)
{
    return flint_px(54) + ui_theme_grid_layout(w).height;
}
