/*
 * Copyright 2011 by Tommi Rantala <tt.rantala@gmail.com>
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

#ifndef ROUTINE_H
#define ROUTINE_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct routine {
	void (*f)(unsigned char **, size_t);
	const char *name;
	const char *desc;
	unsigned multicore : 1;
};

void routine_register(const struct routine *);

#define ROUTINE_REGISTER(_func, _desc, _multicore) \
	static const struct routine _func##_routine = {    \
	        _func,                                     \
	        #_func,                                    \
	        _desc,                                     \
	        _multicore,                                \
	};                                                 \
	static void _func##_register_hook(void)            \
	        __attribute__((constructor));              \
	static void _func##_register_hook(void)            \
	{                                                  \
	        routine_register(&_func##_routine);        \
	}

#define ROUTINE_REGISTER_SINGLECORE(_func, _desc) \
	ROUTINE_REGISTER(_func, _desc, 0)

#define ROUTINE_REGISTER_MULTICORE(_func, _desc) \
	ROUTINE_REGISTER(_func, _desc, 1)

#ifdef __cplusplus
}
#endif

#endif /* ROUTINE_H */
