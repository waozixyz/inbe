#include "app/app_frame_pacing.h"
#include "practices/practice_registry.h"

#include <stdio.h>
#include <string.h>

static Vector2 mouse_position;
static int text_input_active;
static int frame_pacing_active = -1;
static int update_count;

Vector2 GetMousePosition(void) { return mouse_position; }
bool IsMouseButtonPressed(int button) { (void)button; return false; }
bool IsMouseButtonDown(int button) { (void)button; return false; }
bool IsMouseButtonReleased(int button) { (void)button; return false; }
bool IsKeyPressed(int key) { (void)key; return false; }
int TextInputActive(void) { return text_input_active; }
void SetFramePacingActive(int active) { frame_pacing_active = active != 0; }
void UpdateFramePacing(void) { update_count++; }
void PushUIInspectSource(const char *file, int line) { (void)file; (void)line; }
void PopUIInspectSource(void) {}
const PracticeDefinition *practice_active(const InnerBreeze *app) { (void)app; return NULL; }

static int
expect_int(const char *label, int got, int want)
{
    if(got != want) {
        fprintf(stderr, "FAIL: %s got %d want %d\n", label, got, want);
        return 0;
    }
    return 1;
}

int
main(void)
{
    InnerBreeze app;
    int ok = 1;
    memset(&app, 0, sizeof(app));
    mouse_position = (Vector2){10, 20};

    app_update_frame_pacing(&app);
    ok &= expect_int("first frame is active", frame_pacing_active, 1);
    ok &= expect_int("first update count", update_count, 1);

    frame_pacing_active = -1;
    app_update_frame_pacing(&app);
    ok &= expect_int("idle frame becomes inactive", frame_pacing_active, 0);

    text_input_active = 1;
    frame_pacing_active = -1;
    app_update_frame_pacing(&app);
    ok &= expect_int("focused text input keeps active pacing", frame_pacing_active, 1);

    text_input_active = 0;
    frame_pacing_active = -1;
    app_update_frame_pacing(&app);
    ok &= expect_int("cleared text input returns to idle", frame_pacing_active, 0);

    if(ok)
        puts("frame pacing tests passed");
    return ok ? 0 : 1;
}
