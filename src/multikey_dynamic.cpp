/*
 * Copyright 2008 by Tommi Rantala <tommi.rantala@cs.helsinki.fi>
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
 * A variant of the Multi-Key-Quicksort using dynamic arrays to store the three
 * buckets.
 */

#include "util/insertion_sort.h"
#include "util/get_char.h"
#include "util/median.h"
#include <inttypes.h>
#include <cassert>
#include <boost/array.hpp>
#include "vector_bagwell.h"
#include "vector_brodnik.h"
#include "vector_block.h"

template <typename CharT>
static inline unsigned
get_bucket(CharT c, CharT pivot)
{
        return ((c > pivot) << 1) | (c == pivot);
}

static inline void
copy(const std::vector<unsigned char*>& bucket, unsigned char** dst)
{
	std::copy(bucket.begin(), bucket.end(), dst);
}

extern "C" void mkqsort(unsigned char**, int, int);

template <typename BucketT, typename CharT>
static void
multikey_dynamic(unsigned char** strings, size_t N, size_t depth)
{
	if (N < 10000) {
		mkqsort(strings, N, depth);
		return;
	}
	/*if (N < 32) {
		insertion_sort(strings, N, depth);
		return;
	}
	*/
	boost::array<BucketT, 3> buckets;
	CharT partval = pseudo_median<CharT>(strings, N, depth);
	// Use a small cache to reduce memory stalls.
	size_t i=0;
	for (; i < N-N%32; i+=32) {
		boost::array<CharT, 32> cache;
		for (unsigned j=0; j<32; ++j) {
			cache[j] = get_char<CharT>(strings[i+j], depth);
		}
		for (unsigned j=0; j<32; ++j) {
			const unsigned b = get_bucket(cache[j], partval);
			buckets[b].push_back(strings[i+j]);
		}
	}
	for (; i < N; ++i) {
		const CharT c = get_char<CharT>(strings[i], depth);
		const unsigned b = get_bucket(c, partval);
		buckets[b].push_back(strings[i]);
	}
	boost::array<size_t, 3> bucketsize = {
		buckets[0].size(),
		buckets[1].size(),
		buckets[2].size()};
	assert(bucketsize[0] + bucketsize[1] + bucketsize[2] == N);
	if (bucketsize[0]) copy(buckets[0], strings);
	if (bucketsize[1]) copy(buckets[1], strings+bucketsize[0]);
	if (bucketsize[2]) copy(buckets[2], strings+bucketsize[0]+bucketsize[1]);
	buckets[0].clear();
	buckets[1].clear();
	buckets[2].clear();
	multikey_dynamic<BucketT, CharT>(strings, bucketsize[0], depth);
	if (not is_end(partval))
		multikey_dynamic<BucketT, CharT>(strings+bucketsize[0],
				bucketsize[1], depth+sizeof(CharT));
	multikey_dynamic<BucketT, CharT>(strings+bucketsize[0]+bucketsize[1],
			bucketsize[2], depth);
}

void multikey_dynamic_vector1(unsigned char** strings, size_t n)
{
	typedef std::vector<unsigned char*> BucketT;
	typedef unsigned char CharT;
	multikey_dynamic<BucketT, CharT>(strings, n, 0);
}
void multikey_dynamic_vector2(unsigned char** strings, size_t n)
{
	typedef std::vector<unsigned char*> BucketT;
	typedef uint16_t CharT;
	multikey_dynamic<BucketT, CharT>(strings, n, 0);
}
void multikey_dynamic_vector4(unsigned char** strings, size_t n)
{
	typedef std::vector<unsigned char*> BucketT;
	typedef uint32_t CharT;
	multikey_dynamic<BucketT, CharT>(strings, n, 0);
}

void multikey_dynamic_brodnik1(unsigned char** strings, size_t n)
{
	typedef vector_brodnik<unsigned char*> BucketT;
	typedef unsigned char CharT;
	multikey_dynamic<BucketT, CharT>(strings, n, 0);
}
void multikey_dynamic_brodnik2(unsigned char** strings, size_t n)
{
	typedef vector_brodnik<unsigned char*> BucketT;
	typedef uint16_t CharT;
	multikey_dynamic<BucketT, CharT>(strings, n, 0);
}
void multikey_dynamic_brodnik4(unsigned char** strings, size_t n)
{
	typedef vector_brodnik<unsigned char*> BucketT;
	typedef uint32_t CharT;
	multikey_dynamic<BucketT, CharT>(strings, n, 0);
}

void multikey_dynamic_bagwell1(unsigned char** strings, size_t n)
{
	typedef vector_bagwell<unsigned char*> BucketT;
	typedef unsigned char CharT;
	multikey_dynamic<BucketT, CharT>(strings, n, 0);
}
void multikey_dynamic_bagwell2(unsigned char** strings, size_t n)
{
	typedef vector_bagwell<unsigned char*> BucketT;
	typedef uint16_t CharT;
	multikey_dynamic<BucketT, CharT>(strings, n, 0);
}
void multikey_dynamic_bagwell4(unsigned char** strings, size_t n)
{
	typedef vector_bagwell<unsigned char*> BucketT;
	typedef uint32_t CharT;
	multikey_dynamic<BucketT, CharT>(strings, n, 0);
}

void multikey_dynamic_vector_block1(unsigned char** strings, size_t n)
{
	typedef vector_block<unsigned char*> BucketT;
	typedef unsigned char CharT;
	multikey_dynamic<BucketT, CharT>(strings, n, 0);
}
void multikey_dynamic_vector_block2(unsigned char** strings, size_t n)
{
	typedef vector_block<unsigned char*> BucketT;
	typedef uint16_t CharT;
	multikey_dynamic<BucketT, CharT>(strings, n, 0);
}
void multikey_dynamic_vector_block4(unsigned char** strings, size_t n)
{
	typedef vector_block<unsigned char*> BucketT;
	typedef uint32_t CharT;
	multikey_dynamic<BucketT, CharT>(strings, n, 0);
}
