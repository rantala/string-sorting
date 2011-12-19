/*
 * Copyright 2007-2008 by Tommi Rantala <tt.rantala@gmail.com>
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
 * msd_ci() is an implementation of the MSD radix sort using
 *  - double sweep counting sort
 *  - O(n) oracle to reduce cache misses and memory stalls
 *  - the in-place distribution method described by McIlroy, Bostic & McIlroy
 *
 * The adaptive variant msd_ci_adaptive() uses superalphabet when size of the
 * subinput is large, as described by Andersson & Nilsson.
 */

#include "routine.h"
#include "util/insertion_sort.h"
#include "util/get_char.h"
#include <cstddef>
#include <cstdlib>
#include <sys/types.h>

template <typename BucketType>
struct distblock {
	unsigned char* ptr;
	BucketType bucket;
};

static void
msd_ci(unsigned char** strings, size_t n, size_t depth)
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
	static ssize_t bucketindex[256];
	bucketindex[0] = bucketsize[0];
	size_t last_bucket_size = bucketsize[0];
	for (unsigned i=1; i < 256; ++i) {
		bucketindex[i] = bucketindex[i-1] + bucketsize[i];
		if (bucketsize[i]) last_bucket_size = bucketsize[i];
	}
	for (size_t i=0; i < n-last_bucket_size; ) {
		distblock<uint8_t> tmp = { strings[i], oracle[i] };
		while (1) {
			// Continue until the current bucket is completely in
			// place
			if (--bucketindex[tmp.bucket] <= i)
				break;
			// backup all information of the position we are about
			// to overwrite
			size_t backup_idx = bucketindex[tmp.bucket];
			distblock<uint8_t> tmp2 = { strings[backup_idx], oracle[backup_idx] };
			// overwrite everything, ie. move the string to correct
			// position
			strings[backup_idx] = tmp.ptr;
			oracle[backup_idx]  = tmp.bucket;
			tmp = tmp2;
		}
		// Commit last pointer to place. We don't need to copy the
		// oracle entry, it's not read after this.
		strings[i] = tmp.ptr;
		i += bucketsize[tmp.bucket];
	}
	free(oracle);
	size_t bsum = bucketsize[0];
	for (size_t i=1; i < 256; ++i) {
		if (bucketsize[i] == 0) continue;
		msd_ci(strings+bsum, bucketsize[i], depth+1);
		bsum += bucketsize[i];
	}
}

static void
msd_ci_adaptive(unsigned char** strings, size_t n, size_t depth)
{
	if (n < 0x10000) {
		msd_ci(strings, n, depth);
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
	static ssize_t bucketindex[0x10000];
	bucketindex[0] = bucketsize[0];
	size_t last_bucket_size = bucketsize[0];
	for (unsigned i=1; i < 0x10000; ++i) {
		bucketindex[i] = bucketindex[i-1] + bucketsize[i];
		if (bucketsize[i]) last_bucket_size = bucketsize[i];
	}
	for (size_t i=0; i < n-last_bucket_size; ) {
		distblock<uint16_t> tmp = { strings[i], oracle[i] };
		while (1) {
			// Continue until the current bucket is completely in
			// place
			if (--bucketindex[tmp.bucket] <= i)
				break;
			// backup all information of the position we are about
			// to overwrite
			size_t backup_idx = bucketindex[tmp.bucket];
			distblock<uint16_t> tmp2 = { strings[backup_idx], oracle[backup_idx] };
			// overwrite everything, ie. move the string to correct
			// position
			strings[backup_idx] = tmp.ptr;
			oracle[backup_idx]  = tmp.bucket;
			tmp = tmp2;
		}
		// Commit last pointer to place. We don't need to copy the
		// oracle entry, it's not read after this.
		strings[i] = tmp.ptr;
		i += bucketsize[tmp.bucket];
	}
	free(oracle);
	size_t bsum = bucketsize[0];
	for (size_t i=1; i < 0x10000; ++i) {
		if (bucketsize[i] == 0) continue;
		if (i & 0xFF) msd_ci_adaptive(strings+bsum,
				bucketsize[i], depth+2);
		bsum += bucketsize[i];
	}
	free(bucketsize);
}

void msd_ci(unsigned char** strings, size_t n)
{ msd_ci(strings, n, 0); }
void msd_ci_adaptive(unsigned char** strings, size_t n)
{ msd_ci_adaptive(strings, n, 0); }

ROUTINE_REGISTER_SINGLECORE(msd_ci, "msd_CI")
ROUTINE_REGISTER_SINGLECORE(msd_ci_adaptive, "msd_CI: adaptive")
