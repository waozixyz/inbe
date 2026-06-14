#include "flint_web.h"

#include "flint_dpi.h"
#include "flint_layout.h"
#include <raylib.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

int
flint_web_viewport_width(int fallback_width)
{
#if defined(PLATFORM_WEB)
    int width = EM_ASM_INT({
        const viewport = window.visualViewport;
        const width = viewport && viewport.width ? viewport.width :
            (window.innerWidth || document.documentElement.clientWidth || $0);
        return Math.max(1, Math.round(width));
    }, fallback_width);
    return width > 0 ? width : fallback_width;
#else
    return fallback_width;
#endif
}

int
flint_web_viewport_height(int fallback_height)
{
#if defined(PLATFORM_WEB)
    int height = EM_ASM_INT({
        const viewport = window.visualViewport;
        const height = viewport && viewport.height ? viewport.height :
            (window.innerHeight || document.documentElement.clientHeight || $0);
        return Math.max(1, Math.round(height));
    }, fallback_height);
    return height > 0 ? height : fallback_height;
#else
    return fallback_height;
#endif
}

void
flint_web_viewport_size(int fallback_width, int fallback_height,
                        int *width, int *height)
{
    if(width != 0)
        *width = flint_web_viewport_width(fallback_width);
    if(height != 0)
        *height = flint_web_viewport_height(fallback_height);
}

unsigned int
flint_web_window_flags(void)
{
#if defined(PLATFORM_WEB)
    return FLAG_WINDOW_RESIZABLE;
#else
    return 0;
#endif
}

int
flint_web_sync_window_size(void)
{
#if defined(PLATFORM_WEB)
    int width;
    int height;

    flint_web_viewport_size(GetScreenWidth(), GetScreenHeight(), &width, &height);
    if(width == GetScreenWidth() && height == GetScreenHeight())
        return 0;

    SetWindowSize(width, height);
    flint_set_view_size(width, height);
    flint_dpi_update(width, height);
    return 1;
#else
    return 0;
#endif
}
