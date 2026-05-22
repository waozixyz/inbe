#include <android/native_window.h>
#include <string.h>

#include "font8x8.h"

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
drawchar(ANativeWindow_Buffer *b, unsigned char c, int x, int y, int scale, uint32_t color)
{
	const uint8_t *glyph;
	uint32_t *pixels;
	uint32_t stride;
	uint8_t rowbits;
	int row;
	int col;
	int sx;
	int sy;
	int px;
	int py;

	glyph = font_8x8[c];
	pixels = (uint32_t*)b->bits;
	stride = b->stride;

	for(row = 0; row < 8; row++){
		rowbits = glyph[row];

		for(col = 0; col < 8; col++){
			if(rowbits & (1 << (7 - col))){
				for(sy = 0; sy < scale; sy++){
					for(sx = 0; sx < scale; sx++){
						px = x + col * scale + sx;
						py = y + row * scale + sy;

						if(px >= 0 && px < b->width && py >= 0 && py < b->height)
							pixels[py * stride + px] = color;
					}
				}
			}
		}
	}
}

void
drawstring(ANativeWindow_Buffer *b, const char *s, int x, int y, int scale, uint32_t color)
{
	int w;
	int h;
	int cx;
	int i;

	w = strlen(s) * 8 * scale;
	h = 8 * scale;
	cx = x - w / 2;
	y = y - h / 2;

	for(i = 0; s[i] != 0; i++){
		drawchar(b, (unsigned char)s[i], cx, y, scale, color);
		cx += 8 * scale;
	}
}


void
drawcircle(ANativeWindow_Buffer *b, int cx, int cy, int r, uint32_t color)
{
	uint32_t *pixels;
	uint32_t stride;
	int r2;
	int x;
	int y;
	int dx;
	int dy;

	pixels = (uint32_t*)b->bits;
	stride = b->stride;
	r2 = r * r;

	for(y = cy - r; y <= cy + r; y++){
		for(x = cx - r; x <= cx + r; x++){
			if(x >= 0 && x < b->width && y >= 0 && y < b->height){
				dx = x - cx;
				dy = y - cy;

				if(dx * dx + dy * dy <= r2)
					pixels[y * stride + x] = color;
			}
		}
	}
}

clearbuffer(ANativeWindow_Buffer *b, uint32_t color)
{
	uint32_t *pixels;
	uint32_t stride;
	int x;
	int y;

	pixels = (uint32_t*)b->bits;
	stride = b->stride;

	for(y = 0; y < b->height; y++){
		for(x = 0; x < b->width; x++)
			pixels[y * stride + x] = color;
	}
}

void
drawrect(ANativeWindow_Buffer *b, int x, int y, int w, int h, uint32_t color)
{
	uint32_t *pixels;
	uint32_t stride;
	int xx;
	int yy;

	pixels = (uint32_t*)b->bits;
	stride = b->stride;

	for(yy = y; yy < y + h; yy++){
		for(xx = x; xx < x + w; xx++){
			if(xx >= 0 && xx < b->width && yy >= 0 && yy < b->height)
				pixels[yy * stride + xx] = color;
		}
	}
}

int
drawbtn(ANativeWindow_Buffer *b, int mx, int my, int down, int x, int y, const char *label, int scale, uint32_t bg, uint32_t hotbg, uint32_t fg)
{
	int w;
	int h;
	int left;
	int top;
	int hot;

	w = slen(label) * 8 * scale * 2;
	h = 10 * scale * 2;

	left = x - w / 2;
	top = y;

	hot = mx > left && mx < left + w && my > top && my < top + h;

	if(hot)
		drawrect(b, left, top, w, h, hotbg);
	else
		drawrect(b, left, top, w, h, bg);

	drawstring(b, label, x, top + h / 2, scale, fg);

	if(hot && down)
		return 1;

	return 0;
}
