#include "bit_array.h"

#ifndef HEX_IO_H
#define HEX_IO_H
#define HEXCHUNKSZ 32
#define HEXCHARSZ 8
#ifndef max
	#define max(x,y) ((x) > (y) ? (x) : (y))
#endif
#ifndef howmany
	#define howmany(x, y) (((x)+((y)-1))/(y))
#endif

int bitmask_displayhex(char *buf, int buflen, const BIT_ARRAY *bmp);;
int bitmask_parsehex(const char *buf, BIT_ARRAY *bmp);

#endif