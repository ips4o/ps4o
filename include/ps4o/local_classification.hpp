/******************************************************************************
 * include/ps4o/local_classification.hpp
 *
 * Parallel Super Scalar Samplesort (PS⁴o)
 *
 ******************************************************************************
 * BSD 2-Clause License
 *
 * Copyright © 2017, Michael Axtmann <michael.axtmann@gmail.com>
 * Copyright © 2017, Daniel Ferizovic <daniel.ferizovic@student.kit.edu>
 * Copyright © 2017, Sascha Witt <sascha.witt@kit.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#pragma once

#include "ps4o_fwd.hpp"
#include "classifier.hpp"
#include "memory.hpp"

namespace ps4o {
namespace detail {

/**
 * Local classification phase.
 */
template <class Cfg>
template <bool kEqualBuckets, class InputIterator> __attribute__((flatten))
void Sorter<Cfg>::classifyLocally(const InputIterator begin,
                                  const InputIterator end,
                                  unsigned char* oracle,
                                  Classifier* classifier) {

  for (bucket_type bucket = 0; bucket != num_buckets_; ++bucket) {
    local_.bucket_size[bucket] = 0;
  }

  auto bs = local_.bucket_size;

  // Do the classification
  classifier->template classify<kEqualBuckets>(begin, end, [&](bucket_type bucket) {
      *oracle = bucket;
      ++oracle;
      ++bs[bucket];
    });
}

/**
 * Local classification in the sequential case.
 */
template <class Cfg>
template <class InputIterator>
void Sorter<Cfg>::sequentialClassification(const InputIterator begin,
                                           const InputIterator end,
                                           unsigned char* oracle,
                                           diff_t* bucket_start_save,
                                           Classifier* classifier,
                                           const bool use_equal_buckets) {
  if (use_equal_buckets) {
    classifyLocally<true>(begin, end, oracle, classifier);
  } else {
    classifyLocally<false>(begin, end, oracle, classifier);
  }

    // Find bucket boundaries
    local_.bucket_start[0] = 0;

    // Exclusive scan
    std::partial_sum(local_.bucket_size, local_.bucket_size + num_buckets_,
                     local_.bucket_start + 1, std::plus<>{});
    std::copy(local_.bucket_start, local_.bucket_start + num_buckets_ + 1,
              bucket_start_save);
}

/**
 * Local classification in the parallel case.
 */
template <class Cfg>
template <class InputIterator>
void Sorter<Cfg>::parallelClassification(const InputIterator begin,
                                         const InputIterator end,
                                         unsigned char* oracle,
                                         diff_t* bucket_start_save,
                                         Classifier* classifier,
                                         int my_id,
                                         int num_threads,
                                         const bool use_equal_buckets) {
    // Do classification
    if (begin < end) {
      if (use_equal_buckets) {
        // pass bucket size of this specific thread.
        classifyLocally<true>(begin, end, oracle, classifier);
      } else {
        classifyLocally<false>(begin, end, oracle, classifier);
      }

    }

    std::copy(local_.bucket_size, local_.bucket_size + num_buckets_,
              shared_->bucket_size.begin() + Cfg::kMaxBuckets * my_id);

    shared_->sync.barrier();

    if (my_id == 0) {
        
      diff_t off = 0;
      bucket_start_save[0] = 0;
      for (bucket_type bucket = 0; bucket != num_buckets_; ++bucket) {
        for (int thread = 0; thread != num_threads; ++thread) {
          shared_->local[thread]->bucket_start[bucket] = off;
          off += shared_->bucket_size[thread * Cfg::kMaxBuckets + bucket];
        }
        bucket_start_save[bucket + 1] = off;
      }
    }

    shared_->sync.barrier();
}


}  // namespace detail
}  // namespace ps4o
