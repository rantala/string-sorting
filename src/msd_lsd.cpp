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

/* Otherwise identical to msd_A, but uses LSD instead of MSD radix sort to sort
 * the buffer. This variant can be seen as a hybrid of MSD and LSD radix sorts.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <boost/array.hpp>
#include <boost/static_assert.hpp>

template <unsigned CachedChars>
struct Cacheblock
{
	boost::array<unsigned char, CachedChars> chars;
	unsigned char* ptr;
};

template <unsigned CachedChars>
static void
insertion_sort(Cacheblock<CachedChars>* cache, int n, size_t depth)
{
	Cacheblock<CachedChars>* pi;
	Cacheblock<CachedChars>* pj;
	for (pi = cache + 1; --n > 0; ++pi) {
		unsigned char* tmp = pi->ptr;
		for (pj = pi; pj > cache; --pj) {
			unsigned char* s = (pj-1)->ptr+depth;
			unsigned char* t = tmp+depth;
			for (; *s==*t && *s!=0; ++s, ++t)
				;
			if (*s <= *t)
				break;
			pj->ptr = (pj-1)->ptr;
		}
		pj->ptr = tmp;
	}
}

template <unsigned CachedChars>
static void
fill_cache(Cacheblock<CachedChars>* cache, size_t N, size_t depth)
{
	for (size_t i=0; i < N; ++i) {
		unsigned int j=0;
		while (j < CachedChars && cache[i].ptr[depth+j]) {
			cache[i].chars[j] = cache[i].ptr[depth+j];
			++j;
		}
		while (j < CachedChars) {
			cache[i].chars[j] = 0;
			++j;
		}
	}
}

template <unsigned CachedChars>
static void
msd_lsd(Cacheblock<CachedChars>* cache, size_t N, size_t depth)
{
	BOOST_STATIC_ASSERT(CachedChars>=1);
	if (N < 32) {
		insertion_sort(cache, N, depth);
		return;
	}
	fill_cache(cache, N, depth);
	for (int byte=CachedChars-1; byte >= 0; --byte) {
		size_t bucketsize[256] = {0};
		for (size_t i=0; i < N; ++i)
			++bucketsize[cache[i].chars[byte]];
		Cacheblock<CachedChars>* sorted = (Cacheblock<CachedChars>*)
			malloc(N*sizeof(Cacheblock<CachedChars>));
		size_t bucketindex[256];
		bucketindex[0] = 0;
		for (size_t i=1; i < 256; ++i)
			bucketindex[i] = bucketindex[i-1] + bucketsize[i-1];
		for (size_t i=0; i < N; ++i)
			memcpy(&sorted[bucketindex[cache[i].chars[byte]]++],
					cache+i,
					sizeof(Cacheblock<CachedChars>));
		memcpy(cache, sorted, N*sizeof(Cacheblock<CachedChars>));
		free(sorted);
	}
	size_t start=0, cnt=1;
	for (size_t i=0; i < N-1; ++i) {
		if (memcmp(cache[i].chars.data(), cache[i+1].chars.data(),
				CachedChars) == 0) {
			++cnt;
			continue;
		}
		if (cnt > 1 && cache[start].chars[CachedChars-1] != 0) {
			msd_lsd(cache+start, cnt, depth+CachedChars);
		}
		cnt = 1;
		start = i+1;
	}
	if (cnt > 1 && cache[start].chars[CachedChars-1] != 0) {
		msd_lsd(cache+start, cnt, depth+CachedChars);
	}
}

template <unsigned CachedChars>
static void
msd_lsd_adaptive(Cacheblock<CachedChars>* cache, size_t N, size_t depth)
{
	BOOST_STATIC_ASSERT(CachedChars%2==0);
	BOOST_STATIC_ASSERT(CachedChars>=2);
	if (N < 0x10000) {
		msd_lsd(cache, N, depth);
		return;
	}
	fill_cache(cache, N, depth);
	for (int byte=CachedChars-1; byte > 0; byte -= 2) {
		size_t bucketsize[0x10000] = {0};
		for (size_t i=0; i < N; ++i) {
			uint16_t bucket =
				(cache[i].chars[byte-1] << 8) | cache[i].chars[byte];
			++bucketsize[bucket];
		}
		Cacheblock<CachedChars>* sorted = (Cacheblock<CachedChars>*)
			malloc(N*sizeof(Cacheblock<CachedChars>));
		static size_t bucketindex[0x10000];
		bucketindex[0] = 0;
		for (size_t i=1; i < 0x10000; ++i)
			bucketindex[i] = bucketindex[i-1] + bucketsize[i-1];
		for (size_t i=0; i < N; ++i) {
			uint16_t bucket =
				(cache[i].chars[byte-1] << 8) | cache[i].chars[byte];
			memcpy(&sorted[bucketindex[bucket]++],
					cache+i,
					sizeof(Cacheblock<CachedChars>));
		}
		memcpy(cache, sorted, N*sizeof(Cacheblock<CachedChars>));
		free(sorted);
	}
	size_t start=0, cnt=1;
	for (size_t i=0; i < N-1; ++i) {
		if (memcmp(cache[i].chars.data(), cache[i+1].chars.data(),
				CachedChars) == 0) {
			++cnt;
			continue;
		}
		if (cnt > 1 && cache[start].chars[CachedChars-1] != 0) {
			msd_lsd_adaptive(cache+start, cnt, depth+CachedChars);
		}
		cnt = 1;
		start = i+1;
	}
	if (cnt > 1 && cache[start].chars[CachedChars-1] != 0) {
		msd_lsd_adaptive(cache+start, cnt, depth+CachedChars);
	}
}

template <unsigned CachedChars>
static void
msd_A_lsd(unsigned char** strings, size_t N)
{
	Cacheblock<CachedChars>* cache = static_cast<Cacheblock<CachedChars>*>(
			malloc(N*sizeof(Cacheblock<CachedChars>)));
	for (size_t i=0; i < N; ++i) cache[i].ptr = strings[i];
	fill_cache(cache, N, 0);
	msd_lsd(cache, N, 0);
	for (size_t i=0; i < N; ++i) strings[i] = cache[i].ptr;
	free(cache);
}

template <unsigned CachedChars>
static void
msd_A_lsd_adaptive(unsigned char** strings, size_t N)
{
	Cacheblock<CachedChars>* cache = static_cast<Cacheblock<CachedChars>*>(
			malloc(N*sizeof(Cacheblock<CachedChars>)));
	for (size_t i=0; i < N; ++i) cache[i].ptr = strings[i];
	fill_cache(cache, N, 0);
	msd_lsd_adaptive(cache, N, 0);
	for (size_t i=0; i < N; ++i) strings[i] = cache[i].ptr;
	free(cache);
}

void msd_A_lsd4(unsigned char** strings, size_t N) {msd_A_lsd<4>(strings, N); }
void msd_A_lsd6(unsigned char** strings, size_t N) {msd_A_lsd<6>(strings, N); }
void msd_A_lsd8(unsigned char** strings, size_t N) {msd_A_lsd<8>(strings, N); }
void msd_A_lsd10(unsigned char** strings, size_t N){msd_A_lsd<10>(strings, N);}
void msd_A_lsd12(unsigned char** strings, size_t N){msd_A_lsd<12>(strings, N);}

void msd_A_lsd_adaptive4(unsigned char** strings, size_t N)
{ msd_A_lsd_adaptive<4>(strings, N); }
void msd_A_lsd_adaptive6(unsigned char** strings, size_t N)
{ msd_A_lsd_adaptive<6>(strings, N); }
void msd_A_lsd_adaptive8(unsigned char** strings, size_t N)
{ msd_A_lsd_adaptive<8>(strings, N); }
void msd_A_lsd_adaptive10(unsigned char** strings, size_t N)
{ msd_A_lsd_adaptive<10>(strings, N); }
void msd_A_lsd_adaptive12(unsigned char** strings, size_t N)
{ msd_A_lsd_adaptive<12>(strings, N); }
