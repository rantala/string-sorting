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

#ifndef VECTOR_MALLOC
#define VECTOR_MALLOC

#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <cassert>

template <typename T, unsigned InitialSize=16>
class vector_malloc
{
public:
	vector_malloc() : _data(0), _size(0), _capacity(0) {}
	~vector_malloc() { if (_data) free(_data); }
	void push_back(const T& t)
	{
		if (_size == _capacity) { grow(); }
		_data[_size] = t;
		++_size;
	}
	void clear()
	{
		if (_data) free(_data);
		_data = 0;
		_size = 0;
		_capacity = 0;
	}
	T operator[](size_t index) const
	{
		assert(index < size());
		return _data[index];
	}
	const T* begin() const { return _data; }
	const T* end() const  { return _data+_size; }
	size_t capacity() const { return _capacity; }
	size_t size() const     { return _size;     }
private:
	void grow()
	{
		if (_capacity == 0) {
			_capacity = InitialSize;
			_data = static_cast<T*>(malloc(_capacity*sizeof(T)));
		} else {
			_capacity <<= 1;
			T* t = static_cast<T*>(malloc(_capacity*sizeof(T)));
			(void) memcpy(t, _data, _size*sizeof(T));
			free(_data);
			_data = t;
		}
	}
	T* _data;
	size_t _size;
	size_t _capacity;
};

template <typename T, unsigned InitialSize=16>
class vector_malloc_counter_clear
{
public:
	vector_malloc_counter_clear() : _data(0), _size(0), _capacity(0) {}
	~vector_malloc_counter_clear() { if (_data) free(_data); }
	void push_back(const T& t)
	{
		if (_size == _capacity) { grow(); }
		_data[_size] = t;
		++_size;
	}
	void clear() { _size=0; }
	T operator[](size_t index) const
	{
		assert(index < size());
		return _data[index];
	}
	const T* begin() const  { return _data;       }
	const T* end() const    { return _data+_size; }
	size_t capacity() const { return _capacity;   }
	size_t size() const     { return _size;       }
private:
	void grow()
	{
		if (_capacity == 0) {
			_capacity = InitialSize;
			_data = static_cast<T*>(malloc(_capacity*sizeof(T)));
		} else {
			_capacity <<= 1;
			T* t = static_cast<T*>(malloc(_capacity*sizeof(T)));
			(void) memcpy(t, _data, _size*sizeof(T));
			free(_data);
			_data = t;
		}
	}
	T* _data;
	size_t _size;
	size_t _capacity;
};

#endif //VECTOR_MALLOC
