#include <queue>
#include <vector>

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "test_classes.h"

#define TESTS
#define HAMT_IMPLEMENTATION
#include "../foc/hash_array_mapped_trie.h"

#include "hash_array_mapped_trie_test_helpers.h"

using foc::HashArrayMappedTrie;
using foc::Constructable;
using foc::MallocAllocator;

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

TEST_CASE("BitmatpTrieInitialization", "[BitmapTrie]") {
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

// BitmapTrie::cloneRecursively() and BitmapTrie::deallocateRecursively()
// aren't tested directly.

TEST_CASE("LogicalZeroToPhysicalZeroIndexTranslationTest", "[BitmapTrie]") {
  HAMT::BitmapTrie trie;

  // For any bitmap, the logical index 0 will map to physical index 0.
  // (we test all bitmaps that contain a single bit)
  for (int i = 0; i < 32; i++) {
    trie.bitmap() = 0x1 << i;
    REQUIRE(trie.physicalIndex(0) == 0);
  }
}

TEST_CASE("LogicalToPhysicalIndexTranslationTest", "[BitmapTrie]") {
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

TEST_CASE("BitmapTrieInsertEntryTest", "[BitmapTrie]") {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;
  trie.allocate(allocator, 1);

  trie.insertEntry(allocator, 4, nullptr, 2, 0)->asEntry() = std::make_pair(40LL, 4LL);
  REQUIRE(trie.bitmap() == 16);  // 010000
  REQUIRE(trie.size() == 1);
  REQUIRE(trie.physicalGet(0).asEntry().first == 40);
  REQUIRE(trie.physicalGet(0).asEntry().second == 4);

  trie.insertEntry(allocator, 2, nullptr, 2, 0)->asEntry() = std::make_pair(20L, 2L);
  REQUIRE(trie.bitmap() == 20);  // 010100
  REQUIRE(trie.size() == 2);
  REQUIRE(trie.physicalGet(0).asEntry().first == 20);
  REQUIRE(trie.physicalGet(0).asEntry().second == 2);
  REQUIRE(trie.physicalGet(1).asEntry().first == 40);
  REQUIRE(trie.physicalGet(1).asEntry().second == 4);

  trie.insertEntry(allocator, 3, nullptr, 2, 0)->asEntry() = std::make_pair(30L, 3L);
  REQUIRE(trie.bitmap() == 28);  // 011100
  REQUIRE(trie.size() == 3);
  REQUIRE(trie.physicalGet(0).asEntry().first == 20);
  REQUIRE(trie.physicalGet(0).asEntry().second == 2);
  REQUIRE(trie.physicalGet(1).asEntry().first == 30);
  REQUIRE(trie.physicalGet(1).asEntry().second == 3);
  REQUIRE(trie.physicalGet(2).asEntry().first == 40);
  REQUIRE(trie.physicalGet(2).asEntry().second == 4);

  *trie.insertEntry(allocator, 0, nullptr, 2, 0) = std::make_pair(0LL, 0LL);
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

  trie.insertEntry(allocator, 5, nullptr, 2, 0)->asEntry() = std::make_pair(50LL, 5LL);
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

  trie.insertEntry(allocator, 1, nullptr, 2, 0)->asEntry() = std::make_pair(10LL, 1LL);
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

  trie.insertEntry(allocator, 31, nullptr, 2, 0)->asEntry() = std::make_pair(310LL, 31L);
  REQUIRE(trie.bitmap() == (63u | (0x1u << 31)));
  REQUIRE(trie.size() == 7);
  REQUIRE(trie.physicalGet(6).asEntry().first == 310);
  REQUIRE(trie.physicalGet(6).asEntry().second == 31);
}

TEST_CASE("BitmapTrieInsertEntryTilFullTest", "[BitmapTrie]") {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;

  int64_t numbers[] = {
      24, 26, 23, 18, 7, 28, 12, 0,  5, 2,  22, 15, 30, 8, 31, 20,
      1,  13, 17, 21, 4, 14, 25, 19, 6, 27, 16, 10, 29, 3, 11, 9,
  };
  std::vector<std::pair<int64_t, int64_t>> entries;
  for (const auto num : numbers) {
    entries.push_back(std::make_pair(num, num));
  }

  trie.allocate(allocator, 0);
  int64_t inserted_sum = 0;
  for (size_t i = 0; i < entries.size(); i++) {
    auto new_entry = entries[i];
    trie.insertEntry(allocator, entries[i].first, nullptr, 100, 0)->asEntry() =
        std::move(new_entry);
    inserted_sum += entries[i].second;
    int64_t sum = 0;
    for (uint32_t j = 0; j <= i; j++) {
      sum += trie.logicalGet(entries[j].first).asEntry().second;
    }
    REQUIRE(sum == inserted_sum);
  }
}

TEST_CASE("BitmapTrieInsertTrieTest", "[BitmapTrie]") {
  HAMT::BitmapTrie trie;
  HAMT::Node parent(nullptr);
  MallocAllocator allocator;
  std::pair<int64_t, int64_t> entry(2, 4);
  auto new_entry = entry;

  const uint32_t capacity = 2;
  trie.allocate(allocator, capacity);
  REQUIRE(trie.size() == 0);

  // Insert an entry and a trie to the trie
  trie.insertEntry(allocator, 0, &parent, 0, 0)->asEntry() = std::move(new_entry);
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

TEST_CASE("FirstEntryInNodeTest", "[BitmapTrie]") {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;

  std::pair<int64_t, int64_t> two(2, 2);
  std::pair<int64_t, int64_t> three(3, 3);

  // Insert two entries into a trie and check the first
  trie.allocate(allocator, 4);
  trie.insertEntry(allocator, 3, nullptr, 4, 0)->asEntry() = std::move(three);
  REQUIRE(trie.logicalPositionTaken(3));
  trie.insertEntry(allocator, 2, nullptr, 4, 0)->asEntry() = std::move(two);
  REQUIRE(trie.logicalPositionTaken(2));

  const HAMT::Node *node = trie.firstEntryNodeRecursively();
  REQUIRE(node->asEntry().second == 2);
}

TEST_CASE("FirstEntryRecursivelyTest", "[BitmapTrie]") {
  HAMT::BitmapTrie trie;
  MallocAllocator allocator;

  std::pair<int64_t, int64_t> two(2, 2);
  std::pair<int64_t, int64_t> three(3, 3);

  // Insert an entry and a trie with an entry to cause recursion
  trie.allocate(allocator, 4);
  trie.insertEntry(allocator, 3, nullptr, 4, 0)->asEntry() = std::move(three);
  REQUIRE(trie.logicalPositionTaken(3));
  HAMT::Node *child = trie.insertTrie(allocator, nullptr, 0, 1);
  REQUIRE(trie.logicalPositionTaken(0));
  child->asTrie().insertEntry(allocator, 0, nullptr, 1, 1)->asEntry() = std::move(two);
  REQUIRE(child->asTrie().logicalPositionTaken(0));

  const HAMT::Node *node = trie.firstEntryNodeRecursively();
  REQUIRE(node->asEntry().second == 2);
}

TEST_CASE("NodeInitializationAsBitmapTrieTest", "[Node]") {
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

TEST_CASE("NodeInitializationAsEntryTest", "[Node]") {
  HAMT::Node parent(nullptr);

  std::pair<const int64_t, int64_t> entry(2, 4);
  HAMT::Node node(std::move(entry), &parent);
  REQUIRE(node.isEntry());
  REQUIRE(!node.isTrie());
  REQUIRE(node.parent() == &parent);

  HAMT::Node a_node(nullptr);
  a_node = std::move(entry);
  REQUIRE(node.isEntry());
  REQUIRE(!node.isTrie());
}

TEST_CASE("InsertionTest", "[HAMT]") {
  for (int max = 1; max <= 1048576; max *= 2) {
    HAMT hamt;
    for (int64_t i = 1; i <= max; i++) {
      hamt.put(i * 10, i);
    }

    int64_t last_not_found = -1;
    for (int64_t i = 1; i <= max; i++) {
      auto found = hamt.findValue(i * 10);
      REQUIRE(found != nullptr);
      if (found == nullptr) {
        last_not_found = i * 10;
        continue;
      }
      REQUIRE(*found == i);
    }
    REQUIRE(last_not_found == -1);
  }
}

TEST_CASE("InsertDoesntReplace", "[HAMT]") {
  HAMT hamt;
  REQUIRE(hamt.size() == 0);
  hamt.insert(std::make_pair(1, 1));
  REQUIRE(hamt.size() == 1);
  hamt.insert(std::make_pair(1, 10));
  REQUIRE(hamt.size() == 1);
  REQUIRE(hamt.find(1)->second == 1);
  REQUIRE(!hamt.put(2, 20));  // Didn't replace. 2 doesn't exist.
  REQUIRE(hamt.size() == 2);
  REQUIRE(hamt.put(1, 10));  // 1 is replaced
  REQUIRE(hamt.size() == 2);
  REQUIRE(hamt.find(1)->second == 10);
  REQUIRE(hamt.find(2)->second == 20);
}

TEST_CASE("operator[]", "[HAMT]") {
  foc::HashArrayMappedTrie<std::string, int64_t> hamt;
  for (int64_t i = 0; i < 2048; i++) {
    std::string s = std::to_string(i);
    REQUIRE(hamt.count(s) == 0);
    hamt[s] = i * 10;
    REQUIRE(hamt.find(s)->second == i * 10);
    REQUIRE(hamt.count(s) == 1);
    // check size
    REQUIRE(hamt.size() == i + 1);
  }
  hamt.clear();
  for (int64_t i = 0; i < 2048; i++) {
    std::string s = std::to_string(i);
    REQUIRE(hamt[s] == 0);
    hamt[s] = i * 10;
    REQUIRE(hamt[s] == i * 10);
    // check size
    REQUIRE(hamt.size() == i + 1);
  }
}

TEST_CASE("InsertDoesntCopyUnnecessarily", "[HAMT]") {
  foc::HashArrayMappedTrie<Constructable, Constructable> hamt;
  REQUIRE(Constructable::getNumConstructorCalls() == 0);
  REQUIRE(Constructable::getNumDestructorCalls() == 0);

  const Constructable one(1);
  const Constructable two(2);
  Constructable key(one);
  REQUIRE(Constructable::getNumConstructorCalls() == 3);
  Constructable::reset();

  std::pair<const Constructable, Constructable> entry(key, one);
  std::pair<const Constructable, Constructable> new_entry(key, one);
  REQUIRE(Constructable::getNumCopyConstructorCalls() == 4);
  Constructable::reset();

  // insert(const value_type&)
  hamt.insert(entry);
  REQUIRE(Constructable::getNumConstructorCalls() == 2);  // key and value constructed in entry
  REQUIRE(Constructable::getNumMoveConstructorCalls() == 0);
  REQUIRE(Constructable::getNumCopyConstructorCalls() == 2);
  REQUIRE(Constructable::getNumDestructorCalls() == 0);
  REQUIRE(Constructable::getNumAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumMoveAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumCopyAssignmentCalls() == 0);
  Constructable::reset();

  hamt.insert(new_entry);
  REQUIRE(Constructable::getNumConstructorCalls() == 0);  // key already in map, nothing was copied
  REQUIRE(Constructable::getNumMoveConstructorCalls() == 0);
  REQUIRE(Constructable::getNumCopyConstructorCalls() == 0);
  REQUIRE(Constructable::getNumDestructorCalls() == 0);
  REQUIRE(Constructable::getNumAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumMoveAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumCopyAssignmentCalls() == 0);
  hamt.clear();
  Constructable::reset();

  // put(const value_type&)
  hamt.put(entry);
  REQUIRE(Constructable::getNumConstructorCalls() == 2);  // key and value constructed in entry
  REQUIRE(Constructable::getNumMoveConstructorCalls() == 0);
  REQUIRE(Constructable::getNumCopyConstructorCalls() == 2);
  REQUIRE(Constructable::getNumDestructorCalls() == 0);
  REQUIRE(Constructable::getNumAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumMoveAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumCopyAssignmentCalls() == 0);
  Constructable::reset();

  hamt.put(new_entry);
  REQUIRE(Constructable::getNumConstructorCalls() == 0);
  REQUIRE(Constructable::getNumMoveConstructorCalls() == 0);
  REQUIRE(Constructable::getNumCopyConstructorCalls() == 0);
  REQUIRE(Constructable::getNumDestructorCalls() == 0);
  REQUIRE(Constructable::getNumAssignmentCalls() == 1);  // only value was assigned into the map
  REQUIRE(Constructable::getNumMoveAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumCopyAssignmentCalls() == 1);
  hamt.clear();
  Constructable::reset();

  // put(value_type &&)
  hamt.put(std::move(entry));
  REQUIRE(Constructable::getNumConstructorCalls() == 2);  // key and value were moved into the map
  REQUIRE(Constructable::getNumMoveConstructorCalls() == 2);
  REQUIRE(Constructable::getNumCopyConstructorCalls() == 0);
  REQUIRE(Constructable::getNumDestructorCalls() == 0);
  REQUIRE(Constructable::getNumAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumMoveAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumCopyAssignmentCalls() == 0);
  Constructable::reset();

  hamt.put(std::move(new_entry));
  REQUIRE(Constructable::getNumConstructorCalls() == 0);
  REQUIRE(Constructable::getNumMoveConstructorCalls() == 0);
  REQUIRE(Constructable::getNumCopyConstructorCalls() == 0);
  REQUIRE(Constructable::getNumDestructorCalls() == 0);
  REQUIRE(Constructable::getNumAssignmentCalls() == 1);
  REQUIRE(Constructable::getNumMoveAssignmentCalls() ==
          1);  // only value was move-assigned into the map
  REQUIRE(Constructable::getNumCopyAssignmentCalls() == 0);
  hamt.clear();
  Constructable::reset();

  // operator[](const Key&)
  auto ivalue = hamt[entry.first].getValue();
  REQUIRE(ivalue == 0);
  REQUIRE(Constructable::getNumConstructorCalls() == 2);  // key copy, default value constructor
  REQUIRE(Constructable::getNumMoveConstructorCalls() == 0);
  REQUIRE(Constructable::getNumCopyConstructorCalls() == 1);
  REQUIRE(Constructable::getNumDestructorCalls() == 0);
  REQUIRE(Constructable::getNumAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumMoveAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumCopyAssignmentCalls() == 0);

  hamt[new_entry.first] = new_entry.second;
  REQUIRE(Constructable::getNumConstructorCalls() == 2);  // key copy, default value constructor
  REQUIRE(Constructable::getNumMoveConstructorCalls() == 0);
  REQUIRE(Constructable::getNumCopyConstructorCalls() == 1);
  REQUIRE(Constructable::getNumDestructorCalls() == 0);
  REQUIRE(Constructable::getNumAssignmentCalls() == 1);
  REQUIRE(Constructable::getNumMoveAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumCopyAssignmentCalls() == 1);  // assignment
  hamt.clear();
  Constructable::reset();

  // operatorp[](Key &&)
  ivalue = hamt[std::move(key)].getValue();
  REQUIRE(ivalue == 0);
  REQUIRE(Constructable::getNumConstructorCalls() == 2);  // key move, default value constructor
  REQUIRE(Constructable::getNumMoveConstructorCalls() == 1);
  REQUIRE(Constructable::getNumCopyConstructorCalls() == 0);
  REQUIRE(Constructable::getNumDestructorCalls() == 0);
  REQUIRE(Constructable::getNumAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumMoveAssignmentCalls() == 0);
  REQUIRE(Constructable::getNumCopyAssignmentCalls() == 0);

  hamt[std::move(key)] = std::move(new_entry.second);
  REQUIRE(Constructable::getNumConstructorCalls() == 2);  // key move, default value constructor
  REQUIRE(Constructable::getNumMoveConstructorCalls() == 1);
  REQUIRE(Constructable::getNumCopyConstructorCalls() == 0);
  REQUIRE(Constructable::getNumDestructorCalls() == 0);
  REQUIRE(Constructable::getNumAssignmentCalls() == 1);  // value assignment
  REQUIRE(Constructable::getNumMoveAssignmentCalls() == 1);
  REQUIRE(Constructable::getNumCopyAssignmentCalls() == 0);
  hamt.clear();
  Constructable::reset();
}

TEST_CASE("CountTest", "[HAMT]") {
  HAMT hamt;
  REQUIRE(hamt.count(0) == 0);
  REQUIRE(hamt.count(1) == 0);
  hamt.insert(std::make_pair(0, 0));
  REQUIRE(hamt.count(0) == 1);
  hamt.insert(std::make_pair(1, 1));
  REQUIRE(hamt.count(0) == 1);
  REQUIRE(hamt.count(1) == 1);
  for (int64_t i = 2; i < 2048; i++) {
    REQUIRE(hamt.count(i) == 0);
    hamt.insert(std::make_pair(i, i));
    REQUIRE(hamt.count(i) == 1);
  }
}

TEST_CASE("EqualRangeTest", "[HAMT]") {
  HAMT hamt;
  for (int64_t i = 0; i < 2048; i++) {
    hamt[i] = i * 10;
  }

  auto range = hamt.equal_range(4);
  int64_t sum = 0;
  for (auto it = range.first; it != range.second; it++) {
    sum += it->second;
  }
  REQUIRE(sum == 40);

  const auto &h = hamt;
  auto const_range = h.equal_range(5);
  sum = 0;
  for (auto it = const_range.first; it != const_range.second; it++) {
    sum += it->second;
  }
  REQUIRE(sum == 50);
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
  // Lookups fail when a constant hash function is used
  parent_test<HAMT>(64);
}

TEST_CASE("PhysicalIndexOfNodeInTrieWithAParent", "[HAMT]") {
  using HAMT = foc::HashArrayMappedTrie<int64_t, int64_t, IdentityFunction>;
  HAMT hamt;
  HAMT::Node *root = &hamt._root;
  for (int64_t i = 31; i >= 0; i--) {
    hamt.put(i, i);
  }
  for (uint32_t i = 0; i < 32; i++) {
    auto physical_index = root->asTrie().physicalIndex(i);
    HAMT::Node *logical_node = &root->asTrie().logicalGet(i);
    HAMT::Node *physical_node = &root->asTrie().physicalGet(physical_index);
    REQUIRE(logical_node == physical_node);
    REQUIRE(logical_node->parent() == root);
    REQUIRE(root->asTrie().physicalIndexOf(logical_node) == physical_index);
  }
}

TEST_CASE("ConstIteratorTest", "[HAMT]") {
  HAMT empty_hamt;
  const auto &const_empty_hamt = empty_hamt;
  REQUIRE(empty_hamt.size() == 0);

  int64_t checksum = 0;
  HAMT non_empty_hamt;
  for (int64_t i = 0; i < 10000; i++) {
    non_empty_hamt.put(i, i);
    checksum += i;
  }
  const auto &const_non_empty_hamt = non_empty_hamt;

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
    REQUIRE(it->first == 3233);
    REQUIRE(it->second == 3233);
  }
  {
    HAMT::const_iterator it = const_non_empty_hamt.begin();
    REQUIRE(it->first == 3233);
    REQUIRE(it->second == 3233);
  }
  {
    HAMT::const_iterator it = const_non_empty_hamt.cbegin();
    REQUIRE(it->first == 3233);
    REQUIRE(it->second == 3233);
  }

  int64_t sum_keys = 0;
  int64_t sum_values = 0;
  size_t count_items = 0;
  for (auto it = non_empty_hamt.begin(); it != non_empty_hamt.end(); it++) {
    count_items++;
    sum_keys += it->first;
    sum_values += it->second;
  }
  REQUIRE(count_items == 10000);
  REQUIRE(sum_keys == checksum);
  REQUIRE(sum_values == checksum);
}
