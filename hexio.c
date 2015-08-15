/*
 * Temporaly use displayhex and parsehex from LGPL libbitmask 
 * Start of libbitmask code
 */

/*
 * bitmask user library implementation.
 *
 * Copyright (c) 2004-2006 Silicon Graphics, Inc. All rights reserved.
 *
 * Paul Jackson <pj@sgi.com>
 */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "bit_array.h"
#include "hexio.h"

/*
 * Write hex word representation of bmp to buf, 32 bits per
 * comma-separated, zero-filled hex word.  Do not write more
 * than buflen chars to buf.
 *
 * Return number of chars that would have been written
 * if buf were large enough.
 */

const char *nexttoken(const char *q,  int sep)
{
	if (q)
		q = strchr(q, sep);
	if (q)
		q++;
	return q;
}

char _getbit(const BIT_ARRAY* bitarr, bit_index_t b)
{
  if (b >= bitarr->num_of_bits)
  	return 0;
  return bit_array_get_bit(bitarr, b);
}

int bitmask_displayhex(char *buf, int buflen, const BIT_ARRAY *bmp)
{
	int chunk;
	int cnt = 0;
	const char *sep = "";

	if (buflen < 1)
		return 0;
	buf[0] = 0;

	for (chunk = howmany(bmp->num_of_bits, HEXCHUNKSZ); chunk >= 0; chunk--) {
		uint32_t val = 0;
		int bit;

		for (bit = HEXCHUNKSZ - 1; bit >= 0; bit--)
			val = val << 1 | _getbit(bmp, chunk * HEXCHUNKSZ + bit);
		if (val != 0 || cnt != 0 || chunk == 0 )
		{
			cnt += snprintf(buf + cnt, max(buflen - cnt, 0), "%s%0*x",
				sep, HEXCHARSZ, val);
			sep = ",";
		}
	}
	return cnt;
}

/*
 * Parse hex word representation in buf to bmp.
 *
 * Returns -1 on error, leaving unspecified results in bmp.
 */

int bitmask_parsehex(const char *buf, BIT_ARRAY *bmp)
{
	const char *p, *q;
	int nchunks = 0, chunk;

	bit_array_clear_all(bmp);

	q = buf;
	while (p = q, q = nexttoken(q, ','), p)
		nchunks++;

	chunk = nchunks - 1;
	q = buf;
	while (p = q, q = nexttoken(q, ','), p) {
		uint32_t val;
		int bit;
		char *endptr;
		int nchars_read, nchars_unread;

		val = strtoul(p, &endptr, 16);

		/* We should have consumed 1 to 8 (HEXCHARSZ) chars */
		nchars_read = endptr - p;
		if (nchars_read < 1 || nchars_read > HEXCHARSZ)
			goto err;

		/* We should have consumed up to next comma */
		nchars_unread = q - endptr;
		if (q != NULL && nchars_unread != 1)
			goto err;

		for (bit = HEXCHUNKSZ - 1; bit >= 0; bit--) {
			int n = chunk * HEXCHUNKSZ + bit;
			if (n >= bmp->num_of_bits)
				goto err;
			bit_array_assign_bit(bmp, n, (val >> bit) & 1);
		}
		chunk--;
	}
	return 0;
err:
	bit_array_clear_all(bmp);
	return -1;
}