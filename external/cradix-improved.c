/* This source code is from the following article:
 *
 * @article{1226858,
 *     author = {Waihong Ng and Katsuhiko Kakehi},
 *     title = {Cache Efficient Radix Sort for String Sorting},
 *     journal = {IEICE Trans. Fundam. Electron. Commun. Comput. Sci.},
 *     volume = {E90-A},
 *     number = {2},
 *     year = {2007},
 *     issn = {0916-8508},
 *     pages = {457--466},
 *     doi = {http://dx.doi.org/10.1093/ietfec/e90-a.2.457},
 *     publisher = {Oxford University Press},
 *     address = {Oxford, UK},
 * }
 *
 * Appendix: Source Code of CRadix Sort
 */

/*
  This code is based on the program 3.1 of Engineering radix sort by P.M.
  McIlroy, K. Bostic and M.D. McIlroy, Comput. Syst. vol.6, 1993.

  The main improvement is the adoption of the key buffer. Key buffers are
  filled by the function FillKeyBuffer() while the following code in the main
  body is responsible for permuting the key buffers in the order as same as the
  access order as the key pointers

       memcpy(ta, tk, sizeof(unsigned char)*n*kbsd);
       for (i=0, kb=(LPBYTE)ta; i<n;
             i++, *t+=kbsd1) {
          t=&GrpKB[*kb]; ss=*t; tt=kb+1;
          for (j=0; j<kbsd1; j++)
              { *ss=*tt; ss++; tt++; }
          kb+=kbsd;
        }

  Other improvements described in Sect. 6 are as follows.

  The following statements in the main body implement
  the improvement described in Sect. 6.1

        cptr=&count[AL];
	while (*cptr<1) cptr++;
	if (*cptr<n) gs=n; else gs=0;

  RDFK() -- the function which read the keys directly as described in Sect. 6.2

  Other modifications are mainly for assembling the improvements into the
  original code
*/

/* Copyright 2008 by Tommi Rantala <tt.rantala@gmail.com>
 *  - replace original isort() with slightly different insertion_sort()
 *  - re-implement FillKeyBuffer() to reduce memory stalls
 *  - use caching in RDFK() to reduce memory stalls
 */

#include "routine.h"
#include <stdlib.h>
#include <string.h>

#define AS 256 /* Alphabet size */
#define BS 4 /* key buffer size */
#define AL 0 /* Alphabet lower bound */
#define AH 255 /* Alphabet upper bound */
#define IC 20 /* Insertion sort cut off */
#define KBC 128 /* Cache cut off */
#define SS 4096 /* stack size */

#define push(a, k, n, b) _sp->sa=a, _sp->sk=k, _sp->sn=n, (_sp++)->sb=b
#define pop(a, k, n, b) a=(--_sp)->sa, k=_sp->sk, n=_sp->sn, b=_sp->sb
#define stackempty() (_sp<=stack)
#define splittable(c) c > 0 && count[c] > IC
typedef size_t UINT;
typedef unsigned char BYTE, *LPBYTE, **LPPBYTE;
typedef unsigned char STR, *LPSTR, **LPPSTR, **STRPARR;

struct Stack {
	LPSTR* sa; LPBYTE sk;
	int sn, sb;
} stack[SS], *_sp=stack;

static void
insertion_sort(unsigned char** strings, int n, size_t depth)
{
	for (unsigned char** i = strings + 1; --n > 0; ++i) {
		unsigned char** j = i;
		unsigned char* tmp = *i;
		while (j > strings) {
			unsigned char* s = *(j-1)+depth;
			unsigned char* t = tmp+depth;
			while (*s == *t && *s) {
				++s;
				++t;
			}
			if (*s <= *t) break;
			*j = *(j-1);
			--j;
		}
		*j = tmp;
	}
}

static
void FillKeyBuffer(LPPSTR a, LPBYTE kb, UINT* count, UINT n, UINT d)
{
	for (size_t i=0; i<n; ++i) {
		const unsigned char* str = a[i];
		unsigned j=0;
		for (; j<BS; ++j) {
			unsigned char c = str[d+j];
			kb[BS*i+j] = c;
			if (c==0) break;
		}
		if (j<BS) kb[BS*i+j] = 0;
	}
	// Make another sweep through data to calculate counts. Should be very
	// fast, because we access 'count' linearly from start to finish.
	for (size_t i=0; i<n; ++i) ++count[kb[BS*i]];
}

static
void RDFK(LPPSTR* GrpKP, LPPSTR a, UINT n, LPPSTR ta,
		UINT* count, UINT d)
{ /* Read Directly From Keys */
	LPPSTR ak, tc; UINT i, *cptr, gs; unsigned char c=0;
	for (i=0; i<n-n%32; i+=32) {
		unsigned char cache[32];
		for (unsigned j=0; j<32; ++j) cache[j] = a[i+j][d];
		for (unsigned j=0; j<32; ++j) ++count[cache[j]];
	}
	for (; i<n; i++) count[a[i][d]]++;
	cptr=&count[AL]; while (*cptr<1) cptr++;
	if (*cptr<n) gs=n;
	else { c=(cptr-&count[AL])+AL; gs=0; }
	if (!gs) {
		if (splittable(c)) push(a, 0, n, d+1);
		else if (n>1 && c>0) insertion_sort(a, n, d);
		count[c]=0; return;
	}
	GrpKP[AL]=a;
	for (ak=a, i=AL; i<AH; i++) GrpKP[i+1]=ak+=count[i];
	memcpy(ta, a, sizeof(LPSTR)*n);
	for (i=0, tc=ta; i<n; i++, tc++) {
		*GrpKP[ta[i][d]]=*tc; GrpKP[ta[i][d]]++;
	}
	for (ak=a, i=AL; i<AH; i++) {
		if (splittable(i)) push(ak, 0, count[i], d+1);
		else if (count[i]>1 && i>0) insertion_sort(ak, count[i], d);
		ak+=count[i]; count[i]=0;
	}
}

void cradix_rantala(LPPSTR a, UINT n)
{
	UINT kbsd, kbsd1, i, j, stage, d, MEMSIZE;
	UINT *cptr, gs, count[AS];
	LPSTR tj, tk, ax, tl, kb, ss, tt, GrpKB[AS];
	LPPSTR GrpKP[AS], ak, ta, tc, t;
	if (sizeof(LPPSTR)>sizeof(unsigned char)*BS)
		MEMSIZE=sizeof(LPPSTR);
	else
		MEMSIZE=sizeof(unsigned char)*BS;
	/* workspace */
	ta = (LPPSTR)malloc(n * MEMSIZE);
	/* memory for key buffers */
	tk = (LPBYTE)malloc(n * sizeof(unsigned char) * BS);
	tj=tk;
	push(a, tk, n, 0); for (i=AL; i<AH; i++) count[i]=0;
	while (!stackempty()) {
		pop(a, tk, n, stage);
		if (tk) {
			/* set the counters and
			   fill the key buffers if necessary */
			if ((d=stage%BS)!=0)
				for (i=0, tl=tk; i<n; i++, tl+=(BS-d))
					count[*tl]++;
			else {
				if (n>KBC)
					FillKeyBuffer(a, tk, count, n, stage);
				else {
					RDFK(GrpKP, a, n, ta, count, stage);
					continue;
				}
			}
			/* check if there is only 1 group */
			cptr=&count[AL];
			while (*cptr<1) cptr++;
			if (*cptr<n) gs=n; else gs=0;
			/* calculate both key ptr and
			   key buffer addresses */
			kbsd=BS-d, kbsd1=kbsd-1;
			GrpKP[AL]=a; GrpKB[AL]=tk;
			for (ak=a, ax=tk, i=AL; i<AH; i++) {
				GrpKP[i+1]=ak+=count[i];
				GrpKB[i+1]=ax+=count[i]*kbsd1;
			}

			/* permute the key ptrs */
			memcpy(ta, a, sizeof(LPSTR)*gs);
			for (i=0, ax=tk, tc=ta; i<gs;
					i++, ax+=kbsd, tc++) {
				*GrpKP[*ax]=*tc; GrpKP[*ax]++;
			}
			/* permute the key buffers */
			memcpy(ta, tk, sizeof(unsigned char)*n*kbsd);
			for (i=0, kb=(LPBYTE)ta; i<n; i++, *t+=kbsd1) {
				t=&GrpKB[*kb]; ss=*t; tt=kb+1;
				for (j=0; j<kbsd1; j++)
				{ *ss=*tt; ss++; tt++; }
				kb+=kbsd;
			}
			/* down 1 level */
			for (ak=a, ax=tk, i=AL; i<AH; i++) {
				if (splittable(i))
				{ push(ak, ax, count[i], stage+1); }
				else if (count[i]>1 && i>0)
					insertion_sort(ak, count[i], stage);
				ak+=count[i]; ax+=count[i]*(kbsd1);
				count[i]=0;
			}
		}
		else RDFK(GrpKP, a, n, ta, count, stage);
	}
	free((void*)ta);
	free((void*)tj);
}
ROUTINE_REGISTER_SINGLECORE(cradix_rantala,
		"CRadix by Waihong Ng and Katsuhiko Kakehi,"
		" with modifications by Tommi Rantala")
