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

#include "timing.h"

#include <ctime>
#include <sys/resource.h>
#include <sys/time.h>

namespace {

timespec process_cputime_start;
timespec process_cputime_stop;
timespec monotonic_start;
timespec monotonic_stop;
rusage   startclock;
rusage   stopclock;

double
ms_between(const timespec &a, const timespec &b)
{
	return (b.tv_nsec / 1000000.0 + 1000.0 * b.tv_sec)
	     - (a.tv_nsec / 1000000.0 + 1000.0 * a.tv_sec);
}

double
ms_between(const timeval &a, const timeval &b)
{
	timeval d;
	timersub(&b, &a, &d);
	return d.tv_sec * 1000.0 + d.tv_usec / 1000.0;
}

} // namespace

void
timing_start(void)
{
	getrusage(RUSAGE_SELF, &startclock);
	clock_gettime(CLOCK_MONOTONIC,            &monotonic_start);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID,   &process_cputime_start);
}

void
timing_stop(void)
{
	getrusage(RUSAGE_SELF, &stopclock);
	clock_gettime(CLOCK_MONOTONIC,            &monotonic_stop);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID,   &process_cputime_stop);
}

double
gettime_wall_clock(void)
{
	return ms_between(monotonic_start, monotonic_stop);
}

double
gettime_user(void)
{
	return ms_between(startclock.ru_utime, stopclock.ru_utime);
}

double
gettime_sys(void)
{
	return ms_between(startclock.ru_stime, stopclock.ru_stime);
}

double
gettime_user_sys(void)
{
	return gettime_user() + gettime_sys();
}

double
gettime_process_cputime(void)
{
	return ms_between(process_cputime_start, process_cputime_stop);
}
