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

#include "support.h"

uint32_t MurmurHash2(const void *key, int len, uint32_t seed);
uint64_t MurmurHash64A(const void *key, int len, uint64_t seed);

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

// Hashers

template<typename T, bool isPodLike>
struct HasherBase {
  uint32_t hash32(const T& data, uint32_t seed) const;
  uint64_t hash64(const T& data, uint64_t seed) const;
};

// HasherBase<T, isPodLike = true> -- Hashers for POD-like types.
// We fallback to this if the user doesn't provide a specific-hasher.
template<typename T>
struct HasherBase<T, true> {
  uint32_t hash32(const T& data, uint32_t seed) const {
    return MurmurHash2(&data, sizeof(data), seed);
  }

  uint64_t hash64(const T& data, uint64_t seed) const {
    return MurmurHash64A(&data, sizeof(data), seed);
  }
};

template<typename T>
struct Hasher : public HasherBase<T, isPodLike<T>::value> {
};

// A hasher for std::string.
// The only hasher we provide for non-POD-like types.
template<>
struct Hasher<std::string> {
  uint32_t hash32(const std::string& data, uint32_t seed) const {
    return MurmurHash2(data.c_str(), data.size(), seed);
  }

  uint64_t hash64(const std::string& data, uint64_t seed) const {
    // MurmurHash64B is better if running on a 32-bit arch
    return MurmurHash64A(data.c_str(), data.size(), seed);
  }
};

namespace detail {

template<typename K, typename V>
struct Entry {
  K key;
  V value;

  Entry(const K& key, const V& value) : key(key), value(value) {}

  Entry(const Entry& other) : Entry(other.key, other.value) {}

  Entry(Entry&& other) {
    key = std::move(other.key);
    value = std::move(other.value);
  }

  Entry& operator=(const Entry& other) {
    key = other.key;
    value = other.value;
    return *this;
  }

  Entry& operator=(const Entry&& other) {
    if (this != &other) {
      key = std::move(other.key);
      value = std::move(other.value);
    }
    return *this;
  }
};

static uint32_t allocation_size(uint32_t required, uint32_t generation, uint32_t level) {
  const static uint32_t alloc_sizes[] = {
    // 0  1  2  3  4  5  6  7  8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30  31  32
       1, 1, 2, 3, 5, 5, 8, 8, 8, 13, 13, 13, 13, 13, 21, 21, 21, 21, 21, 21, 21, 21, 29, 29, 29, 29, 29, 29, 29, 29, 32, 32, 32
  };
  // [level][generation]
  const static uint32_t alloc_sizes_by_level[][22] = {
    // 1  2  4  8  16  32  64  128 256  512 1024 2048 4096 8192 16384 32768 65536 2^17 2^18 2^19 2^20 2^21
    {  2, 3, 5, 8, 13, 21, 29, 32,  32, 32,  32,  32,  32,  32,   32,   32,   32,  32,  32,  32,  32,  32},
    {  1, 1, 1, 1,  1,  2,  3,  5,   8, 13,  21,  29,  32,  32,   32,   32,   32,  32,  32,  32,  32,  32},
    {  1, 1, 1, 1,  1,  1,  1,  1,   1,  1,   2,   3,   5,   8,   13,   21,   29,  32,  32,  32,  32,  32},
    {  1, 1, 1, 1,  1,  1,  1,  1,   1,  1,   1,   1,   1,   1,    1,    2,    3,   5,   8,  13,  21,  29},
  };

  if (level > 3) {
    uint32_t guess = 1;
    if (required > guess) {
      assert(required <= 32);
      return alloc_sizes[required];
    }
    return guess;
  }

  if (generation > 21) {
    return 32;
  }
  uint32_t guess = alloc_sizes_by_level[level][generation];
  if (required > guess) {
    assert(required <= 32);
    return alloc_sizes[required];
  }
  return guess;
}

}  // namespace detail

// HashArrayMappedTrie

template<typename K, typename V>
class HashArrayMappedTrie {
 public:
  typedef detail::Entry<K, V> Entry;

  class Allocator;
  class BitmapTrie;

  // A Node in the HAMT is a sum type of Entry and BitmapTrie (can be one or the other).
  class Node {
   private:
    bool _is_entry;
    BitmapTrie *_parent;
    union {
      struct {
        alignas(alignof(Entry)) char buffer[sizeof(Entry)];
      } entry;
      BitmapTrie trie;
    } _either;

   public:
    Node *BitmapTrie(Allocator &allocator, uint32_t capacity, uint32_t bitmap, uint32_t level) {
      _is_entry = false;
      return _either.trie.Initialize(allocator, capacity, bitmap, level);
    }

    Node(Node&& other) : _is_entry(other._is_entry) {
      if (_is_entry) {
        _either.entry = std::move(other._either.entry);
      } else {
        _either.trie = other._either.trie;
      }
    }

    Node& operator=(Node&& other) {
      _is_entry = other._is_entry;
      if (_is_entry) {
        _either.entry = std::move(other._either.entry);
      } else {
        _either.trie = other._either.trie;
      }
      return *this;
    }

    Node(const K& key, const V& value) : _is_entry(true) { new (&_either.entry) Entry(key, value); }

    Node(K&& key, V&& value) : _is_entry(true) { new (&_either.entry) Entry(key, value); }

    Node& operator=(Entry&& other) {
      _is_entry = true;
      new (&_either.entry) Entry(std::move(other));
      return *this;
    }

    bool isEntry() const { return _is_entry; }

    Entry& asEntry() {
      assert(_is_entry && "Node should be an entry");
      return *reinterpret_cast<Entry *>(&_either.entry);
    }

    class BitmapTrie& asTrie() {
      assert(!_is_entry && "Node should be a trie");
      return _either.trie;
    }
  };

  // The root of a trie that can contain up to 32 Nodes. A bitmap is used
  // to compress the array as decribed in the paper.
  class BitmapTrie {
   private:
    uint32_t _bitmap;
    uint32_t _capacity;
    Node *_base;

    uint32_t physicalIndex(uint32_t logical_index) const {
      assert(logical_index < 32);
      uint32_t _bitmask = 0x1 << logical_index;
      return __builtin_popcount(_bitmap & (_bitmask - 1));
    }

   public:
    Node *Initialize(Allocator &allocator, uint32_t capacity, uint32_t bitmap, uint32_t level) {
      _capacity = capacity;
      _bitmap = bitmap;
      _base = allocator.allocate(capacity, level);
      return _base;
    } 

    uint32_t size() const { return __builtin_popcount(_bitmap); }
    uint32_t capacity() const { return _capacity; }

    Node &physicalGet(uint32_t i) { return _base[i]; }
    Node &logicalGet(uint32_t i) { return _base[physicalIndex(i)]; }

    bool logicalPositionTaken(uint32_t logical_index) const {
      assert(logical_index < 32);
      return _bitmap & (0x1 << logical_index);
    }

    Node *insert(HashArrayMappedTrie *hamt, int logical_index, const K& key, const V& value, uint32_t level) {
      const uint32_t i = physicalIndex(logical_index);
      const uint32_t sz = this->size();

      uint32_t required = sz + 1;
      assert(required <= 32);
      if (required > _capacity) {
        uint32_t alloc_size = detail::allocation_size(required, hamt->_generation, level);

        if (UNLIKELY(_base == nullptr)) {
          assert(i == 0);
          _base = hamt->_allocator.allocate(alloc_size, level);
          if (!_base) {
            return nullptr;
          }
          _capacity = alloc_size;
        } else {
          Node *new_base = hamt->_allocator.allocate(alloc_size, level);
          if (!new_base) {
            return nullptr;
          }

          for (uint32_t j = 0; j < i; j++) {
            new_base[j] = std::move(_base[j]);
          }
          for (uint32_t j = i + 1; j <= sz; j++) {
            new_base[j] = std::move(_base[j - 1]);
          }

          hamt->_allocator._free(_base, _capacity, level);
          _base = new_base;
          _capacity = alloc_size;
        }
      } else {
        for (uint32_t j = sz; j > i; j--) {
          _base[j] = std::move(_base[j - 1]);
        }
      }

      // Mark position as used
      assert((_bitmap & (0x1 << logical_index)) == 0 && "Logical index should be empty");
      _bitmap |= 0x1 << logical_index;

      // Insert at allocated position
      new (&_base[i]) Node(key, value);

      return &_base[i];
    }
  };

  // System memory blocks from where Allocator allocates fragments.
  class MemoryBlock {
   private:
    Node *_ptr;
    Node *_cursor;
    size_t _size;
    size_t _used;

   public:
    MemoryBlock(Node *ptr, size_t size)
      : _ptr(ptr), _cursor(ptr), _size(size), _used(0) {}

    Node *allocFragment(size_t fragment_size) {
      /* printf("- %zu\n", fragment_size); */
      if (_cursor + fragment_size <= _ptr + _size) {
        auto fragment = _cursor;
        _cursor += fragment_size;
        _used += fragment_size;
        return fragment;
      }

      return nullptr;
    }

    bool freeFragment(const Node *fragment, size_t fragment_size) {
      if (fragment >= _ptr && fragment < _ptr + _size) {
        _used -= fragment_size;
        assert(_used < _size && "Unexpected integer overflow");
        if (_used == 0) {
          free(_ptr);
          _ptr = nullptr;
        }
        return true;
      }
      return false;
    }

    bool empty() const { return _ptr == nullptr; }
    size_t size() const { return _ptr ? _size : 0; }
    size_t used() const { return _used; }
  };

  // Allocator for fragments of 1 to 32 "Node"s.
  class Allocator {
   private:
    std::deque<MemoryBlock> _blocks;
    size_t _next_block_size;

   public:
    Allocator() : _next_block_size(1) {}

    Node *allocate(uint32_t fragment_size, uint32_t level) {
      for (size_t i = 0;;) {
        for (; i < _blocks.size(); i++) {
          auto fragment = _blocks[i].allocFragment(fragment_size);
          if (fragment != nullptr) {
            return fragment;
          }
        }

        // Allocate another block and keep looking for block that fits the fragment
        auto ptr = (Node *)malloc(_next_block_size * sizeof(Node));
        if (ptr == nullptr) {
          return nullptr;
        }
        _blocks.push_back(MemoryBlock(ptr, _next_block_size));
        _next_block_size *= 2;

        assert(i + 1 == _blocks.size());
      }

      return nullptr;
    }

    void _free(Node *ptr, uint32_t fragment_size, uint32_t level) {
      // Look for the block that contains the fragment and free it
      for (auto& block : _blocks) {
        if (block.freeFragment(ptr, fragment_size)) {
          break;
        }
      }
      // Remove empty blocks from the front of the deque
      while (!_blocks.empty()) {
        auto block = _blocks.front();
        if (block.empty()) {
          _blocks.pop_front();
        } else {
          break;
        }
      }
    }

    void dump(size_t n) {
      size_t allocated = 0;
      size_t active = 0;
      for (auto block : _blocks) {
        printf("%zu (%zu)\n", block.size(), block.used());
        allocated += block.size();
        active += block.used();
      }
      printf("%zu, %zu, %zu\n", n, allocated, active);
    }
  };

  BitmapTrie _root;
  uint32_t _seed;
  uint32_t _count;
  uint32_t _expected_count;
  uint32_t _generation;
  Allocator _allocator;
  Hasher<K> _hasher;

 public:
  HashArrayMappedTrie() : HashArrayMappedTrie(1) {}

  HashArrayMappedTrie(uint32_t expected_count) : _count(0) {
    _seed = static_cast<uint32_t>(FOC_GET_HASH_SEED);
    _expected_count = (expected_count > 0) ? next_power_of_2(expected_count - 1) : 1;
    _generation = 0; // FIX: log2 _expected_count

    uint32_t alloc_size = detail::allocation_size(1, _generation, 0);
    assert(alloc_size >= 1);
    _root.Initialize(_allocator, alloc_size, 0, 0);
  }

  bool insert(const K& key, const V& value) {
    uint32_t hash = _hasher.hash32(key, _seed);

    if (_count >= _expected_count) {
      _expected_count *= 2;
      _generation++;
    }
    Node *node_ptr = insert(&_root, key, value, _seed, hash, 0, 0);
    _count++;

    return node_ptr != nullptr;
  }

  const V *find(const K& key) {
    BitmapTrie *trie = &_root;
    uint32_t seed = _seed;
    uint32_t hash = _hasher.hash32(key, _seed);
    uint32_t hash_offset = 0;
    uint32_t t = hash & 0x1f;

    while (trie->logicalPositionTaken(t)) {
      auto& node = trie->logicalGet(t);
      if (node.isEntry()) {
        auto& entry = node.asEntry();
        // Keys match!
        if (entry.key == key) {
          return &entry.value;
        }
        /* printf("%d -> %d\n", key, hash); */
        /* printf("%d -> %d\n", entry.key, _hasher.hash32(entry.key, seed)); */
        return nullptr;
      }

      // The position stores a trie. Keep searching.

      if (LIKELY(hash_offset < 25)) {
        hash_offset += 5;
      } else {
        hash_offset = 0;
        seed++;
        hash = _hasher.hash32(key, seed);
      }

      trie = &node.asTrie();
      t = (hash >> hash_offset) & 0x1f;
    }

    return nullptr;
  }

  size_t size() const { return _count; }
  bool empty() const { return _count == 0; }

 public:
  Node *insert(
      BitmapTrie *trie,
      const K& key,
      const V& value,
      uint32_t seed,
      uint32_t hash,
      uint32_t hash_offset,
      uint32_t level) {
    uint32_t t = (hash >> hash_offset) & 0x1f;

    if (trie->logicalPositionTaken(t)) {
      Node *node = &trie->logicalGet(t);
      if (node->isEntry()) {
        Entry *entry = &node->asEntry();
        if (entry->key == key) {
          // Keys match! Override the value.
          entry->value = value;
          return node;
        }

        // Has to replace the entry with a trie, move this entry there and
        // insert the new entry as well.

        if (LIKELY(hash_offset < 25)) {
          hash_offset += 5;
        } else {
          hash_offset = 0;
          seed++;
          hash = _hasher.hash32(key, seed);
        }

        return branchOff(node, seed, key, value, hash, hash_offset, level + 1);
      }

      if (LIKELY(hash_offset < 25)) {
        hash_offset += 5;
      } else {
        hash_offset = 0;
        seed++;
        hash = _hasher.hash32(key, seed);
      }

      // The position stores a trie. Delegate insertion.
      return insert(&node->asTrie(), key, value, seed, hash, hash_offset, level + 1);
    }

    trie->insert(this, t, key, value, level);
  }

  // This function will replace the node contents with a trie to
  // make more room for (key, value).
  Node *branchOff(
      Node *old_node,
      uint32_t seed,
      const K& key,
      const V& value,
      uint32_t hash,
      uint32_t hash_offset,
      uint32_t level) {
    uint32_t old_entry_hash = _hasher.hash32(old_node->asEntry().key, seed);
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
      old_node->BitmapTrie(_allocator, 1, 0x1 << hash_slice, level);

      if (LIKELY(hash_offset < 25)) {
        hash_offset += 5;
      } else {
        hash_offset = 0;
        seed++;
        old_entry_hash = _hasher.hash32(old_entry.key, seed);
        hash = _hasher.hash32(key, seed);
      }

      // We guess that the two entries will be stored in a single node,
      // so we initialize it with capacity for 2.
      old_node->asTrie().physicalGet(0).BitmapTrie(_allocator, 2, 0, level + 1);

      BitmapTrie *trie = &old_node->asTrie().physicalGet(0).asTrie();
      insert(trie, old_entry.key, old_entry.value, seed, old_entry_hash, hash_offset, level + 1);
      return insert(trie, key, value, seed, hash, hash_offset, level + 1);
    }

    old_node->BitmapTrie(_allocator, 2, (0x1 << old_entry_hash_slice) | (0x1 << hash_slice), level);

    auto& new_trie = old_node->asTrie();
    if (old_entry_hash_slice < hash_slice) {
      new_trie.physicalGet(0) = std::move(old_entry);
      return new (&new_trie.physicalGet(1)) Node(key, value);
    } else {
      Node *node_ptr = &new_trie.physicalGet(0);
      new (node_ptr) Node(key, value);
      new_trie.physicalGet(1) = std::move(old_entry);

      return node_ptr;
    }
  }

#ifdef GTEST
  BitmapTrie &root() { return _root; }
#endif

  void print() {
    _allocator.dump(_count);
    /* printf("%zu, %zu, %zu\n", _count, _allocator._total_allocated, _allocator._total_in_free_lists); */
  }
};

/* HashArrayMappedTrie<int, int> a; */
/* HashArrayMappedTrie<std::string, int> b; */


}  // namespace foc
