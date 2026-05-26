#ifndef INBE_H
#define INBE_H

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
    MaxRounds = 12,
    DefaultMaxRounds = 4,
    DefaultMaxBreaths = 30,
    DefaultPauseSeconds = 2
};

enum {
	InbeScreenStart = 0,
	InbeScreenSession,
	InbeScreenResults,
    InbeScreenSettings,
    InbeScreenManual,
    InbeScreenHistory
};

enum {
    InbePhaseStarting = 0,
	InbePhaseBreathe,
	InbePhaseHold,
	InbePhaseRecover,
	InbePhaseNext
};

typedef struct Inbe Inbe;

struct Inbe {
	int screen;
	int phase;
	int round;

    int r;
    int rmin;
	int rmax;
    int dir;
    int speed_level;
    int breath_frame;
    int breath_half_ticks;
	int frame;

    int breathtick;
    int sectick;
    int halftick;
    int max_rounds;
    int pause_seconds;

    char count[CountSize];
    char maxbreaths[CountSize];

	char results[MaxRounds][CountSize];
};

void inbeinit(Inbe *l);
void inbestep(Inbe *l);
void inccount(char v[CountSize]);
void cpcount(char dst[CountSize], const char src[CountSize]);

#endif
