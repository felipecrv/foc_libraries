// Hash Array Mapped Trie
//
// An implementation of Phil Bagwell's Hash Array Mapped Trie.
//
// "Ideal Hash Trees". Phil Bagwell. 2001.
// http://infoscience.epfl.ch/record/64398
#pragma once

#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <string>
#include <utility>
#include <functional>
#include <initializer_list>
#include <stack>
#include <iterator>

#include "support.h"
#include "allocator.h"

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
  ATTRIBUTE_ALWAYS_INLINE
  Node *allocate(Allocator &allocator, uint32_t capacity);

  ATTRIBUTE_ALWAYS_INLINE
  void deallocate(Allocator &allocator);

  void deallocateRecursively(Allocator&) noexcept;
  void cloneRecursively(Allocator &, BitmapTrieTemplate &root);

  void clear(Allocator &allocator) {
    deallocateRecursively(allocator);
    _bitmap = 0;
    _capacity = 0;
    _base = nullptr;
  }

  // We allow the object to be uninitialized because we want to keep it inside a union.
  // Users of this class should call allocate and deallocate correctly.
  BitmapTrieTemplate() = default;
  BitmapTrieTemplate(BitmapTrieTemplate &&other) = default;

  void swap(BitmapTrieTemplate& other) {
    std::swap(_bitmap, other._bitmap);
    std::swap(_capacity, other._capacity);
    std::swap(_base, other._base);
  }

  uint32_t size() const { return __builtin_popcount(_bitmap); }
  uint32_t capacity() const { return _capacity; }
  Node &physicalGet(uint32_t i) { return _base[i]; }
  const Node &physicalGet(uint32_t i) const { return _base[i]; }
  Node &logicalGet(uint32_t i) { return _base[physicalIndex(i)]; }
  const Node &logicalGet(uint32_t i) const { return _base[physicalIndex(i)]; }

  bool logicalPositionTaken(uint32_t logical_index) const {
    assert(logical_index < 32);
    return _bitmap & (0x1 << logical_index);
  }

  Node *insert(Allocator&, int logical_index, Entry &, Node *parent, uint32_t generation, uint32_t level);
  Node *insertTrie(Allocator&, Node *parent, int logical_index, uint32_t capacity);
};

// A Node in the HAMT is a sum type of Entry and BitmapTrie (i.e. can be one or the other).
template<class Entry, class Allocator>
class NodeTemplate {
 private:
  using _BitmapTrie = BitmapTrieTemplate<Entry, Allocator>;

  NodeTemplate *_parent;
  union {
    struct {
      alignas(alignof(Entry)) char buffer[sizeof(Entry)];
    } entry;
    _BitmapTrie trie;
  } _either;

 public:
  NodeTemplate(NodeTemplate *parent) { BitmapTrie(parent); }

  ATTRIBUTE_ALWAYS_INLINE
  NodeTemplate *BitmapTrie(NodeTemplate *parent);

  ATTRIBUTE_ALWAYS_INLINE
  NodeTemplate *BitmapTrie(Allocator &allocator, NodeTemplate *parent, uint32_t capacity);

  NodeTemplate& operator=(NodeTemplate&& other);
  NodeTemplate(Entry &&entry, NodeTemplate *parent);
  NodeTemplate& operator=(Entry&& other);

  bool isEntry() const { return (uintptr_t)_parent & (uintptr_t)0x1U; }
  bool isTrie() const { return !isEntry(); }

  NodeTemplate *parent() const {
    return (NodeTemplate *)((uintptr_t)_parent & ~(uintptr_t)0x1U);
  }

  Entry& asEntry() {
    assert(isEntry() && "Node should be an entry");
    return *reinterpret_cast<Entry *>(&_either.entry);
  }

  const Entry& asEntry() const {
    assert(isEntry() && "Node should be an entry");
    return *reinterpret_cast<const Entry *>(&_either.entry);
  }

  _BitmapTrie& asTrie() {
    assert(isTrie() && "Node should be a trie");
    return _either.trie;
  }

  const _BitmapTrie& asTrie() const {
    assert(isTrie() && "Node should be a trie");
    return _either.trie;
  }

  NodeTemplate *nextEntryNode() {
    // TODO: fix
    return this;
  }
};

}  // namespace detail

template<class Entry, class Allocator>
class HashArrayMappedTrieForwardIterator {
 private:
   detail::NodeTemplate<Entry, Allocator> *_node;

 public:
  typedef std::forward_iterator_tag                        iterator_category;
  typedef Entry                                   value_type;
  /* typedef typename _NodeTypes::difference_type             difference_type; */
  typedef value_type&                                      reference;
  typedef Entry*                    pointer;

  ATTRIBUTE_ALWAYS_INLINE HashArrayMappedTrieForwardIterator() = default;

  ATTRIBUTE_ALWAYS_INLINE
  HashArrayMappedTrieForwardIterator(detail::NodeTemplate<Entry, Allocator> *node) noexcept
    : _node(node) {}

  ATTRIBUTE_ALWAYS_INLINE reference operator*() const { return _node->asEntry(); }
  ATTRIBUTE_ALWAYS_INLINE pointer operator->() const { return &_node->asEntry(); }

  ATTRIBUTE_ALWAYS_INLINE HashArrayMappedTrieForwardIterator& operator++() {
    _node = _node->nextEntryNode();
    return *this;
  }

  ATTRIBUTE_ALWAYS_INLINE HashArrayMappedTrieForwardIterator operator++(int) {
    HashArrayMappedTrieForwardIterator _this(_node);
    _node = _node->nextEntryNode();
    return _this;
  }

  friend ATTRIBUTE_ALWAYS_INLINE bool operator==(
      const HashArrayMappedTrieForwardIterator& x,
      const HashArrayMappedTrieForwardIterator& y) {
    return x._node == y._node;
  }

  friend ATTRIBUTE_ALWAYS_INLINE bool operator!=(
      const HashArrayMappedTrieForwardIterator& x,
      const HashArrayMappedTrieForwardIterator& y) {
    return x._node != y._node;
  }

  template <class, class, class, class, class> friend class HashArrayMappedTrie;
  template <class, class> friend class NodeTemplate;
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
  typedef Key                                                   key_type;
  typedef T                                                     mapped_type;
  typedef Hash                                                  hasher;
  typedef KeyEqual                                              key_equal;
  typedef Allocator                                             allocator_type;
  typedef std::pair<const Key, T>                               value_type;
  typedef value_type&                                           reference;
  typedef const value_type&                                     const_reference;
  typedef HashArrayMappedTrieForwardIterator<Entry, Allocator>  iterator;
  /* typedef const HashArrayMappedTrieForwardIterator  const_iterator; */

  size_t _count;
  size_t _expected_count;
  uint32_t _generation;
  uint32_t _seed;
  Node _root;
  Hash _hasher;
  KeyEqual _key_equal;
  Allocator _allocator;

 public:
  ATTRIBUTE_ALWAYS_INLINE
  HashArrayMappedTrie() : HashArrayMappedTrie(1) {} // TODO: change to 0?

  explicit HashArrayMappedTrie(
      size_t n,
      const hasher& hf = hasher(),
      const key_equal& eql = key_equal(),
      const allocator_type &a = allocator_type());

  explicit HashArrayMappedTrie(const allocator_type& allocator);

  HashArrayMappedTrie(const HashArrayMappedTrie&);
  HashArrayMappedTrie(const HashArrayMappedTrie&, const allocator_type&);

  HashArrayMappedTrie(HashArrayMappedTrie&& other);

  HashArrayMappedTrie(HashArrayMappedTrie&&, const allocator_type&);
  /*HashArrayMappedTrie(
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

  ~HashArrayMappedTrie() { _root.asTrie().deallocateRecursively(_allocator); }

  HashArrayMappedTrie& operator=(const HashArrayMappedTrie& other) {
    if (this != &other) {
      _root.asTrie().deallocateRecursively(_allocator);
      _count = other._count;
      _expected_count = other._expected_count;
      _generation = other._generation;
      _seed = other._seed;
      _root = other._root;
      _hasher = other._hasher;
      _key_equal = other._key_equal;
      _allocator = other._allocator; // TODO: can copy allocator?
      _root.cloneRecursively(_allocator, other._root);
    }
    return *this;
  }

  HashArrayMappedTrie& operator=(HashArrayMappedTrie&& other) {
    if (this != &other) {
      _root.asTrie().deallocateRecursively(_allocator);
      _count = other._count;
      _expected_count = other._expected_count;
      _generation = other._generation;
      _seed = other._seed;
      _root = std::move(other._root);
      _hasher = std::move(other._hasher);
      _key_equal = std::move(other._key_equal);
      _allocator = std::move(other._allocator); // TODO: can copy allocator?
    }
    return *this;
  }

  /* HashArrayMappedTrie& operator=(initializer_list<value_type>); */

  allocator_type get_allocator() const { return _allocator; }

  bool empty() const { return _count == 0; }
  size_t size() const { return _count; }
  // We don't implement max_size()

  // template <class... Args> pair<iterator, bool> emplace(Args&&... args);
  // template <class... Args> iterator emplace_hint(const_iterator position, Args&&... args);

  iterator insert(const value_type& obj) {
    insert(obj.first, obj.second);
  }

  /*
  template <class P> pair<iterator, bool> insert(P&& obj);
  iterator insert(const_iterator hint, const value_type& obj);
  template <class P> iterator insert(const_iterator hint, P&& obj);
  template <class InputIterator> void insert(InputIterator first, InputIterator last);
  void insert(initializer_list<value_type>);
  */

  void clear() {
    _count = 0;
    _expected_count = 1;
    _generation = 0;
    _root.asTrie().clear(_allocator);
  }

  // TODO: define out-of-line
  void swap(HashArrayMappedTrie& other) {
    std::swap(_count, other._count);
    std::swap(_expected_count, other._expected_count);
    std::swap(_generation, other._generation);
    std::swap(_seed, other._seed);
    std::swap(_hasher, other._hasher);
    std::swap(_key_equal, other._key_equal);
    std::swap(_allocator, other._allocator);
    _root.swap(other._root);
  }

  Node *insert(const Key& key, const T& value) {
    Entry entry(key, value);
    uint32_t hash = hash32(key, _seed);

    if (_count >= _expected_count) {
      _expected_count *= 2;
      _generation++;
    }
    Node *node = insert(&_root, entry, _seed, hash, 0, 0);
    _count++;

    return node;
  }

  const Node *findNode(const Key& key) {
    const BitmapTrie *trie = &_root.asTrie();
    uint32_t seed = _seed;
    uint32_t hash = hash32(key, _seed);
    uint32_t hash_offset = 0;
    uint32_t t = hash & 0x1f;

    while (trie->logicalPositionTaken(t)) {
      const Node *node = &trie->logicalGet(t);
      if (node->isEntry()) {
        const auto& entry = node->asEntry();
        // Keys match!
        if (_key_equal(entry.first, key)) {
          return node;
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

      trie = &node->asTrie();
      t = (hash >> hash_offset) & 0x1f;
    }

    return nullptr;
  }

  const T *find(const Key& key) {
    const Node *node = findNode(key);
    if (node) {
      return &node->asEntry().second;
    }
    return nullptr;
  }

  Node *insert(
      Node *trie_node,
      Entry &new_entry,
      uint32_t seed,
      uint32_t hash,
      uint32_t hash_offset,
      uint32_t level) {
    uint32_t t = (hash >> hash_offset) & 0x1f;

    BitmapTrie *trie = &trie_node->asTrie();
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
      return insert(node, new_entry, seed, hash, hash_offset, level + 1);
    }

    return trie->insert(_allocator, t, new_entry, trie_node, _generation, level);
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
      Node *node,
      Entry &new_entry,
      uint32_t seed,
      uint32_t hash,
      uint32_t hash_offset,
      uint32_t level) {
    uint32_t old_entry_hash = hash32(node->asEntry().first, seed);
    uint32_t old_entry_hash_slice = (old_entry_hash >> hash_offset) & 0x1f;
    uint32_t hash_slice = (hash >> hash_offset) & 0x1f;

    // Make sure a collision happened at the parent trie
    if (hash_offset >= 5) {
      assert(((old_entry_hash >> (hash_offset - 5)) & 0x1f) ==
             ((hash >> (hash_offset - 5)) & 0x1f) &&
             "BranchOff should be used when collisions are found");
    }

    // Copy the old entry inside the old node because the node will turn into a trie.
    Entry old_entry = std::move(node->asEntry());

    if (UNLIKELY(old_entry_hash_slice == hash_slice)) {
      // Turn the old node into a trie that will store another trie -- the child trie.
      Node *node_trie = node->BitmapTrie(_allocator, node->parent(), 1);
      // We guess that the two entries will be stored in a single trie,
      // so we initialize it with capacity for 2.
      Node *child_node = node_trie->asTrie().insertTrie(_allocator, /*parent*/node, hash_slice, 2);

      if (LIKELY(hash_offset < 25)) {
        hash_offset += 5;
      } else {
        hash_offset = 0;
        seed++;
        old_entry_hash = hash32(old_entry.first, seed);
        hash = hash32(new_entry.first, seed);
      }

      /*--*/ insert(child_node, old_entry, seed, old_entry_hash, hash_offset, level + 1);
      return insert(child_node, new_entry, seed,           hash, hash_offset, level + 1);
    }

    Node *ret;

    // Turn the old node into a trie to fit the old and the new entry
    // in the order that avoid moving the entry that's inserted first.
    auto new_trie_node = node->BitmapTrie(_allocator, node->parent(), 2);
    auto new_trie = &new_trie_node->asTrie();
    if (old_entry_hash_slice < hash_slice) {
      /*-*/ new_trie->insert(_allocator, old_entry_hash_slice, old_entry, new_trie_node, _generation, level);
      ret = new_trie->insert(_allocator,           hash_slice, new_entry, new_trie_node, _generation, level);
    } else {
      ret = new_trie->insert(_allocator,           hash_slice, new_entry, new_trie_node, _generation, level);
      /*-*/ new_trie->insert(_allocator, old_entry_hash_slice, old_entry, new_trie_node, _generation, level);
    }

    return ret;
  }

#ifdef GTEST
 PUBLIC_IN_GTEST:
  Node &root() { return _root; }

  size_t countInnerNodes(BitmapTrie &trie) {
    size_t inner_nodes_count = 0;

    for (uint32_t i = 0; i < trie.size(); i++) {
      Node &node = trie.physicalGet(i);
      if (node.isTrie()) {
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

// BitmapTrieTemplate {{{

template<class Entry, class Allocator>
NodeTemplate<Entry, Allocator> *BitmapTrieTemplate<Entry, Allocator>::insert(
  Allocator &allocator, int logical_index, Entry &new_entry, Node *parent, uint32_t generation, uint32_t level) {
  const uint32_t i = physicalIndex(logical_index);
  const uint32_t sz = this->size();

  uint32_t required = sz + 1;
  assert(required <= 32);
  if (required > _capacity) {
    size_t alloc_size = hamt_allocation_size(required, generation, level);

    Node *new_base =
      static_cast<Node *>(allocator.allocate(alloc_size * sizeof(Node), alignof(Node)));
    if (new_base == nullptr) {
      return nullptr;
    }

    if (UNLIKELY(_base == nullptr)) {
      assert(i == 0);
      _base = new_base;
      _capacity = alloc_size;
    } else {
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
  new (&_base[i]) Node(std::move(new_entry), parent);

  return &_base[i];
}

template<class Entry, class Allocator>
NodeTemplate<Entry, Allocator> *BitmapTrieTemplate<Entry, Allocator>::insertTrie(
    Allocator &allocator, Node *parent, int logical_index, uint32_t capacity) {
  assert(_capacity > size());

  const int i = physicalIndex(logical_index);
  for (int j = (int)size(); j > i; j--) {
    _base[j] = std::move(_base[j - 1]);
  }

  // Mark position as used
  assert((_bitmap & (0x1 << logical_index)) == 0 && "Logical index should be empty");
  _bitmap |= 0x1 << logical_index;

  return _base[i].BitmapTrie(allocator, parent, capacity);
}

template<class Entry, class Allocator>
NodeTemplate<Entry, Allocator> *BitmapTrieTemplate<Entry, Allocator>::allocate(
    Allocator &allocator, uint32_t capacity) {
  _capacity = capacity;
  _bitmap = 0;
  if (capacity == 0) {
    _base = nullptr;
  } else {
    _base = static_cast<Node *>(allocator.allocate(capacity * sizeof(Node), alignof(Node)));
  }
  return _base;
}

template<class Entry, class Allocator>
void BitmapTrieTemplate<Entry, Allocator>::deallocate(Allocator &allocator) {
  if (_base) {
    allocator.deallocate(_base, _capacity);
  }
}

template<class Entry, class Allocator>
void BitmapTrieTemplate<Entry, Allocator>::deallocateRecursively(Allocator &allocator) noexcept {
  // Maximum stack size: 1/5 * log2(hamt.size()) * O(32)
  std::stack<BitmapTrieTemplate> stack;
  stack.push(std::move(*this));

  while (!stack.empty()) {
    BitmapTrieTemplate trie(std::move(stack.top()));
    stack.pop();

    uint32_t trie_size = trie.size();
    if (trie_size) {
      for (int i = trie_size - 1; i >= 0; i--) {
        Node *node = &trie.physicalGet(i);
        if (node->isEntry()) {
          node->asEntry().~Entry();
        } else {
          stack.push(std::move(node->asTrie()));
        }
      }
    }
    trie.deallocate(allocator);
  }
}

template<class Entry, class Allocator>
void BitmapTrieTemplate<Entry, Allocator>::cloneRecursively(
    Allocator &allocator, BitmapTrieTemplate &root) {
  // Stack of pair<destination, source>
  std::stack<std::pair<BitmapTrieTemplate*, BitmapTrieTemplate*>> stack;
  stack.push(this, &root);

  while (!stack.empty()) {
    auto pair = stack.top();
    stack.pop();
    BitmapTrieTemplate *dest = pair.first;
    BitmapTrieTemplate *source = pair.second;

    dest->allocate(allocator, source->capacity());

    int source_size = source->size();
    if (source_size) {
      for (int i = source_size - 1; i >= 0; i--) {
        Node *source_node = &source->physicalGet(i);
        Node *dest_node = &dest->physicalGet(i);
        if (source_node->isEntry()) {
          new (dest_node) Node(*source_node);
        } else {
          stack.push(dest_node->BitmapTrie(), &source_node->asTrie());
        }
      }
    }
  }
}

// }}} END of BitmapTrieTemplate

// NodeTemplate {{{

template<class Entry, class Allocator>
NodeTemplate<Entry, Allocator> *NodeTemplate<Entry, Allocator>::BitmapTrie(NodeTemplate *parent) {
  // Make sure an even pointer was passed and this node is a trie -- !isEntry().
  // The LSB is used to indicate if the node is an entry.
  assert(((uintptr_t)parent & (uintptr_t)0x1) == 0);
  _parent = parent;
  return this;
}

template<class Entry, class Allocator>
NodeTemplate<Entry, Allocator> *NodeTemplate<Entry, Allocator>::BitmapTrie(
    Allocator &allocator, NodeTemplate* parent, uint32_t capacity) {
  auto node = this->BitmapTrie(parent);
  node->asTrie().allocate(allocator, capacity);
  return this;
}

template<class Entry, class Allocator>
NodeTemplate<Entry, Allocator>& NodeTemplate<Entry, Allocator>::operator=(
      NodeTemplate<Entry, Allocator>&& other) {
  // The LSB of parent defines if this node will be an entry
  _parent = other._parent;
  if (isEntry()) {
    Entry &rhs = *reinterpret_cast<Entry *>(&other._either.entry);
    new (&_either.entry) Entry(std::move(rhs));
  } else {
    new (&_either.trie) _BitmapTrie(std::move(other.asTrie()));
  }
  return *this;
}

template<class Entry, class Allocator>
NodeTemplate<Entry, Allocator>::NodeTemplate(Entry &&entry, NodeTemplate *parent)
  : _parent((NodeTemplate *)((uintptr_t)parent | (uintptr_t)0x1)) {
    new (&_either.entry) Entry(entry);
}

template<class Entry, class Allocator>
NodeTemplate<Entry, Allocator>& NodeTemplate<Entry, Allocator>::operator=(Entry&& other) {
  assert(false);
  _parent |= (uintptr_t)0x1; // is an entry
  new (&_either.entry) Entry(other);
  return *this;
}

// }}} END of NodeTemplate

}  // namespace detail

// HashArrayMappedTrie {{{

// HashArrayMappedTrie
template<class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(
    size_t n, const hasher& hf, const key_equal& eql, const allocator_type &a)
  : _count(0), _root(nullptr), _hasher(hf), _key_equal(eql), _allocator(a) {

  _seed = static_cast<uint32_t>(FOC_GET_HASH_SEED);
  _expected_count = (n > 0) ? next_power_of_2(n - 1) : 1;
  _generation = 0; // FIX: log2 _expected_count

  uint32_t alloc_size = detail::hamt_allocation_size(1, _generation, 0);
  assert(alloc_size >= 1);
  _root.asTrie().allocate(_allocator, alloc_size);
}

template<class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(
    const allocator_type& a)
  : HashArrayMappedTrie(0, hasher(), key_equal(), a) {}

template<class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(
    const HashArrayMappedTrie& hamt) : HashArrayMappedTrie(hamt, allocator_type()) {}

template<class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(
    const HashArrayMappedTrie&, const allocator_type& a) : _allocator(a) {
  assert(false);
}

template<class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(
    HashArrayMappedTrie&& other)
  : _count(other._count),
  _expected_count(other._expected_count),
  _generation(other._generation),
  _seed(other._seed),
  _root(std::move(other._root)),
  _hasher(std::move(other._hasher)),
  _key_equal(std::move(other._key_equal)),
  _allocator(std::move(other._allocator)) {}

template<class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(
    HashArrayMappedTrie&&, const allocator_type& a) : _allocator(a) {
  assert(false);
}

// }}} End of HashArrayMappedTrie

}  // namespace foc
