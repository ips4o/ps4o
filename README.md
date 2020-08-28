# Parallel Super Scalar Samplesort (PS⁴o)

This is the implementation of the algorithm PS⁴o presented in the publication [In-place Parallel Super Scalar Samplesort (IPS⁴o)](https://arxiv.org/abs/1705.02257) (todo update),
which contains an in-depth description of its inner workings, as well as an extensive experimental performance evaluation.
Here's the abstract:

> We present a sorting algorithm that works in-place, executes in parallel, is
> cache-efficient, avoids branch-mispredictions, and performs work O(n log n) for
> arbitrary inputs with high probability. The main algorithmic contributions are
> new ways to make distribution-based algorithms in-place: On the practical side,
> by using coarse-grained block-based permutations, and on the theoretical side,
> we show how to eliminate the recursion stack. Extensive experiments show that
> our algorithm IPS⁴o scales well on a variety of multi-core machines. We
> outperform our closest in-place competitor by a factor of up to 3. Even as
> a sequential algorithm, we are up to 1.5 times faster than the closest
> sequential competitor, BlockQuicksort.

PS⁴o is an reimplementation of Super Scalar Sample Sort described by Sanders and Winkel [[1](http://algo2.iti.kit.edu/sanders/papers/ssss.pdf)].
The implementation of IPS⁴o is also available on [github](https://github.com/ips4o/ips4o).
We recommend to use IPS⁴o instead of PS⁴o as IPS⁴o is not only in-place but also significantly faster than PS⁴o.

An initial version of IPS⁴o has been described in our [publication](https://drops.dagstuhl.de/opus/volltexte/2017/7854/pdf/LIPIcs-ESA-2017-9.pdf) on the 25th Annual European Symposium on Algorithms.

## Usage

```C++
#include "ps4o.hpp"

// sort sequentially
ps4o::sort(begin, end[, comparator]);

// sort in parallel (uses OpenMP if available, std::thread otherwise)
ps4o::parallel::sort(begin, end[, comparator]);
```

PS⁴o provides a CMake library for simple usage:

```CMake
add_subdirectory(<path-to-this-folder>)
target_link_libraries(<your-target> PRIVATE ps4o)
```

If you use the CMake example shown above, we automatically optimize PS⁴o for the native CPU (e.g., `-march=native`).
You can disable the CMake property `PS4O_OPTIMIZE_FOR_NATIVE` to avoid native optimization.

PS⁴o uses C++ threads if not specified otherwise.
If you prefer OpenMP threads, you need to enable OpenMP threads, e.g., enable the CMake property `IPS4O_USE_OPENMP` or add OpenMP to your target.
If you enable the CMake property `DISABLE_PS4O_PARALLEL`, most of the parallel code will not be compiled and no parallel libraries will be linked.
Otherwise, CMake automatically enables C++ threads (e.g., `-pthread`) and links against TBB.
Thus, you need the Thread Building Blocks (TBB) library to compile and execute the parallel version of PS⁴o.
We search TBB with `find_package(TBB REQUIRED)`.
If you want to execute IPS⁴o in parallel but your TBB library is not accessible via `find_package(TBB REQUIRED)`, you can still compile IPS⁴o with parallel support. 
Just enable the CMake property `DISABLE_IPS4O_PARALLEL`, enable C++ threads for your own target and link your own target against your TBB library.

If you do not set a CMake build type, we use the build type `Release` which disables debugging (e.g., `-DNDEBUG`) and enables optimizations (e.g., `-O3`).

Currently, the code does not compile on Windows.

## Licensing

PS⁴o is free software provided under the BSD 2-Clause License described in the [LICENSE file](LICENSE). If you use PS⁴o in an academic setting please cite the publication [In-place Parallel Super Scalar Samplesort (IPS⁴o)](https://arxiv.org/abs/1705.02257)  (todo update) using the BibTeX entry

(todo update)
```bibtex 
@InProceedings{axtmann2017,
  author =	{Michael Axtmann and
                Sascha Witt and
                Daniel Ferizovic and
                Peter Sanders},
  title =	{{In-Place Parallel Super Scalar Samplesort (IPSSSSo)}},
  booktitle =	{25th Annual European Symposium on Algorithms (ESA 2017)},
  pages =	{9:1--9:14},
  series =	{Leibniz International Proceedings in Informatics (LIPIcs)},
  year =	{2017},
  volume =	{87},
  publisher =	{Schloss Dagstuhl--Leibniz-Zentrum fuer Informatik},
  doi =		{10.4230/LIPIcs.ESA.2017.9},
}
```
