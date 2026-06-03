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
        /* Check for icon keyword */
        if(strncmp(p, "%i", 2) == 0) {
            /* Add icon element */
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

/* ================================================================
 * TEXT LAYOUT REFLOW
 * ================================================================ */

void
text_layout_reflow(TextLayout *layout, int max_width, int font_size, int line_height)
{
    (void)line_height;

    if(layout == NULL || layout->elements == NULL || layout->element_count == 0) {
        return;
    }

    /* Free previous layout data */
    if(layout->line_breaks != NULL) {
        free(layout->line_breaks);
    }
    if(layout->line_widths != NULL) {
        free(layout->line_widths);
    }

    /* Allocate maximum possible line breaks (one per element) */
    layout->line_breaks = (int *)calloc(layout->element_count + 1, sizeof(int));
    layout->line_widths = (int *)calloc(layout->element_count + 1, sizeof(int));

    if(layout->line_breaks == NULL || layout->line_widths == NULL) {
        return;
    }

    /* Cache text widths for all text elements */
    for(int i = 0; i < layout->element_count; i++) {
        if(layout->elements[i].type == TEXT_ELEMENT_TYPE_TEXT && layout->elements[i].text != NULL) {
            layout->elements[i].text_width = MeasureText(layout->elements[i].text, font_size);
        }
    }

    /* Calculate line breaks greedily */
    layout->line_count = 0;
    layout->line_breaks[0] = 0;  /* First line starts at element 0 */
    int current_line_width = 0;


    for(int i = 0; i < layout->element_count; i++) {
        /* Handle forced line breaks from explicit newlines */
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
            spacing = (current_line_width > 0) ? MeasureText(" ", font_size) : 0;
        } else { /* ICON */
            element_width = layout->elements[i].icon_size;
            spacing = (current_line_width > 0) ? ui_px(4) : 0; /* Small spacing around icons */
        }

        /* Check if element fits on current line */
        if(current_line_width + spacing + element_width <= max_width) {
            current_line_width += spacing + element_width;
        } else {
            /* Start new line */
            layout->line_count++;
            layout->line_breaks[layout->line_count] = i;
            layout->line_widths[layout->line_count - 1] = current_line_width;
            current_line_width = element_width;
        }
    }

    /* Store last line width and count */
    layout->line_widths[layout->line_count] = current_line_width;
    layout->line_count++;
    /* Calculate total height properly.
     * For N lines, we have N * font_size for text height + (N-1) * ui_px(4) for spacing between lines.
     * The last line doesn't have spacing after it. */
    if(layout->line_count > 0) {
        layout->total_height = layout->line_count * font_size + (layout->line_count - 1) * ui_px(4);
    } else {
        layout->total_height = 0;
    }

}

/* ================================================================
 * TEXT LAYOUT DRAWING
 * ================================================================ */

void
text_layout_draw(TextLayout *layout, int x, int *y, int font_size, Color color)
{
    if(layout == NULL || layout->elements == NULL || layout->element_count == 0) {
        return;
    }

    int current_x = x;
    int current_y = *y;
    int current_line = 0;


    for(int i = 0; i < layout->element_count; i++) {
        /* Handle forced line breaks from explicit newlines */
        if(layout->elements[i].type == TEXT_ELEMENT_TYPE_LINE_BREAK) {
            current_line++;
            current_y += font_size + ui_px(4);  /* Move to next line */
            current_x = x;
            continue;
        }

        /* Check if this element starts a new line */
        int is_line_break = 0;
        for(int j = 0; j < layout->line_count; j++) {
            if(layout->line_breaks[j] == i) {
                is_line_break = 1;
                break;
            }
        }

        if(is_line_break && i > 0) {
            current_line++;
            current_y += font_size + ui_px(4);  /* Move to next line */
            current_x = x;
        }

        if(layout->elements[i].type == TEXT_ELEMENT_TYPE_TEXT) {
            /* Draw text element */
            if(layout->elements[i].text != NULL && layout->elements[i].text[0] != '\0') {
                DrawText(layout->elements[i].text, current_x, current_y, font_size, color);
                current_x += layout->elements[i].text_width;
            }
        } else { /* ICON */
            /* Draw icon element */
            Texture2D icon = layout->elements[i].icon;
            int icon_size = layout->elements[i].icon_size;

            if(icon.id != 0) {
                Rectangle src = {0, 0, icon.width, icon.height};
                Rectangle dst = {current_x, current_y, (float)icon_size, (float)icon_size};
                DrawTexturePro(icon, src, dst, (Vector2){0}, 0, color);
            } else {
                /* Use fallback icon rendering */
                ui_draw_icon_fallback(layout->elements[i].icon_type, current_x, current_y, icon_size, color);
            }

            current_x += icon_size;
        }

        /* Add spacing after each element (except last in line) */
        current_x += (layout->elements[i].type == TEXT_ELEMENT_TYPE_TEXT) ?
                     MeasureText(" ", font_size) : ui_px(4);
    }

    /* Set final Y position to match total_height calculation.
     * After drawing all lines, current_y is at the top of the last line.
     * Add font_size to account for the height of the last line itself.
     * This matches: total_height = N*font_size + (N-1)*ui_px(4) */
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
