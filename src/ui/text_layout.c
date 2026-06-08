#include "text_layout.h"
#include "ui.h"
#include "app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * TEXT LAYOUT PARSING
 * ================================================================ */

TextLayout
text_layout_parse(const char *input, Texture2D icon, UIIconType icon_type, int icon_size)
{
    TextLayout layout = {0};

    if(input == NULL || input[0] == '\0') {
        return layout;
    }

    /* First pass: count word/phrase elements */
    int element_count = 0;
    const char *p = input;

    while(*p) {
        /* Check for icon keyword */
        if(strncmp(p, "%i", 2) == 0) {
            element_count++;  /* Count icon as element */
            p += 2;
        } else if(*p == '\n') {
            /* Explicit newline - count as forced line break element */
            element_count++;
            p++;
        } else {
            /* Skip whitespace and count word/phrase segments */
            const char *word_start = p;
            while(*p && *p != ' ' && *p != '\n' && strncmp(p, "%i", 2) != 0) {
                p++;
            }

            /* Only count if we found actual text content */
            if(p > word_start) {
                element_count++;
            }

            /* Skip whitespace (spaces only, not newlines) */
            if(*p == ' ') {
                p++;
            }
        }
    }


    /* Allocate elements array */
    layout.elements = (TextElement *)calloc(element_count, sizeof(TextElement));
    if(layout.elements == NULL) {
        return layout;
    }
    layout.element_count = element_count;

    /* Second pass: populate elements with actual word/phrase segments */
    int element_idx = 0;
    p = input;

    while(*p && element_idx < element_count) {
        if(strncmp(p, "%i", 2) == 0) {
            layout.elements[element_idx].type = TEXT_ELEMENT_TYPE_ICON;
            layout.elements[element_idx].icon = icon;
            layout.elements[element_idx].icon_type = icon_type;
            layout.elements[element_idx].icon_size = icon_size;
            layout.elements[element_idx].text_width = 0;
            element_idx++;
            p += 2;
        } else if(*p == '\n') {
            /* Add line break element for explicit newlines */
            layout.elements[element_idx].type = TEXT_ELEMENT_TYPE_LINE_BREAK;
            layout.elements[element_idx].text_width = 0;
            element_idx++;
            p++;
        } else {
            /* Extract word/phrase segment */
            const char *word_start = p;
            while(*p && *p != ' ' && *p != '\n' && strncmp(p, "%i", 2) != 0) {
                p++;
            }

            /* Only add if we found actual text content */
            if(p > word_start) {
                size_t len = p - word_start;
                char *text_copy = (char *)malloc(len + 1);
                if(text_copy != NULL) {
                    memcpy(text_copy, word_start, len);
                    text_copy[len] = '\0';
                    layout.elements[element_idx].type = TEXT_ELEMENT_TYPE_TEXT;
                    layout.elements[element_idx].text = text_copy;
                    layout.elements[element_idx].text_width = 0;
                    element_idx++;
                }
            }

            /* Skip whitespace (spaces only, not newlines) */
            if(*p == ' ') {
                p++;
            }
        }
    }

    return layout;
}

void text_layout_reflow(TextLayout *layout, int max_width, int font_size, int line_height)
{
    if(layout == NULL || layout->elements == NULL || layout->element_count == 0) {
        return;
    }

    int *new_line_breaks = (int *)calloc(layout->element_count + 1, sizeof(int));
    int *new_line_widths = (int *)calloc(layout->element_count + 1, sizeof(int));

    if(new_line_breaks == NULL || new_line_widths == NULL) {
        free(new_line_breaks);
        free(new_line_widths);
        return; 
    }

    free(layout->line_breaks);
    free(layout->line_widths);
    
    layout->line_breaks = new_line_breaks;
    layout->line_widths = new_line_widths;

    for(int i = 0; i < layout->element_count; i++) {
        if(layout->elements[i].type == TEXT_ELEMENT_TYPE_TEXT && layout->elements[i].text != NULL) {
            layout->elements[i].text_width = MeasureText(layout->elements[i].text, font_size);
        }
    }

    int space_width = MeasureText(" ", font_size);
    int icon_spacing = flint_px(4); 

    layout->line_count = 0;
    layout->line_breaks[0] = 0; 
    int current_line_width = 0;

    for(int i = 0; i < layout->element_count; i++) {
        if(layout->elements[i].type == TEXT_ELEMENT_TYPE_LINE_BREAK) {
            layout->line_count++;
            layout->line_breaks[layout->line_count] = i;
            layout->line_widths[layout->line_count - 1] = current_line_width;
            current_line_width = 0;
            continue;
        }

        int element_width = 0;
        int spacing = 0;

        if(layout->elements[i].type == TEXT_ELEMENT_TYPE_TEXT) {
            element_width = layout->elements[i].text_width;
            spacing = (current_line_width > 0) ? space_width : 0;
        } else { /* ICON */
            element_width = layout->elements[i].icon_size;
            spacing = (current_line_width > 0) ? icon_spacing : 0; 
        }

        if(current_line_width + spacing + element_width <= max_width) {
            current_line_width += spacing + element_width;
        } else {
            layout->line_count++;
            layout->line_breaks[layout->line_count] = i;
            layout->line_widths[layout->line_count - 1] = current_line_width;
            current_line_width = element_width;
        }
    }

    layout->line_widths[layout->line_count] = current_line_width;
    layout->line_count++;

    if(layout->line_count > 0) {
        layout->total_height = layout->line_count * font_size + (layout->line_count - 1) * line_height;
    } else {
        layout->total_height = 0;
    }
}

/* ================================================================
 * TEXT LAYOUT DRAWING
 * ================================================================ */
void text_layout_draw(TextLayout *layout, int x, int *y, int font_size, Color color)
{
    if(layout == NULL || layout->elements == NULL || layout->element_count == 0) {
        return;
    }

    int current_x = x;
    int current_y = *y;
    
    int space_width = MeasureText(" ", font_size);
    int icon_spacing = flint_px(4);
    int line_spacing = flint_px(4);

    int next_break_line = 1; 

    for(int i = 0; i < layout->element_count; i++) {        
        if(next_break_line <= layout->line_count && i == layout->line_breaks[next_break_line]) {
            current_y += font_size + line_spacing;
            current_x = x;      
            next_break_line++;
        }

        if(layout->elements[i].type == TEXT_ELEMENT_TYPE_LINE_BREAK) {
            continue;
        }

        int spacing = 0;
        if(current_x > x) {
            spacing = (layout->elements[i].type == TEXT_ELEMENT_TYPE_TEXT) ? space_width : icon_spacing;
        }
        current_x += spacing;

        if(layout->elements[i].type == TEXT_ELEMENT_TYPE_TEXT) {
            if(layout->elements[i].text != NULL && layout->elements[i].text[0] != '\0') {
                DrawText(layout->elements[i].text, current_x, current_y, font_size, color);
                current_x += layout->elements[i].text_width;
            }
        } else {
            Texture2D icon = layout->elements[i].icon;
            int icon_size = layout->elements[i].icon_size;

            if(icon.id != 0) {
                Rectangle src = {0, 0, (float)icon.width, (float)icon.height};
                Rectangle dst = {(float)current_x, (float)current_y, (float)icon_size, (float)icon_size};
                DrawTexturePro(icon, src, dst, (Vector2){0}, 0, color);
            } else {
                flint_draw_icon_fallback((FlintIconType)layout->elements[i].icon_type, current_x, current_y, icon_size, color);
            }

            current_x += icon_size;
        }
    }

    *y = current_y + font_size;
}

/* ================================================================
 * TEXT LAYOUT UTILITIES
 * ================================================================ */

int
text_layout_get_height(TextLayout *layout)
{
    if(layout == NULL) {
        return 0;
    }
    return layout->total_height;
}

void
text_layout_free(TextLayout *layout)
{
    if(layout == NULL) {
        return;
    }

    /* Free text strings (they were allocated during parsing) */
    if(layout->elements != NULL) {
        for(int i = 0; i < layout->element_count; i++) {
            if(layout->elements[i].type == TEXT_ELEMENT_TYPE_TEXT && layout->elements[i].text != NULL) {
                free((void *)layout->elements[i].text);
            }
        }
        free(layout->elements);
    }

    /* Free layout data */
    if(layout->line_breaks != NULL) {
        free(layout->line_breaks);
    }
    if(layout->line_widths != NULL) {
        free(layout->line_widths);
    }

    /* Clear the structure */
    memset(layout, 0, sizeof(TextLayout));
}
