#ifndef INBE_DPI_H
#define INBE_DPI_H

#include "raylib.h"

/* Base design dimensions */
#define INBE_DPI_BASE_WIDTH 320
#define INBE_DPI_BASE_HEIGHT 560

typedef struct DPICache {
	/* Viewport dimensions */
	int view_width;
	int view_height;

	/* UI Scaling */
	float ui_scale;
	float ui_scale_clamped;

	/* Camera/Rendering */
	float camera_zoom;

	/* Design constants */
	int base_width;
	int base_height;

	/* Dirty flag for viewport changes */
	int needs_update;
} DPICache;

/* Global DPI cache instance */
extern DPICache g_dpi;

/* Initialization and updates */
void dpi_init(void);
void dpi_update(int view_width, int view_height);
int dpi_is_dirty(void);

/* Accessor functions (inline for performance) */
static inline float dpi_ui_scale(void) { return g_dpi.ui_scale_clamped; }
static inline int dpi_view_width(void) { return g_dpi.view_width; }
static inline int dpi_view_height(void) { return g_dpi.view_height; }
static inline float dpi_camera_zoom(void) { return g_dpi.camera_zoom; }

#endif
