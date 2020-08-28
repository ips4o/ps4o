/******************************************************************************
 * include/ps4o/sequential.hpp
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

#include <memory>
#include <utility>

#include "ps4o_fwd.hpp"
#include "base_case.hpp"
#include "memory.hpp"
#include "partitioning.hpp"

namespace ps4o {
namespace detail {

/**
 * Sequentially partition input.
 */
template<class Cfg>
template <class InputIterator, class OutputIterator>
void Sorter<Cfg>::sequential(const InputIterator begin, const OutputIterator begin_tmp,
                             unsigned char* oracle, const Task& task,
                             PrivateQueue<Task>& queue, bool in_input_array) {

  const auto n = task.end - task.begin;
  PS4O_IS_NOT(n <= 2 * Cfg::kBaseCaseSize);

  diff_t bucket_start_save[Cfg::kMaxBuckets + 1];
  const bool last_level = n <= Cfg::kSingleLevelThreshold;
    
  // Do the partitioning
  const auto res = partition<false>(begin + task.begin, begin + task.end, begin_tmp + task.begin,
                                    oracle + task.begin, bucket_start_save,
                                    0, 1, last_level, task.in_input_array);
  const int num_buckets = std::get<0>(res);
  const bool equal_buckets = std::get<1>(res);

  // Final base case is executed in cleanup step, so we're done here
  if (last_level) {
    return;
  }
    
  if (equal_buckets) {
    const auto start = bucket_start_save[num_buckets - 1];
    const auto stop = bucket_start_save[num_buckets];
    if (stop - start > 2 * Cfg::kBaseCaseSize) {
      queue.emplace(task.begin + start, task.begin + stop, !task.in_input_array);
    }
  }
      
  for (int i = num_buckets - 1 - equal_buckets; i >= 0; i -= 1 + equal_buckets) {

    const auto start = bucket_start_save[i];
    const auto stop = bucket_start_save[i + 1];
    if (stop - start > 2 * Cfg::kBaseCaseSize) {
      queue.emplace(task.begin + start, task.begin + stop, !task.in_input_array);
    }
  }

}

/**
 * Recursive entry point for sequential algorithm.
 */
template<class Cfg>
template <class InputIterator, class OutputIterator>
void Sorter<Cfg>::sequential(const InputIterator begin, const InputIterator end,
                             const OutputIterator begin_tmp, unsigned char* oracle,
                             bool in_input_array) {

  // Check for base case
  const auto n = end - begin;
  if (n <= 2 * Cfg::kBaseCaseSize) {
    detail::baseCaseSort(begin, end, local_.classifier.getComparator());

    if (!in_input_array) {
      std::move(begin, end, begin_tmp);
      for (auto ptr = begin; ptr != end; ++ptr) {
        ptr->~value_type();
      }
    }
    return;
  }

  sequential_rec(begin, end, begin_tmp, oracle, in_input_array);
}

/**
 * Recursive entry point for sequential algorithm.
 */
template<class Cfg>
template <class InputIterator, class OutputIterator>
void Sorter<Cfg>::sequential_rec(const InputIterator begin, const InputIterator end,
                             const OutputIterator begin_tmp, unsigned char* oracle,
                             bool in_input_array) {

  const auto n = end - begin;
  PS4O_IS_NOT(n <= 2 * Cfg::kBaseCaseSize);
  
  diff_t bucket_start_save[Cfg::kMaxBuckets + 1];
  const bool last_level = n <= Cfg::kSingleLevelThreshold;
    
  // Do the partitioning
  const auto res = partition<false>(begin, end, begin_tmp, oracle, bucket_start_save,
                                    0, 1, last_level, in_input_array);
  const int num_buckets = std::get<0>(res);
  const bool equal_buckets = std::get<1>(res);

  // Final base case is executed in cleanup step, so we're done here
  if (last_level) {
    return;
  }
    
  if (equal_buckets) {
    for (int i = 0; i < num_buckets; i += 2) {

      const auto start = bucket_start_save[i];
      const auto stop = bucket_start_save[i + 1];
      if (stop - start > 2 * Cfg::kBaseCaseSize) {
        sequential(begin_tmp + start, begin_tmp + stop,
                   begin + start, oracle + start, !in_input_array);
      }
        
    }
        
    const auto start = bucket_start_save[num_buckets - 1];
    const auto stop = bucket_start_save[num_buckets];
    if (stop - start > 2 * Cfg::kBaseCaseSize) {
      sequential(begin_tmp + start, begin_tmp + stop,
                 begin + start, oracle + start, !in_input_array);
    }
        
  } else {
        
    for (int i = 0; i < num_buckets; ++i) {
        
      const auto start = bucket_start_save[i];
      const auto stop = bucket_start_save[i + 1];
      if (stop - start > 2 * Cfg::kBaseCaseSize) {
        sequential(begin_tmp + start, begin_tmp + stop,
                   begin + start, oracle + start, !in_input_array);
      }
          
    }
  }
}

}  // namespace detail

/**
 * Reusable sequential sorter.
 */
template <class Cfg>
class SequentialSorter {
  using Sorter = detail::Sorter<Cfg>;
  using iterator = typename Cfg::iterator;

public:
  explicit SequentialSorter(bool check_sorted, typename Cfg::less comp)
    : check_sorted(check_sorted)
    , local_ptr_(Cfg::kDataAlignment, std::move(comp)) {}

  void operator()(iterator begin, iterator end) {
    if (check_sorted) {
      const bool sorted = detail::sortedCaseSort(begin, end,
                                                 local_ptr_.get().classifier.getComparator());
      if (sorted) return;
    }

    const auto n = end - begin;

    // Check for base case
    if (n <= 2 * Cfg::kBaseCaseSize) {
      detail::baseCaseSort(begin, end, local_ptr_.get().classifier.getComparator());
      return;
    }

    ps4o::detail::AlignedRawPtr tmp(Cfg::kDataAlignment, sizeof(typename Cfg::value_type) * n);
    ps4o::detail::AlignedRawPtr oracle(Cfg::kDataAlignment, sizeof(unsigned char) * n);
    Sorter(local_ptr_.get()).sequential(std::move(begin), std::move(end),
                                        static_cast<typename Cfg::value_type*>(static_cast<void*>(tmp.get())),
                                        static_cast<unsigned char*>(static_cast<void*>(oracle.get())),
                                        true);
  }

private:
  const bool check_sorted;
  detail::AlignedPtr<typename Sorter::LocalData> local_ptr_;
};

}  // namespace ps4o
