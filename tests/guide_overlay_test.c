#include "flint_ui.h"
#include "flint_theme.h"
#include "flint_color.h"
#include "flint_dpi.h"
#include "flint_text.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

int ui_view_width = 320;
int ui_view_height = 560;

static int failures = 0;
static int input_blocked = 0;
static int icon_call_count = 0;
static int icon_click_call = 0;
static int icon_saw_blocked = 0;
static int pressed_key = 0;

void
ui_set_input_blocked(int blocked)
{
    input_blocked = blocked != 0;
}

int
flint_ui_icon_button(FlintUIIconButton button)
{
    (void)button;
    icon_call_count++;
    if(input_blocked)
        icon_saw_blocked = 1;
    return icon_click_call == icon_call_count;
}

int
ui_hover_effects_enabled(void)
{
    return !input_blocked;
}

int
flint_px(int value)
{
    return value;
}

int
flint_ui_font(void)
{
    return FLINT_TEXT_16;
}

int
flint_ui_paragraph_height(FlintUIParagraph paragraph)
{
    (void)paragraph;
    return 34;
}

void
flint_ui_paragraph_draw(FlintUIParagraph paragraph, int x, int *y)
{
    (void)paragraph;
    (void)x;
    if(y != NULL)
        *y += 34;
}

Color
flint_theme_get_button(void)
{
    return (Color){80, 90, 100, 255};
}

Color
flint_theme_get_text(void)
{
    return (Color){240, 240, 240, 255};
}

Color
flint_darken(Color color, int amount)
{
    (void)amount;
    return color;
}

void
flint_text_draw(const char *text, int x, int y, int font_size, Color color)
{
    (void)text;
    (void)x;
    (void)y;
    (void)font_size;
    (void)color;
}

bool
IsKeyPressed(int key)
{
    return pressed_key == key;
}

void DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    (void)posX; (void)posY; (void)width; (void)height; (void)color;
}

void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color)
{
    (void)rec; (void)lineThick; (void)color;
}

void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color)
{
    (void)rec; (void)roundness; (void)segments; (void)color;
}

void DrawRectangleRoundedLines(Rectangle rec, float roundness, int segments, Color color)
{
    (void)rec; (void)roundness; (void)segments; (void)color;
}

void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color)
{
    (void)v1; (void)v2; (void)v3; (void)color;
}

void DrawLineEx(Vector2 startPos, Vector2 endPos, float thick, Color color)
{
    (void)startPos; (void)endPos; (void)thick; (void)color;
}

#include "../vendor/flint/src/ui/guide.c"

static FlintUIGuideOverlay
test_guide(int *step)
{
    static FlintUIGuideStep steps[3] = {
        {{20, 20, 80, 30}, "First"},
        {{20, 70, 80, 30}, "Second"},
        {{20, 120, 80, 30}, "Third"}
    };

    return (FlintUIGuideOverlay){
        .steps = steps,
        .count = 3,
        .step = step,
        .view_width = 320,
        .view_height = 560,
        .reserved_top = 0,
        .reserved_bottom = 48,
        .max_width = 240
    };
}

static void
reset_input(void)
{
    input_blocked = 0;
    icon_call_count = 0;
    icon_click_call = 0;
    icon_saw_blocked = 0;
    pressed_key = 0;
}

static void
expect(int condition, const char *message)
{
    if(!condition) {
        fprintf(stderr, "FAIL %s\n", message);
        failures++;
    }
}

static void
test_blocks_passthrough(void)
{
    int step = 0;
    FlintUIGuideResult result;

    reset_input();
    result = flint_ui_draw_guide_overlay(test_guide(&step));
    expect(!result.closed && !result.finished && !result.changed,
           "plain draw should not change guide state");
    expect(input_blocked, "guide should leave screen input blocked");
    expect(!icon_saw_blocked, "guide buttons should be evaluated while temporarily unblocked");
    expect(!ui_hover_effects_enabled(), "guide should disable hover effects behind overlay");
}

static void
test_next_back_close_finish(void)
{
    int step = 0;
    FlintUIGuideResult result;

    reset_input();
    icon_click_call = 2;
    result = flint_ui_draw_guide_overlay(test_guide(&step));
    expect(result.changed && step == 1, "next button should advance step");
    expect(input_blocked, "next button should restore blocked input");

    reset_input();
    icon_click_call = 2;
    result = flint_ui_draw_guide_overlay(test_guide(&step));
    expect(result.changed && step == 0, "back button should return to previous step");
    expect(input_blocked, "back button should restore blocked input");

    reset_input();
    icon_click_call = 1;
    result = flint_ui_draw_guide_overlay(test_guide(&step));
    expect(result.closed, "close button should close guide");
    expect(input_blocked, "close button should restore blocked input");

    step = 2;
    reset_input();
    icon_click_call = 3;
    result = flint_ui_draw_guide_overlay(test_guide(&step));
    expect(result.finished, "check button should finish guide on last step");
    expect(input_blocked, "finish button should restore blocked input");
}

static void
test_keyboard_navigation(void)
{
    int step = 0;
    FlintUIGuideResult result;

    reset_input();
    pressed_key = KEY_RIGHT;
    result = flint_ui_draw_guide_overlay(test_guide(&step));
    expect(result.changed && step == 1, "right key should advance guide");

    reset_input();
    pressed_key = KEY_LEFT;
    result = flint_ui_draw_guide_overlay(test_guide(&step));
    expect(result.changed && step == 0, "left key should go back");

    reset_input();
    pressed_key = KEY_ESCAPE;
    result = flint_ui_draw_guide_overlay(test_guide(&step));
    expect(result.closed, "escape key should close guide");
}

int
main(void)
{
    test_blocks_passthrough();
    test_next_back_close_finish();
    test_keyboard_navigation();

    if(failures != 0) {
        fprintf(stderr, "%d guide overlay test failure(s)\n", failures);
        return 1;
    }
    printf("guide overlay tests passed\n");
    return 0;
}
