#include "flint_scaling.h"
#include <math.h>

static float g_dpi_scale = 1.0f;

void
flint_set_dpi_scale(float scale)
{
    if (scale > 0.0f) {
        g_dpi_scale = scale;
    }
}

float
flint_get_dpi_scale(void)
{
    return g_dpi_scale;
}

int
flint_px(int px)
{
    return (int)(px * g_dpi_scale + 0.5f);
}

int
flint_clamp_px(int px, int min_px, int max_px)
{
    int value = (int)(px * g_dpi_scale + 0.5f);
    int min_value = (int)(min_px * g_dpi_scale + 0.5f);
    int max_value = (int)(max_px * g_dpi_scale + 0.5f);

    if(value < min_value)
        value = min_value;
    if(value > max_value)
        value = max_value;
    return value;
}
