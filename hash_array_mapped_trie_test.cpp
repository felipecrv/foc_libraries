#include <queue>
#include <vector>

#include <gtest/gtest.h>

#define GTEST
#define HAMT_IMPLEMENTATION
#include "hash_array_mapped_trie.h"

using foc::HashArrayMappedTrie;

using HAMT = HashArrayMappedTrie<int64_t, int64_t>;

class HashArrayMappedTrieTest : public testing::Test {
 protected:
  void SetUp() override {
  }

  void TearDown() override {
  }
};

TEST_F(HashArrayMappedTrieTest, AllocationSizeCalculationTest) {
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

TEST_F(HashArrayMappedTrieTest, BitmatpTrieInitialization) {
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

TEST_F(HashArrayMappedTrieTest, LogicalZeroToPhysicalZeroIndexTranslationTest) {
  HAMT::BitmapTrie trie;

  // For any bitmap, the logical index 0 will map to physical index 0.
  // (we test all bitmaps that contain a single bit)
  for (int i = 0; i < 32; i++) {
    trie._bitmap = 0x1 << i;
    EXPECT_EQ(trie.physicalIndex(0), 0);
  }
}

TEST_F(HashArrayMappedTrieTest, LogicalToPhysicalIndexTranslationTest) {
  HAMT::BitmapTrie trie;

  trie._bitmap = 1; // 0001
  EXPECT_EQ(trie.physicalIndex(1), 1);
  EXPECT_EQ(trie.physicalIndex(2), 1);
  EXPECT_EQ(trie.physicalIndex(3), 1);
  EXPECT_EQ(trie.physicalIndex(4), 1);
  EXPECT_EQ(trie.physicalIndex(5), 1);
  EXPECT_EQ(trie.physicalIndex(31), 1);
  trie._bitmap = 2; // 0010
  EXPECT_EQ(trie.physicalIndex(1), 0);
  EXPECT_EQ(trie.physicalIndex(2), 1);
  EXPECT_EQ(trie.physicalIndex(3), 1);
  EXPECT_EQ(trie.physicalIndex(4), 1);
  EXPECT_EQ(trie.physicalIndex(5), 1);
  EXPECT_EQ(trie.physicalIndex(31), 1);
  trie._bitmap = 3; // 0011
  EXPECT_EQ(trie.physicalIndex(1), 1);
  EXPECT_EQ(trie.physicalIndex(2), 2);
  EXPECT_EQ(trie.physicalIndex(3), 2);
  EXPECT_EQ(trie.physicalIndex(4), 2);
  EXPECT_EQ(trie.physicalIndex(5), 2);
  EXPECT_EQ(trie.physicalIndex(31), 2);
  trie._bitmap = 4; // 0100
  EXPECT_EQ(trie.physicalIndex(1), 0);
  EXPECT_EQ(trie.physicalIndex(2), 0);
  EXPECT_EQ(trie.physicalIndex(3), 1);
  EXPECT_EQ(trie.physicalIndex(4), 1);
  EXPECT_EQ(trie.physicalIndex(5), 1);
  EXPECT_EQ(trie.physicalIndex(31), 1);
  trie._bitmap = 5; // 0101
  EXPECT_EQ(trie.physicalIndex(1), 1);
  EXPECT_EQ(trie.physicalIndex(2), 1);
  EXPECT_EQ(trie.physicalIndex(3), 2);
  EXPECT_EQ(trie.physicalIndex(4), 2);
  EXPECT_EQ(trie.physicalIndex(5), 2);
  EXPECT_EQ(trie.physicalIndex(31), 2);
  trie._bitmap = 6; // 0110
  EXPECT_EQ(trie.physicalIndex(1), 0);
  EXPECT_EQ(trie.physicalIndex(2), 1);
  EXPECT_EQ(trie.physicalIndex(3), 2);
  EXPECT_EQ(trie.physicalIndex(4), 2);
  EXPECT_EQ(trie.physicalIndex(5), 2);
  EXPECT_EQ(trie.physicalIndex(31), 2);
  trie._bitmap = 7; // 0111
  EXPECT_EQ(trie.physicalIndex(1), 1);
  EXPECT_EQ(trie.physicalIndex(2), 2);
  EXPECT_EQ(trie.physicalIndex(3), 3);
  EXPECT_EQ(trie.physicalIndex(4), 3);
  EXPECT_EQ(trie.physicalIndex(5), 3);
  EXPECT_EQ(trie.physicalIndex(31), 3);
}

TEST_F(HashArrayMappedTrieTest, BitmapTrieInsertTest) {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;
  trie.allocate(allocator, 1);

  auto e = std::make_pair(40LL, 4LL);
  trie.insert(allocator, 4, e, nullptr, 2, 0);
  EXPECT_EQ(trie._bitmap, 16); // 010000
  EXPECT_EQ(trie.size(), 1);
  EXPECT_EQ(trie.physicalGet(0).asEntry().first, 40);
  EXPECT_EQ(trie.physicalGet(0).asEntry().second, 4);

  e = std::make_pair(20L, 2L);
  trie.insert(allocator, 2, e, nullptr, 2, 0);
  EXPECT_EQ(trie._bitmap, 20); // 010100
  EXPECT_EQ(trie.size(), 2);
  EXPECT_EQ(trie.physicalGet(0).asEntry().first, 20);
  EXPECT_EQ(trie.physicalGet(0).asEntry().second, 2);
  EXPECT_EQ(trie.physicalGet(1).asEntry().first, 40);
  EXPECT_EQ(trie.physicalGet(1).asEntry().second, 4);

  e = std::make_pair(30L, 3L);
  trie.insert(allocator, 3, e, nullptr, 2, 0);
  EXPECT_EQ(trie._bitmap, 28); // 011100
  EXPECT_EQ(trie.size(), 3);
  EXPECT_EQ(trie.physicalGet(0).asEntry().first, 20);
  EXPECT_EQ(trie.physicalGet(0).asEntry().second, 2);
  EXPECT_EQ(trie.physicalGet(1).asEntry().first, 30);
  EXPECT_EQ(trie.physicalGet(1).asEntry().second, 3);
  EXPECT_EQ(trie.physicalGet(2).asEntry().first, 40);
  EXPECT_EQ(trie.physicalGet(2).asEntry().second, 4);

  e = std::make_pair(0LL, 0LL);
  trie.insert(allocator, 0, e, nullptr, 2, 0);
  EXPECT_EQ(trie._bitmap, 29); // 011101
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
  trie.insert(allocator, 5, e, nullptr, 2, 0);
  EXPECT_EQ(trie._bitmap, 61); // 111101
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
  trie.insert(allocator, 1, e, nullptr, 2, 0);
  EXPECT_EQ(trie._bitmap, 63); // 111111
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
  trie.insert(allocator, 31, e, nullptr, 2, 0);
  EXPECT_EQ(trie._bitmap, 63 | (0x1 << 31));
  EXPECT_EQ(trie.size(), 7);
  EXPECT_EQ(trie.physicalGet(6).asEntry().first, 310);
  EXPECT_EQ(trie.physicalGet(6).asEntry().second, 31);
}

TEST_F(HashArrayMappedTrieTest, BitmapTrieInsertTrieTest) {
  HAMT::BitmapTrie trie;
  HAMT::Node parent(nullptr);
  MallocAllocator allocator;
  std::pair<int64_t, int64_t> entry(2, 4);

  const uint32_t capacity = 2;
  trie.allocate(allocator, capacity);
  EXPECT_EQ(trie.size(), 0);

  // Insert an entry and a trie to the trie
  trie.insert(allocator, 0, entry, &parent, 0, 0);
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

template<class Hash>
void print_bitmap_indexed_node(
    typename foc::HashArrayMappedTrie<int64_t, int64_t, Hash>::BitmapTrie &trie,
    std::string indent) {
  std::vector<typename foc::HashArrayMappedTrie<int64_t, int64_t, Hash>::BitmapTrie *> tries;

  printf("%3d/%-3d: %s", trie.size(), trie.capacity(), indent.c_str());
  for (int i = 0; i < 32; i++) {
    if (trie.logicalPositionTaken(i)) {
      auto& node = trie.logicalGet(i);
      if (node.isEntry()) {
        printf("%3lld ", node.asEntry().second);
      } else {
        printf("[ ] ");
        tries.push_back(&node.asTrie());
      }
    } else {
      printf("--- ");
    }
  }
  putchar('\n');

  for (auto trie : tries) {
    print_bitmap_indexed_node<Hash>(*trie, indent + "    ");
  }
}

template<class Hash>
void print_hamt(typename foc::HashArrayMappedTrie<int64_t, int64_t, Hash> &hamt) {
  print_bitmap_indexed_node<Hash>(hamt.root().asTrie(), "");
  putchar('\n');
}

void print_stats(HAMT &hamt) {
  std::queue<HAMT::BitmapTrie *> q;

  int stats[33];
  memset(stats, 0, sizeof(stats));

  q.push(&hamt.root().asTrie());
  while (!q.empty()) {
    auto trie = q.front();
    q.pop();

    stats[trie->size()]++;

    for (int i = 0; i < 32; i++) {
      if (trie->logicalPositionTaken(i)) {
        auto& node = trie->logicalGet(i);
        if (node.isTrie()) {
          q.push(&node.asTrie());
        }
      }
    }
  }

  int total = 0;
  for (int i = 1; i <= 32; i++) {
    printf("%6d ", stats[i]);
    total += stats[i];
  }
  putchar('\n');
  for (int i = 1; i <= 32; i++) {
    printf("%6.3lf ", (double)stats[i] / total);
  }
  putchar('\n');
  for (int i = 1; i <= 32; i++) {
    double a = (double)stats[i] / total;
    printf("%6d ", (int)(a * 100));
  }
  putchar('\n');
}

TEST_F(HashArrayMappedTrieTest, NodeInitializationAsBitmapTrieTest) {
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

TEST_F(HashArrayMappedTrieTest, NodeInitializationAsEntryTest) {
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

TEST_F(HashArrayMappedTrieTest, ParentTest) {
  HAMT hamt;
  const int64_t N = 45000;

  // Insert many items into the HAMT
  for (int64_t i = 0; i < N; i++) {
    hamt.insert(i, i + 1);
  }

  // For each node (leave), make sure the root is reachable through the _parent
  // pointers.
  double height_sum = 0.0;
  for (int64_t i = 0; i < N; i++) {
    const HAMT::Node *node = hamt.findNode(i);
    EXPECT_TRUE(node != nullptr);
    EXPECT_TRUE(node->asEntry().first == i);
    EXPECT_TRUE(node->asEntry().second == i + 1);

    double height = 0.0;
    do {
      auto parent = node->parent();

      // Make sure you can find the node from the parent
      auto parent_trie = &parent->asTrie();
      bool found_node = false;
      for (uint32_t i = 0; i < parent_trie->size() && !found_node; i++) {
        if (&parent_trie->logicalGet(i) == node) {
          found_node = true;
        }
      }
      if (!found_node) {
        printf("%lld\n", i + 1);
        print_hamt<std::hash<int64_t>>(hamt);
        goto out;
      }
      EXPECT_TRUE(found_node);

      node = parent;
      height++;
    } while (node != &hamt.root());

    height_sum += height;
  }
out:

  // Average height is less than 4
  EXPECT_TRUE(height_sum / N < 4.0);
}

TEST_F(HashArrayMappedTrieTest, TopLevelInsertTest) {
  /* for (int max = 1; max <= 1048576; max *= 2) { */
  for (int64_t max = 65536; max <= 65536; max *= 2) {
    HAMT hamt;
    for (int64_t i = 1; i <= max; i++) {
      /* printf("%d:\n", i * 10); */
      hamt.insert(i * 10, i);
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

struct BadHashFunction {
  size_t operator()(int64_t key) const {
    return ((size_t)key % 1024) * 0x3f3f3f3f;
  }
};

TEST_F(HashArrayMappedTrieTest, ParentTestWithBadHashFunction) {
  HashArrayMappedTrie<int64_t, int64_t, BadHashFunction> hamt;
  const int64_t N = 64;

  // Insert many items into the HAMT
  for (int64_t i = 0; i < N; i++) {
    hamt.insert(i, i + 1);
    if (i == 10) {
      print_hamt<BadHashFunction>(hamt);
    }
  }

  // For each node (leave), make sure the root is reachable through the _parent
  // pointers.
  double height_sum = 0.0;
  for (int64_t i = 0; i < N; i++) {
    const HAMT::Node *node = hamt.findNode(i);
    EXPECT_TRUE(node != nullptr);
    EXPECT_TRUE(node->asEntry().first == i);
    EXPECT_TRUE(node->asEntry().second == i + 1);

    double height = 0.0;
    do {
      auto parent = node->parent();

      // Make sure you can find the node from the parent
      auto parent_trie = &parent->asTrie();
      bool found_parent = false;
      for (uint32_t i = 0; i < parent_trie->size() && !found_parent; i++) {
        if (&parent_trie->logicalGet(i) == parent) {
          found_parent = true;
        }
      }
      EXPECT_TRUE(found_parent);

      node = parent;
      height++;
    } while (node != &hamt.root());

    height_sum += height;
  }

  // Average height is less than 4
  EXPECT_TRUE(height_sum / N < 4.0);
}
