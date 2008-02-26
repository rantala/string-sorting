/*
 *   Copyright 2007-2008 by Tommi Rantala <tommi.rantala@cs.helsinki.fi>
 *
 *   *************************************************************************
 *   * Use, modification, and distribution are subject to the Boost Software *
 *   * License, Version 1.0. (See accompanying file LICENSE or a copy at     *
 *   * http://www.boost.org/LICENSE_1_0.txt)                                 *
 *   *************************************************************************
 */

/*
 * Very simple functions used for timing the algorithms. Calculates actual CPU
 * time, not wall clock time, because our algorithms use only one processor.
 */

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

static struct rusage startclock, stopclock;

void clockon()
{
	getrusage(RUSAGE_SELF, &startclock);
}

void clockoff()
{
	getrusage(RUSAGE_SELF, &stopclock);
}

double gettime()
{
	struct timeval result_user;
	struct timeval result_sys;
	struct timeval result;
	timersub(&stopclock.ru_utime, &startclock.ru_utime, &result_user);
	timersub(&stopclock.ru_stime, &startclock.ru_stime, &result_sys);
	timeradd(&result_user, &result_sys, &result);
	return (double)(result.tv_sec)+(double)(result.tv_usec)/1e6;
}
