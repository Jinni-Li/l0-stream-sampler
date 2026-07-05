cmake_minimum_required(VERSION 3.16)
project(l0_sampler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(
    l0_sampler
    src/main.cpp
    src/CsvReader.cpp
    src/ExactSupportTracker.cpp
    src/L0sampler.cpp
    src/OneSparseSketch.cpp
    src/SSparseSketch.cpp
    src/HashBasedL0Sampler.cpp
    src/PairwiseHash.cpp
)

add_executable(
    test_one_sparse
    tests/test_one_sparse.cpp
    src/OneSparseSketch.cpp
)

add_executable(
    test_hash_based
    tests/test_hash_based.cpp
    src/HashBasedL0Sampler.cpp
    src/OneSparseSketch.cpp
    src/SSparseSketch.cpp
    src/PairwiseHash.cpp
)

add_executable(
    test_s_sparse
    tests/test_s_sparse.cpp
    src/SSparseSketch.cpp
    src/OneSparseSketch.cpp
    src/PairwiseHash.cpp
)

add_executable(
    test_pairwise_hash
    src/PairwiseHash.cpp
    tests/test_pairwise_hash.cpp
)