#include "dpi.h"
#include <stdlib.h>

/* Global DPI cache instance */
DPICache g_dpi;

void
dpi_init(void)
{
	g_dpi.view_width = INBE_DPI_BASE_WIDTH;
	g_dpi.view_height = INBE_DPI_BASE_HEIGHT;
	g_dpi.ui_scale = 1.0f;
	g_dpi.ui_scale_clamped = 1.0f;
	g_dpi.camera_zoom = 1.0f;
	g_dpi.base_width = INBE_DPI_BASE_WIDTH;
	g_dpi.base_height = INBE_DPI_BASE_HEIGHT;
	g_dpi.needs_update = 0;
}

void
dpi_update(int view_width, int view_height)
{
	int previous_width = g_dpi.view_width;
	int previous_height = g_dpi.view_height;

	/* Only recalculate if viewport actually changed */
	if(previous_width != view_width || previous_height != view_height) {
		g_dpi.view_width = view_width;
		g_dpi.view_height = view_height;

		/* Calculate UI scale based on viewport height vs base design */
		g_dpi.ui_scale = (float)view_height / (float)g_dpi.base_height;

		/* Clamp to minimum 1.0 (no downscaling) */
		if(g_dpi.ui_scale < 1.0f)
			g_dpi.ui_scale_clamped = 1.0f;
		else
			g_dpi.ui_scale_clamped = g_dpi.ui_scale;

		g_dpi.needs_update = 1;
	} else {
		g_dpi.needs_update = 0;
	}
}

int
dpi_is_dirty(void)
{
	return g_dpi.needs_update;
}
