#include "flint_ui.h"
#include "flint_color.h"
#include "flint_dpi.h"
#include "flint_text.h"
#include "flint_theme.h"
#include "raylib.h"
#include <stdio.h>

static int
guide_clampi(int value, int min, int max)
{
    if(value < min)
        return min;
    if(value > max)
        return max;
    return value;
}

static void
guide_draw_arrow(Rectangle tip, Rectangle anchor)
{
    int anchor_cx = (int)(anchor.x + anchor.width / 2);
    int anchor_cy = (int)(anchor.y + anchor.height / 2);
    int tip_left = (int)tip.x;
    int tip_right = (int)(tip.x + tip.width);
    int tip_top = (int)tip.y;
    int tip_bottom = (int)(tip.y + tip.height);
    int arrow_size = flint_px(10);
    Vector2 start, end;
    Color color = flint_theme_get_text();

    if(anchor_cy < tip_top) {
        start.x = (float)anchor_cx;
        start.y = (float)(anchor_cy + anchor.height / 2);
        end.x = (float)anchor_cx;
        end.y = (float)tip_top;
        DrawLineEx(start, end, (float)flint_px(2), color);
        DrawTriangle((Vector2){end.x, end.y},
                     (Vector2){end.x - arrow_size, end.y - arrow_size},
                     (Vector2){end.x + arrow_size, end.y - arrow_size},
                     color);
    } else if(anchor_cy > tip_bottom) {
        start.x = (float)anchor_cx;
        start.y = (float)(anchor_cy - anchor.height / 2);
        end.x = (float)anchor_cx;
        end.y = (float)tip_bottom;
        DrawLineEx(start, end, (float)flint_px(2), color);
        DrawTriangle((Vector2){end.x, end.y},
                     (Vector2){end.x + arrow_size, end.y + arrow_size},
                     (Vector2){end.x - arrow_size, end.y + arrow_size},
                     color);
    } else if(anchor_cx < tip_left) {
        start.x = (float)(anchor_cx + anchor.width / 2);
        start.y = (float)anchor_cy;
        end.x = (float)tip_left;
        end.y = (float)anchor_cy;
        DrawLineEx(start, end, (float)flint_px(2), color);
        DrawTriangle((Vector2){end.x, end.y},
                     (Vector2){end.x - arrow_size, end.y - arrow_size},
                     (Vector2){end.x - arrow_size, end.y + arrow_size},
                     color);
    } else {
        start.x = (float)(anchor_cx - anchor.width / 2);
        start.y = (float)anchor_cy;
        end.x = (float)tip_right;
        end.y = (float)anchor_cy;
        DrawLineEx(start, end, (float)flint_px(2), color);
        DrawTriangle((Vector2){end.x, end.y},
                     (Vector2){end.x + arrow_size, end.y - arrow_size},
                     (Vector2){end.x + arrow_size, end.y + arrow_size},
                     color);
    }
}

static Rectangle
guide_tip_bounds(Rectangle anchor, int w, int h, int view_w, int view_h,
                 int reserved_top, int reserved_bottom)
{
    int margin = flint_px(12);
    int gap = flint_px(20);
    int bottom = view_h - reserved_bottom;
    int x = (int)(anchor.x + anchor.width / 2) - w / 2;
    int y;

    if(bottom < reserved_top + margin)
        bottom = view_h - margin;
    if(x < margin)
        x = margin;
    if(x + w > view_w - margin)
        x = view_w - margin - w;
    if(x < margin)
        x = margin;

    if(anchor.y + anchor.height + gap + h < bottom)
        y = (int)(anchor.y + anchor.height + gap);
    else
        y = (int)(anchor.y - gap - h);

    if(y < reserved_top + margin)
        y = reserved_top + margin;
    if(y + h > bottom - margin)
        y = bottom - margin - h;
    if(y < margin)
        y = margin;

    return (Rectangle){(float)x, (float)y, (float)w, (float)h};
}

static int
guide_icon_button(FlintUIIconButton button)
{
    int clicked;

    ui_set_input_blocked(0);
    clicked = flint_ui_icon_button(button);
    ui_set_input_blocked(1);
    return clicked;
}

FlintUIGuideResult
flint_ui_draw_guide_overlay(FlintUIGuideOverlay guide)
{
    FlintUIGuideResult result = {0};
    int view_w = guide.view_width > 0 ? guide.view_width : ui_view_width;
    int view_h = guide.view_height > 0 ? guide.view_height : ui_view_height;
    int step;
    int margin = flint_px(12);
    int tip_w = view_w - margin * 2;
    int pad = flint_px(12);
    int button_size = flint_px(34);
    int close_size = flint_px(28);
    int page_font = FLINT_TEXT_12;
    int line_gap = guide.line_gap > 0 ? guide.line_gap : flint_px(6);
    char page_text[32];
    FlintUIParagraph paragraph;
    int paragraph_h;
    int tip_h;
    Rectangle tip;
    int y;
    int finish;

    if(guide.steps == NULL || guide.count <= 0 || guide.step == NULL)
        return result;

    step = guide_clampi(*guide.step, 0, guide.count - 1);
    *guide.step = step;
    result.step = step;

    if(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_ENTER)) {
        if(step >= guide.count - 1) {
            result.finished = 1;
        } else {
            *guide.step = step + 1;
            result.changed = 1;
            result.step = *guide.step;
        }
        return result;
    }
    if(IsKeyPressed(KEY_LEFT) && step > 0) {
        *guide.step = step - 1;
        result.changed = 1;
        result.step = *guide.step;
        return result;
    }
    if(IsKeyPressed(KEY_ESCAPE)) {
        result.closed = 1;
        return result;
    }

    ui_set_input_blocked(1);

    if(guide.max_width > 0 && tip_w > guide.max_width)
        tip_w = guide.max_width;
    else if(tip_w > flint_px(300))
        tip_w = flint_px(300);

    paragraph = (FlintUIParagraph){
        .text = guide.steps[step].text,
        .width = tip_w - pad * 2 - close_size - flint_px(8),
        .font = guide.paragraph_font,
        .line_gap = line_gap
    };
    paragraph_h = flint_ui_paragraph_height(paragraph);
    tip_h = pad + paragraph_h + flint_px(12) + button_size + pad;
    if(tip_h < flint_px(112))
        tip_h = flint_px(112);
    tip = guide_tip_bounds(guide.steps[step].anchor, tip_w, tip_h, view_w, view_h,
                           guide.reserved_top, guide.reserved_bottom);

    DrawRectangle(0, 0, view_w, view_h, (Color){0, 0, 0, 86});
    DrawRectangleLinesEx(guide.steps[step].anchor, (float)flint_px(2),
                         flint_theme_get_text());
    DrawRectangleRounded(tip, 0.08f, 8, flint_theme_get_button());
    DrawRectangleRoundedLines(tip, 0.08f, 8,
                              flint_darken(flint_theme_get_button(), 35));
    guide_draw_arrow(tip, guide.steps[step].anchor);

    if(guide_icon_button((FlintUIIconButton){
           .bounds = {
               tip.x + tip.width - pad - close_size,
               tip.y + pad,
               (float)close_size,
               (float)close_size
           },
           .icon = guide.close_icon,
           .icon_size = flint_px(16),
           .icon_padding = flint_px(6)
       })) {
        result.closed = 1;
        return result;
    }

    y = (int)tip.y + pad;
    flint_ui_paragraph_draw(paragraph, (int)tip.x + pad, &y);

    snprintf(page_text, sizeof(page_text), "%d/%d", step + 1, guide.count);
    flint_text_draw(page_text, (int)tip.x + pad,
                    (int)tip.y + (int)tip.height - pad - button_size +
                        (button_size - page_font) / 2,
                    page_font, flint_theme_get_text());

    finish = step >= guide.count - 1;
    if(step > 0) {
        if(guide_icon_button((FlintUIIconButton){
               .bounds = {
                   tip.x + tip.width - pad - button_size * 2 - flint_px(8),
                   tip.y + tip.height - pad - button_size,
                   (float)button_size,
                   (float)button_size
               },
               .icon = guide.back_icon,
               .icon_size = flint_px(19),
               .icon_padding = flint_px(7)
           })) {
            *guide.step = step - 1;
            result.changed = 1;
            result.step = *guide.step;
        }
    }
    if(guide_icon_button((FlintUIIconButton){
           .bounds = {
               tip.x + tip.width - pad - button_size,
               tip.y + tip.height - pad - button_size,
               (float)button_size,
               (float)button_size
           },
           .icon = finish ? guide.done_icon : guide.next_icon,
           .icon_size = flint_px(19),
           .icon_padding = flint_px(7)
       })) {
        if(finish) {
            result.finished = 1;
        } else {
            *guide.step = step + 1;
            result.changed = 1;
            result.step = *guide.step;
        }
    }

    return result;
}
