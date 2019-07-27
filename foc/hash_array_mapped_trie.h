// Hash Array Mapped Trie
//
// An implementation of Phil Bagwell's Hash Array Mapped Trie.
//
// "Ideal Hash Trees". Phil Bagwell. 2001.
// http://infoscience.epfl.ch/record/64398
#pragma once

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <queue>
#include <stack>
#include <string>
#include <utility>

#include "allocator.h"
#include "support.h"

#ifndef VISIBLE_IN_TESTS
#ifdef TESTS
#define VISIBLE_IN_TESTS public
#else
#define VISIBLE_IN_TESTS private
#endif
#endif

// This needs to be a per-execution seed to avoid denial-of-service attacks
// and you should not rely on the same hashes being generated on different
// runs of the program.
//
// Users of this library can define this macro before including the file to be
// any expression (e.g. a function call) that returns a 64-bit seed.
#ifndef FOC_GET_HASH_SEED
#define FOC_GET_HASH_SEED 0xff51afd7ed558ccdULL
#endif

namespace foc {

namespace detail {

// expected_hamt_size is the expected_hamt_size after insertion
uint32_t hamt_trie_allocation_size(uint32_t required, size_t expected_hamt_size, uint32_t level);

template <class Entry, class Allocator>
class NodeTemplate;

// The root of a trie that can contain up to 32 Nodes. A bitmap is used
// to compress the array as decribed in the paper.
template <class Entry, class Allocator>
class BitmapTrieTemplate {
 private:
  using Node = NodeTemplate<Entry, Allocator>;
  uint32_t _bitmap;
  uint32_t _capacity;
  Node *_base;

 public:
  // We allow the object to be uninitialized because we want to keep it inside a union.
  //
  // WARNING: Users of this class (internal-only) should call allocate and
  // deallocate correctly.
  BitmapTrieTemplate() = default;
  BitmapTrieTemplate(const BitmapTrieTemplate &other) = delete;
  BitmapTrieTemplate(BitmapTrieTemplate &&other) = default;

  FOC_ATTRIBUTE_ALWAYS_INLINE
  Node *allocate(Allocator &allocator, uint32_t capacity);
  FOC_ATTRIBUTE_ALWAYS_INLINE
  void deallocate(Allocator &allocator);

  void cloneRecursively(Allocator &, BitmapTrieTemplate &root);
  void deallocateRecursively(Allocator &) noexcept;

  void clear(Allocator &allocator) {
    deallocateRecursively(allocator);
    _bitmap = 0;
    _capacity = 0;
    _base = nullptr;
  }

  void swap(BitmapTrieTemplate &other) {
    std::swap(_bitmap, other._bitmap);
    std::swap(_capacity, other._capacity);
    std::swap(_base, other._base);
  }

  uint32_t physicalIndex(uint32_t logical_index) const {
    assert(logical_index < 32);
    uint32_t _bitmask = 0x1 << logical_index;
    return __builtin_popcount(_bitmap & (_bitmask - 1));
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

  uint32_t physicalIndexOf(const Node *needle) const {
    assert(needle);
    assert(needle >= _base);
    assert(needle <= _base + size());
    return needle - _base;
  }

  Node *insertEntry(Allocator &,
                    int logical_index,
                    const Node *parent,
                    size_t expected_hamt_size,
                    uint32_t level);

#ifdef TESTS
  Node *insertTrie(Allocator &, Node *parent, int logical_index, uint32_t capacity);
#endif  // TESTS

  const Node *firstEntryNodeRecursively() const noexcept;

#ifdef TESTS
  uint32_t &bitmap() { return _bitmap; }
#endif  // TESTS
};

// A Node in the HAMT is a sum type of Entry and BitmapTrie (i.e. can be one or the other).
// The lower bit in the _parent pointer is used to identify the case.
template <class Entry, class Allocator>
class NodeTemplate {
 private:
  using BitmapTrieT = BitmapTrieTemplate<Entry, Allocator>;

  NodeTemplate *_parent;
  union {
    struct {
      alignas(alignof(Entry)) char buffer[sizeof(Entry)];
    } entry;
    BitmapTrieT trie;
  } _either;

 public:
  FOC_ATTRIBUTE_ALWAYS_INLINE
  explicit NodeTemplate(NodeTemplate *parent) { BitmapTrie(parent); }
  FOC_ATTRIBUTE_ALWAYS_INLINE
  NodeTemplate(Entry &&entry, NodeTemplate *parent);

  FOC_ATTRIBUTE_ALWAYS_INLINE
  NodeTemplate *BitmapTrie(NodeTemplate *parent);
  FOC_ATTRIBUTE_ALWAYS_INLINE
  NodeTemplate *BitmapTrie(Allocator &allocator, NodeTemplate *parent, uint32_t capacity);

  FOC_ATTRIBUTE_ALWAYS_INLINE
  NodeTemplate &operator=(NodeTemplate &&other);
  FOC_ATTRIBUTE_ALWAYS_INLINE
  NodeTemplate &operator=(Entry &&other);

  FOC_ATTRIBUTE_ALWAYS_INLINE
  NodeTemplate *setEntryParent(const NodeTemplate *parent);

  bool isEntry() const { return (uintptr_t)_parent & (uintptr_t)0x1U; }
  bool isTrie() const { return !isEntry(); }

  const NodeTemplate *parent() const {
    return (NodeTemplate *)((uintptr_t)_parent & ~(uintptr_t)0x1U);
  }

  NodeTemplate *parent() { return (NodeTemplate *)((uintptr_t)_parent & ~(uintptr_t)0x1U); }

  Entry &asEntry() {
    assert(isEntry() && "Node should be an entry");
    return *reinterpret_cast<Entry *>(&_either.entry);
  }

  const Entry &asEntry() const {
    assert(isEntry() && "Node should be an entry");
    return *reinterpret_cast<const Entry *>(&_either.entry);
  }

  BitmapTrieT &asTrie() {
    assert(isTrie() && "Node should be a trie");
    return _either.trie;
  }

  const BitmapTrieT &asTrie() const {
    assert(isTrie() && "Node should be a trie");
    return _either.trie;
  }

  const NodeTemplate *nextEntryNode() const;
};

}  // namespace detail

template <class Entry, class Allocator>
class HAMTConstForwardIterator {
 private:
  using Node = detail::NodeTemplate<Entry, Allocator>;
  const Node *_node;

 public:
  // clang-format off
  typedef std::forward_iterator_tag  iterator_category;
  typedef Entry                      value_type;
  typedef ptrdiff_t                  difference_type;
  typedef const Entry&               reference;
  typedef const Entry*               pointer;
  // clang-format on

  HAMTConstForwardIterator() noexcept : _node(nullptr) {}
  HAMTConstForwardIterator(const Node *node) noexcept : _node(node) {}
  // TODO: implement HAMTForwardIterator
  // HAMTConstForwardIterator(const HAMTForwardIterator& it) noexcept : _node(it._node) {}
  HAMTConstForwardIterator(const HAMTConstForwardIterator &it) noexcept : _node(it._node) {}

  reference operator*() const noexcept { return _node->asEntry(); }
  pointer operator->() const noexcept { return &_node->asEntry(); }

  HAMTConstForwardIterator &operator++() {
    _node = _node->nextEntryNode();
    return *this;
  }

  HAMTConstForwardIterator operator++(int) {
    HAMTConstForwardIterator _this(_node);
    ++(*this);
    return _this;
  }

  friend bool operator==(const HAMTConstForwardIterator &x, const HAMTConstForwardIterator &y) {
    return x._node == y._node;
  }

  friend bool operator!=(const HAMTConstForwardIterator &x, const HAMTConstForwardIterator &y) {
    return x._node != y._node;
  }

  template <class, class, class, class, class>
  friend class HashArrayMappedTrie;
  template <class, class>
  friend class NodeTemplate;
};

template <class Key,
          class T,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = MallocAllocator>
class HashArrayMappedTrie {
  // clang-format off
 VISIBLE_IN_TESTS:
  using Entry = std::pair<Key, T>;
  // clang-format on
  using BitmapTrie = detail::BitmapTrieTemplate<Entry, Allocator>;
  using Node = detail::NodeTemplate<Entry, Allocator>;

 public:
  // Some std::unordered_map member types.
  //
  // WARNING: our Allocator is nothing like any std allocator.
  // clang-format off
  typedef Key                                               key_type;
  typedef T                                                 mapped_type;
  typedef Hash                                              hasher;
  typedef KeyEqual                                          key_equal;
  typedef Allocator                                         allocator_type;
  typedef std::pair<const Key, T>                           value_type;
  typedef size_t                                            size_type;
  typedef std::pair<const Key, T>*                          pointer;
  typedef const std::pair<const Key, T>*                    const_pointer;
  typedef std::pair<const Key, T>&                          reference;
  typedef const std::pair<const Key, T>&                    const_reference;
  // TODO: implement HAMTForwardIterator
  // typedef HAMTForwardIterator<Entry, Allocator>             iterator;
  typedef const HAMTConstForwardIterator<Entry, Allocator>  iterator;
  typedef const HAMTConstForwardIterator<Entry, Allocator>  const_iterator;
  // clang-format on

  size_type _count;
  Node _root;
  uint32_t _seed;
  Hash _hasher;
  KeyEqual _key_equal;
  Allocator _allocator;

 public:
  HashArrayMappedTrie() : HashArrayMappedTrie(1) {}

  explicit HashArrayMappedTrie(size_t n,
                               const hasher &hf = hasher(),
                               const key_equal &eql = key_equal(),
                               const allocator_type &a = allocator_type());

  explicit HashArrayMappedTrie(const allocator_type &allocator);

  HashArrayMappedTrie(const HashArrayMappedTrie &);
  HashArrayMappedTrie(const HashArrayMappedTrie &, const allocator_type &);

  HashArrayMappedTrie(HashArrayMappedTrie &&other);

  HashArrayMappedTrie(HashArrayMappedTrie &&, const allocator_type &);
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
  HashArrayMappedTrie(InputIterator f, InputIterator l, size_t n, const hasher& hf, const
  allocator_type& a) : HashArrayMappedTrie(f, l, n, hf, key_equal(), a) {}
  */
  /*
  HashArrayMappedTrie(initializer_list<value_type> il, size_t n, const allocator_type& a)
    : HashArrayMappedTrie(il, n, hasher(), key_equal(), a) {}
  HashArrayMappedTrie(initializer_list<value_type> il, size_t n, const hasher& hf, const
  allocator_type& a) : HashArrayMappedTrie(il, n, hf, key_equal(), a) {}
  */

  ~HashArrayMappedTrie() { _root.asTrie().deallocateRecursively(_allocator); }

  // TODO: define out-of-line
  HashArrayMappedTrie &operator=(const HashArrayMappedTrie &other) {
    if (this != &other) {
      _root.asTrie().deallocateRecursively(_allocator);
      _count = other._count;
      _seed = other._seed;
      _root = other._root;
      _hasher = other._hasher;
      _key_equal = other._key_equal;
      _allocator = other._allocator;  // TODO: can copy allocator?
      _root.cloneRecursively(_allocator, other._root);
    }
    return *this;
  }

  // TODO: define out-of-line
  HashArrayMappedTrie &operator=(HashArrayMappedTrie &&other) {
    if (this != &other) {
      _root.asTrie().deallocateRecursively(_allocator);
      _count = other._count;
      _seed = other._seed;
      _root = std::move(other._root);
      _hasher = std::move(other._hasher);
      _key_equal = std::move(other._key_equal);
      _allocator = std::move(other._allocator);  // TODO: can copy allocator?
    }
    return *this;
  }

  /* HashArrayMappedTrie& operator=(initializer_list<value_type>); */

  allocator_type get_allocator() const { return _allocator; }

  bool empty() const { return _count == 0; }
  size_type size() const { return _count; }
  // We don't implement max_size()

  iterator begin() noexcept {
    return iterator(_count ? _root.asTrie().firstEntryNodeRecursively() : nullptr);
  }

  iterator end() noexcept { return iterator(nullptr); }

  const_iterator begin() const noexcept {
    return const_iterator(_count ? _root.asTrie().firstEntryNodeRecursively() : nullptr);
  }

  const_iterator end() const noexcept { return const_iterator(nullptr); }

  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }

  // template <class... Args> pair<iterator, bool> emplace(Args&&... args);
  // template <class... Args> iterator emplace_hint(const_iterator position, Args&&... args);

  iterator insert(const value_type &entry) {
    bool exists = false;
    Node *node = insertEntry(/*key=*/entry.first, &exists);
    if (node == nullptr) {
      return iterator(nullptr);
    }
    if (!exists) {
      new (&node->asEntry()) Entry(entry);
    }
    return iterator(node);
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
    _root.asTrie().clear(_allocator);
    _seed = static_cast<uint32_t>(FOC_GET_HASH_SEED);
  }

  // TODO: define out-of-line
  void swap(HashArrayMappedTrie &other) {
    std::swap(_count, other._count);
    std::swap(_seed, other._seed);
    std::swap(_hasher, other._hasher);
    std::swap(_key_equal, other._key_equal);
    std::swap(_allocator, other._allocator);
    _root.swap(other._root);
  }

  // We don't implement emplace*()

  // TODO: merge

  // We don't implement at() to avoid the need of exceptions

  T &operator[](const Key &key) {
    bool exists = false;
    Node *node = insertEntry(key, &exists);
    assert(node);  // nullptr here is a bug in the hash function
    auto &entry = node->asEntry();
    if (!exists) {
      new (&entry.first) Key(key);
      new (&entry.second) T();
    }
    return entry.second;
  }

  T &operator[](Key &&key) {
    bool exists = false;
    Node *node = insertEntry(key, &exists);
    assert(node);  // nullptr here is a bug in the hash function
    auto &entry = node->asEntry();
    if (!exists) {
      new (&entry.first) Key(std::move(key));
      new (&entry.second) T();
    }
    return entry.second;
  }

  size_type count(const Key &key) const {
    const Node *node = const_cast<HashArrayMappedTrie *>(this)->findNode(key);
    return node ? 1 : 0;
  }

  iterator find(const Key &key) {
    const auto *node = findNode(key);
    return iterator{node};
  }

  const_iterator find(const Key &key) const {
    const auto *node = const_cast<HashArrayMappedTrie *>(this)->findNode(key);
    return const_iterator{node};
  }

  std::pair<iterator, iterator> equal_range(const Key &key) {
    const auto first = find(key);
    auto second = first;
    second++;
    return std::make_pair(first, second);
  }

  std::pair<const_iterator, const_iterator> equal_range(const Key &key) const {
    const auto first = find(key);
    auto second = first;
    second++;
    return std::make_pair(first, second);
  }

  hasher hash_function() const { return _hasher; }

  key_equal key_eq() const { return _key_equal; }

  // Custom container API {{{

  bool put(value_type &&new_entry) {
    bool exists = false;
    Node *node = insertEntry(/*key=*/new_entry.first, &exists);
    assert(node && "Hash function is buggy");
    if (node == nullptr) {
      return false;
    }
    auto &entry = node->asEntry();
    if (exists) {
      entry.second = std::move(new_entry.second);
    } else {
      new (&entry)
          Entry(std::move(const_cast<Key &>(new_entry.first)), std::move(new_entry.second));
    }
    return exists;
  }

  bool put(const value_type &new_entry) {
    bool exists = false;
    Node *node = insertEntry(/*key=*/new_entry.first, &exists);
    assert(node && "Hash function is buggy");
    if (node == nullptr) {
      return false;
    }
    auto &entry = node->asEntry();
    if (exists) {
      entry.second = new_entry.second;
    } else {
      new (&entry) Entry(new_entry);
    }
    return exists;
  }

  template <typename T1, typename T2>
  bool put(T1 &&key, T2 &&value) {
    return put(std::make_pair(std::forward<const T1>(key), std::forward<T2>(value)));
  }

  const Node *findNode(const Key &key) {
    const BitmapTrie *trie = &_root.asTrie();
    uint32_t hash = hash32(key);
    uint32_t shift = 0;
    uint32_t t = hash & 0x1f;

    const Node *node = nullptr;
    while (trie->logicalPositionTaken(t)) {
      node = &trie->logicalGet(t);
      // 1) Entry found. Check if keys match.
      if (node->isEntry()) {
        const auto &entry = node->asEntry();
        return _key_equal(entry.first, key) ? node : nullptr;
      }
      // 2) The position stores a trie. Keep searching.
      trie = &node->asTrie();
      if (shift >= 32 - 5) {
        // We probably need to perform a serch on the collisions
        break;
      }
      shift += 5;
      t = (hash >> shift) & 0x1f;
    }

    if (!node) {
      return nullptr;
    }

    // The hash was exhausted, perform a Depth-First Search because at this trie
    // depth the positions are meaningless.
    std::stack<const Node *> dfs_stack;
    dfs_stack.push(node);
    while (!dfs_stack.empty()) {
      node = dfs_stack.top();
      dfs_stack.pop();

      trie = &node->asTrie();
      for (uint32_t i = 0; i < trie->size(); i++) {
        const Node *child_node = &trie->physicalGet(i);
        if (child_node->isTrie()) {
          dfs_stack.push(child_node);
        } else {
          const Entry &entry = child_node->asEntry();
          if (_key_equal(entry.first, key)) {
            // Found!
            return child_node;
          }
        }
      }
    }

    return nullptr;
  }

  const T *findValue(const Key &key) {
    const Node *node = findNode(key);
    if (node) {
      return &node->asEntry().second;
    }
    return nullptr;
  }

  Node *insertHashCollidedEntry(Node *trie_node, const Key &key, bool *exists) {
    assert(!*exists);
    // Insert in Breadth-First Order for faster Depth-First Search
    std::queue<Node *> bfs_queue;
    Node *non_full_trie_node = nullptr;  // BitmapTrie Node that can fit the new entry
    Node *first_entry_parent = nullptr;
    Node *first_entry_node = nullptr;  // The first Node (in BFS order) that's an Entry

    bfs_queue.push(trie_node);
    while (!bfs_queue.empty()) {
      trie_node = bfs_queue.front();
      bfs_queue.pop();
      BitmapTrie &trie = trie_node->asTrie();
      if (!non_full_trie_node && trie.size() < 32) {
        assert(trie.size() == trie.physicalIndex(trie.size()));
        non_full_trie_node = trie_node;
      }
      for (uint32_t i = 0; i < trie.size(); i++) {
        Node *child_node = &trie.physicalGet(i);
        if (child_node->isTrie()) {
          bfs_queue.push(child_node);
        } else {
          auto &entry = child_node->asEntry();
          if (_key_equal(entry.first, key)) {
            *exists = true;
            return child_node;
          }
          if (!first_entry_node) {
            first_entry_parent = trie_node;
            first_entry_node = child_node;
          }
        }
      }
    }

    // If a non-full trie Node was found, append the new Entry to it.
    if (non_full_trie_node) {
      BitmapTrie &trie = trie_node->asTrie();
      _count++;
      return trie.insertEntry(_allocator,
                              /*logical_index=*/trie.size(),
                              /*parent=*/trie_node,
                              /*expected_hamt_size=*/_count,
                              /*level=floor(32/5)+1*/ 7);
    }

    // Otherwise, replacing an Entry with a new BitmapTrie that can fit more
    // entries is necessary.

    assert(first_entry_node &&
           "Search started on a BitmapTrie, an entry (leaf) should have been found");
    assert(first_entry_parent);

    Entry replaced_entry(std::move(first_entry_node->asEntry()));
    trie_node = first_entry_node->BitmapTrie(_allocator, first_entry_parent, 2);

    BitmapTrie &trie = trie_node->asTrie();
    auto replaced_node = trie.insertEntry(_allocator,
                                          /*logical_index=*/0,
                                          /*parent=*/trie_node,
                                          /*expected_hamt_size=*/_count,
                                          /*level=floor(32/5)+1*/ 7);
    new (&replaced_node->asEntry()) Entry(std::move(replaced_entry));
    _count++;
    return trie.insertEntry(_allocator,
                            /*logical_index=*/1,
                            /*parent=*/trie_node,
                            /*expected_hamt_size=*/_count,
                            /*level=floor(32/5)+1*/ 7);
  }

  Node *insertEntry(Node *trie_node,
                    const Key &key,
                    uint32_t hash,
                    uint32_t shift,
                    uint32_t level,
                    bool *exists) {
    assert(!*exists);
    BitmapTrie *trie = &trie_node->asTrie();

    // Exhausted hash
    if (FOC_UNLIKELY(shift >= 32)) {
      return insertHashCollidedEntry(trie_node, key, exists);
    }

    // Insert the entry directly in the trie if the t slot is empty.
    uint32_t t = (hash >> shift) & 0x1f;
    if (!trie->logicalPositionTaken(t)) {
      _count++;
      return trie->insertEntry(_allocator,
                               /*logical_index=*/t,
                               /*parent=*/trie_node,
                               /*expected_hamt_size=*/_count,
                               level);
    }

    // If the Node in t is a trie, insert recursively.
    Node *node = &trie->logicalGet(t);
    if (node->isTrie()) {
      return insertEntry(/*trie_node=*/node, key, hash, shift + 5, level + 1, exists);
    }

    // Check if the Node is an entry and the key matches!
    Entry *entry = &node->asEntry();
    if (_key_equal(entry->first, key)) {
      *exists = true;
      return node;
    }

    // Has to replace the entry with a trie.
    // This new trie will contain the replaced_entry and the new entry.
    Entry replaced_entry(std::move(*entry));
    trie_node = node->BitmapTrie(_allocator, node->parent(), 2);
    _count--;

    auto replaced_node = insertEntry(trie_node,
                                     /*key=*/replaced_entry.first,
                                     /*hash=*/hash32(replaced_entry.first),
                                     shift + 5,
                                     level + 1,
                                     exists);
    new (&replaced_node->asEntry()) Entry(std::move(replaced_entry));
    return insertEntry(trie_node, key, hash, shift + 5, level + 1, exists);
  }

  Node *insertEntry(const Key &key, bool *exists) {
    assert(*exists == false);
    return insertEntry(
        /*trie_node=*/&_root, key, /*hash=*/hash32(key), /*shift=*/0, /*level=*/0, exists);
  }

  // }}} End of Custom container API

 private:
  Node *allocateBaseTrieArray(size_t capacity) {
    void *ptr = _allocator.allocate(capacity * sizeof(Node), alignof(Node));
    return static_cast<Node *>(ptr);
  }

  uint32_t hash32(const Key &key) const {
    return _seed ^ (_hasher(key) + 0x9e3779b9 + (_seed << 6) + (_seed >> 2));
  }

#ifdef TESTS
  // clang-format off
 VISIBLE_IN_TESTS:
  Node & root() { return _root; }
  // clang-format on

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
    /* printf("%zu, %zu, %zu\n", _count, _allocator._total_allocated,
     * _allocator._total_in_free_lists); */
  }
#endif  // TESTS
};

namespace detail {

#ifdef HAMT_IMPLEMENTATION

uint32_t hamt_trie_allocation_size(uint32_t required, size_t expected_hamt_size, uint32_t level) {
  // clang-format off
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
  // clang-format on

  assert(required > 0 && required <= 32);
  assert(expected_hamt_size > 0);

  uint32_t generation;  // ceil(log2(expected_hamt_size))
  if (level > 4) {
    level = 4;
    generation = 0;
  } else {
    if (expected_hamt_size - 1 == 0) {
      generation = 0;
    } else {
      generation = 64 - __builtin_clzll(expected_hamt_size - 1);
      if (generation > 22) {
        generation = 22;
      }
    }
  }

  uint32_t guess = alloc_sizes_by_level[level][generation];
  if (required > guess) {
    return alloc_sizes[required];
  }
  return guess;
}

#endif  // HAMT_IMPLEMENTATION

// BitmapTrieTemplate {{{

template <class Entry, class Allocator>
NodeTemplate<Entry, Allocator> *BitmapTrieTemplate<Entry, Allocator>::insertEntry(
    Allocator &allocator,
    int logical_index,
    const NodeTemplate<Entry, Allocator> *parent,
    size_t expected_hamt_size,
    uint32_t level) {
  const uint32_t i = physicalIndex(logical_index);
  const uint32_t sz = this->size();

  uint32_t required = sz + 1;
  assert(required <= 32);
  if (FOC_LIKELY(required <= _capacity)) {
    for (int32_t j = (int32_t)sz; j > (int32_t)i; j--) {
      _base[j] = std::move(_base[j - 1]);
    }
  } else {
    size_t alloc_size = hamt_trie_allocation_size(required, expected_hamt_size, level);

    Node *new_base =
        static_cast<Node *>(allocator.allocate(alloc_size * sizeof(Node), alignof(Node)));
    if (new_base == nullptr) {
      return nullptr;
    }

    if (FOC_UNLIKELY(_base == nullptr)) {
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
  }

  // Mark position as used
  assert((_bitmap & (0x1 << logical_index)) == 0 && "Logical index should be empty");
  _bitmap |= 0x1 << logical_index;

  // Return allocated position
  return _base[i].setEntryParent(parent);
}

#ifdef TESTS

template <class Entry, class Allocator>
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

#endif  // TESTS

template <class Entry, class Allocator>
const NodeTemplate<Entry, Allocator>
    *BitmapTrieTemplate<Entry, Allocator>::firstEntryNodeRecursively() const noexcept {
  const BitmapTrieTemplate *trie = this;
  assert(trie->size() > 0);
  for (;;) {
    const Node &node = trie->physicalGet(0);
    if (node.isEntry()) {
      return &node;
    }
    trie = &node.asTrie();
  }
}

template <class Entry, class Allocator>
NodeTemplate<Entry, Allocator> *BitmapTrieTemplate<Entry, Allocator>::allocate(Allocator &allocator,
                                                                               uint32_t capacity) {
  _capacity = capacity;
  _bitmap = 0;
  if (capacity == 0) {
    _base = nullptr;
  } else {
    _base = static_cast<Node *>(allocator.allocate(capacity * sizeof(Node), alignof(Node)));
  }
  return _base;
}

template <class Entry, class Allocator>
void BitmapTrieTemplate<Entry, Allocator>::deallocate(Allocator &allocator) {
  if (_base) {
    allocator.deallocate(_base, _capacity);
  }
}

template <class Entry, class Allocator>
void BitmapTrieTemplate<Entry, Allocator>::deallocateRecursively(Allocator &allocator) noexcept {
  // Maximum stack size: 1/5 * log2(hamt.size()) * O(32)
  std::stack<BitmapTrieTemplate> stack;
  stack.push(std::move(*this));

  while (!stack.empty()) {
    BitmapTrieTemplate trie(std::move(stack.top()));
    stack.pop();

    int trie_size = (int)trie.size();
    for (int i = trie_size - 1; i >= 0; i--) {
      Node *node = &trie.physicalGet(i);
      if (node->isEntry()) {
        node->asEntry().~Entry();
      } else {
        stack.push(std::move(node->asTrie()));
      }
    }
    trie.deallocate(allocator);
  }
}

template <class Entry, class Allocator>
void BitmapTrieTemplate<Entry, Allocator>::cloneRecursively(Allocator &allocator,
                                                            BitmapTrieTemplate &root) {
  // Stack of pair<destination, source>
  std::stack<std::pair<BitmapTrieTemplate *, BitmapTrieTemplate *>> stack;
  stack.push(this, &root);

  while (!stack.empty()) {
    auto pair = stack.top();
    stack.pop();
    BitmapTrieTemplate *dest = pair.first;
    BitmapTrieTemplate *source = pair.second;

    dest->allocate(allocator, source->capacity());

    int source_size = source->size();
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

// }}} END of BitmapTrieTemplate

// NodeTemplate {{{

template <class Entry, class Allocator>
NodeTemplate<Entry, Allocator> *NodeTemplate<Entry, Allocator>::BitmapTrie(NodeTemplate *parent) {
  // Make sure an even pointer was passed and let this node be a trie.
  // The LSB is used to indicate if the node is an entry.
  assert(((uintptr_t)parent & (uintptr_t)0x1) == 0);
  _parent = parent;
  return this;
}

template <class Entry, class Allocator>
NodeTemplate<Entry, Allocator> *NodeTemplate<Entry, Allocator>::BitmapTrie(Allocator &allocator,
                                                                           NodeTemplate *parent,
                                                                           uint32_t capacity) {
  auto node = this->BitmapTrie(parent);
  node->asTrie().allocate(allocator, capacity);
  return this;
}

template <class Entry, class Allocator>
NodeTemplate<Entry, Allocator> &NodeTemplate<Entry, Allocator>::operator=(
    NodeTemplate<Entry, Allocator> &&other) {
  // The LSB of parent defines if this node will be an entry
  _parent = other._parent;
  if (isEntry()) {
    Entry &rhs = *reinterpret_cast<Entry *>(&other._either.entry);
    new (&_either.entry) Entry(std::move(rhs));
  } else {
    new (&_either.trie) BitmapTrieT(std::move(other.asTrie()));
  }
  return *this;
}

template <class Entry, class Allocator>
NodeTemplate<Entry, Allocator> *NodeTemplate<Entry, Allocator>::setEntryParent(
    const NodeTemplate *parent) {
  _parent = (NodeTemplate *)((uintptr_t)parent | (uintptr_t)0x1);
  return this;
}

template <class Entry, class Allocator>
NodeTemplate<Entry, Allocator>::NodeTemplate(Entry &&entry, NodeTemplate *parent) {
  setEntryParent(parent);
  new (&_either.entry) Entry(std::move(entry));
}

template <class Entry, class Allocator>
NodeTemplate<Entry, Allocator> &NodeTemplate<Entry, Allocator>::operator=(Entry &&other) {
  _parent = (NodeTemplate *)((uintptr_t)_parent | (uintptr_t)0x1);  // is an entry
  new (&_either.entry) Entry(other);
  return *this;
}

template <class Entry, class Allocator>
const NodeTemplate<Entry, Allocator> *NodeTemplate<Entry, Allocator>::nextEntryNode() const {
  assert(isEntry());
  const NodeTemplate *node = this;
  const NodeTemplate *parent_node = node->parent();
  do {
    assert(parent_node != nullptr);
    assert(parent_node->isTrie());

    const BitmapTrieT *parent_trie = &parent_node->asTrie();
    uint32_t index_of_next_node = parent_trie->physicalIndexOf(node) + 1;
    if (index_of_next_node < parent_trie->size()) {
      const NodeTemplate *next_node = &parent_trie->physicalGet(index_of_next_node);
      if (next_node->isEntry()) {
        return next_node;
      }
      const BitmapTrieT &trie = next_node->asTrie();
      return trie.firstEntryNodeRecursively();
    }

    // Go up
    node = parent_node;
    parent_node = node->parent();
  } while (parent_node);

  return nullptr;
}

// }}} END of NodeTemplate

}  // namespace detail

// HashArrayMappedTrie {{{

// HashArrayMappedTrie
template <class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(size_t n,
                                                                            const hasher &hf,
                                                                            const key_equal &eql,
                                                                            const allocator_type &a)
    : _count(0), _root(nullptr), _hasher(hf), _key_equal(eql), _allocator(a) {
  _seed = static_cast<uint32_t>(FOC_GET_HASH_SEED);
  uint32_t alloc_size = detail::hamt_trie_allocation_size(1, (n > 0) ? n : 1, 0);
  assert(alloc_size >= 1);
  _root.asTrie().allocate(_allocator, alloc_size);
}

template <class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(const allocator_type &a)
    : HashArrayMappedTrie(0, hasher(), key_equal(), a) {}

template <class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(
    const HashArrayMappedTrie &hamt)
    : HashArrayMappedTrie(hamt, allocator_type()) {}

template <class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(
    const HashArrayMappedTrie &, const allocator_type &a)
    : _allocator(a) {
  assert(false);
}

template <class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(
    HashArrayMappedTrie &&other)
    : _count(other._count),
      _seed(other._seed),
      _root(std::move(other._root)),
      _hasher(std::move(other._hasher)),
      _key_equal(std::move(other._key_equal)),
      _allocator(std::move(other._allocator)) {}

template <class Key, class T, class Hash, class KeyEqual, class Allocator>
HashArrayMappedTrie<Key, T, Hash, KeyEqual, Allocator>::HashArrayMappedTrie(HashArrayMappedTrie &&,
                                                                            const allocator_type &a)
    : _allocator(a) {
  assert(false);
}

// }}} End of HashArrayMappedTrie

}  // namespace foc
