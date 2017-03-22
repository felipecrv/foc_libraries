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
  class BitmapIndexedNode;

  struct EntryNode {
    bool is_entry;
    BitmapIndexedNode *parent;

    union {
      struct {
        alignas(alignof(Entry)) char buffer[sizeof(Entry)];
      } entry;
      BitmapIndexedNode node;
    } either;

    EntryNode(EntryNode&& other) : is_entry(other.is_entry) {
      if (is_entry) {
        either.entry = std::move(other.either.entry);
      } else {
        either.node = other.either.node;
      }
    }

    EntryNode& operator=(EntryNode&& other) {
      is_entry = other.is_entry;
      if (is_entry) {
        either.entry = std::move(other.either.entry);
      } else {
        either.node = other.either.node;
      }
      return *this;
    }

    EntryNode(const K& key, const V& value) : is_entry(true) {
      new (&either.entry) Entry(key, value);
    }

    EntryNode(K&& key, V&& value) : is_entry(true) {
      new (&either.entry) Entry(key, value);
    }

    EntryNode& operator=(Entry&& other) {
      is_entry = true;
      new (&either.entry) Entry(std::move(other));
      return *this;
    }

    Entry& entry() {
      assert(is_entry && "EntryNode should be an entry");
      return *reinterpret_cast<Entry *>(&either.entry);
    }

    BitmapIndexedNode& node() {
      assert(!is_entry && "EntryNode should be a node");
      return either.node;
    }

    void *InitializeAsNode(Allocator &allocator, uint32_t capacity, uint32_t bitmap, uint32_t level) {
      is_entry = false;
      return either.node.Initialize(allocator, capacity, bitmap, level);
    }
  };

  class BitmapIndexedNode {
   private:
    uint32_t _bitmap;
    uint32_t _capacity;
    EntryNode *_base;

    uint32_t physical_index(uint32_t logical_index) const {
      assert(logical_index < 32);
      uint32_t _bitmask = 0x1 << logical_index;
      return __builtin_popcount(_bitmap & (_bitmask - 1));
    }

   public:
    EntryNode *Initialize(Allocator &allocator, uint32_t capacity, uint32_t bitmap, uint32_t level) {
      _capacity = capacity;
      _bitmap = bitmap;
      _base = allocator.alloc(capacity, level);
      return _base;
    } 

    uint32_t size() const { return __builtin_popcount(_bitmap); }
    uint32_t capacity() const { return _capacity; }

    EntryNode &physicalGet(uint32_t i) { return _base[i]; }
    EntryNode &logicalGet(uint32_t i) { return _base[physical_index(i)]; }

    bool position_occupied(uint32_t logical_index) const {
      assert(logical_index < 32);
      return _bitmap & (0x1 << logical_index);
    }

    EntryNode *Insert(HashArrayMappedTrie *hamt, int logical_index, const K& key, const V& value, uint32_t level) {
      const uint32_t i = physical_index(logical_index);
      const uint32_t sz = this->size();

      uint32_t required = sz + 1;
      assert(required <= 32);
      if (required > _capacity) {
        uint32_t alloc_size = detail::allocation_size(required, hamt->_generation, level);

        if (UNLIKELY(_base == nullptr)) {
          assert(i == 0);
          _base = hamt->allocator.alloc(alloc_size, level);
          if (!_base) {
            return nullptr;
          }
          _capacity = alloc_size;
        } else {
          EntryNode *new_base = hamt->allocator.alloc(alloc_size, level);
          if (!new_base) {
            return nullptr;
          }

          for (uint32_t j = 0; j < i; j++) {
            new_base[j] = std::move(_base[j]);
          }
          for (uint32_t j = i + 1; j <= sz; j++) {
            new_base[j] = std::move(_base[j - 1]);
          }

          hamt->allocator._free(_base, _capacity, level);
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
      new (&_base[i]) EntryNode(key, value);

      return &_base[i];
    }
  };

  class MemoryBlock {
   private:
    EntryNode *_ptr;
    EntryNode *_cursor;
    size_t _size;
    size_t _used;

   public:
    MemoryBlock(EntryNode *ptr, size_t size)
      : _ptr(ptr), _cursor(ptr), _size(size), _used(0) {}

    EntryNode *allocFragment(size_t fragment_size) {
      /* printf("- %zu\n", fragment_size); */
      if (_cursor + fragment_size <= _ptr + _size) {
        auto fragment = _cursor;
        _cursor += fragment_size;
        _used += fragment_size;
        return fragment;
      }

      return nullptr;
    }

    bool freeFragment(const EntryNode *fragment, size_t fragment_size) {
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

  class Allocator {
   private:
    std::deque<MemoryBlock> _blocks;
    size_t _next_block_size;

   public:
    Allocator() : _next_block_size(1) {}

    EntryNode *alloc(uint32_t fragment_size, uint32_t level) {
      for (size_t i = 0;;) {
        for (; i < _blocks.size(); i++) {
          auto fragment = _blocks[i].allocFragment(fragment_size);
          if (fragment != nullptr) {
            return fragment;
          }
        }

        // Allocate another block and keep looking for block that fits the fragment
        auto ptr = (EntryNode *)malloc(_next_block_size * sizeof(EntryNode));
        if (ptr == nullptr) {
          return nullptr;
        }
        _blocks.push_back(MemoryBlock(ptr, _next_block_size));
        _next_block_size *= 2;

        assert(i + 1 == _blocks.size());
      }

      return nullptr;
    }

    void _free(EntryNode *ptr, uint32_t fragment_size, uint32_t level) {
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

 public:
  // root,
  BitmapIndexedNode root;

  HashArrayMappedTrie() : HashArrayMappedTrie(1) {}

  HashArrayMappedTrie(uint32_t expected_count) : _count(0) {
    _seed = static_cast<uint32_t>(FOC_GET_HASH_SEED);
    _expected_count = (expected_count > 0) ? next_power_of_2(expected_count - 1) : 1;
    _generation = 0; // FIX: log2 _expected_count

    uint32_t alloc_size = detail::allocation_size(1, _generation, 0);
    assert(alloc_size >= 1);
    root.Initialize(allocator, alloc_size, 0, 0);
  }

  void Insert(const K& key, const V& value) {
    uint32_t hash = hasher.hash32(key, _seed);

    if (_count >= _expected_count) {
      _expected_count *= 2;
      _generation++;
    }
    Insert(root, key, value, _seed, hash, 0, 0);
    _count++;
  }

  const V *Find(const K& key) {
    BitmapIndexedNode *node = &root;
    uint32_t seed = _seed;
    uint32_t hash = hasher.hash32(key, _seed);
    uint32_t hash_offset = 0;
    uint32_t t = hash & 0x1f;

    while (node->position_occupied(t)) {
      auto& entry_node = node->logicalGet(t);
      if (entry_node.is_entry) {
        auto& entry = entry_node.entry();
        // Keys match!
        if (entry.key == key) {
          return &entry.value;
        }
        /* printf("%d -> %d\n", key, hash); */
        /* printf("%d -> %d\n", entry.key, hasher.hash32(entry.key, seed)); */
        return nullptr;
      }

      // The position stores a node. Keep searching.

      if (LIKELY(hash_offset < 25)) {
        hash_offset += 5;
      } else {
        hash_offset = 0;
        seed++;
        hash = hasher.hash32(key, seed);
      }

      node = &entry_node.node();
      t = (hash >> hash_offset) & 0x1f;
    }

    return nullptr;
  }

  size_t size() const { return _count; }
  bool empty() const { return _count == 0; }

 public:
  void Insert(
      BitmapIndexedNode& node,
      const K& key,
      const V& value,
      uint32_t seed,
      uint32_t hash,
      uint32_t hash_offset,
      uint32_t level) {
    uint32_t t = (hash >> hash_offset) & 0x1f;

    if (node.position_occupied(t)) {
      EntryNode *entry_node = &node.logicalGet(t);
      if (entry_node->is_entry) {
        Entry *entry = &entry_node->entry();
        if (entry->key == key) {
          // Keys match! Override the value.
          entry->value = value;
          return;
        }

        // Has to replace the entry with a node, move this entry there and
        // insert the new entry as well.

        if (LIKELY(hash_offset < 25)) {
          hash_offset += 5;
        } else {
          hash_offset = 0;
          seed++;
          hash = hasher.hash32(key, seed);
        }

        BranchOff(entry_node, seed, key, value, hash, hash_offset, level + 1);
        return;
      }

      if (LIKELY(hash_offset < 25)) {
        hash_offset += 5;
      } else {
        hash_offset = 0;
        seed++;
        hash = hasher.hash32(key, seed);
      }

      // The position stores a node. Delegate insertion.
      Insert(entry_node->node(), key, value, seed, hash, hash_offset, level + 1);
      return;
    }

    node.Insert(this, t, key, value, level);
  }

  // This function will replace the entry_node contents with a node to
  // make more room for (key, value).
  void BranchOff(
      EntryNode *old_entry_node,
      uint32_t seed,
      const K& key,
      const V& value,
      uint32_t hash,
      uint32_t hash_offset,
      uint32_t level) {
    uint32_t old_entry_hash = hasher.hash32(old_entry_node->entry().key, seed);
    // Make sure a collision happened at the parent node
    if (hash_offset >= 5) {
      assert(((old_entry_hash >> (hash_offset - 5)) & 0x1f) ==
             ((hash >> (hash_offset - 5)) & 0x1f) &&
             "BranchOff should be used when collisions are found");
      assert(old_entry_node->is_entry && "old_entry_node should be an entry");
    }

    uint32_t old_entry_hash_slice = (old_entry_hash >> hash_offset) & 0x1f;
    uint32_t hash_slice = (hash >> hash_offset) & 0x1f;

    Entry old_entry = std::move(old_entry_node->entry());

    if (UNLIKELY(old_entry_hash_slice == hash_slice)) {
      old_entry_node->InitializeAsNode(allocator, 1, 0x1 << hash_slice, level);

      if (LIKELY(hash_offset < 25)) {
        hash_offset += 5;
      } else {
        hash_offset = 0;
        seed++;
        old_entry_hash = hasher.hash32(old_entry.key, seed);
        hash = hasher.hash32(key, seed);
      }

      // We guess that the two entries will be stored in a single node,
      // so we initialize it with capacity for 2.
      old_entry_node->node().physicalGet(0).InitializeAsNode(allocator, 2, 0, level + 1);

      auto& node = old_entry_node->node().physicalGet(0).node();
      Insert(node, old_entry.key, old_entry.value, seed, old_entry_hash, hash_offset, level + 1);
      Insert(node, key, value, seed, hash, hash_offset, level + 1);
    } else {
      old_entry_node->InitializeAsNode(
          allocator,
          2,
          (0x1 << old_entry_hash_slice) | (0x1 << hash_slice), level);

      auto& new_node = old_entry_node->node();
      if (old_entry_hash_slice < hash_slice) {
        new_node.physicalGet(0) = std::move(old_entry);
        new (&new_node.physicalGet(1)) EntryNode(key, value);
      } else {
        new (&new_node.physicalGet(0)) EntryNode(key, value);
        new_node.physicalGet(1) = std::move(old_entry);
      }
    }
  }

  void print() {
    allocator.dump(_count);
    /* printf("%zu, %zu, %zu\n", _count, allocator._total_allocated, allocator._total_in_free_lists); */
  }

  Allocator allocator;

 private:
  uint32_t _count;
  uint32_t _expected_count;
  uint32_t _generation;
  uint32_t _seed;

  Hasher<K> hasher;
};

/* HashArrayMappedTrie<int, int> a; */
/* HashArrayMappedTrie<std::string, int> b; */


}  // namespace foc
