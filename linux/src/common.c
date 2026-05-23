#include <stdint.h>
#include <linux/fb.h>
#include "common.h"
#include "graphics.h"

void
render(uint8_t *fb, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo, Lotus *lotus, int mx, int my, int *click)
{
	int cx = vinfo->xres / 2;
	int cy = vinfo->yres / 2;

	clear_screen(fb, vinfo, finfo, COLOR_BG);

	if(lotus->screen < LotusScreenResults){
		int r = lotus->r * 6;
		draw_circle(fb, vinfo, finfo, cx, cy, r, COLOR_CIRCLE);

		if(lotus->screen == LotusScreenSession)
			lotusstep(lotus);

		if(lotus->screen == LotusScreenStart){
			draw_string(fb, vinfo, finfo, "INNER BREEZE", cx, 50, 4, COLOR_TEXT);

			if(draw_button(fb, vinfo, finfo, mx, my, *click, cx, cy + 200, "PLAY", 3, COLOR_PANEL, COLOR_HOT, COLOR_TEXT))
				lotus->screen = LotusScreenSession;
		}else{
			draw_string(fb, vinfo, finfo, lotus->count, cx, cy, 4, COLOR_INNER);

			if(lotus->phase == LotusPhaseHold){
				if(draw_button(fb, vinfo, finfo, mx, my, *click, cx, cy + 200, "BREATH", 3, COLOR_PANEL, COLOR_HOT, COLOR_TEXT)){
					cpcount(lotus->results[lotus->round], lotus->count);
					cpcount(lotus->count, "000");
					lotus->phase = LotusPhaseRecover;
				}
			}
		}
	}else{
		int i;
		char txt_r[4];

		draw_string(fb, vinfo, finfo, "RESULTS", cx, 80, 6, COLOR_TEXT);

		for(i = 0; i < MaxRounds; i++){
			txt_r[0] = 'R';
			txt_r[1] = (char)(i + '1');
			txt_r[2] = ':';
			txt_r[3] = 0;

			int y = 150 + i * 40;
			draw_string(fb, vinfo, finfo, txt_r, cx - 50, y, 4, COLOR_TEXT);
			draw_string(fb, vinfo, finfo, lotus->results[i], cx + 50, y, 4, COLOR_TEXT);
		}

		if(draw_button(fb, vinfo, finfo, mx, my, *click, cx, vinfo->yres - 100, "RESTART", 6, COLOR_PANEL, COLOR_HOT, COLOR_TEXT))
			lotusinit(lotus);
	}

	lotus->frame++;
}
