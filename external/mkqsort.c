/*
   Multikey quicksort, a radix sort algorithm for arrays of character
   strings by Bentley and Sedgewick.

   J. Bentley and R. Sedgewick. Fast algorithms for sorting and
   searching strings. In Proceedings of 8th Annual ACM-SIAM Symposium
   on Discrete Algorithms, 1997.

   http://www.CS.Princeton.EDU/~rs/strings/index.html

   The code presented in this file has been tested with care but is
   not guaranteed for any purpose. The writer does not offer any
   warranties nor does he accept any liabilities with respect to
   the code.

   Ranjan Sinha, 1 jan 2003.

   School of Computer Science and Information Technology,
   RMIT University, Melbourne, Australia
   rsinha@cs.rmit.edu.au

*/

#include "utils.h"

/* MULTIKEY QUICKSORT */

#ifndef min
#define min(a, b) ((a)<=(b) ? (a) : (b))
#endif

/* ssort2 -- Faster Version of Multikey Quicksort */

void vecswap2(unsigned char **a, unsigned char **b, int n)
{   while (n-- > 0) {
        unsigned char *t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

#define swap2(a, b) { t = *(a); *(a) = *(b); *(b) = t; }
#define ptr2char(i) (*(*(i) + depth))

unsigned char **med3func(unsigned char **a, unsigned char **b, unsigned char **c, int depth)
{   int va, vb, vc;
    if ((va=ptr2char(a)) == (vb=ptr2char(b)))
        return a;
    if ((vc=ptr2char(c)) == va || vc == vb)
        return c;       
    return va < vb ?
          (vb < vc ? b : (va < vc ? c : a ) )
        : (vb > vc ? b : (va < vc ? a : c ) );
}
#define med3(a, b, c) med3func(a, b, c, depth)

void mkqsort(unsigned char **a, int n, int depth)
{   int d, r, partval;
    unsigned char **pa, **pb, **pc, **pd, **pl, **pm, **pn, *t;
    if (n < 20) {
        inssort(a, n, depth);
        return;
    }
    pl = a;
    pm = a + (n/2);
    pn = a + (n-1);
    if (n > 30) { /* On big arrays, pseudomedian of 9 */
        d = (n/8);
        pl = med3(pl, pl+d, pl+2*d);
        pm = med3(pm-d, pm, pm+d);
        pn = med3(pn-2*d, pn-d, pn);
    }
    pm = med3(pl, pm, pn);
    swap2(a, pm);
    partval = ptr2char(a);
    pa = pb = a + 1;
    pc = pd = a + n-1;
    for (;;) {
        while (pb <= pc && (r = ptr2char(pb)-partval) <= 0) {
            if (r == 0) { swap2(pa, pb); pa++; }
            pb++;
        }
        while (pb <= pc && (r = ptr2char(pc)-partval) >= 0) {
            if (r == 0) { swap2(pc, pd); pd--; }
            pc--;
       }
        if (pb > pc) break;
        swap2(pb, pc);
        pb++;
        pc--;
    }
    pn = a + n;
    r = min(pa-a, pb-pa);    vecswap2(a,  pb-r, r);
    r = min(pd-pc, pn-pd-1); vecswap2(pb, pn-r, r);
    if ((r = pb-pa) > 1)
        mkqsort(a, r, depth);
    if (ptr2char(a + r) != 0)
        mkqsort(a + r, pa-a + pn-pd-1, depth+1);
    if ((r = pd-pc) > 1)
        mkqsort(a + n-r, r, depth);
}

void mkqsort_main(unsigned char **a, int n) { mkqsort(a, n, 0); }
