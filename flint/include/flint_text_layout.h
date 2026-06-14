#ifndef FLINT_TEXT_LAYOUT_H
#define FLINT_TEXT_LAYOUT_H

#include "raylib.h"
#include "ui_icon_types.h"
#include "flint_text.h"

typedef enum {
    FLINT_TEXT_ELEMENT_TYPE_TEXT,
    FLINT_TEXT_ELEMENT_TYPE_ICON,
    FLINT_TEXT_ELEMENT_TYPE_LINE_BREAK
} FlintTextElementType;

typedef struct {
    FlintTextElementType type;
    const char *text;
    Texture2D icon;
    UIIconType icon_type;
    int icon_size;
    int text_width;
} FlintTextElement;

typedef struct FlintTextLayout {
    FlintTextElement *elements;
    int element_count;
    int *line_breaks;
    int line_count;
    int *line_widths;
    int total_height;
    int line_height;  /* Store line height from reflow for use in draw */
    int last_reflow_width;
} FlintTextLayout;

FlintTextLayout flint_text_layout_parse(const char *input, Texture2D icon, UIIconType icon_type, int icon_size);
void flint_text_layout_reflow(FlintTextLayout *layout, int max_width, int font_size, int line_height);
void flint_text_layout_draw(FlintTextLayout *layout, int x, int *y, int font_size, Color color);
int flint_text_layout_get_height(FlintTextLayout *layout);
void flint_text_layout_free(FlintTextLayout *layout);

#endif
