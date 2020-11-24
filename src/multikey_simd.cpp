/*
 * Copyright 2007-2008,2011 by Tommi Rantala <tt.rantala@gmail.com>
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
 * Implements the Multi-Key-Quicksort using O(n) oracle and SIMD methods for
 * branchlessly populating it. See also the sample sort paper:
 *
 *   Book Title - Algorithms - ESA 2004
 *   Chapter Title - Super Scalar Sample Sort
 *   First Page - 784
 *   Last Page - 796
 *   Copyright - 2004
 *   Author - Peter Sanders
 *   Author - Sebastian Winkel
 *   Link - http://www.springerlink.com/content/aqjvtlhtx9g8ncdx
 */

#include "routine.h"
#include "util/insertion_sort.h"
#include "util/get_char.h"
#include "util/median.h"
#include <inttypes.h>
#include <iostream>
#include <cassert>
#include <array>
#include <xmmintrin.h>

template <bool Prefetch>
static void
calculate_bucketsizes_sse(
		unsigned char** strings, size_t n,
		uint8_t* restrict oracle,
		uint8_t pivot, size_t depth)
{
	assert(n % 16 == 0);
	static const uint8_t _Constants[] __attribute__((aligned(16))) = {
		0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
		0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
		0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02
	};
	const uint8_t _Pivot[] __attribute__((aligned(16))) = {
		pivot, pivot, pivot, pivot, pivot, pivot, pivot, pivot,
		pivot, pivot, pivot, pivot, pivot, pivot, pivot, pivot
	};
	__m128i FlipBit = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+0));
	__m128i Mask1   = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+16));
	__m128i Mask2   = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+32));
	__m128i Pivot   = _mm_add_epi8(
		_mm_load_si128(reinterpret_cast<const __m128i*>(_Pivot)),
		FlipBit);
	for (size_t i=0; i < n; i += 16) {
		unsigned char data00[16] __attribute__ ((aligned (16)));
		if (Prefetch)
			for (unsigned j=0; j < 16; ++j)
				__builtin_prefetch(&strings[i+j+16][depth]);
		for (unsigned j=0; j < 16; ++j)
			data00[j] = strings[i+j][depth];
		__m128i d00 = _mm_load_si128(
				reinterpret_cast<__m128i*>(data00));
		d00 = _mm_add_epi8(d00, FlipBit);
		__m128i tmp1 = _mm_cmpeq_epi8(d00, Pivot);
		tmp1 = _mm_and_si128(tmp1, Mask1);
		__m128i tmp2 = _mm_cmpgt_epi8(d00, Pivot);
		tmp2 = _mm_and_si128(tmp2, Mask2);
		_mm_store_si128(reinterpret_cast<__m128i*>(oracle+i),
				_mm_or_si128(tmp1, tmp2));
	}
}

template <bool Prefetch>
static void
calculate_bucketsizes_sse(
		unsigned char** strings,
		size_t n,
		uint8_t* restrict oracle,
		uint16_t pivot,
		size_t depth)
{
	typedef uint16_t CharT;
	assert(n % 16 == 0);
	static const uint16_t _Constants[] __attribute__((aligned(16))) = {
		0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
		0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
		0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002
	};
	const uint16_t _Pivot[] __attribute__((aligned(16))) = {
		pivot, pivot, pivot, pivot, pivot, pivot, pivot, pivot
	};
	__m128i FlipBit = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+0));
	__m128i Mask1   = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+8));
	__m128i Mask2   = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+16));
	__m128i Pivot   = _mm_add_epi8(
		_mm_load_si128(reinterpret_cast<const __m128i*>(_Pivot)),
		FlipBit);
	for (size_t i=0; i < n; i += 16) {
		if (Prefetch)
			for (unsigned j=0; j < 16; ++j)
				__builtin_prefetch(&strings[i+j+16][depth]);
		const CharT data00[] __attribute__ ((aligned (16)))
			= {
				get_char<CharT>(strings[i+0 ], depth),
				get_char<CharT>(strings[i+2 ], depth),
				get_char<CharT>(strings[i+4 ], depth),
				get_char<CharT>(strings[i+6 ], depth),
				get_char<CharT>(strings[i+8 ], depth),
				get_char<CharT>(strings[i+10], depth),
				get_char<CharT>(strings[i+12], depth),
				get_char<CharT>(strings[i+14], depth)
			};
		const CharT data01[] __attribute__ ((aligned (16)))
			= {
				get_char<CharT>(strings[i+1 ], depth),
				get_char<CharT>(strings[i+3 ], depth),
				get_char<CharT>(strings[i+5 ], depth),
				get_char<CharT>(strings[i+7 ], depth),
				get_char<CharT>(strings[i+9 ], depth),
				get_char<CharT>(strings[i+11], depth),
				get_char<CharT>(strings[i+13], depth),
				get_char<CharT>(strings[i+15], depth)
			};
		__m128i d00 = _mm_load_si128(
				reinterpret_cast<const __m128i*>(data00));
		__m128i d01 = _mm_load_si128(
				reinterpret_cast<const __m128i*>(data01));
		d00 = _mm_add_epi16(d00, FlipBit);
		d01 = _mm_add_epi16(d01, FlipBit);
		__m128i tmp1 = _mm_cmpeq_epi16(d00, Pivot);
		tmp1 = _mm_and_si128(tmp1, Mask1);
		__m128i tmp2 = _mm_cmpgt_epi16(d00, Pivot);
		tmp2 = _mm_and_si128(tmp2, Mask2);
		__m128i tmp3 = _mm_cmpeq_epi16(d01, Pivot);
		tmp3 = _mm_and_si128(tmp3, Mask1);
		__m128i tmp4 = _mm_cmpgt_epi16(d01, Pivot);
		tmp4 = _mm_and_si128(tmp4, Mask2);
		_mm_store_si128(reinterpret_cast<__m128i*>(oracle+i),
				_mm_or_si128(
					_mm_or_si128(tmp1, tmp2),
					_mm_slli_epi64(
						_mm_or_si128(tmp3, tmp4), 8)
					));
	}
}

template <bool Prefetch>
static void
calculate_bucketsizes_sse(
		unsigned char** strings,
		size_t n,
		uint8_t* restrict oracle,
		uint32_t pivot,
		size_t depth)
{
	typedef uint32_t CharT;
	assert(n % 16 == 0);
	static const uint32_t _Constants[] __attribute__((aligned(16))) = {
		0x80000000, 0x80000000, 0x80000000, 0x80000000,
		0x00000001, 0x00000001, 0x00000001, 0x00000001,
		0x00000002, 0x00000002, 0x00000002, 0x00000002
	};
	const uint32_t _Pivot[] __attribute__((aligned(16))) = {
		pivot, pivot, pivot, pivot
	};
	__m128i FlipBit = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+0));
	__m128i Mask1   = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+4));
	__m128i Mask2   = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+8));
	__m128i Pivot   = _mm_add_epi8(
		_mm_load_si128(reinterpret_cast<const __m128i*>(_Pivot)),
		FlipBit);
	for (size_t i=0; i < n; i += 16) {
		if (Prefetch)
			for (unsigned j=0; j < 16; ++j)
				__builtin_prefetch(&strings[i+j+16][depth]);
		const CharT data00[] __attribute__ ((aligned (16)))
			= {
				get_char<CharT>(strings[i+0 ], depth),
				get_char<CharT>(strings[i+4 ], depth),
				get_char<CharT>(strings[i+8 ], depth),
				get_char<CharT>(strings[i+12], depth),
			};
		const CharT data01[] __attribute__ ((aligned (16)))
			= {
				get_char<CharT>(strings[i+1 ], depth),
				get_char<CharT>(strings[i+5 ], depth),
				get_char<CharT>(strings[i+9 ], depth),
				get_char<CharT>(strings[i+13], depth),
			};
		const CharT data02[] __attribute__ ((aligned (16)))
			= {
				get_char<CharT>(strings[i+2 ], depth),
				get_char<CharT>(strings[i+6 ], depth),
				get_char<CharT>(strings[i+10], depth),
				get_char<CharT>(strings[i+14], depth)
			};
		const CharT data03[] __attribute__ ((aligned (16)))
			= {
				get_char<CharT>(strings[i+3 ], depth),
				get_char<CharT>(strings[i+7 ], depth),
				get_char<CharT>(strings[i+11], depth),
				get_char<CharT>(strings[i+15], depth)
			};
		__m128i d00 = _mm_load_si128(
				reinterpret_cast<const __m128i*>(data00));
		__m128i d01 = _mm_load_si128(
				reinterpret_cast<const __m128i*>(data01));
		__m128i d02 = _mm_load_si128(
				reinterpret_cast<const __m128i*>(data02));
		__m128i d03 = _mm_load_si128(
				reinterpret_cast<const __m128i*>(data03));
		d00 = _mm_add_epi16(d00, FlipBit);
		d01 = _mm_add_epi16(d01, FlipBit);
		d02 = _mm_add_epi16(d02, FlipBit);
		d03 = _mm_add_epi16(d03, FlipBit);
		__m128i tmp1=_mm_and_si128(_mm_cmpeq_epi32(d00, Pivot), Mask1);
		__m128i tmp2=_mm_and_si128(_mm_cmpgt_epi32(d00, Pivot), Mask2);
		__m128i tmp3=_mm_and_si128(_mm_cmpeq_epi32(d01, Pivot), Mask1);
		__m128i tmp4=_mm_and_si128(_mm_cmpgt_epi32(d01, Pivot), Mask2);
		__m128i tmp5=_mm_and_si128(_mm_cmpeq_epi32(d02, Pivot), Mask1);
		__m128i tmp6=_mm_and_si128(_mm_cmpgt_epi32(d02, Pivot), Mask2);
		__m128i tmp7=_mm_and_si128(_mm_cmpeq_epi32(d03, Pivot), Mask1);
		__m128i tmp8=_mm_and_si128(_mm_cmpgt_epi32(d03, Pivot), Mask2);
		tmp1 = _mm_or_si128(tmp1, tmp2);
		tmp3 = _mm_slli_epi64(_mm_or_si128(tmp3, tmp4), 8 );
		tmp5 = _mm_slli_epi64(_mm_or_si128(tmp5, tmp6), 16);
		tmp7 = _mm_slli_epi64(_mm_or_si128(tmp7, tmp8), 24);
		_mm_store_si128(reinterpret_cast<__m128i*>(oracle+i),
				_mm_or_si128(_mm_or_si128(tmp1, tmp3),
				             _mm_or_si128(tmp5, tmp7))
			       );
	}
}

/******************************************************************************/

template <typename CharT>
static inline unsigned
get_bucket(CharT c, CharT pivot)
{
        return ((c > pivot) << 1) | (c == pivot);
}

template <typename CharT>
static void
multikey_simd(unsigned char** strings, size_t N, size_t depth)
{
	if (N < 32) {
		insertion_sort(strings, N, depth);
		return;
	}
	CharT partval = pseudo_median<CharT>(strings, N, depth);
	uint8_t* const restrict oracle =
		static_cast<uint8_t*>(_mm_malloc(N, 16));
	std::array<size_t, 3> bucketsize;
	bucketsize.fill(0);
	size_t i=N-N%16;
	calculate_bucketsizes_sse<false>(strings, i, oracle, partval, depth);
	for (; i < N; ++i)
		oracle[i] = get_bucket(
				get_char<CharT>(strings[i], depth),
				partval);
	for (i=0; i < N; ++i)
		++bucketsize[oracle[i]];
	assert(bucketsize[0] + bucketsize[1] + bucketsize[2] == N);
	unsigned char** sorted =
		static_cast<unsigned char**>(malloc(N*sizeof(unsigned char*)));
	size_t bucketindex[3];
	bucketindex[0] = 0;
	bucketindex[1] = bucketsize[0];
	bucketindex[2] = bucketsize[0] + bucketsize[1];
	for (size_t i=0; i < N; ++i) {
		sorted[bucketindex[oracle[i]]++] = strings[i];
	}
	std::copy(sorted, sorted+N, strings);
	free(sorted);
	_mm_free(oracle);
	multikey_simd<CharT>(strings, bucketsize[0], depth);
	if (not is_end(partval))
		multikey_simd<CharT>(strings+bucketsize[0],
				bucketsize[1], depth+sizeof(CharT));
	multikey_simd<CharT>(strings+bucketsize[0]+bucketsize[1],
			bucketsize[2], depth);
}

void multikey_simd1(unsigned char** strings, size_t n)
{ multikey_simd<unsigned char>(strings, n, 0); }

void multikey_simd2(unsigned char** strings, size_t n)
{ multikey_simd<uint16_t>(strings, n, 0); }

void multikey_simd4(unsigned char** strings, size_t n)
{ multikey_simd<uint32_t>(strings, n, 0); }

ROUTINE_REGISTER_SINGLECORE(multikey_simd1,
		"multikey_simd with 1byte alphabet")
ROUTINE_REGISTER_SINGLECORE(multikey_simd2,
		"multikey_simd with 2byte alphabet")
ROUTINE_REGISTER_SINGLECORE(multikey_simd4,
		"multikey_simd with 4byte alphabet")

/*
 * Same as multikey_simd(), but "sorted" and "oracle" memory is preallocated,
 * and prefetching is done to try to speed up string accesses.
 */
template <typename CharT>
static void
multikey_simd_b(unsigned char** strings, size_t N, size_t depth,
		unsigned char** restrict sorted, uint8_t* restrict oracle)
{
	if (N < 32) {
		insertion_sort(strings, N, depth);
		return;
	}
	CharT partval = pseudo_median<CharT>(strings, N, depth);
	std::array<size_t, 3> bucketsize;
	bucketsize.fill(0);
	size_t i=N-N%16;
	calculate_bucketsizes_sse<true>(strings, i, oracle, partval, depth);
	for (; i < N; ++i)
		oracle[i] = get_bucket(
				get_char<CharT>(strings[i], depth),
				partval);
	for (i=0; i < N; ++i) {
		__builtin_prefetch(&oracle[i+1]);
		++bucketsize[oracle[i]];
	}
	assert(bucketsize[0] + bucketsize[1] + bucketsize[2] == N);
	size_t bucketindex[3];
	bucketindex[0] = 0;
	bucketindex[1] = bucketsize[0];
	bucketindex[2] = bucketsize[0] + bucketsize[1];
	for (size_t i=0; i < N; ++i) {
		__builtin_prefetch(&oracle[i+1]);
		sorted[bucketindex[oracle[i]]++] = strings[i];
	}
	std::copy(sorted, sorted+N, strings);
	multikey_simd_b<CharT>(strings, bucketsize[0], depth,
			sorted, oracle);
	if (not is_end(partval))
		multikey_simd_b<CharT>(strings+bucketsize[0],
				bucketsize[1], depth+sizeof(CharT),
				sorted, oracle);
	multikey_simd_b<CharT>(strings+bucketsize[0]+bucketsize[1],
			bucketsize[2], depth, sorted, oracle);
}

void multikey_simd_b_1(unsigned char** strings, size_t n)
{
	unsigned char** sorted =
		static_cast<unsigned char**>(malloc(n*sizeof(unsigned char*)));
	uint8_t* const restrict oracle =
		static_cast<uint8_t*>(_mm_malloc(n, 16));
	multikey_simd_b<unsigned char>(strings, n, 0, sorted, oracle);
	_mm_free(oracle);
	free(sorted);
}

void multikey_simd_b_2(unsigned char** strings, size_t n)
{
	unsigned char** sorted =
		static_cast<unsigned char**>(malloc(n*sizeof(unsigned char*)));
	uint8_t* const restrict oracle =
		static_cast<uint8_t*>(_mm_malloc(n, 16));
	multikey_simd_b<uint16_t>(strings, n, 0, sorted, oracle);
	_mm_free(oracle);
	free(sorted);
}

void multikey_simd_b_4(unsigned char** strings, size_t n)
{
	unsigned char** sorted =
		static_cast<unsigned char**>(malloc(n*sizeof(unsigned char*)));
	uint8_t* const restrict oracle =
		static_cast<uint8_t*>(_mm_malloc(n, 16));
	multikey_simd_b<uint32_t>(strings, n, 0, sorted, oracle);
	_mm_free(oracle);
	free(sorted);
}

ROUTINE_REGISTER_SINGLECORE(multikey_simd_b_1,
		"multikey_simd with 1byte alphabet + prealloc + prefetch")
ROUTINE_REGISTER_SINGLECORE(multikey_simd_b_2,
		"multikey_simd with 2byte alphabet + prealloc + prefetch")
ROUTINE_REGISTER_SINGLECORE(multikey_simd_b_4,
		"multikey_simd with 4byte alphabet + prealloc + prefetch")

template <typename CharT>
static void
multikey_simd_parallel(unsigned char** strings, size_t N, size_t depth)
{
	if (N < 32) {
		insertion_sort(strings, N, depth);
		return;
	}
	CharT partval = pseudo_median<CharT>(strings, N, depth);
	uint8_t* const restrict oracle =
		static_cast<uint8_t*>(_mm_malloc(N, 16));
	std::array<size_t, 3> bucketsize;
	bucketsize.fill(0);
	size_t i=N-N%32;
	if (N > 0x100000) {
#pragma omp parallel sections
		{
#pragma omp section
		calculate_bucketsizes_sse<false>(strings, i/2, oracle,
				partval, depth);
#pragma omp section
		calculate_bucketsizes_sse<false>(strings+i/2, i/2, oracle+i/2,
				partval, depth);
		}
	} else
		calculate_bucketsizes_sse<false>(strings, i, oracle, partval, depth);
	for (; i < N; ++i)
		oracle[i] = get_bucket(
				get_char<CharT>(strings[i], depth),
				partval);
	for (i=0; i < N; ++i)
		++bucketsize[oracle[i]];
	assert(bucketsize[0] + bucketsize[1] + bucketsize[2] == N);
	unsigned char** sorted =
		static_cast<unsigned char**>(malloc(N*sizeof(unsigned char*)));
	size_t bucketindex[3];
	bucketindex[0] = 0;
	bucketindex[1] = bucketsize[0];
	bucketindex[2] = bucketsize[0] + bucketsize[1];
	for (size_t i=0; i < N; ++i) {
		sorted[bucketindex[oracle[i]]++] = strings[i];
	}
	std::copy(sorted, sorted+N, strings);
	free(sorted);
	_mm_free(oracle);
#pragma omp parallel sections
	{
#pragma omp section
	multikey_simd_parallel<CharT>(strings, bucketsize[0], depth);
#pragma omp section
	if (not is_end(partval))
		multikey_simd_parallel<CharT>(strings+bucketsize[0],
				bucketsize[1], depth+sizeof(CharT));
#pragma omp section
	multikey_simd_parallel<CharT>(strings+bucketsize[0]+bucketsize[1],
			bucketsize[2], depth);
	}
}

void multikey_simd_parallel1(unsigned char** strings, size_t n)
{ multikey_simd_parallel<unsigned char>(strings, n, 0); }

void multikey_simd_parallel2(unsigned char** strings, size_t n)
{ multikey_simd_parallel<uint16_t>(strings, n, 0); }

void multikey_simd_parallel4(unsigned char** strings, size_t n)
{ multikey_simd_parallel<uint32_t>(strings, n, 0); }

ROUTINE_REGISTER_MULTICORE(multikey_simd_parallel1,
		"parallel multikey_simd with 1byte alphabet")
ROUTINE_REGISTER_MULTICORE(multikey_simd_parallel2,
		"parallel multikey_simd with 2byte alphabet")
ROUTINE_REGISTER_MULTICORE(multikey_simd_parallel4,
		"parallel multikey_simd with 4byte alphabet")
