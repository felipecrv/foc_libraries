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

}  // namespace detail

// HashArrayMappedTrie

template<typename K, typename V>
class HashArrayMappedTrie {
 public:
  typedef detail::Entry<K, V> Entry;

  struct BitmapIndexedNode;

  struct EntryNode {
    bool is_entry;

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
  };

  struct BitmapIndexedNode {
    uint32_t bitmap;
    uint32_t capacity;
    EntryNode *base;

    EntryNode *Initialize(uint32_t capacity, uint32_t bitmap) {
      this->capacity = capacity;
      this->bitmap = bitmap;
      base = static_cast<EntryNode *>(malloc(capacity * sizeof(EntryNode)));
      return base;
    } 

    void Free() {
      free(base);
    }

    uint32_t size() const { return __builtin_popcount(bitmap); }

    int physical_index(int logical_index) const {
      assert(logical_index < 32);
      int32_t bitmask = 0x1 << logical_index;
      return __builtin_popcount(bitmap & (bitmask - 1));
    }

    bool position_occupied(int logical_index) const {
      assert(logical_index < 32);
      return bitmap & (0x1 << logical_index);
    }

    EntryNode &operator[](int logical_index) {
      uint32_t i = physical_index(logical_index);
      assert(i < size());
      return base[i];
    }

    EntryNode *EnsureCapacity(uint32_t c) {
      assert(c <= 32);
      if (c <= capacity) {
        return base;
      }
      if (UNLIKELY(base == nullptr)) {
        base = static_cast<EntryNode *>(malloc(c * sizeof(EntryNode)));
      } else {
        base = static_cast<EntryNode *>(realloc(base, c * sizeof(EntryNode)));
      }
      capacity = c;
      return base;
    }

    EntryNode *MakeRoomForEntry(int logical_index) {
      int i = physical_index(logical_index);
      uint32_t size = this->size();

      assert((bitmap & (0x1 << logical_index)) == 0 && "Logical index should be empty");

      EnsureCapacity(size + 1);

      int j;
      for (j = size; j > i; j--) {
        base[j] = std::move(base[j - 1]);
      }

      // Mark position as used and return pointer for actual insertion
      bitmap |= 0x1 << logical_index;
      return &base[j];
    }
  };

 public:
  // root,
  BitmapIndexedNode root;

  HashArrayMappedTrie() {
    seed = static_cast<uint32_t>(FOC_GET_HASH_SEED);
    root.Initialize(2, 0);
  }

  void Insert(const K& key, const V& value) {
    uint32_t hash = hasher.hash32(key, seed);
    Insert(root, key, value, seed, hash, 0);
  }

  const V *Find(const K& key) {
    uint32_t hash = hasher.hash32(key, seed);
    return Find(root, key, seed, hash, 0);
  }

  bool size() const { return count; }
  bool empty() const { return count == 0; }

 public:
  void Insert(
      BitmapIndexedNode& node,
      const K& key,
      const V& value,
      uint32_t seed,
      uint32_t hash,
      uint32_t hash_offset) {
    uint32_t t = (hash >> hash_offset) & 0x1f;

    if (node.position_occupied(t)) {
      EntryNode& entry_node = node[t];
      if (entry_node.is_entry) {
        Entry& entry = entry_node.entry();
        if (entry.key == key) {
          // Keys match! Override the value.
          entry.value = value;
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
        uint32_t entry_node_hash = hasher.hash32(entry.key, seed);
        BranchOff(&entry_node, seed, entry_node_hash, key, value, hash, hash_offset);
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
      Insert(entry_node.node(), key, value, seed, hash, hash_offset);
      return;
    }

    new (node.MakeRoomForEntry(t)) EntryNode(key, value);
  }

  const V *Find(
      BitmapIndexedNode& node,
      const K& key,
      uint32_t seed,
      uint32_t hash,
      uint32_t hash_offset) {
    uint32_t t = (hash >> hash_offset) & 0x1f;

    if (LIKELY(node.position_occupied(t))) {
      auto& entry_node = node[t];
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

      if (LIKELY(hash_offset < 25)) {
        hash_offset += 5;
      } else {
        hash_offset = 0;
        seed++;
        hash = hasher.hash32(key, seed);
      }

      // The position stores a node. Delegate lookup.
      return Find(entry_node.node(), key, seed, hash, hash_offset);
    }

    return nullptr;
  }

  // This function will replace the entry_node contents with a node to
  // make more room for (key, value).
  void BranchOff(
      EntryNode *entry_node,
      uint32_t seed,
      uint32_t entry_node_hash,
      const K& key,
      const V& value,
      uint32_t hash,
      uint32_t hash_offset) {
    // Make sure a collision happened at the parent node
    {
      uint32_t last_entry_node_hash = entry_node_hash;
      uint32_t last_hash = hash;
      uint32_t last_hash_offset = hash_offset;
      if (hash_offset == 0) {
        last_entry_node_hash = hasher.hash32(entry_node->entry().key, seed - 1);
        last_hash = hasher.hash32(key, seed - 1);
        last_hash_offset = 25;
        assert(false && "This code should run");
      }
      assert(((last_entry_node_hash >> (last_hash_offset - 5)) & 0x1f) ==
             ((last_hash >> (last_hash_offset - 5)) & 0x1f) &&
             "BranchOff should be used when collisions are found");
      assert(entry_node->is_entry && "entry_node should be an entry");
    }

    uint32_t entry_node_hash_slice = (entry_node_hash >> hash_offset) & 0x1f;
    uint32_t hash_slice = (hash >> hash_offset) & 0x1f;

    auto old_entry = std::move(entry_node->entry());

    if (UNLIKELY(entry_node_hash_slice == hash_slice)) {
      /* assert(false); */
    } else {
      entry_node->is_entry = false;

      auto& new_node = entry_node->node();
      new_node.Initialize(
          2, (0x1 << entry_node_hash_slice) | (0x1 << hash_slice));

      if (entry_node_hash_slice < hash_slice) {
        new_node.base[0] = std::move(old_entry);
        new (&new_node.base[1]) EntryNode(key, value);
      } else {
        new (&new_node.base[0]) EntryNode(key, value);
        new_node.base[1] = std::move(old_entry);
      }
    }
  }

 private:
  uint32_t count;
  uint32_t seed;

  Hasher<K> hasher;
};

HashArrayMappedTrie<int, int> a;
/* HashArrayMappedTrie<std::string, int> b; */


}  // namespace foc
