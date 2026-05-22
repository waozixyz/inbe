#ifndef LOTUS_H
#define LOTUS_H

#if defined(__PLAN9__) || (!defined(__STDC__) && (defined(__i386__) || defined(__amd64__) || defined(__arm__)))
    typedef unsigned char      uint8_t;
    typedef signed char        int8_t;
    typedef unsigned short     uint16_t;
    typedef short              int16_t;
    typedef unsigned int       uint32_t;
    typedef int                int32_t;
    typedef unsigned long long uint64_t;
    typedef long long          int64_t;
#else
    #include <stdint.h>
#endif

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

	char results[MaxRounds][CountSize];
};

void lotusinit(Lotus *l);
void lotusstep(Lotus *l);
void inccount(char v[CountSize]);
void cpcount(char dst[CountSize], const char src[CountSize]);

#endif