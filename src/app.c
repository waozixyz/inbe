#define RINI_IMPLEMENTATION
#include "app.h"
#include "../../vendor/rini/src/rini.h"
#include <stdio.h>
#include <stdlib.h>

#define INBE_DEFAULT_TITLE "Inner Breeze"
#define INBE_DEFAULT_WIDTH 240
#define INBE_DEFAULT_HEIGHT 320
#define INBE_DEFAULT_BACKGROUND_COLOR ((Color){226, 238, 252, 255})
#define INBE_DEFAULT_TEXT_COLOR ((Color){36, 72, 124, 255})
#define INBE_DEFAULT_CIRCLE_COLOR ((Color){126, 183, 230, 255})
#define INBE_DEFAULT_BUTTON_COLOR ((Color){166, 207, 242, 255})
#define INBE_DEFAULT_BUTTON_HOVER_COLOR ((Color){104, 158, 215, 255})

typedef struct InbeConfig {
    char title[64];
    int width;
    int height;
    Color background;
    Color text;
    Color circle;
    Color button;
    Color button_hover;
    int loaded;
} InbeConfig;

static InbeConfig config = {
    .title = INBE_DEFAULT_TITLE,
    .width = INBE_DEFAULT_WIDTH,
    .height = INBE_DEFAULT_HEIGHT,
    .background = INBE_DEFAULT_BACKGROUND_COLOR,
    .text = INBE_DEFAULT_TEXT_COLOR,
    .circle = INBE_DEFAULT_CIRCLE_COLOR,
    .button = INBE_DEFAULT_BUTTON_COLOR,
    .button_hover = INBE_DEFAULT_BUTTON_HOVER_COLOR,
    .loaded = 0
};

static Color
config_color(rini_data ini, const char *prefix, Color fallback)
{
    char key[64];

    snprintf(key, sizeof(key), "%s_r", prefix);
    fallback.r = (unsigned char)rini_get_value_fallback(ini, key, fallback.r);
    snprintf(key, sizeof(key), "%s_g", prefix);
    fallback.g = (unsigned char)rini_get_value_fallback(ini, key, fallback.g);
    snprintf(key, sizeof(key), "%s_b", prefix);
    fallback.b = (unsigned char)rini_get_value_fallback(ini, key, fallback.b);
    snprintf(key, sizeof(key), "%s_a", prefix);
    fallback.a = (unsigned char)rini_get_value_fallback(ini, key, fallback.a);

    return fallback;
}

static void
load_config(void)
{
    if(config.loaded)
        return;

    const char *paths[] = {
        "inbe.ini",
        "apps/inbe.ini",
        "../inbe/inbe.ini",
        0
    };

    for(int i = 0; paths[i] != 0; i++) {
        rini_data ini = rini_load(paths[i]);
        if(ini.count == 0) {
            rini_unload(&ini);
            continue;
        }

        snprintf(config.title, sizeof(config.title), "%s",
                 rini_get_value_text_fallback(ini, "title", INBE_DEFAULT_TITLE));
        config.width = rini_get_value_fallback(ini, "width", INBE_DEFAULT_WIDTH);
        config.height = rini_get_value_fallback(ini, "height", INBE_DEFAULT_HEIGHT);
        config.background = config_color(ini, "background", config.background);
        config.text = config_color(ini, "text", config.text);
        config.circle = config_color(ini, "circle", config.circle);
        config.button = config_color(ini, "button", config.button);
        config.button_hover = config_color(ini, "button_hover", config.button_hover);
        rini_unload(&ini);
        break;
    }

    config.loaded = 1;
}

const char *
inbe_app_title(void)
{
    load_config();
    return config.title;
}

int
inbe_app_width(void)
{
    load_config();
    return config.width;
}

int
inbe_app_height(void)
{
    load_config();
    return config.height;
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
        DrawRectangle(x, y, w, h, config.button_hover);
        *hover = 1;
        app->cursor_clickable = 1;
        if(mb)
            return 1;
    } else {
        DrawRectangle(x, y, w, h, config.button);
        *hover = 0;
    }

    DrawText(label, x + 10, y + 5, font, config.text);

    return 0;
}

static void
drawinbe(InbeApp *app, int center_x, int center_y)
{
    DrawCircle(center_x, center_y, app->inbe.r, config.circle);
    int text_w = MeasureText(app->inbe.count, 20);
    DrawText(app->inbe.count, center_x - text_w / 2, center_y - 10, 20, config.text);
}

void
inbe_app_init(void *vapp) {
    InbeApp *app = vapp;
    if(app == 0)
        return;

    load_config();
    inbeinit(&app->inbe);
    app->inbe.rmax = config.width * 0.4f;
    app->inbe.rmin = config.width * 0.2f;
    app->inbe.r = app->inbe.rmin;
    app->inbe.speed = 3;
    app->camera = (Camera2D){0};
    app->cursor_clickable = 0;
}

static void
updateapp(InbeApp *app)
{
    int center_x = config.width / 2;
    int center_y = config.height / 2;
    int hover = 0;

    drawinbe(app, center_x, center_y);

    if(app->inbe.screen == InbeScreenStart) {
        int title_font = 30;
        int title_w = MeasureText("INNER BREEZE", title_font);
        DrawText("INNER BREEZE", center_x - title_w / 2, 80, title_font, config.text);
        if(drawbtn(app, center_x, center_y + (int)app->inbe.rmin + 20, "PLAY", &hover))
            app->inbe.screen = InbeScreenSession;
    } else if(app->inbe.screen == InbeScreenSession) {
        inbestep(&app->inbe);

        if(app->inbe.phase == InbePhaseHold) {
            if(drawbtn(app, center_x, config.height - 190, "BREATH", &hover)) {
                cpcount(app->inbe.results[app->inbe.round], app->inbe.count);
                cpcount(app->inbe.count, "000");
                app->inbe.phase = InbePhaseRecover;
            }
        }
    } else if(app->inbe.screen == InbeScreenResults) {
        int title_font = 30;
        int title_w = MeasureText("RESULTS", title_font);
        DrawText("RESULTS", center_x - title_w / 2, 50, title_font, config.text);

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

            DrawText(txt, center_x - 40, 120 + i * 40, 24, config.text);
        }

        if(drawbtn(app, center_x, config.height - 140, "RESTART", &hover))
            inbe_app_init(app);
    }

    app->inbe.frame++;
}

void
inbe_app_update_draw(void *vapp, Rectangle viewport) {
    InbeApp *app = vapp;
    if(app == 0 || viewport.width <= 0 || viewport.height <= 0)
        return;

    load_config();

    float scale_x = viewport.width / (float)config.width;
    float scale_y = viewport.height / (float)config.height;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    app->cursor_clickable = 0;
    app->camera.zoom = scale;
    app->camera.offset.x = viewport.x + (viewport.width - ((float)config.width * scale)) * 0.5f;
    app->camera.offset.y = viewport.y + (viewport.height - ((float)config.height * scale)) * 0.5f;

    BeginScissorMode((int)viewport.x, (int)viewport.y, (int)viewport.width, (int)viewport.height);
        DrawRectangleRec(viewport, config.background);
        BeginMode2D(app->camera);
            DrawRectangle(0, 0, config.width, config.height, config.background);
            updateapp(app);
        EndMode2D();
    EndScissorMode();
}

static void *
inbe_app_create(void)
{
    InbeApp *app = calloc(1, sizeof(InbeApp));
    inbe_app_init(app);
    return app;
}

static void
inbe_app_destroy(void *app)
{
    free(app);
}

const LotusAppApi *
inbe_app_api(void)
{
    static const LotusAppApi api = {
        .id = "inbe",
        .create = inbe_app_create,
        .init = inbe_app_init,
        .update_draw = inbe_app_update_draw,
        .destroy = inbe_app_destroy
    };

    return &api;
}
