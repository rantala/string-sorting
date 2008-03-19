/*
 * Copyright 2008 by Tommi Rantala <tommi.rantala@cs.helsinki.fi>
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
 * @inproceedings{673194,
 *     author = {Andrej Brodnik and Svante Carlsson and Erik D. Demaine and J.
 *               Ian Munro and Robert Sedgewick},
 *     title = {Resizable Arrays in Optimal Time and Space},
 *     booktitle = {WADS '99: Proceedings of the 6th International Workshop on
 *                  Algorithms and Data Structures},
 *     year = {1999},
 *     isbn = {3-540-66279-0},
 *     pages = {37--48},
 *     publisher = {Springer-Verlag},
 *     address = {London, UK},
 * }
 */

#ifndef VECTOR_BRODNIK
#define VECTOR_BRODNIK

template <typename T>
struct vector_brodnik
{
	typedef T value_type;
	typedef std::vector<T*> index_block_type;
	void push_back(const T& t)
	{
		if (is_full()) { grow(); }
		*_insertpos++ = t;
		--_left_in_block;
	}
	bool is_full() const { return _left_in_block == 0; }
	size_t size() const
	{
		// The sum of elements in superblocks 0,...,k-1 is 2^k-1.
		return ((1 << (_superblock+1))-1)
			- _left_in_block
			- (_block_size*_left_in_superblock);
	}
	void grow()
	{
		assert(_left_in_block == 0);
		if (_left_in_superblock == 0) {
			if (_superblock&1) { _superblock_size *= 2; }
			else               { _block_size *= 2;      }
			++_superblock;
			_left_in_superblock = _superblock_size;
		}
		_insertpos = static_cast<T*>(malloc(_block_size*sizeof(T)));
		_index_block.push_back(_insertpos);
		_left_in_block = _block_size;
		--_left_in_superblock;
	}
	T operator[](size_t index) const
	{
		// See the paper for details.
		assert(index < size());
		const size_t r = index+1;
		const unsigned k = 31 - __builtin_clz(r);
		const unsigned msbit = 1 << (31 - __builtin_clz(r));
		const size_t b = (r & ~msbit) >> (k-k/2);
		const size_t e = ~((~0 >> (k-k/2)) << (k-k/2)) & r;
		const size_t p = k&1 ? (3*(1<<(k>>1))-2) : ((1<<((k>>1)+1))-2);
		/*
		std::cerr<<__func__<<"\n"
			<<"\tindex="<<index<<"\n"
			<<"\tr    ="<<r<<"\n"
			<<"\tk    ="<<k<<"\n"
			<<"\tmsbit="<<msbit<<"\n"
			<<"\tb    ="<<b<<"\n"
			<<"\te    ="<<e<<"\n"
			<<"\tp    ="<<p<<"\n\n";
		*/
		assert(p+b < _index_block.size());
		return _index_block[p+b][e];
	}
	void clear()
	{
		for (unsigned i=0; i < _index_block.size(); ++i) {
			free(_index_block[i]);
		}
		_index_block.clear();
		_insertpos = 0;
		_left_in_block = 0;
		_left_in_superblock = 1;
		_block_size = 1;
		_superblock_size = 1;
		_superblock = 0;
	}
	~vector_brodnik()
	{
		for (unsigned i=0; i < _index_block.size(); ++i) {
			free(_index_block[i]);
		}
	}
	vector_brodnik() { clear(); }
	index_block_type _index_block;
	T* _insertpos;
	size_t _left_in_block;
	size_t _block_size;
	uint16_t _left_in_superblock;
	uint16_t _superblock_size;
	uint8_t _superblock;
};

template <typename T, typename OutputIterator>
static inline void
copy(const vector_brodnik<T>& bucket, OutputIterator dst)
{
	bool superblock_odd=false;
	size_t superblocksize=1;
	size_t blocksize=1;
	for (size_t i=0; i < bucket._index_block.size(); ) {
		for (size_t j=0; j < superblocksize; ++j) {
			if (i+j == (bucket._index_block.size()-1)) goto done;
			std::copy(bucket._index_block[i+j],
			          bucket._index_block[i+j]+blocksize,
			          dst);
			dst += blocksize;
		}
		i += superblocksize;
		superblocksize <<= superblock_odd;
		blocksize     <<= !superblock_odd;
		superblock_odd = !superblock_odd;
	}
done:
	std::copy(bucket._index_block.back(),
	          bucket._insertpos,
	          dst);
}

#endif //VECTOR_BRODNIK
