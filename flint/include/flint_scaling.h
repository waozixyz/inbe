#ifndef FLINT_SCALING_H
#define FLINT_SCALING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Set the DPI scale factor (should be called once at startup)
void flint_set_dpi_scale(float scale);

// Get the current DPI scale factor
float flint_get_dpi_scale(void);

// Scale a pixel value by the DPI factor
int flint_px(int px);

// Scale and clamp a pixel value between min and max
int flint_clamp_px(int px, int min_px, int max_px);

#ifdef __cplusplus
}
#endif

#endif // FLINT_SCALING_H
