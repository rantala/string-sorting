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

#ifndef UTIL_H
#define UTIL_H

#include <inttypes.h>
#include <boost/array.hpp>
#include <boost/static_assert.hpp>
#include <cassert>
#include <string>
#include <sstream>
#include <ctype.h>
#include <cstring>

#define static_assert BOOST_STATIC_ASSERT

void mkqsort(unsigned char **, int n, int depth);

#ifdef NDEBUG
#define debug() if (1) {} else std::cerr
#else
#define debug() std::cerr
#endif

static inline int
get_bucket(uint64_t c, uint64_t pivot)
{
	if (c < pivot)       return 0;
	else if (c == pivot) return 1;
	else                 return 2;
}

template <typename CharT>
static inline int
get_bucket(CharT c, CharT pivot)
{
	return ((c > pivot) << 1) | (c == pivot);
}

template <typename CharT>
static inline CharT
get_char(unsigned char* ptr, size_t depth)
{
	assert(ptr);
	if (sizeof(CharT) == 1) {
		return ptr[depth];
	}
	else if (sizeof(CharT) == 2) {
		if (ptr[depth] == 0) return 0;
		else                 return (ptr[depth] << 8) | ptr[depth+1];
	}
	else if (sizeof(CharT) == 4) {
		CharT c = 0;

		if (ptr[depth] == 0) return c;
		c = (ptr[depth] << 24);
		++ptr;

		if (ptr[depth] == 0) return c;
		c |= (ptr[depth] << 16);
		++ptr;

		if (ptr[depth] == 0) return c;
		c |= (ptr[depth] << 8);
		++ptr;

		c |= ptr[depth];

		return c;
	}
	else if (sizeof(CharT) == 8) {
		CharT c = 0;

		if (ptr[depth] == 0) return c;
		c = (uint64_t(ptr[depth]) << 56); ++ptr;

		if (ptr[depth] == 0) return c;
		c |= (uint64_t(ptr[depth]) << 48); ++ptr;

		if (ptr[depth] == 0) return c;
		c |= (uint64_t(ptr[depth]) << 40); ++ptr;

		if (ptr[depth] == 0) return c;
		c |= (uint64_t(ptr[depth]) << 32); ++ptr;

		if (ptr[depth] == 0) return c;
		c |= (ptr[depth] << 24); ++ptr;

		if (ptr[depth] == 0) return c;
		c |= (ptr[depth] << 16); ++ptr;

		if (ptr[depth] == 0) return c;
		c |= (ptr[depth] << 8); ++ptr;

		c |= ptr[depth];

		return c;
	}
	else {
		assert(0);
		exit(1);
	}
}

template <typename CharT, int depth>
static inline CharT
get_char(unsigned char* ptr)
{
	if (sizeof(CharT) == 1) {
		return ptr[depth];
	}
	else if (sizeof(CharT) == 2) {
		if (ptr[depth] == 0) return 0;
		else                 return (ptr[depth] << 8) | ptr[depth+1];
	}
	else if (sizeof(CharT) == 4) {
		CharT c = 0;

		if (ptr[depth] == 0) return c;
		c = (ptr[depth] << 24);
		++ptr;

		if (ptr[depth] == 0) return c;
		c |= (ptr[depth] << 16);
		++ptr;

		if (ptr[depth] == 0) return c;
		c |= (ptr[depth] << 8);
		++ptr;

		c |= ptr[depth];

		return c;
	}
	else if (sizeof(CharT) == 8) {
		CharT c = 0;

		if (ptr[depth] == 0) return c;
		c = (uint64_t(ptr[depth]) << 56); ++ptr;

		if (ptr[depth] == 0) return c;
		c |= (uint64_t(ptr[depth]) << 48); ++ptr;

		if (ptr[depth] == 0) return c;
		c |= (uint64_t(ptr[depth]) << 40); ++ptr;

		if (ptr[depth] == 0) return c;
		c |= (uint64_t(ptr[depth]) << 32); ++ptr;

		if (ptr[depth] == 0) return c;
		c |= (ptr[depth] << 24); ++ptr;

		if (ptr[depth] == 0) return c;
		c |= (ptr[depth] << 16); ++ptr;

		if (ptr[depth] == 0) return c;
		c |= (ptr[depth] << 8); ++ptr;

		c |= ptr[depth];

		return c;
	}
	else {
		assert(0);
		exit(1);
	}
}

template <typename CharT>
static inline bool
is_end(CharT c)
{
	if (sizeof(CharT) == 1) {
		return c == 0;
	}
	else if (sizeof(CharT) == 2 or sizeof(CharT) == 4 or sizeof(CharT) == 8) {
		return (c & 0xFF) == 0;
	}
	else {
		assert(0);
		exit(1);
	}
}

template <typename CharT>
static CharT
med3char(CharT a, CharT b, CharT c)
{
	if (a == b)           return a;
	if (c == a || c == b) return c;

	if (a < b) {
		if (b < c) return b;
		if (a < c) return c;
		return a;
	}

	if (b > c) return b;
	if (a < c) return a;
	return c;
}

template <typename CharT, typename Cmp>
static CharT&
med3char(CharT& a, CharT& b, CharT& c, Cmp cmp)
{
	if (cmp(a, b) == 0)           return a;
	if (cmp(c, a) == 0 or cmp(c, b) == 0) return c;

	if (cmp(a, b) < 0) {
		if (cmp(b, c) < 0) return b;
		if (cmp(a, c) < 0) return c;
		return a;
	}

	if (cmp(b, c) > 0) return b;
	if (cmp(a, c) < 0) return a;
	return c;
}

template <typename CharT>
static int
lcp(CharT a, CharT b)
{
	static_assert(sizeof(CharT) <= 4);
	if (sizeof(CharT) == 1) {
		return (a != 0) and (a == b);
	}
	else if (sizeof(CharT) == 2) {
		int A, B;

		A = 0xFF00 & a;
		if (A==0) return 0;
		B = 0xFF00 & b;
		if (A!=B) return 0;

		A = 0x00FF & a;
		if (A==0) return 1;
		B = 0x00FF & b;
		if (A!=B) return 1;

		return 2;
	}
	else if (sizeof(CharT) == 4) {
		int A, B;

		A = 0xFF000000 & a;
		B = 0xFF000000 & b;
		if (A==0 or A!=B) return 0;

		A = 0x00FF0000 & a;
		B = 0x00FF0000 & b;
		if (A==0 or A!=B) return 1;

		A = 0x0000FF00 & a;
		B = 0x0000FF00 & b;
		if (A==0 or A!=B) return 2;

		A = 0x000000FF & a;
		B = 0x000000FF & b;
		if (A==0 or A!=B) return 3;

		return 4;
	}
	exit(1);
}

template <typename CharT>
static std::string
to_str(CharT c)
{
	std::ostringstream strm;

	typedef unsigned char uchar;

	if (sizeof(CharT) == 1) {
		strm << c;
	}
	else if (sizeof(CharT) == 2) {
		uchar a = (0xFF00 & c) >> 8;
		uchar b = 0x00FF & c;

		if (not isprint(a)) a = '?';
		if (not isprint(b)) b = '?';

		strm << a << b;
	}
	else if (sizeof(CharT) == 4) {
		std::string s;

		s += to_str<uint16_t>((0xFFFF0000 & c) >> 16);
		s += to_str<uint16_t>(0x0000FFFF & c);

		return s;
	}
	else {
		assert(0);
		exit(1);
	}
	return strm.str();
}

template <typename CharT>
CharT
pseudo_median(unsigned char** strings, const size_t N)
{
	return med3char(
			med3char(
				get_char<CharT, 0>(strings[0]),
				get_char<CharT, 0>(strings[1]),
				get_char<CharT, 0>(strings[2])
				),
			med3char(
				get_char<CharT, 0>(strings[N/2  ]),
				get_char<CharT, 0>(strings[N/2+1]),
				get_char<CharT, 0>(strings[N/2+2])
				),
			med3char(
				get_char<CharT, 0>(strings[N-3]),
				get_char<CharT, 0>(strings[N-2]),
				get_char<CharT, 0>(strings[N-1])
				)
		       );
}

template <typename CharT>
CharT
pseudo_median(unsigned char** strings, const size_t N, const size_t depth)
{
	if (N > 30)
		return med3char(
			med3char(
				get_char<CharT>(strings[0], depth),
				get_char<CharT>(strings[1], depth),
				get_char<CharT>(strings[2], depth)
				),
			med3char(
				get_char<CharT>(strings[N/2  ], depth),
				get_char<CharT>(strings[N/2+1], depth),
				get_char<CharT>(strings[N/2+2], depth)
				),
			med3char(
				get_char<CharT>(strings[N-3], depth),
				get_char<CharT>(strings[N-2], depth),
				get_char<CharT>(strings[N-1], depth)
				)
		       );
	else
		return med3char(get_char<CharT>(strings[0  ], depth),
				get_char<CharT>(strings[N/2], depth),
				get_char<CharT>(strings[N-1], depth));
}

static inline void
insertion_sort(unsigned char** strings, int n, const size_t depth)
{
	for (unsigned char** i = strings + 1; --n > 0; ++i) {
		unsigned char** j = i;
		unsigned char* tmp = *i;

		while (j > strings) {
			unsigned char* s = *(j-1)+depth;
			unsigned char* t = tmp+depth;

			while (*s == *t and not is_end(*s)) {
				++s;
				++t;
			}

			if (*s <= *t) break;

			*j = *(j-1);
			--j;
		}

		*j = tmp;
	}
}

#endif //UTIL_H
