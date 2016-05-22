#include <stdlib.h>
#include <algorithm>
#include <sys/types.h>
#include "routine.h"

typedef short int Lcp;

template<class CacheType>
class Metadata{
public:
  Lcp lcp;
  CacheType cache;

  void loadcache( unsigned char *s, Lcp l)
  {
    cache = ((CacheType) s[l])<< ((sizeof(CacheType)-1)*8) ;
    size_t b;
    for( b=1; b < sizeof(CacheType) && s[l+b]; b++ )
      cache |= ((CacheType) s[l+b] ) << (sizeof(CacheType)-b-1)*8;
    lcp = l+sizeof(CacheType);
  };

  bool cache_gt( Metadata<CacheType> x ) { return cache > x.cache ; };

  bool nonterminal() { return cache & 0xFF; };

};

template<>
class Metadata<void>{
public:
  Lcp lcp;

  void loadcache(unsigned char *s, Lcp l) {};
  bool nonterminal() { return true; };
  bool cache_gt( Metadata<void> x ) { return false; }
};

template<bool SSE>
Lcp strlcp( unsigned char *s, unsigned char *t, Lcp rlcp );

template<>
Lcp strlcp<true>(  unsigned char *s, unsigned char *t, Lcp rlcp ) {
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

template<>
Lcp strlcp<false>( unsigned char *s, unsigned char *t, Lcp rlcp ) {
  int i;
  for( i = rlcp; s[i]==t[i] && s[i]; i++ )
    ;
  return (Lcp) i;
}

template<class CacheType>
void exch( unsigned char *strings[], Metadata<CacheType> meta[], int I, int J) {
  std::swap(strings[I],strings[J]);
  std::swap(meta[I], meta[J]);
}

template<bool SSE, class CacheType>
void strsort(unsigned char * strings[], Metadata<CacheType> meta[], int lo, int hi );

template <bool ascending, bool SSE, class CacheType>
void lcpsort( unsigned char * strings[], Metadata<CacheType> meta[], int lo, int hi ) {
  if ( hi <= lo ) return;
  int lt = lo, gt = hi;

  Metadata<CacheType> pivot = meta[lo];
  for( int i = lo + 1; i <= gt; ) {
    if      ( ascending ? meta[i].lcp > pivot.lcp : meta[i].lcp < pivot.lcp ) exch<CacheType>( strings, meta, i, gt--);
    else if ( ascending ? meta[i].lcp < pivot.lcp : meta[i].lcp > pivot.lcp ) exch<CacheType>( strings, meta, lt++, i++);
    else if ( meta[i].cache_gt( pivot ) ) exch<CacheType>( strings, meta, i, gt--);
    else if ( pivot.cache_gt( meta[i] ) ) exch<CacheType>( strings, meta, lt++, i++);
    else            i++;
  }

  lcpsort<ascending, SSE>( strings, meta, lo, lt-1 );
  lcpsort<ascending, SSE>( strings, meta, gt+1, hi );
  if( pivot.nonterminal() )
    strsort<SSE, CacheType>( strings, meta, lt, gt );

};

template<class CacheType>
static void qexch( unsigned char **s, Metadata<CacheType> *l, int i, int dst, unsigned char *stmp, Metadata<CacheType> ltmp ) {
  s[i]=s[dst];
  l[i]=l[dst];
  s[dst]=stmp;
  l[dst]=ltmp;
}

template < bool SSE, class CacheType >
void strsort(unsigned char * strings[], Metadata<CacheType> meta[], int lo, int hi ) {
  if ( hi <= lo ) return;
  int lt = lo, gt = hi;

  unsigned char * pivotStr = strings[lo];
  long lcp = meta[lo].lcp;

  for( int i = lo + 1; i <= gt; ) {
      unsigned char *s = strings[i];
      Metadata<CacheType> j;
      Lcp l = strlcp<SSE>( pivotStr, s, lcp );
      j.loadcache(s, l);

      if      ( s[l] < pivotStr[l] ) qexch<CacheType>( strings, meta, i++, lt++, s, j);
      else if ( s[l] > pivotStr[l] ) qexch<CacheType>( strings, meta, i,   gt--, s, j);
      else            i++;
    }

  lcpsort<false, SSE, CacheType>( strings, meta, gt+1, hi );  
  lcpsort<true,  SSE, CacheType>( strings, meta, lo, lt-1 );
};

#define CTYPE uint8_t
extern "C" void lcpquicksort_simd_cache1( unsigned char  * strings[], size_t n ) {
  Metadata<CTYPE> *lcps = (Metadata<CTYPE> *) calloc( n, sizeof(Metadata<CTYPE>)); 
  int i;
  strsort<true, CTYPE>( strings, lcps, 0, n-1 );
  free(lcps);
}

ROUTINE_REGISTER_SINGLECORE( lcpquicksort_simd_cache1,
			     "LCP Quicksort SIMD with 1 byte cache")
#define CTYPE uint32_t
extern "C" void lcpquicksort_simd_cache4( unsigned char  * strings[], size_t n ) {
  Metadata<CTYPE> *lcps = (Metadata<CTYPE> *) calloc( n, sizeof(Metadata<CTYPE>)); 
  int i;
  strsort<true, CTYPE>( strings, lcps, 0, n-1 );
  free(lcps);
}

ROUTINE_REGISTER_SINGLECORE( lcpquicksort_simd_cache4,
			     "LCP Quicksort SIMD with 4 byte cache")
#define CTYPE uint64_t
extern "C" void lcpquicksort_simd_cache8( unsigned char  * strings[], size_t n ) {
  Metadata<CTYPE> *lcps = (Metadata<CTYPE> *) calloc( n, sizeof(Metadata<CTYPE>)); 
  int i;
  strsort<true, CTYPE>( strings, lcps, 0, n-1 );
  free(lcps);
}

ROUTINE_REGISTER_SINGLECORE( lcpquicksort_simd_cache8,
			     "LCP Quicksort SIMD with 8 byte cache")
#define CTYPE void
extern "C" void lcpquicksort_simd( unsigned char  * strings[], size_t n ) {
  Metadata<CTYPE> *lcps = (Metadata<CTYPE> *) calloc( n, sizeof(Metadata<CTYPE>)); 
  int i;
  strsort<true, CTYPE>( strings, lcps, 0, n-1 );
  free(lcps);
}

ROUTINE_REGISTER_SINGLECORE( lcpquicksort_simd,
			     "LCP Quicksort SIMD string compare")
#define CTYPE uint8_t
extern "C" void lcpquicksort_cache1( unsigned char  * strings[], size_t n ) {
  Metadata<CTYPE> *lcps = (Metadata<CTYPE> *) calloc( n, sizeof(Metadata<CTYPE>)); 
  int i;
  strsort<false, CTYPE>( strings, lcps, 0, n-1 );
  free(lcps);
}

ROUTINE_REGISTER_SINGLECORE( lcpquicksort_cache1,
			     "LCP Quicksort Template version")

#define CTYPE void
extern "C" void lcpquicksort_base( unsigned char  * strings[], size_t n ) {
  Metadata<CTYPE> *lcps = (Metadata<CTYPE> *) calloc( n, sizeof(Metadata<CTYPE>)); 
  int i;
  strsort<false, CTYPE>( strings, lcps, 0, n-1 );
  free(lcps);
}

ROUTINE_REGISTER_SINGLECORE( lcpquicksort_base,
			     "LCP Quicksort Template version no SIMD no Cache")
