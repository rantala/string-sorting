// Copyright 2011, Takuya Akiba
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Takuya Akiba nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef PARALLEL_STRING_RADIX_SORT_H_
#define PARALLEL_STRING_RADIX_SORT_H_

//#ifdef _OPENMP
//#include <omp.h>
//#endif

#include <stdint.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#define PSRS_CHECK(expr)                                                \
  if (expr) {                                                           \
  } else {                                                              \
    fprintf(stderr, "CHECK Failed(%s:%d): %s\n",                        \
            __FILE__, __LINE__, #expr);                                 \
    exit(EXIT_FAILURE);                                                 \
  }

namespace parallel_string_radix_sort {
namespace internal {
/**
 * Comparison function class used in |ParallelStringRadixSort|.
 */
template<typename StringType> class Compare;

template<> class Compare<const char*> {
 public:
  explicit Compare(int depth) : depth_(depth) {}

  inline bool operator()(const char* const a, const char* const b) {
    return strcmp(a + depth_, b + depth_) < 0;
  }
 private:
  int depth_;
};

template<> class Compare<std::string> {
 public:
  explicit Compare(int depth) : depth_(depth) {}

  inline bool operator()(const std::string &a, const std::string &b) {
    return a.compare(depth_, a.length() - depth_,
                     b, depth_, b.length() - depth_) < 0;
  }
 private:
  int depth_;
};
}  // namespace internal

/**
 * The class to perform string sorting.
 *
 * @param StringType The type of the strings to be sorted.
 *                   This should be either of |const char*| or |std::string|.
 */
template<typename StringType>
class ParallelStringRadixSort {
 public:
  ParallelStringRadixSort();

  ~ParallelStringRadixSort();

  /**
   * Initialize the class.
   *
   * @param max_elems the maximum number of the elements to be sorted
   */
  void Init(size_t max_elems);

  /**
   * Sort an array of strings.
   *
   * @param strings the array to be sorted
   * @param num_elems the number of the elements in the given array
   */
  void Sort(StringType *strings, size_t num_elems);

 private:
  /// The theshold of size of ranges to call |std::sort|
  static const size_t kThreshold = 30;
  /// The limit of depth to call |std::sort|
  static const size_t kDepthLimit = 100;

  size_t max_elems_;

  StringType *data_, *temp_;

  uint8_t *letters8_;
  uint16_t *letters16_;

  /**
   * Release all the allocated resources.
   */
  void DeleteAll();

  /**
   * Sort elements in range [bgn, end),
   * which have common prefix with |depth| characters.
   *
   * @param flip When false, read from |data_|, write to |temp_| and recurse.
   *             When true, read from |temp_|, write to |data_| and recurse.
   */
  void Sort8(size_t bgn, size_t end, size_t depth, bool flip);

  /**
   * Sort elements in range [bgn, end),
   * which have common prefix with |depth| characters.
   *
   * Treat pairs of characters as single super characters.
   *
   * @param flip When false, read from |data_|, write to |temp_| and recurse.
   *             When true, read from |temp_|, write to |data_| and recurse.
   */
  void Sort16(size_t bgn, size_t end, size_t depth, bool flip);

  /**
   * Sort elements in range [bgn, end),
   * which have common prefix with |depth| characters.
   *
   * Treat pairs of characters as single super characters.
   *
   * Builds threads and recurse in parallel.
   *
   * @param flip When false, read from |data_|, write to |temp_| and recurse.
   *             When true, read from |temp_|, write to |data_| and recurse.
   */
  void Sort16Parallel(size_t bgn, size_t end, size_t depth, bool flip);

  /**
   * Sort elements in range [bgn, end),
   * which have common prefix with |depth| characters.
   *
   * The size of the range determines the action of this function.
   *
   * If the size is sufficiently small (smaller than or equal to |kThreshold|),
   * we call |std::sort| complete the sorting.
   * In doing so, regardless of |flip|, write the result to |data_|.
   *
   * If the size is smaller than |1 << 16| then we call |Sort8|,
   * otherwise we call |Sort16|.
   *
   * If the |depth| is larget than or equal to |kDepthLimit|,
   * we also use |std::sort|. This is to avoid stack overflow.
   */
  inline void Recurse(size_t bgn, size_t end, size_t depth, bool flip) {
    size_t n = end - bgn;
    if (depth >= kDepthLimit || n <= kThreshold) {
      if (flip) {
        for (size_t i = bgn; i < end; ++i) {
          std::swap(data_[i], temp_[i]);
        }
      }
      if (n > 1) {
        std::sort(data_ + bgn, data_ + end, internal::Compare<StringType>(depth));
      }
    } else if (n < (1 << 16)) {
      Sort8(bgn, end, depth, flip);
    } else {
      Sort16(bgn, end, depth, flip);
    }
  }
};

template<typename StringType>
ParallelStringRadixSort<StringType>::ParallelStringRadixSort()
    : max_elems_(0), temp_(NULL), letters8_(NULL), letters16_(NULL) {}

template<typename StringType>
ParallelStringRadixSort<StringType>::~ParallelStringRadixSort() {
  DeleteAll();
}

template<typename StringType>
void ParallelStringRadixSort<StringType>::Init(size_t max_elems) {
  DeleteAll();

  max_elems_ = max_elems;
  temp_ = new StringType[max_elems];
  letters8_ = new uint8_t[max_elems];
  letters16_ = new uint16_t[max_elems];
  PSRS_CHECK(temp_ != NULL && letters8_ != NULL && letters16_ != NULL);
}

template<typename StringType>
void ParallelStringRadixSort<StringType>::DeleteAll() {
  delete [] temp_;
  delete [] letters8_;
  delete [] letters16_;
  max_elems_ = 0;
  temp_ = NULL;
  letters8_ = NULL;
  letters16_ = NULL;
}

template<typename StringType>
void ParallelStringRadixSort<StringType>
::Sort(StringType *strings, size_t num_elems) {
  assert(num_elems <= max_elems_);
  data_ = strings;
  Sort16Parallel(0, num_elems, 0, false);
}

template<typename StringType>
void ParallelStringRadixSort<StringType>
::Sort8(size_t bgn, size_t end, size_t depth, bool flip) {
  // Here we do not use |new| or |malloc|. This is because:
  // 1. these may be heavy bottle-necks under multi-thread, and
  // 2. the size is sufficiently small for preventing stack overflow.
  size_t cnt[1 << 8] = {};

  StringType *src = (flip ? temp_ : data_) + bgn;
  StringType *dst = (flip ? data_ : temp_) + bgn;
  uint8_t *let = letters8_ + bgn;
  size_t n = end - bgn;

  for (size_t i = 0; i < n; ++i) {
    let[i] = src[i][depth];
  }

  for (size_t i = 0; i < n; ++i) {
    ++cnt[let[i]];
  }

  size_t s = 0;
  for (int i = 0; i < 1 << 8; ++i) {
    std::swap(cnt[i], s);
    s += cnt[i];
  }

  for (size_t i = 0; i < n; ++i) {
    std::swap(dst[cnt[let[i]]++], src[i]);
  }

  if (flip == false) {
    size_t b = 0, e = cnt[0];
    for (size_t j = b; j < e; ++j) {
      std::swap(src[j], dst[j]);
    }
  }

  for (size_t i = 1; i < 1 << 8; ++i) {
    if (cnt[i] - cnt[i - 1] >= 1) {
      Recurse(bgn + cnt[i - 1], bgn + cnt[i], depth + 1, !flip);
    }
  }
}

template<typename StringType>
void ParallelStringRadixSort<StringType>
::Sort16(size_t bgn, size_t end, size_t depth, bool flip) {
  size_t *cnt = new size_t[1 << 16]();
  PSRS_CHECK(cnt != NULL);

  StringType *src = (flip ? temp_ : data_) + bgn;
  StringType *dst = (flip ? data_ : temp_) + bgn;
  uint16_t *let = letters16_ + bgn;
  size_t n = end - bgn;

  for (size_t i = 0; i < n; ++i) {
    uint16_t x = src[i][depth];
    let[i] = x == 0 ? 0 : ((x << 8) | src[i][depth + 1]);
  }

  for (size_t i = 0; i < n; ++i) {
    ++cnt[let[i]];
  }

  size_t s = 0;
  for (int i = 0; i < 1 << 16; ++i) {
    std::swap(cnt[i], s);
    s += cnt[i];
  }

  for (size_t i = 0; i < n; ++i) {
    std::swap(dst[cnt[let[i]]++], src[i]);
  }

  if (flip == false) {
    for (int i = 0; i < 1 << 8; ++i) {
      size_t b = i == 0 ? 0 : cnt[(i << 8) - 1];
      size_t e = cnt[i << 8];
      for (size_t j = b; j < e; ++j) {
        std::swap(src[j], dst[j]);
      }
    }
  }

  for (size_t i = 1; i < 1 << 16; ++i) {
    if ((i & 0xFF) != 0 && cnt[i] - cnt[i - 1] >= 1) {
      Recurse(bgn + cnt[i - 1], bgn + cnt[i], depth + 2, !flip);
    }
  }

  delete [] cnt;
}

template<typename StringType>
void ParallelStringRadixSort<StringType>
::Sort16Parallel(size_t bgn, size_t end, size_t depth, bool flip) {
  size_t cnt[1 << 16] = {};

  StringType *src = (flip ? temp_ : data_) + bgn;
  StringType *dst = (flip ? data_ : temp_) + bgn;
  uint16_t *let = letters16_ + bgn;
  size_t n = end - bgn;

  #ifdef _OPENMP
  #pragma omp parallel for schedule(static)
  #endif
  for (size_t i = 0; i < n; ++i) {
    uint16_t x = src[i][depth];
    let[i] = x == 0 ? 0 : ((x << 8) | src[i][depth + 1]);
  }

  for (size_t i = 0; i < n; ++i) {
    ++cnt[let[i]];
  }

  {
    size_t s = 0;
    for (int i = 0; i < 1 << 16; ++i) {
      std::swap(cnt[i], s);
      s += cnt[i];
    }
  }

  for (size_t i = 0; i < n; ++i) {
    std::swap(dst[cnt[let[i]]++], src[i]);
  }

  if (flip == false) {
    #ifdef _OPENMP
    #pragma omp parallel for schedule(static)
    #endif
    for (int i = 0; i < 1 << 8; ++i) {
      size_t b = i == 0 ? 0 : cnt[(i << 8) - 1];
      size_t e = cnt[i << 8];
      for (size_t j = b; j < e; ++j) {
        src[j] = dst[j];
      }
    }
  }

  #ifdef _OPENMP
  #pragma omp parallel for schedule(dynamic)
  #endif
  for (size_t i = 1; i < 1 << 16; ++i) {
    if ((i & 0xFF) != 0 && cnt[i] - cnt[i - 1] >= 1) {
      Recurse(bgn + cnt[i - 1], bgn + cnt[i], depth + 2, !flip);
    }
  }
}

/**
 * Sort an array of strings.
 *
 * This function automatically initialize an instance of
 * |ParallelStringRadixSort| and perform sorting.
 *
 * If you perform sorting more than once, you can avoid the cost of
 * initialization using |ParallelStringRadixSort| class by yourself.
 *
 * @param strings the array to be sorted
 * @param num_elems the number of the elements in the given array
 */
template<typename StringType>
void Sort(StringType *strings, size_t num_elems) {
  ParallelStringRadixSort<StringType> psrs;
  psrs.Init(num_elems);
  psrs.Sort(strings, num_elems);
}

template<typename StringType, size_t kNumElems>
void Sort(StringType (&strings)[kNumElems]) {
  Sort(strings, kNumElems);
}
}  // namespace parallel_string_radix_sort

#undef PSRS_CHECK

#endif  // PARALLEL_STRING_RADIX_SORT_H_
