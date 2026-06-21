#include "flint_transition.h"

#include <stddef.h>

void
flint_transition_reset(FlintTransition *transition)
{
    if(transition == NULL)
        return;
    transition->active = 0;
    transition->phase = FLINT_TRANSITION_NONE;
    transition->ticks = 0;
    transition->duration = 0;
}

void
flint_transition_begin(FlintTransition *transition, int duration)
{
    if(transition == NULL)
        return;
    if(duration <= 0)
        duration = 1;
    transition->active = 1;
    transition->phase = FLINT_TRANSITION_OUT;
    transition->ticks = 0;
    transition->duration = duration;
}

void
flint_transition_reverse_to_out(FlintTransition *transition)
{
    if(transition == NULL || !transition->active)
        return;
    if(transition->phase == FLINT_TRANSITION_IN) {
        transition->phase = FLINT_TRANSITION_OUT;
        transition->ticks = transition->duration - transition->ticks;
        if(transition->ticks < 0)
            transition->ticks = 0;
    }
}

float
flint_transition_alpha(const FlintTransition *transition)
{
    float progress;

    if(transition == NULL || !transition->active || transition->duration <= 0)
        return 0.0f;
    progress = (float)transition->ticks / (float)transition->duration;
    if(progress < 0.0f)
        progress = 0.0f;
    if(progress > 1.0f)
        progress = 1.0f;
    if(transition->phase == FLINT_TRANSITION_OUT)
        return progress;
    if(transition->phase == FLINT_TRANSITION_IN)
        return 1.0f - progress;
    return 0.0f;
}

int
flint_transition_step(FlintTransition *transition)
{
    if(transition == NULL || !transition->active)
        return FLINT_TRANSITION_NONE;

    transition->ticks++;
    if(transition->ticks < transition->duration)
        return FLINT_TRANSITION_NONE;

    if(transition->phase == FLINT_TRANSITION_OUT) {
        transition->phase = FLINT_TRANSITION_IN;
        transition->ticks = 0;
        return FLINT_TRANSITION_OUT;
    }

    flint_transition_reset(transition);
    return FLINT_TRANSITION_IN;
}

void
flint_transition_draw_fade(const FlintTransition *transition,
                           int width, int height, Color color)
{
    int alpha;

    if(transition == NULL || !transition->active)
        return;
    alpha = (int)(flint_transition_alpha(transition) * 255.0f);
    if(alpha <= 0)
        return;
    if(alpha > 255)
        alpha = 255;
    color.a = (unsigned char)(((int)color.a * alpha) / 255);
    if(color.a == 0)
        return;
    DrawRectangle(0, 0, width, height, color);
}
