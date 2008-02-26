/*
 *   Copyright 2007-2008 by Tommi Rantala <tommi.rantala@cs.helsinki.fi>
 *
 *   *************************************************************************
 *   * Use, modification, and distribution are subject to the Boost Software *
 *   * License, Version 1.0. (See accompanying file LICENSE or a copy at     *
 *   * http://www.boost.org/LICENSE_1_0.txt)                                 *
 *   *************************************************************************
 */

#ifndef SORTSTRING_H
#define SORTSTRING_H

#include <cstdio>
#include <cstring>
#include <cstdlib>

#define CHARS 256
#define INSERTBREAK 20
typedef unsigned char* string;

void BenStr(unsigned char**,     size_t);
void McIlroyCC(unsigned char**,  size_t);
void multikey2(unsigned char**,  size_t);
void arssort(unsigned char**,    size_t);
void frssort(unsigned char**,    size_t);
void frssort1(unsigned char**,   size_t);
void MSDsort(unsigned char**,    size_t);
void burstsortL(unsigned char**, size_t);
void burstsortA(unsigned char**, size_t);
void CRadix(unsigned char**,     size_t);

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

void msd_DB(unsigned char**, size_t);
void msd_A(unsigned char**, size_t);
void msd_A_adaptive(unsigned char**, size_t);

void clockon();
void clockoff();
double gettime();

#endif //SORTSTRING_H
