#ifndef FLINT_UI_INTERNAL_H
#define FLINT_UI_INTERNAL_H

#include "flint_ui.h"
#include "flint_clip.h"
#include "flint_dpi.h"
#include "flint.h"
#include "flint_text_layout.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

extern Color c_text, c_bg, c_surface, c_circle, c_button, c_button_hover, c_icon;
extern Camera2D g_ui_camera;
extern Texture2D g_ui_gear_icon;
extern Texture2D g_ui_x_icon;
extern int g_ui_slider_active_id;
extern int g_ui_input_blocked;
extern int g_ui_pointer_dragging;
extern int g_ui_pointer_owner;

enum {
    UI_POINTER_OWNER_NONE = 0,
    UI_POINTER_OWNER_SCROLL,
    UI_POINTER_OWNER_HORIZONTAL_SLIDER,
    UI_POINTER_OWNER_VERTICAL_SLIDER
};

Vector2 ui_mouse_world(void);
void ui_mark_clickable(void);
void ui_mark_disabled(void);
int ui_pointer_drag_is_horizontal(void);
int ui_hover_effects_enabled(void);
int ui_base_input_captures_click(Vector2 point, int include_pointer_drag);
int ui_input_captures_click_internal(Vector2 point, int include_pointer_drag);
void ui_push_input_clip(Rectangle bounds);
void ui_pop_input_clip(void);
int ui_clampi(int value, int min_value, int max_value);

#endif
