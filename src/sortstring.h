/*
 * Copyright 2007-2008 by Tommi Rantala <tommi.rantala@cs.helsinki.fi>
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

#ifndef SORTSTRING_H
#define SORTSTRING_H

#include <cstdio>
#include <cstring>
#include <cstdlib>

#define CHARS 256
#define INSERTBREAK 20
typedef unsigned char* string;

extern "C" {
void quicksort(unsigned char**,  size_t);
void mbmradix(unsigned char**,   size_t);
void multikey2(unsigned char**,  size_t);
void arssort(unsigned char**,    size_t);
void frssort(unsigned char**,    size_t);
void frssort1(unsigned char**,   size_t);
void MSDsort(unsigned char**,    size_t);
void burstsortL(unsigned char**, size_t);
void burstsortA(unsigned char**, size_t);
void CRadix(unsigned char**,     size_t);
void cradix_improved(unsigned char**, size_t);
}

void msd_CE0(unsigned char**, size_t);
void msd_CE1(unsigned char**, size_t);
void msd_CE2(unsigned char**, size_t);
void msd_CE3(unsigned char**, size_t);

void msd_ci(unsigned char**, size_t);
void msd_ci_adaptive(unsigned char**, size_t);

void msd_DV(unsigned char**, size_t);
void msd_DV_adaptive(unsigned char**, size_t);
void msd_DL(unsigned char**, size_t);
void msd_DL_adaptive(unsigned char**, size_t);
void msd_DD(unsigned char**, size_t);
void msd_DD_adaptive(unsigned char**, size_t);

void msd_DV_REALLOC(unsigned char**, size_t);
void msd_DV_REALLOC_adaptive(unsigned char**, size_t);
void msd_DV_MALLOC(unsigned char**, size_t);
void msd_DV_MALLOC_adaptive(unsigned char**, size_t);
void msd_DV_CHEAT_REALLOC(unsigned char**, size_t);
void msd_DV_CHEAT_REALLOC_adaptive(unsigned char**, size_t);
void msd_DV_CHEAT_MALLOC(unsigned char**, size_t);
void msd_DV_CHEAT_MALLOC_adaptive(unsigned char**, size_t);

void msd_D_vector_block(unsigned char**, size_t);
void msd_D_vector_brodnik(unsigned char**, size_t);
void msd_D_vector_bagwell(unsigned char**, size_t);
void msd_D_vector_block_adaptive(unsigned char**, size_t);
void msd_D_vector_brodnik_adaptive(unsigned char**, size_t);
void msd_D_vector_bagwell_adaptive(unsigned char**, size_t);

void msd_DB(unsigned char**, size_t);
void msd_A(unsigned char**, size_t);
void msd_A_adaptive(unsigned char**, size_t);
void msd_A2(unsigned char**, size_t);
void msd_A2_adaptive(unsigned char**, size_t);

void burstsort_vector(unsigned char**, size_t);
void burstsort_brodnik(unsigned char**, size_t);
void burstsort_bagwell(unsigned char**, size_t);
void burstsort_vector_block(unsigned char**, size_t);
void burstsort_superalphabet_vector(unsigned char**, size_t);
void burstsort_superalphabet_brodnik(unsigned char**, size_t);
void burstsort_superalphabet_bagwell(unsigned char**, size_t);
void burstsort_superalphabet_vector_block(unsigned char**, size_t);
void burstsort_sampling_vector(unsigned char**, size_t);
void burstsort_sampling_brodnik(unsigned char**, size_t);
void burstsort_sampling_bagwell(unsigned char**, size_t);
void burstsort_sampling_vector_block(unsigned char**, size_t);
void burstsort_sampling_superalphabet_vector(unsigned char**, size_t);
void burstsort_sampling_superalphabet_brodnik(unsigned char**, size_t);
void burstsort_sampling_superalphabet_bagwell(unsigned char**, size_t);
void burstsort_sampling_superalphabet_vector_block(unsigned char**, size_t);

void burstsort2_vector(unsigned char**, size_t);
void burstsort2_brodnik(unsigned char**, size_t);
void burstsort2_bagwell(unsigned char**, size_t);
void burstsort2_vector_block(unsigned char**, size_t);
void burstsort2_superalphabet_vector(unsigned char**, size_t);
void burstsort2_superalphabet_brodnik(unsigned char**, size_t);
void burstsort2_superalphabet_bagwell(unsigned char**, size_t);
void burstsort2_superalphabet_vector_block(unsigned char**, size_t);
void burstsort2_sampling_vector(unsigned char**, size_t);
void burstsort2_sampling_brodnik(unsigned char**, size_t);
void burstsort2_sampling_bagwell(unsigned char**, size_t);
void burstsort2_sampling_vector_block(unsigned char**, size_t);
void burstsort2_sampling_superalphabet_vector(unsigned char**, size_t);
void burstsort2_sampling_superalphabet_brodnik(unsigned char**, size_t);
void burstsort2_sampling_superalphabet_bagwell(unsigned char**, size_t);
void burstsort2_sampling_superalphabet_vector_block(unsigned char**, size_t);

void burstsort_mkq_simpleburst_1(unsigned char**, size_t);
void burstsort_mkq_simpleburst_2(unsigned char**, size_t);
void burstsort_mkq_simpleburst_4(unsigned char**, size_t);
void burstsort_mkq_recursiveburst_1(unsigned char**, size_t);
void burstsort_mkq_recursiveburst_2(unsigned char**, size_t);
void burstsort_mkq_recursiveburst_4(unsigned char**, size_t);

void multikey_simd1(unsigned char**, size_t);
void multikey_simd2(unsigned char**, size_t);
void multikey_simd4(unsigned char**, size_t);
void multikey_dynamic_vector1(unsigned char**, size_t);
void multikey_dynamic_vector2(unsigned char**, size_t);
void multikey_dynamic_vector4(unsigned char**, size_t);
void multikey_dynamic_brodnik1(unsigned char**, size_t);
void multikey_dynamic_brodnik2(unsigned char**, size_t);
void multikey_dynamic_brodnik4(unsigned char**, size_t);
void multikey_dynamic_bagwell1(unsigned char**, size_t);
void multikey_dynamic_bagwell2(unsigned char**, size_t);
void multikey_dynamic_bagwell4(unsigned char**, size_t);
void multikey_dynamic_vector_block1(unsigned char**, size_t);
void multikey_dynamic_vector_block2(unsigned char**, size_t);
void multikey_dynamic_vector_block4(unsigned char**, size_t);
void multikey_block1(unsigned char**, size_t);
void multikey_block2(unsigned char**, size_t);
void multikey_block4(unsigned char**, size_t);
void multikey_multipivot_brute_simd1(unsigned char**, size_t);
void multikey_multipivot_brute_simd2(unsigned char**, size_t);
void multikey_multipivot_brute_simd4(unsigned char**, size_t);
void multikey_cache4(unsigned char**, size_t);
void multikey_cache8(unsigned char**, size_t);

void clockon();
void clockoff();
double gettime();

#endif //SORTSTRING_H
