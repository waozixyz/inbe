#ifndef INBE_TEXT_LAYOUT_H
#define INBE_TEXT_LAYOUT_H

#include "raylib.h"
#include "ui.h"

/* ================================================================
 * TEXT LAYOUT ENGINE
 * Supports text reflow with inline icon embedding (%i keyword)
 * ================================================================ */

typedef enum {
    TEXT_ELEMENT_TYPE_TEXT,
    TEXT_ELEMENT_TYPE_ICON,
    TEXT_ELEMENT_TYPE_LINE_BREAK
} TextElementType;

typedef struct {
    TextElementType type;
    const char *text;           /* For TEXT elements - points into original string */
    Texture2D icon;             /* For ICON elements */
    UIIconType icon_type;       /* Fallback icon type if texture fails */
    int icon_size;              /* Icon size in pixels */
    int text_width;             /* Cached text width (for TEXT elements) */
} TextElement;

typedef struct TextLayout {
    TextElement *elements;      /* Array of elements (mixed text and icons) */
    int element_count;
    int *line_breaks;           /* Indices where lines break (element indices) */
    int line_count;
    int *line_widths;           /* Width of each line in pixels */
    int total_height;           /* Total height of laid-out text */
    int last_reflow_width;      /* Cache: last width used for reflow */
} TextLayout;

/* Core text layout functions */
TextLayout text_layout_parse(const char *input, Texture2D icon, UIIconType icon_type, int icon_size);
void text_layout_reflow(TextLayout *layout, int max_width, int font_size, int line_height);
void text_layout_draw(TextLayout *layout, int x, int *y, int font_size, Color color);
int text_layout_get_height(TextLayout *layout);
void text_layout_free(TextLayout *layout);

#endif /* INBE_TEXT_LAYOUT_H */