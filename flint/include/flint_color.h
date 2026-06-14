#ifndef FLINT_COLOR_H
#define FLINT_COLOR_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

// Lighten a color by adding to each RGB component
Color flint_lighten(Color c, int amount);

// Darken a color by subtracting from each RGB component
Color flint_darken(Color c, int amount);

#ifdef __cplusplus
}
#endif

#endif // FLINT_COLOR_H
