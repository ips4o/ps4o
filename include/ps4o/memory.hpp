/******************************************************************************
 * include/ps4o/memory.hpp
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

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>
#include <cstdlib>
#include <vector>

#include "ps4o_fwd.hpp"
#include "classifier.hpp"
#include "scheduler.hpp"
#include "task.hpp"
#include "config.hpp"

namespace ps4o {
namespace detail {

/**
 * Aligns a pointer.
 */
template <class T>
static T* alignPointer(T* ptr, std::size_t alignment) {
    uintptr_t v = reinterpret_cast<std::uintptr_t>(ptr);
    v = (v - 1 + alignment) & ~(alignment - 1);
    return reinterpret_cast<T*>(v);
}

/**
 * Constructs an object at the specified alignment.
 */
template <class T>
class AlignedPtr {
 public:
    AlignedPtr() {}

    template <class... Args>
    explicit AlignedPtr(std::size_t alignment, Args&&... args)
            : alloc_(new char[sizeof(T) + alignment])
            , value_(new (alignPointer(alloc_, alignment)) T(std::forward<Args>(args)...))
    {}

    AlignedPtr(const AlignedPtr&) = delete;
    AlignedPtr& operator=(const AlignedPtr&) = delete;

    AlignedPtr(AlignedPtr&& rhs) : alloc_(rhs.alloc_), value_(rhs.value_) {
        rhs.alloc_ = nullptr;
    }
    AlignedPtr& operator=(AlignedPtr&& rhs) {
        std::swap(alloc_, rhs.alloc_);
        std::swap(value_, rhs.value_);
        return *this;
    }

    ~AlignedPtr() {
        if (alloc_) {
            value_->~T();
            delete[] alloc_;
        }
    }

    T& get() {
        return *value_;
    }

 private:
    char* alloc_ = nullptr;
    T* value_;
};

/**
 * Provides aligned storage without constructing an object.
 */
template <>
class AlignedPtr<void> {
 public:
    AlignedPtr() {}

    template <class... Args>
    explicit AlignedPtr(std::size_t alignment, std::size_t size)
            : alloc_(new char[size + alignment])
            , value_(alignPointer(alloc_, alignment))
    {}

    AlignedPtr(const AlignedPtr&) = delete;
    AlignedPtr& operator=(const AlignedPtr&) = delete;

    AlignedPtr(AlignedPtr&& rhs) : alloc_(rhs.alloc_), value_(rhs.value_) {
        rhs.alloc_ = nullptr;
    }
    AlignedPtr& operator=(AlignedPtr&& rhs) {
        std::swap(alloc_, rhs.alloc_);
        std::swap(value_, rhs.value_);
        return *this;
    }

    ~AlignedPtr() {
        if (alloc_) {
            delete[] alloc_;
        }
    }

    char* get() {
        return value_;
    }

 private:
    char* alloc_ = nullptr;
    char* value_;
};

/**
 * Provides aligned storage without constructing an object.
 */
class AlignedRawPtr {
 public:
    AlignedRawPtr() {}

    explicit AlignedRawPtr(std::size_t alignment, std::size_t size)
      : alloc_((char*)malloc(size + alignment))
            , value_(alignPointer(alloc_, alignment))
    {}

    AlignedRawPtr(const AlignedRawPtr&) = delete;
    AlignedRawPtr& operator=(const AlignedRawPtr&) = delete;

    AlignedRawPtr(AlignedRawPtr&& rhs) : alloc_(rhs.alloc_), value_(rhs.value_) {
        rhs.alloc_ = nullptr;
    }
    AlignedRawPtr& operator=(AlignedRawPtr&& rhs) {
        std::swap(alloc_, rhs.alloc_);
        std::swap(value_, rhs.value_);
        return *this;
    }

    ~AlignedRawPtr() {
      free(alloc_);
    }

    char* get() {
        return value_;
    }

 private:
    char* alloc_ = nullptr;
    char* value_;
};

/**
 * Data local to each thread.
 */
template <class Cfg>
struct Sorter<Cfg>::LocalData {
    using diff_t = typename Cfg::difference_type;
    // Buffers
  diff_t bucket_start[Cfg::kMaxBuckets + 1];

    diff_t bucket_size[Cfg::kMaxBuckets];
    Classifier classifier;

  PrivateQueue<Task> seq_task_queue;
  
    // Random bit generator for sampling
    // LCG using constants by Knuth (for 64 bit) or Numerical Recipes (for 32 bit)
    std::linear_congruential_engine<std::uintptr_t,
                                    Cfg::kIs64Bit ? 6364136223846793005u : 1664525u,
                                    Cfg::kIs64Bit ? 1442695040888963407u : 1013904223u,
                                    0u> random_generator;

    LocalData(typename Cfg::less comp)
            : classifier(std::move(comp))
    {
        std::random_device rdev;
        std::ptrdiff_t seed = rdev();
        if (Cfg::kIs64Bit)
            seed = (seed << (Cfg::kIs64Bit * 32)) | rdev();
        random_generator.seed(seed);
        reset();
    }

    /**
     * Resets local data after partitioning is done.
     */
    void reset() {
        classifier.reset();
    }
};

/**
 * Data describing a parallel task and the corresponding threads.
 */
struct BigTask {

  BigTask() : has_task{false}
  {}
  // todo or Cfg::iterator???
  std::ptrdiff_t begin;
  std::ptrdiff_t end;
  // My thread id of this task.
  int task_thread_id;
  // Index of the thread owning the thread pool used by this task.
  int root_thread;
  // Indicates whether this is a task or not
  bool has_task;
  bool in_input_array;
};

/**
 * Data shared between all threads.
 */
template <class Cfg>
struct Sorter<Cfg>::SharedData {
  
    // Bucket information
  std::vector<diff_t> bucket_size;
  typename Cfg::difference_type bucket_start[Cfg::kMaxBuckets + 1];

    int num_buckets;
    bool use_equal_buckets;

    // Classifier for parallel partitioning
    Classifier classifier;

    // Synchronisation support
    typename Cfg::Sync sync;

    // Local thread data
    std::vector<LocalData*> local;

  // Thread pools for bigtasks. One entry for each thread.
  std::vector<std::shared_ptr<SubThreadPool>> thread_pools;

  // Bigtasks. One entry per thread.
  std::vector<BigTask> big_tasks;

  // Scheduler of small tasks.
  Scheduler<Task> scheduler;

    SharedData(typename Cfg::less comp, typename Cfg::Sync sync, std::size_t num_threads)
      : bucket_size(Cfg::kMaxBuckets * num_threads)
      ,classifier(std::move(comp))
      , sync(std::forward<typename Cfg::Sync>(sync))
      , local(num_threads, nullptr)
        , thread_pools(num_threads)
        , big_tasks(num_threads)
        , scheduler(num_threads)
    {
        reset();
    }

    /**
     * Resets shared data after partitioning is done.
     */
    void reset() {
        classifier.reset();
        scheduler.reset();
    }
};

}  // namespace detail
}  // namespace ps4o
