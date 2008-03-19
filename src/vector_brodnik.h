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
