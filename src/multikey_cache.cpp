/*
 * Copyright 2007-2008,2012 by Tommi Rantala <tt.rantala@gmail.com>
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
 * multikey_cache() implements the Multi-Key-Quicksort using a O(n) cache. The
 * idea is to reduce the number of times we access the strings via pointers,
 * and to improve the locality of access patterns.
 *
 * Ng and Kakehi give results for similar ''CMKQ'' algorithm in the CRadix
 * paper, but they mainly focus on the radix sort variant.
 */

#include "routine.h"
#include "util/median.h"
#include <algorithm>

template <unsigned CachedChars>
struct Cacheblock;

template <>
struct Cacheblock<4>
{
	typedef uint32_t CacheType;

	uint32_t cached_bytes;
	unsigned char* ptr;
};

template <>
struct Cacheblock<8>
{
	typedef uint64_t CacheType;

	uint64_t cached_bytes;
	unsigned char* ptr;
};

struct Cmp
{
	template <unsigned CachedChars>
	int operator()(const Cacheblock<CachedChars>& lhs,
	               const Cacheblock<CachedChars>& rhs) const
	{
		if (lhs.cached_bytes > rhs.cached_bytes) return 1;
		if (lhs.cached_bytes < rhs.cached_bytes) return -1;
		return 0;
	}
};

// Insertion sort, ignores any cached characters.
template <unsigned CachedChars>
static inline void
insertion_sort(Cacheblock<CachedChars>* cache, int n, size_t depth)
{
	Cacheblock<CachedChars> *pi, *pj;
	unsigned char *s, *t;
	for (pi = cache + 1; --n > 0; ++pi) {
		unsigned char* tmp = pi->ptr;
		for (pj = pi; pj > cache; --pj) {
			for (s=(pj-1)->ptr+depth, t=tmp+depth; *s==*t && *s!=0;
					++s, ++t)
				;
			if (*s <= *t)
				break;
			pj->ptr = (pj-1)->ptr;
		}
		pj->ptr = tmp;
	}
}

// Insertion sorts the strings only based on the cached characters.
template <unsigned CachedChars>
static inline void
inssort_cache_block(Cacheblock<CachedChars>* cache, int n)
{
	Cacheblock<CachedChars> *pi, *pj;
	for (pi = cache + 1; --n > 0; ++pi) {
		Cacheblock<CachedChars> tmp = *pi;
		for (pj = pi; pj > cache; --pj) {
			if (Cmp()(*(pj-1), tmp) <= 0)
				break;
			*pj = *(pj-1);
		}
		*pj = tmp;
	}
}

// Fill the cache, but swap the characters in such order that we may load them
// as integers in little endian machines.
template <unsigned CachedChars>
static inline void
fill_cache(Cacheblock<CachedChars>* cache, size_t N, size_t depth)
{
	for (size_t i=0; i < N; ++i) {
		unsigned si=0, ci=CachedChars-1; //string index, cache index
		typename Cacheblock<CachedChars>::CacheType ch = 0;
		while (ci < CachedChars) {
			const typename Cacheblock<CachedChars>::CacheType c =
				cache[i].ptr[depth+si];
			ch |= (c << (ci*8));
			--ci; ++si;
			if (is_end(c)) break;
		}
		cache[i].cached_bytes = ch;
	}
}

template <unsigned CachedChars, bool CacheDirty>
static void
multikey_cache(Cacheblock<CachedChars>* cache, size_t N, size_t depth)
{
	if (N < 32) {
		if (N==0) return;
		if (CacheDirty) {
			insertion_sort(cache, N, depth);
			return;
		}
		inssort_cache_block(cache, N);
		size_t start=0, cnt=1;
		for (size_t i=0; i < N-1; ++i) {
			if (Cmp()(cache[i], cache[i+1]) == 0) {
				++cnt;
				continue;
			}
			if (cnt > 1 and cache[start].cached_bytes & 0xFF)
				insertion_sort(cache+start, cnt,
						depth+CachedChars);
			cnt = 1;
			start = i+1;
		}
		if (cnt > 1 and cache[start].cached_bytes & 0xFF)
			insertion_sort(cache+start, cnt, depth+CachedChars);
		return;
	}
	if (CacheDirty) {
		fill_cache(cache, N, depth);
	}
	// Move pivot to first position to avoid wrapping the unsigned values
	// we are using in the main loop from zero to max.
	std::swap(cache[0], med3char(
		med3char(cache[0],       cache[N/8],     cache[N/4],    Cmp()),
		med3char(cache[N/2-N/8], cache[N/2],     cache[N/2+N/8],Cmp()),
		med3char(cache[N-1-N/4], cache[N-1-N/8], cache[N-3],    Cmp()),
		Cmp()));
	Cacheblock<CachedChars> partval = cache[0];
	size_t first   = 1;
	size_t last    = N-1;
	size_t beg_ins = 1;
	size_t end_ins = N-1;
	while (true) {
		while (first <= last) {
			const int res = Cmp()(cache[first], partval);
			if (res > 0) {
				break;
			} else if (res == 0) {
				std::swap(cache[beg_ins++], cache[first]);
			}
			++first;
		}
		while (first <= last) {
			const int res = Cmp()(cache[last], partval);
			if (res < 0) {
				break;
			} else if (res == 0) {
				std::swap(cache[end_ins--], cache[last]);
			}
			--last;
		}
		if (first > last)
			break;
		std::swap(cache[first], cache[last]);
		++first;
		--last;
	}
	// Some calculations to make the code more readable.
	const size_t num_eq_beg = beg_ins;
	const size_t num_eq_end = N-1-end_ins;
	const size_t num_eq     = num_eq_beg+num_eq_end;
	const size_t num_lt     = first-beg_ins;
	const size_t num_gt     = end_ins-last;
	// Swap the equal pointers from the beginning to proper position.
	const size_t size1 = std::min(num_eq_beg, num_lt);
	std::swap_ranges(cache, cache+size1, cache+first-size1);
	// Swap the equal pointers from the end to proper position.
	const size_t size2 = std::min(num_eq_end, num_gt);
	std::swap_ranges(cache+first, cache+first+size2, cache+N-size2);
	// Now recurse.
	multikey_cache<CachedChars, false>(cache, num_lt, depth);
	multikey_cache<CachedChars, false>(cache+num_lt+num_eq, num_gt, depth);
	if (partval.cached_bytes & 0xFF)
		multikey_cache<CachedChars, true>(
			cache+num_lt, num_eq, depth+CachedChars);
}

template <unsigned CachedChars>
static inline void
multikey_cache(unsigned char** strings, size_t n, size_t depth)
{
	Cacheblock<CachedChars>* cache =
		static_cast<Cacheblock<CachedChars>*>(
			malloc(n*sizeof(Cacheblock<CachedChars>)));
	for (size_t i=0; i < n; ++i) {
		cache[i].ptr = strings[i];
	}
	multikey_cache<CachedChars, true>(cache, n, depth);
	for (size_t i=0; i < n; ++i) {
		strings[i] = cache[i].ptr;
	}
	free(cache);
}

void multikey_cache4(unsigned char** strings, size_t n)
{ multikey_cache<4>(strings, n, 0); }

void multikey_cache8(unsigned char** strings, size_t n)
{ multikey_cache<8>(strings, n, 0); }

ROUTINE_REGISTER_SINGLECORE(multikey_cache4, "multikey_cache with 4byte cache")
ROUTINE_REGISTER_SINGLECORE(multikey_cache8, "multikey_cache with 8byte cache")
