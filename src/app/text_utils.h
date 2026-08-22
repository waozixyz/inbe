#ifndef INBE_TEXT_UTILS_H
#define INBE_TEXT_UTILS_H

#include "kryon.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static inline void
inbe_text_fit_ellipsis(const char *text, char *out, size_t out_size, int max_w,
                       int font)
{
    if(out == NULL || out_size == 0)
        return;
    if(text == NULL)
        text = "";
    snprintf(out, out_size, "%s", text);
    if(max_w <= 0)
        return;
    while(out[0] != '\0' && TextWidth(out, font) > max_w) {
        size_t len = strlen(out);
        size_t cut = len;

        if(len <= 3)
            break;
        if(len > 3 && strcmp(out + len - 3, "...") == 0)
            len -= 3;
        cut = len;
        do {
            cut--;
        } while(cut > 0 && (((unsigned char)out[cut] & 0xC0) == 0x80));
        out[cut] = '\0';
        if(cut + 3 < out_size)
            strncat(out, "...", out_size - strlen(out) - 1);
    }
}

static inline const char *
inbe_text_fit_ellipsis_into(const char *text, char *out, size_t out_size, int max_w,
                            int font)
{
    if(text == NULL)
        text = "";
    if(max_w > 0 && TextWidth(text, font) <= max_w)
        return text;
    inbe_text_fit_ellipsis(text, out, out_size, max_w, font);
    return out;
}

static inline void
inbe_text_short_label(const char *text, int max_codepoints, int append_ellipsis,
                      char *out, size_t out_size)
{
    size_t len;
    size_t cut = 0;
    int chars = 0;
    int truncated = 0;

    if(out == NULL || out_size == 0)
        return;
    if(text == NULL)
        text = "";
    if(max_codepoints < 0)
        max_codepoints = 0;
    len = strlen(text);
    while(cut < len && chars < max_codepoints) {
        cut++;
        while(cut < len && (((unsigned char)text[cut] & 0xC0) == 0x80))
            cut++;
        chars++;
    }
    truncated = cut < len;
    if(cut >= out_size) {
        cut = out_size - 1;
        while(cut > 0 && (((unsigned char)text[cut] & 0xC0) == 0x80))
            cut--;
    }
    memcpy(out, text, cut);
    out[cut] = '\0';
    if(append_ellipsis && truncated && strlen(out) + 3 < out_size)
        strncat(out, "...", out_size - strlen(out) - 1);
}

#endif
