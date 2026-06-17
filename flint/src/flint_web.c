#include "flint_web.h"

#include "flint_dpi.h"
#include "flint_layout.h"
#include <raylib.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

static int g_web_orientation_mode = 0;

static void
flint_web_apply_orientation_size(int *width, int *height)
{
    int w;
    int h;

    if(width == 0 || height == 0)
        return;

    w = *width;
    h = *height;
    if(w <= 0 || h <= 0)
        return;

    if(g_web_orientation_mode == 1 && w > h) {
        int portrait_w = (h * 9) / 16;
        if(portrait_w < 1)
            portrait_w = h;
        *width = portrait_w;
    } else if(g_web_orientation_mode == 2 && h > w) {
        int landscape_h = (w * 9) / 16;
        if(landscape_h < 1)
            landscape_h = w;
        *height = landscape_h;
    }
}

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
    flint_web_apply_orientation_size(width, height);
}

void
flint_web_set_orientation_mode(int mode)
{
    if(mode < 0 || mode > 2)
        mode = 0;
    g_web_orientation_mode = mode;
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
    static int pending_width = 0;
    static int pending_height = 0;
    static int stable_frames = 0;
    int width;
    int height;
    int current_width;
    int current_height;
    int delta_w;
    int delta_h;
    int storage_syncing;

    storage_syncing = EM_ASM_INT({
        return Module.__inbeStorageSyncing || Module.__inbeStorageSyncPending ? 1 : 0;
    });
    if(storage_syncing)
        return 0;

    flint_web_viewport_size(GetScreenWidth(), GetScreenHeight(), &width, &height);
    current_width = GetScreenWidth();
    current_height = GetScreenHeight();
    delta_w = width - current_width;
    delta_h = height - current_height;
    if(delta_w < 0)
        delta_w = -delta_w;
    if(delta_h < 0)
        delta_h = -delta_h;

    if(width == current_width && height == current_height) {
        pending_width = 0;
        pending_height = 0;
        stable_frames = 0;
        return 0;
    }

    if(delta_w <= 1 && delta_h <= 1)
        return 0;

    if(width != pending_width || height != pending_height) {
        pending_width = width;
        pending_height = height;
        stable_frames = 1;
        return 0;
    }

    stable_frames++;
    if(stable_frames < 2 || IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        return 0;

#if defined(PLATFORM_WEB)
    EM_ASM({
        const frame = document.getElementById("canvas-frame");
        if(frame && (frame.style.width !== ($0 + "px") || frame.style.height !== ($1 + "px"))) {
            frame.style.width = $0 + "px";
            frame.style.height = $1 + "px";
        }
    }, width, height);
#endif
    SetWindowSize(width, height);
    flint_set_view_size(width, height);
    flint_dpi_update(width, height);
    pending_width = 0;
    pending_height = 0;
    stable_frames = 0;
    return 1;
#else
    return 0;
#endif
}
