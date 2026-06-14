#ifndef FLINT_DPI_H
#define FLINT_DPI_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLINT_DPI_BASE_WIDTH 320
#define FLINT_DPI_BASE_HEIGHT 560

typedef struct FlintDpiState {
    int view_width;
    int view_height;
    float ui_scale;
    float ui_scale_clamped;
    float camera_zoom;
    int base_width;
    int base_height;
    int needs_update;
} FlintDpiState;

extern FlintDpiState flint_dpi_state;

void flint_dpi_init(void);
void flint_dpi_update(int view_width, int view_height);
int flint_dpi_is_dirty(void);
static inline float flint_dpi_scale(void) { return flint_dpi_state.ui_scale_clamped; }
static inline int flint_dpi_view_width(void) { return flint_dpi_state.view_width; }
static inline int flint_dpi_view_height(void) { return flint_dpi_state.view_height; }
static inline float flint_dpi_camera_zoom(void) { return flint_dpi_state.camera_zoom; }

#ifdef __cplusplus
}
#endif

#endif
