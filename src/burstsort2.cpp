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
 * Burstsort2 is otherwise identical to the regular burstsort, but the pointer
 * array in each trie node is grown dynamically based on input, instead of
 * having a fixed size. The arrays are simply expanded to fit the largest
 * character from the alphabet seen so far. This can lead to some space savings.
 */

#include "routine.h"
#include "util/get_char.h"
#include "util/debug.h"
#include <vector>
#include <iostream>
#include <bitset>
#include "vector_bagwell.h"
#include "vector_brodnik.h"
#include "vector_block.h"
#include <array>

template <typename CharT, typename BucketT>
struct TrieNode
{
	// One subtree per character in alphabet. Points to either a 'TrieNode'
	// or a 'Bucket' node.
	//
	// We use the least significant bit from the pointer to know which
	// one. malloc() must give pointers that are aligned to the size of the
	// largest primitive, ie. something like 8 bytes, meaning that the
	// least significant bit is always 0.
	//
	// We set the bit to 1 when pointer points to a 'TrieNode', otherwise
	// it's either a null pointer or points to a 'Bucket'.
	std::vector<void*> _buckets;
	TrieNode() {}
	TrieNode* get_node(unsigned index) const
	{
		assert(is_trie(index));
		return reinterpret_cast<TrieNode*>(
		       static_cast<char*>(_buckets[index])-1);
	}
	BucketT* get_bucket(unsigned index)
	{
		if (index < _buckets.size()) {
			BucketT* b = static_cast<BucketT*>(_buckets[index]);
			if (not b) { _buckets[index] = b = new BucketT; }
			return b;
		} else {
			_buckets.resize(index+1);
			BucketT* b = new BucketT;
			_buckets[index] = b;
			return b;
		}
	}
	bool is_trie(unsigned index) const
	{
		static_assert(sizeof(size_t)==sizeof(void*),
			"size_t and void* size must match");
		if (index < _buckets.size()) {return size_t(_buckets[index])&1;}
		return false;
	}
	void extend(unsigned size)
	{
		if (size > _buckets.size()) { _buckets.resize(size); }
	}
};

static inline void make_trie(void*& ptr)
{
	static_assert(sizeof(size_t)==sizeof(void*),
		"size_t and void* size must match");
	assert(ptr);
	ptr = (void*)(size_t(ptr) | 1);
}

// The burst algorithm as described by Sinha, Zobel et al.
template <typename CharT>
struct BurstSimple
{
	template <typename BucketT>
	TrieNode<CharT, BucketT>*
	operator()(const BucketT& bucket, size_t depth) const
	{
		TrieNode<CharT, BucketT>* new_node = new TrieNode<CharT, BucketT>;
		const unsigned bucket_size = bucket.size();
		// Use a small cache to reduce memory stalls. Also cache the
		// string pointers, in case the indexing operation of the
		// container is expensive.
		unsigned i=0;
		for (; i < bucket_size-bucket_size%64; i+=64) {
			std::array<CharT, 64> cache;
			std::array<unsigned char*, 64> strings;
			for (unsigned j=0; j < 64; ++j) {
				strings[j] = bucket[i+j];
				cache[j] = get_char<CharT>(strings[j], depth);
			}
			for (unsigned j=0; j < 64; ++j) {
				const CharT ch = cache[j];
				BucketT* sub_bucket = new_node->get_bucket(ch);
				assert(sub_bucket);
				sub_bucket->push_back(strings[j]);
			}
		}
		for (; i < bucket_size; ++i) {
			unsigned char* ptr = bucket[i];
			const CharT ch = get_char<CharT>(ptr, depth);
			BucketT* sub_bucket = new_node->get_bucket(ch);
			assert(sub_bucket);
			sub_bucket->push_back(ptr);
		}
		return new_node;
	}
};

// Another burst variant: After bursting the bucket, immediately burst large
// sub buckets in a recursive fashion.
template <typename CharT>
struct BurstRecursive
{
	template <typename BucketT>
	TrieNode<CharT, BucketT>*
	operator()(const BucketT& bucket, size_t depth) const
	{
		TrieNode<CharT, BucketT>* new_node
			= BurstSimple<CharT>()(bucket, depth);
		const size_t threshold = std::max(size_t(100), bucket.size()/2);
		for (unsigned i=0; i < new_node->_buckets.size(); ++i) {
			BucketT* sub_bucket = static_cast<BucketT*>(
					new_node->_buckets[i]);
			if (not sub_bucket) continue;
			if (not is_end(i) and sub_bucket->size() > threshold) {
				new_node->_buckets[i] =
					BurstRecursive<CharT>()(*sub_bucket,
							depth+sizeof(CharT));
				delete sub_bucket;
				make_trie(new_node->_buckets[i]);
			}
		}
		return new_node;
	}
};

// Uses a random sample to create an initial tree.
template <typename CharT, typename BucketT>
static TrieNode<CharT, BucketT>*
random_sample(unsigned char** strings, size_t n)
{
	const size_t sample_size = n/8192;
	debug()<<__PRETTY_FUNCTION__<<" sampling "<<sample_size<<" strings\n";
	size_t max_nodes = (sizeof(CharT) == 1) ? 5000 : 2000;
	TrieNode<CharT, BucketT>* root = new TrieNode<CharT, BucketT>;
	for (size_t i=0; i < sample_size; ++i) {
		unsigned char* str = strings[size_t(drand48()*n)];
		size_t depth = 0;
		TrieNode<CharT, BucketT>* node = root;
		while (true) {
			CharT c = get_char<CharT>(str, depth);
			if (is_end(c)) break;
			depth += sizeof(CharT);
			node->extend(c+1);
			if (not node->is_trie(c)) {
				node->_buckets[c] = new TrieNode<CharT, BucketT>;
				make_trie(node->_buckets[c]);
				if (--max_nodes==0) goto finish;
			}
			node = node->get_node(c);
			assert(node);
		}
	}
finish:
	return root;
}

// Uses a pseudo random sample to create an initial tree.
template <typename CharT, typename BucketT>
static TrieNode<CharT, BucketT>*
pseudo_sample(unsigned char** strings, size_t n)
{
	debug()<<__func__<<"(): sampling "<<n/8192<<" strings ...\n";
	size_t max_nodes = (sizeof(CharT) == 1) ? 5000 : 2000;
	TrieNode<CharT, BucketT>* root = new TrieNode<CharT, BucketT>;
	for (size_t i=0; i < n; i += 8192) {
		unsigned char* str = strings[i];
		size_t depth = 0;
		TrieNode<CharT, BucketT>* node = root;
		while (true) {
			CharT c = get_char<CharT>(str, depth);
			if (is_end(c)) break;
			depth += sizeof(CharT);
			node->extend(c+1);
			if (not node->is_trie(c)) {
				node->_buckets[c] = new TrieNode<CharT, BucketT>;
				make_trie(node->_buckets[c]);
				if (--max_nodes==0) goto finish;
			}
			node = node->get_node(c);
			assert(node);
		}
	}
finish:
	return root;
}

template <unsigned Threshold, typename BucketT,
          typename BurstImpl, typename CharT>
static inline void
insert(TrieNode<CharT, BucketT>* root, unsigned char** strings, size_t n)
{
	for (size_t i=0; i < n; ++i) {
		unsigned char* str = strings[i];
		size_t depth = 0;
		CharT c = get_char<CharT>(str, 0);
		TrieNode<CharT, BucketT>* node = root;
		while (node->is_trie(c)) {
			assert(not is_end(c));
			node = node->get_node(c);
			depth += sizeof(CharT);
			c = get_char<CharT>(str, depth);
		}
		BucketT* bucket = node->get_bucket(c);
		assert(bucket);
		bucket->push_back(str);
		if (is_end(c)) continue;
		if (bucket->size() > Threshold) {
			node->_buckets[c] = BurstImpl()(*bucket,
					depth+sizeof(CharT));
			make_trie(node->_buckets[c]);
			delete bucket;
		}
	}
}

// Use a wrapper to std::copy(). I haven't implemented iterators for some of my
// containers, instead they have optimized copy(bucket, dst).
static inline void
copy(const std::vector<unsigned char*>& bucket, unsigned char** dst)
{
	std::copy(bucket.begin(), bucket.end(), dst);
}

// Traverses the trie and copies the strings back to the original string array.
// Nodes and buckets are deleted from memory during the traversal. The root
// node given to this function will also be deleted.
template <typename SmallSort, typename BucketT, typename CharT>
static unsigned char**
traverse(TrieNode<CharT, BucketT>* node,
         unsigned char** dst,
         size_t depth,
         SmallSort small_sort)
{
	for (unsigned i=0; i < node->_buckets.size(); ++i) {
		if (node->is_trie(i)) {
			dst = traverse(node->get_node(i),
				dst, depth+sizeof(CharT), small_sort);
		} else {
			BucketT* bucket =
				static_cast<BucketT*>(node->_buckets[i]);
			if (not bucket) continue;
			size_t bsize = bucket->size();
			copy(*bucket, dst);
			if (not is_end(i)) small_sort(dst, bsize, depth);
			dst += bsize;
			delete bucket;
		}
	}
	delete node;
	return dst;
}

#define SmallSort mkqsort
extern "C" void mkqsort(unsigned char**, int, int);

//#define SmallSort msd_CE2
//void msd_CE2(unsigned char**, size_t, size_t);

//
// Normal variants
//
void burstsort2_vector(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef std::vector<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = new TrieNode<CharT, BucketT>;
	insert<8192, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}
void burstsort2_brodnik(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef vector_brodnik<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = new TrieNode<CharT, BucketT>;
	insert<16384, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}
void burstsort2_bagwell(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef vector_bagwell<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = new TrieNode<CharT, BucketT>;
	insert<16384, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}
void burstsort2_vector_block(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef vector_block<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = new TrieNode<CharT, BucketT>;
	insert<16384, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}

//
// Superalphabet variants
//
void burstsort2_superalphabet_vector(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef std::vector<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = new TrieNode<CharT, BucketT>;
	insert<32768, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}
void burstsort2_superalphabet_brodnik(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef vector_brodnik<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = new TrieNode<CharT, BucketT>;
	insert<32768, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}
void burstsort2_superalphabet_bagwell(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef vector_bagwell<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = new TrieNode<CharT, BucketT>;
	insert<32768, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}
void burstsort2_superalphabet_vector_block(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef vector_block<unsigned char*, 128> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = new TrieNode<CharT, BucketT>;
	insert<32768, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}

//
// Sampling variants - byte alphabet
//
void burstsort2_sampling_vector(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef std::vector<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = pseudo_sample<CharT, BucketT>(strings, n);
	insert<8192, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}
void burstsort2_sampling_brodnik(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef vector_brodnik<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = pseudo_sample<CharT, BucketT>(strings, n);
	insert<16384, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}
void burstsort2_sampling_bagwell(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef vector_bagwell<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = pseudo_sample<CharT, BucketT>(strings, n);
	insert<16384, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}
void burstsort2_sampling_vector_block(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef vector_block<unsigned char*, 128> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = pseudo_sample<CharT, BucketT>(strings, n);
	insert<16384, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}

//
// Sampling variants - superalphabet
//
void burstsort2_sampling_superalphabet_vector(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef std::vector<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = pseudo_sample<CharT, BucketT>(strings, n);
	insert<16384, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}
void burstsort2_sampling_superalphabet_brodnik(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef vector_brodnik<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = pseudo_sample<CharT, BucketT>(strings, n);
	insert<32768, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}
void burstsort2_sampling_superalphabet_bagwell(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef vector_bagwell<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = pseudo_sample<CharT, BucketT>(strings, n);
	insert<32768, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}
void burstsort2_sampling_superalphabet_vector_block(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef vector_block<unsigned char*, 128> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TrieNode<CharT, BucketT>* root = pseudo_sample<CharT, BucketT>(strings, n);
	insert<32768, BucketT, BurstImpl>(root, strings, n);
	traverse(root, strings, 0, SmallSort);
}

ROUTINE_REGISTER_SINGLECORE(burstsort2_vector,
		"burstsort2 with std::vector bucket type")
ROUTINE_REGISTER_SINGLECORE(burstsort2_brodnik,
		"burstsort2 with vector_brodnik bucket type")
ROUTINE_REGISTER_SINGLECORE(burstsort2_bagwell,
		"burstsort2 with vector_bagwell bucket type")
ROUTINE_REGISTER_SINGLECORE(burstsort2_vector_block,
		"burstsort2 with vector_block bucket type")

ROUTINE_REGISTER_SINGLECORE(burstsort2_superalphabet_vector,
		"superalphabet burstsort2 with std::vector bucket type")
ROUTINE_REGISTER_SINGLECORE(burstsort2_superalphabet_brodnik,
		"superalphabet burstsort2 with vector_brodnik bucket type")
ROUTINE_REGISTER_SINGLECORE(burstsort2_superalphabet_bagwell,
		"superalphabet burstsort2 with vector_bagwell bucket type")
ROUTINE_REGISTER_SINGLECORE(burstsort2_superalphabet_vector_block,
		"superalphabet burstsort2 with vector_block bucket type")

ROUTINE_REGISTER_SINGLECORE(burstsort2_sampling_vector,
		"sampling burstsort2 with std::vector bucket type")
ROUTINE_REGISTER_SINGLECORE(burstsort2_sampling_brodnik,
		"sampling burstsort2 with vector_brodnik bucket type")
ROUTINE_REGISTER_SINGLECORE(burstsort2_sampling_bagwell,
		"sampling burstsort2 with vector_bagwell bucket type")
ROUTINE_REGISTER_SINGLECORE(burstsort2_sampling_vector_block,
		"sampling burstsort2 with vector_block bucket type")

ROUTINE_REGISTER_SINGLECORE(burstsort2_sampling_superalphabet_vector,
		"sampling superalphabet burstsort2 with std::vector bucket type")
ROUTINE_REGISTER_SINGLECORE(burstsort2_sampling_superalphabet_brodnik,
		"sampling superalphabet burstsort2 with vector_brodnik bucket type")
ROUTINE_REGISTER_SINGLECORE(burstsort2_sampling_superalphabet_bagwell,
		"sampling superalphabet burstsort2 with vector_bagwell bucket type")
ROUTINE_REGISTER_SINGLECORE(burstsort2_sampling_superalphabet_vector_block,
		"sampling superalphabet burstsort2 with vector_block bucket type")
