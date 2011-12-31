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
 * Implement the Multi-Key-Quicksort using multiple pivots in a single step. We
 * essentially combine multiple steps of the original MKQ algorithm into one
 * larger step. This is useful, because it reduces the sweeps we have to make
 * over the input, thus reducing cache misses etc.
 *
 * Using multiple pivots is also somewhat more difficult compared to using just
 * a single pivot, meaning that we'll need to use some extra space for
 * efficient execution. This variant uses SIMD methods to brute force calculate
 * the correct bucket for each input string.
 *
 * See also:
 *   @article{sanders04super,
 *      author = "P. Sanders and S. Winkel",
 *      title = "Super scalar sample sort",
 *      text = "P. Sanders and S. Winkel. Super scalar sample sort. In 12th
 *              Annual European Symposium on Algorithms, ESA 2004.",
 *      year = "2004",
 *      url = "citeseer.ist.psu.edu/sanders04super.html"
 *   }
 *
 * We use the same idea, i.e. take the values from comparisons as integers, and
 * the sort the strings using counting sort.
 */

/* TODO: We use one byte per oracle value for simplicity, whereas with small
 * number of pivots we could use smaller number of bits. This could save some
 * memory especially with large inputs.
 */

#include "routine.h"
#include "util/insertion_sort.h"
#include "util/get_char.h"
#include "util/median.h"
#include <inttypes.h>
#include <cassert>
#include <cstring>
#include <set>
#include <vector>
#include <boost/array.hpp>
#include <boost/static_assert.hpp>
#include <xmmintrin.h>
#include <emmintrin.h>

// These values are used with variables and they need to be proper constants
// (used also with templates), so we cannot use inline functions or structs
// etc. Maybe use 'constexpr' some day.
#define left_bucket(pivot)    (2*(pivot))
#define middle_bucket(pivot)  (2*(pivot)+1)
#define right_bucket(pivot)   (2*(pivot)+2)
#define total_buckets(pivots) (2*(pivots)+1)

// Calculates the LCP between two pivot (super)characters.
// No two pivots are equal.
static inline unsigned lcp(unsigned char a, unsigned char b)
{
	assert(a != b);
	(void)a;
	(void)b;
	return 0;
}
static inline unsigned lcp(uint16_t a, uint16_t b)
{
	assert(a != b);
	unsigned A, B;
	A = 0xFF00 & a;
	B = 0xFF00 & b;
	if (A==0 or A!=B) return 0;
	return 1;
}
static inline unsigned lcp(uint32_t a, uint32_t b)
{
	assert(a != b);
	unsigned A, B;
	A = 0xFF000000 & a;
	B = 0xFF000000 & b;
	if (A==0 or A!=B) return 0;
	A = 0x00FF0000 & a;
	B = 0x00FF0000 & b;
	if (A==0 or A!=B) return 1;
	A = 0x0000FF00 & a;
	B = 0x0000FF00 & b;
	if (A==0 or A!=B) return 2;
	return 3;
}

template <unsigned Pivots>
static void
fill_oracle_brute_force_sse(
                unsigned char** strings,
                size_t n,
                uint8_t* restrict oracle,
                const boost::array<unsigned char, Pivots>& pivots,
                size_t depth)
{
	assert(n % 16 == 0);
	BOOST_STATIC_ASSERT(Pivots > 0);
	BOOST_STATIC_ASSERT(total_buckets(Pivots) < 0x100);
	typedef unsigned char CharT;
	static const uint8_t _Constants[] __attribute__((aligned(16))) = {
		0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
		0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
	};
	const __m128i FlipBit = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+0));
	const __m128i Mask1   = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+16));
	// Prebuild an array of the pivots. Seems to be a bit faster than using
	// _mm_set1_epi8() over and over again in the loop.
	uint8_t _Pivots[16*Pivots] __attribute__((aligned(16)));
	for (unsigned i=0; i < Pivots; ++i) {
		const uint8_t p = pivots[i]+0x80;
		for (unsigned j=0; j < 16; ++j) {
			_Pivots[16*i+j] = p;
		}
	}
	for (size_t i=0; i < n; i += 16) {
		CharT cache[16] __attribute__ ((aligned(16)));
		for (unsigned j=0; j < 16; ++j) {
			cache[j] = get_char<CharT>(strings[i+j], depth);
		}
		__m128i result = _mm_setzero_si128();
		__m128i data = _mm_load_si128(
				reinterpret_cast<__m128i*>(cache+0));
		data = _mm_add_epi8(data, FlipBit);
		__m128i p1;
		__m128i p2 = _mm_load_si128(
				reinterpret_cast<__m128i*>(_Pivots+0));
		__m128i mask = Mask1;
		for (unsigned k=0; k < Pivots-1; ++k) {
			p1 = p2;
			p2 = _mm_load_si128(
				reinterpret_cast<__m128i*>(_Pivots+16*(k+1)));
			__m128i tmp1 = _mm_cmpeq_epi8(data, p1);
			tmp1 = _mm_and_si128(tmp1, mask);
			mask = _mm_add_epi8(mask, Mask1);
			result = _mm_or_si128(result, tmp1);
			__m128i tmp2 = _mm_and_si128(
					_mm_cmpgt_epi8(data, p1),
					_mm_cmplt_epi8(data, p2));
			tmp2 = _mm_and_si128(tmp2, mask);
			mask = _mm_add_epi8(mask, Mask1);
			result = _mm_or_si128(result, tmp2);
		}
		__m128i tmp1 = _mm_cmpeq_epi8(data, p2);
		tmp1 = _mm_and_si128(tmp1, mask);
		mask = _mm_add_epi8(mask, Mask1);
		result = _mm_or_si128(result, tmp1);
		__m128i tmp2 = _mm_cmpgt_epi8(data, p2);
		tmp2 = _mm_and_si128(tmp2, mask);
		result = _mm_or_si128(result, tmp2);
		_mm_store_si128(reinterpret_cast<__m128i*>(oracle+i), result);
	}
}

template <unsigned Pivots>
static void
fill_oracle_brute_force_sse(
                unsigned char** strings,
                size_t n,
                uint8_t* restrict oracle,
                const boost::array<uint16_t, Pivots>& pivots,
                size_t depth)
{
	assert(n % 16 == 0);
	BOOST_STATIC_ASSERT(Pivots > 0);
	BOOST_STATIC_ASSERT(total_buckets(Pivots) < 0x100);
	typedef uint16_t CharT;
	static const uint16_t _Constants[] __attribute__((aligned(16))) = {
		0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
		0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001
	};
	const __m128i FlipBit = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+0));
	const __m128i Mask1   = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+8));
	uint16_t _Pivots[8*Pivots] __attribute__((aligned(16)));
	for (unsigned i=0; i < Pivots; ++i) {
		const uint16_t p = pivots[i]+0x8000;
		for (unsigned j=0; j < 8; ++j) {
			_Pivots[8*i+j] = p;
		}
	}
	for (size_t i=0; i < n; i += 8) {
		uint16_t cache[8] __attribute__((aligned(16)));
		for (unsigned j=0; j<8; ++j)  {
			cache[j] = get_char<CharT>(strings[i+j], depth);
		}
		__m128i result = _mm_setzero_si128();
		__m128i data = _mm_load_si128(
				reinterpret_cast<__m128i*>(cache+0));
		data = _mm_add_epi16(data, FlipBit);
		__m128i p1;
		__m128i p2 = _mm_load_si128(
				reinterpret_cast<__m128i*>(_Pivots+0));
		__m128i mask = Mask1;
		for (unsigned k=0; k < Pivots-1; ++k) {
			p1 = p2;
			p2 = _mm_load_si128(
				reinterpret_cast<__m128i*>(_Pivots+8*(k+1)));
			__m128i tmp1 = _mm_cmpeq_epi16(data, p1);
			tmp1 = _mm_and_si128(tmp1, mask);
			mask = _mm_add_epi16(mask, Mask1);
			result = _mm_or_si128(result, tmp1);
			__m128i tmp2 = _mm_and_si128(
					_mm_cmpgt_epi16(data, p1),
					_mm_cmplt_epi16(data, p2));
			tmp2 = _mm_and_si128(tmp2, mask);
			mask = _mm_add_epi16(mask, Mask1);
			result = _mm_or_si128(result, tmp2);
		}
		__m128i tmp1 = _mm_cmpeq_epi16(data, p2);
		tmp1 = _mm_and_si128(tmp1, mask);
		mask = _mm_add_epi16(mask, Mask1);
		result = _mm_or_si128(result, tmp1);
		__m128i tmp2 = _mm_cmpgt_epi16(data, p2);
		tmp2 = _mm_and_si128(tmp2, mask);
		result = _mm_or_si128(result, tmp2);
		result = _mm_packus_epi16(result, _mm_setzero_si128());
		// NB. store address not 16-byte aligned.
		_mm_storel_epi64(reinterpret_cast<__m128i*>(oracle+i), result);
	}
}

// Extract a byte from the SSE register at given byte position.
template <unsigned pos>
static inline __attribute__((__always_inline__))
uint8_t extract8(__m128i a)
{
	BOOST_STATIC_ASSERT(pos%2==0);
	return uint8_t(_mm_extract_epi16(a, pos/2));
}

template <unsigned Pivots>
static void
fill_oracle_brute_force_sse(
                unsigned char** strings,
                size_t n,
                uint8_t* restrict oracle,
                const boost::array<uint32_t, Pivots>& pivots,
                size_t depth)
{
	assert(n % 16 == 0);
	BOOST_STATIC_ASSERT(Pivots > 0);
	BOOST_STATIC_ASSERT(total_buckets(Pivots) < 0x100);
	typedef uint32_t CharT;
	static const CharT _Constants[] __attribute__((aligned(16))) = {
		0x80000000, 0x80000000, 0x80000000, 0x80000000,
		0x00000001, 0x00000001, 0x00000001, 0x00000001
	};
	const __m128i FlipBit = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+0));
	const __m128i Mask1   = _mm_load_si128(
		reinterpret_cast<const __m128i*>(_Constants+4));
	CharT _Pivots[4*Pivots] __attribute__((aligned(16)));
	for (unsigned i=0; i < Pivots; ++i) {
		const CharT p = pivots[i]+0x80000000;
		for (unsigned j=0; j < 4; ++j) {
			_Pivots[4*i+j] = p;
		}
	}
	for (size_t i=0; i < n; i += 4) {
		CharT cache[4] __attribute__((aligned(16)));
		for (unsigned j=0; j<4; ++j)  {
			cache[j] = get_char<CharT>(strings[i+j], depth);
		}
		__m128i result = _mm_setzero_si128();
		__m128i data = _mm_load_si128(
				reinterpret_cast<__m128i*>(cache+0));
		data = _mm_add_epi32(data, FlipBit);
		__m128i p1;
		__m128i p2 = _mm_load_si128(
				reinterpret_cast<__m128i*>(_Pivots+0));
		__m128i mask = Mask1;
		for (unsigned k=0; k < Pivots-1; ++k) {
			p1 = p2;
			p2 = _mm_load_si128(
				reinterpret_cast<__m128i*>(_Pivots+4*(k+1)));
			__m128i tmp1 = _mm_cmpeq_epi32(data, p1);
			tmp1 = _mm_and_si128(tmp1, mask);
			mask = _mm_add_epi32(mask, Mask1);
			result = _mm_or_si128(result, tmp1);
			__m128i tmp2 = _mm_and_si128(
					_mm_cmpgt_epi32(data, p1),
					_mm_cmplt_epi32(data, p2));
			tmp2 = _mm_and_si128(tmp2, mask);
			mask = _mm_add_epi32(mask, Mask1);
			result = _mm_or_si128(result, tmp2);
		}
		__m128i tmp1 = _mm_cmpeq_epi32(data, p2);
		tmp1 = _mm_and_si128(tmp1, mask);
		mask = _mm_add_epi32(mask, Mask1);
		result = _mm_or_si128(result, tmp1);
		__m128i tmp2 = _mm_cmpgt_epi32(data, p2);
		tmp2 = _mm_and_si128(tmp2, mask);
		result = _mm_or_si128(result, tmp2);
		// NB. oracle+i is only 4-byte aligned.
		oracle[i+0] = extract8<0>(result);
		oracle[i+1] = extract8<4>(result);
		oracle[i+2] = extract8<8>(result);
		oracle[i+3] = extract8<12>(result);
	}
}

template <unsigned Pivots, typename CharT>
static void
fill_oracle(unsigned char** strings,
            size_t N,
            uint8_t* oracle,
            const boost::array<CharT, Pivots>& pivots,
            size_t depth)
{
	BOOST_STATIC_ASSERT(Pivots > 0);
	BOOST_STATIC_ASSERT(total_buckets(Pivots) < 0x100);
	size_t n = N-N%16;
	fill_oracle_brute_force_sse<Pivots>(strings, n, oracle, pivots, depth);
	for (size_t i=n; i < N; ++i) {
		const CharT c = get_char<CharT>(strings[i], depth);
		if (c < pivots[0]) {
			oracle[i] = left_bucket(0);
			goto pivoting_done;
		}
		for (unsigned j=0; j < Pivots-1; ++j) {
			if (c == pivots[j]) {
				oracle[i] = middle_bucket(j);
				goto pivoting_done;
			} else if (c > pivots[j] and c < pivots[j+1]) {
				oracle[i] = right_bucket(j);
				goto pivoting_done;
			}
		}
		if (c == pivots[Pivots-1]) {
			oracle[i] = middle_bucket(Pivots-1);
		} else {
			oracle[i] = right_bucket(Pivots-1);
		}
pivoting_done:
		;
	}
}

extern "C" void mkqsort(unsigned char**, int, int);

template <typename CharT, unsigned Pivots>
static void
multikey_multipivot(unsigned char** strings, size_t n, size_t depth)
{
	BOOST_STATIC_ASSERT(Pivots > 0);
	BOOST_STATIC_ASSERT(total_buckets(Pivots) < 0x100);
	if (n < 15000) {
		mkqsort(strings, n, depth);
		return;
	}
	// TODO: collect frequencies to gain knowledge about distribution
	std::set<CharT> sample;
	for (unsigned i=0; i < Pivots; ++i) {
		double r=drand48();
		size_t pos = ((size_t)((n-7)*r));
		assert(pos+6 < n);
		boost::array<CharT, 7> tmp;
		for (unsigned j=0;j<tmp.size();++j) {
			tmp[j] = get_char<CharT>(strings[pos+j], depth);
		}
		sample.insert(tmp.begin(), tmp.end());
	}
	// We _must_ select enough pivots -> insert junk.
	for (CharT i=1; sample.size() < Pivots; ++i) {
		if (is_end(i)) ++i;
		sample.insert(i);
	}
	// Pick pivots from the sample.
	std::vector<CharT> sample_array(sample.begin(), sample.end());
	sample.clear();
	boost::array<CharT, Pivots> pivots;
	unsigned step = sample_array.size() / Pivots;
	assert(step > 0);
	for (unsigned i=0; i < Pivots; ++i) {
		pivots[i] = sample_array[step*i];
	}
	uint8_t* restrict oracle = static_cast<uint8_t*>(_mm_malloc(n, 16));
	fill_oracle<Pivots>(strings, n, oracle, pivots, depth);
	boost::array<size_t, total_buckets(Pivots)> bucketsize;
	bucketsize.assign(0);
	uint8_t prev = oracle[0];
	bool sorted = true;
	size_t i=1;
	++bucketsize[prev];
	for (; i < n; ++i) {
		uint8_t bucket = oracle[i];
		if (prev > bucket) {
			sorted = false; break;
		}
		++bucketsize[bucket];
		prev = bucket;
	}
	for (; i < n; ++i)
		++bucketsize[oracle[i]];
	if (not sorted) {
		unsigned char** sorted = (unsigned char**)
			malloc(n*sizeof(unsigned char*));
		static boost::array<size_t, total_buckets(Pivots)> bucketindex;
		bucketindex[0] = 0;
		for (unsigned i=1; i < total_buckets(Pivots); ++i)
			bucketindex[i] = bucketindex[i-1] + bucketsize[i-1];
		for (size_t i=0; i < n; ++i)
			sorted[bucketindex[oracle[i]]++] = strings[i];
		memcpy(strings, sorted, n*sizeof(unsigned char*));
		free(sorted);
	}
	_mm_free(oracle);
	size_t b=0;
	size_t bsum = bucketsize[0];
	if (bsum) multikey_multipivot<CharT, Pivots>(strings, bsum, depth);
	for (unsigned i=0; i < Pivots-1; ++i) {
		b = bucketsize[middle_bucket(i)];
		if (b and not is_end(pivots[i])) {
			multikey_multipivot<CharT, Pivots>(strings+bsum, b,
					depth+sizeof(CharT));
		}
		bsum += b;
		if ((b = bucketsize[right_bucket(i)])) {
			multikey_multipivot<CharT, Pivots>(strings+bsum, b,
				depth+lcp(pivots[i], pivots[i+1]));
		}
		bsum += b;
	}
	if ((b = bucketsize[middle_bucket(Pivots-1)])) {
		multikey_multipivot<CharT, Pivots>(strings+bsum,
				b, depth+sizeof(CharT));
		bsum += b;
	}
	if ((b = bucketsize[right_bucket(Pivots-1)])) {
		multikey_multipivot<CharT, Pivots>(strings+bsum, b, depth);
	}
}

void multikey_multipivot_brute_simd1(unsigned char** strings, size_t n)
{ multikey_multipivot<unsigned char, 16>(strings, n, 0); }

void multikey_multipivot_brute_simd2(unsigned char** strings, size_t n)
{ multikey_multipivot<uint16_t, 32>(strings, n, 0); }

void multikey_multipivot_brute_simd4(unsigned char** strings, size_t n)
{ multikey_multipivot<uint32_t, 32>(strings, n, 0); }

ROUTINE_REGISTER_SINGLECORE(multikey_multipivot_brute_simd1,
		"multikey_multipivot_brute_simd with 1byte alphabet")
ROUTINE_REGISTER_SINGLECORE(multikey_multipivot_brute_simd2,
		"multikey_multipivot_brute_simd with 2byte alphabet")
ROUTINE_REGISTER_SINGLECORE(multikey_multipivot_brute_simd4,
		"multikey_multipivot_brute_simd with 4byte alphabet")
