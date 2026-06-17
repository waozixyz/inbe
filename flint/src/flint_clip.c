#include "flint_clip.h"

#define FLINT_CLIP_STACK_MAX 16

static Rectangle g_flint_clip_stack[FLINT_CLIP_STACK_MAX];
static int g_flint_clip_stack_count = 0;

Rectangle
flint_clip_intersection(Rectangle a, Rectangle b)
{
    float x1 = a.x > b.x ? a.x : b.x;
    float y1 = a.y > b.y ? a.y : b.y;
    float x2 = a.x + a.width < b.x + b.width ? a.x + a.width : b.x + b.width;
    float y2 = a.y + a.height < b.y + b.height ? a.y + a.height : b.y + b.height;

    if(x2 < x1)
        x2 = x1;
    if(y2 < y1)
        y2 = y1;

    return (Rectangle){x1, y1, x2 - x1, y2 - y1};
}

Rectangle
flint_clip_effective(Rectangle bounds)
{
    if(g_flint_clip_stack_count > 0)
        bounds = flint_clip_intersection(g_flint_clip_stack[g_flint_clip_stack_count - 1],
                                         bounds);
    return bounds;
}

void
flint_clip_begin(int x, int y, int w, int h)
{
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};

    if(w < 0)
        bounds.width = 0;
    if(h < 0)
        bounds.height = 0;

    bounds = flint_clip_effective(bounds);
    if(g_flint_clip_stack_count < FLINT_CLIP_STACK_MAX)
        g_flint_clip_stack[g_flint_clip_stack_count++] = bounds;

    BeginScissorMode((int)bounds.x, (int)bounds.y,
                     (int)bounds.width, (int)bounds.height);
}

void
flint_clip_end(void)
{
    EndScissorMode();
    if(g_flint_clip_stack_count > 0)
        g_flint_clip_stack_count--;
    if(g_flint_clip_stack_count > 0) {
        Rectangle bounds = g_flint_clip_stack[g_flint_clip_stack_count - 1];
        BeginScissorMode((int)bounds.x, (int)bounds.y,
                         (int)bounds.width, (int)bounds.height);
    }
}

void
flint_clip_reset(void)
{
    g_flint_clip_stack_count = 0;
}
