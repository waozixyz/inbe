#include "lotus.h"

static int
eqcount(const char a[CountSize], const char b[CountSize])
{
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

static int
strcmp(const char *s1, const char *s2)
{
	while(*s1 != 0 && *s1 == *s2){
		s1++;
		s2++;
	}
	return *(const uint8_t*)s1 - *(const uint8_t*)s2;
}

void
cpcount(char dst[CountSize], const char src[CountSize])
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = 0;
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
	cpcount(l->results[0], "052");
    cpcount(l->results[1], "040");
    cpcount(l->results[2], "064");
    cpcount(l->results[3], "020");

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
next(Lotus *l)
{
    l->sectick++;
    if(l->sectick < 60)
        return;
    l->sectick = 0;

    if(l->round < MaxRounds - 1){
        cpcount(l->count, "000");
        l->round++;
        l->phase = LotusPhaseBreathe;
    }else{
        l->screen = LotusScreenResults;
    }
}

static void
recover(Lotus *l)
{
    if(l->r < l->rmax && strcmp(l->count, "000") == 0) {
        
        l->breathtick++;
        if(l->breathtick < l->breathtickmax)
            return;
        l->breathtick = 0;

        l->r += l->speed;
        
    } else {

        if(strcmp(l->count, "015") == 0 && l->r > l->rmin) {

            l->breathtick++;
            if(l->breathtick < l->breathtickmax)
                return;
            l->breathtick = 0;

            l->r -= l->speed;

            if(l->r == l->rmin)
                l->phase = LotusPhaseNext;
        } else {

            l->sectick++;
            if(l->sectick < 60)
                return;
            l->sectick = 0;
            
            inccount(l->count);
        }
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
		recover(l);
		break;
	case LotusPhaseNext:
		next(l);
		break;
	}
}
