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

#include "get_char.h"

template <typename CharT>
CharT
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
CharT&
med3char(CharT& a, CharT& b, CharT& c, Cmp cmp)
{
	if (cmp(a, b) == 0)                   return a;
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
inline CharT
pseudo_median(CharT* begin, CharT* end)
{
	size_t N=end-begin;
	assert(N>3);
	return med3char(
			med3char(begin[0],   begin[1],     begin[2]),
			med3char(begin[N/2], begin[N/2+1], begin[N/2+2]),
			med3char(begin[N-3], begin[N-2],   begin[N-1])
		       );
}

template <typename CharT>
CharT
pseudo_median(unsigned char** strings, size_t N, size_t depth)
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

#endif //UTIL_H
