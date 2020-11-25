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

#include "routine.h"
#include <string.h>

#define ROUTINES_MAX 256

static const struct routine *routines[ROUTINES_MAX];
static unsigned routine_cnt;

void
routine_register(const struct routine *r)
{
	if (!r)
		abort();
	if (!r->name)
		abort();
	if (!r->desc)
		abort();
	if (routine_cnt >= ROUTINES_MAX)
		abort();
	routines[routine_cnt++] = r;
}

const struct routine *
routine_from_name(const char *name)
{
	unsigned i;
	for (i=0; i < routine_cnt; ++i)
		if (strcmp(name, routines[i]->name) == 0)
			return routines[i];
	return NULL;
}

static int
routine_cmp(const void *a, const void *b)
{
	const struct routine *aa = *(const struct routine **)a;
	const struct routine *bb = *(const struct routine **)b;
	if (aa->f == bb->f)
		return 0;
	if (aa->multicore < bb->multicore)
		return -1;
	if (aa->multicore > bb->multicore)
		return 1;
	return strcmp(aa->name, bb->name);
}

void
routine_get_all(const struct routine ***r, unsigned *cnt)
{
	*r = routines;
	*cnt = routine_cnt;
	qsort(*r, *cnt, sizeof(struct routine *), routine_cmp);
}
