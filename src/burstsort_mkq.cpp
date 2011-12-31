/*
 * Copyright 2007-2008 by Tommi Rantala <tt.rantala@gmail.com>
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
 * Implements the Multi-Key-Quicksort using an explicit ternary tree, similar
 * to the burstsort algorithm. Might not be optimal, because pivots are chosen
 * based on subinputs.
 */

#include "routine.h"
#include "util/get_char.h"
#include "util/median.h"
#include <iostream>
#include <vector>
#include <bitset>
#include <boost/static_assert.hpp>
#include <boost/array.hpp>

// Ternary search tree:
//    0 : left, smaller than pivot
//    1 : middle, equal to pivot
//    2 : right, larger than pivot
//
// if is_tst[i] is true, then buckets[i] is a TSTNode
// if is_tst[i] is false, then buckets[i] is a Bucket

template <typename CharT>
struct TSTNode
{
	boost::array<void*, 3> buckets;
	std::bitset<3> is_tst;
	CharT pivot;
	TSTNode() { buckets.assign(0); }
};

#define is_middle_bucket(x) ((x)==1)

template <typename CharT>
struct BurstSimple
{
	template <typename BucketT>
	TSTNode<CharT>*
	operator()(const BucketT& bucket, CharT* oracle, size_t /*depth*/) const
	{
		TSTNode<CharT>* new_node = new TSTNode<CharT>;
		BucketT* bucket0 = new BucketT;
		BucketT* bucket1 = new BucketT;
		BucketT* bucket2 = new BucketT;
		CharT pivot = pseudo_median(oracle, oracle+bucket.size());
		for (unsigned i=0; i < bucket.size(); ++i) {
			if (oracle[i] < pivot) {
				bucket0->push_back(bucket[i]);
			} else if (oracle[i] == pivot) {
				bucket1->push_back(bucket[i]);
			} else {
				bucket2->push_back(bucket[i]);
			}
		}
		new_node->pivot = pivot;
		new_node->buckets[0] = bucket0;
		new_node->buckets[1] = bucket1;
		new_node->buckets[2] = bucket2;
		return new_node;
	}
};

template <typename CharT>
struct BurstRecursive
{
	template <typename BucketT>
	TSTNode<CharT>*
	operator()(const BucketT& bucket, CharT* oracle, size_t depth) const
	{
		TSTNode<CharT>* new_node
			= BurstSimple<CharT>()(bucket, oracle, depth);
		BucketT* bucket0 = static_cast<BucketT*>(new_node->buckets[0]);
		BucketT* bucket1 = static_cast<BucketT*>(new_node->buckets[1]);
		BucketT* bucket2 = static_cast<BucketT*>(new_node->buckets[2]);
		const unsigned threshold = std::max(100u,
				unsigned(0.7f*bucket.size()));
		assert(bucket0->size() + bucket1->size() + bucket2->size()
				== bucket.size());
		size_t bsize = bucket0->size();
		if (bucket0->size() > threshold) {
			new_node->buckets[0] = this->operator()(*bucket0,
					oracle, depth);
			delete bucket0;
			new_node->is_tst[0] = true;
		} else {
			new_node->buckets[0] = bucket0;
			assert(new_node->is_tst[0] == false);
		}
		if (bucket1->size() > threshold and not is_end(new_node->pivot)) {
			for (unsigned i=0; i < bucket1->size(); ++i) {
				oracle[bsize+i] = get_char<CharT>((*bucket1)[i],
						depth+sizeof(CharT));
			}
			new_node->buckets[1] = this->operator()(*bucket1,
					oracle+bsize, depth+sizeof(CharT));
			bsize += bucket1->size();
			delete bucket1;
			new_node->is_tst[1] = true;
		} else {
			new_node->buckets[1] = bucket1;
			assert(new_node->is_tst[1] == false);
		}
		if (bucket2->size() > threshold) {
			new_node->buckets[2] = this->operator()(*bucket2,
					oracle+bsize, depth);
			delete bucket2;
			new_node->is_tst[2] = true;
		} else {
			new_node->buckets[2] = bucket2;
			assert(new_node->is_tst[2] == false);
		}
		return new_node;
	}
};

template <typename CharT>
static inline unsigned
get_bucket(CharT c, CharT pivot)
{
        return ((c > pivot) << 1) | (c == pivot);
}

template <unsigned Threshold, typename BucketT,
          typename BurstImpl, typename CharT>
static inline void
burst_insert(TSTNode<CharT>* root, unsigned char** strings, size_t N)
{
	for (size_t i=0; i < N; ++i) {
		unsigned char* str = strings[i];
		size_t depth = 0;
		CharT c = get_char<CharT>(str, depth);
		TSTNode<CharT>* node = root;
		unsigned bucket = get_bucket(c, node->pivot);
		while (node->is_tst[bucket]) {
			if (is_middle_bucket(bucket)) {
				depth += sizeof(CharT);
				c = get_char<CharT>(str, depth);
			}
			node = static_cast<TSTNode<CharT>*>(
					node->buckets[bucket]);
			bucket = get_bucket(c, node->pivot);
		}
		BucketT* buck = static_cast<BucketT*>(node->buckets[bucket]);
		if (not buck)
			node->buckets[bucket] = buck = new BucketT;
		buck->push_back(str);
		if (is_middle_bucket(bucket) && is_end(node->pivot)) {
			continue;
		}
		if (buck->size() > sizeof(CharT)*Threshold
				and buck->size() == buck->capacity()) {
			if (is_middle_bucket(bucket)) {
				depth += sizeof(CharT);
			}
			CharT* oracle = static_cast<CharT*>(
				malloc(buck->size()*sizeof(CharT)));
			for (unsigned j=0; j < buck->size(); ++j) {
				oracle[j] = get_char<CharT>((*buck)[j], depth);
			}
			TSTNode<CharT>* new_node
				= BurstImpl()(*buck, oracle, depth);
			free(oracle);
			delete buck;
			node->buckets[bucket] = new_node;
			node->is_tst[bucket] = true;
		}
	}
}

extern "C" void mkqsort(unsigned char**, int, int);

template <typename BucketT, typename CharT> static inline size_t
burst_traverse(TSTNode<CharT>* node, unsigned char** strings, size_t pos,
		size_t depth);

template <unsigned BucketNum, typename BucketT, typename CharT>
static inline size_t
handle_bucket(TSTNode<CharT>* node,
              unsigned char** strings,
              size_t pos,
              size_t depth)
{
	BOOST_STATIC_ASSERT(BucketNum < 3);
	if (node->is_tst[BucketNum]) {
		pos = burst_traverse<BucketT>(
			static_cast<TSTNode<CharT>*>(node->buckets[BucketNum]),
			strings,
			pos,
			depth + (BucketNum==1)*sizeof(CharT));
	} else if (node->buckets[BucketNum]) {
		BucketT* buck = static_cast<BucketT*>(node->buckets[BucketNum]);
		size_t bsize = buck->size();
		std::copy(buck->begin(), buck->end(), strings+pos);
		delete buck;
		if (not is_middle_bucket(BucketNum)) {
			mkqsort(strings+pos, bsize, depth);
		} else if (not is_end(node->pivot)) {
			mkqsort(strings+pos,
				bsize,
				depth+sizeof(CharT));
		}
		pos += bsize;
	}
	return pos;
}

template <typename BucketT, typename CharT>
static inline size_t
burst_traverse(TSTNode<CharT>* node,
               unsigned char** strings,
               size_t pos,
               size_t depth)
{
	pos = handle_bucket<0, BucketT>(node, strings, pos, depth);
	pos = handle_bucket<1, BucketT>(node, strings, pos, depth);
	pos = handle_bucket<2, BucketT>(node, strings, pos, depth);
	delete node;
	return pos;
}

template <typename CharT>
static inline void
burstsort_mkq_simpleburst(unsigned char** strings, size_t N)
{
	typedef std::vector<unsigned char*> BucketT;
	typedef BurstSimple<CharT> BurstImpl;
	TSTNode<CharT>* root = new TSTNode<CharT>;
	root->pivot = pseudo_median<CharT>(strings, N, 0);
	burst_insert<8192, BucketT, BurstImpl>(root, strings, N);
	burst_traverse<BucketT>(root, strings, 0, 0);
}

void burstsort_mkq_simpleburst_1(unsigned char** strings, size_t N)
{ burstsort_mkq_simpleburst<unsigned char>(strings, N); }

void burstsort_mkq_simpleburst_2(unsigned char** strings, size_t N)
{ burstsort_mkq_simpleburst<uint16_t>(strings, N); }

void burstsort_mkq_simpleburst_4(unsigned char** strings, size_t N)
{ burstsort_mkq_simpleburst<uint32_t>(strings, N); }

ROUTINE_REGISTER_SINGLECORE(burstsort_mkq_simpleburst_1,
		"burstsort_mkq 1byte alphabet with simpleburst")
ROUTINE_REGISTER_SINGLECORE(burstsort_mkq_simpleburst_2,
		"burstsort_mkq 2byte alphabet with simpleburst")
ROUTINE_REGISTER_SINGLECORE(burstsort_mkq_simpleburst_4,
		"burstsort_mkq 4byte alphabet with simpleburst")

template <typename CharT>
static inline void
burstsort_mkq_recursiveburst(unsigned char** strings, size_t N)
{
	typedef std::vector<unsigned char*> BucketT;
	typedef BurstRecursive<CharT> BurstImpl;
	TSTNode<CharT>* root = new TSTNode<CharT>;
	root->pivot = pseudo_median<CharT>(strings, N, 0);
	burst_insert<8192, BucketT, BurstImpl>(root, strings, N);
	burst_traverse<BucketT>(root, strings, 0, 0);
}

void burstsort_mkq_recursiveburst_1(unsigned char** strings, size_t N)
{ burstsort_mkq_recursiveburst<unsigned char>(strings, N); }

void burstsort_mkq_recursiveburst_2(unsigned char** strings, size_t N)
{ burstsort_mkq_recursiveburst<uint16_t>(strings, N); }

void burstsort_mkq_recursiveburst_4(unsigned char** strings, size_t N)
{ burstsort_mkq_recursiveburst<uint32_t>(strings, N); }

ROUTINE_REGISTER_SINGLECORE(burstsort_mkq_recursiveburst_1,
		"burstsort_mkq 1byte alphabet with recursiveburst")
ROUTINE_REGISTER_SINGLECORE(burstsort_mkq_recursiveburst_2,
		"burstsort_mkq 2byte alphabet with recursiveburst")
ROUTINE_REGISTER_SINGLECORE(burstsort_mkq_recursiveburst_4,
		"burstsort_mkq 4byte alphabet with recursiveburst")
