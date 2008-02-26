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
 * Bare bones implementation of the double sweep MSD radix sort variant. Used
 * for comparison purposes.
 */

#include "util.h"

void
msd0(unsigned char** strings, size_t n, size_t depth)
{
	if (n < 32) {
		insertion_sort(strings, n, depth);
		return;
	}
	size_t bucketsize[256] = {0};
	for (size_t i=0; i < n; ++i)
		++bucketsize[strings[i][depth]];
	unsigned char** sorted = (unsigned char**)
		malloc(n*sizeof(unsigned char*));
	static size_t bucketindex[256];
	bucketindex[0] = 0;
	for (size_t i=1; i < 256; ++i)
		bucketindex[i] = bucketindex[i-1]+bucketsize[i-1];
	for (size_t i=0; i < n; ++i)
		sorted[bucketindex[strings[i][depth]]++] = strings[i];
	memcpy(strings, sorted, n*sizeof(unsigned char*));
	free(sorted);
	size_t bsum = bucketsize[0];
	for (size_t i=1; i < 256; ++i) {
		if (bucketsize[i] == 0) continue;
		msd0(strings+bsum, bucketsize[i], depth+1);
		bsum += bucketsize[i];
	}
}

void msd0(unsigned char** strings, size_t n)
{ msd0(strings, n, 0); }
