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

/*
 * vector_bagwell implements the vector Abstract Data Type (ADT) using a series
 * of geometrically expanding memory blocks. Only a very limited set of
 * operations have been implemented.
 *
 * Might be faster than the typical geometrically expanding array in some
 * cases, because it does not need to copy elements from the smaller array to
 * the expanded larger array.
 *
 * Based on: Phil Bagwell: "Fast Functional Lists, Hash-Lists, Deques and
 * Variable Length Arrays"
 *
 * push_back: O(1) amortized time
 * indexing: O(1) time
 * size(): O(1) time
 *
 * wasted space: O(n)
 */

#ifndef VLIST_VECTOR_HPP
#define VLIST_VECTOR_HPP

#include <cstdlib>
#include <cstddef>
#include <vector>
#include <cassert>
#include <boost/static_assert.hpp>

// Initial: Size of the initial memory allocation. Has to be a power of two.
template <typename T, unsigned Initial = 16>
struct vector_bagwell
{
	typedef T value_type;
	typedef std::vector<T*> index_block_type;
	void push_back(const T& t)
	{
		if (__builtin_expect(current_block_full(), false)) {
			_left_in_block = Initial << _index_block.size();
			_insertpos = static_cast<T*>(
					malloc(_left_in_block*sizeof(T)));
			_index_block.push_back(_insertpos);
		}
		*_insertpos++ = t;
		--_left_in_block;
	}
	bool current_block_full() const { return _left_in_block == 0; }
	T operator[](size_t index) const
	{
		assert(index < size());
		BOOST_STATIC_ASSERT(Initial==16);
		BOOST_STATIC_ASSERT(sizeof(size_t)==sizeof(unsigned));
		const size_t fixed = index+16;
		const unsigned msb_diff = 27 - __builtin_clz(fixed);
		const unsigned msbit = 1 << (31 - __builtin_clz(fixed));
		const size_t fixed2 = fixed & ~msbit;
		return _index_block[msb_diff][fixed2];
	}
	void clear()
	{
		for (unsigned i=0; i < _index_block.size(); ++i) {
			free(_index_block[i]);
		}
		_index_block.clear();
		_insertpos = 0;
		_left_in_block = 0;
	}
	size_t size() const
	{
		BOOST_STATIC_ASSERT(Initial==16);
		if (empty()) return 0;
		return (1<<(3+_index_block.size()))-1-15
			+ _insertpos-_index_block.back();
	}
	bool empty() const { return _insertpos==0; }
	~vector_bagwell()
	{
		for (unsigned i=0; i < _index_block.size(); ++i)
			free(_index_block[i]);
	}
	vector_bagwell() : _index_block(), _insertpos(0), _left_in_block(0) {}
	index_block_type _index_block;
	T* _insertpos;
	size_t _left_in_block;
};

template <typename T, unsigned InitialSize, typename OutputIterator>
static inline void
copy(const vector_bagwell<T, InitialSize>& v, OutputIterator dst)
{
	size_t bufsize = InitialSize;
	for (size_t i=1; i < v._index_block.size(); ++i) {
		std::copy(v._index_block[i-1],
		          v._index_block[i-1]+bufsize,
		          dst);
		dst += bufsize;
		bufsize *= 2;
	}
	std::copy(v._index_block.back(), v._insertpos, dst);
}

#endif
