#include "wasm4.h"
#include "../../libinbe/inbe.h"

void*
memset(void *dest, int byte, unsigned long n)
{
	uint8_t *d;

	d = dest;
	while(n--)
		*d++ = (unsigned char)byte;

	return dest;
}

#define WINDOW_WIDTH 160
#define WINDOW_HEIGHT 160

Inbe inbe;

int
strlen(const char *s1)
{
	int len;

	len = 0;
	while(s1[len] != 0)
		len++;

	return len;
}

int
drawbtn(int16_t mx, int16_t my, uint8_t mb, int32_t x, int32_t y, const char *label)
{
	uint32_t w;
	uint32_t h;

	w = (uint32_t)strlen(label) * 8;
	h = 10;
	x = x - (int32_t)(w / 2);

	if(mx > x && mx < x + (int16_t)w && my > y && my < y + (int16_t)h){
		*DRAW_COLORS = 4;
		if(mb & MOUSE_LEFT)
			return 1;
	}else{
		*DRAW_COLORS = 3;
	}

	rect(x, y, w, h);

	*DRAW_COLORS = 2;
	text(label, x, y + 1);

	return 0;
}

void
start(void)
{
	PALETTE[0] = 0xfff6d3;
	PALETTE[1] = 0xf9a875;
	PALETTE[2] = 0xeb6b6f;
	PALETTE[3] = 0x7c3f58;

	inbeinit(&inbe);
}

void
update(void)
{
	int16_t mx;
	int16_t my;
	uint8_t mb;
	int32_t center_x;
	int32_t center_y;
	int32_t current_x;
	int32_t current_y;
	int i;
	char txt_r[4];

	*DRAW_COLORS = 4;

	mx = *MOUSE_X;
	my = *MOUSE_Y;
	mb = *MOUSE_BUTTONS;

	center_x = (int32_t)(WINDOW_WIDTH * 0.5);
	center_y = (int32_t)(WINDOW_HEIGHT * 0.5);


	if(inbe.screen == InbeScreenStart){
		if(drawbtn(mx, my, mb, center_x, center_y + 40, "PLAY") == 1)
			inbe.screen = InbeScreenSession;
	}else if(inbe.screen == InbeScreenSession){
        inbestep(&inbe);

		switch(inbe.phase){
		case InbePhaseHold:
			if(drawbtn(mx, my, mb, center_x, center_y + 40, "BREATH") == 1){
                cpcount(inbe.results[inbe.round], inbe.count);
                cpcount(inbe.count, "000");
                inbe.phase = InbePhaseRecover;
            }
			break;
        }

	}else if(inbe.screen == InbeScreenResults){
		text("RESULTS", center_x - 4 * 7, 10);

		for(i = 0; i < inbe.max_rounds; i++){
			txt_r[0] = 'R';
			txt_r[1] = (char)(i + '1');
			txt_r[2] = ':';
			txt_r[3] = 0;

			text(txt_r, center_x - 30, 40 + i * 20);
			text(inbe.results[i], center_x - 4 * 3 + 20, 40 + i * 20);
		}

		if(drawbtn(mx, my, mb, center_x, WINDOW_HEIGHT - 40, "RESTART") == 1)
			inbeinit(&inbe);
	}

	if(inbe.screen < InbeScreenResults){
		*DRAW_COLORS = 3;

		current_x = center_x - (int32_t)inbe.r;
		current_y = center_y - (int32_t)inbe.r;

		oval(current_x, current_y, (uint32_t)inbe.r * 2, (uint32_t)inbe.r * 2);

		*DRAW_COLORS = 2;
		text(inbe.count, center_x - 3, center_y - 4);
	}

	inbe.frame++;
}
