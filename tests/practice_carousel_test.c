#include "../build/kryon/generated/src/screens/practice_screen.c"

#include <assert.h>
#include <stdarg.h>
#include <string.h>

static int test_scale = 1;
static int clicked_id = -1;
static int button_count;
static int text_count;
static ButtonProps buttons[2];
static TextProps counter;
static char counter_text[32];

int Scale(int value)
{
    return value * test_scale;
}

int StyleClassId(const char *name)
{
    assert(strcmp(name, "practice-pager") == 0);
    return 1;
}

void PushInspectSource(const char *path, int line)
{
    (void)path;
    (void)line;
}

void PopInspectSource(void)
{
}

int Button(ButtonProps props)
{
    assert(button_count < 2);
    buttons[button_count++] = props;
    return !props.disabled && props.id == clicked_id;
}

void Text(TextProps props)
{
    counter = props;
    snprintf(counter_text, sizeof(counter_text), "%s", props.text);
    text_count++;
}

const char *TextFormat(const char *format, ...)
{
    static char text[64];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return text;
}

static int draw(Rectangle bounds, int count, int selected, int disabled, int click)
{
    button_count = 0;
    text_count = 0;
    clicked_id = click;
    return practice_carousel_controls(bounds, count, selected, disabled, 7090);
}

int main(void)
{
    const int widths[] = {280, 350, 480};

    for (test_scale = 1; test_scale <= 2; test_scale++) {
        for (size_t width = 0; width < sizeof(widths) / sizeof(widths[0]); width++) {
            Rectangle row = {Scale(20), Scale(440), Scale(widths[width]), Scale(48)};
            for (int selected = 0; selected < 4; selected++) {
                char expected[32];
                assert(draw(row, 4, selected, 0, -1) == -1);
                assert(button_count == 2 && text_count == 1);
                snprintf(expected, sizeof(expected), "%d / 4", selected + 1);
                assert(strcmp(counter_text, expected) == 0);
                assert(counter.align == TextAlignCenter);
                assert(counter.vertical_align == TextAlignCenter);
                assert(counter.bounds.x == row.x && counter.bounds.y == row.y);
                assert(counter.bounds.width == row.width && counter.bounds.height == row.height);

                for (int i = 0; i < 2; i++) {
                    Rectangle button = buttons[i].bounds;
                    assert(button.width == Scale(48) && button.height == Scale(48));
                    assert(button.y == row.y);
                    assert(button.x >= row.x && button.x + button.width <= row.x + row.width);
                    assert(buttons[i].emphasis == ButtonEmphasisGhost);
                    assert(buttons[i].arrow && buttons[i].class_name == 1);
                }
                assert(buttons[0].bounds.x + buttons[0].bounds.width < row.x + row.width / 2);
                assert(buttons[1].bounds.x > row.x + row.width / 2);
                assert(buttons[0].direction == ArrowLeft && buttons[1].direction == ArrowRight);
                assert(draw(row, 4, selected, 0, 7091) == (selected + 3) % 4);
                assert(draw(row, 4, selected, 0, 7092) == (selected + 1) % 4);
                assert(draw(row, 4, selected, 1, 7091) == -1);
                assert(buttons[0].disabled && buttons[1].disabled && text_count == 1);
                assert(draw(row, 4, selected, 1, 7092) == -1);
            }
            for (int count = 0; count <= 1; count++) {
                assert(draw(row, count, 0, 0, 7092) == -1);
                assert(button_count == 0 && text_count == 0);
            }
        }
    }
    assert(draw((Rectangle){0}, 4, 0, 0, 7092) == -1);
    assert(button_count == 0 && text_count == 0);
    puts("PASS practice pager: centered count, ghost buttons, touch targets, wraparound and disabled state");
    return 0;
}
