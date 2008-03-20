/*
 * Copyright 2007-2008 by Tommi Rantala <tommi.rantala@cs.helsinki.fi>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * These "CE" variants use the double sweep counting sort to implement MSD
 * radix sort. The variants also use an external array of size O(n) when
 * distributing the strings into correct order.
 *
 * msd_CE0: Bare bones implementation, useful for comparison purposes.
 *
 * msd_CE1: Otherwise the same as CE0, except:
 *   - uses O(n) oracle to reduce TLB and other cache misses
 *
 * msd_CE2: Otherwise the same as CE0, except:
 *   - uses O(n) oracle to reduce TLB and other cache misses
 *   - avoids memory stalls by building oracle first, then calculating the size
 *     of each bucket
 *
 * msd_CE3: Otherwise the same as CE0, except:
 *   - uses O(n) oracle to reduce TLB and other cache misses
 *   - avoids memory stalls by building oracle first, then calculating the size
 *     of each bucket
 *   - uses superalphabet when the size of the subinput is large, normal
 *     alphabet otherwise
 *
 * C3 is the fastest implementation in most cases.
 */

#include "util/insertion_sort.h"
#include "util/get_char.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>

void
msd_CE0(unsigned char** strings, size_t n, size_t depth)
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
		msd_CE0(strings+bsum, bucketsize[i], depth+1);
		bsum += bucketsize[i];
	}
}

void msd_CE0(unsigned char** strings, size_t n)
{ msd_CE0(strings, n, 0); }

void
msd_CE1(unsigned char** strings, size_t n, size_t depth)
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
		msd_CE1(strings+bsum, bucketsize[i], depth+1);
		bsum += bucketsize[i];
	}
}

void msd_CE1(unsigned char** strings, size_t n)
{ msd_CE1(strings, n, 0); }

void
msd_CE2(unsigned char** strings, size_t n, size_t depth)
{
	if (n < 32) {
		insertion_sort(strings, n, depth);
		return;
	}
	size_t bucketsize[256] = {0};
	unsigned char* restrict oracle =
		(unsigned char*) malloc(n);
	for (size_t i=0; i < n; ++i)
		oracle[i] = strings[i][depth];
	for (size_t i=0; i < n; ++i)
		++bucketsize[oracle[i]];
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
	for (size_t i=1; i < 256; ++i) {
		if (bucketsize[i] == 0) continue;
		msd_CE2(strings+bsum, bucketsize[i], depth+1);
		bsum += bucketsize[i];
	}
}

void msd_CE2(unsigned char** strings, size_t n)
{ msd_CE2(strings, n, 0); }

void
msd_CE3(unsigned char** strings, size_t n, size_t depth)
{
	if (n < 0x10000) {
		msd_CE2(strings, n, depth);
		return;
	}
	uint16_t* restrict oracle =
		(uint16_t*) malloc(n*sizeof(uint16_t));
	for (size_t i=0; i < n; ++i)
		oracle[i] = get_char<uint16_t>(strings[i], depth);
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
		if (i & 0xFF) msd_CE3(strings+bsum,
				bucketsize[i], depth+2);
		bsum += bucketsize[i];
	}
	free(bucketsize);
}

void msd_CE3(unsigned char** strings, size_t n)
{ msd_CE3(strings, n, 0); }
