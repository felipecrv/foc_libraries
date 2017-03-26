// Hash Array Mapped Trie
//
// An implementation of Phil Bagwell's Hash Array Mapped Trie.
//
// "Ideal Hash Trees". Phil Bagwell. 2001.
// http://infoscience.epfl.ch/record/64398

#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <string>
#include <utility>
#include <functional>
#include <initializer_list>

#include "support.h"

#ifndef PUBLIC_IN_GTEST
# ifdef GTEST
#  define PUBLIC_IN_GTEST public
# else
#  define PUBLIC_IN_GTEST private
# endif
#endif

// This needs to be a per-execution seed to avoid denial-of-service attacks
// and you should not rely on the same hashes being generated on different
// runs of the program.
//
// Users of this library can define this macro before including the file to be
// any expression (e.g. a function call) that returns a 64-bit seed.
#ifndef FOC_GET_HASH_SEED
# define FOC_GET_HASH_SEED 0xff51afd7ed558ccdULL
#endif

namespace foc {

namespace detail {

uint32_t hamt_allocation_size(uint32_t required, uint32_t generation, uint32_t level);

template<class Entry, class Allocator>
class NodeTemplate;

// The root of a trie that can contain up to 32 Nodes. A bitmap is used
// to compress the array as decribed in the paper.
template<class Entry, class Allocator>
class BitmapTrieTemplate {
 PUBLIC_IN_GTEST:
  using Node = NodeTemplate<Entry, Allocator>;

  uint32_t _bitmap;
  uint32_t _capacity;
  Node *_base;

  uint32_t physicalIndex(uint32_t logical_index) const {
    assert(logical_index < 32);
    uint32_t _bitmask = 0x1 << logical_index;
    return __builtin_popcount(_bitmap & (_bitmask - 1));
  }

 public:
  Node *Initialize(Allocator &allocator, uint32_t capacity, uint32_t bitmap);

  uint32_t size() const { return __builtin_popcount(_bitmap); }
  uint32_t capacity() const { return _capacity; }
  Node &physicalGet(uint32_t i) { return _base[i]; }
  Node &logicalGet(uint32_t i) { return _base[physicalIndex(i)]; }

  bool logicalPositionTaken(uint32_t logical_index) const {
    assert(logical_index < 32);
    return _bitmap & (0x1 << logical_index);
  }

  Node *insert(Allocator &allocator, int logical_index, Entry &new_entry, uint32_t generation, uint32_t level);
};

// A Node in the HAMT is a sum type of Entry and BitmapTrie (can be one or the other).
template<class Entry, class Allocator>
class NodeTemplate {
 private:
  bool _is_entry;
  union {
    struct {
      alignas(alignof(Entry)) char buffer[sizeof(Entry)];
    } entry;
    BitmapTrieTemplate<Entry, Allocator> trie;
  } _either;

 public:
  NodeTemplate *BitmapTrie(Allocator &allocator, uint32_t capacity, uint32_t bitmap) {
    _is_entry = false;
    return _either.trie.Initialize(allocator, capacity, bitmap);
  }

  NodeTemplate& operator=(NodeTemplate&& other) {
    _is_entry = other._is_entry;
    if (_is_entry) {
      Entry &rhs = *reinterpret_cast<Entry *>(&other._either.entry);
      new (&_either.entry) Entry(std::move(rhs));
    } else {
      _either.trie = other._either.trie;
    }
    return *this;
  }

  NodeTemplate(Entry &&entry) : _is_entry(true) { new (&_either.entry) Entry(entry); }

  NodeTemplate& operator=(Entry&& other) {
    _is_entry = true;
    new (&_either.entry) Entry(other);
    return *this;
  }

  bool isEntry() const { return _is_entry; }

  Entry& asEntry() {
    assert(_is_entry && "Node should be an entry");
    return *reinterpret_cast<Entry *>(&_either.entry);
  }

  BitmapTrieTemplate<Entry, Allocator>& asTrie() {
    assert(!_is_entry && "Node should be a trie");
    return _either.trie;
  }
};

}  // namespace detail

class MallocAllocator {
 public:
  void *allocate(size_t size, size_t) { return malloc(size); }
  void deallocate(void *ptr, size_t) { free(ptr); }
};

template<
  class Key,
  class T,
  class Hash = std::hash<Key>,
  class KeyEqual = std::equal_to<Key>,
  class Allocator = MallocAllocator>
class HashArrayMappedTrie {
 PUBLIC_IN_GTEST:
  using Entry = std::pair<Key, T>;
  using BitmapTrie = detail::BitmapTrieTemplate<Entry, Allocator>;
  using Node = detail::NodeTemplate<Entry, Allocator>;

 public:
  // Some std::unordered_map member types.
  //
  // Notice that our Allocator is nothing like any std allocator!
  // Because of that, size_type is defined as `size_t` and I didn't
  // bother defining some allocator related types that exist in
  // std::unordered_map (pointer, const_pointer...).
  typedef Key                                       key_type;
  typedef T                                         mapped_type;
  typedef Hash                                      hasher;
  typedef KeyEqual                                  key_equal;
  typedef Allocator                                 allocator_type;
  typedef std::pair<const Key, T>                   value_type;
  typedef value_type&                               reference;
  typedef const value_type&                         const_reference;
  /* typedef HashArrayMappedTrieForwardIterator        iterator; */
  /* typedef const HashArrayMappedTrieForwardIterator  const_iterator; */

  size_t _count;
  size_t _expected_count;
  uint32_t _generation;
  uint32_t _seed;
  BitmapTrie _root;
  Hash _hasher;
  KeyEqual _key_equal;
  Allocator _allocator;

 public:
   HashArrayMappedTrie() : HashArrayMappedTrie(1) {} // TODO: change to 0?

  explicit HashArrayMappedTrie(
      size_t n,
      const hasher& hf = hasher(),
      const key_equal& eql = key_equal(),
      const allocator_type &a = allocator_type());
  /*explicit HashArrayMappedTrie(const allocator_type&);
  HashArrayMappedTrie(const HashArrayMappedTrie&);
  HashArrayMappedTrie(const HashArrayMappedTrie&, const allocator_type&);
  HashArrayMappedTrie(HashArrayMappedTrie&&);
  HashArrayMappedTrie(HashArrayMappedTrie&&, const allocator_type&);
  HashArrayMappedTrie(
      initializer_list<value_type>,
      size_t n = 0,
      const hasher& hf = hasher(),
      const key_equal& eql = key_equal(),
      const allocator_type& a = allocator_type());
      */

  // C++14
  /*
  HashArrayMappedTrie(size_t n, const allocator_type& a)
    : HashArrayMappedTrie(n, hasher(), key_equal(), a) {}
  HashArrayMappedTrie(size_t n, const hasher& hf, const allocator_type& a)
    : HashArrayMappedTrie(n, hf, key_equal(), a) {}
  */
  /*
  template <class InputIterator>
  HashArrayMappedTrie(InputIterator f, InputIterator l, size_t n, const allocator_type& a)
    : HashArrayMappedTrie(f, l, n, hasher(), key_equal(), a) {}
  template <class InputIterator>
  HashArrayMappedTrie(InputIterator f, InputIterator l, size_t n, const hasher& hf, const allocator_type& a)
    : HashArrayMappedTrie(f, l, n, hf, key_equal(), a) {}
  */
  /*
  HashArrayMappedTrie(initializer_list<value_type> il, size_t n, const allocator_type& a)
    : HashArrayMappedTrie(il, n, hasher(), key_equal(), a) {}
  HashArrayMappedTrie(initializer_list<value_type> il, size_t n, const hasher& hf, const allocator_type& a)
    : HashArrayMappedTrie(il, n, hf, key_equal(), a) {}
  */

  /*
  ~HashArrayMappedTrie();
  HashArrayMappedTrie& operator=(const HashArrayMappedTrie&);
  HashArrayMappedTrie& operator=(HashArrayMappedTrie&&);
  HashArrayMappedTrie& operator=(initializer_list<value_type>);
  */

  size_t size() const { return _count; }
  bool empty() const { return _count == 0; }

  bool insert(const Key& key, const T& value) {
    uint32_t hash = hash32(key, _seed);

    if (_count >= _expected_count) {
      _expected_count *= 2;
      _generation++;
    }
    Entry entry(key, value);
    Node *node_ptr = insert(&_root, entry, _seed, hash, 0, 0);
    _count++;

    return node_ptr != nullptr;
  }

  const T *find(const Key& key) {
    BitmapTrie *trie = &_root;
    uint32_t seed = _seed;
    uint32_t hash = hash32(key, _seed);
    uint32_t hash_offset = 0;
    uint32_t t = hash & 0x1f;

    while (trie->logicalPositionTaken(t)) {
      auto& node = trie->logicalGet(t);
      if (node.isEntry()) {
        auto& entry = node.asEntry();
        // Keys match!
        if (_key_equal(entry.first, key)) {
          return &entry.second;
        }
        /* printf("%d -> %d\n", key, hash); */
        /* printf("%d -> %d\n", entry.first, hash32(entry.second, seed)); */
        return nullptr;
      }

      // The position stores a trie. Keep searching.

      if (LIKELY(hash_offset < 25)) {
        hash_offset += 5;
      } else {
        hash_offset = 0;
        seed++;
        hash = hash32(key, seed);
      }

      trie = &node.asTrie();
      t = (hash >> hash_offset) & 0x1f;
    }

    return nullptr;
  }

  Node *insert(
      BitmapTrie *trie,
      Entry &new_entry,
      uint32_t seed,
      uint32_t hash,
      uint32_t hash_offset,
      uint32_t level) {
    uint32_t t = (hash >> hash_offset) & 0x1f;

    if (trie->logicalPositionTaken(t)) {
      Node *node = &trie->logicalGet(t);
      if (node->isEntry()) {
        Entry *entry = &node->asEntry();
        if (_key_equal(entry->first, new_entry.first)) {
          // Keys match! Override the value.
          entry->second = std::move(new_entry.second);
          return node;
        }

        // Has to replace the entry with a trie, move this entry there and
        // insert the new entry as well.

        if (LIKELY(hash_offset < 25)) {
          hash_offset += 5;
        } else {
          hash_offset = 0;
          seed++;
          hash = hash32(new_entry.first, seed);
        }

        return branchOff(node, new_entry, seed, hash, hash_offset, level + 1);
      }

      if (LIKELY(hash_offset < 25)) {
        hash_offset += 5;
      } else {
        hash_offset = 0;
        seed++;
        hash = hash32(new_entry.first, seed);
      }

      // The position stores a trie. Delegate insertion.
      return insert(&node->asTrie(), new_entry, seed, hash, hash_offset, level + 1);
    }

    return trie->insert(_allocator, t, new_entry, _generation, level);
  }

 private:
  Node *allocateBaseTrieArray(size_t capacity) {
    void *ptr = _allocator.allocate(capacity * sizeof(Node), alignof(Node));
    return static_cast<Node *>(ptr);
  }

  uint32_t hash32(const Key &key, uint32_t seed) const {
    uint32_t hash = _hasher(key);
    return hash ^ seed;
  }

  // This function will replace the node contents with a trie to
  // make more room for (key, value).
  Node *branchOff(
      Node *old_node,
      Entry &new_entry,
      uint32_t seed,
      uint32_t hash,
      uint32_t hash_offset,
      uint32_t level) {
    uint32_t old_entry_hash = hash32(old_node->asEntry().first, seed);
    // Make sure a collision happened at the parent trie
    if (hash_offset >= 5) {
      assert(((old_entry_hash >> (hash_offset - 5)) & 0x1f) ==
             ((hash >> (hash_offset - 5)) & 0x1f) &&
             "BranchOff should be used when collisions are found");
      assert(old_node->isEntry() && "old_node should be an entry");
    }

    uint32_t old_entry_hash_slice = (old_entry_hash >> hash_offset) & 0x1f;
    uint32_t hash_slice = (hash >> hash_offset) & 0x1f;

    Entry old_entry = std::move(old_node->asEntry());

    if (UNLIKELY(old_entry_hash_slice == hash_slice)) {
      old_node->BitmapTrie(_allocator, 1, 0x1 << hash_slice);

      if (LIKELY(hash_offset < 25)) {
        hash_offset += 5;
      } else {
        hash_offset = 0;
        seed++;
        old_entry_hash = hash32(old_entry.first, seed);
        hash = hash32(new_entry.first, seed);
      }

      // We guess that the two entries will be stored in a single node,
      // so we initialize it with capacity for 2.
      old_node->asTrie().physicalGet(0).BitmapTrie(_allocator, 2, 0);

      BitmapTrie *trie = &old_node->asTrie().physicalGet(0).asTrie();
      insert(trie, old_entry, seed, old_entry_hash, hash_offset, level + 1);
      return insert(trie, new_entry, seed, hash, hash_offset, level + 1);
    }

    old_node->BitmapTrie(_allocator, 2, (0x1 << old_entry_hash_slice) | (0x1 << hash_slice));

    auto& new_trie = old_node->asTrie();
    if (old_entry_hash_slice < hash_slice) {
      new_trie.physicalGet(0) = std::move(old_entry);
      return new (&new_trie.physicalGet(1)) Node(std::move(new_entry));
    } else {
      Node *node_ptr = &new_trie.physicalGet(0);
      new (node_ptr) Node(std::move(new_entry));
      new_trie.physicalGet(1) = std::move(old_entry);

      return node_ptr;
    }
  }

#ifdef GTEST
 PUBLIC_IN_GTEST:
  BitmapTrie &root() { return _root; }

  size_t countInnerNodes(BitmapTrie &trie) {
    size_t inner_nodes_count = 0;

    for (uint32_t i = 0; i < trie.size(); i++) {
      Node &node = trie.physicalGet(i);
      if (!node.isEntry()) {
        inner_nodes_count += 1 + countInnerNodes(node.asTrie());
      }
    }

    return inner_nodes_count;
  }

  void print() {
    /* size_t inner_nodes_count = countInnerNodes(_root); */
    /* _allocator.dump(_count, inner_nodes_count); */
    /* printf("%zu, %zu, %zu\n", _count, _allocator._total_allocated, _allocator._total_in_free_lists); */
  }
#endif  // GTEST
};

template<class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(
    size_t n, const hasher& hf, const key_equal& eql, const allocator_type &a)
  : _count(0), _hasher(hf), _key_equal(eql), _allocator(a) {

  _seed = static_cast<uint32_t>(FOC_GET_HASH_SEED);
  _expected_count = (n > 0) ? next_power_of_2(n - 1) : 1;
  _generation = 0; // FIX: log2 _expected_count

  uint32_t alloc_size = detail::hamt_allocation_size(1, _generation, 0);
  assert(alloc_size >= 1);
  _root.Initialize(_allocator, alloc_size, 0);
}

namespace detail {

uint32_t hamt_allocation_size(uint32_t required, uint32_t generation, uint32_t level) {
  assert(required <= 32);
  // [level][generation]
  const static uint32_t alloc_sizes_by_level[][23] = {
    // 1  2  4  8  16  32  64  128 256  512 1024 2048 4096 8192 16384 32768 65536 2^17 2^18 2^19 2^20 2^21 2^22
    {  2, 3, 5, 8, 13, 21, 29, 32,  32, 32,  32,  32,  32,  32,   32,   32,   32,  32,  32,  32,  32,  32,  32},
    {  1, 1, 1, 1,  1,  2,  3,  5,   8, 13,  21,  29,  32,  32,   32,   32,   32,  32,  32,  32,  32,  32,  32},
    {  1, 1, 1, 1,  1,  1,  1,  1,   1,  1,   2,   3,   5,   8,   13,   21,   29,  32,  32,  32,  32,  32,  32},
    {  1, 1, 1, 1,  1,  1,  1,  1,   1,  1,   1,   1,   1,   1,    1,    2,    3,   5,   8,  13,  21,  29,  32},
    {  1, 1, 1, 1,  1,  1,  1,  1,   1,  1,   1,   1,   1,   1,    1,    1,    1,   1,   1,   1,   1,   1,   1},
  };
  const static uint32_t alloc_sizes[33] = {
    // 0  1  2  3  4  5  6  7  8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31  32
       1, 1, 2, 3, 5, 5, 8, 8, 8, 13, 13, 13, 13, 13, 21, 21, 21, 21, 21, 21, 21, 21, 29, 29, 29, 29, 29, 29, 29, 29, 32, 32, 32
  };

  if (generation > 22) {
    generation = 22;
  }
  if (level > 4) {
    level = 4;
  }
  uint32_t guess = alloc_sizes_by_level[level][generation];
  if (required > guess) {
    return alloc_sizes[required];
  }
  return guess;
}

// BitmapTrie

template<class Entry, class Allocator>
NodeTemplate<Entry, Allocator> *BitmapTrieTemplate<Entry, Allocator>::insert(
  Allocator &allocator, int logical_index, Entry &new_entry, uint32_t generation, uint32_t level) {
  const uint32_t i = physicalIndex(logical_index);
  const uint32_t sz = this->size();

  uint32_t required = sz + 1;
  assert(required <= 32);
  if (required > _capacity) {
    size_t alloc_size = hamt_allocation_size(required, generation, level);

    if (UNLIKELY(_base == nullptr)) {
      assert(i == 0);
      _base = static_cast<Node *>(allocator.allocate(alloc_size * sizeof(Node), alignof(Node)));
      if (!_base) {
        return nullptr;
      }
      _capacity = alloc_size;
    } else {
      Node *new_base =
        static_cast<Node *>(allocator.allocate(alloc_size * sizeof(Node), alignof(Node)));
      if (!new_base) {
        return nullptr;
      }

      for (uint32_t j = 0; j < i; j++) {
        new_base[j] = std::move(_base[j]);
      }
      for (uint32_t j = i + 1; j <= sz; j++) {
        new_base[j] = std::move(_base[j - 1]);
      }

      allocator.deallocate(_base, _capacity);
      _base = new_base;
      _capacity = alloc_size;
    }
  } else {
    for (int32_t j = (int32_t)sz; j > (int32_t)i; j--) {
      _base[j] = std::move(_base[j - 1]);
    }
  }

  // Mark position as used
  assert((_bitmap & (0x1 << logical_index)) == 0 && "Logical index should be empty");
  _bitmap |= 0x1 << logical_index;

  // Insert at allocated position
  new (&_base[i]) Node(std::move(new_entry));

  return &_base[i];
}

// Node

template<class Entry, class Allocator>
NodeTemplate<Entry, Allocator> *BitmapTrieTemplate<Entry, Allocator>::Initialize(
    Allocator &allocator, uint32_t capacity, uint32_t bitmap) {
  _capacity = capacity;
  _bitmap = bitmap;
  _base = static_cast<Node *>(allocator.allocate(capacity * sizeof(Node), alignof(Node)));
  return _base;
}

}  // namespace detail

}  // namespace foc
