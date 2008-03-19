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

   Stefan Nilsson, 8 jan 1997.

   Laboratory of Information Processing Science
   Helsinki University of Technology
   Stefan.Nilsson@hut.fi
*/

#include "sortstring.h"

#ifndef min
#define min(a, b) ((a)<=(b) ? (a) : (b))
#endif

#define swap(a, b) { string t=x[a]; \
                     x[a]=x[b]; x[b]=t; }
#define i2c(i) x[i][depth]

static void vecswap(int i, int j, int n, string x[])
{   while (n-- > 0) {
        swap(i, j);
        i++;
        j++;
    }
}

static void ssort1(string x[], int n, int depth)
{   int    a, b, c, d, r, v;
    if (n <= 1)
        return;
    a = rand() % n;
    swap(0, a);
    v = i2c(0);
    a = b = 1;
    c = d = n-1;
    for (;;) {
        while (b <= c && (r = i2c(b)-v) <= 0) {
            if (r == 0) { swap(a, b); a++; }
            b++;
        }
        while (b <= c && (r = i2c(c)-v) >= 0) {
            if (r == 0) { swap(c, d); d--; }
            c--;
        }
        if (b > c) break;
        swap(b, c);
        b++;
        c--;
    }
    r = min(a, b-a);     vecswap(0, b-r, r, x);
    r = min(d-c, n-d-1); vecswap(b, n-r, r, x);
    r = b-a; ssort1(x, r, depth);
    if (i2c(r) != 0)
        ssort1(x + r, a + n-d-1, depth+1);
    r = d-c; ssort1(x + n-r, r, depth);
}

void multikey1(string x[], int n)
{ ssort1(x, n, 0); }


/* ssort2 -- Faster Version of Multikey Quicksort */

static void vecswap2(string *a, string *b, int n)
{   while (n-- > 0) {
        string t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

#define swap2(a, b) { t = *(a); *(a) = *(b); *(b) = t; }
#define ptr2char(i) (*(*(i) + depth))

static string *med3func(string *a, string *b, string *c, int depth)
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

static void insertsort(string *a, int n, int d)
{   string *pi, *pj, s, t;
    for (pi = a + 1; --n > 0; pi++)
        for (pj = pi; pj > a; pj--) {
            /* Inline strcmp: break if *(pj-1) <= *pj */
            for (s=*(pj-1)+d, t=*pj+d; *s==*t && *s!=0; s++, t++)
                ;
            if (*s <= *t)
                break;
            swap2(pj, pj-1);
    }
}

static void ssort2(string a[], int n, int depth)
{   int d, r, partval;
    string *pa, *pb, *pc, *pd, *pl, *pm, *pn, t;
    if (n < 10) {
        insertsort(a, n, depth);
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
        ssort2(a, r, depth);
    if (ptr2char(a + r) != 0)
        ssort2(a + r, pa-a + pn-pd-1, depth+1);
    if ((r = pd-pc) > 1)
        ssort2(a + n-r, r, depth);
}

void multikey2(string a[], size_t n) { ssort2(a, n, 0); }
