/******************************************************************************
 * include/ps4o/partitioning.hpp
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

#include <atomic>
#include <tuple>
#include <utility>

#include "memory.hpp"
#include "utils.hpp"

#include "ps4o_fwd.hpp"
#include "sampling.hpp"
#include "local_classification.hpp"
#include "base_case.hpp"

namespace ps4o {
namespace detail {

/**
 * Block permutation phase.
 */
template <class Cfg>
template <class InputIterator, class OutputIterator>
void Sorter<Cfg>::distributeElements(const InputIterator begin,
                                     const InputIterator end,
                                     const OutputIterator begin_tmp,
                                     const unsigned char* oracle) {
  auto bs = local_.bucket_start;
  
  for (auto ptr = begin; ptr != end; ++ptr) {
    new (&begin_tmp[bs[*oracle]]) typename Cfg::value_type(std::move(*ptr));
    ptr->~value_type();

    ++bs[*oracle];
    ++oracle;
  }
}


/**
 * Main partitioning function.
 */
template <class Cfg>
template <bool kIsParallel, class InputIterator, class OutputIterator>
std::pair<int, bool> Sorter<Cfg>::partition(const InputIterator begin,
                                            const InputIterator end,
                                            const OutputIterator begin_tmp,
                                            unsigned char* oracle,
                                            diff_t* bucket_start_save,
                                            const int my_id,
                                            const int num_threads,
                                            bool last_level,
                                            const bool in_input_array) {

  const auto sort_and_move = [this, bucket_start_save,
                              begin, begin_tmp, last_level, in_input_array](int bucket) {

    const auto src_start = begin_tmp + bucket_start_save[bucket];
    const auto src_end = begin_tmp + bucket_start_save[bucket + 1];
    if (last_level || src_end - src_start <= 2 * Cfg::kBaseCaseSize) {
      detail::baseCaseSort(src_start, src_end, local_.classifier.getComparator());

      // Elements are not in the input array anymore as the result of
      // the partitioning phase is written into the temporal array.
      if (in_input_array) {
        const diff_t size = bucket_start_save[bucket + 1] - bucket_start_save[bucket];
        const auto target = begin + bucket_start_save[bucket];

        for (diff_t i = 0; i != size; ++i) {
          new (&target[i]) typename Cfg::value_type(std::move(src_start[i]));
          (src_start + i)->~value_type();
        }
      }
    }
  };

  const auto move = [this, bucket_start_save, begin, begin_tmp, in_input_array](int bucket) {
    // Elements are not in the input array anymore as the result of
    // the partitioning phase is written into the temporal array.
    if (in_input_array) {
      const diff_t size = bucket_start_save[bucket + 1] - bucket_start_save[bucket];
      const auto src = begin_tmp + bucket_start_save[bucket];
      const auto target = begin + bucket_start_save[bucket];

      for (diff_t i = 0; i != size; ++i) {
        new (&target[i]) typename Cfg::value_type(std::move(src[i]));
        (src + i)->~value_type();
      }
    }
  };

  const auto n = end - begin;
  
  // Sampling
  bool use_equal_buckets = false;
  {
    if (!kIsParallel) {
      std::tie(this->num_buckets_, use_equal_buckets) = buildClassifier(begin, end, begin_tmp, oracle, local_.classifier);
    } else {
      shared_->sync.single([&] {
          std::tie(this->num_buckets_, use_equal_buckets) = buildClassifier(begin, end, begin_tmp, oracle, shared_->classifier);
          shared_->num_buckets = this->num_buckets_;
          shared_->use_equal_buckets = use_equal_buckets;
        });
      this->num_buckets_ = shared_->num_buckets;
      use_equal_buckets = shared_->use_equal_buckets;
    }
  }
   
    // Compute stripe for each thread
    const diff_t elements_per_thread = (n + num_threads - 1) / num_threads;
    const diff_t my_begin_offset = std::min(my_id * elements_per_thread, n);
    const diff_t my_end_offset = std::min((my_id + 1) * elements_per_thread, n);
    const auto my_begin = begin + my_begin_offset;
    const auto my_end = begin + my_end_offset;
    const auto my_oracle = oracle + my_begin_offset;

    // Local Classification
    if (kIsParallel) {
      parallelClassification(my_begin, my_end, my_oracle, bucket_start_save,
                             &shared_->classifier, my_id, num_threads, use_equal_buckets);
    } else {
      sequentialClassification(my_begin, my_end, my_oracle, bucket_start_save,
                               &local_.classifier, use_equal_buckets);
    }

    // Block Permutation
    distributeElements(my_begin, my_end, begin_tmp, my_oracle);

    if (!kIsParallel) {
      
      if (use_equal_buckets) {
        for (int i = 0; i < num_buckets_ - 2; i += 2) {
          sort_and_move(i);
          move(i + 1);
        }

        sort_and_move(num_buckets_ - 2);
        sort_and_move(num_buckets_ - 1);
      
      } else {
        
        for (int i = 0; i < num_buckets_; ++i) {
          sort_and_move(i);
        }
      }
    } else {
      shared_->sync.barrier();

      if (!use_equal_buckets) {

        // Distribute buckets among threads
        const int num_buckets = num_buckets_;
        const int buckets_per_thread = (num_buckets + num_threads - 1) / num_threads;
        
        int my_first_bucket = my_id * buckets_per_thread;
        int my_last_bucket = (my_id + 1) * buckets_per_thread;
        
        my_first_bucket = num_buckets < my_first_bucket ? num_buckets : my_first_bucket;
        my_last_bucket = num_buckets < my_last_bucket ? num_buckets : my_last_bucket;
        
        for (int i = my_first_bucket; i < my_last_bucket; ++i) {
          sort_and_move(i);
        }

      } else {

        // Distribute buckets among threads
        int my_first_bucket = num_buckets_;
        while (my_begin_offset < bucket_start_save[my_first_bucket]) {
          --my_first_bucket;
        }
        int my_last_bucket = my_first_bucket;
        while (bucket_start_save[my_last_bucket] < my_end_offset) {
          ++ my_last_bucket;
        }

        // Process first bucket if equal bucket (might be shared with
        // other processes)
        if (my_first_bucket % 2 == 1 && my_first_bucket < num_buckets_ - 2) {
          
          const auto start_tmp = begin_tmp + my_begin_offset;
          // My bucket stripe may end before the bucket ends,
          // i.e., I process just one bucket.
          const auto stop_tmp = begin_tmp + std::min(my_end_offset,
                                                     bucket_start_save[my_first_bucket + 1]);
          
          std::move(start_tmp, stop_tmp, begin + my_begin_offset);
          
          for (auto ptr = start_tmp; ptr != stop_tmp; ++ptr) {
            ptr->~value_type();
          }

          ++my_first_bucket;

        } else {
          // Skip first bucket if it is not an equal bucket and I do not
          // process the first element of the bucket.
          if (my_begin_offset > bucket_start_save[my_first_bucket]) {
            ++my_first_bucket;
          }
        }

        // Process last bucket if equal bucket (might be shared with
        // other processes)
        if (my_first_bucket < my_last_bucket) {
          const auto bucket = my_last_bucket - 1;
          if (bucket % 2 == 1 && bucket < num_buckets_ - 2) {

            const auto start_tmp = begin_tmp + bucket_start_save[bucket];
            // My bucket stripe may end before the bucket ends,
            // i.e., I process just one bucket.
            const auto stop_tmp = begin_tmp + my_end_offset;
          
            std::move(start_tmp, stop_tmp, begin + bucket_start_save[bucket]);
          
            for (auto ptr = start_tmp; ptr != stop_tmp; ++ptr) {
              ptr->~value_type();
            }

            --my_last_bucket;

          }
        }

        // Process remaining buckets. Buckets are not shared.
        if (my_first_bucket < my_last_bucket) {
          // This block processes the remaining buckets assigned to this
          // process. Note that only full buckets are left.

          // First, we process the first bucket if its an equal bucket.
          // Second, we process the last bucket (if its not bucket
          // num_buckets_ - 2 or num_buckets_ - 1) if tis not an equal
          // bucket.  Only blocks of two buckets (non equal bucket
          // followed by an equal bucket) are left (plus bucket
          // num_buckets_ - 2 and num_buckets_ - 1 if assigned).
          // Finally, we process bucket blocks (and the buckets
          // num_buckets_ - 2 and num_buckets_ - 1 if assigned).
        
          // Process first bucket if its an equal bucket
          if (my_first_bucket % 2 == 1 && my_first_bucket < num_buckets_ - 2) {
            move(my_first_bucket);
            ++ my_first_bucket;
          }

          // Process last bucket if its not an equal bucket
          if (my_first_bucket < my_last_bucket
              && (my_last_bucket) % 2 == 0
              && my_first_bucket < num_buckets_ - 2) {
            sort_and_move(my_last_bucket - 1);
            --my_last_bucket;
          }
        
          const int end_eqb = std::min(my_last_bucket, num_buckets_ - 2);
          for (int i = my_first_bucket; i < end_eqb; i += 2) {
            sort_and_move(i);
            move(i + 1);
          }

          // Process bucket num_buckets_ - 2 and num_buckets_ - 1
          for (int i = end_eqb; i < my_last_bucket; ++i) {
            sort_and_move(i);
          }
        }
      }
      
      shared_->sync.barrier();

    }
    
    local_.reset();

    return {this->num_buckets_, use_equal_buckets};
}

}  // namespace detail
}  // namespace ps4o
