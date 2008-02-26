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
 */

#include "util.h"

void
msd1(unsigned char** strings, size_t n, size_t depth)
{
	if (n < 32) {
		insertion_sort(strings, n, depth);
		return;
	}
	size_t bucketsize[256] = {0};
	unsigned char* restrict oracle =
		(unsigned char*) malloc(n);
	for (size_t i=0; i < n; ++i)
		++bucketsize[oracle[i] = strings[i][depth]];
	unsigned char** restrict sorted = (unsigned char**)
		malloc(n*sizeof(unsigned char*));
	size_t bucketindex[256];
	bucketindex[0] = 0;
	for (size_t i=1; i < 256; ++i)
		bucketindex[i] = bucketindex[i-1]+bucketsize[i-1];
	for (size_t i=0; i < n; ++i)
		sorted[bucketindex[oracle[i]]++] = strings[i];
	memcpy(strings, sorted, n*sizeof(unsigned char*));
	free(sorted);
	free(oracle);
	size_t bsum = bucketsize[0];
	for (unsigned short i=1; i < 256; ++i) {
		if (bucketsize[i] == 0) continue;
		msd1(strings+bsum, bucketsize[i], depth+1);
		bsum += bucketsize[i];
	}
}

void msd1(unsigned char** strings, size_t n)
{ msd1(strings, n, 0); }
