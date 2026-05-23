#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>
#include <linux/fb.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <linux/kd.h>
#include <termios.h>

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

typedef struct {
	int (*init)(struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo);
	void (*flip)(uint8_t *frame, size_t screen_size);
	int (*poll_input)(int *mx, int *my, int *click, int *need_redraw);
	void (*cleanup)(void);
} Backend;

/* ================= X11 Backend ================= */
static Display *x11_display;
static Window x11_window;
static GC x11_gc;
static XImage *x11_image;
static uint8_t *x11_framebuffer;
static uint8_t *x11_prev;
static int x11_mx, x11_my;

static int
x11_init(struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo)
{
	x11_display = XOpenDisplay(NULL);
	if(!x11_display)
		return -1;

	int screen = DefaultScreen(x11_display);

	vinfo->xres = 800;
	vinfo->yres = 600;
	vinfo->xoffset = 0;
	vinfo->yoffset = 0;
	vinfo->bits_per_pixel = 32;

	finfo->line_length = 800 * 4;
	finfo->smem_len = finfo->line_length * vinfo->yres;

	x11_framebuffer = malloc(finfo->smem_len);
	x11_prev = malloc(finfo->smem_len);
	if(!x11_framebuffer || !x11_prev) {
		free(x11_framebuffer);
		free(x11_prev);
		XCloseDisplay(x11_display);
		return -1;
	}
	memset(x11_prev, 0, finfo->smem_len);

	x11_window = XCreateSimpleWindow(
		x11_display,
		RootWindow(x11_display, screen),
		10, 10,
		vinfo->xres,
		vinfo->yres,
		1,
		BlackPixel(x11_display, screen),
		WhitePixel(x11_display, screen)
	);

	XStoreName(x11_display, x11_window, "Inner Breeze");
	XSelectInput(x11_display, x11_window,
		ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

	/* Hide cursor */
	static unsigned char cursor_data[1] = { 0 };
	Pixmap data = XCreateBitmapFromData(x11_display, DefaultRootWindow(x11_display),
		(char*)cursor_data, 1, 1);
	XColor color = { .pixel = BlackPixel(x11_display, screen), .red = 0, .green = 0, .blue = 0,
	                 .flags = DoRed | DoGreen | DoBlue };
	Cursor cursor = XCreatePixmapCursor(x11_display, data, data, &color, &color, 0, 0);
	XDefineCursor(x11_display, x11_window, cursor);
	XFreePixmap(x11_display, data);

	XMapWindow(x11_display, x11_window);

	x11_image = XCreateImage(
		x11_display,
		DefaultVisual(x11_display, screen),
		24,
		ZPixmap,
		0,
		(char *)x11_framebuffer,
		vinfo->xres,
		vinfo->yres,
		32,
		finfo->line_length
	);

	x11_gc = DefaultGC(x11_display, screen);
	x11_mx = vinfo->xres / 2;
	x11_my = vinfo->yres / 2;

	return 0;
}

static void
x11_flip(uint8_t *frame, size_t screen_size)
{
	(void)screen_size;
	/* Copy entire frame at once */
	memcpy(x11_framebuffer, frame, screen_size);
	XPutImage(x11_display, x11_window, x11_gc, x11_image,
		0, 0, 0, 0, x11_image->width, x11_image->height);
	memcpy(x11_prev, x11_framebuffer, screen_size);
}

static int
x11_poll_input(int *mx, int *my, int *click, int *need_redraw)
{
	XEvent event;

	while(XPending(x11_display) > 0) {
		XNextEvent(x11_display, &event);

		if(event.type == Expose) {
			*need_redraw = 1;
		}

		if(event.type == KeyPress) {
			KeySym key = XLookupKeysym(&event.xkey, 0);
			if(key == XK_q || key == XK_Q)
				return -1;  /* quit */
			if(key == XK_space || key == XK_Return) {
				*click = 1;
				*need_redraw = 1;
			}
		}

		if(event.type == ButtonPress) {
			if(event.xbutton.button == Button1) {
				*click = 1;
				*need_redraw = 1;
			}
		}

		if(event.type == MotionNotify) {
			*mx = event.xmotion.x;
			*my = event.xmotion.y;
			*need_redraw = 1;
		}
	}

	return 0;
}

static void
x11_cleanup(void)
{
	if(x11_image) XDestroyImage(x11_image);
	if(x11_display) XCloseDisplay(x11_display);
	if(x11_framebuffer) free(x11_framebuffer);
	if(x11_prev) free(x11_prev);
}

/* ================= Framebuffer Backend ================= */
static int fb_tty_fd = -1;
static int fb_mouse_fd = -1;
static int fb_fd = -1;
static uint8_t *fb_mem;
static uint8_t *fb_frame;
static struct termios fb_orig_termios;
static struct fb_var_screeninfo fb_vinfo;
static struct fb_fix_screeninfo fb_finfo;

static void fb_signal_cleanup(int sig);

static int
fb_init(struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo)
{
	signal(SIGINT, fb_signal_cleanup);
	signal(SIGTERM, fb_signal_cleanup);

	fb_tty_fd = open("/dev/tty0", O_RDWR | O_NONBLOCK);
	if(fb_tty_fd < 0)
		fb_tty_fd = open("/dev/tty", O_RDWR | O_NONBLOCK);

	if(fb_tty_fd >= 0){
		ioctl(fb_tty_fd, KDSETMODE, KD_GRAPHICS);
		tcgetattr(fb_tty_fd, &fb_orig_termios);
		struct termios raw = fb_orig_termios;
		raw.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(fb_tty_fd, TCSANOW, &raw);
	}

	fb_mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);

	fb_fd = open("/dev/fb0", O_RDWR);
	if(fb_fd < 0)
		return -1;

	if(ioctl(fb_fd, FBIOGET_FSCREENINFO, &fb_finfo) == -1 ||
	   ioctl(fb_fd, FBIOGET_VSCREENINFO, &fb_vinfo) == -1) {
		close(fb_fd);
		return -1;
	}

	long screen_size = fb_finfo.smem_len;
	fb_mem = mmap(0, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
	if(fb_mem == MAP_FAILED) {
		close(fb_fd);
		return -1;
	}

	fb_frame = malloc((size_t)screen_size);
	if(!fb_frame) {
		munmap(fb_mem, screen_size);
		close(fb_fd);
		return -1;
	}
	memset(fb_frame, 0, (size_t)screen_size);

	*vinfo = fb_vinfo;
	*finfo = fb_finfo;

	return 0;
}

static void
fb_flip(uint8_t *frame, size_t screen_size)
{
	memcpy(fb_mem, frame, screen_size);
}

static int
fb_poll_input(int *mx, int *my, int *click, int *need_redraw)
{
	char buf;
	char mouse_buf[3];
	static int mouse_down = 0;

	if(read(fb_tty_fd, &buf, 1) > 0){
		if(buf == 'q' || buf == 'Q')
			return -1;
		if(buf == ' ' || buf == '\n'){
			*click = 1;
			*need_redraw = 1;
		}
	}

	if(fb_mouse_fd >= 0){
		while(read(fb_mouse_fd, mouse_buf, 3) == 3){
			signed char dx = mouse_buf[1];
			signed char dy = -mouse_buf[2];
			int new_buttons = mouse_buf[0] & 0x07;

			*mx += dx;
			*my += dy;

			if(*mx < 0) *mx = 0;
			if(*mx >= (int)fb_vinfo.xres) *mx = fb_vinfo.xres - 1;
			if(*my < 0) *my = 0;
			if(*my >= (int)fb_vinfo.yres) *my = fb_vinfo.yres - 1;

			if(dx != 0 || dy != 0)
				*need_redraw = 1;

			if((new_buttons & 0x01) && !(mouse_down & 0x01)){
				*click = 1;
				*need_redraw = 1;
			}

			mouse_down = new_buttons;
		}
	}

	return 0;
}

static void
fb_signal_cleanup(int sig)
{
	(void)sig;
	if(fb_tty_fd >= 0){
		ioctl(fb_tty_fd, KDSETMODE, KD_TEXT);
		tcsetattr(fb_tty_fd, TCSANOW, &fb_orig_termios);
		close(fb_tty_fd);
	}
	if(fb_mouse_fd >= 0)
		close(fb_mouse_fd);
	_exit(0);
}

static void
fb_cleanup(void)
{
	if(fb_tty_fd >= 0){
		ioctl(fb_tty_fd, KDSETMODE, KD_TEXT);
		tcsetattr(fb_tty_fd, TCSANOW, &fb_orig_termios);
		close(fb_tty_fd);
	}
	if(fb_mouse_fd >= 0)
		close(fb_mouse_fd);
	if(fb_frame) free(fb_frame);
	if(fb_mem) munmap(fb_mem, fb_finfo.smem_len);
	if(fb_fd >= 0) close(fb_fd);
}

/* ================= Main ================= */
static Backend x11_backend = {
	.init = x11_init,
	.flip = x11_flip,
	.poll_input = x11_poll_input,
	.cleanup = x11_cleanup
};

static Backend fb_backend = {
	.init = fb_init,
	.flip = fb_flip,
	.poll_input = fb_poll_input,
	.cleanup = fb_cleanup
};

int
main(void)
{
	Backend *backend;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	uint8_t *frame;
	int mx, my, click, need_redraw;
	Lotus lotus;
	const uint64_t target_ns = 16666666ULL;

	/* Detect backend */
	if(getenv("DISPLAY")) {
		backend = &x11_backend;
	} else {
		backend = &fb_backend;
	}

	/* Initialize backend */
	if(backend->init(&vinfo, &finfo) < 0)
		return 1;

	/* Allocate frame buffer */
	frame = calloc(1, finfo.smem_len);
	if(!frame) {
		backend->cleanup();
		return 1;
	}

	/* Initialize game state */
	lotusinit(&lotus);
	lotus.speed = 1;
	lotus.breathtickmax = 2;
	lotus.screen = LotusScreenStart;

	mx = vinfo.xres / 2;
	my = vinfo.yres / 2;
	click = 0;
	need_redraw = 1;

	/* Main loop */
	while(1){
		uint64_t frame_start = get_time_ns();

		/* Poll input */
		int input_result = backend->poll_input(&mx, &my, &click, &need_redraw);
		if(input_result < 0)
			break;

		/* Render */
		if(need_redraw || lotus.screen == LotusScreenSession){
			render(frame, &vinfo, &finfo, &lotus, mx, my, &click);
			draw_cursor(frame, &vinfo, &finfo, mx, my);
			backend->flip(frame, finfo.smem_len);
			need_redraw = 0;
		}

		/* Frame rate limiting */
		uint64_t frame_time = get_time_ns() - frame_start;
		if(frame_time < target_ns)
			usleep((useconds_t)((target_ns - frame_time) / 1000));
	}

	/* Cleanup */
	free(frame);
	backend->cleanup();
	return 0;
}
