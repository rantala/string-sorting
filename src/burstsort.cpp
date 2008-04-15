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
 * Implementation of the burstsort algorithm by Sinha, Zobel et al.
 *
 * @article{1041517,
 *     author = {Ranjan Sinha and Justin Zobel},
 *     title = {Cache-conscious sorting of large sets of strings with dynamic tries},
 *     journal = {J. Exp. Algorithmics},
 *     volume = {9},
 *     year = {2004},
 *     issn = {1084-6654},
 *     pages = {1.5},
 *     doi = {http://doi.acm.org/10.1145/1005813.1041517},
 *     publisher = {ACM},
 *     address = {New York, NY, USA},
 * }
 */

#include "util/get_char.h"
#include "util/debug.h"
#include <vector>
#include <iostream>
#include <bitset>
#include "vector_bagwell.h"
#include "vector_brodnik.h"
#include "vector_block.h"
#include <boost/array.hpp>

using boost::array;

// Unfortunately std::numeric_limits<T>::max() cannot be used as constant
// values in template parameters.
template <typename T> struct max {};
template <> struct max<unsigned char> { enum { value = 0x100   }; };
template <> struct max<uint16_t>      { enum { value = 0x10000 }; };

template <typename CharT>
struct TrieNode
{
	// One subtree per alphabet. Points to either a 'TrieNode' or a
	// 'Bucket' node. Use the value from is_trie to know which one.
	array<void*, max<CharT>::value> buckets;
	// is_trie[i] equals true if buckets[i] points to a TrieNode
	// is_trie[i] equals false if buckets[i] points to a Bucket
	std::bitset<max<CharT>::value>  is_trie;
	TrieNode() { buckets.assign(0); }
};

// The burst algorithm as described by Sinha, Zobel et al.
template <typename CharT>
struct BurstSimple
{
	template <typename BucketT>
	TrieNode<CharT>*
	operator()(const BucketT& bucket, size_t depth) const
	{
		TrieNode<CharT>* new_node = new TrieNode<CharT>;
		const unsigned bucket_size = bucket.size();
		// Use a small cache to reduce memory stalls. Also cache the
		// string pointers, in case the indexing operation of the
		// container is expensive.
		unsigned i=0;
		for (; i < bucket_size-bucket_size%64; i+=64) {
			array<CharT, 64> cache;
			array<unsigned char*, 64> strings;
			for (unsigned j=0; j < 64; ++j) {
				strings[j] = bucket[i+j];
				cache[j] = get_char<CharT>(strings[j], depth);
			}
			for (unsigned j=0; j < 64; ++j) {
				const CharT ch = cache[j];
				BucketT* sub_bucket = static_cast<BucketT*>(
					new_node->buckets[ch]);
				if (not sub_bucket) {
					new_node->buckets[ch] = sub_bucket
						= new BucketT;
				}
				sub_bucket->push_back(strings[j]);
			}
		}
		for (; i < bucket_size; ++i) {
			unsigned char* ptr = bucket[i];
			const CharT ch = get_char<CharT>(ptr, depth);
			BucketT* sub_bucket = static_cast<BucketT*>(
				new_node->buckets[ch]);
			if (not sub_bucket) {
				new_node->buckets[ch] = sub_bucket
					= new BucketT;
			}
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
	TrieNode<CharT>*
	operator()(const BucketT& bucket, size_t depth) const
	{
		TrieNode<CharT>* new_node
			= BurstSimple<CharT>()(bucket, depth);
		const size_t threshold = std::max(
				//size_t(100), size_t(0.4f*bucket.size()));
				size_t(100), bucket.size()/2);
		for (unsigned i=0; i < max<CharT>::value; ++i) {
			assert(new_node->is_trie[i] == false);
			BucketT* sub_bucket = static_cast<BucketT*>(
					new_node->buckets[i]);
			if (not sub_bucket) continue;
			if (not is_end(i) and sub_bucket->size() > threshold) {
				new_node->buckets[i] =
					BurstRecursive<CharT>()(*sub_bucket,
							depth+sizeof(CharT));
				delete sub_bucket;
				new_node->is_trie[i] = true;
			}
		}
		return new_node;
	}
};

// Uses a random sample to create an initial tree.
template <typename CharT>
static TrieNode<CharT>*
random_sample(unsigned char** strings, size_t n)
{
	// Limit the maximum number of nodes to whatever fits in 30 megabytes.
	const size_t sample_size = n/8192;
	size_t max_nodes = 30000000/sizeof(TrieNode<CharT>);
	debug()<<__PRETTY_FUNCTION__<<" sampling "<<sample_size<<" strings\n";
	TrieNode<CharT>* root = new TrieNode<CharT>;
	for (size_t i=0; i < sample_size; ++i) {
		unsigned char* str = strings[size_t(drand48()*n)];
		size_t depth = 0;
		TrieNode<CharT>* node = root;
		while (true) {
			CharT c = get_char<CharT>(str, depth);
			if (is_end(c)) break;
			depth += sizeof(CharT);
			if (not node->is_trie[c]) {
				node->is_trie[c] = true;
				node->buckets[c] = new TrieNode<CharT>;
				if (--max_nodes==0) goto finish;
			}
			node = static_cast<TrieNode<CharT>*>(node->buckets[c]);
			assert(node);
		}
	}
finish:
	return root;
}

// Uses a pseudo random sample to create an initial tree.
template <typename CharT>
static TrieNode<CharT>*
pseudo_sample(unsigned char** strings, size_t n)
{
	// Limit the maximum number of nodes to whatever fits in 30 megabytes.
	debug()<<__func__<<"(): sampling "<<n/8192<<" strings ...\n";
	size_t max_nodes = 30000000/sizeof(TrieNode<CharT>);
	TrieNode<CharT>* root = new TrieNode<CharT>;
	for (size_t i=0; i < n; i += 8192) {
		unsigned char* str = strings[i];
		size_t depth = 0;
		TrieNode<CharT>* node = root;
		while (true) {
			CharT c = get_char<CharT>(str, depth);
			if (is_end(c)) break;
			depth += sizeof(CharT);
			if (not node->is_trie[c]) {
				node->is_trie[c] = true;
				node->buckets[c] = new TrieNode<CharT>;
				if (--max_nodes==0) goto finish;
			}
			node = static_cast<TrieNode<CharT>*>(node->buckets[c]);
			assert(node);
		}
	}
finish:
	debug()<<"   Sampling done, created "
	       <<(30000000/sizeof(TrieNode<CharT>))-max_nodes<<" nodes.\n";
	return root;
}

template <unsigned Threshold, typename BucketT,
          typename BurstImpl, typename CharT>
static inline void
insert(TrieNode<CharT>* root, unsigned char** strings, size_t n)
{
	for (size_t i=0; i < n; ++i) {
		unsigned char* str = strings[i];
		size_t depth = 0;
		CharT c = get_char<CharT>(str, 0);
		TrieNode<CharT>* node = root;
		while (node->is_trie[c]) {
			assert(not is_end(c));
			node = static_cast<TrieNode<CharT>*>(node->buckets[c]);
			depth += sizeof(CharT);
			c = get_char<CharT>(str, depth);
		}
		BucketT* bucket = static_cast<BucketT*>(node->buckets[c]);
		if (not bucket) {
			node->buckets[c] = bucket = new BucketT;
		}
		bucket->push_back(str);
		if (is_end(c)) continue;
		if (bucket->size() > Threshold) {
			node->buckets[c] = BurstImpl()(*bucket,
					depth+sizeof(CharT));
			node->is_trie[c] = true;
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
template <typename BucketT, typename SmallSort, typename CharT>
static unsigned char**
traverse(TrieNode<CharT>* node,
         unsigned char** dst,
         size_t depth,
         SmallSort small_sort)
{
	for (unsigned i=0; i < max<CharT>::value; ++i) {
		if (node->is_trie[i]) {
			dst = traverse<BucketT>(
				static_cast<TrieNode<CharT>*>(node->buckets[i]),
				dst, depth+sizeof(CharT), small_sort);
		} else {
			BucketT* bucket =
				static_cast<BucketT*>(node->buckets[i]);
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
void burstsort_vector(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef std::vector<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = new TrieNode<CharT>;
	insert<8000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
void burstsort_brodnik(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef vector_brodnik<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = new TrieNode<CharT>;
	insert<16000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
void burstsort_bagwell(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef vector_bagwell<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = new TrieNode<CharT>;
	insert<16000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
void burstsort_vector_block(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef vector_block<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = new TrieNode<CharT>;
	insert<16000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}

//
// Superalphabet variants
//
void burstsort_superalphabet_vector(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef std::vector<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = new TrieNode<CharT>;
	insert<32000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
void burstsort_superalphabet_brodnik(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef vector_brodnik<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = new TrieNode<CharT>;
	insert<32000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
void burstsort_superalphabet_bagwell(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef vector_bagwell<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = new TrieNode<CharT>;
	insert<32000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
void burstsort_superalphabet_vector_block(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef vector_block<unsigned char*, 128> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = new TrieNode<CharT>;
	insert<32000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}

//
// Sampling variants - byte alphabet
//
void burstsort_sampling_vector(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef std::vector<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = pseudo_sample<CharT>(strings, n);
	insert<8000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
void burstsort_sampling_brodnik(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef vector_brodnik<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = pseudo_sample<CharT>(strings, n);
	insert<16000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
void burstsort_sampling_bagwell(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef vector_bagwell<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = pseudo_sample<CharT>(strings, n);
	insert<16000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
void burstsort_sampling_vector_block(unsigned char** strings, size_t n)
{
	typedef unsigned char CharT;
	typedef vector_block<unsigned char*, 128> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = pseudo_sample<CharT>(strings, n);
	insert<16000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}

//
// Sampling variants - superalphabet
//
void burstsort_sampling_superalphabet_vector(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef std::vector<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = pseudo_sample<CharT>(strings, n);
	insert<16000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
void burstsort_sampling_superalphabet_brodnik(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef vector_brodnik<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = pseudo_sample<CharT>(strings, n);
	insert<32000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
void burstsort_sampling_superalphabet_bagwell(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef vector_bagwell<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = pseudo_sample<CharT>(strings, n);
	insert<32000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
void burstsort_sampling_superalphabet_vector_block(unsigned char** strings, size_t n)
{
	typedef uint16_t CharT;
	typedef vector_block<unsigned char*, 128> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	//typedef BurstRecursive<CharT> BurstImpl;
	TrieNode<CharT>* root = pseudo_sample<CharT>(strings, n);
	insert<32000, BucketT, BurstImpl>(root, strings, n);
	traverse<BucketT>(root, strings, 0, SmallSort);
}
