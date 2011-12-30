/*
   A stripped down version of a quicksort algorithm by Bentley
   and McIlroy. It sorts an array of pointers to strings.

   J. L. Bentley and M. D. McIlroy. Engineering a sort function.
   Software---Practice and Experience, 23(11):1249-1265, 1993.

   The code presented in this file has been tested with care but is
   not guaranteed for any purpose. The writer does not offer any
   warranties nor does he accept any liabilities with respect to
   the code.

   Stefan Nilsson, 2 jan 1997.

   Laboratory of Information Processing Science
   Helsinki University of Technology
   Stefan.Nilsson@hut.fi
*/

#include "routine.h"
#include <stddef.h>
#include "utils.h"

#define swap(a, b)  (t = (a), (a) = (b), (b) = t)
static void vecswap(int pa, int pb, int n, string a[])
{
   string t;
   for( ; n > 0; pa++, pb++, n--)
      swap(a[pa], a[pb]);
}

#define min(a, b) ((a) < (b) ? (a) : (b))

static int med3(int pa, int pb, int pc, string a[])
{   return scmp(a[pa], a[pb]) < 0 ?
       (scmp(a[pb], a[pc]) < 0 ? pb : scmp(a[pa], a[pc]) < 0 ? pc : pa)
     : (scmp(a[pb], a[pc]) > 0 ? pb : scmp(a[pa], a[pc]) > 0 ? pc : pa);
}

void quicksort(string a[], size_t n)
{
   int pa, pb, pc, pd, pl, pm, pn, r, s;
   string t;
   string pv;

   if (n < 10) {       /* Insertion sort on smallest arrays */
      for (pm = 1; pm < n; pm++)
         for (pl = pm; pl > 0 && scmp(a[pl-1], a[pl]) > 0; pl--)
            swap(a[pl], a[pl-1]);
      return;
   }
   pm = n/2;               /* Small arrays, middle element */
   if (n > 7) {
      pl = 0;
      pn = n-1;
      if (n > 40) {       /* Big arrays, pseudomedian of 9 */
         s = n/8;
         pl = med3(pl, pl+s, pl+2*s, a);
         pm = med3(pm-s, pm, pm+s, a);
         pn = med3(pn-2*s, pn-s, pn, a);
      }
      pm = med3(pl, pm, pn, a);      /* Mid-size, med of 3 */
   }
   pv = a[pm];
   pa = pb = 0;
   pc = pd = n-1;
   for (;;) {
      while (pb <= pc && (r = scmp(a[pb], pv)) <= 0) {
         if (r == 0) { swap(a[pa], a[pb]); pa++; }
         pb++;
      }
      while (pc >= pb && (r = scmp(a[pc], pv)) >= 0) {
         if (r == 0) { swap(a[pc], a[pd]); pd--; }
         pc--;
      }
      if (pb > pc) break;
      swap(a[pb], a[pc]);
      pb++;
      pc--;
   }
   pn = n;
   s = min(pa,  pb-pa   ); vecswap(0,  pb-s, s, a);
   s = min(pd-pc, pn-pd-1); vecswap(pb, pn-s, s, a);
   if ((s = pb-pa) > 1) quicksort(a,    s);
   if ((s = pd-pc) > 1) quicksort(&a[pn-s], s);
}

ROUTINE_REGISTER_SINGLECORE(quicksort,
		"Quicksort by J. L. Bentley and M. D. McIlroy")
