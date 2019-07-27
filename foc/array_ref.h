// Array Reference Wrapper.  Based on llvm/ADT/ArrayRef.h

#pragma once

#include <algorithm>
#include <vector>

/* #include "llvm/ADT/Hashing.h" */
#include "none.h"
#include "small_vector.h"

namespace foc {
/// ArrayRef - Represent a constant reference to an array (0 or more elements
/// consecutively in memory), i.e. a start pointer and a length.  It allows
/// various APIs to take consecutive elements easily and conveniently.
///
/// This class does not own the underlying data, it is expected to be used in
/// situations where the data resides in some other buffer, whose lifetime
/// extends past that of the ArrayRef. For this reason, it is not in general
/// safe to store an ArrayRef.
///
/// This is intended to be trivially copyable, so it should be passed by
/// value.
template <typename T>
class ArrayRef {
 public:
  typedef const T *iterator;
  typedef const T *const_iterator;
  typedef size_t size_type;

  typedef std::reverse_iterator<iterator> reverse_iterator;

 private:
  /// The start of the array, in an external buffer.
  const T *_data;

  /// The number of elements.
  size_type length;

 public:
  /// @name Constructors
  /// @{

  /// Construct an empty ArrayRef.
  /*implicit*/ ArrayRef() : _data(nullptr), length(0) {}

  /// Construct an empty ArrayRef from None.
  /*implicit*/ ArrayRef(NoneType) : _data(nullptr), length(0) {}

  /// Construct an ArrayRef from a single element.
  /*implicit*/ ArrayRef(const T &one_element) : _data(&one_element), length(1) {}

  /// Construct an ArrayRef from a pointer and length.
  /*implicit*/ ArrayRef(const T *ptr, size_t _length) : _data(ptr), length(_length) {}

  /// Construct an ArrayRef from a range.
  ArrayRef(const T *begin, const T *end) : _data(begin), length(end - begin) {}

  /// Construct an ArrayRef from a SmallVector. This is templated in order to
  /// avoid instantiating SmallVectorTemplateCommon<T> whenever we
  /// copy-construct an ArrayRef.
  template <typename U>
  /*implicit*/ ArrayRef(const SmallVectorTemplateCommon<T, U> &vec)
      : _data(vec.data()), length(vec.size()) {}

  /// Construct an ArrayRef from a std::vector.
  template <typename A>
  /*implicit*/ ArrayRef(const std::vector<T, A> &vec) : _data(vec.data()), length(vec.size()) {}

  /// Construct an ArrayRef from a C array.
  template <size_t N>
  /*implicit*/ CONSTEXPR ArrayRef(const T (&arr)[N]) : _data(arr), length(N) {}

  /// Construct an ArrayRef from a std::initializer_list.
  /*implicit*/ ArrayRef(const std::initializer_list<T> &vec)
      : _data(vec.begin() == vec.end() ? (T *)nullptr : vec.begin()), length(vec.size()) {}

  /// Construct an ArrayRef<const T*> from ArrayRef<T*>. This uses SFINAE to
  /// ensure that only ArrayRefs of pointers can be converted.
  template <typename U>
  ArrayRef(
      const ArrayRef<U *> &a,
      typename std::enable_if<std::is_convertible<U *const *, T const *>::value>::type * = nullptr)
      : _data(a.data()), length(a.size()) {}

  /// Construct an ArrayRef<const T*> from a SmallVector<T*>. This is
  /// templated in order to avoid instantiating SmallVectorTemplateCommon<T>
  /// whenever we copy-construct an ArrayRef.
  template <typename U, typename DummyT>
  /*implicit*/ ArrayRef(
      const SmallVectorTemplateCommon<U *, DummyT> &vec,
      typename std::enable_if<std::is_convertible<U *const *, T const *>::value>::type * = nullptr)
      : _data(vec.data()), length(vec.size()) {}

  /// Construct an ArrayRef<const T*> from std::vector<T*>. This uses SFINAE
  /// to ensure that only vectors of pointers can be converted.
  template <typename U, typename A>
  ArrayRef(const std::vector<U *, A> &vec,
           typename std::enable_if<std::is_convertible<U *const *, T const *>::value>::type * = 0)
      : _data(vec.data()), length(vec.size()) {}

  /// @}
  /// @name Simple Operations
  /// @{

  iterator begin() const { return _data; }
  iterator end() const { return _data + length; }

  reverse_iterator rbegin() const { return reverse_iterator(end()); }
  reverse_iterator rend() const { return reverse_iterator(begin()); }

  /// empty - Check if the array is empty.
  bool empty() const { return length == 0; }

  const T *data() const { return _data; }

  /// size - Get the array size.
  size_t size() const { return length; }

  /// front - Get the first element.
  const T &front() const {
    assert(!empty());
    return _data[0];
  }

  /// back - Get the last element.
  const T &back() const {
    assert(!empty());
    return _data[length - 1];
  }

  // copy - Allocate copy in Allocator and return ArrayRef<T> to it.
  template <typename Allocator>
  ArrayRef<T> copy(Allocator &allocator) {
    T *buffer = allocator.template Allocate<T>(length);
    std::uninitialized_copy(begin(), end(), buffer);
    return ArrayRef<T>(buffer, length);
  }

  /// equals - Check for element-wise equality.
  bool equals(ArrayRef rhs) const {
    if (length != rhs.length) {
      return false;
    }
    return std::equal(begin(), end(), rhs.begin());
  }

  /// slice(n) - Chop off the first n elements of the array.
  ArrayRef<T> slice(size_t n) const {
    assert(n <= size() && "Invalid specifier");
    return ArrayRef<T>(data() + n, size() - n);
  }

  /// slice(n, m) - Chop off the first N elements of the array, and keep M
  /// elements in the array.
  ArrayRef<T> slice(size_t n, size_t m) const {
    assert(n + m <= size() && "Invalid specifier");
    return ArrayRef<T>(data() + n, m);
  }

  /// \brief Drop the first \p n elements of the array.
  ArrayRef<T> drop_front(size_t n = 1) const {
    assert(size() >= n && "Dropping more elements than exist");
    return slice(n, size() - n);
  }

  /// \brief Drop the last \p n elements of the array.
  ArrayRef<T> drop_back(size_t n = 1) const {
    assert(size() >= n && "Dropping more elements than exist");
    return slice(0, size() - n);
  }

  /// @}
  /// @name Operator Overloads
  /// @{
  const T &operator[](size_t index) const {
    assert(index < length && "Invalid index!");
    return _data[index];
  }

  /// @}
  /// @name Expensive Operations
  /// @{
  std::vector<T> vec() const { return std::vector<T>(_data, _data + length); }

  /// @}
  /// @name Conversion operators
  /// @{
  operator std::vector<T>() const { return std::vector<T>(_data, _data + length); }

  /// @}
};

/// MutableArrayRef - Represent a mutable reference to an array (0 or more
/// elements consecutively in memory), i.e. a start pointer and a length.  It
/// allows various APIs to take and modify consecutive elements easily and
/// conveniently.
///
/// This class does not own the underlying data, it is expected to be used in
/// situations where the data resides in some other buffer, whose lifetime
/// extends past that of the MutableArrayRef. For this reason, it is not in
/// general safe to store a MutableArrayRef.
///
/// This is intended to be trivially copyable, so it should be passed by
/// value.
template <typename T>
class MutableArrayRef : public ArrayRef<T> {
 public:
  typedef T *iterator;

  typedef std::reverse_iterator<iterator> reverse_iterator;

  /// Construct an empty MutableArrayRef.
  /*implicit*/ MutableArrayRef() : ArrayRef<T>() {}

  /// Construct an empty MutableArrayRef from None.
  /*implicit*/ MutableArrayRef(NoneType) : ArrayRef<T>() {}

  /// Construct an MutableArrayRef from a single element.
  /*implicit*/ MutableArrayRef(T &one_element) : ArrayRef<T>(one_element) {}

  /// Construct an MutableArrayRef from a pointer and length.
  /*implicit*/ MutableArrayRef(T *data, size_t length) : ArrayRef<T>(data, length) {}

  /// Construct an MutableArrayRef from a range.
  MutableArrayRef(T *begin, T *end) : ArrayRef<T>(begin, end) {}

  /// Construct an MutableArrayRef from a SmallVector.
  /*implicit*/ MutableArrayRef(SmallVectorImpl<T> &vec) : ArrayRef<T>(vec) {}

  /// Construct a MutableArrayRef from a std::vector.
  /*implicit*/ MutableArrayRef(std::vector<T> &vec) : ArrayRef<T>(vec) {}

  /// Construct an MutableArrayRef from a C array.
  template <size_t N>
  /*implicit*/ CONSTEXPR MutableArrayRef(T (&arr)[N]) : ArrayRef<T>(arr) {}

  T *data() const { return const_cast<T *>(ArrayRef<T>::data()); }

  iterator begin() const { return data(); }
  iterator end() const { return data() + this->size(); }

  reverse_iterator rbegin() const { return reverse_iterator(end()); }
  reverse_iterator rend() const { return reverse_iterator(begin()); }

  /// front - Get the first element.
  T &front() const {
    assert(!this->empty());
    return data()[0];
  }

  /// back - Get the last element.
  T &back() const {
    assert(!this->empty());
    return data()[this->size() - 1];
  }

  /// slice(n) - Chop off the first n elements of the array.
  MutableArrayRef<T> slice(size_t n) const {
    assert(n <= this->size() && "Invalid specifier");
    return MutableArrayRef<T>(data() + n, this->size() - n);
  }

  /// slice(n, m) - Chop off the first n elements of the array, and keep M
  /// elements in the array.
  MutableArrayRef<T> slice(size_t n, size_t m) const {
    assert(n + m <= this->size() && "Invalid specifier");
    return MutableArrayRef<T>(data() + n, m);
  }

  /// \brief Drop the first \p n elements of the array.
  MutableArrayRef<T> drop_front(size_t n = 1) const {
    assert(this->size() >= n && "Dropping more elements than exist");
    return slice(n, this->size() - n);
  }

  MutableArrayRef<T> drop_back(size_t n = 1) const {
    assert(this->size() >= n && "Dropping more elements than exist");
    return slice(0, this->size() - n);
  }

  /// @}
  /// @name Operator Overloads
  /// @{
  T &operator[](size_t index) const {
    assert(index < this->size() && "Invalid index!");
    return data()[index];
  }
};

/// @name ArrayRef Convenience constructors
/// @{

/// Construct an ArrayRef from a single element.
template <typename T>
ArrayRef<T> makeArrayRef(const T &one_element) {
  return one_element;
}

/// Construct an ArrayRef from a pointer and length.
template <typename T>
ArrayRef<T> makeArrayRef(const T *data, size_t length) {
  return ArrayRef<T>(data, length);
}

/// Construct an ArrayRef from a range.
template <typename T>
ArrayRef<T> makeArrayRef(const T *begin, const T *end) {
  return ArrayRef<T>(begin, end);
}

/// Construct an ArrayRef from a SmallVector.
template <typename T>
ArrayRef<T> makeArrayRef(const SmallVectorImpl<T> &vec) {
  return vec;
}

/// Construct an ArrayRef from a SmallVector.
template <typename T, unsigned N>
ArrayRef<T> makeArrayRef(const SmallVector<T, N> &vec) {
  return vec;
}

/// Construct an ArrayRef from a std::vector.
template <typename T>
ArrayRef<T> makeArrayRef(const std::vector<T> &vec) {
  return vec;
}

/// Construct an ArrayRef from an ArrayRef (no-op) (const)
template <typename T>
ArrayRef<T> makeArrayRef(const ArrayRef<T> &vec) {
  return vec;
}

/// Construct an ArrayRef from an ArrayRef (no-op)
template <typename T>
ArrayRef<T> &makeArrayRef(ArrayRef<T> &vec) {
  return vec;
}

/// Construct an ArrayRef from a C array.
template <typename T, size_t N>
ArrayRef<T> makeArrayRef(const T (&arr)[N]) {
  return ArrayRef<T>(arr);
}

/// @}
/// @name ArrayRef Comparison Operators
/// @{

template <typename T>
inline bool operator==(ArrayRef<T> lhs, ArrayRef<T> rhs) {
  return lhs.equals(rhs);
}

template <typename T>
inline bool operator!=(ArrayRef<T> lhs, ArrayRef<T> rhs) {
  return !(lhs == rhs);
}

/// @}

// ArrayRefs can be treated like a POD type.
/*
   TODO(philix): define isPodLike as a type trait
template <typename T> struct isPodLike;
template <typename T> struct isPodLike<ArrayRef<T> > {
  static const bool value = true;
};
*/

/*
TODO(philix): bring hashing from llvm/ADT
template <typename T> hash_code hash_value(ArrayRef<T> S) {
  return hash_combine_range(S.begin(), S.end());
}
*/
}  // end namespace foc
