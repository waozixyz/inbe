#include "raylib.h"
#include "../libinbe/inbe.h"

#if defined(PLATFORM_ANDROID)
#include <android/native_app_glue.h>
#endif

// Define our strict internal virtual canvas dimensions
#define VIRTUAL_WIDTH  480
#define VIRTUAL_HEIGHT 640

Inbe inbe;
static Camera2D g_camera = {0};

static int
drawbtn(int x, int y, const char *label, int *hover)
{
    // Convert physical screen touch/mouse coordinates to our 480x640 virtual world
    Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), g_camera);
    int mx = (int)mouseWorld.x;
    int my = (int)mouseWorld.y;
    
    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int font = 20;
    int w = (int)MeasureText(label, font) + 20;
    int h = 30;

    x = x - w / 2;

    if(mx > x && mx < x + w && my > y && my < y + h){
        DrawRectangle(x, y, w, h, (Color){235, 107, 111, 255});
        *hover = 1;
        if(mb)
            return 1;
    }else{
        DrawRectangle(x, y, w, h, (Color){249, 168, 117, 255});
        *hover = 0;
    }

    DrawText(label, x + 10, y + 5, font, (Color){124, 63, 88, 255});

    return 0;
}

static void
drawinbe(int center_x, int center_y)
{
    DrawCircle(center_x, center_y, inbe.r, (Color){249, 168, 117, 255});
    int text_w = MeasureText(inbe.count, 20);
    DrawText(inbe.count, center_x - text_w / 2, center_y - 10, 20, (Color){124, 63, 88, 255});
}

static void
initapp(void)
{
    inbeinit(&inbe);
    inbe.rmax = VIRTUAL_WIDTH * 0.4f;
    inbe.rmin = VIRTUAL_WIDTH * 0.2f;
    inbe.r = inbe.rmin;
    inbe.speed = 3;
}

static int
updateapp(void)
{
    int center_x = VIRTUAL_WIDTH / 2;
    int center_y = VIRTUAL_HEIGHT / 2;
    int hover = 0;

    drawinbe(center_x, center_y);

    if(inbe.screen == InbeScreenStart){
        int title_font = 30;
        int title_w = MeasureText("INNER BREEZE", title_font);
        DrawText("INNER BREEZE", center_x - title_w / 2, 80, title_font, (Color){124, 63, 88, 255});
        if(drawbtn(center_x, center_y + (int)inbe.rmin + 20, "PLAY", &hover))
            inbe.screen = InbeScreenSession;
    }else if(inbe.screen == InbeScreenSession){
        inbestep(&inbe);

        if(inbe.phase == InbePhaseHold){
            if(drawbtn(center_x, 450, "BREATH", &hover)){
                cpcount(inbe.results[inbe.round], inbe.count);
                cpcount(inbe.count, "000");
                inbe.phase = InbePhaseRecover;
            }
        }
    }else if(inbe.screen == InbeScreenResults){
        int title_font = 30;
        int title_w = MeasureText("RESULTS", title_font);
        DrawText("RESULTS", center_x - title_w / 2, 50, title_font, (Color){124, 63, 88, 255});

        for(int i = 0; i < MaxRounds; i++){
            char txt[16];
            txt[0] = 'R';
            txt[1] = (char)('1' + i);
            txt[2] = ':';
            txt[3] = ' ';
            txt[4] = inbe.results[i][0];
            txt[5] = inbe.results[i][1];
            txt[6] = inbe.results[i][2];
            txt[7] = 0;

            DrawText(txt, center_x - 40, 120 + i * 40, 24, (Color){124, 63, 88, 255});
        }

        if(drawbtn(center_x, 500, "RESTART", &hover))
            initapp();
    }

    inbe.frame++;
    return 0;
}

// Universal update function that automatically scales the camera bounds to match any display size
static void update_viewport_scale(void)
{
    float w_physical = (float)GetScreenWidth();
    float h_physical = (float)GetScreenHeight();

    // Determine the minimum scaling factor required to fit our 480x640 canvas without cropping
    float scale_x = w_physical / (float)VIRTUAL_WIDTH;
    float scale_y = h_physical / (float)VIRTUAL_HEIGHT;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    // Apply the scaling factor directly to the camera zoom matrix
    g_camera.zoom = scale;

    // Position the camera offset so our virtual window centers perfectly inside excess black padding space
    g_camera.offset.x = (w_physical - ((float)VIRTUAL_WIDTH * scale)) * 0.5f;
    g_camera.offset.y = (h_physical - ((float)VIRTUAL_HEIGHT * scale)) * 0.5f;
}

// Combined entry point wrapper supporting modern Raylib cross-platform builds seamlessly
#if defined(PLATFORM_ANDROID)
void android_main(struct android_app *app) {
    (void)app;
    InitWindow(0, 0, "Inner Breeze");
#else
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    InitWindow(480, 640, "Inner Breeze");
#endif

    initapp();
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        update_viewport_scale();

        BeginDrawing();
        ClearBackground((Color){255, 246, 211, 255}); 

        // Enter camera space: anything drawn here will look like a native 480x640 canvas
        BeginMode2D(g_camera);
            DrawRectangle(0, 0, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, (Color){255, 246, 211, 255});
            updateapp();
        EndMode2D();

        EndDrawing();
    }

    CloseWindow();
#if !defined(PLATFORM_ANDROID)
    return 0;
#endif
}