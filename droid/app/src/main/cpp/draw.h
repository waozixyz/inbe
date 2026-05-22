#ifndef DRAW_H
#define DRAW_H

#include <android/native_window.h>
#include <stdint.h>

void clearbuffer(ANativeWindow_Buffer *b, uint32_t color);
void drawchar(ANativeWindow_Buffer *b, unsigned char c, int x, int y, int scale, uint32_t color);
void drawstring(ANativeWindow_Buffer *b, const char *s, int x, int y, int scale, uint32_t color);
void drawcircle(ANativeWindow_Buffer *b, int cx, int cy, int r, uint32_t color);
void drawrect(ANativeWindow_Buffer *b, int x, int y, int w, int h, uint32_t color);


int drawbtn(
	ANativeWindow_Buffer *b,
	int mx,
	int my,
	int down,
	int x,
	int y,
	const char *label,
	int scale,
	uint32_t bg,
	uint32_t hotbg,
	uint32_t fg
);

#endif
