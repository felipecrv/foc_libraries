# small_vector_test
add_executable(small_vector_test small_vector_test.cpp)
add_test(SmallVectorTest small_vector_test)

# hash_array_mapped_trie_test
add_executable(hash_array_mapped_trie_test hash_array_mapped_trie_test.cpp)
add_test(HashArrayMappedTrieTest hash_array_mapped_trie_test)

# logging_test
add_executable(logging_test logging_test.cpp)
target_link_libraries(logging_test "-framework CoreFoundation")
