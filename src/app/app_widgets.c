/* App-owned composed widgets.
 *
 * Kryon owns the standard control surface; these are Inner Breeze chrome
 * pieces built on that public surface: modal panel frames, the first-run
 * guide overlay, popup icon sliders, bottom icon rows, the profile picture
 * picker, and reorder affordances. Everything here uses the canonical
 * widgets (Button, Text, Surface, Slider, Icon) and the active style pack
 * for all colors and typography. */

#include "app.h"

#include <stdio.h>
#include <string.h>

static int app_clampi(int value, int low, int high)
{
    if(value < low)
        return low;
    if(value > high)
        return high;
    return value;
}

static int
app_icon_button(int icon_type, Rectangle bounds, int icon_size)
{
    (void)icon_size;
    return Button((ButtonProps){
        .bounds = bounds,
        .icon_type = icon_type,
        .icon_only = 1,
        .tone = ButtonToneNeutral,
        .emphasis = ButtonEmphasisSoft
    });
}

static void
app_panel_style(Style *style)
{
    memset(style, 0, sizeof(*style));
    style->fields = StyleBackground | StyleBorder | StyleRadius |
                    StyleBorderWidth;
    style->background = GetThemeSurface();
    style->border = Fade(GetThemeText(), 0.22f);
    style->radius = 8.0f;
    style->border_width = 1.0f;
}

UIPanelFrame
AppModalFrame(int width, int height, const char *title,
                 int left_icon_type, int right_icon_type)
{
    UIPanelFrame frame;
    Style panel_style;
    Rectangle capture;
    Vector2 mouse;
    int icon_size = Scale(20);
    int icon_padding = Scale(8);
    int icon_w = icon_size + icon_padding * 2;
    int title_font;

    memset(&frame, 0, sizeof(frame));
    if(width > view_width - Scale(24))
        width = view_width - Scale(24);
    if(height > view_height - Scale(24))
        height = view_height - Scale(24);

    frame.w = width;
    frame.h = height;
    frame.x = (view_width - width) / 2;
    frame.y = (view_height - height) / 2;
    frame.content_x = frame.x + Scale(18);
    frame.content_y = frame.y + Scale(58);
    frame.content_w = frame.w - Scale(36);
    frame.content_h = frame.h - Scale(74);

    capture = (Rectangle){(float)frame.x, (float)frame.y,
                          (float)frame.w, (float)frame.h};
    SetModalCapture(capture);
    mouse = GetMousePosition();
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       !ReleaseConsumed() &&
       !CheckCollisionPointRec(mouse, capture)) {
        ConsumeRelease();
        frame.right_clicked = 1;
    }

    DrawRectangle(0, 0, view_width, view_height, (Color){0, 0, 0, 180});

    app_panel_style(&panel_style);
    Surface(capture, panel_style);

    title_font = AppTitleFontSize(title, frame.w - icon_w * 2 - Scale(24));
    Text((TextProps){
        .bounds = {(float)(frame.x + (frame.w - AppTextWidth(title, title_font)) / 2),
                   (float)(frame.y + Scale(14)), 0, 0},
        .text = title,
        .wrap = TextWrapNone
    });

    if(left_icon_type > 0) {
        if(app_icon_button(left_icon_type,
                           (Rectangle){(float)(frame.x + Scale(6)),
                                       (float)(frame.y + Scale(6)),
                                       (float)icon_w, (float)icon_w},
                           icon_size))
            frame.left_clicked = 1;
    }
    if(frame.right_clicked == 0 && right_icon_type > 0) {
        if(app_icon_button(right_icon_type,
                           (Rectangle){(float)(frame.x + frame.w - icon_w - Scale(6)),
                                       (float)(frame.y + Scale(6)),
                                       (float)icon_w, (float)icon_w},
                           icon_size))
            frame.right_clicked = 1;
    }

    return frame;
}

static void
app_guide_draw_scrim(int view_w, int view_h, Rectangle anchor, Color scrim)
{
    int padding = Scale(4);
    int left = app_clampi((int)anchor.x - padding, 0, view_w);
    int top = app_clampi((int)anchor.y - padding, 0, view_h);
    int right = app_clampi((int)(anchor.x + anchor.width) + padding, 0, view_w);
    int bottom = app_clampi((int)(anchor.y + anchor.height) + padding, 0, view_h);

    DrawRectangle(0, 0, view_w, top, scrim);
    DrawRectangle(0, bottom, view_w, view_h - bottom, scrim);
    DrawRectangle(0, top, left, bottom - top, scrim);
    DrawRectangle(right, top, view_w - right, bottom - top, scrim);
}

static void
app_guide_draw_arrow(Rectangle tip, Rectangle anchor)
{
    int anchor_cx = (int)(anchor.x + anchor.width / 2);
    int anchor_cy = (int)(anchor.y + anchor.height / 2);
    int tip_left = (int)tip.x;
    int tip_right = (int)(tip.x + tip.width);
    int tip_top = (int)tip.y;
    int tip_bottom = (int)(tip.y + tip.height);
    int arrow_size = Scale(10);
    Vector2 start, end;
    Vector2 tip0, tip1, tip2;
    Color color = GetThemeText();

    if(anchor_cy < tip_top) {
        start.x = (float)anchor_cx;
        start.y = (float)(anchor_cy + anchor.height / 2);
        end.x = (float)anchor_cx;
        end.y = (float)tip_top;
        DrawLineEx(start, end, (float)Scale(2), color);
        tip0.x = end.x; tip0.y = end.y;
        tip1.x = end.x - arrow_size; tip1.y = end.y - arrow_size;
        tip2.x = end.x + arrow_size; tip2.y = end.y - arrow_size;
        DrawTriangle(tip0, tip1, tip2, color);
    } else if(anchor_cy > tip_bottom) {
        start.x = (float)anchor_cx;
        start.y = (float)(anchor_cy - anchor.height / 2);
        end.x = (float)anchor_cx;
        end.y = (float)tip_bottom;
        DrawLineEx(start, end, (float)Scale(2), color);
        tip0.x = end.x; tip0.y = end.y;
        tip1.x = end.x + arrow_size; tip1.y = end.y + arrow_size;
        tip2.x = end.x - arrow_size; tip2.y = end.y + arrow_size;
        DrawTriangle(tip0, tip1, tip2, color);
    } else if(anchor_cx < tip_left) {
        start.x = (float)(anchor_cx + anchor.width / 2);
        start.y = (float)anchor_cy;
        end.x = (float)tip_left;
        end.y = (float)anchor_cy;
        DrawLineEx(start, end, (float)Scale(2), color);
        tip0.x = end.x; tip0.y = end.y;
        tip1.x = end.x - arrow_size; tip1.y = end.y - arrow_size;
        tip2.x = end.x - arrow_size; tip2.y = end.y + arrow_size;
        DrawTriangle(tip0, tip1, tip2, color);
    } else {
        start.x = (float)(anchor_cx - anchor.width / 2);
        start.y = (float)anchor_cy;
        end.x = (float)tip_right;
        end.y = (float)anchor_cy;
        DrawLineEx(start, end, (float)Scale(2), color);
        tip0.x = end.x; tip0.y = end.y;
        tip1.x = end.x + arrow_size; tip1.y = end.y - arrow_size;
        tip2.x = end.x + arrow_size; tip2.y = end.y + arrow_size;
        DrawTriangle(tip0, tip1, tip2, color);
    }
}

static Rectangle
app_guide_tip_bounds(Rectangle anchor, int w, int h, int view_w, int view_h,
                     int reserved_top, int reserved_bottom)
{
    int margin = Scale(12);
    int gap = Scale(20);
    int bottom = view_h - reserved_bottom;
    int x = (int)(anchor.x + anchor.width / 2) - w / 2;
    int y;
    Rectangle rect;

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

    rect.x = (float)x;
    rect.y = (float)y;
    rect.width = (float)w;
    rect.height = (float)h;
    return rect;
}

UIGuideResult
AppGuideOverlay(GuideOverlayProps guide)
{
    UIGuideResult result;
    Style panel_style;
    ParagraphSpec paragraph;
    int view_w = guide.view_width > 0 ? guide.view_width : view_width;
    int view_h = guide.view_height > 0 ? guide.view_height : view_height;
    int step;
    int margin = Scale(12);
    int tip_w = view_w - margin * 2;
    int pad = Scale(12);
    int button_size = Scale(34);
    int close_size = Scale(28);
    int line_gap = guide.line_gap > 0 ? guide.line_gap : Scale(6);
    int text_gap = Scale(8);
    int controls_gap = Scale(12);
    int text_guard = Scale(8);
    int tip_chrome_h;
    int max_tip_h;
    int paragraph_h;
    int tip_h;
    Rectangle tip;
    int y;
    int text_clip_h;
    int controls_y;
    int finish;
    char page_text[32];

    memset(&result, 0, sizeof(result));
    if(guide.steps == NULL || guide.count <= 0 || guide.step == NULL)
        return result;

    step = app_clampi(*guide.step, 0, guide.count - 1);
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
    if(IsKeyPressed(KEY_BACK) || IsKeyPressed(KEY_ESCAPE)) {
        result.closed = 1;
        return result;
    }

    if(guide.max_width > 0 && tip_w > guide.max_width)
        tip_w = guide.max_width;
    else if(tip_w > Scale(300))
        tip_w = Scale(300);

    max_tip_h = view_h - guide.reserved_top - guide.reserved_bottom -
                margin * 2;
    if(max_tip_h < Scale(112))
        max_tip_h = view_h - margin * 2;
    tip_chrome_h = pad + close_size + text_gap + text_guard + controls_gap +
                   button_size + pad;

    memset(&paragraph, 0, sizeof(paragraph));
    paragraph.text = guide.steps[step].text;
    paragraph.width = tip_w - pad * 2;
    paragraph.font = guide.paragraph_font > 0 ? guide.paragraph_font : AppFontSize();
    paragraph.line_gap = line_gap;
    paragraph_h = AppParagraphHeight(paragraph.text, paragraph.width,
                                     paragraph.font, paragraph.line_gap);
    while(paragraph.font > AppSmallFontSize() &&
          paragraph_h > max_tip_h - tip_chrome_h) {
        paragraph.font--;
        paragraph_h = AppParagraphHeight(paragraph.text, paragraph.width,
                                         paragraph.font, paragraph.line_gap);
    }
    tip_h = tip_chrome_h + paragraph_h + text_guard;
    if(tip_h > max_tip_h)
        tip_h = max_tip_h;

    tip = app_guide_tip_bounds(guide.steps[step].anchor, tip_w, tip_h,
                               view_w, view_h, guide.reserved_top,
                               guide.reserved_bottom);
    SetModalCapture(tip);

    app_guide_draw_scrim(view_w, view_h, guide.steps[step].anchor,
                         (Color){0, 0, 0, 86});
    DrawRectangleLinesEx(guide.steps[step].anchor, (float)Scale(2),
                         GetThemeText());
    app_guide_draw_arrow(tip, guide.steps[step].anchor);

    app_panel_style(&panel_style);
    Surface(tip, panel_style);

    if(app_icon_button(guide.close_icon_type,
                       (Rectangle){tip.x + tip.width - pad - close_size,
                                   tip.y + pad,
                                   (float)close_size, (float)close_size},
                       Scale(16))) {
        result.closed = 1;
        return result;
    }

    y = (int)tip.y + pad + close_size + text_gap;
    controls_y = (int)tip.y + (int)tip.height - pad - button_size;
    text_clip_h = controls_y - controls_gap - y;
    if(text_clip_h > 0) {
        if(text_clip_h < paragraph_h + text_guard)
            PushInputClip((Rectangle){(float)((int)tip.x + pad),
                                      (float)(y - text_guard / 2),
                                      (float)paragraph.width,
                                      (float)(text_clip_h + text_guard)});
        Paragraph(paragraph, (int)tip.x + pad, &y);
        if(text_clip_h < paragraph_h + text_guard)
            PopInputClip();
    }

    snprintf(page_text, sizeof(page_text), "%d/%d", step + 1, guide.count);
    Text((TextProps){
        .bounds = {(float)((int)tip.x + pad),
                   (float)(controls_y + (button_size - AppSmallFontSize()) / 2),
                   0, 0},
        .text = page_text,
        .wrap = TextWrapNone
    });

    finish = step >= guide.count - 1;
    if(step > 0) {
        if(app_icon_button(guide.back_icon_type,
                           (Rectangle){tip.x + tip.width - pad - button_size * 2 - Scale(8),
                                       (float)controls_y,
                                       (float)button_size, (float)button_size},
                           Scale(19))) {
            *guide.step = step - 1;
            result.changed = 1;
            result.step = *guide.step;
        }
    }
    if(app_icon_button(finish ? guide.done_icon_type : guide.next_icon_type,
                       (Rectangle){tip.x + tip.width - pad - button_size,
                                   (float)controls_y,
                                   (float)button_size, (float)button_size},
                       Scale(19))) {
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

int
AppIconSliderPopup(IconSliderPopupProps popup)
{
    Style panel_style;
    Rectangle panel_bounds;
    Vector2 mouse;
    int popup_w;
    int popup_h;
    int popup_x;
    int popup_y;
    int button_w;
    int icon_clicked;
    int was_open;

    if(popup.open == NULL || popup.value == NULL)
        return 0;

    was_open = *popup.open;
    button_w = popup.icon_size + popup.icon_padding * 2;
    icon_clicked = Button((ButtonProps){
        .bounds = {(float)popup.x, (float)popup.y,
                   (float)button_w, (float)button_w},
        .icon_type = popup.icon_type,
        .icon_only = 1,
        .tone = ButtonToneNeutral,
        .emphasis = ButtonEmphasisSoft
    });
    if(icon_clicked) {
        *popup.open = !was_open;
        if(was_open)
            return 0;
    }

    if(!*popup.open)
        return 0;

    popup_w = popup.popup_width > 0 ? popup.popup_width : button_w;
    if(popup_w < button_w)
        popup_w = button_w;
    popup_h = popup.popup_height > 0 ? popup.popup_height : Scale(200);
    popup_x = popup.x + button_w / 2 - popup_w / 2;
    popup_y = popup.y + button_w + Scale(4);
    mouse = GetMousePosition();

    if(!icon_clicked && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
       (mouse.x < popup_x || mouse.x > popup_x + popup_w ||
        mouse.y < popup_y || mouse.y > popup_y + popup_h)) {
        *popup.open = 0;
        return 0;
    }

    panel_bounds = (Rectangle){(float)popup_x, (float)popup_y,
                               (float)popup_w, (float)popup_h};
    SetModalCapture(panel_bounds);
    app_panel_style(&panel_style);
    Surface(panel_bounds, panel_style);

    return Slider((SliderProps){
        .bounds = {(float)(popup_x + popup_w / 2 - Scale(10)),
                   (float)(popup_y + Scale(14)),
                   (float)Scale(20), (float)(popup_h - Scale(28))},
        .id = popup.id,
        .kind = NumericInt,
        .int_values = popup.value,
        .value_count = 1,
        .min = popup.min,
        .max = popup.max,
        .vertical = 1
    });
}

IconRowResult
AppBottomIconRow(IconRowRequest row)
{
    IconRowResult result;
    int icon_size = Scale(24);
    int gap = Scale(12);
    int count = row.count;
    int button_w = icon_size + Scale(20);
    int row_w;
    int start_x;
    int y;
    int i;

    memset(&result, 0, sizeof(result));
    result.clicked_index = -1;
    if(row.items == NULL || count <= 0)
        return result;

    row_w = count * button_w + (count - 1) * gap;
    if(row_w > row.view_width) {
        gap = Scale(8);
        button_w = icon_size + Scale(12);
        row_w = count * button_w + (count - 1) * gap;
        if(row_w > row.view_width) {
            button_w = (row.view_width - gap * (count - 1)) / count;
            if(button_w < icon_size)
                button_w = icon_size;
            row_w = count * button_w + (count - 1) * gap;
        }
    }
    start_x = row.center_x - row_w / 2;
    y = row.view_height - button_w - Scale(8);

    for(i = 0; i < count; i++) {
        int x = start_x + i * (button_w + gap);

        if(Button((ButtonProps){
            .bounds = {(float)x, (float)y, (float)button_w, (float)button_w},
            .id = 83000 + i,
            .icon_type = row.items[i].icon_type,
            .icon_only = 1,
            .tone = ButtonToneNeutral,
            .emphasis = ButtonEmphasisSoft,
            .disabled = row.items[i].disabled
        }))
            result.clicked_index = i;
    }

    result.y = y;
    result.button_width = button_w;
    return result;
}

void
AppReorderHandle(int x, int y, int w, int h, int active)
{
    int dot = Scale(3);
    int dot_gap = Scale(4);
    int col_gap = Scale(8);
    int total_w = dot * 2 + col_gap;
    int total_h = dot * 3 + dot_gap * 2;
    int start_x = x + (w - total_w) / 2;
    int start_y = y + (h - total_h) / 2;
    Color color = active ? GetThemeText() : Fade(GetThemeText(), 0.6f);
    int row;
    int col;

    if(w <= 0 || h <= 0)
        return;
    MarkClickable();
    for(row = 0; row < 3; row++) {
        for(col = 0; col < 2; col++) {
            DrawRectangle(start_x + col * (dot + col_gap),
                          start_y + row * (dot + dot_gap),
                          dot, dot, color);
        }
    }
}

void
AppReorderPlaceholder(Rectangle bounds)
{
    int x = (int)bounds.x;
    int y = (int)bounds.y;
    int w = (int)bounds.width;
    int h = (int)bounds.height;
    Color color = GetThemeButtonHover();

    if(w <= 0 || h <= 0)
        return;
    if(h >= Scale(32)) {
        int inset = Scale(3);
        Rectangle slot = {(float)(x + inset), (float)(y + inset),
                          (float)(w - inset * 2), (float)(h - inset * 2)};

        if(slot.width <= 0 || slot.height <= 0)
            return;
        DrawRectangleRounded(slot, 0.12f, 10, Fade(color, 0.10f));
        DrawRectangleRoundedLinesEx(slot, 0.12f, 10, (float)Scale(2), color);
    } else {
        DrawRectangle(x, y + (h - Scale(2)) / 2, w, Scale(2), color);
    }
}

ProfileImagePickerResult
AppProfileImagePickerModal(ProfileImagePickerProps modal)
{
    ProfileImagePickerResult result;
    UIPanelFrame frame;
    Rectangle clip;
    IconType candidates[] = {
        ICON_PFP_BAMBUS, ICON_PFP_BIRD, ICON_PFP_BOWL, ICON_PFP_BUSH,
        ICON_PFP_BUTTERFLY, ICON_PFP_CACTUS, ICON_PFP_COFFEE,
        ICON_PFP_DRAGONFLY, ICON_PFP_FIREPLACE, ICON_PFP_FLOWER1,
        ICON_PFP_FLOWER2, ICON_PFP_FOX, ICON_PFP_HEART, ICON_PFP_INCENSE,
        ICON_PFP_LOTUS, ICON_PFP_MOUNTAIN, ICON_PFP_MUSHROOM, ICON_PFP_PALM,
        ICON_PFP_PERSON1, ICON_PFP_RAINBOW, ICON_PFP_TENT, ICON_PFP_TREE1,
        ICON_PFP_TREE2, ICON_PFP_TREE3, ICON_PFP_TREE4
    };
    int count = (int)(sizeof(candidates) / sizeof(candidates[0]));
    int default_scroll_offset = 0;
    int *scroll_offset = modal.scroll_offset != NULL ? modal.scroll_offset
                                                     : &default_scroll_offset;
    int width = modal.max_width > 0 ? modal.max_width : Scale(440);
    int gap = Scale(8);
    int content_w;
    int columns;
    int cell;
    int rows;
    int grid_w;
    int content_h;
    int height;
    int max_height;
    int icon_inset;
    int max_scroll;
    Vector2 mouse;
    int i;

    memset(&result, 0, sizeof(result));
    if(width > view_width - Scale(24))
        width = view_width - Scale(24);
    if(width < Scale(240))
        width = Scale(240);

    content_w = width - Scale(36);
    columns = (content_w + gap) / (Scale(64) + gap);
    if(columns < 3)
        columns = 3;
    if(columns > count)
        columns = count;
    if(columns < 1)
        columns = 1;
    cell = (content_w - (columns - 1) * gap) / columns;
    if(cell > Scale(64))
        cell = Scale(64);
    if(cell < Scale(52))
        cell = Scale(52);
    grid_w = columns * cell + (columns - 1) * gap;
    rows = columns > 0 ? (count + columns - 1) / columns : 0;
    content_h = rows > 0 ? rows * cell + (rows - 1) * gap : 0;
    height = Scale(74) + content_h + Scale(18);
    max_height = view_height - Scale(24);
    if(height > max_height)
        height = max_height;

    frame = AppModalFrame(width, height,
                             modal.title != NULL ? modal.title : "Profile image",
                             0, modal.close_icon_type);
    if(frame.right_clicked) {
        result.closed = 1;
        return result;
    }

    max_scroll = content_h - frame.content_h;
    if(max_scroll < 0)
        max_scroll = 0;
    *scroll_offset -= (int)(GetMouseWheelMove() * (cell + gap));
    if(*scroll_offset < 0)
        *scroll_offset = 0;
    if(*scroll_offset > max_scroll)
        *scroll_offset = max_scroll;

    clip = (Rectangle){(float)frame.content_x, (float)frame.content_y,
                       (float)frame.content_w, (float)frame.content_h};
    BeginScissorMode((int)clip.x, (int)clip.y, (int)clip.width,
                     (int)clip.height);
    mouse = GetMousePosition();
    icon_inset = Scale(6);
    for(i = 0; i < count; i++) {
        int row = i / columns;
        int col = i % columns;
        int x = frame.content_x + (frame.content_w - grid_w) / 2 +
                col * (cell + gap);
        int y = frame.content_y + row * (cell + gap) - *scroll_offset;
        IconType type = candidates[i];
        Rectangle bounds = {(float)x, (float)y, (float)cell, (float)cell};
        int hovered = CheckCollisionPointRec(mouse, bounds) &&
                      !InputCapturesClick(mouse);
        int active = type == (modal.selected_icon_type != NULL
                                  ? *modal.selected_icon_type
                                  : ICON_NONE);

        DrawRectangleRounded(bounds, 0.12f, 8,
                             hovered ? GetThemeSurfaceAlt()
                                     : GetThemeSurface());
        DrawRectangleRoundedLinesEx(bounds, 0.12f, 8, 1.0f,
                                    active ? GetThemeButtonHover()
                                           : GetThemeBorder());
        Icon(0, x + icon_inset, y + icon_inset, cell - icon_inset * 2, type,
             WHITE);
        if(hovered) {
            MarkClickable();
            if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                ConsumeRelease();
                if(modal.selected_icon_type != NULL)
                    *modal.selected_icon_type = type;
                result.changed = 1;
                result.selected_index = i;
                result.selected_icon_type = type;
                result.closed = 1;
            }
        }
    }
    EndScissorMode();

    return result;
}
