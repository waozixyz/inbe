#include "inbe.h"

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
inbeinit(Inbe *l)
{
	if(l == 0)
		return;

    l->screen = InbeScreenStart;
    l->phase = InbePhaseBreathe;
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
breathe(Inbe *l)
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
		l->phase = InbePhaseHold;
	}
}

static void
next(Inbe *l)
{
    l->sectick++;
    if(l->sectick < 60)
        return;
    l->sectick = 0;

    if(l->round < MaxRounds - 1){
        cpcount(l->count, "000");
        l->round++;
        l->phase = InbePhaseBreathe;
    }else{
        l->screen = InbeScreenResults;
    }
}

static void
recover(Inbe *l)
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
                l->phase = InbePhaseNext;
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
hold(Inbe *l)
{
	l->sectick++;
	if(l->sectick < 60)
		return;
	l->sectick = 0;

	inccount(l->count);
}

void
inbestep(Inbe *l)
{
	if(l == 0)
		return;

	switch(l->phase){
	case InbePhaseBreathe:
		breathe(l);
		break;
	case InbePhaseHold:
		hold(l);
		break;
	case InbePhaseRecover:
		recover(l);
		break;
	case InbePhaseNext:
		next(l);
		break;
	}
}
