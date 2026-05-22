#include "wasm4.h"
#include "lotus.h"

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

void pixel(int x, int y) {
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

#define WINDOW_WIDTH 160
#define WINDOW_HEIGHT 160
#define MAX_ROUNDS 4

uint32_t radius = 25;
uint8_t dir = 0;

uint8_t minr = 25;
uint8_t maxr = 50;
uint8_t speed = 2;
int frame_count = 0;
char count[4] = "000";
char max_breaths[4] = "030";
uint8_t phase = 0;
uint8_t round = 0;
uint8_t state = 0;


char results[MAX_ROUNDS][4] = {
    "052",
    "040",
    "064",
    "020"
};


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

int strlen(const char *s1) {
    int len = 0;
    while (s1[len] != '\0') {
        len++;
    }
    return len;
}
int drawbtn(int16_t mx, int16_t my, uint8_t mb, int32_t x, int32_t y, const char *label) {
    uint32_t w = (uint32_t)strlen(label) * 8;
    uint32_t h = 10;
    x = x - (int32_t)(w / 2);
    if( mx > x && mx < x + (int16_t)w && my > y && my < y + (int16_t)h) {
        *DRAW_COLORS = 4;
        if (mb & MOUSE_LEFT) {
            return 1;
        }
    } else {
        *DRAW_COLORS = 3;
    }

    rect(x, y, w, h);

    *DRAW_COLORS = 2;
    text(label, x, y + 1);
    return 0;
}
void update () {
    *DRAW_COLORS = 4;
    int16_t mx = *MOUSE_X;
    int16_t my = *MOUSE_Y;
    uint8_t mb = *MOUSE_BUTTONS;


    int32_t center_x = (int32_t) (WINDOW_WIDTH * 0.5);
    int32_t center_y = (int32_t) (WINDOW_HEIGHT* 0.5);

    if (state == 0) {
        if (drawbtn(mx, my, mb, center_x, center_y + 40, "PLAY") == 1) {
            state++;
        }
    }
    else if (state == 1) {
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
                            inccount(count);
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
                    inccount(count);

                if (drawbtn(mx, my, mb, center_x, center_y + 40, "BREATH") == 1) {
                    strcpy(results[round], count);
                    strcpy(count, "000");
                    phase++;
                }
                break;
            case 2:
                if (radius < maxr && strcmp(count, "000") == 0) {
                    if (frame_count % 30 == 0)
                        radius++;

                } else {
                    if (strcmp(count, "015") == 0 && radius > minr) {
                        if (frame_count % 30 == 0)
                            radius--;
                        
                        if (radius == minr)
                            phase++;
                    }
                    if (frame_count % 60 == 0)
                        inccount(count);
                }
                break;
            case 3:
                if (round < MAX_ROUNDS) {
                    strcpy(count, "000");
                    round++;
                    phase = 0;
                } else {
                    state++;
                }
                break;
        }
    } else {

    }
   
    if (state < 2) {
        *DRAW_COLORS = 3;
        int32_t current_x = center_x - (int32_t)radius;
        int32_t current_y = center_y - (int32_t)radius;
        oval(current_x, current_y, radius * 2, radius * 2);

        *DRAW_COLORS = 2;
        text(count, center_x - (3 * 4), center_y - 4);
    } else {
        text("RESULTS", center_x - 4 * 7, 10);
        for (int i = 0; i < MAX_ROUNDS; i++) {
            char txt_r[4];
            txt_r[0] = 'R';
            txt_r[1] = (char)(i + '1');
            txt_r[2] = ':';
            txt_r[3] = '\0';
            text(txt_r, center_x - 30, 40 + i * 20);
            text(&results[i][0], center_x - 4*3 + 20, 40 +i * 20); 
          
        }
        if (drawbtn(mx, my, mb, center_x, WINDOW_HEIGHT - 40, "RESTART") == 1) {
            state = 1;
            round = 0;        
        }
    }



    frame_count++;
}
