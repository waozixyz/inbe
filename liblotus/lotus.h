#ifndef LOTUS_H
#define LOTUS_H

enum {
	Rshift = 3,
	Runit = 1 << Rshift
};

enum {
    CountSize = 4,
    MaxRounds = 4
};

enum {
	LotusScreenStart = 0,
	LotusScreenSession,
	LotusScreenResults
};

enum {
	LotusPhaseBreathe = 0,
	LotusPhaseHold,
	LotusPhaseRecover,
	LotusPhaseNext
};

typedef struct Lotus Lotus;

struct Lotus {
	int screen;
	int phase;
	int round;

    int r;
    int rmin;
    int rmax;
    int dir;
    int speed;
    int frame;

	int breathtick;
	int breathtickmax;
	int sectick;
	int halftick;

    char count[CountSize];
    char maxbreaths[CountSize];
};

void lotusinit(Lotus *l);
void lotusstep(Lotus *l);
void inccount(char v[CountSize]);

#endif