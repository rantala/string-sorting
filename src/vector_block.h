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
 * vector_block implements the vector Abstract Data Type (ADT) using a series
 * of memory blocks of fixed size B. Only a very limited set of operations have
 * been implemented.
 *
 * push_back: O(1) time + ceiling(n/B) malloc() calls
 * indexing: O(1) time
 * size(): O(1) time
 *
 * wasted space: at most B-1 elements + O(n/B) wasted pointers in the
 * geometrically expanding index block
 */

#ifndef VECTOR_BLOCK
#define VECTOR_BLOCK

template <typename T, unsigned B=1024>
struct vector_block
{
	typedef T value_type;
	typedef std::vector<T*> index_block_type;
	void push_back(const T& t)
	{
		if (__builtin_expect(is_full(), false)) {
			_insertpos = static_cast<T*>(malloc(B*sizeof(T)));
			_index_block.push_back(_insertpos);
			_left_in_block = B;
		}
		*_insertpos++ = t;
		--_left_in_block;
	}
	bool is_full() const { return _left_in_block==0; }
	T operator[](size_t index) const
	{
		assert(index < size());
		return _index_block[index/B][index%B];
	}
	size_t size() const
	{
		return _index_block.size()*B - _left_in_block;
	}
	void clear()
	{
		for (size_t i=0; i < _index_block.size(); ++i) {
			free(_index_block[i]);
		}
		_index_block.clear();
		_insertpos=0;
		_left_in_block=0;
	}
	~vector_block()
	{
		for (size_t i=0; i < _index_block.size(); ++i) {
			free(_index_block[i]);
		}
	}
	vector_block() : _index_block(), _insertpos(0), _left_in_block(0) {}
	index_block_type _index_block;
	T* _insertpos;
	unsigned _left_in_block;
};

// Copies all elements from this container to the given output iterator.
// This is probably redundant if iterators are ever implemented.
template <typename T, unsigned B, typename OutputIterator>
static inline void
copy(const vector_block<T, B>& v, OutputIterator dst)
{
	assert(not v._index_block.empty());
	for (size_t i=1; i < v._index_block.size(); ++i) {
		std::copy(v._index_block[i-1],
		          v._index_block[i-1]+B,
		          dst);
		dst += B;
	}
	std::copy(v._index_block.back(), v._insertpos, dst);
}

#endif //VECTOR_BLOCK
