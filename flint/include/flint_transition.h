#ifndef FLINT_TRANSITION_H
#define FLINT_TRANSITION_H

#include "raylib.h"

typedef enum FlintTransitionPhase {
    FLINT_TRANSITION_NONE = 0,
    FLINT_TRANSITION_OUT = 1,
    FLINT_TRANSITION_IN = 2
} FlintTransitionPhase;

typedef struct FlintTransition {
    int active;
    int phase;
    int ticks;
    int duration;
} FlintTransition;

void flint_transition_reset(FlintTransition *transition);
void flint_transition_begin(FlintTransition *transition, int duration);
void flint_transition_reverse_to_out(FlintTransition *transition);
float flint_transition_alpha(const FlintTransition *transition);
int flint_transition_step(FlintTransition *transition);
void flint_transition_draw_fade(const FlintTransition *transition,
                                int width, int height, Color color);

#endif
