#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>

#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#include "lotus.h"
#include "draw.h"


typedef struct AppState AppState;

struct AppState {
	ANativeWindow *window;
	AInputQueue *input_queue;
	volatile bool running;
	pthread_t thread;

	Lotus lotus;

	int mx;
	int my;
	int down;
};

static AppState app;

typedef struct Theme Theme;

struct Theme {
	uint32_t bg;
	uint32_t panel;
	uint32_t hot;
	uint32_t text;
	uint32_t muted;
	uint32_t circle;

	int title_scale;
	int text_scale;
	int button_scale;
};

static Theme theme = {
	.bg = 0xffd3f6ff,
	.panel = 0xff75a8f9,
	.hot = 0xff6f6beb,
	.text = 0xff583f7c,
	.muted = 0xff583f7c,
	.circle = 0xff6f6beb,

	.title_scale = 6,
	.text_scale = 5,
	.button_scale = 6
};

static void
drain_input(void)
{
	AInputEvent *event;
	int type;
	int action;

	if(app.input_queue == 0)
		return;

	event = 0;
	while(AInputQueue_getEvent(app.input_queue, &event) >= 0){
		if(AInputQueue_preDispatchEvent(app.input_queue, event))
			continue;

		type = AInputEvent_getType(event);

		if(type == AINPUT_EVENT_TYPE_MOTION){
			action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;

			app.mx = (int)AMotionEvent_getX(event, 0);
			app.my = (int)AMotionEvent_getY(event, 0);

			if(action == AMOTION_EVENT_ACTION_DOWN)
				app.down = 1;
			else if(action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL)
				app.down = 0;
		}

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
				clearbuffer(&buffer, theme.bg);

				cx = buffer.width / 2;
				cy = buffer.height / 2;

				drawstring(&buffer, "INNER BREEZE", cx, 150, theme.title_scale, theme.text);

				r = app.lotus.r * 6;
				drawcircle(&buffer, cx, cy, r, theme.circle);
				drawstring(&buffer, app.lotus.count, cx, cy, theme.text_scale, theme.text);

				if(app.lotus.screen == LotusScreenStart){
					if(drawbtn(&buffer, app.mx, app.my, app.down, cx, cy + 240,
						"PLAY", theme.button_scale, theme.panel, theme.hot, theme.text))
						app.lotus.screen = LotusScreenSession;
				}else if(app.lotus.screen == LotusScreenSession){
					lotusstep(&app.lotus);

					if(app.lotus.phase == LotusPhaseHold){
						if(drawbtn(&buffer, app.mx, app.my, app.down, cx, cy + 240,
							"BREATH", theme.button_scale, theme.panel, theme.hot, theme.text)){
						}
					}
				}

				ANativeWindow_unlockAndPost(app.window);
			}
		}

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
    app.lotus.speed = 1;
	app.lotus.breathtickmax = 1;

	app.lotus.screen = LotusScreenStart;

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
