#include "ui.h"

int
ui_scrollbar_reserved_width(int max_scroll)
{
    return max_scroll > 0 ? flint_px(16) : 0;
}

int
ui_scrollbar_content_width(int content_width, int max_scroll)
{
    int reserved = ui_scrollbar_reserved_width(max_scroll);

    if(reserved <= 0)
        return content_width;
    if(content_width <= reserved)
        return 0;
    return content_width - reserved;
}

int
ui_scrollbar_safe_content_width(int content_x, int content_width,
                                int scrollbar_x, int max_scroll)
{
    int gap = flint_px(20);
    int safe_width = content_width;

    if(max_scroll <= 0 || scrollbar_x <= 0)
        return content_width;

    safe_width = scrollbar_x - content_x - gap;
    if(safe_width > content_width)
        return content_width;
    if(safe_width < 0)
        return 0;
    return safe_width;
}

FlintUIScrollView
ui_scroll_container_measure(FlintUIScrollArea area)
{
    FlintUIScrollView view;
    int x = (int)area.bounds.x;
    int y = (int)area.bounds.y;
    int w = (int)area.bounds.width;
    int h = (int)area.bounds.height;
    int scrollbar_w = flint_px(8);
    int scrollbar_x = area.scrollbar_x > 0
                          ? area.scrollbar_x
                          : x + w - scrollbar_w;
    int content_x = area.content_x > 0 ? area.content_x : x;
    int content_w = area.content_width > 0 ? area.content_width : w;

    memset(&view, 0, sizeof(view));
    view.content_x = content_x;
    view.viewport_h = h;
    view.content_h = area.content_height > 0 ? area.content_height : 0;
    view.max_scroll = view.content_h - h;
    if(view.max_scroll < 0)
        view.max_scroll = 0;

    if(area.scroll_offset != NULL) {
        int scroll_offset = ui_clampi(*area.scroll_offset, 0, view.max_scroll);
        view.content_y = y - scroll_offset;
    } else {
        view.content_y = y;
    }
    view.content_w = ui_scrollbar_safe_content_width(content_x, content_w,
                                                     scrollbar_x, view.max_scroll);

    return view;
}

FlintUIScrollPage
ui_scroll_page_begin(FlintUIScrollPageSpec spec)
{
    FlintUIScrollPage page;
    FlintUIScrollArea area;
    FlintUIScrollView measured;
    int max_content_w = spec.max_content_width;
    int min_content_w = spec.min_content_width;
    int side_padding = spec.side_padding > 0 ? spec.side_padding : flint_page_side_padding();
    int content_x = 0;
    int content_w = 0;
    int draw_w;
    int passes = spec.measure_passes > 0 ? spec.measure_passes : 3;

    memset(&page, 0, sizeof(page));

    if(max_content_w <= 0)
        max_content_w = ui_view_width;
    if(max_content_w > ui_view_width - side_padding * 2)
        max_content_w = ui_view_width - side_padding * 2;
    if(min_content_w > 0 && max_content_w < min_content_w)
        max_content_w = min_content_w;
    if(max_content_w < 0)
        max_content_w = 0;

    flint_centered_column(max_content_w, side_padding, &content_x, &content_w);
    draw_w = content_w;

    for(int i = 0; i < passes; i++) {
        int content_h;

        if(spec.content_height != NULL)
            content_h = spec.content_height(draw_w, spec.user_data);
        else
            content_h = 0;
        area = (FlintUIScrollArea){
            .bounds = {0.0f, (float)spec.y, (float)ui_view_width, (float)spec.height},
            .content_height = content_h,
            .content_x = content_x,
            .content_width = content_w,
            .scroll_offset = spec.scroll_offset,
            .wheel_step = spec.wheel_step > 0 ? spec.wheel_step : flint_px(42),
            .scrollbar_x = spec.scrollbar_x > 0 ? spec.scrollbar_x : ui_view_width - flint_px(8)
        };
        measured = ui_scroll_container_measure(area);
        if(measured.content_w == draw_w)
            break;
        draw_w = measured.content_w;
    }

    if(spec.content_height != NULL)
        area.content_height = spec.content_height(draw_w, spec.user_data);
    page.area = area;
    page.view = ui_scroll_container_begin(area);
    page.content_x = page.view.content_x;
    page.content_y = page.view.content_y;
    page.content_w = page.view.content_w;
    page.content_h = area.content_height;
    return page;
}

void
ui_scroll_page_end(FlintUIScrollPage page)
{
    ui_scroll_container_end(page.area, page.view);
}
FlintUIScrollView
ui_scroll_container_begin(FlintUIScrollArea area)
{
    static int content_drag_active = 0;
    static int content_dragging = 0;
    static int content_drag_start_y = 0;
    static int content_drag_start_scroll = 0;
    FlintUIScrollView view = ui_scroll_container_measure(area);
    Vector2 mouse_world = ui_mouse_world();
    int y = (int)area.bounds.y;
    int wheel_step = area.wheel_step > 0 ? area.wheel_step : flint_px(42);
    int inside = CheckCollisionPointRec(mouse_world, area.bounds);
    int captured = ui_input_captures_click(mouse_world);
    int drag_threshold = flint_px(5);
    int scrollbar_w = flint_px(8);
    int scrollbar_x = area.scrollbar_x > 0
                          ? area.scrollbar_x
                          : (int)(area.bounds.x + area.bounds.width) - scrollbar_w;
    Rectangle scrollbar_bounds = {
        (float)scrollbar_x,
        area.bounds.y,
        (float)scrollbar_w,
        area.bounds.height
    };
    int on_scrollbar = view.max_scroll > 0 &&
                       CheckCollisionPointRec(mouse_world, scrollbar_bounds);

    if(area.scroll_offset != NULL) {
        *area.scroll_offset = ui_clampi(*area.scroll_offset, 0, view.max_scroll);
        if(view.max_scroll > 0 && inside && !captured) {
            float wheel = GetMouseWheelMove();
            if(wheel != 0.0f) {
                *area.scroll_offset -= (int)(wheel * (float)wheel_step);
                *area.scroll_offset = ui_clampi(*area.scroll_offset, 0, view.max_scroll);
            }
        }

        if(g_ui_slider_active_id != 0 &&
           g_ui_pointer_owner == UI_POINTER_OWNER_NONE &&
           g_ui_pointer_dragging &&
           ui_pointer_drag_is_horizontal())
            g_ui_pointer_owner = UI_POINTER_OWNER_HORIZONTAL_SLIDER;

        if(g_ui_pointer_owner == UI_POINTER_OWNER_HORIZONTAL_SLIDER ||
           g_ui_pointer_owner == UI_POINTER_OWNER_VERTICAL_SLIDER) {
            content_drag_active = 0;
            content_dragging = 0;
        }

        if(view.max_scroll > 0 &&
           g_ui_pointer_owner == UI_POINTER_OWNER_NONE &&
           IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && inside && !captured &&
           !on_scrollbar) {
            content_drag_active = 1;
            content_dragging = 0;
            content_drag_start_y = (int)mouse_world.y;
            content_drag_start_scroll = *area.scroll_offset;
        }
        if(content_drag_active && IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
           g_ui_pointer_owner != UI_POINTER_OWNER_HORIZONTAL_SLIDER &&
           g_ui_pointer_owner != UI_POINTER_OWNER_VERTICAL_SLIDER) {
            int dy = (int)mouse_world.y - content_drag_start_y;
            if(content_dragging || dy > drag_threshold || dy < -drag_threshold) {
                g_ui_pointer_owner = UI_POINTER_OWNER_SCROLL;
                content_dragging = 1;
                *area.scroll_offset = content_drag_start_scroll - dy;
                *area.scroll_offset = ui_clampi(*area.scroll_offset, 0, view.max_scroll);
                ui_set_input_blocked(1);
            }
        } else if(content_drag_active) {
            if(content_dragging)
                ui_set_input_blocked(1);
            content_drag_active = 0;
            content_dragging = 0;
        }
        view.content_y = y - *area.scroll_offset;
    } else {
        view.content_y = y;
    }
    {
        Rectangle screen_bounds = {
            g_ui_camera.offset.x + area.bounds.x * g_ui_camera.zoom,
            g_ui_camera.offset.y + area.bounds.y * g_ui_camera.zoom,
            area.bounds.width * g_ui_camera.zoom,
            area.bounds.height * g_ui_camera.zoom
        };
        Rectangle clipped_screen_bounds = flint_clip_effective(screen_bounds);
        Rectangle clipped_world_bounds = {
            (clipped_screen_bounds.x - g_ui_camera.offset.x) / g_ui_camera.zoom,
            (clipped_screen_bounds.y - g_ui_camera.offset.y) / g_ui_camera.zoom,
            clipped_screen_bounds.width / g_ui_camera.zoom,
            clipped_screen_bounds.height / g_ui_camera.zoom
        };

        ui_push_input_clip(clipped_world_bounds);
        flint_clip_begin((int)screen_bounds.x, (int)screen_bounds.y,
                         (int)screen_bounds.width, (int)screen_bounds.height);
    }
    return view;
}

void
ui_scroll_container_end(FlintUIScrollArea area, FlintUIScrollView view)
{
    int scrollbar_w = flint_px(8);
    int scrollbar_x;

    flint_clip_end();
    ui_pop_input_clip();

    if(area.scroll_offset == NULL || view.max_scroll <= 0)
        return;

    scrollbar_x = area.scrollbar_x > 0
                      ? area.scrollbar_x
                      : (int)(area.bounds.x + area.bounds.width) - scrollbar_w;
    ui_draw_scrollbar(scrollbar_x,
                      (int)area.bounds.y,
                      (int)area.bounds.height,
                      view.content_h,
                      area.scroll_offset,
                      view.max_scroll);
}

/* ================================================================
 * SCROLLBAR
 * ================================================================ */

int
ui_draw_scrollbar(int x, int y, int viewport_h, int content_h, int *scroll_offset, int max_scroll)
{
    /* Don't show scrollbar if no scrolling needed */
    if(max_scroll <= 0)
        return 0;

    static int scrollbar_drag_active = 0;
    static int scrollbar_drag_start_y = 0;
    static int scrollbar_drag_start_scroll = 0;

    int scrollbar_width = flint_px(8);
    int scrollbar_min_thumb = flint_px(24);
    int track_padding = 0;

    /* Calculate thumb size and position */
    float content_ratio = (float)viewport_h / (float)content_h;
    int thumb_height = (int)(viewport_h * content_ratio);
    if(thumb_height < scrollbar_min_thumb)
        thumb_height = scrollbar_min_thumb;
    if(thumb_height > viewport_h)
        thumb_height = viewport_h;

    float scroll_ratio = max_scroll > 0 ? (float)*scroll_offset / (float)max_scroll : 0.0f;
    int thumb_y = y + (int)(scroll_ratio * (viewport_h - thumb_height));

    Vector2 mouse_pos = ui_mouse_world();
    int my = (int)mouse_pos.y;
    Rectangle thumb_bounds = {x + track_padding, thumb_y, scrollbar_width - track_padding * 2, thumb_height};
    int input_captured = ui_input_captures_click_internal(mouse_pos, 0);
    int thumb_active = CheckCollisionPointRec(mouse_pos, thumb_bounds) && !input_captured;
    int thumb_hover = thumb_active && ui_hover_effects_enabled();

    /* Handle drag state */
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       (!input_captured || scrollbar_drag_active)) {
        if(!scrollbar_drag_active) {
            /* Start drag if clicking on thumb */
            if(thumb_active && g_ui_pointer_owner == UI_POINTER_OWNER_NONE) {
                scrollbar_drag_active = 1;
                g_ui_pointer_owner = UI_POINTER_OWNER_SCROLL;
                scrollbar_drag_start_y = my;
                scrollbar_drag_start_scroll = *scroll_offset;
            }
        } else {
            /* Continue drag */
            int dy = my - scrollbar_drag_start_y;
            float scroll_per_pixel = (float)max_scroll / (float)(viewport_h - thumb_height);
            int new_scroll = scrollbar_drag_start_scroll + (int)(dy * scroll_per_pixel);
            *scroll_offset = new_scroll;
            if(*scroll_offset < 0) *scroll_offset = 0;
            if(*scroll_offset > max_scroll) *scroll_offset = max_scroll;
        }
    } else {
        if(scrollbar_drag_active && g_ui_pointer_owner == UI_POINTER_OWNER_SCROLL)
            g_ui_pointer_owner = UI_POINTER_OWNER_NONE;
        scrollbar_drag_active = 0;
    }

    DrawRectangle(x, y, scrollbar_width, viewport_h, flint_darken(c_bg, 20));

    Color thumb_color = thumb_hover || scrollbar_drag_active ? c_button_hover : c_button;
    DrawRectangleRec(thumb_bounds, thumb_color);

    return 1;
}

/* ================================================================
 * END OF UI FUNCTIONS
 * ================================================================ */
