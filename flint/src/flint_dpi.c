#include "flint_dpi.h"

FlintDpiState flint_dpi_state;

void
flint_dpi_init(void)
{
    flint_dpi_state.view_width = FLINT_DPI_BASE_WIDTH;
    flint_dpi_state.view_height = FLINT_DPI_BASE_HEIGHT;
    flint_dpi_state.ui_scale = 1.0f;
    flint_dpi_state.ui_scale_clamped = 1.0f;
    flint_dpi_state.camera_zoom = 1.0f;
    flint_dpi_state.base_width = FLINT_DPI_BASE_WIDTH;
    flint_dpi_state.base_height = FLINT_DPI_BASE_HEIGHT;
    flint_dpi_state.needs_update = 0;
}

void
flint_dpi_invalidate(void)
{
    flint_dpi_state.view_width = -1;
    flint_dpi_state.view_height = -1;
    flint_dpi_state.needs_update = 1;
}

void
flint_dpi_update(int view_width, int view_height)
{
    int previous_width = flint_dpi_state.view_width;
    int previous_height = flint_dpi_state.view_height;

    if(previous_width != view_width || previous_height != view_height) {
        flint_dpi_state.view_width = view_width;
        flint_dpi_state.view_height = view_height;
        flint_dpi_state.ui_scale = (float)view_height / (float)flint_dpi_state.base_height;
        flint_dpi_state.ui_scale_clamped = (flint_dpi_state.ui_scale < 1.0f) ? 1.0f : flint_dpi_state.ui_scale;
        flint_dpi_state.needs_update = 1;
    } else {
        flint_dpi_state.needs_update = 0;
    }
}

int
flint_dpi_is_dirty(void)
{
    return flint_dpi_state.needs_update;
}
