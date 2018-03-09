/*
 * Copyright 2008,2011 by Tommi Rantala <tt.rantala@gmail.com>
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

/* Stable mergesort variants for sorting strings. We closely follow the
 * implementation described by Sanders:
 *
 * @article{384249,
 *     author = {Peter Sanders},
 *     title = {Fast priority queues for cached memory},
 *     journal = {J. Exp. Algorithmics},
 *     volume = {5},
 *     year = {2000},
 *     issn = {1084-6654},
 *     pages = {7},
 *     doi = {http://doi.acm.org/10.1145/351827.384249},
 *     publisher = {ACM},
 *     address = {New York, NY, USA},
 * }
 */

#include "routine.h"
#include "util/debug.h"
#include "util/insertion_sort.h"
#include <cstdlib>
#include <cassert>
#include <cstring>

static inline int
cmp(const unsigned char* a, const unsigned char* b)
{
	assert(a != 0);
	assert(b != 0);
	return strcmp(reinterpret_cast<const char*>(a),
	              reinterpret_cast<const char*>(b));
}

/*******************************************************************************
 *
 * mergesort_2way
 *
 ******************************************************************************/

static void
merge_2way(unsigned char** restrict from0, size_t n0,
           unsigned char** restrict from1, size_t n1,
           unsigned char** restrict result)
{
	debug()<<__func__<<"(), n0="<<n0<<", n1="<<n1<<"\n";
	unsigned char* key0 = *from0;
	unsigned char* key1 = *from1;
	assert(n0); assert(n1);
	assert(from0); assert(from1); assert(result);
	assert(key0); assert(key1);
	while (true) {
		if (cmp(key0, key1) <= 0) {
			*result++ = key0;
			if (--n0 == 0) goto finish0;
			++from0;
			key0 = *from0;
		} else {
			*result++ = key1;
			if (--n1 == 0) goto finish1;
			++from1;
			key1 = *from1;
		}
	}
finish0:
	assert(n0 == 0);
	assert(n1 > 0);
	debug()<<__func__<<"(), n0="<<n0<<", n1="<<n1<<"\n";
	(void) memcpy(result, from1, n1*sizeof(unsigned char*));
	return;
finish1:
	assert(n1 == 0);
	assert(n0 > 0);
	debug()<<__func__<<"(), n0="<<n0<<", n1="<<n1<<"\n";
	(void) memcpy(result, from0, n0*sizeof(unsigned char*));
	return;
}
static void
mergesort_2way(unsigned char** strings, size_t n, unsigned char** tmp)
{
	if (n < 32) {
		insertion_sort(strings, n, 0);
		return;
	}
	const size_t split0 = n/2;
	mergesort_2way(strings,        split0,   tmp);
	mergesort_2way(strings+split0, n-split0, tmp);
	merge_2way(strings, split0,
	           strings+split0, n-split0,
	           tmp);
	(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
}
void mergesort_2way(unsigned char** strings, size_t n)
{
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	mergesort_2way(strings, n, tmp);
	free(tmp);
}
ROUTINE_REGISTER_SINGLECORE(mergesort_2way, "mergesort_2way")

static void
mergesort_2way_parallel(unsigned char** strings, size_t n, unsigned char** tmp)
{
	if (n < 32) {
		insertion_sort(strings, n, 0);
		return;
	}
	const size_t split0 = n/2;
#pragma omp parallel sections
	{
#pragma omp section
		mergesort_2way_parallel(strings,        split0,   tmp);
#pragma omp section
		mergesort_2way_parallel(strings+split0, n-split0, tmp+split0);
	}
	merge_2way(strings, split0,
	           strings+split0, n-split0,
	           tmp);
	(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
}
void mergesort_2way_parallel(unsigned char** strings, size_t n)
{
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	mergesort_2way_parallel(strings, n, tmp);
	free(tmp);
}
ROUTINE_REGISTER_MULTICORE(mergesort_2way_parallel,
		"Parallel mergesort with 2way merger")

/*******************************************************************************
 *
 * mergesort_3way
 *
 ******************************************************************************/

static void
merge_3way(unsigned char** restrict from0, size_t n0,
           unsigned char** restrict from1, size_t n1,
           unsigned char** restrict from2, size_t n2,
           unsigned char** restrict result)
{
	debug()<<__func__<<"(), n0="<<n0<<", n1="<<n1<<", n2="<<n2<<"\n";
	unsigned char* key0 = *from0;
	unsigned char* key1 = *from1;
	unsigned char* key2 = *from2;
	const int cmp01 = cmp(key0, key1);
	const int cmp12 = cmp(key1, key2);
	int cmp02 = 0;
	if (cmp01 < 0) {
		if (cmp12 < 0)  { goto state0lt1lt2; }
		if (cmp12 == 0) { goto state0lt1eq2; }
		cmp02 = cmp(key0, key2);
		if (cmp02 <= 0) { goto state0lt2lt1; }
		goto state2lt0lt1;
	}
	if (cmp01 == 0) {
		if (cmp12 < 0)  { goto state0lt1lt2; }
		if (cmp12 == 0) { goto state0lt1eq2; }
		goto state2lt0eq1;
	}
	if (cmp12 > 0)  { goto state2lt1lt0; }
	if (cmp12 == 0) { goto state1lt2lt0; }
	cmp02 = cmp(key0, key2);
	if (cmp02 < 0)  { goto state1lt0lt2; }
	if (cmp02 == 0) { goto state1lt0eq2; }
	goto state1lt2lt0;

#define StrictCase(a,b,c)                                                      \
        state##a##lt##b##lt##c:                                                \
        {                                                                      \
                *result++ = key##a;                                            \
                if (--n##a == 0) goto finish##a;                               \
                ++from##a;                                                     \
                key##a = *from##a;                                             \
                const int cmpab = cmp(key##a, key##b);                         \
                if (cmpab < 0)  goto state##a##lt##b##lt##c;                   \
                if (cmpab == 0) goto state##a##eq##b##lt##c;                   \
                const int cmpac = cmp(key##a, key##c);                         \
                if (cmpac < 0)  goto state##b##lt##a##lt##c;                   \
                if (cmpac == 0) goto state##b##lt##a##eq##c;                   \
                goto state##b##lt##c##lt##a;                                   \
        }
        StrictCase(1, 0, 2)
        StrictCase(1, 2, 0)
        StrictCase(2, 0, 1)
        StrictCase(2, 1, 0)
#undef StrictCase

state0lt1lt2:
{
	*result++ = key0;
	if (--n0 == 0) goto finish0;
	++from0;
	key0 = *from0;
	const int cmp01 = cmp(key0, key1);
	if (cmp01 <= 0) goto state0lt1lt2;
	const int cmp02 = cmp(key0, key2);
	if (cmp02 < 0)  goto state1lt0lt2;
	if (cmp02 == 0) goto state1lt0eq2;
	goto state1lt2lt0;
}
state0lt2lt1:
{
	*result++ = key0;
	if (--n0 == 0) goto finish0;
	++from0;
	key0 = *from0;
	const int cmp02 = cmp(key0, key2);
	if (cmp02 <= 0) goto state0lt2lt1;
	const int cmp01 = cmp(key0, key1);
	if (cmp01 < 0)  goto state2lt0lt1;
	if (cmp01 == 0) goto state2lt0eq1;
	goto state2lt1lt0;
}

// No advantage from equality of first two strings.
state0eq1lt2: goto state0lt1lt2;
state0eq2lt1: goto state0lt2lt1;
state1eq2lt0: goto state1lt2lt0;

// Fix order.
state2eq0lt1: goto state0eq2lt1;
state1eq0lt2: goto state0eq1lt2;
state2eq1lt0: goto state1eq2lt0;

state0lt1eq2:
{
	*result++ = key0;
	if (--n0 == 0) goto finish0;
	++from0;
	key0 = *from0;
	const int cmp01 = cmp(*from0, *from1);
	if (cmp01 <= 0) goto state0lt1eq2;
	goto state1lt2lt0;
}
state1lt0eq2:
{
	*result++ = key1;
	if (--n1 == 0) goto finish1;
	++from1;
	key1 = *from1;
	const int cmp10 = cmp(key1, key0);
	if (cmp10 < 0)  goto state1lt0eq2;
	if (cmp10 == 0) goto state0lt1eq2;
	goto state0lt2lt1;
}
state2lt0eq1:
{
	*result++ = key2;
	if (--n2 == 0) goto finish2;
	++from2;
	key2 = *from2;
	const int cmp20 = cmp(key2, key0);
	if (cmp20 < 0)  goto state2lt0eq1;
	if (cmp20 == 0) goto state0lt1eq2;
	goto state0lt1lt2;
}
state0lt2eq1: goto state0lt1eq2;
state1lt2eq0: goto state1lt0eq2;
state2lt1eq0: goto state2lt0eq1;

finish0:
	assert(n0 == 0);
	merge_2way(from1, n1, from2, n2, result);
	return;
finish1:
	assert(n1 == 0);
	merge_2way(from0, n0, from2, n2, result);
	return;
finish2:
	assert(n2 == 0);
	merge_2way(from0, n0, from1, n1, result);
	return;
}
static void
mergesort_3way(unsigned char** strings, size_t n, unsigned char** tmp)
{
	debug() << __func__ << "(), n="<<n<<"\n";
	if (n < 32) {
		insertion_sort(strings, n, 0);
		return;
	}
	const size_t split0 = n/3, split1 = (2*n)/3;
	mergesort_3way(strings,        split0,        tmp);
	mergesort_3way(strings+split0, split1-split0, tmp);
	mergesort_3way(strings+split1, n-split1,      tmp);
	merge_3way(strings, split0,
	           strings+split0, split1-split0,
	           strings+split1, n-split1,
	           tmp);
	(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
}
void mergesort_3way(unsigned char** strings, size_t n)
{
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	mergesort_3way(strings, n, tmp);
	free(tmp);
}
ROUTINE_REGISTER_SINGLECORE(mergesort_3way, "mergesort_3way")

static void
mergesort_3way_parallel(unsigned char** strings, size_t n, unsigned char** tmp)
{
	debug() << __func__ << "(), n="<<n<<"\n";
	if (n < 32) {
		insertion_sort(strings, n, 0);
		return;
	}
	const size_t split0 = n/3, split1 = (2*n)/3;
#pragma omp parallel sections
	{
#pragma omp section
		mergesort_3way_parallel(strings,        split0,        tmp);
#pragma omp section
		mergesort_3way_parallel(strings+split0, split1-split0, tmp+split0);
#pragma omp section
		mergesort_3way_parallel(strings+split1, n-split1,      tmp+split1);
	}
	merge_3way(strings, split0,
	           strings+split0, split1-split0,
	           strings+split1, n-split1,
	           tmp);
	(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
}
void mergesort_3way_parallel(unsigned char** strings, size_t n)
{
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	mergesort_3way_parallel(strings, n, tmp);
	free(tmp);
}
ROUTINE_REGISTER_MULTICORE(mergesort_3way_parallel,
		"Parallel mergesort with 3way merger")

/*******************************************************************************
 *
 * mergesort_4way
 *
 ******************************************************************************/

static void
merge_4way(unsigned char** restrict from0, size_t n0,
           unsigned char** restrict from1, size_t n1,
           unsigned char** restrict from2, size_t n2,
           unsigned char** restrict from3, size_t n3,
           unsigned char** restrict result)
{
	debug()<<__func__<<"(), n0="<<n0<<", n1="<<n1<<", n2="<<n2
	       <<", n3="<<n3<<"\n";
	if (cmp(*from0, *from1) <= 0) {
		if (cmp(*from1, *from2) <= 0) {
			/* 0 <= 1 <= 2 */
			if (cmp(*from3, *from0) < 0) goto state_3012;
			if (cmp(*from3, *from1) < 0) goto state_0312;
			if (cmp(*from3, *from2) < 0) goto state_0132;
			goto state_0123;
		}
		if (cmp(*from2, *from0) < 0) {
			/* 2 < 0 <= 1 */
			if (cmp(*from3, *from2) < 0) goto state_3201;
			if (cmp(*from3, *from0) < 0) goto state_2301;
			if (cmp(*from3, *from1) < 0) goto state_2031;
			goto state_2013;
		}
		/* 0 <= 2 < 1 */
		if (cmp(*from3, *from0) < 0) goto state_3021;
		if (cmp(*from3, *from2) < 0) goto state_0321;
		if (cmp(*from3, *from1) < 0) goto state_0231;
		goto state_0213;
	}
	if (cmp(*from1, *from2) <= 0) {
		if (cmp(*from0, *from2) <= 0) {
			/* 1 < 0 <= 2 */
			if (cmp(*from3, *from1) < 0) goto state_3102;
			if (cmp(*from3, *from0) < 0) goto state_1302;
			if (cmp(*from3, *from2) < 0) goto state_1032;
			goto state_1023;
		}
		/* 1 <= 2 < 0 */
		if (cmp(*from3, *from1) < 0) goto state_3120;
		if (cmp(*from3, *from2) < 0) goto state_1320;
		if (cmp(*from3, *from0) < 0) goto state_1230;
		goto state_1203;
	}
	/* 2 < 1 < 0 */
	if (cmp(*from3, *from2) < 0) goto state_3210;
	if (cmp(*from3, *from1) < 0) goto state_2310;
	if (cmp(*from3, *from0) < 0) goto state_2130;
	goto state_2103;

#define MAKE_STATE(a,b,c,d)                                                    \
        state_ ## a ## b ## c ## d:                                            \
        assert(cmp(*from ## a, *from ## b) <= 0);                              \
        assert(cmp(*from ## b, *from ## c) <= 0);                              \
        assert(cmp(*from ## c, *from ## d) <= 0);                              \
        *result++ = *from ## a ++;                                             \
        if (--n ## a == 0) goto finish ## a ;                                  \
        if (a < b) if (cmp(*from##a, *from##b) <= 0) goto state_##a##b##c##d;  \
        if (a > b) if (cmp(*from##a, *from##b) <  0) goto state_##a##b##c##d;  \
        if (a < c) if (cmp(*from##a, *from##c) <= 0) goto state_##b##a##c##d;  \
        if (a > c) if (cmp(*from##a, *from##c) <  0) goto state_##b##a##c##d;  \
        if (a < d) if (cmp(*from##a, *from##d) <= 0) goto state_##b##c##a##d;  \
        if (a > d) if (cmp(*from##a, *from##d) <  0) goto state_##b##c##a##d;  \
        goto state_ ## b ## c ## d ## a;

	MAKE_STATE(0, 1, 2, 3); MAKE_STATE(0, 1, 3, 2); MAKE_STATE(0, 2, 1, 3);
	MAKE_STATE(0, 2, 3, 1); MAKE_STATE(0, 3, 1, 2); MAKE_STATE(0, 3, 2, 1);
	MAKE_STATE(1, 0, 2, 3); MAKE_STATE(1, 0, 3, 2); MAKE_STATE(1, 2, 0, 3);
	MAKE_STATE(1, 2, 3, 0); MAKE_STATE(1, 3, 0, 2); MAKE_STATE(1, 3, 2, 0);
	MAKE_STATE(2, 0, 1, 3); MAKE_STATE(2, 0, 3, 1); MAKE_STATE(2, 1, 0, 3);
	MAKE_STATE(2, 1, 3, 0); MAKE_STATE(2, 3, 0, 1); MAKE_STATE(2, 3, 1, 0);
	MAKE_STATE(3, 0, 1, 2); MAKE_STATE(3, 0, 2, 1); MAKE_STATE(3, 1, 0, 2);
	MAKE_STATE(3, 1, 2, 0); MAKE_STATE(3, 2, 0, 1); MAKE_STATE(3, 2, 1, 0);
#undef MAKE_STATE

finish0:
	assert(n0 == 0);
	merge_3way(from1, n1, from2, n2, from3, n3, result);
	return;
finish1:
	assert(n1 == 0);
	merge_3way(from0, n0, from2, n2, from3, n3, result);
	return;
finish2:
	assert(n2 == 0);
	merge_3way(from0, n0, from1, n1, from3, n3, result);
	return;
finish3:
	assert(n3 == 0);
	merge_3way(from0, n0, from1, n1, from2, n2, result);
	return;
}
void
mergesort_4way(unsigned char** strings, size_t n, unsigned char** tmp)
{
	debug() << __func__ << "(), n="<<n<<"\n";
	if (n < 32) {
		insertion_sort(strings, n, 0);
		return;
	}
	const size_t split0 = n/4,
	             split1 = n/2,
	             split2 = split0+split1;
	mergesort_4way(strings,        split0,        tmp);
	mergesort_4way(strings+split0, split1-split0, tmp);
	mergesort_4way(strings+split1, split2-split1, tmp);
	mergesort_4way(strings+split2, n-split2,      tmp);
	merge_4way(strings,        split0,
	           strings+split0, split1-split0,
	           strings+split1, split2-split1,
	           strings+split2, n-split2,
	           tmp);
	(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
}
void mergesort_4way(unsigned char** strings, size_t n)
{
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	mergesort_4way(strings, n, tmp);
	free(tmp);
}
ROUTINE_REGISTER_SINGLECORE(mergesort_4way, "mergesort_4way")

void
mergesort_4way_parallel(unsigned char** strings, size_t n, unsigned char** tmp)
{
	debug() << __func__ << "(), n="<<n<<"\n";
	if (n < 32) {
		insertion_sort(strings, n, 0);
		return;
	}
	const size_t split0 = n/4,
	             split1 = n/2,
	             split2 = split0+split1;
#pragma omp parallel sections
	{
#pragma omp section
		mergesort_4way_parallel(strings,        split0,        tmp);
#pragma omp section
		mergesort_4way_parallel(strings+split0, split1-split0, tmp+split0);
#pragma omp section
		mergesort_4way_parallel(strings+split1, split2-split1, tmp+split1);
#pragma omp section
		mergesort_4way_parallel(strings+split2, n-split2,      tmp+split2);
	}
	merge_4way(strings,        split0,
	           strings+split0, split1-split0,
	           strings+split1, split2-split1,
	           strings+split2, n-split2,
	           tmp);
	(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
}
void mergesort_4way_parallel(unsigned char** strings, size_t n)
{
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	mergesort_4way_parallel(strings, n, tmp);
	free(tmp);
}
ROUTINE_REGISTER_MULTICORE(mergesort_4way_parallel,
		"Parallel mergesort with 4way merger")
