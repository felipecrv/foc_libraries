#include <queue>
#include <vector>

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#define GTEST
#define HAMT_IMPLEMENTATION
#include "hash_array_mapped_trie.h"
#include "hash_array_mapped_trie_test_helpers.h"

using foc::HashArrayMappedTrie;

using HAMT = HashArrayMappedTrie<int64_t, int64_t>;

TEST_CASE("AllocationSizeCalculationTest", "[HAMT]") {
  // Make sure the required value overrides the alloc
  // size estimation for the generation and level.
  for (size_t expected_hamt_size = 1; expected_hamt_size < 24; expected_hamt_size *= 2) {
    for (uint32_t level = 0; level < 8; level++) {
      for (uint32_t required = 1; required <= 32; required++) {
        REQUIRE(foc::detail::hamt_trie_allocation_size(required, expected_hamt_size, level) >=
                required);
        REQUIRE(foc::detail::hamt_trie_allocation_size(required, expected_hamt_size, level) >=
                required);
      }
    }
  }
}

TEST_CASE("BitmatpTrieInitialization", "[HAMT]") {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;

  trie.allocate(allocator, 0);
  REQUIRE(trie.size() == 0);
  REQUIRE(trie.capacity() == 0);

  trie.allocate(allocator, 16);
  REQUIRE(trie.size() == 0);
  REQUIRE(trie.capacity() == 16);

  // Make sure the base array was allocated and all logical positions point to the first
  // position of the array.
  for (uint32_t i = 0; i < 32; i++) {
    REQUIRE(trie.physicalIndex(i) == 0);
    REQUIRE(&trie.physicalGet(0) == &trie.logicalGet(i));
    REQUIRE(!trie.logicalPositionTaken(i));
  }
}

TEST_CASE("LogicalZeroToPhysicalZeroIndexTranslationTest", "[HAMT]") {
  HAMT::BitmapTrie trie;

  // For any bitmap, the logical index 0 will map to physical index 0.
  // (we test all bitmaps that contain a single bit)
  for (int i = 0; i < 32; i++) {
    trie.bitmap() = 0x1 << i;
    REQUIRE(trie.physicalIndex(0) == 0);
  }
}

TEST_CASE("FirstEntryTest", "[HAMT]") {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;

  std::pair<int64_t, int64_t> two(2, 2);
  std::pair<int64_t, int64_t> three(3, 3);

  // Insert two entrie into a trie and check the first
  trie.allocate(allocator, 4);
  trie.insertEntry(allocator, 2, two, nullptr, 4, 0);
  REQUIRE(trie.logicalPositionTaken(2));
  trie.insertEntry(allocator, 3, three, nullptr, 4, 0);
  REQUIRE(trie.logicalPositionTaken(3));

  const HAMT::Node *node = trie.firstEntryNodeRecursively();
  REQUIRE(node->asEntry().second == 2);
}

TEST_CASE("FirstEntryRecursivelyTest", "[HAMT]") {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;

  std::pair<int64_t, int64_t> two(2, 2);
  std::pair<int64_t, int64_t> three(3, 3);

  // Isert an entry and a trie with an entry to cause recursion
  trie.allocate(allocator, 4);
  trie.insertEntry(allocator, 3, three, nullptr, 4, 0);
  REQUIRE(trie.logicalPositionTaken(3));
  HAMT::Node *child = trie.insertTrie(allocator, nullptr, 0, 1);
  REQUIRE(trie.logicalPositionTaken(0));
  child->asTrie().insertEntry(allocator, 0, two, nullptr, 1, 1);
  REQUIRE(child->asTrie().logicalPositionTaken(0));

  const HAMT::Node *node = trie.firstEntryNodeRecursively();
  REQUIRE(node->asEntry().second == 2);
}

TEST_CASE("LogicalToPhysicalIndexTranslationTest", "[HAMT]") {
  HAMT::BitmapTrie trie;

  trie.bitmap() = 1;  // 0001
  REQUIRE(trie.physicalIndex(1) == 1);
  REQUIRE(trie.physicalIndex(2) == 1);
  REQUIRE(trie.physicalIndex(3) == 1);
  REQUIRE(trie.physicalIndex(4) == 1);
  REQUIRE(trie.physicalIndex(5) == 1);
  REQUIRE(trie.physicalIndex(31) == 1);
  trie.bitmap() = 2;  // 0010
  REQUIRE(trie.physicalIndex(1) == 0);
  REQUIRE(trie.physicalIndex(2) == 1);
  REQUIRE(trie.physicalIndex(3) == 1);
  REQUIRE(trie.physicalIndex(4) == 1);
  REQUIRE(trie.physicalIndex(5) == 1);
  REQUIRE(trie.physicalIndex(31) == 1);
  trie.bitmap() = 3;  // 0011
  REQUIRE(trie.physicalIndex(1) == 1);
  REQUIRE(trie.physicalIndex(2) == 2);
  REQUIRE(trie.physicalIndex(3) == 2);
  REQUIRE(trie.physicalIndex(4) == 2);
  REQUIRE(trie.physicalIndex(5) == 2);
  REQUIRE(trie.physicalIndex(31) == 2);
  trie.bitmap() = 4;  // 0100
  REQUIRE(trie.physicalIndex(1) == 0);
  REQUIRE(trie.physicalIndex(2) == 0);
  REQUIRE(trie.physicalIndex(3) == 1);
  REQUIRE(trie.physicalIndex(4) == 1);
  REQUIRE(trie.physicalIndex(5) == 1);
  REQUIRE(trie.physicalIndex(31) == 1);
  trie.bitmap() = 5;  // 0101
  REQUIRE(trie.physicalIndex(1) == 1);
  REQUIRE(trie.physicalIndex(2) == 1);
  REQUIRE(trie.physicalIndex(3) == 2);
  REQUIRE(trie.physicalIndex(4) == 2);
  REQUIRE(trie.physicalIndex(5) == 2);
  REQUIRE(trie.physicalIndex(31) == 2);
  trie.bitmap() = 6;  // 0110
  REQUIRE(trie.physicalIndex(1) == 0);
  REQUIRE(trie.physicalIndex(2) == 1);
  REQUIRE(trie.physicalIndex(3) == 2);
  REQUIRE(trie.physicalIndex(4) == 2);
  REQUIRE(trie.physicalIndex(5) == 2);
  REQUIRE(trie.physicalIndex(31) == 2);
  trie.bitmap() = 7;  // 0111
  REQUIRE(trie.physicalIndex(1) == 1);
  REQUIRE(trie.physicalIndex(2) == 2);
  REQUIRE(trie.physicalIndex(3) == 3);
  REQUIRE(trie.physicalIndex(4) == 3);
  REQUIRE(trie.physicalIndex(5) == 3);
  REQUIRE(trie.physicalIndex(31) == 3);
}

TEST_CASE("BitmapTrieInsertTest", "[HAMT]") {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;
  trie.allocate(allocator, 1);

  auto e = std::make_pair(40LL, 4LL);
  trie.insertEntry(allocator, 4, e, nullptr, 2, 0);
  REQUIRE(trie.bitmap() == 16);  // 010000
  REQUIRE(trie.size() == 1);
  REQUIRE(trie.physicalGet(0).asEntry().first == 40);
  REQUIRE(trie.physicalGet(0).asEntry().second == 4);

  e = std::make_pair(20L, 2L);
  trie.insertEntry(allocator, 2, e, nullptr, 2, 0);
  REQUIRE(trie.bitmap() == 20);  // 010100
  REQUIRE(trie.size() == 2);
  REQUIRE(trie.physicalGet(0).asEntry().first == 20);
  REQUIRE(trie.physicalGet(0).asEntry().second == 2);
  REQUIRE(trie.physicalGet(1).asEntry().first == 40);
  REQUIRE(trie.physicalGet(1).asEntry().second == 4);

  e = std::make_pair(30L, 3L);
  trie.insertEntry(allocator, 3, e, nullptr, 2, 0);
  REQUIRE(trie.bitmap() == 28);  // 011100
  REQUIRE(trie.size() == 3);
  REQUIRE(trie.physicalGet(0).asEntry().first == 20);
  REQUIRE(trie.physicalGet(0).asEntry().second == 2);
  REQUIRE(trie.physicalGet(1).asEntry().first == 30);
  REQUIRE(trie.physicalGet(1).asEntry().second == 3);
  REQUIRE(trie.physicalGet(2).asEntry().first == 40);
  REQUIRE(trie.physicalGet(2).asEntry().second == 4);

  e = std::make_pair(0LL, 0LL);
  trie.insertEntry(allocator, 0, e, nullptr, 2, 0);
  REQUIRE(trie.bitmap() == 29);  // 011101
  REQUIRE(trie.size() == 4);
  REQUIRE(trie.physicalGet(0).asEntry().first == 0);
  REQUIRE(trie.physicalGet(0).asEntry().second == 0);
  REQUIRE(trie.physicalGet(1).asEntry().first == 20);
  REQUIRE(trie.physicalGet(1).asEntry().second == 2);
  REQUIRE(trie.physicalGet(2).asEntry().first == 30);
  REQUIRE(trie.physicalGet(2).asEntry().second == 3);
  REQUIRE(trie.physicalGet(3).asEntry().first == 40);
  REQUIRE(trie.physicalGet(3).asEntry().second == 4);

  e = std::make_pair(50LL, 5LL);
  trie.insertEntry(allocator, 5, e, nullptr, 2, 0);
  REQUIRE(trie.bitmap() == 61);  // 111101
  REQUIRE(trie.size() == 5);
  REQUIRE(trie.physicalGet(0).asEntry().first == 0);
  REQUIRE(trie.physicalGet(0).asEntry().second == 0);
  REQUIRE(trie.physicalGet(1).asEntry().first == 20);
  REQUIRE(trie.physicalGet(1).asEntry().second == 2);
  REQUIRE(trie.physicalGet(2).asEntry().first == 30);
  REQUIRE(trie.physicalGet(2).asEntry().second == 3);
  REQUIRE(trie.physicalGet(3).asEntry().first == 40);
  REQUIRE(trie.physicalGet(3).asEntry().second == 4);
  REQUIRE(trie.physicalGet(4).asEntry().first == 50);
  REQUIRE(trie.physicalGet(4).asEntry().second == 5);

  e = std::make_pair(10LL, 1LL);
  trie.insertEntry(allocator, 1, e, nullptr, 2, 0);
  REQUIRE(trie.bitmap() == 63);  // 111111
  REQUIRE(trie.size() == 6);
  REQUIRE(trie.physicalGet(0).asEntry().first == 0);
  REQUIRE(trie.physicalGet(0).asEntry().second == 0);
  REQUIRE(trie.physicalGet(1).asEntry().first == 10);
  REQUIRE(trie.physicalGet(1).asEntry().second == 1);
  REQUIRE(trie.physicalGet(2).asEntry().first == 20);
  REQUIRE(trie.physicalGet(2).asEntry().second == 2);
  REQUIRE(trie.physicalGet(3).asEntry().first == 30);
  REQUIRE(trie.physicalGet(3).asEntry().second == 3);
  REQUIRE(trie.physicalGet(4).asEntry().first == 40);
  REQUIRE(trie.physicalGet(4).asEntry().second == 4);
  REQUIRE(trie.physicalGet(5).asEntry().first == 50);
  REQUIRE(trie.physicalGet(5).asEntry().second == 5);

  e = std::make_pair(310LL, 31L);
  trie.insertEntry(allocator, 31, e, nullptr, 2, 0);
  REQUIRE(trie.bitmap() == (63 | (0x1 << 31)));
  REQUIRE(trie.size() == 7);
  REQUIRE(trie.physicalGet(6).asEntry().first == 310);
  REQUIRE(trie.physicalGet(6).asEntry().second == 31);
}

TEST_CASE("BitmapTrieInsertTrieTest", "[HAMT]") {
  HAMT::BitmapTrie trie;
  HAMT::Node parent(nullptr);
  MallocAllocator allocator;
  std::pair<int64_t, int64_t> entry(2, 4);

  const uint32_t capacity = 2;
  trie.allocate(allocator, capacity);
  REQUIRE(trie.size() == 0);

  // Insert an entry and a trie to the trie
  trie.insertEntry(allocator, 0, entry, &parent, 0, 0);
  REQUIRE(trie.size() == 1);
  trie.insertTrie(allocator, &parent, 1, capacity);
  REQUIRE(trie.size() == 2);

  // Retrieve the inserted entry
  HAMT::Node &inserted_entry_node = trie.logicalGet(0);
  REQUIRE(inserted_entry_node.isEntry());
  auto &inserted_entry = inserted_entry_node.asEntry();
  REQUIRE(inserted_entry == entry);

  // Retrieve the inserted trie
  HAMT::Node &child_trie_node = trie.logicalGet(1);
  REQUIRE(!child_trie_node.isEntry());
  REQUIRE(child_trie_node.isTrie());

  // Insert another trie into the child trie
  HAMT::BitmapTrie &child_trie = child_trie_node.asTrie();
  REQUIRE(child_trie.size() == 0);
  child_trie.insertTrie(allocator, &child_trie_node, 0, 2);
  REQUIRE(child_trie.size() == 1);

  // Retrieve the inserted trie
  HAMT::Node &grand_child_trie_node = child_trie.logicalGet(0);
  REQUIRE(grand_child_trie_node.isTrie());
  REQUIRE(grand_child_trie_node.parent() == &child_trie_node);
  REQUIRE(child_trie_node.parent() == &parent);
}

TEST_CASE("NodeInitializationAsBitmapTrieTest", "[HAMT]") {
  HAMT::Node parent(nullptr);
  REQUIRE(parent.parent() == nullptr);

  HAMT::Node node(&parent);
  REQUIRE(node.isTrie());
  REQUIRE(!node.isEntry());
  REQUIRE(node.parent() == &parent);

  MallocAllocator allocator;
  node.BitmapTrie(allocator, &parent, 2);
  REQUIRE(node.isTrie());
  REQUIRE(!node.isEntry());
  REQUIRE(node.parent() == &parent);
  auto &trie = node.asTrie();
  REQUIRE(trie.capacity() == 2);
  REQUIRE(trie.size() == 0);

  HAMT::Node a_node(nullptr);
  a_node = std::move(node);
  REQUIRE(a_node.parent() == &parent);
  auto &a_trie = node.asTrie();
  REQUIRE(&trie.physicalGet(0) == &a_trie.physicalGet(0));
  REQUIRE(a_trie.capacity() == 2);
  REQUIRE(a_trie.size() == 0);

  a_trie.deallocate(allocator);
}

TEST_CASE("NodeInitializationAsEntryTest", "[HAMT]") {
  HAMT::Node parent(nullptr);

  std::pair<int64_t, int64_t> entry = std::make_pair(2, 4);
  HAMT::Node node(std::move(entry), &parent);
  REQUIRE(node.isEntry());
  REQUIRE(!node.isTrie());
  REQUIRE(node.parent() == &parent);

  HAMT::Node a_node(nullptr);
  a_node = std::move(entry);
  REQUIRE(node.isEntry());
  REQUIRE(!node.isTrie());
}

TEST_CASE("ParentTest", "[HAMT]") {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t>;
  parent_test<HAMT>(2048);
}

TEST_CASE("ParentTestWithBadHashFunction", "[HAMT]") {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t, BadHashFunction>;
  parent_test<HAMT>(64);
}

TEST_CASE("ParentTestWithIdentityFunction", "[HAMT]") {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t, IdentityFunction>;
  parent_test<HAMT>(2048);
}

TEST_CASE("ParentTestConstantFunction", "[HAMT]") {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t, ConstantFunction>;
  loose_parent_test<HAMT>(64);
}

TEST_CASE("TopLevelInsertTest", "[HAMT]") {
  /* for (int max = 1; max <= 1048576; max *= 2) { */
  for (int64_t max = 65536; max <= 65536; max *= 2) {
    HAMT hamt;
    for (int64_t i = 1; i <= max; i++) {
      /* printf("%d:\n", i * 10); */
      insertKeyAndValue(hamt, i * 10, i);
      if (__builtin_popcount(i) == 1) {
        hamt.print();
      }
    }
    /* print_hamt(hamt); */
    /* print_stats(hamt); */

    /* int64_t count_not_found = 0; */
    int64_t last_not_found = -1;
    for (int64_t i = 1; i <= max; i++) {
      auto found = hamt.find(i * 10);
      REQUIRE(found != nullptr);
      /*
      if (found == nullptr) {
        count_not_found++;
        last_not_found = i * 10;
        continue;
      }
      */
      REQUIRE(*found == i);
    }
    /* printf("count_not_found %d\n", count_not_found); */
    REQUIRE(last_not_found == -1);
  }
}

TEST_CASE("ConstantHashFunctionTest", "[HAMT]") {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t, ConstantFunction>;
  HAMT hamt;
  for (int64_t i = 0; i < 32; i++) {
    insertKeyAndValue(hamt, i, i);
  }
}

TEST_CASE("PhysicalIndexOfNodeInTrie", "[HAMT]") {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t, IdentityFunction>;
  HAMT hamt;
  hamt._seed = 0;
  HAMT::Node *root = &hamt._root;
  for (int64_t i = 31; i >= 0; i--) {
    insertKeyAndValue(hamt, i, i);
  }
  for (uint32_t i = 0; i < 32; i++) {
    REQUIRE(root->asTrie().physicalIndex(i) == i);
    HAMT::Node *logical_node = &root->asTrie().logicalGet(i);
    HAMT::Node *physical_node = &root->asTrie().physicalGet(i);
    REQUIRE(logical_node->asEntry().first == i);
    REQUIRE(logical_node == physical_node);
    REQUIRE(logical_node->parent() == root);
    REQUIRE(root->asTrie().physicalIndexOf(logical_node) == i);
  }
}

TEST_CASE("IteratorTest", "[HAMT]") {
  HAMT empty_hamt;
  const auto &const_empty_hamt = empty_hamt;
  REQUIRE(empty_hamt.size() == 0);

  HAMT non_empty_hamt;
  const auto &const_non_empty_hamt = non_empty_hamt;
  for (int64_t i = 0; i < 128; i++) {
    insertKeyAndValue(non_empty_hamt, i, i);
  }
  print_hamt(non_empty_hamt);
  check_parent_pointers(empty_hamt);
  check_parent_pointers(non_empty_hamt);

  // Check begin == end on empty HAMTs
  REQUIRE(empty_hamt.begin() == empty_hamt.end());
  REQUIRE(const_empty_hamt.begin() == const_empty_hamt.end());
  REQUIRE(const_empty_hamt.cbegin() == const_empty_hamt.cend());

  // Check begin != end on non-empty HAMTs
  REQUIRE(non_empty_hamt.begin() != non_empty_hamt.end());
  REQUIRE(const_non_empty_hamt.begin() != const_non_empty_hamt.end());
  REQUIRE(const_non_empty_hamt.cbegin() != const_non_empty_hamt.cend());

  // Check the value of begin()
  {
    HAMT::const_iterator it = const_non_empty_hamt.begin();
    REQUIRE(it->first == 77);
    REQUIRE(it->second == 77);
  }
  {
    HAMT::const_iterator it = const_non_empty_hamt.begin();
    REQUIRE(it->first == 77);
    REQUIRE(it->second == 77);
  }
  {
    HAMT::const_iterator it = const_non_empty_hamt.cbegin();
    REQUIRE(it->first == 77);
    REQUIRE(it->second == 77);
  }

  size_t count_items = 0;
  for (auto it = non_empty_hamt.begin(); it != non_empty_hamt.end(); it++) {
    count_items++;
    printf("%lld %lld\n", it->first, it->second);
  }
  REQUIRE(count_items == 128);
}
