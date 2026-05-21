#include "wasm4.h"


void* memset(void* dest, int byte, unsigned long n) {
    uint8_t* d = dest;
    while (n--) {
        *d++ = (unsigned char)byte;
    }
    return dest;
}

void start() {
    PALETTE[0] = 0xfff6d3;
    PALETTE[1] = 0xf9a875;
    PALETTE[2] = 0xeb6b6f;
    PALETTE[3] = 0x7c3f58;
}

void pixel (int x, int y) {
    int idx = (y*160 + x) >> 2;

    int shift = (x & 0b11) << 1;
    int mask = 0b11 << shift;

    int palette_color = *DRAW_COLORS & 0b1111;
    if (palette_color == 0) {
        return;
    }
    int color = (palette_color - 1) & 0b11;

    FRAMEBUFFER[idx] = (uint8_t)((color << shift) | (FRAMEBUFFER[idx] & ~mask));
}

const int WINDOW_WIDTH = 160;
const int WINDOW_HEIGHT = 160;

uint32_t radius = 25;
uint8_t dir = 0;

uint8_t minr = 25;
uint8_t maxr = 50;
uint8_t speed = 2;
int frame_count = 0;
char count[4] = "000";
char max_breaths[4] = "030";
uint8_t phase = 0;
uint8_t rounds = 0;
uint8_t max_rounds = 4;

void incrstr(char *str) {
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

int strcmp(const char *s1, const char *s2) {
    while (*s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(const uint8_t*)s1 - *(const uint8_t*)s2;
}
char* strcpy(char *dest, const char *src) {
    char *start = dest;
    while (*src != '\0') {
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';
    return start;
}

void update () {
    *DRAW_COLORS = 4;
    int16_t mx = *MOUSE_X;
    int16_t my = *MOUSE_Y;
    uint8_t mb = *MOUSE_BUTTONS;


    int32_t center_x = (int32_t) (WINDOW_WIDTH * 0.5);
    int32_t center_y = (int32_t) (WINDOW_HEIGHT* 0.5);

    switch (phase) {
        case 0:
            if (frame_count % speed == 0) {
                if (dir == 0) {
                    if (radius < maxr) {
                        radius++;
                    } else {
                        dir = 1;
                    }
                } else {
                    if (radius > minr) {
                        radius--;
                    } else {
                        dir = 0;
                        incrstr(count);
                    }
                }
            }

            if (strcmp(count, max_breaths) == 0) {
                strcpy(count, "000");
                phase++;
            }
            break;
        case 1:
            if (frame_count % 60 == 0)
                incrstr(count);

    
            uint32_t rect_w = 50;
            uint32_t rect_h = 10;
            int32_t rect_x = center_x - 25;
            int32_t rect_y = center_y + 40;
            if( mx > rect_x && mx < rect_x + (int16_t)rect_w && my > rect_y && my < rect_y + (int16_t)rect_h) {
                *DRAW_COLORS = 4;
                if (mb & MOUSE_LEFT) {
                    strcpy(count, "000");
                    phase++;
                }

            } else {
                *DRAW_COLORS = 3;
            }
            rect(rect_x, rect_y, rect_w, rect_h);
            *DRAW_COLORS = 2;
            text("BREATH", center_x - (6 * 4), rect_y + 1);
        
            break;
        case 2:
            if (radius < maxr && strcmp(count, "000") == 0) {
                radius++;
            } else {
                if (strcmp(count, "015") == 0 && radius > minr) {
                    radius--;
                    if (radius == minr)
                        phase++;
                }
                if (frame_count % 60 == 0)
                    incrstr(count);
            }
            break;
        case 3:
            if (rounds < max_rounds) {
                strcpy(count, "000");
                rounds++;
                phase = 0;
            }
            break;
    }
    *DRAW_COLORS = 3;
    int32_t current_x = center_x - (int32_t)radius;
    int32_t current_y = center_y - (int32_t)radius;
    oval(current_x, current_y, radius * 2, radius * 2);


            
    *DRAW_COLORS = 2;
    text(count, center_x - (3 * 4), center_y - 4);



    frame_count++;
}
