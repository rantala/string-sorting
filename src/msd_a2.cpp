/*
 * Copyright 2008 by Tommi Rantala <tt.rantala@gmail.com>
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
 * msd_A2 is identical to msd_A, with one exception: we now use the original
 * input array as temporary space. msd_A is memory hungry, because it uses the
 * external array distribution method.
 */

#include "util/debug.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <inttypes.h>
#include <boost/array.hpp>

#define CACHED_BYTES 4

typedef struct {
	unsigned char bytes[CACHED_BYTES];
	unsigned char* ptr;
} cacheblock_t;

static inline void
inssort_cache(cacheblock_t* cache, int n, size_t depth)
{
	cacheblock_t *pi, *pj;
	unsigned char *s, *t;
	for (pi = cache + 1; --n > 0; ++pi) {
		unsigned char* tmp = pi->ptr;
		for (pj = pi; pj > cache; --pj) {
			t = tmp + depth;
			for (s=(pj-1)->ptr+depth; *s==*t && *s!=0; ++s, ++t)
				;
			if (*s <= *t)
				break;
			pj->ptr = (pj-1)->ptr;
		}
		pj->ptr = tmp;
	}
}

static void
fill_cache(cacheblock_t* cache, size_t N, size_t depth)
{
	for (size_t i=0; i < N; ++i) {
		unsigned int j=0;
		while (j < CACHED_BYTES && cache[i].ptr[depth+j]) {
			cache[i].bytes[j] = cache[i].ptr[depth+j];
			++j;
		}
		while (j < CACHED_BYTES) {
			cache[i].bytes[j] = 0;
			++j;
		}
	}
}

struct TempSpace
{
	cacheblock_t* strings;
	cacheblock_t* allocated;
	size_t elements_in_strings;
	TempSpace(unsigned char** strs, size_t n)
		: strings(0), allocated(0), elements_in_strings(0)
	{
		debug()<<__PRETTY_FUNCTION__<<"\n";
		char* raw = reinterpret_cast<char*>(strs);
		size_t rawbytes = n*sizeof(unsigned char*);
		if (ptrdiff_t(raw) % sizeof(cacheblock_t)) {
			unsigned diff = ptrdiff_t(raw) % sizeof(cacheblock_t);
			debug()<<"\t: alignment mismatch by "<<diff<<"bytes\n";
			raw      += diff;
			rawbytes -= diff;
		}
		if (rawbytes % sizeof(cacheblock_t)) {
			unsigned diff = rawbytes % sizeof(cacheblock_t);
			debug()<<"\t: truncate by "<<diff<<"bytes\n";
			rawbytes -= diff;
		}
		strings = reinterpret_cast<cacheblock_t*>(raw);
		elements_in_strings = rawbytes / sizeof(cacheblock_t);
	}
	cacheblock_t& operator[](size_t index)
	{
		if (index < elements_in_strings) {
			return strings[index];
		} else {
			assert(allocated);
			return allocated[index-elements_in_strings];
		}
	}
	void allocate(size_t elems)
	{
		assert(allocated==0);
		if (elems > elements_in_strings) {
			allocated = static_cast<cacheblock_t*>(
					malloc((elems-elements_in_strings) *
						sizeof(cacheblock_t)));
		}
	}
	void free()
	{
		if (allocated) {
			::free(allocated);
			allocated = 0;
		}
	}
};

static inline void
copy(TempSpace& src, cacheblock_t* dst, size_t n)
{
	if (n > src.elements_in_strings) {
		(void) memcpy(dst, src.strings,
			src.elements_in_strings*sizeof(cacheblock_t));
		(void) memcpy(dst+src.elements_in_strings, src.allocated,
			(n-src.elements_in_strings)*sizeof(cacheblock_t));
	} else {
		(void) memcpy(dst, src.strings, n*sizeof(cacheblock_t));
	}
}

static void
msd_A2(cacheblock_t* cache,
       size_t N,
       size_t cache_depth,
       size_t true_depth,
       TempSpace& tmp)
{
	if (N < 32) {
		inssort_cache(cache, N, true_depth);
		return;
	}
	if (cache_depth >= CACHED_BYTES) {
		fill_cache(cache, N, true_depth);
		cache_depth = 0;
	}
	boost::array<size_t, 256> bucketsize;
	bucketsize.assign(0);
	for (size_t i=0; i < N; ++i)
		++bucketsize[cache[i].bytes[cache_depth]];
	tmp.allocate(N);
	static boost::array<size_t, 256> bucketindex;
	bucketindex[0] = 0;
	for (unsigned i=1; i < 256; ++i)
		bucketindex[i] = bucketindex[i-1] + bucketsize[i-1];
	for (size_t i=0; i < N; ++i)
		tmp[bucketindex[cache[i].bytes[cache_depth]]++] = cache[i];
	copy(tmp, cache, N);
	tmp.free();
	size_t bsum = bucketsize[0];
	for (unsigned i=1; i < 256; ++i) {
		if (bucketsize[i] == 0) continue;
		msd_A2(cache+bsum, bucketsize[i],
			  cache_depth+1, true_depth+1, tmp);
		bsum += bucketsize[i];
	}
}

static void
msd_A2_adaptive(cacheblock_t* cache,
                size_t N,
                size_t cache_depth,
                size_t true_depth,
                TempSpace& tmp)
{
	if (N < 0x10000) {
		msd_A2(cache, N, cache_depth, true_depth, tmp);
		return;
	}
	if (cache_depth >= CACHED_BYTES) {
		fill_cache(cache, N, true_depth);
		cache_depth = 0;
	}
	tmp.allocate(N);
	size_t* bucketsize = static_cast<size_t*>(calloc(0x10000,
				sizeof(size_t)));
	for (size_t i=0; i < N; ++i) {
		uint16_t bucket =
			(cache[i].bytes[cache_depth] << 8) |
			 cache[i].bytes[cache_depth+1];
		++bucketsize[bucket];
	}
	static boost::array<size_t, 0x10000> bucketindex;
	bucketindex[0] = 0;
	for (unsigned i=1; i < 0x10000; ++i)
		bucketindex[i] = bucketindex[i-1] + bucketsize[i-1];
	for (size_t i=0; i < N; ++i) {
		uint16_t bucket = (cache[i].bytes[cache_depth] << 8)
			| cache[i].bytes[cache_depth+1];
		tmp[bucketindex[bucket]++] = cache[i];
	}
	copy(tmp, cache, N);
	tmp.free();
	size_t bsum = bucketsize[0];
	for (unsigned i=1; i < 0x10000; ++i) {
		if (bucketsize[i] == 0) continue;
		if (i & 0xFF) msd_A2_adaptive(cache+bsum, bucketsize[i],
				cache_depth+2, true_depth+2, tmp);
		bsum += bucketsize[i];
	}
	free(bucketsize);
}

void
msd_A2(unsigned char** strings, size_t N)
{
	cacheblock_t* cache =
		static_cast<cacheblock_t*>(malloc(N*sizeof(cacheblock_t)));
	for (size_t i=0; i < N; ++i) cache[i].ptr = strings[i];
	TempSpace tmp(strings, N);
	fill_cache(cache, N, 0);
	msd_A2(cache, N, 0, 0, tmp);
	for (size_t i=0; i < N; ++i) strings[i] = cache[i].ptr;
	free(cache);
}

void
msd_A2_adaptive(unsigned char** strings, size_t N)
{
	cacheblock_t* cache =
		static_cast<cacheblock_t*>(malloc(N*sizeof(cacheblock_t)));
	for (size_t i=0; i < N; ++i) cache[i].ptr = strings[i];
	TempSpace tmp(strings, N);
	fill_cache(cache, N, 0);
	msd_A2_adaptive(cache, N, 0, 0, tmp);
	for (size_t i=0; i < N; ++i) strings[i] = cache[i].ptr;
	free(cache);
}
