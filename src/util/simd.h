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

#ifndef SIMD_H
#define SIMD_H

#include <emmintrin.h>
#include <xmmintrin.h>

// A wrapper for the __m128i type. If __m128i is typedef'd to some integral
// type, we cannot give our own nice operator overloads.

template <typename CharT>
struct M128i
{
	M128i(__m128i r) : reg(r) {}
	operator __m128i() { return reg; }
	__m128i reg;
	M128i(__m128i* addr) : reg(_mm_load_si128(addr)) {}
};

struct NullType {};
struct NullOp   {};

template <typename CharT> static __attribute__((always_inline))
M128i<CharT> operator & (M128i<CharT> lhs, M128i<CharT> rhs)
{ return _mm_and_si128(lhs, rhs); }

template <typename CharT> static __attribute__((always_inline))
M128i<CharT> operator & (__m128i lhs, M128i<CharT> rhs)
{ return M128i<CharT>(lhs) & rhs; }

template <typename CharT> static __attribute__((always_inline))
M128i<CharT> operator | (M128i<CharT> lhs, M128i<CharT> rhs)
{ return _mm_or_si128(lhs, rhs); }

template <typename CharT> static __attribute__((always_inline))
M128i<CharT> operator | (__m128i lhs, M128i<CharT> rhs)
{ return _mm_or_si128(lhs, rhs); }

static __attribute__((always_inline)) M128i<unsigned char>
operator + (M128i<unsigned char> lhs, M128i<unsigned char> rhs)
{ return _mm_add_epi8(lhs, rhs); }

static __attribute__((always_inline)) M128i<uint16_t>
operator + (M128i<uint16_t> lhs, M128i<uint16_t> rhs)
{ return _mm_add_epi16(lhs, rhs); }

static __attribute__((always_inline)) M128i<uint32_t>
operator + (M128i<uint32_t> lhs, M128i<uint32_t> rhs)
{ return _mm_add_epi32(lhs, rhs); }




/*
static inline
M128i<unsigned char> operator << (M128i<unsigned char> lhs, int t)
{ return _mm_slli_epi64(lhs, 4); }
*/



template <typename CharT>
static __attribute__((always_inline)) M128i<CharT>
maskify(CharT c)
{
	if      (sizeof(CharT) == 1) return _mm_set1_epi8(c);
	else if (sizeof(CharT) == 2) return _mm_set1_epi16(c);
	else if (sizeof(CharT) == 4) return _mm_set1_epi32(c);
	else exit(1);
}


template <typename CharT>
struct less
{
	__m128i compare_to;
	less(__m128i r) : compare_to(r) {}
	M128i<CharT>
	operator()(M128i<CharT> arg) const
	{
		if      (sizeof(CharT) == 1) return M128i<CharT>(_mm_cmplt_epi8(arg, compare_to));
		else if (sizeof(CharT) == 2) return M128i<CharT>(_mm_cmplt_epi16(arg, compare_to));
		else if (sizeof(CharT) == 4) return M128i<CharT>(_mm_cmplt_epi32(arg, compare_to));
		else exit(1);
	}
};

template <typename CharT>
struct equal
{
	__m128i compare_to;
	equal(__m128i r) : compare_to(r) {}
	M128i<CharT>
	operator()(M128i<CharT> arg) const
	{
		if      (sizeof(CharT) == 1) return M128i<CharT>(_mm_cmpeq_epi8(arg, compare_to));
		else if (sizeof(CharT) == 2) return M128i<CharT>(_mm_cmpeq_epi16(arg, compare_to));
		else if (sizeof(CharT) == 4) return M128i<CharT>(_mm_cmpeq_epi32(arg, compare_to));
		else exit(1);
	}
};

template <typename CharT>
struct greater
{
	__m128i compare_to;
	greater(__m128i r) : compare_to(r) {}

	M128i<CharT>
	operator()(M128i<CharT> arg) const
	{
		if      (sizeof(CharT) == 1) return M128i<CharT>(_mm_cmpgt_epi8(arg, compare_to));
		else if (sizeof(CharT) == 2) return M128i<CharT>(_mm_cmpgt_epi16(arg, compare_to));
		else if (sizeof(CharT) == 4) return M128i<CharT>(_mm_cmpgt_epi32(arg, compare_to));
		else exit(1);
	}
};

#endif //SIMD_H
