#include "inbe.h"

static int
eqcount(const char a[CountSize], const char b[CountSize])
{
	return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

static int
count_value(const char v[CountSize])
{
	int a = (v[0] >= '0' && v[0] <= '9') ? v[0] - '0' : 0;
	int b = (v[1] >= '0' && v[1] <= '9') ? v[1] - '0' : 0;
	int c = (v[2] >= '0' && v[2] <= '9') ? v[2] - '0' : 0;
	return a * 100 + b * 10 + c;
}

int
inbe_breath_half_ticks_for_speed(int speed)
{
	static const int breath_half_ticks[] = {108, 100, 93, 86, 80, 74, 68, 63, 58, 53, 48, 42, 36, 30, 24, 18};

	if(speed < 1)
		speed = 1;
	if(speed > 16)
		speed = 16;

	return breath_half_ticks[speed - 1];
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

int
inbe_get_play_in_background(Inbe *inbe)
{
	return inbe->play_in_background;
}

void
inbeinit(Inbe *l)
{
	if(l == 0)
		return;

    l->screen = InbeScreenStart;
    l->phase = InbePhaseStarting;
    l->round = 0;

	l->r = 25;
	l->rmin = 25;
	l->rmax = 50;
	l->dir = 0;
    l->speed_level = 3;
    l->breath_frame = 0;
    l->breath_half_ticks = 120;
	l->frame = 0;
	l->breathtick = 0;
	l->sectick = 0;
	l->halftick = 0;
    l->max_rounds = DefaultMaxRounds;
    l->pause_seconds = DefaultPauseSeconds;
    l->progressive_speed = 1;
#ifdef __ANDROID__
    l->play_in_background = 1;  // Enabled by default on Android
#else
    l->play_in_background = 0;  // Disabled on other platforms
#endif

    for(int i = 0; i < MaxRounds; i++) {
        cpcount(l->results[i], "000");
    }

	cpcount(l->count, "000");
	cpcount(l->maxbreaths, "030");
}

static int
lerp_int(int a, int b, int num, int den)
{
    int delta = b - a;
    int scaled = delta * num;

    if(scaled >= 0)
        scaled += den / 2;
    else
        scaled -= den / 2;

    return a + scaled / den;
}

static int
effective_breath_half_ticks(const Inbe *l)
{
    int target_ticks = inbe_breath_half_ticks_for_speed(l->speed_level);

    if(!l->progressive_speed || l->round != 0)
        return target_ticks;

    int completed_breaths = count_value(l->count);

    if(completed_breaths < 5)
        return inbe_breath_half_ticks_for_speed(1);

    if(completed_breaths >= 10)
        return target_ticks;

    return lerp_int(
        inbe_breath_half_ticks_for_speed(1),
        target_ticks,
        completed_breaths - 4,
        5
    );
}


static void
starting(Inbe *l)
{
    int pause_ticks = l->pause_seconds * 60;

    if(pause_ticks <= 0) {
        l->sectick = 0;
        l->phase = InbePhaseBreathe;
        return;
    }

    l->sectick++;
    if(l->sectick < pause_ticks)
        return;

    l->sectick = 0;
    l->phase = InbePhaseBreathe;
}

static void
breathe(Inbe *l)
{
    int span = l->rmax - l->rmin;
    int eased;
    int half_ticks = effective_breath_half_ticks(l);

    if(span <= 0 || half_ticks <= 0)
        return;

    l->breath_frame++;
    if(l->breath_frame > half_ticks)
        l->breath_frame = half_ticks;

    eased = (l->breath_frame * span) / half_ticks;
    if(l->dir == 0)
        l->r = l->rmin + eased;
    else
        l->r = l->rmax - eased;

    if(l->breath_frame >= half_ticks) {
        l->breath_frame = 0;
        if(l->dir == 0) {
            l->dir = 1;
            l->r = l->rmax;
        } else {
            l->dir = 0;
            l->r = l->rmin;
            inccount(l->count);
        }
    }

	if(count_value(l->count) >= count_value(l->maxbreaths)){
		cpcount(l->count, "000");
		l->phase = InbePhaseHold;
	}
}

static void
next(Inbe *l)
{
    int span = l->rmax - l->rmin;
    int eased;

    if(span <= 0 || l->breath_half_ticks <= 0)
        return;

    l->breath_frame++;
    if(l->breath_frame > l->breath_half_ticks)
        l->breath_frame = l->breath_half_ticks;

    eased = (l->breath_frame * span) / l->breath_half_ticks;
    l->r = l->rmax - eased;

    if(l->breath_frame >= l->breath_half_ticks) {
        l->breath_frame = 0;
        l->r = l->rmin;
        cpcount(l->count, "000");
        if(l->round < l->max_rounds - 1){
            l->round++;
            l->phase = InbePhaseStarting;
        }else{
            l->screen = InbeScreenResults;
        }
    }
}

static void
recover(Inbe *l)
{
    int span = l->rmax - l->rmin;
    int eased;

    if(span <= 0 || l->breath_half_ticks <= 0)
        return;

    if(l->r < l->rmax) {
        l->breath_frame++;
        if(l->breath_frame > l->breath_half_ticks)
            l->breath_frame = l->breath_half_ticks;

        eased = (l->breath_frame * span) / l->breath_half_ticks;
        l->r = l->rmin + eased;

        if(l->breath_frame >= l->breath_half_ticks) {
            l->breath_frame = 0;
            l->sectick = 0;
            l->r = l->rmax;
        }
        return;
    }

    if(l->count[0] != '0' || l->count[1] != '0' || l->count[2] != '0') {
        l->sectick++;
        if(l->sectick < 60)
            return;
        l->sectick = 0;

        inccount(l->count);
        if(eqcount(l->count, "015")) {
            cpcount(l->count, "000");
            l->breath_frame = 0;
            l->phase = InbePhaseNext;
        }
        return;
    }

    l->sectick++;
    if(l->sectick < 60)
        return;
    l->sectick = 0;

    inccount(l->count);
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
	case InbePhaseStarting:
		starting(l);
		break;
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
