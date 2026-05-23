#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdint.h>
#include <linux/fb.h>
#include <time.h>
#include <unistd.h>
#include "graphics.h"
#include "common.h"
#include "../liblotus/lotus.h"

static uint64_t
get_time_ns(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
}

static Cursor
create_hidden_cursor(Display *display)
{
	static unsigned char cursor_data[1] = { 0 };
	Cursor cursor;
	Pixmap data;
	XColor color;

	data = XCreateBitmapFromData(display, DefaultRootWindow(display), (char*)cursor_data, 1, 1);

	color.pixel = BlackPixel(display, DefaultScreen(display));
	color.red = color.green = color.blue = 0;
	color.flags = DoRed | DoGreen | DoBlue;

	cursor = XCreatePixmapCursor(display, data, data, &color, &color, 0, 0);

	XFreePixmap(display, data);

	return cursor;
}

static int
quiet_x_io_error(Display *display)
{
	(void)display;
	_exit(0);
	return 0;
}

static void
put_changed_pixels(Display *display, Window window, GC gc, XImage *x_image,
                   uint8_t *frame, uint8_t *prev,
                   struct fb_var_screeninfo *vinfo,
                   struct fb_fix_screeninfo *finfo, int first_frame)
{
	int bytes_per_pixel = vinfo->bits_per_pixel / 8;
	size_t row_bytes = (size_t)vinfo->xres * (size_t)bytes_per_pixel;

	if(bytes_per_pixel <= 0)
		return;

	for(int y = 0; y < (int)vinfo->yres; y++){
		size_t row = (size_t)y * (size_t)finfo->line_length;

		if(first_frame || memcmp(frame + row, prev + row, row_bytes) != 0){
			int x = 0;

			while(x < (int)vinfo->xres){
				size_t pixel = row + (size_t)x * (size_t)bytes_per_pixel;

				if(!first_frame && memcmp(frame + pixel, prev + pixel, bytes_per_pixel) == 0){
					x++;
					continue;
				}

				int start = x;

				while(x < (int)vinfo->xres){
					pixel = row + (size_t)x * (size_t)bytes_per_pixel;
					if(!first_frame && memcmp(frame + pixel, prev + pixel, bytes_per_pixel) == 0)
						break;
					x++;
				}

				size_t offset = row + (size_t)start * (size_t)bytes_per_pixel;
				int width = x - start;

				XPutImage(display, window, gc, x_image, start, y, start, y, (unsigned int)width, 1);
				memcpy(prev + offset, frame + offset, (size_t)width * (size_t)bytes_per_pixel);
			}
		}
	}
}

int
main(void)
{
	XSetIOErrorHandler(quiet_x_io_error);

	struct fb_var_screeninfo vinfo = {
		.xres = 800,
		.yres = 600,
		.xoffset = 0,
		.yoffset = 0,
		.bits_per_pixel = 32
	};
	struct fb_fix_screeninfo finfo = {
		.line_length = 800 * 4
	};
	finfo.smem_len = finfo.line_length * vinfo.yres;

	uint8_t *fb = malloc(finfo.smem_len);
	if (!fb) return 1;

	uint8_t *prev = malloc(finfo.smem_len);
	if (!prev) {
		free(fb);
		return 1;
	}
	memset(prev, 0, finfo.smem_len);

	Display *display = XOpenDisplay(NULL);
	if (!display) {
		free(prev);
		free(fb);
		return 1;
	}

	int screen = DefaultScreen(display);
	Window window = XCreateSimpleWindow(
		display,
		RootWindow(display, screen),
		10, 10,
		vinfo.xres,
		vinfo.yres,
		1,
		BlackPixel(display, screen),
		WhitePixel(display, screen)
	);

	XStoreName(display, window, "Inner Breeze");
	XSelectInput(display, window, ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

	Cursor cursor = create_hidden_cursor(display);
	XDefineCursor(display, window, cursor);
	XMapWindow(display, window);

	XImage *x_image = XCreateImage(
		display,
		DefaultVisual(display, screen),
		24,
		ZPixmap,
		0,
		(char *)fb,
		vinfo.xres,
		vinfo.yres,
		32,
		finfo.line_length
	);

	GC gc = DefaultGC(display, screen);
	XEvent event;

	Lotus lotus;
	lotusinit(&lotus);
	lotus.speed = 1;
	lotus.breathtickmax = 2;
	lotus.screen = LotusScreenStart;

	int mx = vinfo.xres / 2;
	int my = vinfo.yres / 2;

	const uint64_t target_ns = 16666666ULL;
	int need_redraw = 1;
	int first_frame = 1;

	while (1) {
		uint64_t frame_start = get_time_ns();

		int click = 0;
		while (XPending(display) > 0) {
			XNextEvent(display, &event);

			if (event.type == Expose) {
				need_redraw = 1;
			}

			if (event.type == KeyPress) {
				KeySym key = XLookupKeysym(&event.xkey, 0);
				if (key == XK_q || key == XK_Q) {
					goto cleanup;
				}
				if (key == XK_space || key == XK_Return) {
					click = 1;
					need_redraw = 1;
				}
			}

			if (event.type == ButtonPress) {
				if (event.xbutton.button == Button1) {
					click = 1;
					need_redraw = 1;
				}
			}

			if (event.type == MotionNotify) {
				mx = event.xmotion.x;
				my = event.xmotion.y;
				need_redraw = 1;
			}
		}

		if (need_redraw || lotus.screen == LotusScreenSession) {
			render(fb, &vinfo, &finfo, &lotus, mx, my, &click);
			draw_cursor(fb, &vinfo, &finfo, mx, my);
			put_changed_pixels(display, window, gc, x_image, fb, prev,
			                   &vinfo, &finfo, first_frame);
			first_frame = 0;
			need_redraw = 0;
		}

		uint64_t frame_time = get_time_ns() - frame_start;
		if (frame_time < target_ns) {
			usleep((useconds_t)((target_ns - frame_time) / 1000));
		}
	}

cleanup:
	XDestroyImage(x_image);
	XFreeCursor(display, cursor);
	XCloseDisplay(display);
	free(prev);
	return 0;
}
