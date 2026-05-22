#include "lotus.h"

static void
cpcount(char dst[CountSize], const char src[CountSize])
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = 0;
}

static int
eqcount(const char a[CountSize], const char b[CountSize])
{
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

void
inccount(char v[CountSize])
{
	if(v[2] < '9'){
		v[2]++;
		return;
	}
	v[2] = '0';

	if(v[1] < '9'){
		v[1]++;
		return;
	}
	v[1] = '0';

	if(v[0] < '9'){
		v[0]++;
		return;
	}
	v[0] = '0';
}


void
lotusinit(Lotus *l)
{
	if(l == 0)
		return;

    l->screen = LotusScreenStart;
    l->phase = LotusPhaseBreathe;
    l->round = 0;

	l->r = 25;
	l->rmin = 25;
	l->rmax = 50;
	l->dir = 0;
	l->speed = 1;
	l->frame = 0;
	l->breathtick = 0;
	l->breathtickmax = 3;
	l->sectick = 0;
	l->halftick = 0;

	cpcount(l->count, "000");
	cpcount(l->maxbreaths, "030");
}


static void
breathe(Lotus *l)
{
	l->breathtick++;
	if(l->breathtick < l->breathtickmax)
		return;
	l->breathtick = 0;

	if(l->dir == 0){
		if(l->r < l->rmax)
			l->r += l->speed;
		else
			l->dir = 1;
	}else{
		if(l->r > l->rmin){
			l->r -= l->speed;
		}else{
			l->dir = 0;
			inccount(l->count);
		}
	}

	if(eqcount(l->count, l->maxbreaths)){
		cpcount(l->count, "000");
		l->phase = LotusPhaseHold;
	}
}

static void
hold(Lotus *l)
{
	l->sectick++;
	if(l->sectick < 60)
		return;
	l->sectick = 0;

	inccount(l->count);
}

void
lotusstep(Lotus *l)
{
	if(l == 0)
		return;

	switch(l->phase){
	case LotusPhaseBreathe:
		breathe(l);
		break;
	case LotusPhaseHold:
		hold(l);
		break;
	case LotusPhaseRecover:
		break;
	case LotusPhaseNext:
		break;
	}
}
