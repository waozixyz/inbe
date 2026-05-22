#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#include "font8x8.h"



#define MINR 150
#define MAXR 300

typedef struct {
    ANativeWindow* window;
    AInputQueue* input_queue;
    volatile bool running;
    pthread_t thread;

    char count[4];
    int radius;
    int dir;
    int speed;

} AppState;

static AppState app = {
    .count = {'0', '0', '0', '\0'},
    .radius = MINR,
    .speed = 5,
    .dir = 1
};

static void draw_char(ANativeWindow_Buffer* buffer, unsigned char c, int x, int y, int scale, uint32_t color) {
    const uint8_t* glyph = font_8x8[c]; 

    uint32_t* pixels = (uint32_t*)buffer->bits;
    uint32_t stride = buffer->stride;

    for (int row = 0; row < 8; ++row) {
        uint8_t glyph_row = glyph[row];
        for (int col = 0; col < 8; ++col) {
            if (glyph_row & (1 << (7 - col))) {
                for (int sy = 0; sy < scale; ++sy) {
                    for (int sx = 0; sx < scale; ++sx) {
                        int px = x + (col * scale) + sx;
                        int py = y + (row * scale) + sy;

                        if (px >= 0 && px < buffer->width && py >= 0 && py < buffer->height) {
                            pixels[py * stride + px] = color;
                        }
                    }
                }
            }
        }
    }
}

static void draw_string(ANativeWindow_Buffer* buffer, const char* str, int x, int y, int scale, uint32_t color) {
    int text_width = strlen(str) * 8 * scale;
    int text_height = 8 * scale;
    int cursor_x = x - (text_width / 2);
    y = y - (text_height / 2);

    for (int i = 0; str[i] != '\0'; ++i) {
        draw_char(buffer, (unsigned char)str[i], cursor_x, y, scale, color);
        cursor_x += (8 * scale);
    }
}

static void draw_circle(ANativeWindow_Buffer* buffer, int cx, int cy, int r) {
    uint32_t* pixels = (uint32_t*)buffer->bits;
    uint32_t stride = buffer->stride;

    int r_squared = r * r;

    for (int y = cy - r; y <= cy + r; ++y) {
        for (int x = cx - r; x <= cx + r; ++x) {
            if (x >= 0 && x < buffer->width && y >= 0 && y < buffer->height) {
                int dx = x - cx;
                int dy = y - cy;

                if ((dx * dx) + (dy * dy) <= r_squared) {
                    pixels[y * stride + x] = 0xFFFF0000;
                }
            }
        }
    }
}

static void clear_buffer(ANativeWindow_Buffer* buffer) {
    uint32_t* pixels = (uint32_t*)buffer->bits;
    uint32_t stride = buffer->stride;


    for (int y = 0; y < buffer->height; ++y) {
        for (int x = 0; x < buffer->width; ++x) {
            pixels[y * stride + x] = 0xFF000000;
        }
    }
}
static void incrstr(char *str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }

    for (int i = len - 1; i >= 0; i--) {
        if (str[i] < '9') {
            str[i]++;
            return;
        }
        str[i] = '0';
    }

    // fallback
    str[0] = '0';
    str[1] = '0';
    str[2] = '0';
    str[3] = '\0';
}

static void* render_loop(void* arg) {
    while (app.running) {
        if (app.input_queue != NULL) {
            AInputEvent* event = NULL;
            while (AInputQueue_getEvent(app.input_queue, &event) >= 0) {
                if (AInputQueue_preDispatchEvent(app.input_queue, event)) {
                    continue;
                }
                AInputQueue_finishEvent(app.input_queue, event, 1);
            }
        }

        if (app.window != NULL) {
            ANativeWindow_Buffer buffer;
            
            if (ANativeWindow_lock(app.window, &buffer, NULL) == 0) {
                clear_buffer(&buffer);

      
                int cx = buffer.width / 2;
                int cy = buffer.height / 2;

                draw_string(&buffer, "INNER BREEZE", cx, 150, 6, 0xFF00FF00);
                draw_circle(&buffer, cx, cy, app.radius);
                
                draw_string(&buffer, app.count, cx, cy, 6, 0xFF00FF00);

                ANativeWindow_unlockAndPost(app.window);
            }
        }

        app.radius += app.dir * app.speed;
        if (app.radius > MAXR) {
            app.dir = -1;
        }
        if (app.radius < MINR) {
            app.dir = 1;
            incrstr(app.count);

        }

        usleep(16000);
    }
}

static void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window) {
    ANativeWindow_setBuffersGeometry(window, 0, 0, 1); 

    app.window = window;
    app.running = true;
    
    pthread_create(&app.thread, NULL, render_loop, NULL);
}

static void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window) {
    app.running = false;
    pthread_join(app.thread, NULL);
    app.window = NULL;
}

static void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue) {
    app.input_queue = queue;
}

static void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue) {
    app.input_queue = NULL;
}

__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t sazedStateSize) {
    activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
    activity->callbacks->onNativeWindowDestroyed = onNativeWindowDestroyed;
    
    activity->callbacks->onInputQueueCreated = onInputQueueCreated;
    activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;
}
