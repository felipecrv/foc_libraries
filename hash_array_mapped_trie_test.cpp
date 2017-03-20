#include <queue>
#include <vector>

#include <gtest/gtest.h>

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

TEST_F(HashArrayMappedTrieTest, LogicalToPhysicalIndexTranslationTest) {
  HAMT::BitmapIndexedNode node;

  // All items will be stored at index=0 if the bitmap is empty
  node._bitmap = 0; // 0000
  for (int i = 0; i < 32; i++) {
    EXPECT_EQ(node.physical_index(i), 0);
  }
  // For any bitmap, the logical index 0 will map to physical index 0.
  // (we test all bitmaps that contain a single bit)
  for (int i = 0; i < 32; i++) {
    node._bitmap = 0x1 << i;
    EXPECT_EQ(node.physical_index(0), 0);
  }

  node._bitmap = 1; // 0001
  EXPECT_EQ(node.physical_index(1), 1);
  EXPECT_EQ(node.physical_index(2), 1);
  EXPECT_EQ(node.physical_index(3), 1);
  EXPECT_EQ(node.physical_index(4), 1);
  EXPECT_EQ(node.physical_index(5), 1);
  EXPECT_EQ(node.physical_index(31), 1);
  node._bitmap = 2; // 0010
  EXPECT_EQ(node.physical_index(1), 0);
  EXPECT_EQ(node.physical_index(2), 1);
  EXPECT_EQ(node.physical_index(3), 1);
  EXPECT_EQ(node.physical_index(4), 1);
  EXPECT_EQ(node.physical_index(5), 1);
  EXPECT_EQ(node.physical_index(31), 1);
  node._bitmap = 3; // 0011
  EXPECT_EQ(node.physical_index(1), 1);
  EXPECT_EQ(node.physical_index(2), 2);
  EXPECT_EQ(node.physical_index(3), 2);
  EXPECT_EQ(node.physical_index(4), 2);
  EXPECT_EQ(node.physical_index(5), 2);
  EXPECT_EQ(node.physical_index(31), 2);
  node._bitmap = 4; // 0100
  EXPECT_EQ(node.physical_index(1), 0);
  EXPECT_EQ(node.physical_index(2), 0);
  EXPECT_EQ(node.physical_index(3), 1);
  EXPECT_EQ(node.physical_index(4), 1);
  EXPECT_EQ(node.physical_index(5), 1);
  EXPECT_EQ(node.physical_index(31), 1);
  node._bitmap = 5; // 0101
  EXPECT_EQ(node.physical_index(1), 1);
  EXPECT_EQ(node.physical_index(2), 1);
  EXPECT_EQ(node.physical_index(3), 2);
  EXPECT_EQ(node.physical_index(4), 2);
  EXPECT_EQ(node.physical_index(5), 2);
  EXPECT_EQ(node.physical_index(31), 2);
  node._bitmap = 6; // 0110
  EXPECT_EQ(node.physical_index(1), 0);
  EXPECT_EQ(node.physical_index(2), 1);
  EXPECT_EQ(node.physical_index(3), 2);
  EXPECT_EQ(node.physical_index(4), 2);
  EXPECT_EQ(node.physical_index(5), 2);
  EXPECT_EQ(node.physical_index(31), 2);
  node._bitmap = 7; // 0111
  EXPECT_EQ(node.physical_index(1), 1);
  EXPECT_EQ(node.physical_index(2), 2);
  EXPECT_EQ(node.physical_index(3), 3);
  EXPECT_EQ(node.physical_index(4), 3);
  EXPECT_EQ(node.physical_index(5), 3);
  EXPECT_EQ(node.physical_index(31), 3);
}

TEST_F(HashArrayMappedTrieTest, BitmapIndexedNodeInsertionTest) {
  HAMT hamt;
  HAMT::BitmapIndexedNode node;
  node.Initialize(&hamt, 1, 0, 0);

  auto e = node.Insert(&hamt, 4, 40, 4, 0);
  EXPECT_EQ(node._bitmap, 16); // 010000
  EXPECT_EQ(node.size(), 1);
  EXPECT_EQ(node._base[0].entry().key, 40);
  EXPECT_EQ(node._base[0].entry().value, 4);

  e = node.Insert(&hamt, 2, 20, 2, 0);
  EXPECT_EQ(node._bitmap, 20); // 010100
  EXPECT_EQ(node.size(), 2);
  EXPECT_EQ(node._base[0].entry().key, 20);
  EXPECT_EQ(node._base[0].entry().value, 2);
  EXPECT_EQ(node._base[1].entry().key, 40);
  EXPECT_EQ(node._base[1].entry().value, 4);

  e = node.Insert(&hamt, 3, 30, 3, 0);
  EXPECT_EQ(node._bitmap, 28); // 011100
  EXPECT_EQ(node.size(), 3);
  EXPECT_EQ(node._base[0].entry().key, 20);
  EXPECT_EQ(node._base[0].entry().value, 2);
  EXPECT_EQ(node._base[1].entry().key, 30);
  EXPECT_EQ(node._base[1].entry().value, 3);
  EXPECT_EQ(node._base[2].entry().key, 40);
  EXPECT_EQ(node._base[2].entry().value, 4);

  e = node.Insert(&hamt, 0, 0, 0, 0);
  EXPECT_EQ(node._bitmap, 29); // 011101
  EXPECT_EQ(node.size(), 4);
  EXPECT_EQ(node._base[0].entry().key, 0);
  EXPECT_EQ(node._base[0].entry().value, 0);
  EXPECT_EQ(node._base[1].entry().key, 20);
  EXPECT_EQ(node._base[1].entry().value, 2);
  EXPECT_EQ(node._base[2].entry().key, 30);
  EXPECT_EQ(node._base[2].entry().value, 3);
  EXPECT_EQ(node._base[3].entry().key, 40);
  EXPECT_EQ(node._base[3].entry().value, 4);

  e = node.Insert(&hamt, 5, 50, 5, 0);
  EXPECT_EQ(node._bitmap, 61); // 111101
  EXPECT_EQ(node.size(), 5);
  EXPECT_EQ(node._base[0].entry().key, 0);
  EXPECT_EQ(node._base[0].entry().value, 0);
  EXPECT_EQ(node._base[1].entry().key, 20);
  EXPECT_EQ(node._base[1].entry().value, 2);
  EXPECT_EQ(node._base[2].entry().key, 30);
  EXPECT_EQ(node._base[2].entry().value, 3);
  EXPECT_EQ(node._base[3].entry().key, 40);
  EXPECT_EQ(node._base[3].entry().value, 4);
  EXPECT_EQ(node._base[4].entry().key, 50);
  EXPECT_EQ(node._base[4].entry().value, 5);

  e = node.Insert(&hamt, 1, 10, 1, 0);
  EXPECT_EQ(node._bitmap, 63); // 111111
  EXPECT_EQ(node.size(), 6);
  EXPECT_EQ(node._base[0].entry().key, 0);
  EXPECT_EQ(node._base[0].entry().value, 0);
  EXPECT_EQ(node._base[1].entry().key, 10);
  EXPECT_EQ(node._base[1].entry().value, 1);
  EXPECT_EQ(node._base[2].entry().key, 20);
  EXPECT_EQ(node._base[2].entry().value, 2);
  EXPECT_EQ(node._base[3].entry().key, 30);
  EXPECT_EQ(node._base[3].entry().value, 3);
  EXPECT_EQ(node._base[4].entry().key, 40);
  EXPECT_EQ(node._base[4].entry().value, 4);
  EXPECT_EQ(node._base[5].entry().key, 50);
  EXPECT_EQ(node._base[5].entry().value, 5);

  e = node.Insert(&hamt, 31, 310, 31, 0);
  EXPECT_EQ(node._bitmap, 63 | (0x1 << 31));
  EXPECT_EQ(node.size(), 7);
  EXPECT_EQ(node._base[6].entry().key, 310);
  EXPECT_EQ(node._base[6].entry().value, 31);
}

void print_bitmap_indexed_node(
    HAMT::BitmapIndexedNode &node,
    std::string indent) {
  std::vector<HAMT::BitmapIndexedNode *> nodes;

  printf("%3d/%-3d: %s", node.size(), node._capacity, indent.c_str());
  for (int i = 0; i < 32; i++) {
    if (node.position_occupied(i)) {
      auto& entry_node = node[i];
      if (entry_node.is_entry) {
        printf("%3lld ", entry_node.entry().value);
      } else {
        printf("[ ] ");
        nodes.push_back(&entry_node.node());
      }
    } else {
      printf("--- ");
    }
  }
  putchar('\n');

  for (auto node : nodes) {
    print_bitmap_indexed_node(*node, indent + "    ");
  }
}

void print_hamt(HAMT &hamt) {
  print_bitmap_indexed_node(hamt.root, "");
  putchar('\n');
}

void print_stats(HAMT &hamt) {
  std::queue<HAMT::BitmapIndexedNode *> q;

  int stats[33];
  memset(stats, 0, sizeof(stats));

  q.push(&hamt.root);
  while (!q.empty()) {
    auto node = q.front();
    q.pop();

    stats[node->size()]++;

    for (int i = 0; i < 32; i++) {
      if (node->position_occupied(i)) {
        auto& entry_node = (*node)[i];
        if (!entry_node.is_entry) {
          q.push(&entry_node.node());
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

TEST_F(HashArrayMappedTrieTest, TopLevelInsertionTest) {
  /* for (int max = 1; max <= 1048576; max *= 2) { */
  for (int64_t max = 65536; max <= 65536; max *= 2) {
    HAMT hamt;
    for (int64_t i = 1; i <= max; i++) {
      /* printf("%d:\n", i * 10); */
      hamt.Insert(i * 10, i);
      if (__builtin_popcount(i) == 1) {
        hamt.print();
      }
    }
    /* print_hamt(hamt); */
    /* print_stats(hamt); */

    /* int64_t count_not_found = 0; */
    int64_t last_not_found = -1;
    for (int64_t i = 1; i <= max; i++) {
      auto found = hamt.Find(i * 10);
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
