#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/kd.h>
#include <signal.h>
#include <stdint.h>
#include <linux/fb.h>
#include <termios.h>
#include <time.h>
#include "graphics.h"
#include "common.h"
#include "../liblotus/lotus.h"

static int tty_fd = -1;
static int mouse_fd = -1;
static struct termios orig_termios;

static void
cleanup(int sig)
{
	(void)sig;
	if(tty_fd >= 0){
		ioctl(tty_fd, KDSETMODE, KD_TEXT);
		tcsetattr(tty_fd, TCSANOW, &orig_termios);
		close(tty_fd);
	}
	if(mouse_fd >= 0)
		close(mouse_fd);
	_exit(0);
}

static uint64_t
get_time_ns(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
}

static void
flush_changed_pixels(uint8_t *fb, uint8_t *frame, uint8_t *prev,
                     struct fb_var_screeninfo *vinfo,
                     struct fb_fix_screeninfo *finfo, size_t screen_size,
                     int first_frame)
{
	int bytes_per_pixel = vinfo->bits_per_pixel / 8;
	size_t row_bytes = (size_t)vinfo->xres * (size_t)bytes_per_pixel;

	if(bytes_per_pixel <= 0)
		return;

	for(int y = 0; y < (int)vinfo->yres; y++){
		size_t row = ((size_t)y + vinfo->yoffset) * finfo->line_length +
		             (size_t)vinfo->xoffset * (size_t)bytes_per_pixel;

		if(row + row_bytes > screen_size)
			continue;

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
				size_t bytes = (size_t)(x - start) * (size_t)bytes_per_pixel;

				memcpy(fb + offset, frame + offset, bytes);
				memcpy(prev + offset, frame + offset, bytes);
			}
		}
	}
}

int
main(void)
{
	signal(SIGINT, cleanup);
	signal(SIGTERM, cleanup);

	tty_fd = open("/dev/tty0", O_RDWR | O_NONBLOCK);
	if(tty_fd < 0)
		tty_fd = open("/dev/tty", O_RDWR | O_NONBLOCK);

	if(tty_fd >= 0){
		ioctl(tty_fd, KDSETMODE, KD_GRAPHICS);
		tcgetattr(tty_fd, &orig_termios);
		struct termios raw = orig_termios;
		raw.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(tty_fd, TCSANOW, &raw);
	}

	mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);

	int fb_fd = open("/dev/fb0", O_RDWR);
	if(fb_fd < 0){
		cleanup(0);
		return 1;
	}

	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;

	if(ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == -1 ||
	   ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == -1){
		close(fb_fd);
		cleanup(0);
		return 1;
	}

	long screen_size = finfo.smem_len;
	uint8_t *fb = mmap(0, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);

	if(fb == MAP_FAILED){
		close(fb_fd);
		cleanup(0);
		return 1;
	}

	uint8_t *frame = malloc((size_t)screen_size);
	uint8_t *prev = malloc((size_t)screen_size);

	if(!frame || !prev){
		free(frame);
		free(prev);
		munmap(fb, screen_size);
		close(fb_fd);
		cleanup(0);
		return 1;
	}

	memset(frame, 0, (size_t)screen_size);
	memset(prev, 0, (size_t)screen_size);

	Lotus lotus;
	lotusinit(&lotus);
	lotus.speed = 1;
	lotus.breathtickmax = 2;
	lotus.screen = LotusScreenStart;

	const uint64_t target_ns = 16666666ULL;
	int mx = vinfo.xres / 2;
	int my = vinfo.yres / 2;
	char buf;
	char mouse_buf[3];
	int mouse_down = 0;
	int need_redraw = 1;
	int first_frame = 1;

	while(1){
		uint64_t frame_start = get_time_ns();

		int click = 0;

		if(read(tty_fd, &buf, 1) > 0){
			if(buf == 'q' || buf == 'Q')
				break;
			if(buf == ' ' || buf == '\n'){
				click = 1;
				need_redraw = 1;
			}
		}

		if(mouse_fd >= 0){
			while(read(mouse_fd, mouse_buf, 3) == 3){
				signed char dx = mouse_buf[1];
				signed char dy = -mouse_buf[2];
				int new_buttons = mouse_buf[0] & 0x07;

				mx += dx;
				my += dy;

				if(mx < 0) mx = 0;
				if(mx >= (int)vinfo.xres) mx = vinfo.xres - 1;
				if(my < 0) my = 0;
				if(my >= (int)vinfo.yres) my = vinfo.yres - 1;

				if(dx != 0 || dy != 0)
					need_redraw = 1;

				if((new_buttons & 0x01) && !(mouse_down & 0x01)){
					click = 1;
					need_redraw = 1;
				}

				mouse_down = new_buttons;
			}
		}

		if(need_redraw || lotus.screen == LotusScreenSession){
			render(frame, &vinfo, &finfo, &lotus, mx, my, &click);
			draw_cursor(frame, &vinfo, &finfo, mx, my);
			flush_changed_pixels(fb, frame, prev, &vinfo, &finfo,
			                     (size_t)screen_size, first_frame);
			first_frame = 0;
			need_redraw = 0;
		}

		uint64_t frame_time = get_time_ns() - frame_start;
		if(frame_time < target_ns)
			usleep((useconds_t)((target_ns - frame_time) / 1000));
	}

	free(frame);
	free(prev);
	munmap(fb, screen_size);
	close(fb_fd);
	cleanup(0);
	return 0;
}
