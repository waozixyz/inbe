#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <linux/fb.h>
#include "../liblotus/lotus.h"

void render(uint8_t *fb, struct fb_var_screeninfo *vinfo,
            struct fb_fix_screeninfo *finfo, Lotus *lotus,
            int mx, int my, int *click);

#endif
