#include "flint_text_layout.h"

#include "flint_scaling.h"
#include "flint_ui.h"

#include <stdlib.h>
#include <string.h>

FlintTextLayout
flint_text_layout_parse(const char *input, Texture2D icon, UIIconType icon_type, int icon_size)
{
    FlintTextLayout layout = {0};

    if(input == NULL || input[0] == '\0')
        return layout;

    int element_count = 0;
    const char *p = input;
    while(*p) {
        if(strncmp(p, "%i", 2) == 0) {
            element_count++;
            p += 2;
        } else if(*p == '\n') {
            element_count++;
            p++;
        } else {
            const char *word_start = p;
            while(*p && *p != ' ' && *p != '\n' && strncmp(p, "%i", 2) != 0)
                p++;
            if(p > word_start)
                element_count++;
            if(*p == ' ')
                p++;
        }
    }

    layout.elements = (FlintTextElement *)calloc((size_t)element_count, sizeof(FlintTextElement));
    if(layout.elements == NULL)
        return layout;
    layout.element_count = element_count;

    int element_idx = 0;
    p = input;
    while(*p && element_idx < element_count) {
        if(strncmp(p, "%i", 2) == 0) {
            layout.elements[element_idx].type = FLINT_TEXT_ELEMENT_TYPE_ICON;
            layout.elements[element_idx].icon = icon;
            layout.elements[element_idx].icon_type = icon_type;
            layout.elements[element_idx].icon_size = icon_size;
            element_idx++;
            p += 2;
        } else if(*p == '\n') {
            layout.elements[element_idx].type = FLINT_TEXT_ELEMENT_TYPE_LINE_BREAK;
            element_idx++;
            p++;
        } else {
            const char *word_start = p;
            while(*p && *p != ' ' && *p != '\n' && strncmp(p, "%i", 2) != 0)
                p++;
            if(p > word_start) {
                size_t len = (size_t)(p - word_start);
                char *text_copy = (char *)malloc(len + 1);
                if(text_copy != NULL) {
                    memcpy(text_copy, word_start, len);
                    text_copy[len] = '\0';
                    layout.elements[element_idx].type = FLINT_TEXT_ELEMENT_TYPE_TEXT;
                    layout.elements[element_idx].text = text_copy;
                    element_idx++;
                }
            }
            if(*p == ' ')
                p++;
        }
    }

    return layout;
}

void
flint_text_layout_reflow(FlintTextLayout *layout, int max_width, int font_size, int line_height)
{
    if(layout == NULL || layout->elements == NULL || layout->element_count == 0)
        return;

    int *new_line_breaks = (int *)calloc((size_t)layout->element_count + 1, sizeof(int));
    int *new_line_widths = (int *)calloc((size_t)layout->element_count + 1, sizeof(int));
    if(new_line_breaks == NULL || new_line_widths == NULL) {
        free(new_line_breaks);
        free(new_line_widths);
        return;
    }

    free(layout->line_breaks);
    free(layout->line_widths);
    layout->line_breaks = new_line_breaks;
    layout->line_widths = new_line_widths;

    /* Initialize all line_breaks to -1 (invalid) except line_breaks[0] */
    for(int i = 1; i < layout->element_count + 1; i++) {
        layout->line_breaks[i] = -1;
    }

    for(int i = 0; i < layout->element_count; i++) {
        if(layout->elements[i].type == FLINT_TEXT_ELEMENT_TYPE_TEXT && layout->elements[i].text != NULL)
            layout->elements[i].text_width = flint_text_measure(layout->elements[i].text, font_size);
    }

    int space_width = flint_text_measure(" ", font_size);
    int icon_spacing = flint_px(4);
    layout->line_count = 0;
    layout->line_breaks[0] = 0;
    int current_line_width = 0;

    for(int i = 0; i < layout->element_count; i++) {
        if(layout->elements[i].type == FLINT_TEXT_ELEMENT_TYPE_LINE_BREAK) {
            layout->line_count++;
            layout->line_breaks[layout->line_count] = i;
            layout->line_widths[layout->line_count - 1] = current_line_width;
            current_line_width = 0;
            continue;
        }

        int element_width = 0;
        int spacing = 0;
        if(layout->elements[i].type == FLINT_TEXT_ELEMENT_TYPE_TEXT) {
            element_width = layout->elements[i].text_width;
            spacing = (current_line_width > 0) ? space_width : 0;
        } else {
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
    {
        int drawn_line_height = flint_text_line_height(font_size);
        layout->total_height = layout->line_count > 0
                                   ? layout->line_count * drawn_line_height +
                                     (layout->line_count - 1) * line_height
                                   : 0;
    }
    layout->line_height = line_height;  /* Store for later use in draw */
}

void
flint_text_layout_draw(FlintTextLayout *layout, int x, int *y, int font_size, Color color)
{
    if(layout == NULL || layout->elements == NULL || layout->element_count == 0)
        return;

    int current_x = x;
    int current_y = *y;
    int space_width = flint_text_measure(" ", font_size);
    int icon_spacing = flint_px(4);
    int line_spacing = (layout->line_height > 0) ? layout->line_height : flint_px(4);
    int drawn_line_height = flint_text_line_height(font_size);
    int next_break_line = 1;

    for(int i = 0; i < layout->element_count; i++) {
        if(next_break_line <= layout->line_count && next_break_line > 0 && i == layout->line_breaks[next_break_line]) {
            current_y += drawn_line_height + line_spacing;
            current_x = x;
            next_break_line++;
        }

        if(layout->elements[i].type == FLINT_TEXT_ELEMENT_TYPE_LINE_BREAK)
            continue;

        int spacing = 0;
        if(current_x > x)
            spacing = (layout->elements[i].type == FLINT_TEXT_ELEMENT_TYPE_TEXT) ? space_width : icon_spacing;
        current_x += spacing;

        if(layout->elements[i].type == FLINT_TEXT_ELEMENT_TYPE_TEXT) {
            if(layout->elements[i].text != NULL && layout->elements[i].text[0] != '\0') {
                flint_text_draw(layout->elements[i].text, current_x, current_y, font_size, color);
                current_x += layout->elements[i].text_width;
            }
        } else {
            Texture2D icon = layout->elements[i].icon;
            int icon_size = layout->elements[i].icon_size;
            if(icon.id != 0) {
                Rectangle src = {0, 0, (float)icon.width, (float)icon.height};
                Rectangle dst = {(float)current_x, (float)current_y, (float)icon_size, (float)icon_size};
                DrawTexturePro(icon, src, dst, (Vector2){0}, 0, color);
            }
            current_x += icon_size;
        }
    }

    *y = current_y + drawn_line_height;
}

int
flint_text_layout_get_height(FlintTextLayout *layout)
{
    if(layout == NULL)
        return 0;
    return layout->total_height;
}

void
flint_text_layout_free(FlintTextLayout *layout)
{
    if(layout == NULL)
        return;

    if(layout->elements != NULL) {
        for(int i = 0; i < layout->element_count; i++) {
            if(layout->elements[i].type == FLINT_TEXT_ELEMENT_TYPE_TEXT && layout->elements[i].text != NULL)
                free((void *)layout->elements[i].text);
        }
        free(layout->elements);
    }
    free(layout->line_breaks);
    free(layout->line_widths);
    memset(layout, 0, sizeof(*layout));
}
