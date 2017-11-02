#include <stdlib.h>
#include <algorithm>
#include "routine.h"

typedef short int Lcp;

Lcp strlcp(  unsigned char *s, unsigned char *t, Lcp rlcp ) {
  long lcp = rlcp;
  __asm__ __volatile__
    (
     "1:                                          \n"
     "movdqu     (%[s], %[lcp]), %%xmm0           \n"    
     "pcmpistri  $0x18, (%[t],%[lcp]), %%xmm0     \n"
     "lea        16(%[lcp]), %[lcp]               \n"
     "ja 1b                                       \n"
     "and        $0xF,  %%rcx                     \n"  
     "lea        -16(%[lcp],%%rcx), %[lcp]        \n"
     : [lcp] "+r"(lcp)  
     : [s] "r"(s), [t] "r"(t)
     : "xmm0", "rcx" );
  return (Lcp) lcp;
}

void exch( unsigned char *strings[], Lcp lcps[], int I, int J) { 
  std::swap(strings[I],strings[J]);
  std::swap(lcps[I],lcps[J]);
}

void strsort(unsigned char * strings[], Lcp lcps[], int lo, int hi );

template <bool ascending>
void lcpsort( unsigned char * strings[], Lcp lcps[], int lo, int hi ) {
  if ( hi <= lo ) return;
  int lt = lo, gt = hi;

  if( hi > lo +100)
    exch(strings, lcps, lo, (lo+hi)/2);

  Lcp pivot = lcps[lo];
  for( int i = lo + 1; i <= gt; ) {
    if      ( ascending ? lcps[i] > pivot : lcps[i] < pivot ) exch( strings, lcps, i, gt--);
    else if ( ascending ? lcps[i] < pivot : lcps[i] > pivot ) exch( strings, lcps, lt++, i++);
    else            i++;
  }

  lcpsort<ascending>( strings, lcps, lo, lt-1 );
  lcpsort<ascending>( strings, lcps, gt+1, hi );
  strsort( strings, lcps, lt, gt );
};

static void qexch( unsigned char **s, Lcp *l, int i, int dst, unsigned char *stmp, Lcp ltmp ) {
  s[i]=s[dst];
  l[i]=l[dst];
  s[dst]=stmp;
  l[dst]=ltmp;
}

void strsort(unsigned char * strings[], Lcp lcps[], int lo, int hi ) {
  if ( hi <= lo ) return;
  int lt = lo, gt = hi;

  if( hi > lo +100)
    exch(strings, lcps, lo, (lo+hi)/2);

  unsigned char * pivotStr = strings[lo];
  Lcp lcp = lcps[lo];

  for( int i = lo + 1; i <= gt; ) {
      unsigned char *s = strings[i];
      Lcp j = strlcp( pivotStr, s, lcp );
      if      ( s[j] < pivotStr[j] ) qexch( strings, lcps, i++, lt++, s, j);
      else if ( s[j] > pivotStr[j] ) qexch( strings, lcps, i,   gt--, s, j);
      else            i++;
    }

  lcpsort<false>( strings, lcps, gt+1, hi );  
  lcpsort<true> ( strings, lcps, lo, lt-1 );
};

extern "C" void lcpquicksortSSE( unsigned char  * strings[], size_t n ) {
  Lcp *lcps = (Lcp *) calloc( n, sizeof(Lcp)); 
  strsort( strings, lcps, 0, n-1 );
  free(lcps);
}

ROUTINE_REGISTER_SINGLECORE( lcpquicksortSSE,
			     "LCP Quicksort with SSE comparisons by Kendall Willets")
