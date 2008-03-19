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

#ifndef GET_CHAR_H
#define GET_CHAR_H

#include <cassert>
#include <inttypes.h>

template <typename CharT>
static inline CharT
get_char(unsigned char* ptr, size_t depth);

template <>
static inline unsigned char
get_char(unsigned char* ptr, size_t depth)
{
	assert(ptr);
	return ptr[depth];
}

template <>
static inline uint16_t
get_char(unsigned char* ptr, size_t depth)
{
	assert(ptr);
	if (ptr[depth] == 0) return 0;
	else return (ptr[depth] << 8) | ptr[depth+1];
}

template <>
static inline uint32_t
get_char(unsigned char* ptr, size_t depth)
{
	assert(ptr);
	uint32_t c = 0;
	ptr += depth;
	if (*ptr == 0) return c;
	c  = (uint32_t(*ptr++) << 24);
	if (*ptr == 0) return c;
	c |= (uint32_t(*ptr++) << 16);
	if (*ptr == 0) return c;
	c |= (uint32_t(*ptr++) << 8 );
	return c | *ptr;
}

template <>
static inline uint64_t
get_char(unsigned char* ptr, size_t depth)
{
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
is_end(CharT c);

template <>
static inline bool
is_end(unsigned char c)
{
	return c==0;
}

template <>
static inline bool
is_end(uint16_t c)
{
	return (c&0xFF)==0;
}

template <>
static inline bool
is_end(uint32_t c)
{
	return (c&0xFF)==0;
}

template <>
static inline bool
is_end(uint64_t c)
{
	return (c&0xFF)==0;
}

#endif //GET_CHAR_H
