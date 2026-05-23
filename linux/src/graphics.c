#include <stdint.h>
#include <linux/fb.h>
#include <string.h>
#include "../liblotus/font8x8.h"
#include "graphics.h"

static int
slen(const char *s)
{
	int n;
	n = 0;
	while(s[n] != 0)
		n++;
	return n;
}

void
put_pixel(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, int x, int y, uint32_t color)
{
	if(x < 0 || y < 0)
		return;
	if(x >= (int)vinfo->xres || y >= (int)vinfo->yres)
		return;

	int bytes_per_pixel = vinfo->bits_per_pixel / 8;
	long location = (x + vinfo->xoffset) * bytes_per_pixel + (y + vinfo->yoffset) * finfo->line_length;

	if(bytes_per_pixel == 4){
		*(uint32_t *)(fb + location) = color;
	}else if(bytes_per_pixel == 2){
		uint8_t r = (color >> 16) & 0xff;
		uint8_t g = (color >> 8) & 0xff;
		uint8_t b = color & 0xff;
		uint16_t rgb565 = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
		*(uint16_t *)(fb + location) = rgb565;
	}
}

void
draw_circle(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, int cx, int cy, int radius, uint32_t color)
{
	int x = radius;
	int y = 0;
	int decision = 1 - radius;

	while(x >= y){
		for(int i = cx - x; i <= cx + x; i++){
			put_pixel(fb, vinfo, finfo, i, cy + y, color);
			put_pixel(fb, vinfo, finfo, i, cy - y, color);
		}
		for(int i = cx - y; i <= cx + y; i++){
			put_pixel(fb, vinfo, finfo, i, cy + x, color);
			put_pixel(fb, vinfo, finfo, i, cy - x, color);
		}
		y++;
		if(decision <= 0)
			decision += 2 * y + 1;
		else{
			x--;
			decision += 2 * (y - x) + 1;
		}
	}
}

void
clear_screen(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, uint32_t color)
{
	int bytes_per_pixel = vinfo->bits_per_pixel / 8;
	long total_pixels = (long)vinfo->xres * (long)vinfo->yres;

	if(bytes_per_pixel == 4){
		uint32_t *fb32 = (uint32_t *)fb;
		for(long i = 0; i < total_pixels; i++)
			fb32[i] = color;
	}else if(bytes_per_pixel == 2){
		uint8_t r = (color >> 16) & 0xff;
		uint8_t g = (color >> 8) & 0xff;
		uint8_t b = color & 0xff;
		uint16_t rgb565 = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
		uint16_t *fb16 = (uint16_t *)fb;
		for(long i = 0; i < total_pixels; i++)
			fb16[i] = rgb565;
	}
}

void
draw_char(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, char c, int x, int y, int scale, uint32_t color)
{
	const uint8_t *glyph = font_8x8[(unsigned char)c];
	int bytes_per_pixel = vinfo->bits_per_pixel / 8;

	if(bytes_per_pixel == 4){
		for(int row = 0; row < 8; row++){
			uint8_t rowbits = glyph[row];
			for(int col = 0; col < 8; col++){
				if(rowbits & (1 << (7 - col))){
					int px = x + col * scale;
					int py = y + row * scale;
					for(int sy = 0; sy < scale && py + sy < (int)vinfo->yres; sy++){
						if(py + sy < 0) continue;
						int clip_x = px < 0 ? 0 : px;
						int clip_w = scale;
						if(px < 0) clip_w += px;
						if(px + scale > (int)vinfo->xres) clip_w = vinfo->xres - px;
						if(clip_w > 0){
							uint32_t *dst = (uint32_t *)(fb + (py + sy) * finfo->line_length + clip_x * 4);
							for(int sx = 0; sx < clip_w; sx++)
								dst[sx] = color;
						}
					}
				}
			}
		}
	}else if(bytes_per_pixel == 2){
		uint8_t r = (color >> 16) & 0xff;
		uint8_t g = (color >> 8) & 0xff;
		uint8_t b = color & 0xff;
		uint16_t rgb565 = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
		for(int row = 0; row < 8; row++){
			uint8_t rowbits = glyph[row];
			for(int col = 0; col < 8; col++){
				if(rowbits & (1 << (7 - col))){
					int px = x + col * scale;
					int py = y + row * scale;
					for(int sy = 0; sy < scale && py + sy < (int)vinfo->yres; sy++){
						if(py + sy < 0) continue;
						int clip_x = px < 0 ? 0 : px;
						int clip_w = scale;
						if(px < 0) clip_w += px;
						if(px + scale > (int)vinfo->xres) clip_w = vinfo->xres - px;
						if(clip_w > 0){
							uint16_t *dst = (uint16_t *)(fb + (py + sy) * finfo->line_length + clip_x * 2);
							for(int sx = 0; sx < clip_w; sx++)
								dst[sx] = rgb565;
						}
					}
				}
			}
		}
	}
}

int
string_width(const char *s, int scale)
{
	return slen(s) * 8 * scale;
}

void
draw_string(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, const char *s, int x, int y, int scale, uint32_t color)
{
	int w = string_width(s, scale);
	int h = 8 * scale;
	int cx = x - w / 2;
	int cy = y - h / 2;

	for(int i = 0; s[i] != 0; i++){
		draw_char(fb, vinfo, finfo, s[i], cx, cy, scale, color);
		cx += 8 * scale;
	}
}

static void
draw_rect(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, int x, int y, int w, int h, uint32_t color)
{
	int bytes_per_pixel = vinfo->bits_per_pixel / 8;

	if(bytes_per_pixel == 4){
		for(int yy = y; yy < y + h; yy++){
			if(yy >= 0 && yy < (int)vinfo->yres){
				int clip_x = x < 0 ? 0 : x;
				int clip_w = w;
				if(x < 0) clip_w += x;
				if(x + w > (int)vinfo->xres) clip_w = vinfo->xres - x;
				if(clip_w > 0){
					uint32_t *dst = (uint32_t *)(fb + yy * finfo->line_length + clip_x * 4);
					for(int xx = 0; xx < clip_w; xx++)
						dst[xx] = color;
				}
			}
		}
	}else if(bytes_per_pixel == 2){
		uint8_t r = (color >> 16) & 0xff;
		uint8_t g = (color >> 8) & 0xff;
		uint8_t b = color & 0xff;
		uint16_t rgb565 = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
		for(int yy = y; yy < y + h; yy++){
			if(yy >= 0 && yy < (int)vinfo->yres){
				int clip_x = x < 0 ? 0 : x;
				int clip_w = w;
				if(x < 0) clip_w += x;
				if(x + w > (int)vinfo->xres) clip_w = vinfo->xres - x;
				if(clip_w > 0){
					uint16_t *dst = (uint16_t *)(fb + yy * finfo->line_length + clip_x * 2);
					for(int xx = 0; xx < clip_w; xx++)
						dst[xx] = rgb565;
				}
			}
		}
	}
}

int
draw_button(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, int mx, int my, int click, int x, int y, const char *label, int scale, uint32_t bg, uint32_t hot_bg, uint32_t fg)
{
	int w = slen(label) * 8 * scale * 2;
	int h = 10 * scale * 2;
	int left = x - w / 2;
	int top = y;
	int hot = mx > left && mx < left + w && my > top && my < top + h;

	draw_rect(fb, vinfo, finfo, left, top, w, h, hot ? hot_bg : bg);
	draw_string(fb, vinfo, finfo, label, x, top + h / 2, scale, fg);

	return hot && click;
}

void
draw_cursor(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, int x, int y)
{
	static const char *shape[] = {
		"X...........",
		"XX..........",
		"X.X.........",
		"X..X........",
		"X...X.......",
		"X....X......",
		"X.....X.....",
		"X......X....",
		"X.......X...",
		"X....XXXX...",
		"X..X.X......",
		"XX...X......",
		"X.....X.....",
		"......X.....",
		".......X...."
	};
	int rows = (int)(sizeof(shape) / sizeof(shape[0]));
	int bytes_per_pixel = vinfo->bits_per_pixel / 8;

	/* Precompute edge pixels */
	char edge[17][14] = {0};
	for(int yy = -1; yy <= rows; yy++){
		for(int xx = -1; xx <= 12; xx++){
			for(int oy = -1; oy <= 1; oy++){
				for(int ox = -1; ox <= 1; ox++){
					int sy = yy + oy;
					int sx = xx + ox;
					if(sy >= 0 && sy < rows && sx >= 0 && sx < 12 && shape[sy][sx] == 'X')
						edge[yy + 1][xx + 1] = 1;
				}
			}
		}
	}

	if(bytes_per_pixel == 4){
		/* Draw white edge */
		for(int yy = 0; yy < rows + 2; yy++){
			for(int xx = 0; xx < 14; xx++){
				if(edge[yy][xx] && !(yy > 0 && yy <= rows && xx > 0 && xx <= 12 && shape[yy-1][xx-1] == 'X')){
					int px = x + xx - 1;
					int py = y + yy - 1;
					if(px >= 0 && px < (int)vinfo->xres && py >= 0 && py < (int)vinfo->yres)
						*(uint32_t *)(fb + py * finfo->line_length + px * 4) = COLOR_WHITE;
				}
			}
		}
		/* Draw main cursor */
		for(int yy = 0; yy < rows; yy++){
			for(int xx = 0; xx < 12; xx++){
				if(shape[yy][xx] == 'X'){
					int px = x + xx;
					int py = y + yy;
					if(px >= 0 && px < (int)vinfo->xres && py >= 0 && py < (int)vinfo->yres)
						*(uint32_t *)(fb + py * finfo->line_length + px * 4) = COLOR_TEXT;
				}
			}
		}
	}else if(bytes_per_pixel == 2){
		uint16_t white565 = ((0xff & 0xf8) << 8) | ((0xff & 0xfc) << 3) | (0xff >> 3);
		uint8_t tr = (COLOR_TEXT >> 16) & 0xff;
		uint8_t tg = (COLOR_TEXT >> 8) & 0xff;
		uint8_t tb = COLOR_TEXT & 0xff;
		uint16_t text565 = ((tr & 0xf8) << 8) | ((tg & 0xfc) << 3) | (tb >> 3);

		for(int yy = 0; yy < rows + 2; yy++){
			for(int xx = 0; xx < 14; xx++){
				if(edge[yy][xx] && !(yy > 0 && yy <= rows && xx > 0 && xx <= 12 && shape[yy-1][xx-1] == 'X')){
					int px = x + xx - 1;
					int py = y + yy - 1;
					if(px >= 0 && px < (int)vinfo->xres && py >= 0 && py < (int)vinfo->yres)
						*(uint16_t *)(fb + py * finfo->line_length + px * 2) = white565;
				}
			}
		}
		for(int yy = 0; yy < rows; yy++){
			for(int xx = 0; xx < 12; xx++){
				if(shape[yy][xx] == 'X'){
					int px = x + xx;
					int py = y + yy;
					if(px >= 0 && px < (int)vinfo->xres && py >= 0 && py < (int)vinfo->yres)
						*(uint16_t *)(fb + py * finfo->line_length + px * 2) = text565;
				}
			}
		}
	}
}
