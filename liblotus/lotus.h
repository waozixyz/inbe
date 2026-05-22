#ifndef LOTUS_H
#define LOTUS_H

enum {
	LotusScreenStart = 0,
	LotusScreenSession,
	LotusScreenResults
};

enum {
    CountSize = 4
};

void inccount(char v[CountSize]);

#endif