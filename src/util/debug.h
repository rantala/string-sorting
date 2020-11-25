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

#ifndef UTIL_DEBUG_H
#define UTIL_DEBUG_H

#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
#include <iostream>
#if defined(NDEBUG) || defined(UNIT_TEST)
#define debug_indent
#define debug() if (1) {} else std::cerr
#else
static std::string __debug_indent_str;
#define debug_indent struct DI{std::string&i;DI(std::string&_i):i(_i){i+="    ";}~DI(){i=i.substr(0,i.size()-4);}}__d(__debug_indent_str);
#define debug() std::cerr << __debug_indent_str
#endif
#endif /* __cplusplus */

static inline int
check_result(unsigned char **strings, size_t n)
{
	size_t wrong = 0;
	size_t identical = 0;
	size_t invalid = 0;
	for (size_t i=0; i < n-1; ++i) {
		if (strings[i] == strings[i+1])
			++identical;
		if (strings[i]==NULL || strings[i+1]==NULL)
			++invalid;
		else if (strcmp((char*)strings[i], (char*)strings[i+1]) > 0)
			++wrong;
	}
	if (identical)
		fprintf(stderr,
			"WARNING: found %zu identical pointers!\n",
			identical);
	if (wrong)
		fprintf(stderr,
			"WARNING: found %zu incorrect orderings!\n",
			wrong);
	if (invalid)
		fprintf(stderr,
			"WARNING: found %zu invalid pointers!\n",
			invalid);
	if (identical || wrong || invalid)
		return 1;
	return 0;
}

#endif //UTIL_DEBUG_H
