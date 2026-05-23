#include "raylib.h"
#include "../libinbe/inbe.h"

#if defined(PLATFORM_ANDROID)
#include <android/native_app_glue.h>
#else
#include <string.h>
#endif

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 450
#define SCALE 3

Inbe inbe;

static int
drawbtn(int x, int y, const char *label, int *hover)
{
	int mx = GetMouseX();
	int my = GetMouseY();
	int mb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
	int w = (int)MeasureText(label, 20) + 20;
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

	DrawText(label, x + 10, y + 5, 20, (Color){124, 63, 88, 255});

	return 0;
}

static void
drawinbe(int center_x, int center_y)
{
	DrawCircle(center_x, center_y, inbe.r * SCALE, (Color){249, 168, 117, 255});
	int text_w = MeasureText(inbe.count, 20);
	DrawText(inbe.count, center_x - text_w / 2, center_y - 10, 20, (Color){124, 63, 88, 255});
}

static void
initapp(void)
{
	inbeinit(&inbe);
}

static int
updateapp(void)
{
	int center_x = WINDOW_WIDTH / 2;
	int center_y = WINDOW_HEIGHT / 2;
	int hover = 0;

	if(inbe.screen == InbeScreenStart){
		int title_w = MeasureText("INNER BREEZE", 30);
		DrawText("INNER BREEZE", center_x - title_w / 2, 100, 30, (Color){124, 63, 88, 255});
		drawinbe(center_x, center_y);
		if(drawbtn(center_x, center_y + 100, "PLAY", &hover))
			inbe.screen = InbeScreenSession;
	}else if(inbe.screen == InbeScreenSession){
		inbestep(&inbe);

		if(inbe.phase == InbePhaseHold){
			if(drawbtn(center_x, center_y + 120, "BREATH", &hover)){
				cpcount(inbe.results[inbe.round], inbe.count);
				cpcount(inbe.count, "000");
				inbe.phase = InbePhaseRecover;
			}
		}

		drawinbe(center_x, center_y - 20);
	}else if(inbe.screen == InbeScreenResults){
		int title_w = MeasureText("RESULTS", 30);
		DrawText("RESULTS", center_x - title_w / 2, 50, 30, (Color){124, 63, 88, 255});

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

			DrawText(txt, center_x - 40, 100 + i * 40, 24, (Color){124, 63, 88, 255});
		}

		if(drawbtn(center_x, WINDOW_HEIGHT - 80, "RESTART", &hover))
			initapp();
	}

	inbe.frame++;

	return 0;
}

#if defined(PLATFORM_ANDROID)
void android_main(struct android_app *app) {
	(void)app;

	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "inbe - inbe");
	initapp();
	SetTargetFPS(60);

	while (!WindowShouldClose()) {
		BeginDrawing();
		ClearBackground((Color){255, 246, 211, 255});
		updateapp();
		EndDrawing();
	}

	CloseWindow();
}
#else
int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "inbe - inbe");
	initapp();
	SetTargetFPS(60);

	while (!WindowShouldClose()) {
		BeginDrawing();
		ClearBackground((Color){255, 246, 211, 255});
		updateapp();
		EndDrawing();
	}

	CloseWindow();

	return 0;
}
#endif
