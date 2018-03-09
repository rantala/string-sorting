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

/* Mergesort variants for sorting strings. When merging the input streams, use
 * previously calculated longest common prefix (LCP) values in order to avoid
 * string comparisons. Input stream example:
 *
 *      aaaa
 *      aaab   LCP with respect to 'aaaa':3
 *      aacd   LCP with respect to 'aaab':2
 *
 * In another variant, we also cache those characters that appear right after
 * the distinguishing prefix, when accessing the strings for the first time.
 * Later we can use both the LCP and cache values to decide which string to
 * merge next. Input stream example:
 *
 *      aaaa
 *      aaab   LCP with respect to 'aaaa':3, cache: 'b'
 *      aacd   LCP with respect to 'aaab':2, cache: 'c'
 *
 *
 * See also:
 *   @article{384249,
 *       author = {Peter Sanders},
 *       title = {Fast priority queues for cached memory},
 *       journal = {J. Exp. Algorithmics},
 *       volume = {5},
 *       year = {2000},
 *       issn = {1084-6654},
 *       pages = {7},
 *       doi = {http://doi.acm.org/10.1145/351827.384249},
 *       publisher = {ACM},
 *       address = {New York, NY, USA},
 *   }
 *
 *   Waihong Ng and Katsuhiko Kakehi:
 *     “Merging String Sequences by Longest Common Prefixes”,
 *     IPSJ Digital Courier, Vol. 4, pp.69-78 (2008)
 *     http://dx.doi.org/10.2197/ipsjdc.4.69
 */

#include "routine.h"
#include "util/debug.h"
#include "util/insertion_sort.h"
#include <algorithm>
#include <iostream>
#include <iterator>
#include <cassert>
#include <tuple>
#include <cstring>
#include <cstdlib>
#include <sstream>

// Handle very long strings. I guess in most cases we could choose smaller type
// to save some memory.
typedef size_t lcp_t;

#ifndef NDEBUG
static inline int
cmp(const unsigned char* a, const unsigned char* b)
{
	assert(a != 0);
	assert(b != 0);
	return strcmp(reinterpret_cast<const char*>(a),
	              reinterpret_cast<const char*>(b));
}
#endif

static lcp_t
lcp(unsigned char* a, unsigned char* b)
{
	size_t i=0;
	while (true) {
		const unsigned char A = *a++;
		const unsigned char B = *b++;
		if (A==0 or A != B) return i;
		++i;
	}
	assert(0);
	return lcp_t(-1);
}

std::tuple<int, lcp_t>
compare(unsigned char* a, unsigned char* b, size_t depth=0)
{
	assert(a); assert(b);
	for (size_t i=depth; ; ++i) {
		const unsigned char A = a[i];
		const unsigned char B = b[i];
		if (A == 0 or A != B) {
			return std::make_tuple(int(A)-int(B), i);
		}
	}
	assert(0);
	return std::make_tuple(int(-1), lcp_t(-1));
}

enum MergeResult {
	SortedInPlace,
	SortedInTemp,
};

/*******************************************************************************
 *
 * mergesort_lcp_2way
 *
 ******************************************************************************/

/* If the template parameter OutputLCP is true, write LCP values to lcp_result.
 * It is set to false when performing the final merge step -- at that point we
 * dont need the LCP results anymore.
 */

template <bool OutputLCP>
static void
merge_lcp_2way(unsigned char** from0,  lcp_t* restrict lcp_input0, size_t n0,
               unsigned char** from1,  lcp_t* restrict lcp_input1, size_t n1,
               unsigned char** result, lcp_t* restrict lcp_result)
{
	debug() << __func__ << "(): n0=" << n0 << ", n1=" << n1 << '\n';
	lcp_t lcp0=0, lcp1=0;
	{
		int cmp01; lcp_t lcp01;
		std::tie(cmp01, lcp01) = compare(*from0, *from1);
		if (cmp01 <= 0) {
			*result++ = *from0++;
			lcp0 = *lcp_input0++;
			lcp1 = lcp01;
			if (--n0 == 0) goto finish0;
		} else {
			*result++ = *from1++;
			lcp1 = *lcp_input1++;
			lcp0 = lcp01;
			if (--n1 == 0) goto finish1;
		}
	}
	while (true) {
		/*
		debug() << "Starting loop, n0="<<n0<<", n1="<<n1<<" ...\n"
		        << "\tprev  = '"<<*(result-1)<<"'\n"
		        << "\tfrom0 = '"<<*from0<<"'\n"
		        << "\tfrom1 = '"<<*from1<<"'\n"
		        << "\tlcp0  = " << lcp0 << "\n"
		        << "\tlcp1  = " << lcp1 << "\n"
		        << "\n";
		*/
		if (lcp0 > lcp1) {
			assert(cmp(*from0, *from1) < 0);
			*result++ = *from0++;
			if (OutputLCP) *lcp_result++ = lcp0;
			lcp0 = *lcp_input0++;
			if (--n0 == 0) goto finish0;
		} else if (lcp0 < lcp1) {
			assert(cmp(*from0, *from1) > 0);
			*result++ = *from1++;
			if (OutputLCP) *lcp_result++ = lcp1;
			lcp1 = *lcp_input1++;
			if (--n1 == 0) goto finish1;
		} else {
			int cmp01; lcp_t lcp01;
			std::tie(cmp01,lcp01) = compare(*from0, *from1, lcp0);
			if (OutputLCP) *lcp_result++ = lcp0;
			if (cmp01 <= 0) {
				*result++ = *from0++;
				lcp1 = lcp01;
				if (--n0 == 0) goto finish0;
				lcp0 = *lcp_input0++;
			} else {
				*result++ = *from1++;
				lcp0 = lcp01;
				if (--n1 == 0) goto finish1;
				lcp1 = *lcp_input1++;
			}
		}
	}
finish0:
	debug() << '~' << __func__ << "(): n0=" << n0 << ", n1=" << n1 << '\n';
	assert(not n0);
	assert(n1);
	std::copy(from1, from1+n1, result);
	if (OutputLCP) *lcp_result++ = lcp1;
	if (OutputLCP) std::copy(lcp_input1, lcp_input1+n1, lcp_result);
	return;
finish1:
	debug() << '~' << __func__ << "(): n0=" << n0 << ", n1=" << n1 << '\n';
	assert(not n1);
	assert(n0);
	std::copy(from0, from0+n0, result);
	if (OutputLCP) *lcp_result++ = lcp0;
	if (OutputLCP) std::copy(lcp_input0, lcp_input0+n0, lcp_result);
	return;
}

template <bool OutputLCP>
MergeResult
mergesort_lcp_2way(unsigned char** restrict strings_input,
                   unsigned char** restrict strings_output,
                   lcp_t* restrict lcp_input, lcp_t* restrict lcp_output,
                   size_t n)
{
	assert(n > 0);
	debug() << __func__ << "(): n=" << n << '\n';
	if (n < 32) {
		insertion_sort(strings_input, n, 0);
		for (unsigned i=0; i < n-1; ++i)
			lcp_input[i] = lcp(strings_input[i], strings_input[i+1]);
		return SortedInPlace;
	}
	const size_t split0 = n/2;
	MergeResult ml = mergesort_lcp_2way<true>(
			strings_input, strings_output,
			lcp_input,     lcp_output,
			split0);
	MergeResult mr = mergesort_lcp_2way<true>(
			strings_input+split0, strings_output+split0,
			lcp_input+split0,     lcp_output+split0,
			n-split0);
	if (ml != mr) {
		if (ml == SortedInPlace) {
			std::copy(strings_output+split0, strings_output+n,
					strings_input+split0);
			std::copy(lcp_output+split0, lcp_output+n,
					lcp_input+split0);
			mr = SortedInPlace;
		} else {
			assert(0);
			abort();
		}
	}
	if (ml == SortedInPlace) {
		merge_lcp_2way<OutputLCP>(
		           strings_input,        lcp_input,        split0,
		           strings_input+split0, lcp_input+split0, n-split0,
		           strings_output, lcp_output);
		return SortedInTemp;
	} else {
		merge_lcp_2way<OutputLCP>(
		           strings_output,        lcp_output,        split0,
		           strings_output+split0, lcp_output+split0, n-split0,
		           strings_input, lcp_input);
		return SortedInPlace;
	}
}
void
mergesort_lcp_2way(unsigned char** strings, size_t n)
{
	lcp_t* lcp_input  = static_cast<lcp_t*>(malloc(n*sizeof(lcp_t)));
	lcp_t* lcp_output = static_cast<lcp_t*>(malloc(n*sizeof(lcp_t)));
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	const MergeResult m = mergesort_lcp_2way<false>(strings, tmp,
			lcp_input, lcp_output, n);
	if (m == SortedInTemp) {
		(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
	}
	free(lcp_input);
	free(lcp_output);
	free(tmp);
}
ROUTINE_REGISTER_SINGLECORE(mergesort_lcp_2way, "LCP mergesort with 2way merger")

template <bool OutputLCP>
MergeResult
mergesort_lcp_2way_parallel(
		unsigned char** restrict strings_input,
		unsigned char** restrict strings_output,
		lcp_t* restrict lcp_input, lcp_t* restrict lcp_output,
		size_t n)
{
	assert(n > 0);
	debug() << __func__ << "(): n=" << n << '\n';
	if (n < 32) {
		insertion_sort(strings_input, n, 0);
		for (unsigned i=0; i < n-1; ++i)
			lcp_input[i] = lcp(strings_input[i], strings_input[i+1]);
		return SortedInPlace;
	}
	const size_t split0 = n/2;
	MergeResult ml, mr;
#pragma omp parallel sections
	{
#pragma omp section
	ml = mergesort_lcp_2way_parallel<true>(
			strings_input, strings_output,
			lcp_input,     lcp_output,
			split0);
#pragma omp section
	mr = mergesort_lcp_2way_parallel<true>(
			strings_input+split0, strings_output+split0,
			lcp_input+split0,     lcp_output+split0,
			n-split0);
	}
	if (ml != mr) {
		if (ml == SortedInPlace) {
			std::copy(strings_output+split0, strings_output+n,
					strings_input+split0);
			std::copy(lcp_output+split0, lcp_output+n,
					lcp_input+split0);
			mr = SortedInPlace;
		} else {
			assert(0);
			abort();
		}
	}
	if (ml == SortedInPlace) {
		merge_lcp_2way<OutputLCP>(
		           strings_input,        lcp_input,        split0,
		           strings_input+split0, lcp_input+split0, n-split0,
		           strings_output, lcp_output);
		return SortedInTemp;
	} else {
		merge_lcp_2way<OutputLCP>(
		           strings_output,        lcp_output,        split0,
		           strings_output+split0, lcp_output+split0, n-split0,
		           strings_input, lcp_input);
		return SortedInPlace;
	}
}
void
mergesort_lcp_2way_parallel(unsigned char** strings, size_t n)
{
	lcp_t* lcp_input  = static_cast<lcp_t*>(malloc(n*sizeof(lcp_t)));
	lcp_t* lcp_output = static_cast<lcp_t*>(malloc(n*sizeof(lcp_t)));
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	const MergeResult m = mergesort_lcp_2way_parallel<false>(strings, tmp,
			lcp_input, lcp_output, n);
	if (m == SortedInTemp) {
		(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
	}
	free(lcp_input);
	free(lcp_output);
	free(tmp);
}
ROUTINE_REGISTER_MULTICORE(mergesort_lcp_2way_parallel,
		"Parallel LCP mergesort with 2way merger")

/*******************************************************************************
 *
 * mergesort_lcp_3way
 *
 ******************************************************************************/

static void
check_lcps(unsigned char* latest,
           unsigned char* from0, lcp_t lcp0,
           unsigned char* from1, lcp_t lcp1,
           unsigned char* from2, lcp_t lcp2)
{
	debug()<<"******** CHECK ********\n"
	       <<"Latest: '"<<latest<<"'\n"
	       <<"     0: '"<<from0<<"', lcp="<<lcp0<<"\n"
	       <<"     1: '"<<from1<<"', lcp="<<lcp1<<"\n"
	       <<"     2: '"<<from2<<"', lcp="<<lcp2<<"\n"
	       <<"***********************\n";
	assert(lcp(latest, from0) == lcp0);
	assert(lcp(latest, from1) == lcp1);
	assert(lcp(latest, from2) == lcp2);
}

static void
check_input(unsigned char** from0, lcp_t* lcp_input0, size_t n0)
{
	(void) from0;
	(void) lcp_input0;
	(void) n0;
#ifndef NDEBUG
	for(size_t i=1;i<n0;++i)if(cmp(from0[i-1],from0[i])>0){
		debug()<<"Oops: ''"<<from0[i-1]<<"'' > ''"<<from0[i]<<"''\n";}
	for(size_t i=1;i<n0;++i)if(lcp(from0[i-1],from0[i])!=lcp_input0[i-1]){
		debug()<<"Oops: "<<"lcp('"<<from0[i-1]<<"', '"<<from0[i]<<"')="
			<<lcp(from0[i-1],from0[i])<<" != "<<lcp_input0[i-1]<<"\n";}
	for(size_t i=1;i<n0;++i)assert(cmp(from0[i-1],from0[i])<=0);
	for(size_t i=1;i<n0;++i)assert(lcp(from0[i-1],from0[i])==lcp_input0[i-1]);
#endif
}

template <bool OutputLCP>
static void
merge_lcp_3way(unsigned char** from0, lcp_t* lcp_input0, size_t n0,
               unsigned char** from1, lcp_t* lcp_input1, size_t n1,
               unsigned char** from2, lcp_t* lcp_input2, size_t n2,
               unsigned char** result, lcp_t* lcp_result)
{
	debug()<<__func__<<"(), n0="<<n0<<", n1="<<n1<<", n2="<<n2<<"\n";
	/*
	debug()<<"*** STREAM 0 ***\n"; for (unsigned i=0;i<n0;++i) { debug()<<"\t"<<from0[i]<<"\n"; }
	debug()<<"*** STREAM 1 ***\n"; for (unsigned i=0;i<n1;++i) { debug()<<"\t"<<from1[i]<<"\n"; }
	debug()<<"*** STREAM 2 ***\n"; for (unsigned i=0;i<n2;++i) { debug()<<"\t"<<from2[i]<<"\n"; }
	debug()<<"******\n";
	*/
	check_input(from0, lcp_input0, n0);
	check_input(from1, lcp_input1, n1);
	check_input(from2, lcp_input2, n2);
	lcp_t lcp0=0, lcp1=0, lcp2=0;

#define INITIAL_STATE(output_lcps)                                             \
{                                                                              \
	assert(lcp0 == lcp1); assert(lcp1 == lcp2);                            \
	int cmp01; lcp_t lcp01;                                                \
	std::tie(cmp01, lcp01) = compare(*from0, *from1, lcp0);                \
	if (cmp01 == 0) {                                                      \
		int cmp02; lcp_t lcp02;                                        \
		std::tie(cmp02, lcp02) = compare(*from0, *from2, lcp0);        \
		if (cmp02 < 0) {                                               \
			debug()<<"\t0 = 1 < 2\n";                              \
			assert(lcp01 >= lcp02);                                \
			*result++ = *from0++;                                  \
			if (output_lcps && OutputLCP) *lcp_result++ = lcp0;    \
			lcp0 = *lcp_input0++;                                  \
			lcp1 = lcp01;                                          \
			lcp2 = lcp02;                                          \
			if (--n0 == 0) goto finish0;                           \
			if (lcp0 > lcp1)  goto lcp_0gt1gt2;                    \
			if (lcp0 == lcp1) goto lcp_0eq1gt2;                    \
			if (lcp0 > lcp2)  goto lcp_1gt0gt2;                    \
			if (lcp0 == lcp2) goto lcp_1gt0eq2;                    \
			goto lcp_1gt2gt0;                                      \
		} else if (cmp02 == 0) {                                       \
			debug()<<"\t0 = 1 = 2\n";                              \
			assert(lcp01 == lcp02);                                \
			*result++ = *from0++;                                  \
			if (output_lcps && OutputLCP) *lcp_result++ = lcp0;    \
			lcp0 = *lcp_input0++;                                  \
			lcp1 = lcp01;                                          \
			lcp2 = lcp02;                                          \
			if (--n0 == 0) goto finish0;                           \
			if (lcp0 > lcp1)  goto lcp_0gt1eq2;                    \
			if (lcp0 == lcp1) goto lcp_0eq1eq2;                    \
			goto lcp_1eq2gt0;                                      \
		} else {                                                       \
			debug()<<"\t2 < 0 = 1\n";                              \
			*result++ = *from2++;                                  \
			if (output_lcps && OutputLCP) *lcp_result++ = lcp2;    \
			lcp0 = lcp02;                                          \
			lcp1 = lcp02;                                          \
			lcp2 = *lcp_input2++;                                  \
			if (--n2 == 0) goto finish2;                           \
			if (lcp2 > lcp0)  goto lcp_2gt0eq1;                    \
			if (lcp2 == lcp0) goto lcp_0eq1eq2;                    \
			goto lcp_0eq1gt2;                                      \
		}                                                              \
	} else if (cmp01 < 0) {                                                \
		int cmp12; lcp_t lcp12;                                        \
		std::tie(cmp12, lcp12) = compare(*from1, *from2, lcp0);        \
		if (cmp12 == 0) {                                              \
			debug()<<"\t0 < 1 = 2\n";                              \
			*result++ = *from0++;                                  \
			if (output_lcps && OutputLCP) *lcp_result++ = lcp0;    \
			lcp0 = *lcp_input0++;                                  \
			lcp1 = lcp01;                                          \
			lcp2 = lcp01;                                          \
			if (--n0 == 0) goto finish0;                           \
			if (lcp0 > lcp1)  goto lcp_0gt1eq2;                    \
			if (lcp0 == lcp1) goto lcp_0eq1eq2;                    \
			goto lcp_1eq2gt0;                                      \
		} else if (cmp12 < 0) {                                        \
			debug()<<"\t0 < 1 < 2\n";                              \
			*result++ = *from0++;                                  \
			if (output_lcps && OutputLCP) *lcp_result++ = lcp0;    \
			lcp0 = *lcp_input0++;                                  \
			lcp1 = lcp01;                                          \
			lcp2 = std::min(lcp01, lcp12);                         \
			if (--n0 == 0) goto finish0;                           \
			assert(lcp1 >= lcp2);                                  \
			if (lcp1 > lcp2) {                                     \
				if (lcp0 > lcp1)  goto lcp_0gt1gt2;            \
				if (lcp0 == lcp1) goto lcp_0eq1gt2;            \
				if (lcp0 > lcp2)  goto lcp_1gt0gt2;            \
				if (lcp0 == lcp2) goto lcp_1gt0eq2;            \
				goto lcp_1gt2gt0;                              \
			} else {                                               \
				if (lcp0 > lcp1)  goto lcp_0gt1eq2;            \
				if (lcp0 == lcp1) goto lcp_0eq1eq2;            \
				goto lcp_1eq2gt0;                              \
			}                                                      \
		} else {                                                       \
			/* TODO: possible to avoid comparison in some cases?*/ \
                                                                               \
			/* 0 < 1 && 2 < 1*/                                    \
			int cmp02; lcp_t lcp02;                                \
			std::tie(cmp02, lcp02)=compare(*from0, *from2, lcp0);  \
			if (cmp02 <= 0) {                                      \
				debug()<<"\t0 <= 2 < 1\n";                     \
				*result++ = *from0++;                          \
				if (output_lcps&&OutputLCP) *lcp_result++=lcp0;\
				lcp0 = *lcp_input0++;                          \
				lcp1 = lcp01;                                  \
				lcp2 = lcp02;                                  \
				if (--n0 == 0) goto finish0;                   \
			} else {                                               \
				debug()<<"\t2 < 0 < 1\n";                      \
				*result++ = *from2++;                          \
				if (output_lcps&&OutputLCP) *lcp_result++=lcp2;\
				lcp0 = lcp02;                                  \
				lcp1 = lcp12;                                  \
				lcp2 = *lcp_input2++;                          \
				if (--n2 == 0) goto finish2;                   \
				assert(lcp0 >= lcp1 && "2<0<1");               \
				if (lcp0 > lcp1) {                             \
					if (lcp2 > lcp0)  goto lcp_2gt0gt1;    \
					if (lcp2 == lcp0) goto lcp_0eq2gt1;    \
					if (lcp2 > lcp1)  goto lcp_0gt2gt1;    \
					if (lcp2 == lcp1) goto lcp_0gt1eq2;    \
					goto lcp_0gt1gt2;                      \
				} else {                                       \
					if (lcp2 > lcp0)  goto lcp_2gt0eq1;    \
					if (lcp2 == lcp0) goto lcp_0eq1eq2;    \
					goto lcp_0eq1gt2;                      \
				}                                              \
			}                                                      \
		}                                                              \
	} else {                                                               \
		/* 1 < 0*/                                                     \
		int cmp12; lcp_t lcp12;                                        \
		std::tie(cmp12, lcp12) = compare(*from1, *from2, lcp0);        \
		if (cmp12 <= 0) {                                              \
			debug()<<"\t1 < 0 and 1 <= 2\n";                       \
			*result++ = *from1++;                                  \
			if (output_lcps && OutputLCP) *lcp_result++ = lcp1;    \
			lcp0 = lcp01;                                          \
			lcp1 = *lcp_input1++;                                  \
			lcp2 = lcp12;                                          \
			if (--n1 == 0) goto finish1;                           \
		} else {                                                       \
			debug()<<"\t2 < 1 < 0\n";                              \
			*result++ = *from2++;                                  \
			if (output_lcps && OutputLCP) *lcp_result++ = lcp2;    \
			lcp0 = std::min(lcp01, lcp12);                         \
			lcp1 = lcp12;                                          \
			lcp2 = *lcp_input2++;                                  \
			if (--n2 == 0) goto finish2;                           \
			assert(lcp1 >= lcp0);                                  \
			if (lcp1 > lcp0) {                                     \
				if (lcp2 > lcp1)  goto lcp_2gt1gt0;            \
				if (lcp2 == lcp1) goto lcp_1eq2gt0;            \
				if (lcp2 > lcp0)  goto lcp_1gt2gt0;            \
				if (lcp2 == lcp0) goto lcp_1gt0eq2;            \
				goto lcp_1gt0gt2;                              \
			} else {                                               \
				if (lcp2 > lcp0)  goto lcp_2gt0eq1;            \
				if (lcp2 == lcp0) goto lcp_0eq1eq2;            \
				goto lcp_0eq1gt2;                              \
			}                                                      \
		}                                                              \
	}                                                                      \
	check_lcps(*(result-1),*from0,lcp0,*from1,lcp1,*from2,lcp2);           \
	goto branch_by_lcp;                                                    \
}

INITIAL_STATE(false)

branch_by_lcp:
	if (lcp0 > lcp1) {
		if (lcp1 >  lcp2) goto lcp_0gt1gt2;
		if (lcp1 == lcp2) goto lcp_0gt1eq2;
		if (lcp0 >  lcp2) goto lcp_0gt2gt1;
		if (lcp0 == lcp2) goto lcp_0eq2gt1;
		goto lcp_2gt0gt1;
	}
	if (lcp0 == lcp1) {
		if (lcp1 >  lcp2) goto lcp_0eq1gt2;
		if (lcp1 == lcp2) goto lcp_0eq1eq2;
		goto lcp_2gt0eq1;
	}
	if (lcp0 >  lcp2) goto lcp_1gt0gt2;
	if (lcp0 == lcp2) goto lcp_1gt0eq2;
	if (lcp1 >  lcp2) goto lcp_1gt2gt0;
	if (lcp1 == lcp2) goto lcp_1eq2gt0;
	goto lcp_2gt1gt0;

#define StrictCase(a,b,c)                                                      \
        lcp_##a##gt##b##gt##c:                                                 \
        debug()<<"\tlcp_"<<a<<"lt"<<b<<"lt"<<c<<"\n";                          \
        assert(lcp##a > lcp##b);                                               \
        assert(lcp##b > lcp##c);                                               \
        assert(cmp(*from##a, *from##b) < 0);                                   \
        assert(cmp(*from##b, *from##c) < 0);                                   \
        check_lcps(*(result-1),*from0,lcp0,*from1,lcp1,*from2,lcp2);           \
        *result++ = *from##a++;                                                \
        debug()<<"\tlcp result << " << lcp##a << '\n';                         \
        if (OutputLCP) *lcp_result++ = lcp##a;                                 \
        if (--n##a == 0) goto finish##a;                                       \
        lcp##a = *lcp_input##a++;                                              \
        if (lcp##a >  lcp##b) goto lcp_##a##gt##b##gt##c;                      \
        if (lcp##a == lcp##b) goto lcp_##a##eq##b##gt##c;                      \
        if (lcp##a >  lcp##c) goto lcp_##b##gt##a##gt##c;                      \
        if (lcp##a == lcp##c) goto lcp_##b##gt##a##eq##c;                      \
        goto lcp_##b##gt##c##gt##a;
	StrictCase(0, 1, 2);
	StrictCase(0, 2, 1);
	StrictCase(1, 0, 2);
	StrictCase(1, 2, 0);
	StrictCase(2, 0, 1);
	StrictCase(2, 1, 0);
#undef StrictCase

#define GtEq(a, b, c)                                                          \
        static_assert(b < c, "b < c");                                         \
        lcp_##a##gt##b##eq##c:                                                 \
        debug()<<"\tlcp_"<<a<<"gt"<<b<<"eq"<<c<<"\n";                          \
        assert(lcp##a >  lcp##b);                                              \
        assert(lcp##b == lcp##c);                                              \
        assert(cmp(*from##a, *from##b) < 0);                                   \
        assert(cmp(*from##a, *from##c) < 0);                                   \
        check_lcps(*(result-1),*from0,lcp0,*from1,lcp1,*from2,lcp2);           \
        *result++ = *from##a++;                                                \
        if (OutputLCP) *lcp_result++ = lcp##a;                                 \
        debug()<<"\tlcp result << " << lcp##a << '\n';                         \
        lcp##a = *lcp_input##a++;                                              \
        if (--n##a == 0) goto finish##a;                                       \
        if (lcp##a >  lcp##b) goto lcp_##a##gt##b##eq##c;                      \
        if (lcp##a == lcp##b) goto lcp_##a##eq##b##eq##c;                      \
        goto lcp_##b##eq##c##gt##a;
	GtEq(0, 1, 2); // lcp0 > lcp1 = lcp2
	GtEq(1, 0, 2); // lcp1 > lcp0 = lcp2
	GtEq(2, 0, 1); // lcp2 > lcp0 = lcp1
lcp_0gt2eq1: goto lcp_0gt1eq2;
lcp_2gt1eq0: goto lcp_2gt0eq1;
lcp_1gt2eq0: goto lcp_1gt0eq2;
#undef GtEq

#define EqGt(a, b, c)                                                          \
        static_assert(a < b, "a < b");                                         \
        lcp_##a##eq##b##gt##c:                                                 \
{                                                                              \
        debug()<<"\tlcp_"<<a<<"eq"<<b<<"gt"<<c<<"\n";                          \
        assert(lcp##a == lcp##b);                                              \
        assert(lcp##b >= lcp##c);                                              \
        assert(cmp(*from##a, *from##c) < 0);                                   \
        assert(cmp(*from##b, *from##c) < 0);                                   \
        check_lcps(*(result-1),*from0,lcp0,*from1,lcp1,*from2,lcp2);           \
        int cmp##a##b; lcp_t lcp##a##b;                                        \
        std::tie(cmp##a##b, lcp##a##b)=compare(*from##a, *from##b, lcp##a);    \
        if (cmp##a##b <= 0) {                                                  \
                *result++ = *from##a++;                                        \
                if (OutputLCP) *lcp_result++ = lcp##a;                         \
                debug()<<"\tlcp result << " << lcp##a << '\n';                 \
                lcp##a = *lcp_input##a++;                                      \
                lcp##b = lcp##a##b;                                            \
                if (--n##a == 0) goto finish##a;                               \
        } else {                                                               \
                *result++ = *from##b++;                                        \
                if (OutputLCP) *lcp_result++ = lcp##b;                         \
                debug()<<"\tlcp result << " << lcp##a << '\n';                 \
                lcp##b = *lcp_input##b++;                                      \
                lcp##a = lcp##a##b;                                            \
                if (--n##b == 0) goto finish##b;                               \
        }                                                                      \
        goto branch_by_lcp;                                                    \
}
	EqGt(0, 1, 2); // lcp0 = lcp1 > lcp2
	EqGt(0, 2, 1); // lcp0 = lcp2 > lcp1
	EqGt(1, 2, 0); // lcp1 = lcp2 > lcp0
lcp_1eq0gt2: goto lcp_0eq1gt2;
lcp_2eq0gt1: goto lcp_0eq2gt1;
lcp_2eq1gt0: goto lcp_1eq2gt0;
#undef EqGt

lcp_1eq0eq2:
lcp_2eq0eq1:
lcp_0eq1eq2: INITIAL_STATE(true)

finish0:
	assert(n0 == 0);
	if (OutputLCP) *lcp_result++ = std::max(lcp1, lcp2);
	merge_lcp_2way<OutputLCP>(from1, lcp_input1, n1,
	                          from2, lcp_input2, n2,
	                          result, lcp_result);
	debug() << '~' << __func__ << '\n';
	return;
finish1:
	assert(n1 == 0);
	if (OutputLCP) *lcp_result++ = std::max(lcp0, lcp2);
	merge_lcp_2way<OutputLCP>(from0, lcp_input0, n0,
	                          from2, lcp_input2, n2,
	                          result, lcp_result);
	debug() << '~' << __func__ << '\n';
	return;
finish2:
	assert(n2 == 0);
	if (OutputLCP) *lcp_result++ = std::max(lcp0, lcp1);
	merge_lcp_2way<OutputLCP>(from0, lcp_input0, n0,
	                          from1, lcp_input1, n1,
	                          result, lcp_result);
	debug() << '~' << __func__ << '\n';
	return;
}

template <bool OutputLCP>
MergeResult
mergesort_lcp_3way(unsigned char** restrict strings_input,
                   unsigned char** restrict strings_output,
                   lcp_t* restrict lcp_input, lcp_t* restrict lcp_output,
                   size_t n)
{
	debug() << __func__ << "(): n=" << n << '\n';
	if (n < 32) {
		insertion_sort(strings_input, n, 0);
		for (unsigned i=0; i < n-1; ++i)
			lcp_input[i] = lcp(strings_input[i], strings_input[i+1]);
		return SortedInPlace;
	}
	const size_t split0 = n/3,
	             split1 = size_t((2.0/3.0)*n);
	MergeResult m0 = mergesort_lcp_3way<true>(
			strings_input,        strings_output,
			lcp_input,            lcp_output,
			split0);
	debug() << __func__ << "(): checking first merge\n";
	if (m0 == SortedInPlace) check_input(strings_input,  lcp_input,  split0);
	else                     check_input(strings_output, lcp_output, split0);
	MergeResult m1 = mergesort_lcp_3way<true>(
			strings_input+split0, strings_output+split0,
			lcp_input+split0,     lcp_output+split0,
			split1-split0);
	debug() << __func__ << "(): checking second merge\n";
	if (m1 == SortedInPlace) check_input(strings_input+split0,  lcp_input+split0,  split1-split0);
	else                     check_input(strings_output+split0, lcp_output+split0, split1-split0);
	MergeResult m2 = mergesort_lcp_3way<true>(
			strings_input+split1, strings_output+split1,
			lcp_input+split1,     lcp_output+split1,
			n-split1);
	debug() << __func__ << "(): checking third merge\n";
	if (m2 == SortedInPlace) check_input(strings_input+split1,  lcp_input+split1,  n-split1);
	else                     check_input(strings_output+split1, lcp_output+split1, n-split1);
	debug() << __func__ << "(): m0="<<m0<<", m1="<<m1<<", m2="<<m2<<"\n";
	if (m0 != m1) {
		if (m1 != m2) {
			// m0 == m2 != m1
			if (m1 == SortedInPlace) {
				std::copy(strings_input+split0, strings_input+split1,
						strings_output+split0);
				std::copy(lcp_input+split0, lcp_input+split1,
						lcp_output+split0);
				m1 = SortedInTemp;
			} else {
				std::copy(strings_output+split0, strings_output+split1,
						strings_input+split0);
				std::copy(lcp_output+split0, lcp_output+split1,
						lcp_input+split0);
				m1 = SortedInPlace;
			}
		} else {
			// m0 != m1 == m2
			if (m0 == SortedInPlace) {
				std::copy(strings_input, strings_input+split0,
						strings_output);
				std::copy(lcp_input, lcp_input+split0,
						lcp_output);
				m0 = SortedInTemp;
			} else {
				std::copy(strings_output, strings_output+split0,
						strings_input);
				std::copy(lcp_output, lcp_output+split0,
						lcp_input);
				m0 = SortedInPlace;
			}
		}
	}
	if (m1 != m2) {
		if (m2 == SortedInPlace) {
			std::copy(strings_input+split1, strings_input+n,
					strings_output+split1);
			std::copy(lcp_input+split1, lcp_input+n,
					lcp_output+split1);
			m2 = SortedInTemp;
		} else {
			std::copy(strings_output+split1, strings_output+n,
					strings_input+split1);
			std::copy(lcp_output+split1, lcp_output+n,
					lcp_input+split1);
			m2 = SortedInPlace;
		}
	}
	assert(m0 == m1); assert(m1 == m2);
	if (m0 == SortedInPlace) {
		merge_lcp_3way<OutputLCP>(
			   strings_input,        lcp_input,        split0,
			   strings_input+split0, lcp_input+split0, split1-split0,
			   strings_input+split1, lcp_input+split1, n-split1,
			   strings_output, lcp_output);
		//debug() << __func__ << "(): checking merged result, in -> out\n";
		if (OutputLCP) check_input(strings_output, lcp_output, n);
		return SortedInTemp;
	} else {
		merge_lcp_3way<OutputLCP>(
			   strings_output,        lcp_output,        split0,
			   strings_output+split0, lcp_output+split0, split1-split0,
			   strings_output+split1, lcp_output+split1, n-split1,
			   strings_input, lcp_input);
		//debug() << __func__ << "(): checking merged result, out -> in\n";
		if (OutputLCP) check_input(strings_input, lcp_input, n);
		return SortedInPlace;
	}
}
void
mergesort_lcp_3way(unsigned char** strings, size_t n)
{
	debug() << __func__ << '\n';
	lcp_t* lcp_input = (lcp_t*) malloc(n*sizeof(lcp_t));
	lcp_t* lcp_tmp   = (lcp_t*) malloc(n*sizeof(lcp_t));
	unsigned char** input_tmp = (unsigned char**)
		malloc(n*sizeof(unsigned char*));
	const MergeResult m = mergesort_lcp_3way<false>(strings, input_tmp,
			lcp_input, lcp_tmp, n);
	if (m == SortedInTemp) {
		(void) memcpy(strings, input_tmp, n*sizeof(unsigned char*));
	}
	free(lcp_input);
	free(lcp_tmp);
	free(input_tmp);
}
ROUTINE_REGISTER_SINGLECORE(mergesort_lcp_3way, "LCP mergesort with 3way merger")

template <bool OutputLCP>
MergeResult
mergesort_lcp_3way_parallel(unsigned char** restrict strings_input,
                   unsigned char** restrict strings_output,
                   lcp_t* restrict lcp_input, lcp_t* restrict lcp_output,
                   size_t n)
{
	debug() << __func__ << "(): n=" << n << '\n';
	if (n < 32) {
		insertion_sort(strings_input, n, 0);
		for (unsigned i=0; i < n-1; ++i)
			lcp_input[i] = lcp(strings_input[i], strings_input[i+1]);
		return SortedInPlace;
	}
	const size_t split0 = n/3,
	             split1 = size_t((2.0/3.0)*n);
	MergeResult m0, m1, m2;
#pragma omp parallel sections
	{
#pragma omp section
	m0 = mergesort_lcp_3way_parallel<true>(
			strings_input,        strings_output,
			lcp_input,            lcp_output,
			split0);
#pragma omp section
	m1 = mergesort_lcp_3way_parallel<true>(
			strings_input+split0, strings_output+split0,
			lcp_input+split0,     lcp_output+split0,
			split1-split0);
#pragma omp section
	m2 = mergesort_lcp_3way_parallel<true>(
			strings_input+split1, strings_output+split1,
			lcp_input+split1,     lcp_output+split1,
			n-split1);
	}
	debug() << __func__ << "(): m0="<<m0<<", m1="<<m1<<", m2="<<m2<<"\n";
	if (m0 != m1) {
		if (m1 != m2) {
			// m0 == m2 != m1
			if (m1 == SortedInPlace) {
				std::copy(strings_input+split0, strings_input+split1,
						strings_output+split0);
				std::copy(lcp_input+split0, lcp_input+split1,
						lcp_output+split0);
				m1 = SortedInTemp;
			} else {
				std::copy(strings_output+split0, strings_output+split1,
						strings_input+split0);
				std::copy(lcp_output+split0, lcp_output+split1,
						lcp_input+split0);
				m1 = SortedInPlace;
			}
		} else {
			// m0 != m1 == m2
			if (m0 == SortedInPlace) {
				std::copy(strings_input, strings_input+split0,
						strings_output);
				std::copy(lcp_input, lcp_input+split0,
						lcp_output);
				m0 = SortedInTemp;
			} else {
				std::copy(strings_output, strings_output+split0,
						strings_input);
				std::copy(lcp_output, lcp_output+split0,
						lcp_input);
				m0 = SortedInPlace;
			}
		}
	}
	if (m1 != m2) {
		if (m2 == SortedInPlace) {
			std::copy(strings_input+split1, strings_input+n,
					strings_output+split1);
			std::copy(lcp_input+split1, lcp_input+n,
					lcp_output+split1);
			m2 = SortedInTemp;
		} else {
			std::copy(strings_output+split1, strings_output+n,
					strings_input+split1);
			std::copy(lcp_output+split1, lcp_output+n,
					lcp_input+split1);
			m2 = SortedInPlace;
		}
	}
	assert(m0 == m1); assert(m1 == m2);
	if (m0 == SortedInPlace) {
		merge_lcp_3way<OutputLCP>(
			   strings_input,        lcp_input,        split0,
			   strings_input+split0, lcp_input+split0, split1-split0,
			   strings_input+split1, lcp_input+split1, n-split1,
			   strings_output, lcp_output);
		//debug() << __func__ << "(): checking merged result, in -> out\n";
		if (OutputLCP) check_input(strings_output, lcp_output, n);
		return SortedInTemp;
	} else {
		merge_lcp_3way<OutputLCP>(
			   strings_output,        lcp_output,        split0,
			   strings_output+split0, lcp_output+split0, split1-split0,
			   strings_output+split1, lcp_output+split1, n-split1,
			   strings_input, lcp_input);
		//debug() << __func__ << "(): checking merged result, out -> in\n";
		if (OutputLCP) check_input(strings_input, lcp_input, n);
		return SortedInPlace;
	}
}
void
mergesort_lcp_3way_parallel(unsigned char** strings, size_t n)
{
	debug() << __func__ << '\n';
	lcp_t* lcp_input = (lcp_t*) malloc(n*sizeof(lcp_t));
	lcp_t* lcp_tmp   = (lcp_t*) malloc(n*sizeof(lcp_t));
	unsigned char** input_tmp = (unsigned char**)
		malloc(n*sizeof(unsigned char*));
	const MergeResult m = mergesort_lcp_3way_parallel<false>(strings, input_tmp,
			lcp_input, lcp_tmp, n);
	if (m == SortedInTemp) {
		(void) memcpy(strings, input_tmp, n*sizeof(unsigned char*));
	}
	free(lcp_input);
	free(lcp_tmp);
	free(input_tmp);
}
ROUTINE_REGISTER_MULTICORE(mergesort_lcp_3way_parallel,
		"Parallel LCP mergesort with 3way merger")

/*******************************************************************************
 *
 * mergesort_cache_lcp_2way
 *
 ******************************************************************************/

#if 0
static int STAT_try_cache=0;
static int STAT_cache_useless=0;
static inline void stat_try_cache()     { ++STAT_try_cache;     }
static inline void stat_cache_useless() { ++STAT_cache_useless; }
static inline void stat_print()
{
	std::cout << "[Cache useful: "
	          << int(100*double(STAT_try_cache-STAT_cache_useless)
	                             /double(STAT_try_cache))
	          << "%, " << STAT_try_cache
	          << "/" << STAT_try_cache-STAT_cache_useless
	          << "/" << STAT_cache_useless << "]\n";
}
#else
static inline void stat_try_cache()     {}
static inline void stat_cache_useless() {}
static inline void stat_print()         {}
#endif

static std::string to_str(unsigned char c)
{
	std::ostringstream strm;
	if (isprint(c)) strm << c;
	else            strm << '<' << int(c) << '>';
	return strm.str();
}
static std::string to_str(uint16_t c)
{ return to_str(uint8_t((0xFF00 & c) >> 8)) + to_str(uint8_t(c)); }
static std::string to_str(uint32_t c)
{ return to_str(uint16_t((0xFFFF0000 & c) >> 16)) + to_str(uint16_t(c)); }

// Calculates the LCP between two string caches.
static inline unsigned lcp(unsigned char a, unsigned char b)
{
	assert(0);
	(void) a;
	(void) b;
	return 0;
}
static inline unsigned lcp(uint16_t a, uint16_t b)
{
	assert(a != b or a==0);
	unsigned A, B;
	A = 0xFF00 & a;
	B = 0xFF00 & b;
	if (A==0 or A!=B) return 0;
	return 1;
}
static inline unsigned lcp(uint32_t a, uint32_t b)
{
	assert(a != b or a==0);
	unsigned A, B;
	A = 0xFF000000 & a;
	B = 0xFF000000 & b;
	if (A==0 or A!=B) return 0;
	A = 0x00FF0000 & a;
	B = 0x00FF0000 & b;
	if (A==0 or A!=B) return 1;
	A = 0x0000FF00 & a;
	B = 0x0000FF00 & b;
	if (A==0 or A!=B) return 2;
	return 3;
}

template <typename CharT>
static void
check_lcp_and_cache(unsigned char* latest,
                    unsigned char* from0, lcp_t lcp0, CharT cache0,
                    unsigned char* from1, lcp_t lcp1, CharT cache1)
{
	(void) latest;
	(void) from0; (void) lcp0; (void) cache0;
	(void) from1; (void) lcp1; (void) cache1;
	/*debug()<<"******** CHECK ********\n"
	       <<"Latest: '"<<latest<<"'\n"
	       <<"     0: '"<<from0<<"', lcp="<<lcp0<<", cache="<<to_str(cache0)<<"\n"
	       <<"     1: '"<<from1<<"', lcp="<<lcp1<<", cache="<<to_str(cache1)<<"\n"
	       <<"***********************\n";*/
	assert(lcp(latest, from0) == lcp0);
	assert(lcp(latest, from1) == lcp1);
	assert(get_char<CharT>(from0, lcp0) == cache0);
	assert(get_char<CharT>(from1, lcp1) == cache1);
}

template <typename CharT>
static void
check_input(unsigned char** from0, lcp_t* lcp_input0, CharT* cache_input0, size_t n0)
{
	(void) from0;
	(void) lcp_input0;
	(void) cache_input0;
	(void) n0;
#ifndef NDEBUG
	for(size_t i=1;i<n0;++i)if(cmp(from0[i-1],from0[i])>0){
		debug()<<"Oops: ''"<<from0[i-1]<<"'' > ''"<<from0[i]<<"''\n";}
	for(size_t i=1;i<n0;++i)if(lcp(from0[i-1],from0[i])!=lcp_input0[i-1]){
		debug()<<"Oops: "<<"lcp('"<<from0[i-1]<<"', '"<<from0[i]<<"')="
			<<lcp(from0[i-1],from0[i])<<" != "<<lcp_input0[i-1]<<"\n";}
	for(size_t i=1;i<n0;++i)assert(cmp(from0[i-1],from0[i])<=0);
	for(size_t i=1;i<n0;++i)assert(lcp(from0[i-1],from0[i])==lcp_input0[i-1]);
	assert(get_char<CharT>(*from0,0)==cache_input0[0]);
	for(size_t i=1;i<n0;++i)assert(get_char<CharT>(from0[i],lcp_input0[i-1])
			==cache_input0[i]);
#endif
}

template <bool OutputLCP, typename CharT>
void
merge_cache_lcp_2way(
       unsigned char** from0,  lcp_t* restrict lcp_input0, CharT* restrict cache_input0, size_t n0,
       unsigned char** from1,  lcp_t* restrict lcp_input1, CharT* restrict cache_input1, size_t n1,
       unsigned char** result, lcp_t* restrict lcp_result, CharT* restrict cache_result)
{
	debug() << __func__ << "(): n0=" << n0 << ", n1=" << n1 << '\n';

	check_input(from0, lcp_input0, cache_input0, n0);
	check_input(from1, lcp_input1, cache_input1, n1);

	lcp_t lcp0=0, lcp1=0;
	CharT cache0 = *cache_input0++;
	CharT cache1 = *cache_input1++;
	{
		stat_try_cache();
		if (cache0 < cache1) {
			assert(cmp(*from0, *from1) < 0);
			if (sizeof(CharT) > 1) {
				const int l = lcp(cache0, cache1);
				if (l > 0) {
					cache1 = get_char<CharT>(*from1, l);
				}
				*result++ = *from0++;
				if (OutputLCP) *cache_result++ = cache0;
				lcp0 = *lcp_input0++;
				lcp1 = l;
				cache0 = *cache_input0++;
			} else {
				*result++ = *from0++;
				if (OutputLCP) *cache_result++ = cache0;
				lcp0 = *lcp_input0++;
				cache0 = *cache_input0++;
			}
			if (--n0 == 0) goto finish0;
		} else if (cache0 > cache1) {
			assert(cmp(*from0, *from1) > 0);
			if (sizeof(CharT) > 1) {
				const int l = lcp(cache0, cache1);
				if (l > 0) {
					cache0 = get_char<CharT>(*from0, l);
				}
				*result++ = *from1++;
				if (OutputLCP) *cache_result++ = cache1;
				lcp0 = l;
				lcp1 = *lcp_input1++;
				cache1 = *cache_input1++;
			} else {
				*result++ = *from1++;
				if (OutputLCP) *cache_result++ = cache1;
				lcp1 = *lcp_input1++;
				cache1 = *cache_input1++;
			}
			if (--n1 == 0) goto finish1;
		} else {
			if (is_end(cache0)) {
				assert(cmp(*from0, *from1) == 0);
				if (sizeof(CharT) > 1) {
					const int l = lcp(cache0, cache1);
					*result++ = *from0++;
					if (OutputLCP) *cache_result++ = cache0;
					lcp0 = *lcp_input0++;
					lcp1 = l;
					cache0 = *cache_input0++;
					cache1 = 0;
				} else {
					*result++ = *from0++;
					if (OutputLCP) *cache_result++ = cache0;
					lcp0 = *lcp_input0++;
					cache0 = *cache_input0++;
					cache1 = 0;
				}
				if (--n0 == 0) goto finish0;
			} else {
				stat_cache_useless();
				int cmp01; lcp_t lcp01;
				std::tie(cmp01, lcp01) = compare(*from0, *from1, sizeof(CharT));
				if (cmp01 < 0) {
					*result++ = *from0++;
					if (OutputLCP) *cache_result++ = cache0;
					lcp0 = *lcp_input0++;
					lcp1 = lcp01;
					cache0 = *cache_input0++;
					cache1 = get_char<CharT>(*from1, lcp01);
					if (--n0 == 0) goto finish0;
				} else if (cmp01 == 0) {
					*result++ = *from0++;
					if (OutputLCP) *cache_result++ = cache0;
					lcp0 = *lcp_input0++;
					lcp1 = lcp01;
					cache0 = *cache_input0++;
					cache1 = 0;
					if (--n0 == 0) goto finish0;
				} else {
					*result++ = *from1++;
					if (OutputLCP) *cache_result++ = cache1;
					lcp0 = lcp01;
					lcp1 = *lcp_input1++;
					cache0 = get_char<CharT>(*from0, lcp01);
					cache1 = *cache_input1++;
					if (--n1 == 0) goto finish1;
				}
			}
		}
	}
	while (true) {
		debug() << "Starting loop, n0="<<n0<<", n1="<<n1<<" ...\n"
			<< "\tprev   = '"<<*(result-1)<<"'\n"
			<< "\t*from0 = '"<<*from0<<"'\n"
			<< "\t*from1 = '"<<*from1<<"'\n"
			<< "\tlcp0   = " <<lcp0<<"\n"
			<< "\tlcp1   = " <<lcp1<<"\n"
			<< "\tcache0 = '"<<to_str(cache0)<<"'\n"
			<< "\tcache1 = '"<<to_str(cache1)<<"'\n"
			<< "\n";
		check_lcp_and_cache(*(result-1),*from0,lcp0,cache0,*from1,lcp1,cache1);
		if (lcp0 > lcp1) {
			debug() << "\tlcp0 > lcp1\n";
			assert(cmp(*from0, *from1) < 0);
			*result++ = *from0++;
			if (OutputLCP) *lcp_result++ = lcp0;
			if (OutputLCP) *cache_result++ = cache0;
			lcp0 = *lcp_input0++;
			cache0 = *cache_input0++;
			if (--n0 == 0) goto finish0;
		} else if (lcp0 < lcp1) {
			debug() << "\tlcp0 < lcp1\n";
			assert(cmp(*from0, *from1) > 0);
			*result++ = *from1++;
			if (OutputLCP) *lcp_result++ = lcp1;
			if (OutputLCP) *cache_result++ = cache1;
			lcp1 = *lcp_input1++;
			cache1 = *cache_input1++;
			if (--n1 == 0) goto finish1;
		} else {
			debug() << "\tlcp0 == lcp1\n";
			stat_try_cache();
			// Both strings in the sorted sequences have the same
			// prefix than the latest string in output. Check
			// cached characters first.
			if (cache0 < cache1) {
				debug() << "\t\tcache0 < cache1\n";
				assert(cmp(*from0, *from1) < 0);
				if (sizeof(CharT) > 1) {
					const int l = lcp(cache0, cache1);
					if (l > 0) {
						lcp1 += l;
						cache1 = get_char<CharT>(*from1, lcp1);
					}
				}
				*result++ = *from0++;
				if (OutputLCP) *lcp_result++ = lcp0;
				if (OutputLCP) *cache_result++ = cache0;
				lcp0 = *lcp_input0++;
				cache0 = *cache_input0++;
				if (--n0 == 0) goto finish0;
			} else if (cache0 > cache1) {
				debug() << "\t\tcache0 > cache1\n";
				assert(cmp(*from0, *from1) > 0);
				if (sizeof(CharT) > 1) {
					const int l = lcp(cache0, cache1);
					if (l > 0) {
						lcp0 += l;
						cache0 = get_char<CharT>(*from0, lcp0);
					}
				}
				*result++ = *from1++;
				if (OutputLCP) *lcp_result++ = lcp1;
				if (OutputLCP) *cache_result++ = cache1;
				lcp1 = *lcp_input1++;
				cache1 = *cache_input1++;
				if (--n1 == 0) goto finish1;
			} else {
				debug() << "\t\tcache0 == cache1\n";
				if (is_end(cache0)) {
					assert(cmp(*from0, *from1) == 0);
					if (sizeof(CharT) > 1) {
						*result++ = *from0++;
						if (OutputLCP) *lcp_result++ = lcp0 /* == lcp1 */;
						if (OutputLCP) *cache_result++ = cache0 /* == cache1 */;
						lcp0 = *lcp_input0++;
						lcp1 += lcp(cache0, cache1);
						cache0 = *cache_input0++;
						cache1 = 0;
					} else {
						*result++ = *from0++;
						if (OutputLCP) *lcp_result++ = lcp0 /* == lcp1 */;
						if (OutputLCP) *cache_result++ = cache0 /* == cache1 */;
						lcp0 = *lcp_input0++;
						cache0 = *cache_input0++;
					}
					if (--n0 == 0) goto finish0;
				} else {
					stat_cache_useless();
					int cmp01; lcp_t lcp01;
					std::tie(cmp01, lcp01) = compare(*from0, *from1, lcp0+sizeof(CharT));
					if (cmp01 < 0) {
						*result++ = *from0++;
						if (OutputLCP) *lcp_result++ = lcp0 /* == lcp1 */;
						if (OutputLCP) *cache_result++ = cache0 /* == cache1 */;
						lcp0 = *lcp_input0++;
						lcp1 = lcp01;
						cache0 = *cache_input0++;
						cache1 = get_char<CharT>(*from1, lcp01);
						if (--n0 == 0) goto finish0;
					} else if (cmp01 == 0) {
						*result++ = *from0++;
						if (OutputLCP) *lcp_result++ = lcp0 /* == lcp1 */;
						if (OutputLCP) *cache_result++ = cache0 /* == cache1 */;
						lcp0 = *lcp_input0++;
						lcp1 = lcp01;
						cache0 = *cache_input0++;
						cache1 = 0;
						if (--n0 == 0) goto finish0;
					} else {
						*result++ = *from1++;
						if (OutputLCP) *lcp_result++ = lcp0 /* == lcp1 */;
						if (OutputLCP) *cache_result++ = cache0 /* == cache1 */;
						lcp0 = lcp01;
						lcp1 = *lcp_input1++;
						cache0 = get_char<CharT>(*from0, lcp01);
						cache1 = *cache_input1++;
						if (--n1 == 0) goto finish1;
					}
				}
			}
		}
	}
finish0:
	assert(n0==0);
	assert(n1);
	if (OutputLCP) *lcp_result++ = lcp1;
	if (OutputLCP) *cache_result++ = cache1;
	std::copy(from1, from1+n1, result);
	if (OutputLCP) std::copy(lcp_input1, lcp_input1+n1, lcp_result);
	if (OutputLCP) std::copy(cache_input1, cache_input1+n1, cache_result);
	debug() << '~' << __func__ << '\n';
	return;
finish1:
	assert(n0);
	assert(n1==0);
	if (OutputLCP) *lcp_result++ = lcp0;
	if (OutputLCP) *cache_result++ = cache0;
	std::copy(from0, from0+n0, result);
	if (OutputLCP) std::copy(lcp_input0, lcp_input0+n0, lcp_result);
	if (OutputLCP) std::copy(cache_input0, cache_input0+n0, cache_result);
	debug() << '~' << __func__ << '\n';
	return;
}

template <bool OutputLCP, typename CharT>
MergeResult
mergesort_cache_lcp_2way(
              unsigned char** strings_input, unsigned char** strings_output,
              lcp_t* restrict lcp_input, lcp_t* restrict lcp_output,
              CharT* restrict cache_input, CharT* restrict cache_output,
              size_t n)
{
	debug() << __func__ << "(): n=" << n << '\n';
	if (n < 32) {
		insertion_sort(strings_input, n, 0);
		lcp_input[0] = lcp(strings_input[0], strings_input[1]);
		cache_input[0] = get_char<CharT>(strings_input[0], 0);
		for (unsigned i=1; i < n-1; ++i) {
			lcp_input[i] = lcp(strings_input[i], strings_input[i+1]);
			cache_input[i] = get_char<CharT>(strings_input[i], lcp_input[i-1]);
		}
		cache_input[n-1] = get_char<CharT>(strings_input[n-1], lcp_input[n-2]);
		check_input(strings_input, lcp_input, cache_input, n);
		return SortedInPlace;
	}
	const size_t split0 = n/2;
	MergeResult m0 = mergesort_cache_lcp_2way<true>(
			strings_input, strings_output,
			lcp_input, lcp_output,
			cache_input, cache_output,
			split0);

	if (m0==SortedInPlace) check_input(strings_input, lcp_input, cache_input, split0);
	else                   check_input(strings_output, lcp_output, cache_output, split0);

	MergeResult m1 = mergesort_cache_lcp_2way<true>(
			strings_input+split0, strings_output+split0,
			lcp_input+split0, lcp_output+split0,
			cache_input+split0, cache_output+split0,
			n-split0);

	if (m0==SortedInPlace) check_input(strings_input+split0, lcp_input+split0, cache_input+split0, n-split0);
	else                   check_input(strings_output+split0, lcp_output+split0, cache_output+split0, n-split0);

	if (m0 != m1) {
		debug() << "Warning: extra copying due to m0 != m1. n="<<n<<"\n";
		if (m0 == SortedInPlace) {
			std::copy(strings_input, strings_input+split0,
					strings_output);
			std::copy(cache_input, cache_input+split0,
					cache_output);
			std::copy(lcp_input, lcp_input+split0,
					lcp_output);
			m0 = SortedInTemp;
		} else {
			std::copy(strings_output, strings_output+split0,
					strings_input);
			std::copy(cache_output, cache_output+split0,
					cache_input);
			std::copy(lcp_output, lcp_output+split0,
					lcp_input);
			m1 = SortedInTemp;
		}
	}
	assert(m0 == m1);
	if (m0 == SortedInPlace) {
		merge_cache_lcp_2way<OutputLCP>(
			strings_input, lcp_input, cache_input, split0,
			strings_input+split0, lcp_input+split0, cache_input+split0, n-split0,
			strings_output, lcp_output, cache_output);
		return SortedInTemp;
	} else {
		merge_cache_lcp_2way<OutputLCP>(
			strings_output, lcp_output, cache_output, split0,
			strings_output+split0, lcp_output+split0, cache_output+split0, n-split0,
			strings_input, lcp_input, cache_input);
		return SortedInPlace;
	}
}

template <typename CharT>
static void
mergesort_cache_lcp_2way(unsigned char** strings, size_t n)
{
	lcp_t* lcp_input = (lcp_t*) malloc(n*sizeof(lcp_t));
	lcp_t* lcp_tmp   = (lcp_t*) malloc(n*sizeof(lcp_t));
	unsigned char** input_tmp = (unsigned char**) malloc(n*sizeof(unsigned char*));
	CharT* cache     = (CharT*) malloc(n*sizeof(CharT));
	CharT* cache_tmp = (CharT*) malloc(n*sizeof(CharT));
	MergeResult m = mergesort_cache_lcp_2way<false>(strings, input_tmp,
			lcp_input, lcp_tmp, cache, cache_tmp, n);
	if (m == SortedInTemp) {
		memcpy(strings, input_tmp, n*sizeof(unsigned char*));
	}
	free(lcp_input);
	free(lcp_tmp);
	free(input_tmp);
	free(cache);
	free(cache_tmp);
	stat_print();
}

void mergesort_cache1_lcp_2way(unsigned char** strings, size_t n)
{ mergesort_cache_lcp_2way<unsigned char>(strings, n); }
void mergesort_cache2_lcp_2way(unsigned char** strings, size_t n)
{ mergesort_cache_lcp_2way<uint16_t>(strings, n); }
void mergesort_cache4_lcp_2way(unsigned char** strings, size_t n)
{ mergesort_cache_lcp_2way<uint32_t>(strings, n); }

ROUTINE_REGISTER_SINGLECORE(mergesort_cache1_lcp_2way,
		"LCP mergesort with 2way merger and 1byte cache")
ROUTINE_REGISTER_SINGLECORE(mergesort_cache2_lcp_2way,
		"LCP mergesort with 2way merger and 2byte cache")
ROUTINE_REGISTER_SINGLECORE(mergesort_cache4_lcp_2way,
		"LCP mergesort with 2way merger and 4byte cache")

template <bool OutputLCP, typename CharT>
MergeResult
mergesort_cache_lcp_2way_parallel(
              unsigned char** strings_input, unsigned char** strings_output,
              lcp_t* restrict lcp_input, lcp_t* restrict lcp_output,
              CharT* restrict cache_input, CharT* restrict cache_output,
              size_t n)
{
	debug() << __func__ << "(): n=" << n << '\n';
	if (n < 32) {
		insertion_sort(strings_input, n, 0);
		lcp_input[0] = lcp(strings_input[0], strings_input[1]);
		cache_input[0] = get_char<CharT>(strings_input[0], 0);
		for (unsigned i=1; i < n-1; ++i) {
			lcp_input[i] = lcp(strings_input[i], strings_input[i+1]);
			cache_input[i] = get_char<CharT>(strings_input[i], lcp_input[i-1]);
		}
		cache_input[n-1] = get_char<CharT>(strings_input[n-1], lcp_input[n-2]);
		check_input(strings_input, lcp_input, cache_input, n);
		return SortedInPlace;
	}
	const size_t split0 = n/2;
	MergeResult m0, m1;
	m0 = mergesort_cache_lcp_2way_parallel<true>(
			strings_input, strings_output,
			lcp_input, lcp_output,
			cache_input, cache_output,
			split0);

	if (m0==SortedInPlace) check_input(strings_input, lcp_input, cache_input, split0);
	else                   check_input(strings_output, lcp_output, cache_output, split0);

	m1 = mergesort_cache_lcp_2way_parallel<true>(
			strings_input+split0, strings_output+split0,
			lcp_input+split0, lcp_output+split0,
			cache_input+split0, cache_output+split0,
			n-split0);

	if (m0==SortedInPlace) check_input(strings_input+split0, lcp_input+split0, cache_input+split0, n-split0);
	else                   check_input(strings_output+split0, lcp_output+split0, cache_output+split0, n-split0);

	if (m0 != m1) {
		debug() << "Warning: extra copying due to m0 != m1. n="<<n<<"\n";
		if (m0 == SortedInPlace) {
			std::copy(strings_input, strings_input+split0,
					strings_output);
			std::copy(cache_input, cache_input+split0,
					cache_output);
			std::copy(lcp_input, lcp_input+split0,
					lcp_output);
			m0 = SortedInTemp;
		} else {
			std::copy(strings_output, strings_output+split0,
					strings_input);
			std::copy(cache_output, cache_output+split0,
					cache_input);
			std::copy(lcp_output, lcp_output+split0,
					lcp_input);
			m1 = SortedInTemp;
		}
	}
	assert(m0 == m1);
	if (m0 == SortedInPlace) {
		merge_cache_lcp_2way<OutputLCP>(
			strings_input, lcp_input, cache_input, split0,
			strings_input+split0, lcp_input+split0, cache_input+split0, n-split0,
			strings_output, lcp_output, cache_output);
		return SortedInTemp;
	} else {
		merge_cache_lcp_2way<OutputLCP>(
			strings_output, lcp_output, cache_output, split0,
			strings_output+split0, lcp_output+split0, cache_output+split0, n-split0,
			strings_input, lcp_input, cache_input);
		return SortedInPlace;
	}
}

template <typename CharT>
static void
mergesort_cache_lcp_2way_parallel(unsigned char** strings, size_t n)
{
	lcp_t* lcp_input = (lcp_t*) malloc(n*sizeof(lcp_t));
	lcp_t* lcp_tmp   = (lcp_t*) malloc(n*sizeof(lcp_t));
	unsigned char** input_tmp = (unsigned char**) malloc(n*sizeof(unsigned char*));
	CharT* cache     = (CharT*) malloc(n*sizeof(CharT));
	CharT* cache_tmp = (CharT*) malloc(n*sizeof(CharT));
	MergeResult m = mergesort_cache_lcp_2way_parallel<false>(strings, input_tmp,
			lcp_input, lcp_tmp, cache, cache_tmp, n);
	if (m == SortedInTemp) {
		memcpy(strings, input_tmp, n*sizeof(unsigned char*));
	}
	free(lcp_input);
	free(lcp_tmp);
	free(input_tmp);
	free(cache);
	free(cache_tmp);
	stat_print();
}

void mergesort_cache1_lcp_2way_parallel(unsigned char** strings, size_t n)
{ mergesort_cache_lcp_2way_parallel<unsigned char>(strings, n); }
void mergesort_cache2_lcp_2way_parallel(unsigned char** strings, size_t n)
{ mergesort_cache_lcp_2way_parallel<uint16_t>(strings, n); }
void mergesort_cache4_lcp_2way_parallel(unsigned char** strings, size_t n)
{ mergesort_cache_lcp_2way_parallel<uint32_t>(strings, n); }

ROUTINE_REGISTER_MULTICORE(mergesort_cache1_lcp_2way_parallel,
		"Parallel LCP mergesort with 2way merger and 1byte cache")
ROUTINE_REGISTER_MULTICORE(mergesort_cache2_lcp_2way_parallel,
		"Parallel LCP mergesort with 2way merger and 2byte cache")
ROUTINE_REGISTER_MULTICORE(mergesort_cache4_lcp_2way_parallel,
		"Parallel LCP mergesort with 2way merger and 4byte cache")

/*******************************************************************************
 *
 * mergesort_lcp_2way_unstable
 *
 ******************************************************************************/

static void
check_lcps(unsigned char* latest,
           unsigned char* from0, lcp_t lcp0,
           unsigned char* from1, lcp_t lcp1)
{
	debug()<<"******** CHECK ********\n"
	       <<"Latest: '"<<latest<<"'\n"
	       <<"     0: '"<<from0<<"', lcp="<<lcp0<<"\n"
	       <<"     1: '"<<from1<<"', lcp="<<lcp1<<"\n"
	       <<"***********************\n";
	assert(lcp(latest, from0) == lcp0);
	assert(lcp(latest, from1) == lcp1);
}

template <bool OutputLCP>
static void
merge_lcp_2way_unstable(
               unsigned char** from0,  lcp_t* restrict lcp_input0, size_t n0,
               unsigned char** from1,  lcp_t* restrict lcp_input1, size_t n1,
               unsigned char** result, lcp_t* restrict lcp_result)
{
	debug() << __func__ << "(): n0=" << n0 << ", n1=" << n1 << '\n';
	check_input(from0, lcp_input0, n0);
	check_input(from1, lcp_input1, n1);
	lcp_t lcp0=0, lcp1=0;
	int cmp01; lcp_t lcp01;
	std::tie(cmp01, lcp01) = compare(*from0, *from1);
	if (cmp01 <= 0) {
		*result++ = *from0++;
		lcp0 = *lcp_input0++;
		lcp1 = lcp01;
		if (--n0 == 0) goto finish0;
	} else {
		*result++ = *from1++;
		lcp1 = *lcp_input1++;
		lcp0 = lcp01;
		if (--n1 == 0) goto finish1;
	}
	while (true) {
		debug() << "Starting loop, n0="<<n0<<", n1="<<n1<<" ...\n"
		        << "\tprev  = '" <<*(result-1)<<"'\n"
		        << "\tfrom0 = '"<<*from0<<"'\n"
		        << "\tfrom1 = '"<<*from1<<"'\n"
		        << "\tlcp0  = " << lcp0 << "\n"
		        << "\tlcp1  = " << lcp1 << "\n"
		        << "\n";
		check_lcps(*(result-1),*from0,lcp0,*from1,lcp1);
		if (lcp0 > lcp1) {
			assert(cmp(*from0, *from1) < 0);
			*result++ = *from0++;
			if (OutputLCP) *lcp_result++ = lcp0;
			lcp0 = *lcp_input0++;
			if (--n0 == 0) goto finish0;
		} else if (lcp0 < lcp1) {
			assert(cmp(*from0, *from1) > 0);
			*result++ = *from1++;
			if (OutputLCP) *lcp_result++ = lcp1;
			lcp1 = *lcp_input1++;
			if (--n1 == 0) goto finish1;
		} else {
			int cmp01; lcp_t lcp01;
			std::tie(cmp01,lcp01) = compare(*from0, *from1, lcp0);
			if (cmp01 < 0) {
				*result++ = *from0++;
				if (OutputLCP) *lcp_result++ = lcp0;
				lcp1 = lcp01;
				if (--n0 == 0) goto finish0;
				lcp0 = *lcp_input0++;
			} else if (cmp01 == 0) {
				assert(cmp(*from0, *from1)==0);
				assert(n0); assert(n1);
				if (OutputLCP) *lcp_result++ = lcp0;
				if (OutputLCP) *lcp_result++ = lcp01;
				*result++ = *from0++;
				*result++ = *from1++;
				--n0;
				--n1;
				lcp0 = *lcp_input0++;
				lcp1 = *lcp_input1++;
				if (n0 == 0) goto finish0;
				if (n1 == 0) goto finish1;
			} else {
				*result++ = *from1++;
				if (OutputLCP) *lcp_result++ = lcp0;
				lcp0 = lcp01;
				if (--n1 == 0) goto finish1;
				lcp1 = *lcp_input1++;
			}
		}
	}
finish0:
	debug() << '~' << __func__ << "(): n0=" << n0 << ", n1=" << n1 << '\n';
	assert(not n0);
	if (n1) {
		std::copy(from1, from1+n1, result);
		if (OutputLCP) *lcp_result++ = lcp1;
		if (OutputLCP) std::copy(lcp_input1, lcp_input1+n1, lcp_result);
	}
	return;
finish1:
	debug() << '~' << __func__ << "(): n0=" << n0 << ", n1=" << n1 << '\n';
	assert(not n1);
	if (n0) {
		std::copy(from0, from0+n0, result);
		if (OutputLCP) *lcp_result++ = lcp0;
		if (OutputLCP) std::copy(lcp_input0, lcp_input0+n0, lcp_result);
	}
	return;
}

template <bool OutputLCP>
MergeResult
mergesort_lcp_2way_unstable(unsigned char** restrict strings_input,
                            unsigned char** restrict strings_output,
                            lcp_t* restrict lcp_input,
                            lcp_t* restrict lcp_output,
                            size_t n)
{
	debug() << __func__ << "(): n=" << n << '\n';
	if (n < 32) {
		insertion_sort(strings_input, n, 0);
		for (unsigned i=0; i < n-1; ++i)
			lcp_input[i] = lcp(strings_input[i], strings_input[i+1]);
		return SortedInPlace;
	}
	const size_t split0 = n/2;
	MergeResult ml = mergesort_lcp_2way_unstable<true>(
			strings_input, strings_output,
			lcp_input,     lcp_output,
			split0);

	debug() << __func__ << "(): checking first merge\n";
	if (ml == SortedInPlace) check_input(strings_input,  lcp_input,  split0);
	else                     check_input(strings_output, lcp_output, split0);

	MergeResult mr = mergesort_lcp_2way_unstable<true>(
			strings_input+split0, strings_output+split0,
			lcp_input+split0,     lcp_output+split0,
			n-split0);

	debug() << __func__ << "(): checking second merge\n";
	if (mr == SortedInPlace) check_input(strings_input+split0,  lcp_input+split0,  n-split0);
	else                     check_input(strings_output+split0, lcp_output+split0, n-split0);

	if (ml != mr) {
		if (ml == SortedInPlace) {
			std::copy(strings_output+split0, strings_output+n,
					strings_input+split0);
			std::copy(lcp_output+split0, lcp_output+n,
					lcp_input+split0);
			mr = SortedInPlace;
		} else {
			assert(0);
			abort();
		}
	}
	if (ml == SortedInPlace) {
		merge_lcp_2way_unstable<OutputLCP>(
		           strings_input,        lcp_input,        split0,
		           strings_input+split0, lcp_input+split0, n-split0,
		           strings_output, lcp_output);
		return SortedInTemp;
	} else {
		merge_lcp_2way_unstable<OutputLCP>(
		           strings_output,        lcp_output,        split0,
		           strings_output+split0, lcp_output+split0, n-split0,
		           strings_input, lcp_input);
		return SortedInPlace;
	}
}
void
mergesort_lcp_2way_unstable(unsigned char** strings, size_t n)
{
	lcp_t* lcp_input  = static_cast<lcp_t*>(malloc(n*sizeof(lcp_t)));
	lcp_t* lcp_output = static_cast<lcp_t*>(malloc(n*sizeof(lcp_t)));
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	const MergeResult m = mergesort_lcp_2way_unstable<false>(strings, tmp,
			lcp_input, lcp_output, n);
	if (m == SortedInTemp) {
		(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
	}
	free(lcp_input);
	free(lcp_output);
	free(tmp);
}
ROUTINE_REGISTER_SINGLECORE(mergesort_lcp_2way_unstable,
		"Unstable LCP mergesort with 2way merger")

template <bool OutputLCP>
MergeResult
mergesort_lcp_2way_unstable_parallel(unsigned char** restrict strings_input,
                            unsigned char** restrict strings_output,
                            lcp_t* restrict lcp_input,
                            lcp_t* restrict lcp_output,
                            size_t n)
{
	debug() << __func__ << "(): n=" << n << '\n';
	if (n < 32) {
		insertion_sort(strings_input, n, 0);
		for (unsigned i=0; i < n-1; ++i)
			lcp_input[i] = lcp(strings_input[i], strings_input[i+1]);
		return SortedInPlace;
	}
	const size_t split0 = n/2;
	MergeResult ml, mr;
#pragma omp parallel sections
	{
#pragma omp section
	ml = mergesort_lcp_2way_unstable_parallel<true>(
			strings_input, strings_output,
			lcp_input,     lcp_output,
			split0);
#pragma omp section
	mr = mergesort_lcp_2way_unstable_parallel<true>(
			strings_input+split0, strings_output+split0,
			lcp_input+split0,     lcp_output+split0,
			n-split0);
	}
	if (ml != mr) {
		if (ml == SortedInPlace) {
			std::copy(strings_output+split0, strings_output+n,
					strings_input+split0);
			std::copy(lcp_output+split0, lcp_output+n,
					lcp_input+split0);
			mr = SortedInPlace;
		} else {
			assert(0);
			abort();
		}
	}
	if (ml == SortedInPlace) {
		merge_lcp_2way_unstable<OutputLCP>(
		           strings_input,        lcp_input,        split0,
		           strings_input+split0, lcp_input+split0, n-split0,
		           strings_output, lcp_output);
		return SortedInTemp;
	} else {
		merge_lcp_2way_unstable<OutputLCP>(
		           strings_output,        lcp_output,        split0,
		           strings_output+split0, lcp_output+split0, n-split0,
		           strings_input, lcp_input);
		return SortedInPlace;
	}
}
void
mergesort_lcp_2way_unstable_parallel(unsigned char** strings, size_t n)
{
	lcp_t* lcp_input  = static_cast<lcp_t*>(malloc(n*sizeof(lcp_t)));
	lcp_t* lcp_output = static_cast<lcp_t*>(malloc(n*sizeof(lcp_t)));
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	const MergeResult m = mergesort_lcp_2way_unstable_parallel<false>(strings, tmp,
			lcp_input, lcp_output, n);
	if (m == SortedInTemp) {
		(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
	}
	free(lcp_input);
	free(lcp_output);
	free(tmp);
}
ROUTINE_REGISTER_MULTICORE(mergesort_lcp_2way_unstable_parallel,
		"Parallel unstable LCP mergesort with 2way merger")
