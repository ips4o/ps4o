/******************************************************************************
 * include/ps4o/ps4o_fwd.hpp
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
#include <iterator>
#include <utility>

#include "task.hpp"
#include "scheduler.hpp"

namespace ps4o {
namespace detail {

template <class It, class Comp>
inline void baseCaseSort(It begin, It end, Comp&& comp);

inline constexpr unsigned long log2(unsigned long n);

template <class It, class RandomGen>
inline void selectSample(It begin, It end,
                         typename std::iterator_traits<It>::difference_type num_samples,
                         RandomGen&& gen);

template <class Cfg>
class Sorter {
 public:
    using iterator = typename Cfg::iterator;
    using diff_t = typename Cfg::difference_type;
    using value_type = typename Cfg::value_type;
  using SubThreadPool = typename Cfg::SubThreadPool;
  using bucket_type = typename Cfg::bucket_type;

    class Block;
    class Buffers;
    class BucketPointers;
    class Classifier;
    struct LocalData;
    struct SharedData;
  explicit Sorter(LocalData& local)
    : local_(local) {}

  template <class InputIterator, class OutputIterator>
  void sequential(const InputIterator begin, const InputIterator end,
                  const OutputIterator begin_tmp, unsigned char* oracle,
                  bool in_input_array);

  template <class InputIterator, class OutputIterator>
  void sequential_rec(const InputIterator begin, const InputIterator end,
                  const OutputIterator begin_tmp, unsigned char* oracle,
                  bool in_input_array);
  
#if defined(_REENTRANT)
  
  template<class InputIterator, class OutputIterator>
  void parallelSortSecondary(const InputIterator begin, const InputIterator end,
                             const OutputIterator begin_tmp, unsigned char* oracle,
                             int id, int num_threads,
			     std::vector<std::shared_ptr<SubThreadPool>>& tp_trash);
  
  template<class InputIterator, class OutputIterator>
  void parallelSortPrimary(const InputIterator begin, const InputIterator end,
                           const OutputIterator begin_tmp, unsigned char* oracle,
                           const int num_threads,
			   std::vector<std::shared_ptr<SubThreadPool>>& tp_trash);

  void setShared(SharedData* shared_);

  template<class InputIterator, class OutputIterator>
  void parallelPartitionSecondary(const InputIterator begin, const OutputIterator begin_tmp,
                                  unsigned char* oracle, diff_t task_begin, diff_t task_end,
                                  bool in_input_array, int id, int num_threads);
  
  template<class InputIterator, class OutputIterator>
  std::pair<std::vector<typename Cfg::difference_type>, bool> parallelPartitionPrimary(
    const InputIterator begin, const OutputIterator begin_tmp, unsigned char* oracle,
    diff_t task_begin, diff_t task_end, bool in_input_array, const int num_threads);
  
  template <class InputIterator, class OutputIterator>
  void sequential(const InputIterator begin, const OutputIterator begin_tmp,
                           unsigned char* oracle, const Task& task,
                           PrivateQueue<Task>& queue, bool in_input_array);

#endif

 private:
    LocalData& local_;
    int num_buckets_;
  SharedData* shared_;

    static inline int computeLogBuckets(diff_t n);

  template <class InputIterator, class OutputIterator>
  std::pair<int, bool> buildClassifier(const InputIterator begin,
                                       const InputIterator end,
                                       const OutputIterator begin_tmp,
                                       unsigned char* oracle,
                                       Classifier& classifier);

  template <bool kEqualBuckets, class InputIterator> __attribute__((flatten))
  void classifyLocally(const InputIterator begin,
                       const InputIterator end,
                       unsigned char* oracle,
                       Classifier* classifier);

/**
 * Local classification in the parallel case.
 */
template <class InputIterator>
void sequentialClassification(const InputIterator begin,
                              const InputIterator end,
                              unsigned char* oracle,
                              diff_t* bucket_start_save,
                              Classifier* classifier,
                              const bool use_equal_buckets);

  template <class InputIterator>
  void parallelClassification(const InputIterator begin,
                              const InputIterator end,
                              unsigned char* oracle,
                              diff_t* bucket_start_save,
                              Classifier* classifier,
                              int my_id,
                              int num_threads,
                              const bool use_equal_buckets);

  template <class InputIterator, class OutputIterator>
  void distributeElements(const InputIterator begin,
                          const InputIterator end,
                          const OutputIterator begin_tmp,
                          const unsigned char* oracle);

  template<class InputIterator, class OutputIterator>
  void sortBaseCases(const InputIterator begin, const InputIterator end,
                     const OutputIterator begin_tmp,
                     typename Cfg::bucket_type* bucket_start,
                     int my_first_bucket, int my_last_bucket,
                     bool use_equal_buckets, bool in_input_array);

  template <bool kIsParallel, class InputIterator, class OutputIterator>
  std::pair<int, bool> partition(const InputIterator begin, const InputIterator end,
                                 const OutputIterator begin_tmp,
                                 unsigned char* oracle, diff_t* bucket_start_save,
                                 const int my_id, const int num_threads,
				 bool last_level, bool in_input_array);


  template<class InputIterator, class OutputIterator>
  void processSmallTasks(const InputIterator begin, const OutputIterator begin_tmp,
                         unsigned char* oracle, int num_threads);

  void queueTasks(const diff_t stripe, const int id, const int task_num_threads,
                  const diff_t parent_task_size, const bool parent_in_input_array,
                  const diff_t offset, const diff_t* bucket_start, int num_buckets,
                  bool equal_buckets);

  template<class InputIterator, class OutputIterator>
  void processBigTasks(const InputIterator begin, const OutputIterator begin_tmp,
                       unsigned char* oracle, const diff_t stripe, const int id,
		       std::vector<std::shared_ptr<SubThreadPool>>& tp_trash);

  void processBigTasksSecondary(const int id);

  template<class InputIterator, class OutputIterator>
  void processBigTaskPrimary(const InputIterator begin, const OutputIterator begin_tmp,
                             unsigned char* oracle, const diff_t stripe, const int id,
			     std::vector<std::shared_ptr<SubThreadPool>>& tp_trash);

};

}  // namespace detail

template <class Cfg>
class SequentialSorter;

template <class Cfg>
class ParallelSorter;

template <class It, class Comp>
inline void sort(It begin, It end, Comp comp);

template <class It>
inline void sort(It begin, It end);

#if defined(_REENTRANT)
namespace parallel {

template <class It, class Comp>
inline void sort(It begin, It end, Comp comp);

template <class It>
inline void sort(It begin, It end);

}  // namespace parallel
#endif // _REENTRANT
}  // namespace ps4o
