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
 * msd_ci() is an implementation of the MSD radix sort using
 *  - double sweep counting sort
 *  - O(n) oracle to reduce cache misses and memory stalls
 *  - the in-place distribution method described by McIlroy, Bostic & McIlroy
 *
 * The adaptive variant msd_ci_adaptive() uses superalphabet when size of the
 * subinput is large, as described by Andersson & Nilsson.
 */

#include "util.h"

template <typename BucketType>
struct distblock {
	unsigned char* ptr;
	BucketType bucket;
};

void
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

static inline uint16_t
double_char(unsigned char* str, size_t depth)
{
	unsigned c = str[depth];
	if (c == 0) return 0;
	return (c << 8) | str[depth+1];
}

void
msd_ci_adaptive(unsigned char** strings, size_t n, size_t depth)
{
	if (n < 0x10000) {
		msd_ci(strings, n, depth);
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
