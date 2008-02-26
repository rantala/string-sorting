/*
 *   Copyright 2007-2008 by Tommi Rantala <tommi.rantala@cs.helsinki.fi>
 *
 *   *************************************************************************
 *   * Use, modification, and distribution are subject to the Boost Software *
 *   * License, Version 1.0. (See accompanying file LICENSE or a copy at     *
 *   * http://www.boost.org/LICENSE_1_0.txt)                                 *
 *   *************************************************************************
 */

/*
 * msd_DB() is an implementation of the MSD radix sort
 *  - uses dynamic bucket sizes, ie. just one sweep over data
 *  - uses the almost in place distribution method described in:
 *
 *        @article{1217858,
 *           author = {Juha K\"{a}rkk\"{a}inen and Peter Sanders and Stefan Burkhardt},
 *           title = {Linear work suffix array construction},
 *           journal = {J. ACM},
 *           volume = {53},
 *           number = {6},
 *           year = {2006},
 *           issn = {0004-5411},
 *           pages = {918--936},
 *           doi = {http://doi.acm.org/10.1145/1217856.1217858},
 *           publisher = {ACM},
 *           address = {New York, NY, USA},
 *        }
 *
 *    See appendix B.
 *
 *  - The idea is to save some memory when the size of the subinput is large.
 *    For small n, we switch to more efficient MSD radix sort variants.
 */

#include "util.h"
#include <vector>
#include <list>
#include <algorithm>
#include <iostream>
//#include <deque>

void msd2(unsigned char** strings, size_t n, size_t depth);

static inline uint16_t
double_char(unsigned char* str, size_t depth)
{
	unsigned c = str[depth];
	return (c << 8) | (c != 0 ? str[depth+1] : c);
}

typedef unsigned char** Block;
//typedef std::deque<Block> FreeBlocks;
typedef std::list<Block> FreeBlocks;
typedef std::list<Block> Bucket;
typedef boost::array<Bucket, 256> Buckets;
typedef std::vector<Block*> BackLinks;

static inline Block
take_free_block(FreeBlocks& fb)
{
	assert(not fb.empty());
	Block b(fb.front());
	fb.pop_front();
	return b;
}

template <unsigned B>
static void
msd_D(unsigned char** strings, size_t n, size_t depth)
{
	if (n < 0x10000) {
		msd2(strings, n, depth);
		return;
	}
	assert(n > B);
	static Buckets buckets;
	static boost::array<unsigned char*, (256+6)*B> temp_space;
	static FreeBlocks freeblocks;
	BackLinks backlinks(n/B+1);
	boost::array<size_t, 256> bucketsize;

	for (unsigned i=0; i < 256; ++i) {
		bucketsize[i] = 0;
		buckets[i].clear();
	}

	// Initialize our list of free blocks.
	for (size_t i=0; i < 256+6; ++i)
		freeblocks.push_back(&temp_space[i*B]);
	for (size_t i=0; i < n-B; i+=B)
		freeblocks.push_back(&strings[i]);

	// Distribute strings to buckets. Use a small cache to reduce memory
	// stalls. The exact size of the cache is not very important.
	size_t i=0;
	for (; i < n-n%32; i+=32) {
		unsigned char cache[32];
		for (unsigned j=0; j < 32; ++j) {
			cache[j] = strings[i+j][depth];
		}
		for (unsigned j=0; j < 32; ++j) {
			const unsigned char c = cache[j];
			if (bucketsize[c] % B == 0) {
				Block b = take_free_block(freeblocks);
				buckets[c].push_back(b);
				// Backlinks must be set for those blocks, that
				// use the original string array space.
				if (b >= strings && b < strings+n) {
					backlinks[(b-strings)/B] = &(buckets[c].back());
				}
			}
			assert(not buckets[c].empty());
			buckets[c].back()[bucketsize[c] % B] = strings[i+j];
			++bucketsize[c];
		}
	}
	for (; i < n; ++i) {
		const unsigned char c = strings[i][depth];
		if (bucketsize[c] % B == 0) {
			Block b = take_free_block(freeblocks);
			buckets[c].push_back(b);
			if (b >= strings && b < strings+n) {
				backlinks[(b-strings)/B] = &(buckets[c].back());
			}
		}
		assert(not buckets[c].empty());
		buckets[c].back()[bucketsize[c] % B] = strings[i];
		++bucketsize[c];
	}

	// Process each bucket, and copy all strings in that bucket to proper
	// place in the original string pointer array. This means that those
	// positions that are occupied by other blocks must be moved to free
	// space etc.
	size_t pos = 0;
	for (unsigned i=0; i < 256; ++i) {
		if (bucketsize[i] == 0) continue;

		Bucket::const_iterator it = buckets[i].begin();
		for (size_t bucket_pos=0; bucket_pos < bucketsize[i]; ++it, bucket_pos+=B) {
			const size_t block_items = std::min(B, bucketsize[i]-bucket_pos);
			const size_t block_overlap = (pos+block_items-1)/B;

			if (*it == (strings+pos)) {
				assert(pos%B==0);
				backlinks[pos/B] = 0;
				pos += block_items;
				continue;
			}

			// Don't overwrite the block in the position we are
			// about to write to, but copy it into safety.
			if (backlinks[block_overlap]) {
				// Take a free block. The block can be 'stale',
				// i.e. it can point to positions we have
				// already copied new strings into. Take free
				// blocks until we have non-stale block.
				Block tmp = take_free_block(freeblocks);
				while (tmp >= strings && tmp < strings+pos)
					tmp = take_free_block(freeblocks);
				if (tmp >= strings && tmp < strings+n) {
					assert(backlinks[(tmp-strings)/B]==0);
					backlinks[(tmp-strings)/B] = backlinks[block_overlap];
				}
				memcpy(tmp, *(backlinks[block_overlap]), B*sizeof(unsigned char*));
				*(backlinks[block_overlap]) = tmp;
				backlinks[block_overlap] = 0;
			}

			if (*it >= strings && *it < strings+n) {
				assert(*it > strings+pos);
				backlinks[(*it-strings)/B] = 0;
			}

			// Copy string pointers to correct position.
			memcpy(strings+pos, *it, block_items*sizeof(unsigned char*));

			// Return block for later use. Favor those in the
			// temporary space.
			if (*it >= strings && *it < strings+n) {
				freeblocks.push_back(*it);
			} else {
				freeblocks.push_front(*it);
			}

			pos += block_items;
		}
	}
	freeblocks.clear();
	backlinks.clear();
	pos = bucketsize[0];
	for (unsigned i=1; i < 256; ++i) {
		if (bucketsize[i] == 0) continue;
		msd_D<B>(strings+pos, bucketsize[i], depth+1);
		pos += bucketsize[i];
	}
}

void msd_DB(unsigned char** strings, size_t n)
{ msd_D<1024>(strings, n, 0); }
