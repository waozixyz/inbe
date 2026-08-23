#include "kryon.h"

void *CreateApp(const char *project_path);
void DestroyApp(void *app);
void app_update_draw(void *app, Rectangle viewport);

void
main(void)
{
    void *app;
    Rectangle viewport;

    InitWindow(900, 720, "Inner Breeze");
    if(!IsWindowReady())
        exits("window");

    SetExitKey(0);
    InitUIDPI();
    SetTargetFPS(60);

    app = CreateApp("/sys/src/inbe");
    if(app == 0) {
        CloseWindow();
        exits("app");
    }

    while(!WindowShouldClose()) {
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)GetScreenWidth();
        viewport.height = (float)GetScreenHeight();

        BeginDrawing();
        ClearBackground(GetThemeBackground());
        app_update_draw(app, viewport);
        EndDrawing();
    }

    DestroyApp(app);
    CloseWindow();
    exits(nil);
}
