/*
 * Copyright 2008 by Tommi Rantala <tt.rantala@gmail.com>
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

/* Implements the lazy funnelsort algorithm for sorting strings. This
 * implementation approximates the theoretical funnel size scheme by using
 * predefined fixed size K-mergers. This allows compile time generation of the
 * mergers.
 *
 *     @inproceedings{796479,
 *           author = {Matteo Frigo and Charles E. Leiserson and Harald Prokop
 *                     and Sridhar Ramachandran},
 *           title = {Cache-Oblivious Algorithms},
 *           booktitle = {FOCS '99: Proceedings of the 40th Annual Symposium on
 *                        Foundations of Computer Science},
 *           year = {1999},
 *           isbn = {0-7695-0409-4},
 *           pages = {285},
 *           publisher = {IEEE Computer Society},
 *           address = {Washington, DC, USA},
 *     }
 *
 *     @inproceedings{684422,
 *           author = {Gerth St\olting Brodal and Rolf Fagerberg},
 *           title = {Cache Oblivious Distribution Sweeping},
 *           booktitle = {ICALP '02: Proceedings of the 29th International
 *                        Colloquium on Automata, Languages and Programming},
 *           year = {2002},
 *           isbn = {3-540-43864-5},
 *           pages = {426--438},
 *           publisher = {Springer-Verlag},
 *           address = {London, UK},
 *     }
 *
 *     @inproceedings{689897,
 *           author = {Gerth St\olting Brodal and Rolf Fagerberg},
 *           title = {Funnel Heap - A Cache Oblivious Priority Queue},
 *           booktitle = {ISAAC '02: Proceedings of the 13th International
 *                        Symposium on Algorithms and Computation},
 *           year = {2002},
 *           isbn = {3-540-00142-5},
 *           pages = {219--228},
 *           publisher = {Springer-Verlag},
 *           address = {London, UK},
 *     }
 *
 *     @article{1227164,
 *           author = {Gerth St\olting Brodal and Rolf Fagerberg and Kristoffer
 *                     Vinther},
 *           title = {Engineering a cache-oblivious sorting algorithm},
 *           journal = {Journal of Experimental Algorithmics},
 *           volume = {12},
 *           year = {2007},
 *           issn = {1084-6654},
 *           pages = {2.2},
 *           doi = {http://doi.acm.org/10.1145/1227161.1227164},
 *           publisher = {ACM},
 *           address = {New York, NY, USA},
 *     }
 */

#include "routine.h"
#include "util/debug.h"
#include "util/insertion_sort.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <array>
#include <type_traits>

static inline int
cmp(const unsigned char* a, const unsigned char* b)
{
	assert(a != 0);
	assert(b != 0);
	return strcmp(reinterpret_cast<const char*>(a),
	              reinterpret_cast<const char*>(b));
}

struct Stream { unsigned char** restrict stream; size_t n; };

static void
check_input(unsigned char** from0, size_t n0)
{
	(void) from0;
	(void) n0;
#ifndef NDEBUG
	for(size_t i=1;i<n0;++i)if(cmp(from0[i-1],from0[i])>0){
		debug()<<"Oops: ''"<<from0[i-1]<<"'' > ''"<<from0[i]<<"''\n";}
	for(size_t i=1;i<n0;++i)assert(cmp(from0[i-1],from0[i])<=0);
#endif
}

template <unsigned K>
static void
debug_print(const std::array<size_t,K>& buffer_count,
            const std::array<Stream,K>& streams,
            unsigned me, unsigned pos=1)
{
#ifndef NDEBUG
	debug_indent;
	if (pos >= K) {
		std::string sample = "[";
		for (size_t i=0; i < streams[pos-K].n; ++i) {
			sample
			   += std::string((char*)((streams[pos-K].stream)[i]))
			   += ", ";
		}
		if (sample.size()>2) sample = sample.substr(0,sample.size()-2);
		sample += "]";
		debug()<<"("<<streams[pos-K].n<<") "<<sample<<"\n";
		return;
	}
	debug_print(buffer_count, streams, me, 2*pos+1);
	if (pos==1) {
		if (pos==me) { debug()<<"(*)\n"; }
		else         { debug()<<"(o)\n"; }
	} else {
		if (pos==me) { debug()<<"* "<<buffer_count[pos]<<"\n"; }
		else         { debug()<<"- "<<buffer_count[pos]<<"\n"; }
	}
	debug_print(buffer_count, streams, me, 2*pos);
#endif
}

// Gives the buffer size of a given node.
template <unsigned K, unsigned I> struct buffer_size;
// Helper struct for subtree size calculation.
template <unsigned K, unsigned I, bool> struct subtree_size_fwd;
// Gives the total size of all buffers in a subtree.
template <unsigned K, unsigned I> struct subtree_size;
// Inner node:
template <unsigned K, unsigned I> struct subtree_size_fwd<K,I,true>
{ enum { value = 2*buffer_size<K,I>::value
                 + subtree_size<K,2*I>::value
                 + subtree_size<K,2*I+1>::value }; };
// Leaf node:
template <unsigned K, unsigned I> struct subtree_size_fwd<K,I,false>
{ enum { value=0 }; };
template <unsigned K, unsigned I> struct subtree_size
{ enum { value=subtree_size_fwd<K,I,(I<K/2)>::value }; };

template <unsigned K, unsigned I> struct buffer_size
{ enum { value=buffer_size<K,I-1>::value }; };
template <unsigned K> struct buffer_size<K,0> { enum { value=0 }; };

/* DFS layout for buffers.
 *
 * First places the buffer for the left subnode, then recursively for the left
 * subtree, then the buffer for the right subnode, and finally recursively for
 * the right subtree.
 *
 * Example with K=8.
 *
 *             (output)
 *
 *                (o)
 *                / \
 *               /   \
 *            1 /     \ 4
 *             /       \
 *            /         \
 *          (o)         (o)
 *          / \         / \
 *       2 /   \ 3   5 /   \ 6
 *        /     \     /     \
 *      (o)     (o) (o)     (o)
 *      ^ ^     ^ ^ ^ ^     ^ ^
 *      | |     | | | |     | |
 *
 *          (input streams)
 */
template <unsigned K, unsigned I> struct buffer_layout_dfs
{ enum { lindex=(I%2==0?buffer_layout_dfs<K,I/2>::lindex
                       :buffer_layout_dfs<K,I/2>::rindex)
                + buffer_size<K,I/2>::value,
         rindex=lindex+buffer_size<K,I>::value+subtree_size<K,2*I>::value }; };
template <unsigned K> struct buffer_layout_dfs<K,0>
{ enum { lindex=0, rindex=0 }; };

/* BFS layout for buffers.
 *
 * Example with K=8.
 *
 *             (output)
 *
 *                (o)
 *                / \
 *               /   \
 *            1 /     \ 2
 *             /       \
 *            /         \
 *          (o)         (o)
 *          / \         / \
 *       3 /   \ 4   5 /   \ 6
 *        /     \     /     \
 *      (o)     (o) (o)     (o)
 *      ^ ^     ^ ^ ^ ^     ^ ^
 *      | |     | | | |     | |
 *
 *          (input streams)
 */
template <unsigned K, unsigned I> struct buffer_layout_bfs
{ enum { lindex=buffer_layout_bfs<K,I-1>::rindex+buffer_size<K,I-1>::value,
         rindex=lindex+buffer_size<K,I>::value }; };
template <unsigned K> struct buffer_layout_bfs<K,0>
{ enum { lindex=0, rindex=0 }; };

#define MAKE_BUFFER_SIZE(K, I, SIZE) \
template <> struct buffer_size<K,I> { enum { value=SIZE }; };

/* The buffer sizes have been manually calculated for each K based on the
 * description of the algorithm.
 */

MAKE_BUFFER_SIZE(8, 1, 8)
MAKE_BUFFER_SIZE(8, 2, 23)

MAKE_BUFFER_SIZE(16, 1, 8)
MAKE_BUFFER_SIZE(16, 2, 64)
MAKE_BUFFER_SIZE(16, 4, 8)

MAKE_BUFFER_SIZE(32, 1, 8)
MAKE_BUFFER_SIZE(32, 2, 23)
MAKE_BUFFER_SIZE(32, 4, 182)
MAKE_BUFFER_SIZE(32, 8, 8)

MAKE_BUFFER_SIZE(64, 1, 8)
MAKE_BUFFER_SIZE(64, 2, 23)
MAKE_BUFFER_SIZE(64, 4, 512)
MAKE_BUFFER_SIZE(64, 8, 8)
MAKE_BUFFER_SIZE(64, 16, 23)

MAKE_BUFFER_SIZE(128, 1, 8)
MAKE_BUFFER_SIZE(128, 2, 64)
//MAKE_BUFFER_SIZE(128, 4, 8)
MAKE_BUFFER_SIZE(128, 4, 32)
MAKE_BUFFER_SIZE(128, 8, 1449)
MAKE_BUFFER_SIZE(128, 16, 8)
MAKE_BUFFER_SIZE(128, 32, 23)

// Calculates the total number of elements the buffer requires.
template <unsigned K, unsigned I=K/2-1>
struct buffer_total_size {
	enum { value=buffer_total_size<K,I-1>::value+2*buffer_size<K,I>::value };
};
template <unsigned K> struct buffer_total_size<K,0> { enum { value = 0 }; };

// Sanity check K=8.
static_assert(buffer_total_size<8>::value==108, "sanity check with K=8");
static_assert(buffer_layout_dfs<8,1>::lindex==0, "sanity check with K=8");
static_assert(buffer_layout_dfs<8,2>::lindex==8, "sanity check with K=8");
static_assert(buffer_layout_dfs<8,2>::rindex==8+23, "sanity check with K=8");
static_assert(buffer_layout_dfs<8,1>::rindex==8+23+23, "sanity check with K=8");
static_assert(buffer_layout_dfs<8,3>::lindex==8+23+23+8, "sanity check with K=8");
static_assert(buffer_layout_dfs<8,3>::rindex==8+23+23+8+23, "sanity check with K=8");
static_assert(subtree_size<8,1>::value==108, "sanity check with K=8");
static_assert(subtree_size<8,2>::value==23+23, "sanity check with K=8");
static_assert(subtree_size<8,3>::value==23+23, "sanity check with K=8");
static_assert(subtree_size<8,4>::value==0, "sanity check with K=8");
static_assert(subtree_size<8,5>::value==0, "sanity check with K=8");
static_assert(subtree_size<8,6>::value==0, "sanity check with K=8");
static_assert(subtree_size<8,7>::value==0, "sanity check with K=8");

template <unsigned K, unsigned I,
          template <unsigned, unsigned> class BufferLayout> struct fill;

/*
 * Handles the leaves of the merge tree.
 */

// Leaf to left/right stream
#define L2Ln streams[2*I-K  ].n
#define L2Rn streams[2*I-K+1].n
#define L2Ls streams[2*I-K  ].stream
#define L2Rs streams[2*I-K+1].stream

// Leaf to buffer
#define L2B (buffer+((I%2==0)?BufferLayout<K,I/2>::lindex:BufferLayout<K,I/2>::rindex))
#define L2Bsize (buffer_size<K,I/2>::value)

template <unsigned K,unsigned I,template <unsigned,unsigned> class BufferLayout>
static inline __attribute__((always_inline))
void fill_leaf(std::array<Stream,K>&, unsigned char**,
               std::array<size_t,K>&, std::false_type) {}

template <unsigned K,unsigned I,template <unsigned,unsigned> class BufferLayout>
static inline __attribute__((always_inline))
void fill_leaf(std::array<Stream,K>& restrict streams,
               unsigned char** restrict buffer,
               std::array<size_t,K>& restrict buffer_count,
               std::true_type)
{
	debug() << __func__ << ", leaf,  I="<<I<<"\n"; debug_indent;
	assert(buffer_count[I] == 0);
	if ((L2Ln + L2Rn) < L2Bsize) {
		const size_t empty = (L2Bsize-L2Ln)-L2Rn;
		size_t n = L2Bsize;
		debug()<<"need="<<n<<", Ln="<<L2Ln<<", Rn="<<L2Rn<<"\n";
		if (L2Ln==0) goto finish1_left;
		if (L2Rn==0) goto finish1_right;
		while (true) {
			if (cmp(*(L2Ls), *(L2Rs)) <= 0) {
				*(L2B+empty+(L2Bsize-n)) = *L2Ls;
				--n;
				++(L2Ls);
				--(L2Ln);
				if (L2Ln == 0) goto finish1_left;
			} else {
				*(L2B+empty+(L2Bsize-n)) = *L2Rs;
				--n;
				++(L2Rs);
				--(L2Rn);
				if (L2Rn == 0) goto finish1_right;
			}
		}
finish1_left:
		debug()<<"left drained, Ln="<<L2Ln<<", Rn="<<L2Rn<<"\n";
		std::copy(L2Rs, L2Rs+L2Rn, L2B+empty+L2Bsize-n);
		L2Rn = 0;
		buffer_count[I] = L2Bsize - empty;
		return;
finish1_right:
		debug()<<"right drained, Ln="<<L2Ln<<", Rn="<<L2Rn<<"\n";
		std::copy(L2Ls, L2Ls+L2Ln, L2B+empty+L2Bsize-n);
		L2Ln = 0;
		buffer_count[I] = L2Bsize - empty;
		check_input(L2B+empty, L2Bsize - empty);
		return;
	} else {
		size_t n = L2Bsize;
		debug()<<"need="<<n<<", Ln="<<L2Ln<<", Rn="<<L2Rn<<"\n";
		if (L2Ln==0) goto finish2_left;
		if (L2Rn==0) goto finish2_right;
		while (n) {
			if (cmp(*(L2Ls), *(L2Rs)) <= 0) {
				*(L2B+(L2Bsize-n)) = *L2Ls;
				--n;
				++(L2Ls);
				--(L2Ln);
				if (L2Ln == 0) goto finish2_left;
			} else {
				*(L2B+(L2Bsize-n)) = *L2Rs;
				--n;
				++(L2Rs);
				--(L2Rn);
				if (L2Rn == 0) goto finish2_right;
			}
		}
		buffer_count[I] = L2Bsize;
		check_input(L2B, L2Bsize);
		return;
finish2_left:
		debug()<<"left drained, Ln="<<L2Ln<<", Rn="<<L2Rn<<"\n";
		std::copy(L2Rs, L2Rs+n, L2B+L2Bsize-n);
		L2Rn -= n;
		L2Rs += n;
		buffer_count[I] = L2Bsize;
		check_input(L2B, L2Bsize);
		return;
finish2_right:
		debug()<<"right drained, Ln="<<L2Ln<<", Rn="<<L2Rn<<"\n";
		std::copy(L2Ls, L2Ls+n, L2B+L2Bsize-n);
		L2Ln -= n;
		L2Ls += n;
		buffer_count[I] = L2Bsize;
		check_input(L2B, L2Bsize);
		return;
	}
}


/*
 * Handles inner nodes in the merge tree.
 */

template <unsigned K,unsigned I,template <unsigned,unsigned> class BufferLayout>
static inline __attribute__((always_inline))
void fill_inner(std::array<Stream,K>&, unsigned char**,
                std::array<size_t,K>&, std::false_type) {}

template <unsigned K,unsigned I,template <unsigned,unsigned> class BufferLayout>
static inline __attribute__((always_inline))
void fill_inner(std::array<Stream,K>& restrict streams,
                unsigned char** restrict buffer,
                std::array<size_t,K>& restrict buffer_count,
                std::true_type)
{
	debug() << __func__ << ", inner, I="<<I<<"\n"; debug_indent;
	size_t need = buffer_size<K,I/2>::value;
	while (true) {
		if (buffer_count[2*I]==0) {
			fill<K,2*I,BufferLayout>()(streams,buffer,buffer_count);
			if (buffer_count[2*I]==0) goto finish_left;
		}
		if (buffer_count[2*I+1]==0) {
			fill<K,2*I+1,BufferLayout>()(streams, buffer,
			                             buffer_count);
			if (buffer_count[2*I+1]==0) goto finish_right;
		}
		const size_t lcount = buffer_count[2*I];
		const size_t rcount = buffer_count[2*I+1];
		unsigned char** l = buffer + BufferLayout<K,I>::lindex +
			(buffer_size<K,I>::value - lcount);
		unsigned char** r = buffer + BufferLayout<K,I>::rindex +
			(buffer_size<K,I>::value - rcount);
		unsigned char** resultbuf = buffer +
			(I%2==0?BufferLayout<K,I/2>::lindex :
			        BufferLayout<K,I/2>::rindex) +
			(buffer_size<K,I/2>::value - need);
		debug()<<"L:'"<<*l<<"', R:'"<<*r<<"'\n";
		if (cmp(*l, *r) <= 0) {
			*resultbuf = *l;
			--buffer_count[2*I];
		} else {
			*resultbuf = *r;
			--buffer_count[2*I+1];
		}
		check_input(buffer+(I%2==0?BufferLayout<K,I/2>::lindex:
		                           BufferLayout<K,I/2>::rindex),
		            buffer_size<K,I/2>::value-need);
		if (--need==0) {
			debug()<<"Done, buffer filled\n";
			buffer_count[I] = buffer_size<K,I/2>::value;
			return;
		}
	}
finish_left:
	debug()<<"left stream drained\n";
	while (true) {
		if (buffer_count[2*I+1]==0) {
			fill<K,2*I+1,BufferLayout>()(streams, buffer,
			                             buffer_count);
			if (buffer_count[2*I+1]==0) {
				debug()<<"both streams prematurely drained\n";
				unsigned char** begin = buffer +
					(I%2==0?BufferLayout<K,I/2>::lindex :
						BufferLayout<K,I/2>::rindex);
				unsigned char** newbegin = buffer +
					(I%2==0?BufferLayout<K,I/2>::lindex :
						BufferLayout<K,I/2>::rindex) +
					need;
				memmove(newbegin, begin,
					(buffer_size<K,I/2>::value - need)
					* sizeof(unsigned char*));
				buffer_count[I]=buffer_size<K,I/2>::value-need;
				check_input(newbegin, buffer_count[I]);
				return;
			}
		}
		const size_t rcount = buffer_count[2*I+1];
		unsigned char** r = buffer + BufferLayout<K,I>::rindex +
			(buffer_size<K,I>::value - rcount);
		unsigned char** resultbuf = buffer +
			(I%2==0?BufferLayout<K,I/2>::lindex :
			        BufferLayout<K,I/2>::rindex) +
			(buffer_size<K,I/2>::value - need);
		*resultbuf = *r;
		--buffer_count[2*I+1];
		if (--need==0) {
			debug()<<"\tbuffer filled\n";
			buffer_count[I] = buffer_size<K,I/2>::value;
			check_input(buffer+(I%2==0?BufferLayout<K,I/2>::lindex :
			        BufferLayout<K,I/2>::rindex), buffer_count[I]);
			return;
		}
	}
finish_right:
	debug()<<"right stream drained\n";
	while (true) {
		if (buffer_count[2*I]==0) {
			fill<K,2*I,BufferLayout>()(streams,buffer,buffer_count);
			if (buffer_count[2*I]==0) {
				debug()<<"both streams prematurely drained\n";
				unsigned char** begin = buffer +
					(I%2==0?BufferLayout<K,I/2>::lindex :
						BufferLayout<K,I/2>::rindex);
				unsigned char** newbegin = buffer +
					(I%2==0?BufferLayout<K,I/2>::lindex :
						BufferLayout<K,I/2>::rindex) +
					need;
				memmove(newbegin, begin,
					(buffer_size<K,I/2>::value - need)
					* sizeof(unsigned char*));
				buffer_count[I]=buffer_size<K,I/2>::value-need;
				check_input(newbegin, buffer_count[I]);
				return;
			}
		}
		const size_t lcount = buffer_count[2*I];
		unsigned char** l = buffer + BufferLayout<K,I>::lindex +
			(buffer_size<K,I>::value - lcount);
		unsigned char** resultbuf = buffer +
			(I%2==0?BufferLayout<K,I/2>::lindex :
			        BufferLayout<K,I/2>::rindex) +
			(buffer_size<K,I/2>::value - need);
		*resultbuf = *l;
		--buffer_count[2*I];
		if (--need==0) {
			debug()<<"buffer filled\n";
			buffer_count[I] = buffer_size<K,I/2>::value;
			check_input(buffer+(I%2==0?BufferLayout<K,I/2>::lindex :
			        BufferLayout<K,I/2>::rindex), buffer_count[I]);
			return;
		}
	}
}


/*
 * Handles the root of the merge tree.
 */

template <unsigned K, template <unsigned, unsigned> class BufferLayout>
static inline __attribute__((always_inline))
void fill_root(std::array<Stream,K>& restrict streams,
               unsigned char** restrict result,
               unsigned char** restrict buffer,
               std::array<size_t,K>& restrict buffer_count)
{
	debug() << __func__ << ", root\n"; debug_indent;
	while (true) {
		if (buffer_count[2]==0) {
			fill<K,2,BufferLayout>()(streams, buffer, buffer_count);
			if (buffer_count[2]==0) { goto finish_left; }
		}
		if (buffer_count[3]==0) {
			fill<K,3,BufferLayout>()(streams, buffer, buffer_count);
			if (buffer_count[3]==0) { goto finish_right; }
		}
		const size_t lcount = buffer_count[2];
		const size_t rcount = buffer_count[3];
		unsigned char** l = buffer + BufferLayout<K,1>::lindex +
			(buffer_size<K,1>::value - lcount);
		unsigned char** r = buffer + BufferLayout<K,1>::rindex +
			(buffer_size<K,1>::value - rcount);
		debug()<<"L:'"<<*l<<"', R:'"<<*r<<"'\n";
		if (cmp(*l, *r) <= 0) {
			*(result++) = *l;
			--buffer_count[2];
		} else {
			*(result++) = *r;
			--buffer_count[3];
		}
	}
finish_left:
	debug()<<"left stream drained\n";
	assert(buffer_count[2] == 0);
	assert(buffer_count[3] != 0);
	while (true) {
		const size_t num = buffer_count[3];
		if (num==0) { return; }
		unsigned char** begin = buffer + BufferLayout<K,1>::rindex +
			(buffer_size<K,1>::value - num);
		std::copy(begin, begin+num, result);
		result += num;
		buffer_count[3] = 0;
		fill<K,3,BufferLayout>()(streams, buffer, buffer_count);
	}
	assert(0);
finish_right:
	debug()<<"right stream drained\n";
	assert(buffer_count[2] != 0);
	assert(buffer_count[3] == 0);
	while (true) {
		const size_t num = buffer_count[2];
		if (num==0) { return; }
		unsigned char** begin = buffer + buffer_size<K,1>::value - num;
		std::copy(begin, begin+num, result);
		result += num;
		buffer_count[2] = 0;
		fill<K,2,BufferLayout>()(streams, buffer, buffer_count);
	}
	assert(0);
}

// Choose the correct algorithm based on our location in the merge tree.
// We need to use integral_constant<> in order to avoid choking the compiler.
template <unsigned K,unsigned I,template <unsigned,unsigned> class BufferLayout>
struct fill
{
	void operator()(std::array<Stream,K>& restrict streams,
	                unsigned char** restrict buffer,
	                std::array<size_t,K>& restrict buffer_count) const
	{
		fill_inner<K,I,BufferLayout>(streams, buffer, buffer_count,
		    typename std::integral_constant<bool, (I>1 && I<K/2)>());
		fill_leaf<K,I,BufferLayout>(streams, buffer, buffer_count,
		    typename std::integral_constant<bool, (I>=K/2 && I<K)>());
	}
};

#ifdef FUNNELSORT_EXTRA_INLINING
// Fully inline K=8 and K=16 with ''__attribute__((always_inline))''. Increases
// compilation times by 5-10 minutes even with a fast computer. Adding the
// attribute to the generic 'fill' struct above effectively kills the compiler.
template <unsigned I,template <unsigned,unsigned> class BufferLayout>
struct fill<8,I,BufferLayout>
{
	enum { K=8 };
	void operator()(std::array<Stream,K>& restrict streams,
	                unsigned char** restrict buffer,
	                std::array<size_t,K>& restrict buffer_count) const
	                __attribute__((always_inline))
	{
		fill_inner<K,I,BufferLayout>(streams, buffer, buffer_count,
		    typename std::integral_constant<bool, (I>1 && I<K/2)>());
		fill_leaf<K,I,BufferLayout>(streams, buffer, buffer_count,
		    typename std::integral_constant<bool, (I>=K/2 && I<K)>());
	}
};
template <unsigned I,template <unsigned,unsigned> class BufferLayout>
struct fill<16,I,BufferLayout>
{
	enum { K=16 };
	void operator()(std::array<Stream,K>& restrict streams,
	                unsigned char** restrict buffer,
	                std::array<size_t,K>& restrict buffer_count) const
	                __attribute__((always_inline))
	{
		fill_inner<K,I,BufferLayout>(streams, buffer, buffer_count,
		    typename std::integral_constant<bool, (I>1 && I<K/2)>());
		fill_leaf<K,I,BufferLayout>(streams, buffer, buffer_count,
		    typename std::integral_constant<bool, (I>=K/2 && I<K)>());
	}
};
#endif

// Funnelsort recursion. Approximate the theoretical funnel size scheme by
// splitting the input into K streams, and using a fixed size K-merger.
// Then use K/4 or K/2 on the next level of recursion.
template <unsigned K, template <unsigned, unsigned> class BufferLayout>
static void
funnelsort(unsigned char** strings, size_t n, unsigned char** restrict tmp)
{
	debug() << __func__ << "(), n=" << n << "\n";
	if (n < 32) {
		insertion_sort(strings, n, 0);
		return;
	}
	size_t splitter = n/K;
	std::array<Stream, K> streams;
	streams[0].stream = strings;
	streams[0].n      = splitter;
	for (unsigned i=1; i < K-1; ++i) {
		streams[i].stream = streams[i-1].stream + splitter;
		streams[i].n = splitter;
	}
	streams[K-1].stream = streams[K-2].stream + splitter;
	streams[K-1].n      = n - (K-1)*splitter;
	for (unsigned i=0; i < K; ++i) {
		funnelsort<(K>16?K/4:K/2),BufferLayout>(streams[i].stream,
		                                        streams[i].n, tmp);
		check_input(streams[i].stream, streams[i].n);
	}
	std::array<unsigned char*, buffer_total_size<K>::value> buffers;
	std::array<size_t, K> buffer_count;
	buffer_count.fill(0);
	fill_root<K,BufferLayout>(streams,tmp,buffers.data(),buffer_count);
	(void) memcpy(strings, tmp, n*sizeof(unsigned char*));
	check_input(strings, n);
}

// Switch to 4-way mergesort on small inputs/lower levels of recursion.
void mergesort_4way(unsigned char**, size_t, unsigned char**);
//template <> void funnelsort<2,buffer_layout_bfs>(unsigned char** strings,
//	size_t n, unsigned char** tmp) { mergesort_4way(strings, n, tmp); }
//template <> void funnelsort<2,buffer_layout_dfs>(unsigned char** strings,
//	size_t n, unsigned char** tmp) { mergesort_4way(strings, n, tmp); }
template <> void funnelsort<4,buffer_layout_bfs>(unsigned char** strings,
	size_t n, unsigned char** tmp) { mergesort_4way(strings, n, tmp); }
template <> void funnelsort<4,buffer_layout_dfs>(unsigned char** strings,
	size_t n, unsigned char** tmp) { mergesort_4way(strings, n, tmp); }

template <unsigned K, template <unsigned, unsigned> class BufferLayout>
void funnelsort_Kway(unsigned char** strings, size_t n)
{
	unsigned char** tmp = static_cast<unsigned char**>(
			malloc(n*sizeof(unsigned char*)));
	funnelsort<K, BufferLayout>(strings, n, tmp);
	free(tmp);
}

void funnelsort_8way_bfs(unsigned char** strings, size_t n)
{ funnelsort_Kway<8, buffer_layout_bfs>(strings, n); }
void funnelsort_16way_bfs(unsigned char** strings, size_t n)
{ funnelsort_Kway<16, buffer_layout_bfs>(strings, n); }
void funnelsort_32way_bfs(unsigned char** strings, size_t n)
{ funnelsort_Kway<32, buffer_layout_bfs>(strings, n); }
void funnelsort_64way_bfs(unsigned char** strings, size_t n)
{ funnelsort_Kway<64, buffer_layout_bfs>(strings, n); }
void funnelsort_128way_bfs(unsigned char** strings, size_t n)
{ funnelsort_Kway<128, buffer_layout_bfs>(strings, n); }

void funnelsort_8way_dfs(unsigned char** strings, size_t n)
{ funnelsort_Kway<8, buffer_layout_dfs>(strings, n); }
void funnelsort_16way_dfs(unsigned char** strings, size_t n)
{ funnelsort_Kway<16, buffer_layout_dfs>(strings, n); }
void funnelsort_32way_dfs(unsigned char** strings, size_t n)
{ funnelsort_Kway<32, buffer_layout_dfs>(strings, n); }
void funnelsort_64way_dfs(unsigned char** strings, size_t n)
{ funnelsort_Kway<64, buffer_layout_dfs>(strings, n); }
void funnelsort_128way_dfs(unsigned char** strings, size_t n)
{ funnelsort_Kway<128, buffer_layout_dfs>(strings, n); }

ROUTINE_REGISTER_SINGLECORE(funnelsort_8way_bfs,
		"funnelsort_8way_bfs")
ROUTINE_REGISTER_SINGLECORE(funnelsort_16way_bfs,
		"funnelsort_16way_bfs")
ROUTINE_REGISTER_SINGLECORE(funnelsort_32way_bfs,
		"funnelsort_32way_bfs")
ROUTINE_REGISTER_SINGLECORE(funnelsort_64way_bfs,
		"funnelsort_64way_bfs")
ROUTINE_REGISTER_SINGLECORE(funnelsort_128way_bfs,
		"funnelsort_128way_bfs")

ROUTINE_REGISTER_SINGLECORE(funnelsort_8way_dfs,
		"funnelsort_8way_dfs")
ROUTINE_REGISTER_SINGLECORE(funnelsort_16way_dfs,
		"funnelsort_16way_dfs")
ROUTINE_REGISTER_SINGLECORE(funnelsort_32way_dfs,
		"funnelsort_32way_dfs")
ROUTINE_REGISTER_SINGLECORE(funnelsort_64way_dfs,
		"funnelsort_64way_dfs")
ROUTINE_REGISTER_SINGLECORE(funnelsort_128way_dfs,
		"funnelsort_128way_dfs")
