/*
 * Copyright 2008 by Tommi Rantala <tt.rantala@gmail.com>
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

/* Implements unstable mergesort variants, ie. whenever the streams contain
 * equal elements, we output them all at once. The idea is to save some
 * comparisons.
 *
 * We closely follow the implementation described by Sanders:
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
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdlib>

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
 * mergesort_2way_unstable
 *
 ******************************************************************************/

static void
merge_2way_unstable(unsigned char** restrict from0, size_t n0,
                    unsigned char** restrict from1, size_t n1,
                    unsigned char** restrict result)
{
	debug()<<__func__<<"(), n0="<<n0<<", n1="<<n1<<"\n";
	unsigned char* key0 = *from0;
	unsigned char* key1 = *from1;
initial:
	const int cmp01 = cmp(key0, key1);
	if (cmp01 > 0)  goto state_1lt0;
	if (cmp01 == 0) goto state_0eq1;
	// Fall through.
state_0lt1:
{
	debug() << "\tstate_0lt1\n";
	assert(cmp(key0, key1) <  0);
	*result++ = key0;
	if (--n0 == 0) goto finish0;
	++from0;
	key0 = *from0;
	const int cmp_0_1 = cmp(key0, key1);
	if (cmp_0_1 <  0) goto state_0lt1;
	if (cmp_0_1 == 0) goto state_0eq1;
	// Fall through.
}
state_1lt0:
{
	debug() << "\tstate_1lt0\n";
	assert(cmp(key1, key0) <  0);
	*result++ = key1;
	if (--n1 == 0) goto finish1;
	++from1;
	key1 = *from1;
	const int cmp_1_0 = cmp(key1, key0);
	if (cmp_1_0 < 0) goto state_1lt0;
	if (cmp_1_0 > 0) goto state_0lt1;
	// Fall through.
}
state_0eq1:
{
	debug() << "\tstate_0eq1\n";
	assert(cmp(key0, key1) == 0);
	*result++ = key0;
	*result++ = key1;
	++from0;
	++from1;
	--n0;
	--n1;
	if (n0 == 0) goto finish0;
	if (n1 == 0) goto finish1;
	key0 = *from0;
	key1 = *from1;
	goto initial;
}
finish0:
	assert(n0==0);
	if (n1) (void) memcpy(result, from1, n1*sizeof(unsigned char*));
	debug()<< "~" << __func__ << "\n";
	return;
finish1:
	assert(n1==0);
	if (n0) (void) memcpy(result, from0, n0*sizeof(unsigned char*));
	debug()<< "~" << __func__ << "\n";
	return;
}
static void
mergesort_2way_unstable(unsigned char** strings, size_t n, unsigned char** tmp)
{
	debug() << __func__ << "(), n="<<n<<"\n";
	if (n < 32) {
		insertion_sort(strings, n, 0);
		return;
	}
	const size_t split0 = n/2;
	mergesort_2way_unstable(strings,        split0,   tmp);
	mergesort_2way_unstable(strings+split0, n-split0, tmp);
	merge_2way_unstable(strings, split0,
	                    strings+split0, n-split0,
	                    tmp);
	(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
}
void mergesort_2way_unstable(unsigned char** strings, size_t n)
{
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	mergesort_2way_unstable(strings, n, tmp);
	free(tmp);
}
ROUTINE_REGISTER_SINGLECORE(mergesort_2way_unstable, "2way unstable mergesort")

/*******************************************************************************
 *
 * mergesort_3way_unstable
 *
 ******************************************************************************/

static void
merge_3way_unstable(unsigned char** restrict from0, size_t n0,
                    unsigned char** restrict from1, size_t n1,
                    unsigned char** restrict from2, size_t n2,
                    unsigned char** restrict result)
{
	debug()<<__func__<<"(), n0="<<n0<<", n1="<<n1<<", n2="<<n2<<"\n";

initial:
{
	const int cmp01 = cmp(*from0, *from1);
	const int cmp12 = cmp(*from1, *from2);
	int cmp02 = 0;
	if (cmp01 < 0) {
		if (cmp12 < 0)  { goto state0lt1lt2; }
		if (cmp12 == 0) { goto state0lt1eq2; }
		cmp02 = cmp(*from0, *from2);
		if (cmp02 < 0)  { goto state0lt2lt1; }
		if (cmp02 == 0) { goto state0eq2lt1; }
		goto state2lt0lt1;
	}
	if (cmp01 == 0) {
		if (cmp12 < 0)  { goto state0eq1lt2; }
		if (cmp12 == 0) { goto all_eq;       }
		goto state2lt0eq1;
	}
	if (cmp12 > 0)  { goto state2lt1lt0; }
	if (cmp12 == 0) { goto state1eq2lt0; }
	cmp02 = cmp(*from0, *from2);
	if (cmp02 < 0)  { goto state1lt0lt2; }
	if (cmp02 == 0) { goto state1lt0eq2; }
	goto state1lt2lt0;
}

#define StrictCase(a,b,c)                                                      \
	state##a##lt##b##lt##c:                                                \
	{                                                                      \
		*result++ = *from##a++;                                        \
		if (--n##a == 0) goto finish##a;                               \
		int cmpab = cmp(*from##a, *from##b);                           \
		if (cmpab < 0)  goto state##a##lt##b##lt##c;                   \
		if (cmpab == 0) goto state##a##eq##b##lt##c;                   \
		int cmpac = cmp(*from##a, *from##c);                           \
		if (cmpac < 0)  goto state##b##lt##a##lt##c;                   \
		if (cmpac == 0) goto state##b##lt##a##eq##c;                   \
		goto state##b##lt##c##lt##a;                                   \
	}
	StrictCase(0, 1, 2)
	StrictCase(0, 2, 1)
	StrictCase(1, 0, 2)
	StrictCase(1, 2, 0)
	StrictCase(2, 0, 1)
	StrictCase(2, 1, 0)
#undef StrictCase

#define EqLt(a,b,c)                                                            \
	static_assert(a < b, "a < b");                                         \
	state##a##eq##b##lt##c:                                                \
	{                                                                      \
		*result++ = *from ##a ++;                                      \
		*result++ = *from ##b ++;                                      \
		--n##a;                                                        \
		--n##b;                                                        \
		if (n##a == 0) goto finish ##a;                                \
		if (n##b == 0) goto finish ##b;                                \
		goto initial;                                                  \
	}
	EqLt(0, 1, 2)
	EqLt(0, 2, 1)
	EqLt(1, 2, 0)
state2eq0lt1: goto state0eq2lt1;
state1eq0lt2: goto state0eq1lt2;
state2eq1lt0: goto state1eq2lt0;
#undef EqLt

#define LtEq(a,b,c)                                                            \
	static_assert(b < c, "b < c");                                         \
	state##a##lt##b##eq##c:                                                \
	{                                                                      \
		*result++ = *from##a++;                                        \
		if (--n##a == 0) goto finish##a;                               \
		int cmpab = cmp(*from##a, *from##b);                           \
		if (cmpab < 0)  goto state##a##lt##b##eq##c;                   \
		if (cmpab == 0) goto all_eq;                                   \
		goto state##b##eq##c##lt## a;                                  \
	}
	LtEq(0, 1, 2)
	LtEq(1, 0, 2)
	LtEq(2, 0, 1)
state0lt2eq1: goto state0lt1eq2;
state1lt2eq0: goto state1lt0eq2;
state2lt1eq0: goto state2lt0eq1;
#undef LtEq

all_eq:
{
	*result++ = *from0++;
	*result++ = *from1++;
	*result++ = *from2++;
	--n0;
	--n1;
	--n2;
	if (n0==0) goto finish0;
	if (n1==0) goto finish1;
	if (n2==0) goto finish2;
	goto initial;
}

finish0:
	assert(n0 == 0);
	if (n1) {
		if (n2) {
			merge_2way_unstable(from1, n1, from2, n2, result);
		} else {
			(void) memcpy(result, from1, n1*sizeof(unsigned char*));
		}
	} else if (n2) {
		(void) memcpy(result, from2, n2*sizeof(unsigned char*));
	}
	debug()<< "~" << __func__ << "\n";
	return;
finish1:
	assert(n1 == 0);
	if (n0) {
		if (n2) {
			merge_2way_unstable(from0, n0, from2, n2, result);
		} else {
			(void) memcpy(result, from0, n0*sizeof(unsigned char*));
		}
	} else if (n2) {
		(void) memcpy(result, from2, n2*sizeof(unsigned char*));
	}
	debug()<< "~" << __func__ << "\n";
	return;
finish2:
	assert(n2 == 0);
	if (n0) {
		if (n1) {
			merge_2way_unstable(from0, n0, from1, n1, result);
		} else {
			(void) memcpy(result, from0, n0*sizeof(unsigned char*));
		}
	} else if (n1) {
		(void) memcpy(result, from1, n1*sizeof(unsigned char*));
	}
	debug()<< "~" << __func__ << "\n";
	return;
}
static void
mergesort_3way_unstable(unsigned char** strings, size_t n, unsigned char** tmp)
{
	debug() << __func__ << "(), n="<<n<<"\n";
	if (n < 32) {
		insertion_sort(strings, n, 0);
		return;
	}
	const size_t split0 = n/3, split1 = (2*n)/3;
	mergesort_3way_unstable(strings,        split0,        tmp);
	mergesort_3way_unstable(strings+split0, split1-split0, tmp);
	mergesort_3way_unstable(strings+split1, n-split1,      tmp);
	merge_3way_unstable(strings, split0,
	                    strings+split0, split1-split0,
	                    strings+split1, n-split1,
	                    tmp);
	(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
}
void mergesort_3way_unstable(unsigned char** strings, size_t n)
{
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	mergesort_3way_unstable(strings, n, tmp);
	free(tmp);
}
ROUTINE_REGISTER_SINGLECORE(mergesort_3way_unstable, "3way unstable mergesort")

/*******************************************************************************
 *
 * mergesort_4way_unstable
 *
 ******************************************************************************/

static void
merge_4way_unstable(unsigned char** restrict from0, size_t n0,
                    unsigned char** restrict from1, size_t n1,
                    unsigned char** restrict from2, size_t n2,
                    unsigned char** restrict from3, size_t n3,
                    unsigned char** restrict result)
{
	debug()<<__func__<<"(), n0="<<n0<<", n1="<<n1<<", n2="<<n2<<", n3="<<n3<<"\n";

initial:
	debug() << "\tinitial state\n";
	const int cmp01 = cmp(*from0, *from1);
	const int cmp12 = cmp(*from1, *from2);
	const int cmp23 = cmp(*from2, *from3);
	if (cmp23 < 0) {
		int cmp02 = 0;
		if (cmp01 < 0) {
			if (cmp12 < 0)  { goto state_0lt1lt2lt3; }
			if (cmp12 == 0) { goto state_0lt1eq2lt3; }
			cmp02 = cmp(*from0, *from2);
			if (cmp02 < 0) {
				const int cmp13 = cmp(*from1, *from3);
				if (cmp13 < 0)  { goto state_0lt2lt1lt3; }
				if (cmp13 == 0) { goto state_0lt2lt1eq3; }
				goto state_0lt2lt3lt1;
			}
			if (cmp02 == 0) {
				const int cmp13 = cmp(*from1, *from3);
				if (cmp13 < 0)  { goto state_0eq2lt1lt3; }
				if (cmp13 == 0) { goto state_0eq2lt1eq3; }
				goto state_0eq2lt3lt1;
			}
			const int cmp03 = cmp(*from0, *from3);
			if (cmp03 < 0) {
				const int cmp13 = cmp(*from1, *from3);
				if (cmp13 < 0)  { goto state_2lt0lt1lt3; }
				if (cmp13 == 0) { goto state_2lt0lt1eq3; }
				goto state_2lt0lt3lt1;
			}
			if (cmp03 == 0) { goto state_2lt0eq3lt1; }
			goto state_2lt3lt0lt1;
		}
		if (cmp01 == 0) {
			if (cmp12 < 0)  { goto state_0eq1lt2lt3; }
			if (cmp12 == 0) { goto state_0eq1eq2lt3; }
			const int cmp03 = cmp(*from0, *from3);
			if (cmp03 < 0)  { goto state_2lt0eq1lt3; }
			if (cmp03 == 0) { goto state_2lt0eq1eq3; }
			goto state_2lt3lt0eq1;
		}
		if (cmp12 > 0) {
			const int cmp13 = cmp(*from1, *from3);
			if (cmp13 < 0) {
				const int cmp03 = cmp(*from0, *from3);
				if (cmp03 < 0)  { goto state_2lt1lt0lt3; }
				if (cmp03 == 0) { goto state_2lt1lt0eq3; }
				goto state_2lt1lt3lt0;
			}
			if (cmp13 == 0) { goto state_2lt1eq3lt0; }
			goto state_2lt3lt1lt0;
		}
		if (cmp12 == 0) {
			const int cmp03 = cmp(*from0, *from3);
			if (cmp03 < 0)  { goto state_1eq2lt0lt3; }
			if (cmp03 == 0) { goto state_1eq2lt0eq3; }
			goto state_1eq2lt3lt0;
		}
		cmp02 = cmp(*from0, *from2);
		if (cmp02 < 0)  { goto state_1lt0lt2lt3; }
		if (cmp02 == 0) { goto state_1lt0eq2lt3; }
		const int cmp03 = cmp(*from0, *from3);
		if (cmp03 < 0)  { goto state_1lt2lt0lt3; }
		if (cmp03 == 0) { goto state_1lt2lt0eq3; }
		goto state_1lt2lt3lt0;
	} else if (cmp23 == 0) {
		int cmp02 = 0;
		if (cmp01 < 0) {
			if (cmp12 < 0)  { goto state_0lt1lt2eq3; }
			if (cmp12 == 0) { goto state_0lt1eq2eq3; }
			cmp02 = cmp(*from0, *from2);
			if (cmp02 < 0)  { goto state_0lt2eq3lt1; }
			if (cmp02 == 0) { goto state_0eq2eq3lt1; }
			goto state_2eq3lt0lt1;
		}
		if (cmp01 == 0) {
			if (cmp12 < 0)  { goto state_0eq1lt2eq3; }
			if (cmp12 == 0) { goto state_0eq1eq2eq3; }
			goto state_2eq3lt0eq1;
		}
		if (cmp12 > 0)  { goto state_2eq3lt1lt0; }
		if (cmp12 == 0) { goto state_1eq2eq3lt0; }
		cmp02 = cmp(*from0, *from2);
		if (cmp02 < 0)  { goto state_1lt0lt2eq3; }
		if (cmp02 == 0) { goto state_1lt0eq2eq3; }
		goto state_1lt2eq3lt0;
	} else {
		int cmp02 = 0;
		if (cmp01 < 0) {
			if (cmp12 < 0) {
				const int cmp03 = cmp(*from0, *from3);
				if (cmp03 < 0) {
					const int cmp13 = cmp(*from1, *from3);
					if (cmp13 < 0)  { goto state_0lt1lt3lt2; }
					if (cmp13 == 0) { goto state_0lt1eq3lt2; }
					goto state_0lt3lt1lt2;
				}
				if (cmp03 == 0) { goto state_0eq3lt1lt2; }
				goto state_3lt0lt1lt2;
			}
			if (cmp12 == 0) {
				const int cmp03 = cmp(*from0, *from3);
				if (cmp03 < 0)  { goto state_0lt3lt1eq2; }
				if (cmp03 == 0) { goto state_0eq3lt1eq2; }
				goto state_3lt0lt1eq2;
			}
			cmp02 = cmp(*from0, *from2);
			if (cmp02 < 0) {
				const int cmp03 = cmp(*from0, *from3);
				if (cmp03 < 0)  { goto state_0lt3lt2lt1; }
				if (cmp03 == 0) { goto state_0eq3lt2lt1; }
				goto state_3lt0lt2lt1;
			}
			if (cmp02 == 0) { goto state_3lt0eq2lt1; }
			goto state_3lt2lt0lt1;
		}
		if (cmp01 == 0) {
			if (cmp12 < 0) {
				const int cmp03 = cmp(*from0, *from3);
				if (cmp03 < 0)  { goto state_0eq1lt3lt2; }
				if (cmp03 == 0) { goto state_0eq1eq3lt2; }
				goto state_3lt0eq1lt2;
			}
			if (cmp12 == 0) { goto state_3lt0eq1eq2; }
			goto state_3lt2lt0eq1;
		}
		if (cmp12 > 0)  { goto state_3lt2lt1lt0; }
		if (cmp12 == 0) { goto state_3lt1eq2lt0; }
		cmp02 = cmp(*from0, *from2);
		if (cmp02 < 0) {
			const int cmp13 = cmp(*from1, *from3);
			if (cmp13 < 0) {
				const int cmp03 = cmp(*from0, *from3);
				if (cmp03 < 0)  { goto state_1lt0lt3lt2; }
				if (cmp03 == 0) { goto state_1lt0eq3lt2; }
				goto state_1lt3lt0lt2;
			}
			if (cmp13 == 0) { goto state_1eq3lt0lt2; }
			goto state_3lt1lt0lt2;
		}
		if (cmp02 == 0) {
			const int cmp13 = cmp(*from1, *from3);
			if (cmp13 < 0)  { goto state_1lt3lt0eq2; }
			if (cmp13 == 0) { goto state_1eq3lt0eq2; }
			goto state_3lt1lt0eq2;
		}
		const int cmp13 = cmp(*from1, *from3);
		if (cmp13 < 0)  { goto state_1lt3lt2lt0; }
		if (cmp13 == 0) { goto state_1eq3lt2lt0; }
		goto state_3lt1lt2lt0;
	}

state_0lt1lt2lt3:
{
	debug() << "\tstate_0lt1lt2lt3\n";
	assert(cmp(*from0, *from1) <  0);
	assert(cmp(*from1, *from2) <  0);
	assert(cmp(*from2, *from3) <  0);
	*result++ = *from0++;
	if (--n0 == 0) goto finish0;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_0lt1lt2lt3;
	if (cmp_0_1 == 0) goto state_0eq1lt2lt3;
	const int cmp_0_2 = cmp(*from0, *from2);
	if (cmp_0_2 <  0) goto state_1lt0lt2lt3;
	if (cmp_0_2 == 0) goto state_1lt0eq2lt3;
	const int cmp_0_3 = cmp(*from0, *from3);
	if (cmp_0_3 <  0) goto state_1lt2lt0lt3;
	if (cmp_0_3 == 0) goto state_1lt2lt0eq3;
	goto state_1lt2lt3lt0;
}
state_0lt1lt3lt2:
{
	debug() << "\tstate_0lt1lt3lt2\n";
	assert(cmp(*from0, *from1) <  0);
	assert(cmp(*from1, *from3) <  0);
	assert(cmp(*from3, *from2) <  0);
	*result++ = *from0++;
	if (--n0 == 0) goto finish0;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_0lt1lt3lt2;
	if (cmp_0_1 == 0) goto state_0eq1lt3lt2;
	const int cmp_0_3 = cmp(*from0, *from3);
	if (cmp_0_3 <  0) goto state_1lt0lt3lt2;
	if (cmp_0_3 == 0) goto state_1lt0eq3lt2;
	const int cmp_0_2 = cmp(*from0, *from2);
	if (cmp_0_2 <  0) goto state_1lt3lt0lt2;
	if (cmp_0_2 == 0) goto state_1lt3lt0eq2;
	goto state_1lt3lt2lt0;
}
state_0lt2lt1lt3:
{
	debug() << "\tstate_0lt2lt1lt3\n";
	assert(cmp(*from0, *from2) <  0);
	assert(cmp(*from2, *from1) <  0);
	assert(cmp(*from1, *from3) <  0);
	*result++ = *from0++;
	if (--n0 == 0) goto finish0;
	const int cmp_0_2 = cmp(*from0, *from2);
	if (cmp_0_2 <  0) goto state_0lt2lt1lt3;
	if (cmp_0_2 == 0) goto state_0eq2lt1lt3;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_2lt0lt1lt3;
	if (cmp_0_1 == 0) goto state_2lt0eq1lt3;
	const int cmp_0_3 = cmp(*from0, *from3);
	if (cmp_0_3 <  0) goto state_2lt1lt0lt3;
	if (cmp_0_3 == 0) goto state_2lt1lt0eq3;
	goto state_2lt1lt3lt0;
}
state_0lt2lt3lt1:
{
	debug() << "\tstate_0lt2lt3lt1\n";
	assert(cmp(*from0, *from2) <  0);
	assert(cmp(*from2, *from3) <  0);
	assert(cmp(*from3, *from1) <  0);
	*result++ = *from0++;
	if (--n0 == 0) goto finish0;
	const int cmp_0_2 = cmp(*from0, *from2);
	if (cmp_0_2 <  0) goto state_0lt2lt3lt1;
	if (cmp_0_2 == 0) goto state_0eq2lt3lt1;
	const int cmp_0_3 = cmp(*from0, *from3);
	if (cmp_0_3 <  0) goto state_2lt0lt3lt1;
	if (cmp_0_3 == 0) goto state_2lt0eq3lt1;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_2lt3lt0lt1;
	if (cmp_0_1 == 0) goto state_2lt3lt0eq1;
	goto state_2lt3lt1lt0;
}
state_0lt3lt1lt2:
{
	debug() << "\tstate_0lt3lt1lt2\n";
	assert(cmp(*from0, *from3) <  0);
	assert(cmp(*from3, *from1) <  0);
	assert(cmp(*from1, *from2) <  0);
	*result++ = *from0++;
	if (--n0 == 0) goto finish0;
	const int cmp_0_3 = cmp(*from0, *from3);
	if (cmp_0_3 <  0) goto state_0lt3lt1lt2;
	if (cmp_0_3 == 0) goto state_0eq3lt1lt2;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_3lt0lt1lt2;
	if (cmp_0_1 == 0) goto state_3lt0eq1lt2;
	const int cmp_0_2 = cmp(*from0, *from2);
	if (cmp_0_2 <  0) goto state_3lt1lt0lt2;
	if (cmp_0_2 == 0) goto state_3lt1lt0eq2;
	goto state_3lt1lt2lt0;
}
state_0lt3lt2lt1:
{
	debug() << "\tstate_0lt3lt2lt1\n";
	assert(cmp(*from0, *from3) <  0);
	assert(cmp(*from3, *from2) <  0);
	assert(cmp(*from2, *from1) <  0);
	*result++ = *from0++;
	if (--n0 == 0) goto finish0;
	const int cmp_0_3 = cmp(*from0, *from3);
	if (cmp_0_3 <  0) goto state_0lt3lt2lt1;
	if (cmp_0_3 == 0) goto state_0eq3lt2lt1;
	const int cmp_0_2 = cmp(*from0, *from2);
	if (cmp_0_2 <  0) goto state_3lt0lt2lt1;
	if (cmp_0_2 == 0) goto state_3lt0eq2lt1;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_3lt2lt0lt1;
	if (cmp_0_1 == 0) goto state_3lt2lt0eq1;
	goto state_3lt2lt1lt0;
}
state_1lt0lt2lt3:
{
	debug() << "\tstate_1lt0lt2lt3\n";
	assert(cmp(*from1, *from0) <  0);
	assert(cmp(*from0, *from2) <  0);
	assert(cmp(*from2, *from3) <  0);
	*result++ = *from1++;
	if (--n1 == 0) goto finish1;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_1lt0lt2lt3;
	if (cmp_1_0 == 0) goto state_0eq1lt2lt3;
	const int cmp_1_2 = cmp(*from1, *from2);
	if (cmp_1_2 <  0) goto state_0lt1lt2lt3;
	if (cmp_1_2 == 0) goto state_0lt1eq2lt3;
	const int cmp_1_3 = cmp(*from1, *from3);
	if (cmp_1_3 <  0) goto state_0lt2lt1lt3;
	if (cmp_1_3 == 0) goto state_0lt2lt1eq3;
	goto state_0lt2lt3lt1;
}
state_1lt0lt3lt2:
{
	debug() << "\tstate_1lt0lt3lt2\n";
	assert(cmp(*from1, *from0) <  0);
	assert(cmp(*from0, *from3) <  0);
	assert(cmp(*from3, *from2) <  0);
	*result++ = *from1++;
	if (--n1 == 0) goto finish1;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_1lt0lt3lt2;
	if (cmp_1_0 == 0) goto state_1eq0lt3lt2;
	const int cmp_1_3 = cmp(*from1, *from3);
	if (cmp_1_3 <  0) goto state_0lt1lt3lt2;
	if (cmp_1_3 == 0) goto state_0lt1eq3lt2;
	const int cmp_1_2 = cmp(*from1, *from2);
	if (cmp_1_2 <  0) goto state_0lt3lt1lt2;
	if (cmp_1_2 == 0) goto state_0lt3lt1eq2;
	goto state_0lt3lt2lt1;
}
state_1lt2lt0lt3:
{
	debug() << "\tstate_1lt2lt0lt3\n";
	assert(cmp(*from1, *from2) <  0);
	assert(cmp(*from2, *from0) <  0);
	assert(cmp(*from0, *from3) <  0);
	*result++ = *from1++;
	if (--n1 == 0) goto finish1;
	const int cmp_1_2 = cmp(*from1, *from2);
	if (cmp_1_2 <  0) goto state_1lt2lt0lt3;
	if (cmp_1_2 == 0) goto state_1eq2lt0lt3;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_2lt1lt0lt3;
	if (cmp_1_0 == 0) goto state_2lt1eq0lt3;
	const int cmp_1_3 = cmp(*from1, *from3);
	if (cmp_1_3 <  0) goto state_2lt0lt1lt3;
	if (cmp_1_3 == 0) goto state_2lt0lt1eq3;
	goto state_2lt0lt3lt1;
}
state_1lt2lt3lt0:
{
	debug() << "\tstate_1lt2lt3lt0\n";
	assert(cmp(*from1, *from2) <  0);
	assert(cmp(*from2, *from3) <  0);
	assert(cmp(*from3, *from0) <  0);
	*result++ = *from1++;
	if (--n1 == 0) goto finish1;
	const int cmp_1_2 = cmp(*from1, *from2);
	if (cmp_1_2 <  0) goto state_1lt2lt3lt0;
	if (cmp_1_2 == 0) goto state_1eq2lt3lt0;
	const int cmp_1_3 = cmp(*from1, *from3);
	if (cmp_1_3 <  0) goto state_2lt1lt3lt0;
	if (cmp_1_3 == 0) goto state_2lt1eq3lt0;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_2lt3lt1lt0;
	if (cmp_1_0 == 0) goto state_2lt3lt1eq0;
	goto state_2lt3lt0lt1;
}
state_1lt3lt0lt2:
{
	debug() << "\tstate_1lt3lt0lt2\n";
	assert(cmp(*from1, *from3) <  0);
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from2) <  0);
	*result++ = *from1++;
	if (--n1 == 0) goto finish1;
	const int cmp_1_3 = cmp(*from1, *from3);
	if (cmp_1_3 <  0) goto state_1lt3lt0lt2;
	if (cmp_1_3 == 0) goto state_1eq3lt0lt2;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_3lt1lt0lt2;
	if (cmp_1_0 == 0) goto state_3lt1eq0lt2;
	const int cmp_1_2 = cmp(*from1, *from2);
	if (cmp_1_2 <  0) goto state_3lt0lt1lt2;
	if (cmp_1_2 == 0) goto state_3lt0lt1eq2;
	goto state_3lt0lt2lt1;
}
state_1lt3lt2lt0:
{
	debug() << "\tstate_1lt3lt2lt0\n";
	assert(cmp(*from1, *from3) <  0);
	assert(cmp(*from3, *from2) <  0);
	assert(cmp(*from2, *from0) <  0);
	*result++ = *from1++;
	if (--n1 == 0) goto finish1;
	const int cmp_1_3 = cmp(*from1, *from3);
	if (cmp_1_3 <  0) goto state_1lt3lt2lt0;
	if (cmp_1_3 == 0) goto state_1eq3lt2lt0;
	const int cmp_1_2 = cmp(*from1, *from2);
	if (cmp_1_2 <  0) goto state_3lt1lt2lt0;
	if (cmp_1_2 == 0) goto state_3lt1eq2lt0;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_3lt2lt1lt0;
	if (cmp_1_0 == 0) goto state_3lt2lt0eq1;
	goto state_3lt2lt0lt1;
}
state_2lt0lt1lt3:
{
	debug() << "\tstate_2lt0lt1lt3\n";
	assert(cmp(*from2, *from0) <  0);
	assert(cmp(*from0, *from1) <  0);
	assert(cmp(*from1, *from3) <  0);
	*result++ = *from2++;
	if (--n2 == 0) goto finish2;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_2lt0lt1lt3;
	if (cmp_2_0 == 0) goto state_2eq0lt1lt3;
	const int cmp_2_1 = cmp(*from2, *from1);
	if (cmp_2_1 <  0) goto state_0lt2lt1lt3;
	if (cmp_2_1 == 0) goto state_0lt2eq1lt3;
	const int cmp_2_3 = cmp(*from2, *from3);
	if (cmp_2_3 <  0) goto state_0lt1lt2lt3;
	if (cmp_2_3 == 0) goto state_0lt1lt2eq3;
	goto state_0lt1lt3lt2;
}
state_2lt0lt3lt1:
{
	debug() << "\tstate_2lt0lt3lt1\n";
	assert(cmp(*from2, *from0) <  0);
	assert(cmp(*from0, *from3) <  0);
	assert(cmp(*from3, *from1) <  0);
	*result++ = *from2++;
	if (--n2 == 0) goto finish2;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_2lt0lt3lt1;
	if (cmp_2_0 == 0) goto state_2eq0lt3lt1;
	const int cmp_2_3 = cmp(*from2, *from3);
	if (cmp_2_3 <  0) goto state_0lt2lt3lt1;
	if (cmp_2_3 == 0) goto state_0lt2eq3lt1;
	const int cmp_2_1 = cmp(*from2, *from1);
	if (cmp_2_1 <  0) goto state_0lt3lt2lt1;
	if (cmp_2_1 == 0) goto state_0lt3lt1eq2;
	goto state_0lt3lt1lt2;
}
state_2lt1lt0lt3:
{
	debug() << "\tstate_2lt1lt0lt3\n";
	assert(cmp(*from2, *from1) <  0);
	assert(cmp(*from1, *from0) <  0);
	assert(cmp(*from0, *from3) <  0);
	*result++ = *from2++;
	if (--n2 == 0) goto finish2;
	const int cmp_2_1 = cmp(*from2, *from1);
	if (cmp_2_1 <  0) goto state_2lt1lt0lt3;
	if (cmp_2_1 == 0) goto state_2eq1lt0lt3;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_1lt2lt0lt3;
	if (cmp_2_0 == 0) goto state_1lt2eq0lt3;
	const int cmp_2_3 = cmp(*from2, *from3);
	if (cmp_2_3 <  0) goto state_1lt0lt2lt3;
	if (cmp_2_3 == 0) goto state_1lt0lt2eq3;
	goto state_1lt0lt3lt2;
}
state_2lt1lt3lt0:
{
	debug() << "\tstate_2lt1lt3lt0\n";
	assert(cmp(*from2, *from1) <  0);
	assert(cmp(*from1, *from3) <  0);
	assert(cmp(*from3, *from0) <  0);
	*result++ = *from2++;
	if (--n2 == 0) goto finish2;
	const int cmp_2_1 = cmp(*from2, *from1);
	if (cmp_2_1 <  0) goto state_2lt1lt3lt0;
	if (cmp_2_1 == 0) goto state_2eq1lt3lt0;
	const int cmp_2_3 = cmp(*from2, *from3);
	if (cmp_2_3 <  0) goto state_1lt2lt3lt0;
	if (cmp_2_3 == 0) goto state_1lt2eq3lt0;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_1lt3lt2lt0;
	if (cmp_2_0 == 0) goto state_1lt3lt2eq0;
	goto state_1lt3lt0lt2;
}
state_2lt3lt0lt1:
{
	debug() << "\tstate_2lt3lt0lt1\n";
	assert(cmp(*from2, *from3) <  0);
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from1) <  0);
	*result++ = *from2++;
	if (--n2 == 0) goto finish2;
	const int cmp_2_3 = cmp(*from2, *from3);
	if (cmp_2_3 <  0) goto state_2lt3lt0lt1;
	if (cmp_2_3 == 0) goto state_2eq3lt0lt1;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_3lt2lt0lt1;
	if (cmp_2_0 == 0) goto state_3lt2eq0lt1;
	const int cmp_2_1 = cmp(*from2, *from1);
	if (cmp_2_1 <  0) goto state_3lt0lt2lt1;
	if (cmp_2_1 == 0) goto state_3lt0lt2eq1;
	goto state_3lt0lt1lt2;
}
state_2lt3lt1lt0:
{
	debug() << "\tstate_2lt3lt1lt0\n";
	assert(cmp(*from2, *from3) <  0);
	assert(cmp(*from3, *from1) <  0);
	assert(cmp(*from1, *from0) <  0);
	*result++ = *from2++;
	if (--n2 == 0) goto finish2;
	const int cmp_2_3 = cmp(*from2, *from3);
	if (cmp_2_3 <  0) goto state_2lt3lt1lt0;
	if (cmp_2_3 == 0) goto state_2eq3lt1lt0;
	const int cmp_2_1 = cmp(*from2, *from1);
	if (cmp_2_1 <  0) goto state_3lt2lt1lt0;
	if (cmp_2_1 == 0) goto state_3lt1eq2lt0;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_3lt1lt2lt0;
	if (cmp_2_0 == 0) goto state_3lt1lt2eq0;
	goto state_3lt1lt0lt2;
}
state_3lt0lt1lt2:
{
	debug() << "\tstate_3lt0lt1lt2\n";
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from1) <  0);
	assert(cmp(*from1, *from2) <  0);
	*result++ = *from3++;
	if (--n3 == 0) goto finish3;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_3lt0lt1lt2;
	if (cmp_3_0 == 0) goto state_3eq0lt1lt2;
	const int cmp_3_1 = cmp(*from3, *from1);
	if (cmp_3_1 <  0) goto state_0lt3lt1lt2;
	if (cmp_3_1 == 0) goto state_0lt3eq1lt2;
	const int cmp_3_2 = cmp(*from3, *from2);
	if (cmp_3_2 <  0) goto state_0lt1lt3lt2;
	if (cmp_3_2 == 0) goto state_0lt1lt3eq2;
	goto state_0lt1lt2lt3;
}
state_3lt0lt2lt1:
{
	debug() << "\tstate_3lt0lt2lt1\n";
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from2) <  0);
	assert(cmp(*from2, *from1) <  0);
	*result++ = *from3++;
	if (--n3 == 0) goto finish3;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_3lt0lt2lt1;
	if (cmp_3_0 == 0) goto state_3eq0lt2lt1;
	const int cmp_3_2 = cmp(*from3, *from2);
	if (cmp_3_2 <  0) goto state_0lt3lt2lt1;
	if (cmp_3_2 == 0) goto state_0lt2eq3lt1;
	const int cmp_3_1 = cmp(*from3, *from1);
	if (cmp_3_1 <  0) goto state_0lt2lt3lt1;
	if (cmp_3_1 == 0) goto state_0lt2lt3eq1;
	goto state_0lt2lt1lt3;
}
state_3lt1lt0lt2:
{
	debug() << "\tstate_3lt1lt0lt2\n";
	assert(cmp(*from3, *from1) <  0);
	assert(cmp(*from1, *from0) <  0);
	assert(cmp(*from0, *from2) <  0);
	*result++ = *from3++;
	if (--n3 == 0) goto finish3;
	const int cmp_3_1 = cmp(*from3, *from1);
	if (cmp_3_1 <  0) goto state_3lt1lt0lt2;
	if (cmp_3_1 == 0) goto state_3eq1lt0lt2;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_1lt3lt0lt2;
	if (cmp_3_0 == 0) goto state_1lt3eq0lt2;
	const int cmp_3_2 = cmp(*from3, *from2);
	if (cmp_3_2 <  0) goto state_1lt0lt3lt2;
	if (cmp_3_2 == 0) goto state_1lt0lt3eq2;
	goto state_1lt0lt2lt3;
}
state_3lt1lt2lt0:
{
	debug() << "\tstate_3lt1lt2lt0\n";
	assert(cmp(*from3, *from1) <  0);
	assert(cmp(*from1, *from2) <  0);
	assert(cmp(*from2, *from0) <  0);
	*result++ = *from3++;
	if (--n3 == 0) goto finish3;
	const int cmp_3_1 = cmp(*from3, *from1);
	if (cmp_3_1 <  0) goto state_3lt1lt2lt0;
	if (cmp_3_1 == 0) goto state_1eq3lt2lt0;
	const int cmp_3_2 = cmp(*from3, *from2);
	if (cmp_3_2 <  0) goto state_1lt3lt2lt0;
	if (cmp_3_2 == 0) goto state_1lt3eq2lt0;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_1lt2lt3lt0;
	if (cmp_3_0 == 0) goto state_1lt2lt3eq0;
	goto state_1lt2lt0lt3;
}
state_3lt2lt0lt1:
{
	debug() << "\tstate_3lt2lt0lt1\n";
	assert(cmp(*from3, *from2) <  0);
	assert(cmp(*from2, *from0) <  0);
	assert(cmp(*from0, *from1) <  0);
	*result++ = *from3++;
	if (--n3 == 0) goto finish3;
	const int cmp_3_2 = cmp(*from3, *from2);
	if (cmp_3_2 <  0) goto state_3lt2lt0lt1;
	if (cmp_3_2 == 0) goto state_3eq2lt0lt1;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_2lt3lt0lt1;
	if (cmp_3_0 == 0) goto state_2lt3eq0lt1;
	const int cmp_3_1 = cmp(*from3, *from1);
	if (cmp_3_1 <  0) goto state_2lt0lt3lt1;
	if (cmp_3_1 == 0) goto state_2lt0lt3eq1;
	goto state_2lt0lt1lt3;
}
state_3lt2lt1lt0:
{
	debug() << "\tstate_3lt2lt1lt0\n";
	assert(cmp(*from3, *from2) <  0);
	assert(cmp(*from2, *from1) <  0);
	assert(cmp(*from1, *from0) <  0);
	*result++ = *from3++;
	if (--n3 == 0) goto finish3;
	const int cmp_3_2 = cmp(*from3, *from2);
	if (cmp_3_2 <  0) goto state_3lt2lt1lt0;
	if (cmp_3_2 == 0) goto state_2eq3lt1lt0;
	const int cmp_3_1 = cmp(*from3, *from1);
	if (cmp_3_1 <  0) goto state_2lt3lt1lt0;
	if (cmp_3_1 == 0) goto state_2lt3eq1lt0;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_2lt1lt3lt0;
	if (cmp_3_0 == 0) goto state_2lt1lt3eq0;
	goto state_2lt1lt0lt3;
}
state_0eq1lt2lt3:
{
	debug() << "\tstate_0eq1lt2lt3\n";
	assert(cmp(*from0, *from1) == 0);
	assert(cmp(*from1, *from2) <  0);
	assert(cmp(*from2, *from3) <  0);
	*result++ = *from0++;
	*result++ = *from1++;
	--n0;
	--n1;
	if (n0 == 0) goto finish;
	if (n1 == 0) goto finish;
	goto initial;
}
state_0eq1eq2lt3:
{
	debug() << "\tstate_0eq1eq2lt3\n";
	assert(cmp(*from0, *from1) == 0);
	assert(cmp(*from1, *from2) == 0);
	assert(cmp(*from2, *from3) <  0);
	*result++ = *from0++;
	*result++ = *from1++;
	*result++ = *from2++;
	--n0;
	--n1;
	--n2;
	if (n0 == 0) goto finish;
	if (n1 == 0) goto finish;
	if (n2 == 0) goto finish;
	goto initial;
}
state_0eq1eq2eq3:
{
	debug() << "\tstate_0eq1eq2eq3\n";
	assert(cmp(*from0, *from1) == 0);
	assert(cmp(*from1, *from2) == 0);
	assert(cmp(*from2, *from3) == 0);
	*result++ = *from0++;
	*result++ = *from1++;
	*result++ = *from2++;
	*result++ = *from3++;
	--n0;
	--n1;
	--n2;
	--n3;
	if (n0 == 0) goto finish;
	if (n1 == 0) goto finish;
	if (n2 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_0eq1lt2eq3:
{
	debug() << "\tstate_0eq1lt2eq3\n";
	assert(cmp(*from0, *from1) == 0);
	assert(cmp(*from1, *from2) <  0);
	assert(cmp(*from2, *from3) == 0);
	*result++ = *from0++;
	*result++ = *from1++;
	--n0;
	--n1;
	if (n0 == 0) goto finish;
	if (n1 == 0) goto finish;
	goto initial;
}
state_0lt1eq2lt3:
{
	debug() << "\tstate_0lt1eq2lt3\n";
	assert(cmp(*from0, *from1) <  0);
	assert(cmp(*from1, *from2) == 0);
	assert(cmp(*from2, *from3) <  0);
	*result++ = *from0++;
	--n0;
	if (n0 == 0) goto finish;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_0lt1eq2lt3;
	if (cmp_0_1 == 0) goto state_0eq1eq2lt3;
	const int cmp_0_3 = cmp(*from0, *from3);
	if (cmp_0_3 <  0) goto state_1eq2lt0lt3;
	if (cmp_0_3 == 0) goto state_1eq2lt0eq3;
	goto state_1eq2lt3lt0;
}
state_0lt1eq2eq3:
{
	debug() << "\tstate_0lt1eq2eq3\n";
	assert(cmp(*from0, *from1) <  0);
	assert(cmp(*from1, *from2) == 0);
	assert(cmp(*from2, *from3) == 0);
	*result++ = *from0++;
	--n0;
	if (n0 == 0) goto finish;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_0lt1eq2eq3;
	if (cmp_0_1 == 0) goto state_0eq1eq2eq3;
	goto state_1eq2eq3lt0;
}
state_0lt1lt2eq3:
{
	debug() << "\tstate_0lt1lt2eq3\n";
	assert(cmp(*from0, *from1) <  0);
	assert(cmp(*from1, *from2) <  0);
	assert(cmp(*from2, *from3) == 0);
	*result++ = *from0++;
	--n0;
	if (n0 == 0) goto finish;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_0lt1lt2eq3;
	if (cmp_0_1 == 0) goto state_0eq1lt2eq3;
	const int cmp_0_2 = cmp(*from0, *from2);
	if (cmp_0_2 <  0) goto state_1lt0lt2eq3;
	if (cmp_0_2 == 0) goto state_1lt0eq2eq3;
	goto state_1lt2eq3lt0;
}
state_0eq1lt3lt2:
{
	debug() << "\tstate_0eq1lt3lt2\n";
	assert(cmp(*from0, *from1) == 0);
	assert(cmp(*from1, *from3) <  0);
	assert(cmp(*from3, *from2) <  0);
	*result++ = *from0++;
	*result++ = *from1++;
	--n0;
	--n1;
	if (n0 == 0) goto finish;
	if (n1 == 0) goto finish;
	goto initial;
}
state_0eq1eq3lt2:
{
	debug() << "\tstate_0eq1eq3lt2\n";
	assert(cmp(*from0, *from1) == 0);
	assert(cmp(*from1, *from3) == 0);
	assert(cmp(*from3, *from2) <  0);
	*result++ = *from0++;
	*result++ = *from1++;
	*result++ = *from3++;
	--n0;
	--n1;
	--n3;
	if (n0 == 0) goto finish;
	if (n1 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_0eq1lt3eq2: goto state_0eq1lt2eq3;
state_0lt1eq3lt2:
{
	debug() << "\tstate_0lt1eq3lt2\n";
	assert(cmp(*from0, *from1) <  0);
	assert(cmp(*from1, *from3) == 0);
	assert(cmp(*from3, *from2) <  0);
	*result++ = *from0++;
	--n0;
	if (n0 == 0) goto finish;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_0lt1eq3lt2;
	if (cmp_0_1 == 0) goto state_0eq1eq3lt2;
	const int cmp_0_2 = cmp(*from0, *from2);
	if (cmp_0_2 <  0) goto state_1eq3lt0lt2;
	if (cmp_0_2 == 0) goto state_1eq3lt0eq2;
	goto state_1eq3lt2lt0;
}
state_0lt1lt3eq2: goto state_0lt1lt2eq3;
state_0eq2lt1lt3:
{
	debug() << "\tstate_0eq2lt1lt3\n";
	assert(cmp(*from0, *from2) == 0);
	assert(cmp(*from2, *from1) <  0);
	assert(cmp(*from1, *from3) <  0);
	*result++ = *from0++;
	*result++ = *from2++;
	--n0;
	--n2;
	if (n0 == 0) goto finish;
	if (n2 == 0) goto finish;
	goto initial;
}
state_0eq2lt1eq3:
{
	debug() << "\tstate_0eq2lt1eq3\n";
	assert(cmp(*from0, *from2) == 0);
	assert(cmp(*from2, *from1) <  0);
	assert(cmp(*from1, *from3) == 0);
	*result++ = *from0++;
	*result++ = *from2++;
	--n0;
	--n2;
	if (n0 == 0) goto finish;
	if (n2 == 0) goto finish;
	goto initial;
}
state_0lt2eq1lt3: goto state_0lt1eq2lt3;
state_0lt2eq1eq3: goto state_0lt1eq2eq3;
state_0lt2lt1eq3:
{
	debug() << "\tstate_0lt2lt1eq3\n";
	assert(cmp(*from0, *from2) <  0);
	assert(cmp(*from2, *from1) <  0);
	assert(cmp(*from1, *from3) == 0);
	*result++ = *from0++;
	--n0;
	if (n0 == 0) goto finish;
	const int cmp_0_2 = cmp(*from0, *from2);
	if (cmp_0_2 <  0) goto state_0lt2lt1eq3;
	if (cmp_0_2 == 0) goto state_0eq2lt1eq3;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_2lt0lt1eq3;
	if (cmp_0_1 == 0) goto state_2lt0eq1eq3;
	goto state_2lt1eq3lt0;
}
state_0eq2lt3lt1:
{
	debug() << "\tstate_0eq2lt3lt1\n";
	assert(cmp(*from0, *from2) == 0);
	assert(cmp(*from2, *from3) <  0);
	assert(cmp(*from3, *from1) <  0);
	*result++ = *from0++;
	*result++ = *from2++;
	--n0;
	--n2;
	if (n0 == 0) goto finish;
	if (n2 == 0) goto finish;
	goto initial;
}
state_0eq2eq3lt1:
{
	debug() << "\tstate_0eq2eq3lt1\n";
	assert(cmp(*from0, *from2) == 0);
	assert(cmp(*from2, *from3) == 0);
	assert(cmp(*from3, *from1) <  0);
	*result++ = *from0++;
	*result++ = *from2++;
	*result++ = *from3++;
	--n0;
	--n2;
	--n3;
	if (n0 == 0) goto finish;
	if (n2 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_0eq2lt3eq1: goto state_0eq2lt1eq3;
state_0lt2eq3lt1:
{
	debug() << "\tstate_0lt2eq3lt1\n";
	assert(cmp(*from0, *from2) <  0);
	assert(cmp(*from2, *from3) == 0);
	assert(cmp(*from3, *from1) <  0);
	*result++ = *from0++;
	--n0;
	if (n0 == 0) goto finish;
	const int cmp_0_2 = cmp(*from0, *from2);
	if (cmp_0_2 <  0) goto state_0lt2eq3lt1;
	if (cmp_0_2 == 0) goto state_0eq2eq3lt1;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_2eq3lt0lt1;
	if (cmp_0_1 == 0) goto state_2eq3lt0eq1;
	goto state_2eq3lt1lt0;
}
state_0lt2lt3eq1: goto state_0lt2lt1eq3;
state_0eq3lt1lt2:
{
	debug() << "\tstate_0eq3lt1lt2\n";
	assert(cmp(*from0, *from3) == 0);
	assert(cmp(*from3, *from1) <  0);
	assert(cmp(*from1, *from2) <  0);
	*result++ = *from0++;
	*result++ = *from3++;
	--n0;
	--n3;
	if (n0 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_0eq3lt1eq2:
{
	debug() << "\tstate_0eq3lt1eq2\n";
	assert(cmp(*from0, *from3) == 0);
	assert(cmp(*from3, *from1) <  0);
	assert(cmp(*from1, *from2) == 0);
	*result++ = *from0++;
	*result++ = *from3++;
	--n0;
	--n3;
	if (n0 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_0lt3eq1lt2: goto state_0lt1eq3lt2;
state_0lt3eq1eq2: goto state_0lt1eq2eq3;
state_0lt3lt1eq2:
{
	debug() << "\tstate_0lt3lt1eq2\n";
	assert(cmp(*from0, *from3) <  0);
	assert(cmp(*from3, *from1) <  0);
	assert(cmp(*from1, *from2) == 0);
	*result++ = *from0++;
	--n0;
	if (n0 == 0) goto finish;
	const int cmp_0_3 = cmp(*from0, *from3);
	if (cmp_0_3 <  0) goto state_0lt3lt1eq2;
	if (cmp_0_3 == 0) goto state_0eq3lt1eq2;
	const int cmp_0_1 = cmp(*from0, *from1);
	if (cmp_0_1 <  0) goto state_3lt0lt1eq2;
	if (cmp_0_1 == 0) goto state_3lt0eq1eq2;
	goto state_3lt1eq2lt0;
}
state_0eq3lt2lt1:
{
	debug() << "\tstate_0eq3lt2lt1\n";
	assert(cmp(*from0, *from3) == 0);
	assert(cmp(*from3, *from2) <  0);
	assert(cmp(*from2, *from1) <  0);
	*result++ = *from0++;
	*result++ = *from3++;
	--n0;
	--n3;
	if (n0 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_1lt0eq2lt3:
{
	debug() << "\tstate_1lt0eq2lt3\n";
	assert(cmp(*from1, *from0) <  0);
	assert(cmp(*from0, *from2) == 0);
	assert(cmp(*from2, *from3) <  0);
	*result++ = *from1++;
	--n1;
	if (n1 == 0) goto finish;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_1lt0eq2lt3;
	if (cmp_1_0 == 0) goto state_0eq1eq2lt3;
	const int cmp_1_3 = cmp(*from1, *from3);
	if (cmp_1_3 <  0) goto state_0eq2lt1lt3;
	if (cmp_1_3 == 0) goto state_0eq2lt1eq3;
	goto state_0eq2lt3lt1;
}
state_1lt0eq2eq3:
{
	debug() << "\tstate_1lt0eq2eq3\n";
	assert(cmp(*from1, *from0) <  0);
	assert(cmp(*from0, *from2) == 0);
	assert(cmp(*from2, *from3) == 0);
	*result++ = *from1++;
	--n1;
	if (n1 == 0) goto finish;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_1lt0eq2eq3;
	if (cmp_1_0 == 0) goto state_0eq1eq2eq3;
	goto state_0eq2eq3lt1;
}
state_1lt0lt2eq3:
{
	debug() << "\tstate_1lt0lt2eq3\n";
	assert(cmp(*from1, *from0) <  0);
	assert(cmp(*from0, *from2) <  0);
	assert(cmp(*from2, *from3) == 0);
	*result++ = *from1++;
	--n1;
	if (n1 == 0) goto finish;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_1lt0lt2eq3;
	if (cmp_1_0 == 0) goto state_0eq1lt2eq3;
	const int cmp_1_2 = cmp(*from1, *from2);
	if (cmp_1_2 <  0) goto state_0lt1lt2eq3;
	if (cmp_1_2 == 0) goto state_0lt1eq2eq3;
	goto state_0lt2eq3lt1;
}
state_1eq0lt3lt2: goto state_0eq1lt3lt2;
state_1eq0eq3lt2: goto state_0eq1eq3lt2;
state_1lt0eq3lt2:
{
	debug() << "\tstate_1lt0eq3lt2\n";
	assert(cmp(*from1, *from0) <  0);
	assert(cmp(*from0, *from3) == 0);
	assert(cmp(*from3, *from2) <  0);
	*result++ = *from1++;
	--n1;
	if (n1 == 0) goto finish;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_1lt0eq3lt2;
	if (cmp_1_0 == 0) goto state_1eq0eq3lt2;
	const int cmp_1_2 = cmp(*from1, *from2);
	if (cmp_1_2 <  0) goto state_0eq3lt1lt2;
	if (cmp_1_2 == 0) goto state_0eq3lt1eq2;
	goto state_0eq3lt2lt1;
}
state_1lt0lt3eq2: goto state_1lt0lt2eq3;
state_1eq2lt0lt3:
{
	debug() << "\tstate_1eq2lt0lt3\n";
	assert(cmp(*from1, *from2) == 0);
	assert(cmp(*from2, *from0) <  0);
	assert(cmp(*from0, *from3) <  0);
	*result++ = *from1++;
	*result++ = *from2++;
	--n1;
	--n2;
	if (n1 == 0) goto finish;
	if (n2 == 0) goto finish;
	goto initial;
}
state_1eq2lt0eq3:
{
	debug() << "\tstate_1eq2lt0eq3\n";
	assert(cmp(*from1, *from2) == 0);
	assert(cmp(*from2, *from0) <  0);
	assert(cmp(*from0, *from3) == 0);
	*result++ = *from1++;
	*result++ = *from2++;
	--n1;
	--n2;
	if (n1 == 0) goto finish;
	if (n2 == 0) goto finish;
	goto initial;
}
state_1lt2eq0lt3: goto state_1lt0eq2lt3;
state_1lt2eq0eq3: goto state_1lt0eq2eq3;
state_1lt2lt0eq3:
{
	debug() << "\tstate_1lt2lt0eq3\n";
	assert(cmp(*from1, *from2) <  0);
	assert(cmp(*from2, *from0) <  0);
	assert(cmp(*from0, *from3) == 0);
	*result++ = *from1++;
	--n1;
	if (n1 == 0) goto finish;
	const int cmp_1_2 = cmp(*from1, *from2);
	if (cmp_1_2 <  0) goto state_1lt2lt0eq3;
	if (cmp_1_2 == 0) goto state_1eq2lt0eq3;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_2lt1lt0eq3;
	if (cmp_1_0 == 0) goto state_2lt1eq0eq3;
	goto state_2lt0eq3lt1;
}
state_1eq2lt3lt0:
{
	debug() << "\tstate_1eq2lt3lt0\n";
	assert(cmp(*from1, *from2) == 0);
	assert(cmp(*from2, *from3) <  0);
	assert(cmp(*from3, *from0) <  0);
	*result++ = *from1++;
	*result++ = *from2++;
	--n1;
	--n2;
	if (n1 == 0) goto finish;
	if (n2 == 0) goto finish;
	goto initial;
}
state_1eq2eq3lt0:
{
	debug() << "\tstate_1eq2eq3lt0\n";
	assert(cmp(*from1, *from2) == 0);
	assert(cmp(*from2, *from3) == 0);
	assert(cmp(*from3, *from0) <  0);
	*result++ = *from1++;
	*result++ = *from2++;
	*result++ = *from3++;
	--n1;
	--n2;
	--n3;
	if (n1 == 0) goto finish;
	if (n2 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_1eq2lt3eq0: goto state_1eq2lt0eq3;
state_1lt2eq3lt0:
{
	debug() << "\tstate_1lt2eq3lt0\n";
	assert(cmp(*from1, *from2) <  0);
	assert(cmp(*from2, *from3) == 0);
	assert(cmp(*from3, *from0) <  0);
	*result++ = *from1++;
	--n1;
	if (n1 == 0) goto finish;
	const int cmp_1_2 = cmp(*from1, *from2);
	if (cmp_1_2 <  0) goto state_1lt2eq3lt0;
	if (cmp_1_2 == 0) goto state_1eq2eq3lt0;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_2eq3lt1lt0;
	if (cmp_1_0 == 0) goto state_2eq3lt1eq0;
	goto state_2eq3lt0lt1;
}
state_1lt2lt3eq0: goto state_1lt2lt0eq3;
state_1eq3lt0lt2:
{
	debug() << "\tstate_1eq3lt0lt2\n";
	assert(cmp(*from1, *from3) == 0);
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from2) <  0);
	*result++ = *from1++;
	*result++ = *from3++;
	--n1;
	--n3;
	if (n1 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_1eq3lt0eq2:
{
	debug() << "\tstate_1eq3lt0eq2\n";
	assert(cmp(*from1, *from3) == 0);
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from2) == 0);
	*result++ = *from1++;
	*result++ = *from3++;
	--n1;
	--n3;
	if (n1 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_1lt3eq0lt2: goto state_1lt0eq3lt2;
state_1lt3eq0eq2: goto state_1lt0eq2eq3;
state_1lt3lt0eq2:
{
	debug() << "\tstate_1lt3lt0eq2\n";
	assert(cmp(*from1, *from3) <  0);
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from2) == 0);
	*result++ = *from1++;
	--n1;
	if (n1 == 0) goto finish;
	const int cmp_1_3 = cmp(*from1, *from3);
	if (cmp_1_3 <  0) goto state_1lt3lt0eq2;
	if (cmp_1_3 == 0) goto state_1eq3lt0eq2;
	const int cmp_1_0 = cmp(*from1, *from0);
	if (cmp_1_0 <  0) goto state_3lt1lt0eq2;
	if (cmp_1_0 == 0) goto state_3lt1eq0eq2;
	goto state_3lt0eq2lt1;
}
state_1eq3lt2lt0:
{
	debug() << "\tstate_1eq3lt2lt0\n";
	assert(cmp(*from1, *from3) == 0);
	assert(cmp(*from3, *from2) <  0);
	assert(cmp(*from2, *from0) <  0);
	*result++ = *from1++;
	*result++ = *from3++;
	--n1;
	--n3;
	if (n1 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_1eq3lt2eq0: goto state_1eq3lt0eq2;
state_1lt3eq2lt0: goto state_1lt2eq3lt0;
state_1lt3lt2eq0: goto state_1lt3lt0eq2;
state_2eq0lt1lt3: goto state_0eq2lt1lt3;
state_2eq0eq1lt3: goto state_0eq1eq2lt3;
state_2eq0eq1eq3: goto state_0eq1eq2eq3;
state_2eq0lt1eq3: goto state_0eq2lt1eq3;
state_2lt0eq1lt3:
{
	debug() << "\tstate_2lt0eq1lt3\n";
	assert(cmp(*from2, *from0) <  0);
	assert(cmp(*from0, *from1) == 0);
	assert(cmp(*from1, *from3) <  0);
	*result++ = *from2++;
	--n2;
	if (n2 == 0) goto finish;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_2lt0eq1lt3;
	if (cmp_2_0 == 0) goto state_2eq0eq1lt3;
	const int cmp_2_3 = cmp(*from2, *from3);
	if (cmp_2_3 <  0) goto state_0eq1lt2lt3;
	if (cmp_2_3 == 0) goto state_0eq1lt2eq3;
	goto state_0eq1lt3lt2;
}
state_2lt0eq1eq3:
{
	debug() << "\tstate_2lt0eq1eq3\n";
	assert(cmp(*from2, *from0) <  0);
	assert(cmp(*from0, *from1) == 0);
	assert(cmp(*from1, *from3) == 0);
	*result++ = *from2++;
	--n2;
	if (n2 == 0) goto finish;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_2lt0eq1eq3;
	if (cmp_2_0 == 0) goto state_2eq0eq1eq3;
	goto state_0eq1eq3lt2;
}
state_2lt0lt1eq3:
{
	debug() << "\tstate_2lt0lt1eq3\n";
	assert(cmp(*from2, *from0) <  0);
	assert(cmp(*from0, *from1) <  0);
	assert(cmp(*from1, *from3) == 0);
	*result++ = *from2++;
	--n2;
	if (n2 == 0) goto finish;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_2lt0lt1eq3;
	if (cmp_2_0 == 0) goto state_2eq0lt1eq3;
	const int cmp_2_1 = cmp(*from2, *from1);
	if (cmp_2_1 <  0) goto state_0lt2lt1eq3;
	if (cmp_2_1 == 0) goto state_0lt2eq1eq3;
	goto state_0lt1eq3lt2;
}
state_2eq0lt3lt1: goto state_0eq2lt3lt1;
state_2eq0eq3lt1: goto state_0eq2eq3lt1;
state_2lt0eq3lt1:
{
	debug() << "\tstate_2lt0eq3lt1\n";
	assert(cmp(*from2, *from0) <  0);
	assert(cmp(*from0, *from3) == 0);
	assert(cmp(*from3, *from1) <  0);
	*result++ = *from2++;
	--n2;
	if (n2 == 0) goto finish;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_2lt0eq3lt1;
	if (cmp_2_0 == 0) goto state_2eq0eq3lt1;
	const int cmp_2_1 = cmp(*from2, *from1);
	if (cmp_2_1 <  0) goto state_0eq3lt2lt1;
	if (cmp_2_1 == 0) goto state_0eq3lt1eq2;
	goto state_0eq3lt1lt2;
}
state_2lt0lt3eq1: goto state_2lt0lt1eq3;
state_2eq1lt0lt3: goto state_1eq2lt0lt3;
state_2eq1lt0eq3: goto state_1eq2lt0eq3;
state_2lt1eq0lt3: goto state_2lt0eq1lt3;
state_2lt1eq0eq3: goto state_2lt0eq1eq3;
state_2lt1lt0eq3:
{
	debug() << "\tstate_2lt1lt0eq3\n";
	assert(cmp(*from2, *from1) <  0);
	assert(cmp(*from1, *from0) <  0);
	assert(cmp(*from0, *from3) == 0);
	*result++ = *from2++;
	--n2;
	if (n2 == 0) goto finish;
	const int cmp_2_1 = cmp(*from2, *from1);
	if (cmp_2_1 <  0) goto state_2lt1lt0eq3;
	if (cmp_2_1 == 0) goto state_2eq1lt0eq3;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_1lt2lt0eq3;
	if (cmp_2_0 == 0) goto state_1lt2eq0eq3;
	goto state_1lt0eq3lt2;
}
state_2eq1lt3lt0: goto state_1eq2lt3lt0;
state_2eq1eq3lt0: goto state_1eq2eq3lt0;
state_2lt1eq3lt0:
{
	debug() << "\tstate_2lt1eq3lt0\n";
	assert(cmp(*from2, *from1) <  0);
	assert(cmp(*from1, *from3) == 0);
	assert(cmp(*from3, *from0) <  0);
	*result++ = *from2++;
	--n2;
	if (n2 == 0) goto finish;
	const int cmp_2_1 = cmp(*from2, *from1);
	if (cmp_2_1 <  0) goto state_2lt1eq3lt0;
	if (cmp_2_1 == 0) goto state_2eq1eq3lt0;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_1eq3lt2lt0;
	if (cmp_2_0 == 0) goto state_1eq3lt2eq0;
	goto state_1eq3lt0lt2;
}
state_2lt1lt3eq0: goto state_2lt1lt0eq3;
state_2eq3lt0lt1:
{
	debug() << "\tstate_2eq3lt0lt1\n";
	assert(cmp(*from2, *from3) == 0);
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from1) <  0);
	*result++ = *from2++;
	*result++ = *from3++;
	--n2;
	--n3;
	if (n2 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_2eq3lt0eq1:
{
	debug() << "\tstate_2eq3lt0eq1\n";
	assert(cmp(*from2, *from3) == 0);
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from1) == 0);
	*result++ = *from2++;
	*result++ = *from3++;
	--n2;
	--n3;
	if (n2 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_2lt3eq0lt1: goto state_2lt0eq3lt1;
state_2lt3eq0eq1: goto state_2lt0eq1eq3;
state_2lt3lt0eq1:
{
	debug() << "\tstate_2lt3lt0eq1\n";
	assert(cmp(*from2, *from3) <  0);
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from1) == 0);
	*result++ = *from2++;
	--n2;
	if (n2 == 0) goto finish;
	const int cmp_2_3 = cmp(*from2, *from3);
	if (cmp_2_3 <  0) goto state_2lt3lt0eq1;
	if (cmp_2_3 == 0) goto state_2eq3lt0eq1;
	const int cmp_2_0 = cmp(*from2, *from0);
	if (cmp_2_0 <  0) goto state_3lt2lt0eq1;
	if (cmp_2_0 == 0) goto state_3lt2eq0eq1;
	goto state_3lt0eq1lt2;
}
state_2eq3lt1lt0:
{
	debug() << "\tstate_2eq3lt1lt0\n";
	assert(cmp(*from2, *from3) == 0);
	assert(cmp(*from3, *from1) <  0);
	assert(cmp(*from1, *from0) <  0);
	*result++ = *from2++;
	*result++ = *from3++;
	--n2;
	--n3;
	if (n2 == 0) goto finish;
	if (n3 == 0) goto finish;
	goto initial;
}
state_2eq3lt1eq0: goto state_2eq3lt0eq1;
state_2lt3eq1lt0: goto state_2lt1eq3lt0;
state_2lt3lt1eq0: goto state_2lt3lt0eq1;
state_3eq0lt1lt2: goto state_0eq3lt1lt2;
state_3eq0eq1lt2: goto state_0eq1eq3lt2;
state_3eq0eq1eq2: goto state_0eq1eq2eq3;
state_3eq0lt1eq2: goto state_0eq3lt1eq2;
state_3lt0eq1lt2:
{
	debug() << "\tstate_3lt0eq1lt2\n";
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from1) == 0);
	assert(cmp(*from1, *from2) <  0);
	*result++ = *from3++;
	--n3;
	if (n3 == 0) goto finish;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_3lt0eq1lt2;
	if (cmp_3_0 == 0) goto state_3eq0eq1lt2;
	const int cmp_3_2 = cmp(*from3, *from2);
	if (cmp_3_2 <  0) goto state_0eq1lt3lt2;
	if (cmp_3_2 == 0) goto state_0eq1lt3eq2;
	goto state_0eq1lt2lt3;
}
state_3lt0eq1eq2:
{
	debug() << "\tstate_3lt0eq1eq2\n";
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from1) == 0);
	assert(cmp(*from1, *from2) == 0);
	*result++ = *from3++;
	--n3;
	if (n3 == 0) goto finish;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_3lt0eq1eq2;
	if (cmp_3_0 == 0) goto state_3eq0eq1eq2;
	goto state_0eq1eq2lt3;
}
state_3lt0lt1eq2:
{
	debug() << "\tstate_3lt0lt1eq2\n";
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from1) <  0);
	assert(cmp(*from1, *from2) == 0);
	*result++ = *from3++;
	--n3;
	if (n3 == 0) goto finish;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_3lt0lt1eq2;
	if (cmp_3_0 == 0) goto state_3eq0lt1eq2;
	const int cmp_3_1 = cmp(*from3, *from1);
	if (cmp_3_1 <  0) goto state_0lt3lt1eq2;
	if (cmp_3_1 == 0) goto state_0lt3eq1eq2;
	goto state_0lt1eq2lt3;
}
state_3eq0lt2lt1: goto state_0eq3lt2lt1;
state_3eq0eq2lt1: goto state_0eq2eq3lt1;
state_3lt0eq2lt1:
{
	debug() << "\tstate_3lt0eq2lt1\n";
	assert(cmp(*from3, *from0) <  0);
	assert(cmp(*from0, *from2) == 0);
	assert(cmp(*from2, *from1) <  0);
	*result++ = *from3++;
	--n3;
	if (n3 == 0) goto finish;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_3lt0eq2lt1;
	if (cmp_3_0 == 0) goto state_3eq0eq2lt1;
	const int cmp_3_1 = cmp(*from3, *from1);
	if (cmp_3_1 <  0) goto state_0eq2lt3lt1;
	if (cmp_3_1 == 0) goto state_0eq2lt3eq1;
	goto state_0eq2lt1lt3;
}
state_3lt0lt2eq1: goto state_3lt0lt1eq2;
state_3eq1lt0lt2: goto state_1eq3lt0lt2;
state_3eq1lt0eq2: goto state_1eq3lt0eq2;
state_3lt1eq0lt2: goto state_3lt0eq1lt2;
state_3lt1eq0eq2: goto state_3lt0eq1eq2;
state_3lt1lt0eq2:
{
	debug() << "\tstate_3lt1lt0eq2\n";
	assert(cmp(*from3, *from1) <  0);
	assert(cmp(*from1, *from0) <  0);
	assert(cmp(*from0, *from2) == 0);
	*result++ = *from3++;
	--n3;
	if (n3 == 0) goto finish;
	const int cmp_3_1 = cmp(*from3, *from1);
	if (cmp_3_1 <  0) goto state_3lt1lt0eq2;
	if (cmp_3_1 == 0) goto state_3eq1lt0eq2;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_1lt3lt0eq2;
	if (cmp_3_0 == 0) goto state_1lt3eq0eq2;
	goto state_1lt0eq2lt3;
}
state_3lt1eq2lt0:
{
	debug() << "\tstate_3lt1eq2lt0\n";
	assert(cmp(*from3, *from1) <  0);
	assert(cmp(*from1, *from2) == 0);
	assert(cmp(*from2, *from0) <  0);
	*result++ = *from3++;
	--n3;
	if (n3 == 0) goto finish;
	const int cmp_3_1 = cmp(*from3, *from1);
	if (cmp_3_1 <  0) goto state_3lt1eq2lt0;
	if (cmp_3_1 == 0) goto state_1eq2eq3lt0;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_1eq2lt3lt0;
	if (cmp_3_0 == 0) goto state_1eq2lt3eq0;
	goto state_1eq2lt0lt3;
}
state_3lt1lt2eq0: goto state_3lt1lt0eq2;
state_3eq2lt0lt1: goto state_2eq3lt0lt1;
state_3eq2lt0eq1: goto state_2eq3lt0eq1;
state_3lt2eq0lt1: goto state_3lt0eq2lt1;
state_3lt2eq0eq1: goto state_3lt0eq1eq2;
state_3lt2lt0eq1:
{
	debug() << "\tstate_3lt2lt0eq1\n";
	assert(cmp(*from3, *from2) <  0);
	assert(cmp(*from2, *from0) <  0);
	assert(cmp(*from0, *from1) == 0);
	*result++ = *from3++;
	--n3;
	if (n3 == 0) goto finish;
	const int cmp_3_2 = cmp(*from3, *from2);
	if (cmp_3_2 <  0) goto state_3lt2lt0eq1;
	if (cmp_3_2 == 0) goto state_3eq2lt0eq1;
	const int cmp_3_0 = cmp(*from3, *from0);
	if (cmp_3_0 <  0) goto state_2lt3lt0eq1;
	if (cmp_3_0 == 0) goto state_2lt3eq0eq1;
	goto state_2lt0eq1lt3;
}

finish0:
finish1:
finish2:
finish3:
finish:
	std::vector<std::pair<unsigned char**, size_t> > nonempty;
	if (n0) nonempty.push_back(std::make_pair(from0, n0));
	if (n1) nonempty.push_back(std::make_pair(from1, n1));
	if (n2) nonempty.push_back(std::make_pair(from2, n2));
	if (n3) nonempty.push_back(std::make_pair(from3, n3));
	assert(nonempty.size()!=4);
	switch (nonempty.size()) {
	case 3: merge_3way_unstable(nonempty[0].first, nonempty[0].second,
		                    nonempty[1].first, nonempty[1].second,
		                    nonempty[2].first, nonempty[2].second,
		                    result);
		break;
	case 2: merge_2way_unstable(nonempty[0].first, nonempty[0].second,
		                    nonempty[1].first, nonempty[1].second,
		                    result);
		break;
	case 1: (void) memcpy(result, nonempty[0].first,
				(nonempty[0].second)*sizeof(unsigned char*));
		break;
	default:
		break;
	};
	debug()<< "~" << __func__ << "\n";
}
void
mergesort_4way_unstable(unsigned char** strings, size_t n, unsigned char** tmp)
{
	//debug() << __func__ << "(), n="<<n<<"\n";
	if (n < 32) {
		insertion_sort(strings, n, 0);
		return;
	}
	const size_t split0 = n/4,
	             split1 = n/2,
	             split2 = size_t(0.75*n);
	mergesort_4way_unstable(strings,        split0,        tmp);
	mergesort_4way_unstable(strings+split0, split1-split0, tmp);
	mergesort_4way_unstable(strings+split1, split2-split1, tmp);
	mergesort_4way_unstable(strings+split2, n-split2,      tmp);
	merge_4way_unstable(strings,        split0,
	                    strings+split0, split1-split0,
	                    strings+split1, split2-split1,
	                    strings+split2, n-split2,
	                    tmp);
	(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
}
void mergesort_4way_unstable(unsigned char** strings, size_t n)
{
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	mergesort_4way_unstable(strings, n, tmp);
	free(tmp);
}
ROUTINE_REGISTER_SINGLECORE(mergesort_4way_unstable, "4way unstable mergesort")
