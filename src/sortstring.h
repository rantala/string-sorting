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

void msd_DB(unsigned char**, size_t);
void msd_A(unsigned char**, size_t);
void msd_A_adaptive(unsigned char**, size_t);

void clockon();
void clockoff();
double gettime();

#endif //SORTSTRING_H
