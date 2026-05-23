#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <linux/fb.h>

#define COLOR_BG      0xffd3f6ff
#define COLOR_PANEL   0xff75a8f9
#define COLOR_HOT     0xff6f6feb
#define COLOR_TEXT    0xff583f7c
#define COLOR_INNER   0xff75a8f9
#define COLOR_CIRCLE  0xff6f6feb
#define COLOR_WHITE   0xffffffff

void put_pixel(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, int x, int y, uint32_t color);
void draw_circle(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, int cx, int cy, int radius, uint32_t color);
void clear_screen(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, uint32_t color);
void draw_char(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, char c, int x, int y, int scale, uint32_t color);
void draw_string(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, const char *s, int x, int y, int scale, uint32_t color);
int string_width(const char *s, int scale);
int draw_button(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, int mx, int my, int click, int x, int y, const char *label, int scale, uint32_t bg, uint32_t hot_bg, uint32_t fg);
void draw_cursor(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, int x, int y);

#endif
