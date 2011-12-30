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

#define _GNU_SOURCE
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

static struct timespec process_cputime_start;
static struct timespec process_cputime_stop;
static struct timespec monotonic_start;
static struct timespec monotonic_stop;
static struct rusage startclock;
static struct rusage stopclock;

void timing_start(void)
{
	getrusage(RUSAGE_SELF, &startclock);
	clock_gettime(CLOCK_MONOTONIC, &monotonic_start);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &process_cputime_start);
}

void timing_stop(void)
{
	getrusage(RUSAGE_SELF, &stopclock);
	clock_gettime(CLOCK_MONOTONIC, &monotonic_stop);
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &process_cputime_stop);
}

double gettime_wall_clock(void)
{
	double msecs_1 = monotonic_start.tv_nsec/1000000 + 1000*monotonic_start.tv_sec;
	double msecs_2 = monotonic_stop.tv_nsec/1000000 + 1000*monotonic_stop.tv_sec;
	return msecs_2 - msecs_1;
}

double gettime_user(void)
{
	struct timeval result;
	timersub(&stopclock.ru_utime, &startclock.ru_utime, &result);
	return (double)(result.tv_sec*1000)+(double)(result.tv_usec)/1e3;
}

double gettime_sys(void)
{
	struct timeval result;
	timersub(&stopclock.ru_stime, &startclock.ru_stime, &result);
	return (double)(result.tv_sec*1000)+(double)(result.tv_usec)/1e3;
}

double gettime_user_sys(void)
{
	struct timeval result_user;
	struct timeval result_sys;
	struct timeval result;
	timersub(&stopclock.ru_utime, &startclock.ru_utime, &result_user);
	timersub(&stopclock.ru_stime, &startclock.ru_stime, &result_sys);
	timeradd(&result_user, &result_sys, &result);
	return (double)(result.tv_sec*1000)+(double)(result.tv_usec)/1e3;
}

double gettime_process_cputime(void)
{
	double msecs_1 = process_cputime_start.tv_nsec/1000000 + 1000*process_cputime_start.tv_sec;
	double msecs_2 = process_cputime_stop.tv_nsec/1000000 + 1000*process_cputime_stop.tv_sec;
	return msecs_2 - msecs_1;
}
