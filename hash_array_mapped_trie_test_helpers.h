#include <queue>
#include <vector>

using foc::HashArrayMappedTrie;

using HAMT = HashArrayMappedTrie<int64_t, int64_t>;

template<class HAMT>
static void print_bitmap_indexed_node(typename HAMT::BitmapTrie &trie, std::string indent) {
  std::vector<typename HAMT::BitmapTrie *> tries;

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
    print_bitmap_indexed_node<HAMT>(*trie, indent + "    ");
  }
}

template<class HAMT>
static void print_hamt(HAMT &hamt) {
  print_bitmap_indexed_node<HAMT>(hamt.root().asTrie(), "");
  putchar('\n');
}

template<typename  HAMT>
static void print_stats(HAMT &hamt) {
  std::queue<typename HAMT::BitmapTrie *> q;

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

// Property checking helpers

template<class HAMT>
static void check_parent_pointers(HAMT &hamt) {
  // From each trie node, check if its children point to the trie node.
  EXPECT_EQ(hamt._root.parent(), nullptr);
  std::queue<typename HAMT::Node *> q;
  q.push(&hamt._root);
  size_t bfs_count = 0;
  while (!q.empty()) {
    auto *node = q.front();
    q.pop();
    if (node->isTrie()) {
      auto *trie = &node->asTrie();
      for (uint32_t i = 0; i < trie->size(); i++) {
        auto *child_node = &trie->physicalGet(i);
        EXPECT_EQ(child_node->parent(), node);
        if (child_node->isTrie()) {
          q.push(child_node);
        } else {
          bfs_count++;
        }
      }
    }
  }
  EXPECT_EQ(bfs_count, hamt.size());

  // For each entry node (leave), make sure the root is reachable through the _parent
  // pointers.
  for (int64_t i = 0; i < hamt.size(); i++) {
    const typename HAMT::Node *node = hamt.findNode(i);
    EXPECT_TRUE(node != nullptr);
    EXPECT_TRUE(node->asEntry().first == i);
    EXPECT_TRUE(node->asEntry().second == i);

    do {
      // Make sure you can find the root from the node
      node = node->parent();
    } while (node != &hamt.root());
  }
}