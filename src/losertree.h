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

/* The loser tree is defined in:
 *
 *     Donald Knuth: The Art of Computer Programming,
 *              Volume III: Sorting and Searching, 1973,
 *              section 5.4.1, page 253
 *
 * It is used to implement multi-way merging.
 */

/* Example with 8 streams:
 *
 * _nodes:
 *   Each node contains an index to the _streams array. The winner (smallest
 *   item) is stored in position 0. Other nodes contain the loser of each
 *   comparison.
 *
 *                   <0>
 *                    |
 *                   <1>
 *                  /   \
 *                 /     \
 *                /       \
 *              <2>       <3>
 *              / \       / \
 *             /   \     /   \
 *           <4>   <5> <6>   <7>
 *
 * _streams:
 *   0:(T*,n), 1:(T*,n), ..., 7:(T*,n)
 *
 * Both structures contain exactly 2^k items. Empty streams are inserted if
 * required.
 */

#ifndef LOSERTREE_H
#define LOSERTREE_H

#include "util/debug.h"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <algorithm>

static inline unsigned log2(unsigned n)
{ return 8*sizeof(unsigned)-1-__builtin_clz(n); }

template <typename T>
struct loser_tree
{
	typedef struct { T* stream; size_t n; } Stream;
	unsigned* restrict _nodes;
	Stream* restrict _streams;
	unsigned _nonempty_streams;
	const unsigned _stream_offset;

	template <typename Iterator>
	loser_tree(Iterator begin, Iterator end)
		: _nodes(0), _streams(0), _nonempty_streams(end-begin),
		  _stream_offset(1 << (log2(_nonempty_streams-1)+1))
	{
		assert(_nonempty_streams>1);
		void* raw = malloc(_stream_offset*sizeof(unsigned) +
		                   _stream_offset*sizeof(Stream));
		_nodes = static_cast<unsigned*>(raw);
		_streams = reinterpret_cast<Stream*>(
				static_cast<char*>(raw) +
					_stream_offset*sizeof(unsigned));
		for (unsigned i=0; i < _nonempty_streams; ++i) {
			_streams[i].stream = begin[i].first;
			_streams[i].n      = begin[i].second;
		}
		(void) memset(_streams+_nonempty_streams, 0,
			(_stream_offset-_nonempty_streams)*sizeof(Stream));
		_nodes[0] = init_min(1);
		//debug()<<*this;
	}

	~loser_tree()
	{
		assert(_nodes); assert(_streams);
		free(static_cast<void*>(_nodes));
	}

	Stream& node2stream(unsigned pos)
	{
		assert(pos < _stream_offset);
		assert(_nodes[pos] < _stream_offset);
		return _streams[_nodes[pos]];
	}

	bool stream_empty(const Stream& pos) const { return pos.n == size_t(0); }

	unsigned init_min(unsigned root)
	{
		if (root >= _stream_offset) { return root-_stream_offset; }
		//debug() << __PRETTY_FUNCTION__ << " root="<<root<<"\n";
		const unsigned l = init_min(root << 1);
		const unsigned r = init_min((root << 1) + 1);
		if (stream_empty(_streams[r])) {
			_nodes[root] = r;
			return l;
		}
		if (stream_empty(_streams[l])) {
			_nodes[root] = l;
			return r;
		}
		if (cmp(*(_streams[l].stream), *(_streams[r].stream)) <= 0) {
			_nodes[root] = r;
			return l;
		}
		_nodes[root] = l;
		return r;
	}

	bool empty() const { return _nonempty_streams == 0; }

	void update()
	{
		//debug() << __PRETTY_FUNCTION__ << std::endl;
		unsigned new_min = _nodes[0];
		for (unsigned i=(_stream_offset+new_min) >> 1; i!=0; i >>= 1) {
			if (stream_empty(_streams[new_min]) or
			    (not stream_empty(node2stream(i)) and
			     cmp(*node2stream(i).stream,
			         *_streams[new_min].stream) < 0)) {
				std::swap(new_min, _nodes[i]);
			}
		}
		_nodes[0] = new_min;
	}

	T min()
	{
		//debug() << __PRETTY_FUNCTION__ << std::endl;
		assert(_nonempty_streams);
		assert(not stream_empty(node2stream(0)));
		T ret = *(node2stream(0).stream++);
		if (--node2stream(0).n == size_t(0)) { --_nonempty_streams; }
		update();
		//debug() << "\t -> " << ret << std::endl;
		return ret;
	}
};

#ifndef NDEBUG
#include <ostream>
template <typename T>
std::ostream& operator<<(std::ostream& strm, const loser_tree<T>& tree)
{
	strm<<"/-------------------\n";
	for(unsigned i=0;i<tree._stream_offset;++i){
		if(i==1)strm<<"--------------------\n";
		strm<<i<<": "<<tree._nodes[i]<<"\n";
	}
	strm<<"--------------------\n";
	for(unsigned i=0;i<tree._stream_offset;++i)
		strm<<i<<": "<<tree._streams[i].stream
		    <<", n="<<tree._streams[i].n<<"\n";
	strm<<"-------------------/\n";
	return strm;
}
#endif //NDEBUG
#endif //LOSERTREE_H
