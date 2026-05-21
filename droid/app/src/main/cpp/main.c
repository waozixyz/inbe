#include <android/native_activity.h>
#include <android/native_window.h>
#include <stdint.h>

static const uint8_t font_HELLO_WORLD[11][8] = {
    {0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00}, // H
    {0x7E, 0x40, 0x40, 0x7C, 0x40, 0x40, 0x7E, 0x00}, // E
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x00}, // L
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x00}, // L
    {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00}, // O
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // [Space]
    {0x42, 0x42, 0x42, 0x42, 0x42, 0x5A, 0x24, 0x00}, // W
    {0x3C, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00}, // O
    {0x7C, 0x42, 0x42, 0x7C, 0x48, 0x44, 0x42, 0x00}, // R
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x00}, // L
    {0x78, 0x44, 0x42, 0x42, 0x42, 0x44, 0x78, 0x00}  // D
};

static void draw_text(ANativeWindow_Buffer* buffer) {
    uint32_t* pixels = (uint32_t*)buffer->bits;
    uint32_t stride = buffer->stride;

    for (int y = 0; y < buffer->height; ++y) {
        for (int x = 0; x < buffer->width; ++x) {
            pixels[y * stride + x] = 0xFF000000;
        }
    }

    int start_x = 80;
    int start_y = 150;
    int scale = 6;

    for (int c = 0; c < 11; ++c) {
        for (int row = 0; row < 8; ++row) {
            uint8_t glyph_row = font_HELLO_WORLD[c][row];
            for (int col = 0; col < 8; ++col) {
                if (glyph_row & (1 << (7 - col))) {
                    for (int sy = 0; sy < scale; ++sy) {
                        for (int sx = 0; sx < scale; ++sx) {
                            int px = start_x + (c * 8 * scale) + (col * scale) + sx;
                            int py = start_y + (row * scale) + sy;

                            if (px < buffer->width && py < buffer->height) {
                                pixels[py * stride + px] = 0xFF00FF00;
                            }
                        }
                    }
                }
            }
        }
    }
}

static void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window) {
    ANativeWindow_setBuffersGeometry(window, 0, 0, 1);

    ANativeWindow_Buffer buffer;

    if (ANativeWindow_lock(window, &buffer, 0) == 0) {
        draw_text(&buffer);
        ANativeWindow_unlockAndPost(window);
    }
}

__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t sazedStateSize) {
    activity->callbacks->onNativeWindowCreated = onNativeWindowCreated;
}
