cmake_minimum_required(VERSION 3.10)

project(ps4o)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/)
include(default_build_type)

SET(CMAKE_CXX_STANDARD 14)

add_library(ps4o INTERFACE)
target_include_directories(ps4o INTERFACE include/)

option(PS4O_OPTIMIZE_FOR_NATIVE "Build with -march=native" ON)
if (PS4O_OPTIMIZE_FOR_NATIVE)
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
  if(COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    message(STATUS "PS4O_OPTIMIZE_FOR_NATIVE: ${PS4O_OPTIMIZE_FOR_NATIVE}")
  else()
    message(SEND_ERROR "PS4O_OPTIMIZE_FOR_NATIVE: ${PS4O_OPTIMIZE_FOR_NATIVE}")
  endif()
endif()

option(PS4O_DISABLE_PARALLEL "Disable parallel code of Ps4o" OFF)
if (NOT PS4O_DISABLE_PARALLEL)

  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  find_package (Threads REQUIRED)
  target_link_libraries(ps4o INTERFACE Threads::Threads)

  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/extern/cmake-modules)
  find_package(TBB REQUIRED)
  target_link_libraries(ps4o INTERFACE TBB::tbb)

endif()
message(STATUS "Parallel support of PS4o disabled: ${PS4O_DISABLE_PARALLEL}")

option(PS4O_USE_OPENMP "Enable OpenMP threads" OFF)
if (PS4O_USE_OPENMP)
  if (NOT PS4O_DISABLE_PARALLEL)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
      target_link_libraries(ps4o INTERFACE OpenMP::OpenMP_CXX)
    endif()
  endif()
endif()

add_executable(ps4o_example src/example.cpp)
target_link_libraries(ps4o_example PRIVATE ps4o)
