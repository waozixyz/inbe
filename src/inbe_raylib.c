#include "inbe_raylib.h"

static Color
inbe_background_color(void)
{
    return (Color){
        INBE_BACKGROUND_R,
        INBE_BACKGROUND_G,
        INBE_BACKGROUND_B,
        INBE_BACKGROUND_A
    };
}

static Color
inbe_text_color(void)
{
    return (Color){
        INBE_TEXT_R,
        INBE_TEXT_G,
        INBE_TEXT_B,
        INBE_TEXT_A
    };
}

static Color
inbe_circle_color(void)
{
    return (Color){
        INBE_CIRCLE_R,
        INBE_CIRCLE_G,
        INBE_CIRCLE_B,
        INBE_CIRCLE_A
    };
}

static Color
inbe_button_color(void)
{
    return (Color){
        INBE_BUTTON_R,
        INBE_BUTTON_G,
        INBE_BUTTON_B,
        INBE_BUTTON_A
    };
}

static Color
inbe_button_hover_color(void)
{
    return (Color){
        INBE_BUTTON_HOVER_R,
        INBE_BUTTON_HOVER_G,
        INBE_BUTTON_HOVER_B,
        INBE_BUTTON_HOVER_A
    };
}

static int
drawbtn(InbeApp *app, int x, int y, const char *label, int *hover)
{
    Vector2 mouse_world = GetScreenToWorld2D(GetMousePosition(), app->camera);
    int mx = (int)mouse_world.x;
    int my = (int)mouse_world.y;

    int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    int font = 20;
    int w = (int)MeasureText(label, font) + 20;
    int h = 30;

    x = x - w / 2;

    if(mx > x && mx < x + w && my > y && my < y + h) {
        DrawRectangle(x, y, w, h, inbe_button_hover_color());
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb)
            return 1;
    } else {
        DrawRectangle(x, y, w, h, inbe_button_color());
        *hover = 0;
    }

    DrawText(label, x + 10, y + 5, font, inbe_text_color());

    return 0;
}

static void
drawinbe(InbeApp *app, int center_x, int center_y)
{
    DrawCircle(center_x, center_y, app->inbe.r, inbe_circle_color());
    int text_w = MeasureText(app->inbe.count, 20);
    DrawText(app->inbe.count, center_x - text_w / 2, center_y - 10, 20, inbe_text_color());
}

void
inbe_raylib_init(void *vapp) {
    InbeApp *app = vapp;
    if(app == 0)
        return;

    inbeinit(&app->inbe);
    app->inbe.rmax = INBE_WIDTH * 0.4f;
    app->inbe.rmin = INBE_WIDTH * 0.2f;
    app->inbe.r = app->inbe.rmin;
    app->inbe.speed = 3;
    app->camera = (Camera2D){0};
    app->cursor_clickable = 0;
}

static void
updateapp(InbeApp *app)
{
    int center_x = INBE_WIDTH / 2;
    int center_y = INBE_HEIGHT / 2;
    int hover = 0;

    drawinbe(app, center_x, center_y);

    if(app->inbe.screen == InbeScreenStart) {
        int title_font = 30;
        int title_w = MeasureText("INNER BREEZE", title_font);
        DrawText("INNER BREEZE", center_x - title_w / 2, 80, title_font, inbe_text_color());
        if(drawbtn(app, center_x, center_y + (int)app->inbe.rmin + 20, "PLAY", &hover))
            app->inbe.screen = InbeScreenSession;
    } else if(app->inbe.screen == InbeScreenSession) {
        inbestep(&app->inbe);

        if(app->inbe.phase == InbePhaseHold) {
            if(drawbtn(app, center_x, 450, "BREATH", &hover)) {
                cpcount(app->inbe.results[app->inbe.round], app->inbe.count);
                cpcount(app->inbe.count, "000");
                app->inbe.phase = InbePhaseRecover;
            }
        }
    } else if(app->inbe.screen == InbeScreenResults) {
        int title_font = 30;
        int title_w = MeasureText("RESULTS", title_font);
        DrawText("RESULTS", center_x - title_w / 2, 50, title_font, inbe_text_color());

        for(int i = 0; i < MaxRounds; i++) {
            char txt[16];
            txt[0] = 'R';
            txt[1] = (char)('1' + i);
            txt[2] = ':';
            txt[3] = ' ';
            txt[4] = app->inbe.results[i][0];
            txt[5] = app->inbe.results[i][1];
            txt[6] = app->inbe.results[i][2];
            txt[7] = 0;

            DrawText(txt, center_x - 40, 120 + i * 40, 24, inbe_text_color());
        }

        if(drawbtn(app, center_x, 500, "RESTART", &hover))
            inbe_raylib_init(app);
    }

    app->inbe.frame++;
}

void
inbe_raylib_update_draw(void *vapp, Rectangle viewport) {
    InbeApp *app = vapp;
    if(app == 0 || viewport.width <= 0 || viewport.height <= 0)
        return;

    float scale_x = viewport.width / (float)INBE_WIDTH;
    float scale_y = viewport.height / (float)INBE_HEIGHT;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    app->cursor_clickable = 0;
    app->camera.zoom = scale;
    app->camera.offset.x = viewport.x + (viewport.width - ((float)INBE_WIDTH * scale)) * 0.5f;
    app->camera.offset.y = viewport.y + (viewport.height - ((float)INBE_HEIGHT * scale)) * 0.5f;

    BeginScissorMode((int)viewport.x, (int)viewport.y, (int)viewport.width, (int)viewport.height);
        DrawRectangleRec(viewport, inbe_background_color());
        BeginMode2D(app->camera);
            DrawRectangle(0, 0, INBE_WIDTH, INBE_HEIGHT, inbe_background_color());
            updateapp(app);
        EndMode2D();
    EndScissorMode();
}
