#include "lotus.h"

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
