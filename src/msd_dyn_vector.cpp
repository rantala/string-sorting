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
 * This file contains several variants of MSD radix sort that uses dynamic
 * buckets instead of first making the extra sweep over the input to calculate
 * each size.
 *
 * Because strings can be expensive to access (indirect addressing, cache
 * misses, memory stalls), these variants are actually rather efficient.
 *
 * There are several variants that use different choice of the actual dynamic
 * memory structure. For each implementation, there is also an adaptive
 * version, that uses two byte superalphabet when the size of the subinput is
 * large, and normal alphabet otherwise.
 */

#include "util/insertion_sort.h"
#include <cstring>
#include <cstddef>
#include <vector>
#include <list>
#include <deque>
#include <algorithm>
#include "vector_realloc.h"
#include "vector_malloc.h"
#include "vector_block.h"
#include "vector_brodnik.h"
#include "vector_bagwell.h"
#include <boost/array.hpp>

static inline uint16_t
double_char(unsigned char* str, size_t depth)
{
	unsigned c = str[depth];
	return (c << 8) | (c != 0 ? str[depth+1] : c);
}

// std::list::size() is O(n), so keep track of size manually.
template <typename T>
class counting_list : public std::list<T>
{
public:
	counting_list() : _size(0) {}

	void push_back(const T& x)
	{ 
		++_size;
		std::list<T>::push_back(x);
	}

	size_t size() const
	{
		return _size;
	}

	void clear()
	{
		_size = 0;
		std::list<T>::clear();
	}

private:
	size_t _size;
};

template <typename BucketT, typename OutputIterator>
static inline void
copy(const BucketT& bucket, OutputIterator dst)
{
	std::copy(bucket.begin(), bucket.end(), dst);
}

template <typename Bucket>
static void
msd_D(unsigned char** strings, size_t n, size_t depth, Bucket* buckets)
{
	if (n < 32) {
		insertion_sort(strings, n, depth);
		return;
	}
	// Use a small cache to reduce memory stalls.
	size_t i=0;
	for (; i < n-n%32; i+=32) {
		unsigned char cache[32];
		for (unsigned j=0; j < 32; ++j) {
			cache[j] = strings[i+j][depth];
		}
		for (unsigned j=0; j < 32; ++j) {
			buckets[cache[j]].push_back(strings[i+j]);
		}
	}
	for (; i < n; ++i) {
		buckets[strings[i][depth]].push_back(strings[i]);
	}
	boost::array<size_t, 256> bucketsize;
	for (unsigned i=0; i < 256; ++i) {
		bucketsize[i] = buckets[i].size();
	}
	size_t pos = 0;
	for (unsigned i=0; i < 256; ++i) {
		if (bucketsize[i] == 0) continue;
		copy(buckets[i], strings+pos);
		pos += bucketsize[i];
	}
	for (unsigned i=0; i < 256; ++i) {
		buckets[i].clear();
	}
	pos = bucketsize[0];
	for (unsigned i=1; i < 256; ++i) {
		if (bucketsize[i] == 0) continue;
		msd_D(strings+pos, bucketsize[i], depth+1, buckets);
		pos += bucketsize[i];
	}
}

template <typename Bucket>
static void
msd_D_adaptive(unsigned char** strings, size_t n, size_t depth, Bucket* buckets)
{
	if (n < 0x10000) {
		msd_D(strings, n, depth, buckets);
		return;
	}
	size_t* bucketsize = (size_t*) malloc(0x10000 * sizeof(size_t));
	size_t i=0;
	for (; i < n-n%16; i+=16) {
		uint16_t cache[16];
		for (size_t j=0; j < 16; ++j) {
			cache[j] = double_char(strings[i+j], depth);
		}
		for (size_t j=0; j < 16; ++j) {
			buckets[cache[j]].push_back(strings[i+j]);
		}
	}
	for (; i < n; ++i) {
		const uint16_t ch = double_char(strings[i], depth);
		buckets[ch].push_back(strings[i]);
	}
	for (unsigned i=0; i < 0x10000; ++i) {
		bucketsize[i] = buckets[i].size();
	}
	size_t pos = 0;
	for (unsigned i=0; i < 0x10000; ++i) {
		if (bucketsize[i] == 0) continue;
		copy(buckets[i], strings+pos);
		pos += bucketsize[i];
	}
	for (unsigned i=0; i < 0x10000; ++i) {
		buckets[i].clear();
	}
	pos = bucketsize[0];
	for (unsigned i=1; i < 0x10000; ++i) {
		if (bucketsize[i] == 0) continue;
		if (i & 0xFF) msd_D_adaptive(
				strings+pos, bucketsize[i],
				depth+2, buckets);
		pos += bucketsize[i];
	}
	free(bucketsize);
}

void msd_DV(unsigned char** strings, size_t n)
{
	std::vector<unsigned char*> buckets[256];
	msd_D(strings, n, 0, buckets);
}
void msd_DV_adaptive(unsigned char** strings, size_t n)
{
	std::vector<unsigned char*> buckets[0x10000];
	msd_D_adaptive(strings, n, 0, buckets);
}

void msd_DL(unsigned char** strings, size_t n)
{
	counting_list<unsigned char*> buckets[256];
	msd_D(strings, n, 0, buckets);
}
void msd_DL_adaptive(unsigned char** strings, size_t n)
{
	counting_list<unsigned char*> buckets[0x10000];
	msd_D_adaptive(strings, n, 0, buckets);
}

void msd_DD(unsigned char** strings, size_t n)
{
	std::deque<unsigned char*> buckets[256];
	msd_D(strings, n, 0, buckets);
}
void msd_DD_adaptive(unsigned char** strings, size_t n)
{
	std::deque<unsigned char*> buckets[0x10000];
	msd_D_adaptive(strings, n, 0, buckets);
}

void msd_DV_REALLOC(unsigned char** strings, size_t n)
{
	vector_realloc<unsigned char*> buckets[256];
	msd_D(strings, n, 0, buckets);
}
void msd_DV_REALLOC_adaptive(unsigned char** strings, size_t n)
{
	vector_realloc<unsigned char*, 32> buckets[0x10000];
	msd_D_adaptive(strings, n, 0, buckets);
}

void msd_DV_MALLOC(unsigned char** strings, size_t n)
{
	vector_malloc<unsigned char*> buckets[256];
	msd_D(strings, n, 0, buckets);
}
void msd_DV_MALLOC_adaptive(unsigned char** strings, size_t n)
{
	vector_malloc<unsigned char*, 32> buckets[0x10000];
	msd_D_adaptive(strings, n, 0, buckets);
}

void msd_DV_CHEAT_REALLOC(unsigned char** strings, size_t n)
{
	vector_realloc_counter_clear<unsigned char*> buckets[256];
	msd_D(strings, n, 0, buckets);
}
void msd_DV_CHEAT_REALLOC_adaptive(unsigned char** strings, size_t n)
{
	vector_realloc_counter_clear<unsigned char*, 32> buckets[0x10000];
	msd_D_adaptive(strings, n, 0, buckets);
}

void msd_DV_CHEAT_MALLOC(unsigned char** strings, size_t n)
{
	vector_malloc_counter_clear<unsigned char*> buckets[256];
	msd_D(strings, n, 0, buckets);
}
void msd_DV_CHEAT_MALLOC_adaptive(unsigned char** strings, size_t n)
{
	vector_malloc_counter_clear<unsigned char*, 32> buckets[0x10000];
	msd_D_adaptive(strings, n, 0, buckets);
}

void msd_D_vector_block(unsigned char** strings, size_t n)
{
	vector_block<unsigned char*> buckets[256];
	msd_D(strings, n, 0, buckets);
}

void msd_D_vector_brodnik(unsigned char** strings, size_t n)
{
	vector_brodnik<unsigned char*> buckets[256];
	msd_D(strings, n, 0, buckets);
}

void msd_D_vector_bagwell(unsigned char** strings, size_t n)
{
	vector_bagwell<unsigned char*> buckets[256];
	msd_D(strings, n, 0, buckets);
}
