#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>

#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include "font8x8.h"
#include "lotus.h"

#define Rscale 6

typedef struct AppState AppState;

struct AppState {
	ANativeWindow *window;
	AInputQueue *input_queue;
	volatile bool running;
	pthread_t thread;

	Lotus lotus;
};

static AppState app;

static void
draw_char(ANativeWindow_Buffer *buffer, unsigned char c, int x, int y, int scale, uint32_t color)
{
	const uint8_t *glyph;
	uint32_t *pixels;
	uint32_t stride;
	uint8_t glyph_row;
	int row;
	int col;
	int sx;
	int sy;
	int px;
	int py;

	glyph = font_8x8[c];
	pixels = (uint32_t*)buffer->bits;
	stride = buffer->stride;

	for(row = 0; row < 8; row++){
		glyph_row = glyph[row];

		for(col = 0; col < 8; col++){
			if(glyph_row & (1 << (7 - col))){
				for(sy = 0; sy < scale; sy++){
					for(sx = 0; sx < scale; sx++){
						px = x + col * scale + sx;
						py = y + row * scale + sy;

						if(px >= 0 && px < buffer->width && py >= 0 && py < buffer->height)
							pixels[py * stride + px] = color;
					}
				}
			}
		}
	}
}

static void
draw_string(ANativeWindow_Buffer *buffer, const char *str, int x, int y, int scale, uint32_t color)
{
	int text_width;
	int text_height;
	int cursor_x;
	int i;

	text_width = strlen(str) * 8 * scale;
	text_height = 8 * scale;
	cursor_x = x - text_width / 2;
	y = y - text_height / 2;

	for(i = 0; str[i] != 0; i++){
		draw_char(buffer, (unsigned char)str[i], cursor_x, y, scale, color);
		cursor_x += 8 * scale;
	}
}

static void
draw_circle(ANativeWindow_Buffer *buffer, int cx, int cy, int r)
{
	uint32_t *pixels;
	uint32_t stride;
	int r2;
	int x;
	int y;
	int dx;
	int dy;

	pixels = (uint32_t*)buffer->bits;
	stride = buffer->stride;
	r2 = r * r;

	for(y = cy - r; y <= cy + r; y++){
		for(x = cx - r; x <= cx + r; x++){
			if(x >= 0 && x < buffer->width && y >= 0 && y < buffer->height){
				dx = x - cx;
				dy = y - cy;

				if(dx * dx + dy * dy <= r2)
					pixels[y * stride + x] = 0xFFFF0000;
			}
		}
	}
}

static void
clear_buffer(ANativeWindow_Buffer *buffer)
{
	uint32_t *pixels;
	uint32_t stride;
	int x;
	int y;

	pixels = (uint32_t*)buffer->bits;
	stride = buffer->stride;

	for(y = 0; y < buffer->height; y++){
		for(x = 0; x < buffer->width; x++)
			pixels[y * stride + x] = 0xFF000000;
	}
}

static void
drain_input(void)
{
	AInputEvent *event;

	if(app.input_queue == 0)
		return;

	event = 0;
	while(AInputQueue_getEvent(app.input_queue, &event) >= 0){
		if(AInputQueue_preDispatchEvent(app.input_queue, event))
			continue;

		AInputQueue_finishEvent(app.input_queue, event, 1);
	}
}

static void*
render_loop(void *arg)
{
	ANativeWindow_Buffer buffer;
	int cx;
	int cy;
	int r;

	(void)arg;

	while(app.running){
		drain_input();

		if(app.window != 0){
			if(ANativeWindow_lock(app.window, &buffer, 0) == 0){
				clear_buffer(&buffer);

				cx = buffer.width / 2;
				cy = buffer.height / 2;

				draw_string(&buffer, "INNER BREEZE", cx, 150, 6, 0xFF00FF00);

				r = app.lotus.r * Rscale;
				draw_circle(&buffer, cx, cy, r);

				draw_string(&buffer, app.lotus.count, cx, cy, 6, 0xFF00FF00);

				ANativeWindow_unlockAndPost(app.window);
			}
		}

		lotusbreath(&app.lotus);
		app.lotus.frame++;

		usleep(16000);
	}

	return 0;
}

static void
onNativeWindowCreated(ANativeActivity *activity, ANativeWindow *window)
{
	(void)activity;

	ANativeWindow_setBuffersGeometry(window, 0, 0, 1);

	app.window = window;
	app.running = true;

	lotusinit(&app.lotus);


	app.lotus.screen = LotusScreenSession;

	pthread_create(&app.thread, 0, render_loop, 0);
}

static void
onNativeWindowDestroyed(ANativeActivity *activity, ANativeWindow *window)
{
	(void)activity;
	(void)window;

	app.running = false;
	pthread_join(app.thread, 0);
	app.window = 0;
}

static void
onInputQueueCreated(ANativeActivity *activity, AInputQueue *queue)
{
	(void)activity;

	app.input_queue = queue;
}

static void
onInputQueueDestroyed(ANativeActivity *activity, AInputQueue *queue)
{
	(void)activity;
	(void)queue;

	app.input_queue = 0;
}

__attribute__((visibility("default")))
void
ANativeActivity_onCreate(ANativeActivity *activity, void *savedState, size_t savedStateSize)
{
	(void)savedState;
	(void)savedStateSize;

	activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
	activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
	activity->callbacks->onInputQueueCreated = onInputQueueCreated;
	activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
}
