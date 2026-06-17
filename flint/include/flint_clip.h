#ifndef FLINT_CLIP_H
#define FLINT_CLIP_H

#include "raylib.h"

Rectangle flint_clip_intersection(Rectangle a, Rectangle b);
Rectangle flint_clip_effective(Rectangle bounds);
void flint_clip_begin(int x, int y, int w, int h);
void flint_clip_end(void);
void flint_clip_reset(void);

#endif
