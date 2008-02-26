/*
 *   Copyright 2007-2008 by Tommi Rantala <tommi.rantala@cs.helsinki.fi>
 *
 *   *************************************************************************
 *   * Use, modification, and distribution are subject to the Boost Software *
 *   * License, Version 1.0. (See accompanying file LICENSE or a copy at     *
 *   * http://www.boost.org/LICENSE_1_0.txt)                                 *
 *   *************************************************************************
 */

/*
 * Implementation of the double sweep MSD radix sort variant
 *   - uses O(n) oracle to reduce TLB and other cache misses
 *   - avoids memory stalls by building oracle first, then calculating the size
 *     of each bucket
 *   - uses superalphabet when the size of the subinput is large, normal
 *     alphabet otherwise
 */

#include "util.h"

static inline uint16_t
double_char(unsigned char* str, size_t depth)
{
	unsigned c = str[depth];
	if (c == 0) return 0;
	return (c << 8) | str[depth+1];
}

void msd2(unsigned char** strings, size_t n, size_t depth);

void
msd3(unsigned char** strings, size_t n, size_t depth)
{
	if (n < 0x10000) {
		msd2(strings, n, depth);
		return;
	}
	uint16_t* restrict oracle =
		(uint16_t*) malloc(n*sizeof(uint16_t));
	for (size_t i=0; i < n; ++i)
		oracle[i] = double_char(strings[i], depth);
	size_t* restrict bucketsize = (size_t*)
		calloc(0x10000, sizeof(size_t));
	for (size_t i=0; i < n; ++i)
		++bucketsize[oracle[i]];
	unsigned char** sorted = (unsigned char**)
		malloc(n*sizeof(unsigned char*));
	static size_t bucketindex[0x10000];
	bucketindex[0] = 0;
	for (size_t i=1; i < 0x10000; ++i)
		bucketindex[i] = bucketindex[i-1]+bucketsize[i-1];
	for (size_t i=0; i < n; ++i)
		sorted[bucketindex[oracle[i]]++] = strings[i];
	memcpy(strings, sorted, n*sizeof(unsigned char*));
	free(sorted);
	free(oracle);
	size_t bsum = bucketsize[0];
	for (size_t i=1; i < 0x10000; ++i) {
		if (bucketsize[i] == 0) continue;
		if (i & 0xFF) msd3(strings+bsum,
				bucketsize[i], depth+2);
		bsum += bucketsize[i];
	}
	free(bucketsize);
}

void msd3(unsigned char** strings, size_t n)
{ msd3(strings, n, 0); }
