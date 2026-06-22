#include "ui.h"

FlintUIToolbarResult
ui_draw_toolbar(FlintUIToolbar toolbar)
{
    FlintUIToolbarResult result = {-1, -1};
    int side_padding = toolbar.side_padding < 0
                           ? 0
                           : (toolbar.side_padding > 0 ? toolbar.side_padding
                                                       : flint_px(12));
    int action_icon_size = toolbar.action_icon_size > 0
                               ? toolbar.action_icon_size
                               : flint_px(20);
    int action_icon_padding = toolbar.action_icon_padding > 0
                                  ? toolbar.action_icon_padding
                                  : flint_px(8);
    int action_gap = toolbar.action_gap > 0 ? toolbar.action_gap : flint_px(6);
    int action_w = action_icon_size + action_icon_padding * 2;
    int action_y = toolbar.y + (toolbar.height - action_w) / 2;
    int controls_x = toolbar.x + toolbar.width - side_padding;

    if(toolbar.draw_menu) {
        if(toolbar.options != NULL && toolbar.selected_index != NULL &&
           ui_draw_dropdown_menu(toolbar.id))
            result.selected_menu_item = toolbar.selected_index != NULL
                                            ? *toolbar.selected_index
                                            : -1;
        return result;
    }

    DrawRectangle(toolbar.x, toolbar.y, toolbar.width, toolbar.height,
                  flint_darken(c_bg, 14));
    DrawLine(toolbar.x, toolbar.y + toolbar.height - 1,
             toolbar.x + toolbar.width, toolbar.y + toolbar.height - 1,
             flint_darken(c_bg, 42));

    if(toolbar.actions != NULL && toolbar.action_count > 0) {
        for(int i = toolbar.action_count - 1; i >= 0; i--) {
            int hover = 0;
            int action_x;

            controls_x -= action_w;
            action_x = controls_x;
            if(!toolbar.actions[i].disabled &&
               ui_draw_icon_btn_padded(action_x, action_y, action_icon_size,
                                       action_icon_padding,
                                       toolbar.actions[i].icon, &hover))
                result.clicked_action = i;
            controls_x -= action_gap;
        }
    }

    if(toolbar.options != NULL && toolbar.option_count > 0 &&
       toolbar.selected_index != NULL) {
        int dropdown_h = toolbar.dropdown_height > 0
                             ? toolbar.dropdown_height
                             : flint_px(36);
        int dropdown_x = 0;
        int dropdown_y = toolbar.y;
        int dropdown_w = controls_x - dropdown_x;
        int dropdown_available_w;

        if(toolbar.action_count > 0)
            dropdown_w -= side_padding - action_gap;
        dropdown_available_w = dropdown_w;
        if(toolbar.dropdown_min_width > 0 && dropdown_w < toolbar.dropdown_min_width)
            dropdown_w = toolbar.dropdown_min_width;
        if(toolbar.dropdown_max_width > 0 && dropdown_w > toolbar.dropdown_max_width)
            dropdown_w = toolbar.dropdown_max_width;
        if(dropdown_available_w > 0 && dropdown_w > dropdown_available_w)
            dropdown_w = dropdown_available_w;
        if(dropdown_w < 0)
            dropdown_w = 0;
        ui_draw_dropdown_button(toolbar.id, dropdown_x, dropdown_y,
                                dropdown_w, dropdown_h,
                                toolbar.options, toolbar.option_count,
                                toolbar.selected_index);
    }

    return result;
}

FlintUIToolbarHeaderResult
ui_draw_toolbar_header(FlintUIToolbarHeader header)
{
    FlintUIToolbarHeaderResult result;
    FlintUIToolbar toolbar = header.toolbar;
    int height = toolbar.height > 0 ? toolbar.height : flint_px(58);
    int icon_size = header.leading_icon_size > 0 ? header.leading_icon_size : flint_px(20);
    int icon_padding = header.leading_icon_padding > 0 ? header.leading_icon_padding : flint_px(8);
    int leading_w = header.leading_width;
    int hover = 0;

    memset(&result, 0, sizeof(result));
    if(leading_w <= 0 && header.leading_icon.id != 0)
        leading_w = icon_size + icon_padding * 2 + flint_px(24);

    if(!toolbar.draw_menu) {
        DrawRectangle(0, 0, ui_view_width, height, flint_darken(c_bg, 14));
        DrawLine(0, height - 1, ui_view_width, height - 1,
                 flint_darken(c_bg, 42));
        if(header.leading_icon.id != 0) {
            result.leading_clicked = ui_draw_icon_btn_padded(flint_px(12), flint_px(12),
                                                             icon_size, icon_padding,
                                                             header.leading_icon,
                                                             &hover);
        }
        toolbar.x = leading_w;
        toolbar.y = 0;
        toolbar.width = ui_view_width - leading_w;
        toolbar.height = height;
        if(toolbar.width < 0)
            toolbar.width = 0;
    }

    result.toolbar = ui_draw_toolbar(toolbar);
    return result;
}
