#include <stdlib.h>
#include <algorithm>
#include "routine.h"

typedef int Lcp;

inline int lcpstrcmp( unsigned char const * const p, unsigned char const * const q, Lcp &i) {
  for( ; !(q[i] - p[i]) && p[i]; i++ )
    ;
  return q[i]-p[i];
}

inline void exch( unsigned char  *strings[], Lcp lcps[], int I, int J) { 
  std::swap(strings[I],strings[J]);
  std::swap(lcps[I],lcps[J]);
}

void strsort(unsigned char * strings[], Lcp lcps[], int lo, int hi );

template <bool ascending>
void lcpsort( unsigned char * strings[], Lcp lcps[], int lo, int hi ) {
  if ( hi <= lo ) return;
  int lt = lo, gt = hi;

  Lcp pivot = lcps[lo];
  for( int i = lo + 1; i <= gt; ) {
    if      ( ascending ? lcps[i] > pivot : lcps[i] < pivot ) exch( strings, lcps, i, gt--);
    else if ( ascending ? lcps[i] < pivot : lcps[i] > pivot ) exch( strings, lcps, lt++, i++);
    else            i++;
  }

  strsort( strings, lcps, lt, gt );
  lcpsort<ascending>( strings, lcps, lo, lt-1 );
  lcpsort<ascending>( strings, lcps, gt+1, hi );
};

void strsort(unsigned char * strings[], Lcp lcps[], int lo, int hi )
{
  if ( hi <= lo ) return;
  int lt = lo, gt = hi;

  unsigned char  *  pivotStr = strings[lo];
  for( int i = lo + 1; i <= gt; )
    {
      int cmpr = lcpstrcmp( pivotStr, strings[i], lcps[i] );
      if      (cmpr < 0) exch( strings, lcps, lt++, i++);
      else if (cmpr > 0) exch( strings, lcps, i, gt--);
      else            i++;
    }

  lcpsort<true> ( strings, lcps, lo, lt-1 );
  lcpsort<false>( strings, lcps, gt+1, hi );  
};

extern "C" void lcpquicksort( unsigned char * strings[], size_t n ) {
  Lcp *lcps = (Lcp *) calloc( n, sizeof(Lcp)); 
  strsort( strings, lcps, 0, n-1 );
  free(lcps);
}

ROUTINE_REGISTER_SINGLECORE( lcpquicksort,
			    "LCP Quicksort by Kendall Willets")
