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
	l->speed = 2;
	l->frame = 0;

	cpcount(l->count, "000");
	cpcount(l->maxbreaths, "030");
}


void
lotusbreath(Lotus *l)
{
	if(l == 0)
		return;

	if(l->phase != LotusPhaseBreathe)
		return;

	l->breathtick++;
	if(l->breathtick < l->speed)
		return;
	l->breathtick = 0;

	if(l->dir == 0){
		if(l->r < l->rmax)
			l->r++;
		else
			l->dir = 1;
	}else{
		if(l->r > l->rmin){
			l->r--;
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
