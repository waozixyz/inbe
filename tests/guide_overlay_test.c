#include "ui.h"
#include "theme.h"
#include "ui_color.h"
#include "ui_dpi.h"
#include "ui_text.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

int ui_view_width = 320;
int ui_view_height = 560;

static int failures = 0;
int g_ui_input_blocked = 0;
static int icon_call_count = 0;
static int icon_click_call = 0;
static int icon_saw_blocked = 0;
static int pressed_key = 0;

void
ui_set_input_blocked(int blocked)
{
    g_ui_input_blocked = blocked != 0;
}

void
SetUIModalCapture(Rectangle bounds)
{
    (void)bounds;
}

int
DrawUIIconButton(UIIconButton button)
{
    (void)button;
    icon_call_count++;
    if(g_ui_input_blocked)
        icon_saw_blocked = 1;
    return icon_click_call == icon_call_count;
}

int
UIHoverEffectsEnabled(void)
{
    return !g_ui_input_blocked;
}

int
ScaleUIPx(int value)
{
    return value;
}

int
ui_clampi(int value, int min_value, int max_value)
{
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;
    return value;
}

int
GetUIFontSize(void)
{
    return UI_TEXT_16;
}

int
ui_paragraph_height(UIParagraphSpec paragraph)
{
    (void)paragraph;
    return 34;
}

void
DrawUIParagraph(UIParagraphSpec paragraph, int x, int *y)
{
    (void)paragraph;
    (void)x;
    if(y != NULL)
        *y += 34;
}

Color
GetThemeButton(void)
{
    return (Color){80, 90, 100, 255};
}

Color
GetThemeText(void)
{
    return (Color){240, 240, 240, 255};
}

Color
DarkenUIColor(Color color, int amount)
{
    (void)amount;
    return color;
}

void
DrawUIText(const char *text, int x, int y, int font_size, Color color)
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

#include "src/ui/guide.c"

static UIGuideOverlay
test_guide(int *step)
{
    static UIGuideStep steps[3] = {
        {{20, 20, 80, 30}, "First"},
        {{20, 70, 80, 30}, "Second"},
        {{20, 120, 80, 30}, "Third"}
    };

    return (UIGuideOverlay){
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
    g_ui_input_blocked = 0;
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
    UIGuideResult result;

    reset_input();
    result = DrawUIGuideOverlay(test_guide(&step));
    expect(!result.closed && !result.finished && !result.changed,
           "plain draw should not change guide state");
    expect(!g_ui_input_blocked, "guide must restore unblocked input after draw");
    expect(!icon_saw_blocked, "guide buttons should be evaluated while temporarily unblocked");
    expect(UIHoverEffectsEnabled(), "guide must restore hover effects after draw");

    g_ui_input_blocked = 1;
    result = DrawUIGuideOverlay(test_guide(&step));
    expect(!result.closed && !result.finished && !result.changed,
           "plain draw with preblocked input should not change guide state");
    expect(g_ui_input_blocked, "guide must restore preblocked input after draw");
}

static void
test_next_back_close_finish(void)
{
    int step = 0;
    UIGuideResult result;

    reset_input();
    icon_click_call = 2;
    result = DrawUIGuideOverlay(test_guide(&step));
    expect(result.changed && step == 1, "next button should advance step");
    expect(!g_ui_input_blocked, "next button should restore unblocked input");

    reset_input();
    icon_click_call = 2;
    result = DrawUIGuideOverlay(test_guide(&step));
    expect(result.changed && step == 0, "back button should return to previous step");
    expect(!g_ui_input_blocked, "back button should restore unblocked input");

    reset_input();
    icon_click_call = 1;
    result = DrawUIGuideOverlay(test_guide(&step));
    expect(result.closed, "close button should close guide");
    expect(!g_ui_input_blocked, "close button should restore unblocked input");

    step = 2;
    reset_input();
    icon_click_call = 3;
    result = DrawUIGuideOverlay(test_guide(&step));
    expect(result.finished, "check button should finish guide on last step");
    expect(!g_ui_input_blocked, "finish button should restore unblocked input");
}

static void
test_keyboard_navigation(void)
{
    int step = 0;
    UIGuideResult result;

    reset_input();
    pressed_key = KEY_RIGHT;
    result = DrawUIGuideOverlay(test_guide(&step));
    expect(result.changed && step == 1, "right key should advance guide");

    reset_input();
    pressed_key = KEY_LEFT;
    result = DrawUIGuideOverlay(test_guide(&step));
    expect(result.changed && step == 0, "left key should go back");

    reset_input();
    pressed_key = KEY_ESCAPE;
    result = DrawUIGuideOverlay(test_guide(&step));
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
