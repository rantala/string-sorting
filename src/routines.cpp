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

#include "routines.h"

#include <cstdlib>
#include <cstring>

namespace {

constexpr unsigned ROUTINES_MAX = 256;

const routine *routines[ROUTINES_MAX];
unsigned routine_cnt;

int
routine_cmp(const void *a, const void *b)
{
	const routine *aa = *static_cast<const routine *const *>(a);
	const routine *bb = *static_cast<const routine *const *>(b);
	if (aa->f == bb->f)
		return 0;
	if (aa->multicore < bb->multicore)
		return -1;
	if (aa->multicore > bb->multicore)
		return 1;
	return std::strcmp(aa->name, bb->name);
}

} // namespace

extern "C" void
routine_register(const routine *r)
{
	if (!r)
		std::abort();
	if (!r->name)
		std::abort();
	if (!r->desc)
		std::abort();
	if (routine_cnt >= ROUTINES_MAX)
		std::abort();
	routines[routine_cnt++] = r;
}

const routine *
routine_from_name(const char *name)
{
	for (unsigned i = 0; i < routine_cnt; ++i)
		if (std::strcmp(name, routines[i]->name) == 0)
			return routines[i];
	return nullptr;
}

void
routine_get_all(const routine ***r, unsigned *cnt)
{
	*r = routines;
	*cnt = routine_cnt;
	std::qsort(*r, *cnt, sizeof(routine *), routine_cmp);
}
