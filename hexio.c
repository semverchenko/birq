#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include "bit_array.h"
#include "hexio.h"

int bitmask_scnprintf(char *buf, size_t buflen, const BIT_ARRAY *bmp)
{
	int i = HOW_MANY(bmp->num_of_bits, HEXCHUNKSZ) - 1;
	int len = 0;
	uint32_t val;
	buf[0] = 0;

	for (; i >= 0; i--) {
		val = bit_array_get_word32(bmp, i * HEXCHUNKSZ);
		if (val != 0 || len != 0 || i == 0 )
			len += snprintf(buf + len, MAX(buflen - len, 0),
				len ? ",%0*x" : "%0*x", HEXCHARSZ, val);
	}
	return len;
}

/*
 * Converts one hex digit to int
 */
int hex_to_dec(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else
		return -EINVAL;
}

/*
 * Returns 32-bit token or -EOVERFLOW or -EINVAL in case of error
 */
int64_t next_chunk(const char **buf, size_t *buflen)
{
	uint32_t chunk = 0;
	int h;
	while (*buflen)
	{
		if (**buf == '\0' || **buf == ',')
		{
			(*buf)++, (*buflen)--;
			return chunk;
		}
		
		if (isspace(**buf))
		{
			while (isspace(**buf) && *buflen)
				(*buf)++, (*buflen)--;
			if (buflen && **buf != '\0')
				return -EINVAL;
			else
				return chunk;
		}

		h = hex_to_dec(**buf);
		if (h < 0)
			return h;

		if (chunk > CHUNK_MASK >> 4)
			return -EOVERFLOW;

		chunk = (chunk << 4) | h;
		(*buf)++, (*buflen)--;
	}
	return chunk;
}

int count_chunks(const char *buf, size_t buflen)
{
	int chunks = 0;
	int64_t chunk;
	while (buflen && *buf != '\0')
	{
		if ((chunk = next_chunk(&buf, &buflen)) >= 0)
			chunks++;
		else
			return (int)chunk;
	}
	return chunks;
}

/*
 * Returns 0 or -EOVERFLOW or -EINVAL in case of error
 */
int bitmask_parse_user(const char *buf, size_t buflen, BIT_ARRAY *bmp)
{
	int nchunks = 0;
	int64_t chunk;

	nchunks = count_chunks(buf, buflen);
	if (nchunks < 0)
		return nchunks;

	bit_array_clear_all(bmp);

	while (nchunks)
	{
		chunk = next_chunk(&buf, &buflen);
		if (chunk < 0)
			return chunk;
		nchunks--;
		bit_array_set_word32(bmp, nchunks * HEXCHUNKSZ, (uint32_t)chunk);
	}
	return 0;
}