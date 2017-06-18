#include <queue>
#include <vector>

#include <gtest/gtest.h>

#define GTEST
#define HAMT_IMPLEMENTATION
#include "hash_array_mapped_trie.h"
#include "hash_array_mapped_trie_test_helpers.h"

using foc::HashArrayMappedTrie;

using HAMT = HashArrayMappedTrie<int64_t, int64_t>;

TEST(HashArrayMappedTrieTest, AllocationSizeCalculationTest) {
  // Make sure the required value overrides the alloc
  // size estimation for the generation and level.
  for (size_t expected_hamt_size = 1; expected_hamt_size < 24; expected_hamt_size *= 2) {
    for (uint32_t level = 0; level < 8; level++) {
      for (uint32_t required = 1; required <= 32; required++) {
        EXPECT_TRUE(foc::detail::hamt_trie_allocation_size(required, expected_hamt_size, level) >= required);
        EXPECT_TRUE(foc::detail::hamt_trie_allocation_size(required, expected_hamt_size, level) >= required);
      }
    }
  }
}

TEST(HashArrayMappedTrieTest, BitmatpTrieInitialization) {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;

  trie.allocate(allocator, 0);
  EXPECT_EQ(trie.size(), 0);
  EXPECT_EQ(trie.capacity(), 0);

  trie.allocate(allocator, 16);
  EXPECT_EQ(trie.size(), 0);
  EXPECT_EQ(trie.capacity(), 16);

  // Make sure the base array was allocated and all logical positions point to the first
  // position of the array.
  for (uint32_t i = 0; i < 32; i++) {
    EXPECT_EQ(trie.physicalIndex(i), 0);
    EXPECT_EQ(&trie.physicalGet(0), &trie.logicalGet(i));
    EXPECT_FALSE(trie.logicalPositionTaken(i));
  }
}

TEST(HashArrayMappedTrieTest, LogicalZeroToPhysicalZeroIndexTranslationTest) {
  HAMT::BitmapTrie trie;

  // For any bitmap, the logical index 0 will map to physical index 0.
  // (we test all bitmaps that contain a single bit)
  for (int i = 0; i < 32; i++) {
    trie.bitmap() = 0x1 << i;
    EXPECT_EQ(trie.physicalIndex(0), 0);
  }
}

TEST(HashArrayMappedTrieTest, FirstEntryTest) {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;

  std::pair<int64_t, int64_t> two(2, 2);
  std::pair<int64_t, int64_t> three(3, 3);

  // Insert two entrie into a trie and check the first
  trie.allocate(allocator, 4);
  trie.insertEntry(allocator, 2, two, nullptr, 4, 0);
  EXPECT_TRUE(trie.logicalPositionTaken(2));
  trie.insertEntry(allocator, 3, three, nullptr, 4, 0);
  EXPECT_TRUE(trie.logicalPositionTaken(3));

  const HAMT::Node *node = trie.firstEntryNodeRecursively();
  EXPECT_EQ(node->asEntry().second, 2);
}

TEST(HashArrayMappedTrieTest, FirstEntryRecursivelyTest) {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;

  std::pair<int64_t, int64_t> two(2, 2);
  std::pair<int64_t, int64_t> three(3, 3);

  // Isert an entry and a trie with an entry to cause recursion
  trie.allocate(allocator, 4);
  trie.insertEntry(allocator, 3, three, nullptr, 4, 0);
  EXPECT_TRUE(trie.logicalPositionTaken(3));
  HAMT::Node *child = trie.insertTrie(allocator, nullptr, 0, 1);
  EXPECT_TRUE(trie.logicalPositionTaken(0));
  child->asTrie().insertEntry(allocator, 0, two, nullptr, 1, 1);
  EXPECT_TRUE(child->asTrie().logicalPositionTaken(0));

  const HAMT::Node *node = trie.firstEntryNodeRecursively();
  EXPECT_EQ(node->asEntry().second, 2);
}

TEST(HashArrayMappedTrieTest, LogicalToPhysicalIndexTranslationTest) {
  HAMT::BitmapTrie trie;

  trie.bitmap() = 1; // 0001
  EXPECT_EQ(trie.physicalIndex(1), 1);
  EXPECT_EQ(trie.physicalIndex(2), 1);
  EXPECT_EQ(trie.physicalIndex(3), 1);
  EXPECT_EQ(trie.physicalIndex(4), 1);
  EXPECT_EQ(trie.physicalIndex(5), 1);
  EXPECT_EQ(trie.physicalIndex(31), 1);
  trie.bitmap() = 2; // 0010
  EXPECT_EQ(trie.physicalIndex(1), 0);
  EXPECT_EQ(trie.physicalIndex(2), 1);
  EXPECT_EQ(trie.physicalIndex(3), 1);
  EXPECT_EQ(trie.physicalIndex(4), 1);
  EXPECT_EQ(trie.physicalIndex(5), 1);
  EXPECT_EQ(trie.physicalIndex(31), 1);
  trie.bitmap() = 3; // 0011
  EXPECT_EQ(trie.physicalIndex(1), 1);
  EXPECT_EQ(trie.physicalIndex(2), 2);
  EXPECT_EQ(trie.physicalIndex(3), 2);
  EXPECT_EQ(trie.physicalIndex(4), 2);
  EXPECT_EQ(trie.physicalIndex(5), 2);
  EXPECT_EQ(trie.physicalIndex(31), 2);
  trie.bitmap() = 4; // 0100
  EXPECT_EQ(trie.physicalIndex(1), 0);
  EXPECT_EQ(trie.physicalIndex(2), 0);
  EXPECT_EQ(trie.physicalIndex(3), 1);
  EXPECT_EQ(trie.physicalIndex(4), 1);
  EXPECT_EQ(trie.physicalIndex(5), 1);
  EXPECT_EQ(trie.physicalIndex(31), 1);
  trie.bitmap() = 5; // 0101
  EXPECT_EQ(trie.physicalIndex(1), 1);
  EXPECT_EQ(trie.physicalIndex(2), 1);
  EXPECT_EQ(trie.physicalIndex(3), 2);
  EXPECT_EQ(trie.physicalIndex(4), 2);
  EXPECT_EQ(trie.physicalIndex(5), 2);
  EXPECT_EQ(trie.physicalIndex(31), 2);
  trie.bitmap() = 6; // 0110
  EXPECT_EQ(trie.physicalIndex(1), 0);
  EXPECT_EQ(trie.physicalIndex(2), 1);
  EXPECT_EQ(trie.physicalIndex(3), 2);
  EXPECT_EQ(trie.physicalIndex(4), 2);
  EXPECT_EQ(trie.physicalIndex(5), 2);
  EXPECT_EQ(trie.physicalIndex(31), 2);
  trie.bitmap() = 7; // 0111
  EXPECT_EQ(trie.physicalIndex(1), 1);
  EXPECT_EQ(trie.physicalIndex(2), 2);
  EXPECT_EQ(trie.physicalIndex(3), 3);
  EXPECT_EQ(trie.physicalIndex(4), 3);
  EXPECT_EQ(trie.physicalIndex(5), 3);
  EXPECT_EQ(trie.physicalIndex(31), 3);
}

TEST(HashArrayMappedTrieTest, BitmapTrieInsertTest) {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;
  trie.allocate(allocator, 1);

  auto e = std::make_pair(40LL, 4LL);
  trie.insertEntry(allocator, 4, e, nullptr, 2, 0);
  EXPECT_EQ(trie.bitmap() , 16); // 010000
  EXPECT_EQ(trie.size(), 1);
  EXPECT_EQ(trie.physicalGet(0).asEntry().first, 40);
  EXPECT_EQ(trie.physicalGet(0).asEntry().second, 4);

  e = std::make_pair(20L, 2L);
  trie.insertEntry(allocator, 2, e, nullptr, 2, 0);
  EXPECT_EQ(trie.bitmap() , 20); // 010100
  EXPECT_EQ(trie.size(), 2);
  EXPECT_EQ(trie.physicalGet(0).asEntry().first, 20);
  EXPECT_EQ(trie.physicalGet(0).asEntry().second, 2);
  EXPECT_EQ(trie.physicalGet(1).asEntry().first, 40);
  EXPECT_EQ(trie.physicalGet(1).asEntry().second, 4);

  e = std::make_pair(30L, 3L);
  trie.insertEntry(allocator, 3, e, nullptr, 2, 0);
  EXPECT_EQ(trie.bitmap() , 28); // 011100
  EXPECT_EQ(trie.size(), 3);
  EXPECT_EQ(trie.physicalGet(0).asEntry().first, 20);
  EXPECT_EQ(trie.physicalGet(0).asEntry().second, 2);
  EXPECT_EQ(trie.physicalGet(1).asEntry().first, 30);
  EXPECT_EQ(trie.physicalGet(1).asEntry().second, 3);
  EXPECT_EQ(trie.physicalGet(2).asEntry().first, 40);
  EXPECT_EQ(trie.physicalGet(2).asEntry().second, 4);

  e = std::make_pair(0LL, 0LL);
  trie.insertEntry(allocator, 0, e, nullptr, 2, 0);
  EXPECT_EQ(trie.bitmap() , 29); // 011101
  EXPECT_EQ(trie.size(), 4);
  EXPECT_EQ(trie.physicalGet(0).asEntry().first, 0);
  EXPECT_EQ(trie.physicalGet(0).asEntry().second, 0);
  EXPECT_EQ(trie.physicalGet(1).asEntry().first, 20);
  EXPECT_EQ(trie.physicalGet(1).asEntry().second, 2);
  EXPECT_EQ(trie.physicalGet(2).asEntry().first, 30);
  EXPECT_EQ(trie.physicalGet(2).asEntry().second, 3);
  EXPECT_EQ(trie.physicalGet(3).asEntry().first, 40);
  EXPECT_EQ(trie.physicalGet(3).asEntry().second, 4);

  e = std::make_pair(50LL, 5LL);
  trie.insertEntry(allocator, 5, e, nullptr, 2, 0);
  EXPECT_EQ(trie.bitmap() , 61); // 111101
  EXPECT_EQ(trie.size(), 5);
  EXPECT_EQ(trie.physicalGet(0).asEntry().first, 0);
  EXPECT_EQ(trie.physicalGet(0).asEntry().second, 0);
  EXPECT_EQ(trie.physicalGet(1).asEntry().first, 20);
  EXPECT_EQ(trie.physicalGet(1).asEntry().second, 2);
  EXPECT_EQ(trie.physicalGet(2).asEntry().first, 30);
  EXPECT_EQ(trie.physicalGet(2).asEntry().second, 3);
  EXPECT_EQ(trie.physicalGet(3).asEntry().first, 40);
  EXPECT_EQ(trie.physicalGet(3).asEntry().second, 4);
  EXPECT_EQ(trie.physicalGet(4).asEntry().first, 50);
  EXPECT_EQ(trie.physicalGet(4).asEntry().second, 5);

  e = std::make_pair(10LL, 1LL);
  trie.insertEntry(allocator, 1, e, nullptr, 2, 0);
  EXPECT_EQ(trie.bitmap() , 63); // 111111
  EXPECT_EQ(trie.size(), 6);
  EXPECT_EQ(trie.physicalGet(0).asEntry().first, 0);
  EXPECT_EQ(trie.physicalGet(0).asEntry().second, 0);
  EXPECT_EQ(trie.physicalGet(1).asEntry().first, 10);
  EXPECT_EQ(trie.physicalGet(1).asEntry().second, 1);
  EXPECT_EQ(trie.physicalGet(2).asEntry().first, 20);
  EXPECT_EQ(trie.physicalGet(2).asEntry().second, 2);
  EXPECT_EQ(trie.physicalGet(3).asEntry().first, 30);
  EXPECT_EQ(trie.physicalGet(3).asEntry().second, 3);
  EXPECT_EQ(trie.physicalGet(4).asEntry().first, 40);
  EXPECT_EQ(trie.physicalGet(4).asEntry().second, 4);
  EXPECT_EQ(trie.physicalGet(5).asEntry().first, 50);
  EXPECT_EQ(trie.physicalGet(5).asEntry().second, 5);

  e = std::make_pair(310LL, 31L);
  trie.insertEntry(allocator, 31, e, nullptr, 2, 0);
  EXPECT_EQ(trie.bitmap() , 63 | (0x1 << 31));
  EXPECT_EQ(trie.size(), 7);
  EXPECT_EQ(trie.physicalGet(6).asEntry().first, 310);
  EXPECT_EQ(trie.physicalGet(6).asEntry().second, 31);
}

TEST(HashArrayMappedTrieTest, BitmapTrieInsertTrieTest) {
  HAMT::BitmapTrie trie;
  HAMT::Node parent(nullptr);
  MallocAllocator allocator;
  std::pair<int64_t, int64_t> entry(2, 4);

  const uint32_t capacity = 2;
  trie.allocate(allocator, capacity);
  EXPECT_EQ(trie.size(), 0);

  // Insert an entry and a trie to the trie
  trie.insertEntry(allocator, 0, entry, &parent, 0, 0);
  EXPECT_EQ(trie.size(), 1);
  trie.insertTrie(allocator, &parent, 1, capacity);
  EXPECT_EQ(trie.size(), 2);

  // Retrieve the inserted entry
  HAMT::Node &inserted_entry_node = trie.logicalGet(0);
  EXPECT_TRUE(inserted_entry_node.isEntry());
  auto &inserted_entry = inserted_entry_node.asEntry();
  EXPECT_EQ(inserted_entry, entry);

  // Retrieve the inserted trie
  HAMT::Node &child_trie_node = trie.logicalGet(1);
  EXPECT_FALSE(child_trie_node.isEntry());
  EXPECT_TRUE(child_trie_node.isTrie());

  // Insert another trie into the child trie
  HAMT::BitmapTrie &child_trie = child_trie_node.asTrie();
  EXPECT_EQ(child_trie.size(), 0);
  child_trie.insertTrie(allocator, &child_trie_node, 0, 2);
  EXPECT_EQ(child_trie.size(), 1);

  // Retrieve the inserted trie
  HAMT::Node &grand_child_trie_node = child_trie.logicalGet(0);
  EXPECT_TRUE(grand_child_trie_node.isTrie());
  EXPECT_EQ(grand_child_trie_node.parent(), &child_trie_node);
  EXPECT_EQ(child_trie_node.parent(), &parent);
}

TEST(HashArrayMappedTrieTest, NodeInitializationAsBitmapTrieTest) {
  HAMT::Node parent(nullptr);
  EXPECT_EQ(parent.parent(), nullptr);

  HAMT::Node node(&parent);
  EXPECT_TRUE(node.isTrie());
  EXPECT_FALSE(node.isEntry());
  EXPECT_EQ(node.parent(), &parent);

  MallocAllocator allocator;
  node.BitmapTrie(allocator, &parent, 2);
  EXPECT_TRUE(node.isTrie());
  EXPECT_FALSE(node.isEntry());
  EXPECT_EQ(node.parent(), &parent);
  auto &trie = node.asTrie();
  EXPECT_EQ(trie.capacity(), 2);
  EXPECT_EQ(trie.size(), 0);

  HAMT::Node a_node(nullptr);
  a_node = std::move(node);
  EXPECT_EQ(a_node.parent(), &parent);
  auto &a_trie = node.asTrie();
  EXPECT_EQ(&trie.physicalGet(0), &a_trie.physicalGet(0));
  EXPECT_EQ(a_trie.capacity(), 2);
  EXPECT_EQ(a_trie.size(), 0);

  a_trie.deallocate(allocator);
}

TEST(HashArrayMappedTrieTest, NodeInitializationAsEntryTest) {
  HAMT::Node parent(nullptr);

  std::pair<int64_t, int64_t> entry = std::make_pair(2, 4);
  HAMT::Node node(std::move(entry), &parent);
  EXPECT_TRUE(node.isEntry());
  EXPECT_FALSE(node.isTrie());
  EXPECT_EQ(node.parent(), &parent);

  HAMT::Node a_node(nullptr);
  a_node = std::move(entry);
  EXPECT_TRUE(node.isEntry());
  EXPECT_FALSE(node.isTrie());
}

TEST(HashArrayMappedTrieTest, ParentTest) {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t>;
  parent_test<HAMT>(2048);
}

TEST(HashArrayMappedTrieTest, ParentTestWithBadHashFunction) {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t, BadHashFunction>;
  parent_test<HAMT>(64);
}

TEST(HashArrayMappedTrieTest, ParentTestWithIdentityFunction) {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t, IdentityFunction>;
  parent_test<HAMT>(2048);
}

TEST(HashArrayMappedTrieTest, ParentTestConstantFunction) {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t, ConstantFunction>;
  parent_test<HAMT>(64);
}

TEST(HashArrayMappedTrieTest, TopLevelInsertTest) {
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
      EXPECT_TRUE(found != nullptr);
      /*
      if (found == nullptr) {
        count_not_found++;
        last_not_found = i * 10;
        continue;
      }
      */
      EXPECT_EQ(*found, i);
    }
    /* printf("count_not_found %d\n", count_not_found); */
    EXPECT_EQ(last_not_found, -1);
  }
}


TEST(HashArrayMappedTrieTest, ConstantHashFunctionTest) {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t, ConstantFunction>;
  HAMT hamt;
  for (int64_t i = 0; i < 32; i++) {
    insertKeyAndValue(hamt, i, i);
  }
}

TEST(HashArrayMappedTrieTest, PhysicalIndexOfNodeInTrie) {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t, IdentityFunction>;
  HAMT hamt;
  hamt._seed = 0;
  HAMT::Node *root = &hamt._root;
  for (int64_t i = 31; i >= 0; i--) {
    insertKeyAndValue(hamt, i, i);
  }
  for (uint32_t i = 0; i < 32; i++) {
    EXPECT_EQ(root->asTrie().physicalIndex(i), i);
    HAMT::Node *logical_node = &root->asTrie().logicalGet(i);
    HAMT::Node *physical_node = &root->asTrie().physicalGet(i);
    EXPECT_EQ(logical_node->asEntry().first, i);
    EXPECT_EQ(logical_node, physical_node);
    EXPECT_EQ(logical_node->parent(), root);
    EXPECT_EQ(root->asTrie().physicalIndexOf(logical_node), i);
  }
}
