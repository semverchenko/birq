#include "bit_array.h"

#ifndef HEX_IO_H
#define HEX_IO_H
#define HEXCHUNKSZ 32
#define HEXCHARSZ 8
#define CHUNK_MASK ((1ULL << HEXCHUNKSZ) - 1)
#define HOW_MANY(x, y) (((x) + (y) - 1) / (y))
#ifndef MAX
	#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

int bitmask_scnprintf(char *buf, size_t buflen, const BIT_ARRAY *bmp);
int bitmask_parse_user(const char *buf, size_t buflen, BIT_ARRAY *bmp);

#endif
