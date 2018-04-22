cmake_minimum_required (VERSION 3.6)
project (foc_libraries)

# C99
set(CMAKE_C_STANDARD 99)

# C++11
set(CMAKE_CXX_STANDARD 11)

include(prelude.cmake)

include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}")

# Google Test
set(googletest_SOURCE_DIR googletest/googletest)
set(googletest_LIBRARIES gtest gtest_main)
add_subdirectory(${googletest_SOURCE_DIR})
include_directories(SYSTEM ${googletest_SOURCE_DIR}/include ${googletest_SOURCE_DIR})
enable_testing()

# small_vector_test
add_executable(small_vector_test small_vector_test.cpp)
add_test(SmallVectorTest small_vector_test)

# hash_array_mapped_trie_test
add_executable(hash_array_mapped_trie_test hash_array_mapped_trie_test.cpp)
add_test(HashArrayMappedTrieTest hash_array_mapped_trie_test)

# sqlkit_test
add_executable(sqlkit_test sqlkit_test.cpp sqlite3.c)
add_test(SQLKitTest sqlkit_test)
