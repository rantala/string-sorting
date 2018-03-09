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

#include "routine.h"
#include "util/insertion_sort.h"
#include "util/get_char.h"
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
#include <array>

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

template <typename Bucket, typename BucketsizeType>
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
	std::array<BucketsizeType, 256> bucketsize;
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
		msd_D<Bucket, BucketsizeType>(strings+pos, bucketsize[i], depth+1, buckets);
		pos += bucketsize[i];
	}
}

template <typename Bucket>
static void
msd_D_adaptive(unsigned char** strings, size_t n, size_t depth, Bucket* buckets)
{
	if (n < 0x10000) {
		msd_D<Bucket, uint16_t>(strings, n, depth, buckets);
		return;
	}
	size_t* bucketsize = (size_t*) malloc(0x10000 * sizeof(size_t));
	size_t i=0;
	for (; i < n-n%16; i+=16) {
		uint16_t cache[16];
		for (size_t j=0; j < 16; ++j) {
			cache[j] = get_char<uint16_t>(strings[i+j], depth);
		}
		for (size_t j=0; j < 16; ++j) {
			buckets[cache[j]].push_back(strings[i+j]);
		}
	}
	for (; i < n; ++i) {
		const uint16_t ch = get_char<uint16_t>(strings[i], depth);
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

#define MAKE_ALG2(name, vec)                                                   \
void msd_D_##name(unsigned char** strings, size_t n)                           \
{                                                                              \
        vec<unsigned char*> buckets[256];                                      \
        msd_D<vec<unsigned char*>, size_t>(strings, n, 0, buckets);            \
}                                                                              \
ROUTINE_REGISTER_SINGLECORE(msd_D_##name, "msd_D_"#name)                       \
void msd_D_##name##_adaptive(unsigned char** strings, size_t n)                \
{                                                                              \
        vec<unsigned char*>* buckets = new vec<unsigned char*>[0x10000];       \
        msd_D_adaptive(strings, n, 0, buckets);                                \
        delete [] buckets;                                                     \
}                                                                              \
ROUTINE_REGISTER_SINGLECORE(msd_D_##name##_adaptive, "msd_D_"#name"_adaptive")

#define MAKE_ALG1(vec) MAKE_ALG2(vec, vec)

MAKE_ALG2(std_vector, std::vector)
MAKE_ALG2(std_deque, std::deque)
MAKE_ALG2(std_list, counting_list)
MAKE_ALG1(vector_realloc)
MAKE_ALG1(vector_malloc)
MAKE_ALG1(vector_realloc_counter_clear)
MAKE_ALG1(vector_malloc_counter_clear)
MAKE_ALG1(vector_realloc_shrink_clear)
MAKE_ALG1(vector_block)
MAKE_ALG1(vector_brodnik)
MAKE_ALG1(vector_bagwell)
