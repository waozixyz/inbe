#include "wasm4.h"
#include "inbe.h"

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
int_from_count(const char v[4])
{
	int a = (v[0] >= '0' && v[0] <= '9') ? v[0] - '0' : 0;
	int b = (v[1] >= '0' && v[1] <= '9') ? v[1] - '0' : 0;
	int c = (v[2] >= '0' && v[2] <= '9') ? v[2] - '0' : 0;
	return a * 100 + b * 10 + c;
}

void
int_from_str(int *out, const char *s)
{
	int v, sign;

	v = 0;
	sign = 1;

	if(*s == '-'){
		sign = -1;
		s++;
	}

	while(*s >= '0' && *s <= '9'){
		v = v * 10 + (*s - '0');
		s++;
	}

	*out = v * sign;
}

void
int_to_str(int v, char *out)
{
	char tmp[16];
	int i, len, sign;

	if(v == 0){
		out[0] = '0';
		out[1] = 0;
		return;
	}

	sign = 0;
	if(v < 0){
		sign = 1;
		v = -v;
	}

	i = 0;
	while(v > 0){
		tmp[i++] = '0' + (v % 10);
		v /= 10;
	}
	len = i;

	if(sign)
		out[0] = '-';

	for(i = 0; i < len; i++)
		out[sign + i] = tmp[len - 1 - i];
	out[sign + len] = 0;
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

	/* Faster speed for wasm4: 60 ticks per half-breath instead of 120 */
	inbe.breath_half_ticks = 60;
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
	char txt_tmp[16];
	int remaining;

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

		/* Only show "STARTING IN" after pressing PLAY (session screen, round 0) */
		if(inbe.screen == InbeScreenSession && inbe.phase == InbePhaseStarting && inbe.round == 0){
			remaining = inbe.pause_seconds - inbe.sectick / 60;
			if(remaining < 1)
				remaining = 1;

			/* "STARTING IN X..." - single line, higher up, left-aligned with padding */
			text("STARTING IN  ", 12, center_y - (int32_t)inbe.r - 14);
			int_to_str(remaining, txt_tmp);
			text(txt_tmp, 12 + 104, center_y - (int32_t)inbe.r - 14);
			text("...", 12 + 104 + (int32_t)strlen(txt_tmp) * 8, center_y - (int32_t)inbe.r - 14);
		}else if(inbe.phase == InbePhaseRecover){
			/* Show countdown from 15 during recover */
			if(inbe.r >= inbe.rmax){
				i = int_from_count(inbe.count);
				if(i < 15){
					int_to_str(15 - i, txt_tmp);
					text(txt_tmp, center_x - 4 * (int)strlen(txt_tmp) / 2, center_y - 4);
				}else{
					text(inbe.count, center_x - 4 * 3, center_y - 4);
				}
			}else{
				text("000", center_x - 4 * 3, center_y - 4);
			}
		}else if(inbe.phase == InbePhaseNext){
			text("000", center_x - 4 * 3, center_y - 4);
		}else{
			text(inbe.count, center_x - 4 * 3, center_y - 4);
		}
	}

	inbe.frame++;
}
