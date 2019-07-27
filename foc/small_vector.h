// 'Normally small' vectors.  Based on llvm/ADT/SmallVector.h
#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <memory>

#include "support.h"

namespace foc {

/// This is all the non-templated stuff common to all SmallVectors.
class SmallVectorBase {
 protected:
  void *begin_p, *end_p, *capacity_p;

 protected:
  SmallVectorBase(void *first_el, size_t size)
      : begin_p(first_el), end_p(first_el), capacity_p((char *)first_el + size) {}

  /// This is an implementation of the Grow() method which only works
  /// on POD-like data types and is out of line to reduce code duplication.
  void GrowPod(void *first_el, size_t min_size_in_bytes, size_t type_size);

 public:
  /// This returns size()*sizeof(T).
  size_t size_in_bytes() const { return size_t((char *)end_p - (char *)begin_p); }

  /// capacity_in_bytes - This returns capacity()*sizeof(T).
  size_t capacity_in_bytes() const { return size_t((char *)capacity_p - (char *)begin_p); }

  bool empty() const { return begin_p == end_p; }
};

template <typename T, unsigned N>
struct SmallVectorStorage;

/// This is the part of SmallVectorTemplateBase which does not depend on whether
/// the type T is a POD. The extra dummy template argument is used by ArrayRef
/// to avoid unnecessarily requiring T to be complete.
template <typename T, typename = void>
class SmallVectorTemplateCommon : public SmallVectorBase {
 private:
  template <typename, unsigned>
  friend struct SmallVectorStorage;

  // Allocate raw space for N elements of type T.  If T has a ctor or dtor, we
  // don't want it to be automatically run, so we need to represent the space as
  // something else.  Use an array of char of sufficient alignment.
  struct U {
    alignas(alignof(T)) char buffer[sizeof(T)];
  };
  // Space after 'first_el' is clobbered, do not add any instance vars after it.
  U first_el;

 protected:
  explicit SmallVectorTemplateCommon(size_t size) : SmallVectorBase(&first_el, size) {}

  void GrowPod(size_t min_size_in_bytes, size_t type_size) {
    SmallVectorBase::GrowPod(&first_el, min_size_in_bytes, type_size);
  }

  /// Return true if this is a smallvector which has not had dynamic
  /// memory allocated for it.
  bool is_small() const { return this->begin_p == static_cast<const void *>(&first_el); }

  /// Put this vector in a state of being small.
  void reset_to_small() { this->begin_p = this->end_p = this->capacity_p = &first_el; }

  void set_end(T *p) { this->end_p = p; }

 public:
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  typedef T value_type;
  typedef T *iterator;
  typedef const T *const_iterator;

  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;

  typedef T &reference;
  typedef const T &const_reference;
  typedef T *pointer;
  typedef const T *const_pointer;

  // forward iterator creation methods.
  ATTRIBUTE_ALWAYS_INLINE
  iterator begin() { return (iterator)this->begin_p; }
  ATTRIBUTE_ALWAYS_INLINE
  const_iterator begin() const { return (const_iterator)this->begin_p; }
  ATTRIBUTE_ALWAYS_INLINE
  iterator end() { return (iterator)this->end_p; }
  ATTRIBUTE_ALWAYS_INLINE
  const_iterator end() const { return (const_iterator)this->end_p; }

 protected:
  iterator capacity_ptr() { return (iterator)this->capacity_p; }
  const_iterator capacity_ptr() const { return (const_iterator)this->capacity_p; }

 public:
  // reverse iterator creation methods.
  reverse_iterator rbegin() { return reverse_iterator(end()); }
  const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
  reverse_iterator rend() { return reverse_iterator(begin()); }
  const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

  ATTRIBUTE_ALWAYS_INLINE
  size_type size() const { return end() - begin(); }
  size_type max_size() const { return size_type(-1) / sizeof(T); }

  /// Return the total number of elements in the currently allocated buffer.
  size_t capacity() const { return capacity_ptr() - begin(); }

  /// Return a pointer to the vector's buffer, even if empty().
  pointer data() { return pointer(begin()); }
  /// Return a pointer to the vector's buffer, even if empty().
  const_pointer data() const { return const_pointer(begin()); }

  ATTRIBUTE_ALWAYS_INLINE
  reference operator[](size_type idx) {
    assert(idx < size());
    return begin()[idx];
  }
  ATTRIBUTE_ALWAYS_INLINE
  const_reference operator[](size_type idx) const {
    assert(idx < size());
    return begin()[idx];
  }

  reference front() {
    assert(!empty());
    return begin()[0];
  }
  const_reference front() const {
    assert(!empty());
    return begin()[0];
  }

  reference back() {
    assert(!empty());
    return end()[-1];
  }
  const_reference back() const {
    assert(!empty());
    return end()[-1];
  }
};

/// SmallVectorTemplateBase<isPodLike = false> - This is where we put method
/// implementations that are designed to work with non-POD-like T's.
template <typename T, bool isPodLike>
class SmallVectorTemplateBase : public SmallVectorTemplateCommon<T> {
 protected:
  explicit SmallVectorTemplateBase(size_t size) : SmallVectorTemplateCommon<T>(size) {}

  static void destroy_range(T *start, T *end) {
    while (start != end) {
      --end;
      end->~T();
    }
  }

  /// Move the range [it, end) into the uninitialized memory starting with "dest",
  /// constructing elements as needed.
  template <typename It1, typename It2>
  static void uninitialized_move(It1 it, It1 end, It2 dest) {
    std::uninitialized_copy(std::make_move_iterator(it), std::make_move_iterator(end), dest);
  }

  /// Copy the range [it, end) onto the uninitialized memory starting with "dest",
  /// constructing elements as needed.
  template <typename It1, typename It2>
  static void uninitialized_copy(It1 it, It1 end, It2 dest) {
    std::uninitialized_copy(it, end, dest);
  }

  /// Grow the allocated memory (without initializing new elements), doubling
  /// the size of the allocated memory. Guarantees space for at least one more
  /// element, or min_size more elements if specified.
  void Grow(size_t min_size = 0);

 public:
  void push_back(const T &element) {
    if (UNLIKELY(this->end_p >= this->capacity_p)) {
      this->Grow();
    }
    ::new ((void *)this->end()) T(element);
    this->set_end(this->end() + 1);
  }

  void push_back(T &&element) {
    if (UNLIKELY(this->end_p >= this->capacity_p)) {
      this->Grow();
    }
    ::new ((void *)this->end()) T(::std::move(element));
    this->set_end(this->end() + 1);
  }

  void pop_back() {
    this->set_end(this->end() - 1);
    this->end()->~T();
  }
};

// Define this out-of-line to dissuade the C++ compiler from inlining it.
template <typename T, bool isPodLike>
void SmallVectorTemplateBase<T, isPodLike>::Grow(size_t min_size) {
  size_t cur_capacity = this->capacity();
  size_t cur_size = this->size();
  // Always grow, even from zero.
  size_t new_capacity = size_t(next_power_of_2(cur_capacity + 2));
  if (new_capacity < min_size) {
    new_capacity = min_size;
  }
  T *new_elements = static_cast<T *>(malloc(new_capacity * sizeof(T)));

  // Move the elements over.
  this->uninitialized_move(this->begin(), this->end(), new_elements);

  // Destroy the original elements.
  destroy_range(this->begin(), this->end());

  // If this wasn't grown from the inline copy, deallocate the old space.
  if (!this->is_small()) {
    free(this->begin());
  }

  this->set_end(new_elements + cur_size);
  this->begin_p = new_elements;
  this->capacity_p = this->begin() + new_capacity;
}

/// SmallVectorTemplateBase<isPodLike = true> - This is where we put method
/// implementations that are designed to work with POD-like T's.
template <typename T>
class SmallVectorTemplateBase<T, true> : public SmallVectorTemplateCommon<T> {
 protected:
  explicit SmallVectorTemplateBase(size_t size) : SmallVectorTemplateCommon<T>(size) {}

  // No need to do a destroy loop for POD's.
  static void destroy_range(T *, T *) {}

  /// Move the range [it, end) onto the uninitialized memory
  /// starting with "dest", constructing elements into it as needed.
  template <typename It1, typename It2>
  static void uninitialized_move(It1 it, It1 end, It2 dest) {
    // Just do a copy.
    uninitialized_copy(it, end, dest);
  }

  /// Copy the range [it, end) onto the uninitialized memory
  /// starting with "dest", constructing elements into it as needed.
  template <typename It1, typename It2>
  static void uninitialized_copy(It1 it, It1 end, It2 dest) {
    // Arbitrary iterator types; just use the basic implementation.
    std::uninitialized_copy(it, end, dest);
  }

  /// Copy the range [it, end) onto the uninitialized memory
  /// starting with "dest", constructing elements into it as needed.
  template <typename T1, typename T2>
  static void uninitialized_copy(
      T1 *it,
      T1 *end,
      T2 *dest,
      typename std::enable_if<std::is_same<typename std::remove_const<T1>::type, T2>::value>::type
          * = nullptr) {
    // Use memcpy for PODs iterated by pointers (which includes SmallVector
    // iterators): std::uninitialized_copy optimizes to memmove, but we can
    // use memcpy here. Note that it and end are iterators and thus might be
    // invalid for memcpy if they are equal.
    if (it != end) {
      memcpy(dest, it, (end - it) * sizeof(T));
    }
  }

  /// Double the size of the allocated memory, guaranteeing space for at
  /// least one more element or min_size if specified.
  void Grow(size_t min_size = 0) { this->GrowPod(min_size * sizeof(T), sizeof(T)); }

 public:
  void push_back(const T &element) {
    if (UNLIKELY(this->end_p >= this->capacity_p)) {
      this->Grow();
    }
    memcpy(this->end(), &element, sizeof(T));
    this->set_end(this->end() + 1);
  }

  void pop_back() { this->set_end(this->end() - 1); }
};

/// This class consists of common code factored out of the SmallVector class to
/// reduce code duplication based on the SmallVector 'N' template parameter.
template <typename T>
class SmallVectorImpl : public SmallVectorTemplateBase<T, isPodLike<T>::value> {
  typedef SmallVectorTemplateBase<T, isPodLike<T>::value> SuperClass;

  SmallVectorImpl(const SmallVectorImpl &) = delete;

 public:
  typedef typename SuperClass::iterator iterator;
  typedef typename SuperClass::const_iterator const_iterator;
  typedef typename SuperClass::size_type size_type;

 protected:
  // Default ctor - Initialize to empty.
  explicit SmallVectorImpl(unsigned N)
      : SmallVectorTemplateBase<T, isPodLike<T>::value>(N * sizeof(T)) {}

 public:
  ~SmallVectorImpl() {
    // Destroy the constructed elements in the vector.
    this->destroy_range(this->begin(), this->end());

    // If this wasn't grown from the inline copy, deallocate the old space.
    if (!this->is_small()) {
      free(this->begin());
    }
  }

  void clear() {
    this->destroy_range(this->begin(), this->end());
    this->end_p = this->begin_p;
  }

  void resize(size_type N) {
    if (N < this->size()) {
      this->destroy_range(this->begin() + N, this->end());
      this->set_end(this->begin() + N);
    } else if (N > this->size()) {
      if (this->capacity() < N) {
        this->Grow(N);
      }
      for (auto it = this->end(), end_p = this->begin() + N; it != end_p; ++it) {
        new (&*it) T();
      }
      this->set_end(this->begin() + N);
    }
  }

  void resize(size_type N, const T &null_value) {
    if (N < this->size()) {
      this->destroy_range(this->begin() + N, this->end());
      this->set_end(this->begin() + N);
    } else if (N > this->size()) {
      if (this->capacity() < N) {
        this->Grow(N);
      }
      std::uninitialized_fill(this->end(), this->begin() + N, null_value);
      this->set_end(this->begin() + N);
    }
  }

  void reserve(size_type N) {
    if (this->capacity() < N) {
      this->Grow(N);
    }
  }

  ATTRIBUTE_UNUSED_RESULT T pop_back_val() {
    T Result = ::std::move(this->back());
    this->pop_back();
    return Result;
  }

  void swap(SmallVectorImpl &rhs);

  /// Add the specified range to the end of the SmallVector.
  template <typename in_iter>
  void append(in_iter in_start, in_iter in_end) {
    size_type num_inputs = std::distance(in_start, in_end);
    // Grow allocated space if needed.
    if (num_inputs > size_type(this->capacity_ptr() - this->end())) {
      this->Grow(this->size() + num_inputs);
    }

    // Copy the new elements over.
    this->uninitialized_copy(in_start, in_end, this->end());
    this->set_end(this->end() + num_inputs);
  }

  /// Add the specified range to the end of the SmallVector.
  void append(size_type num_inputs, const T &element) {
    // Grow allocated space if needed.
    if (num_inputs > size_type(this->capacity_ptr() - this->end())) {
      this->Grow(this->size() + num_inputs);
    }

    // Copy the new elements over.
    std::uninitialized_fill_n(this->end(), num_inputs, element);
    this->set_end(this->end() + num_inputs);
  }

  void append(std::initializer_list<T> il) { append(il.begin(), il.end()); }

  void assign(size_type num_elements, const T &element) {
    clear();
    if (this->capacity() < num_elements) {
      this->Grow(num_elements);
    }
    this->set_end(this->begin() + num_elements);
    std::uninitialized_fill(this->begin(), this->end(), element);
  }

  void assign(std::initializer_list<T> il) {
    clear();
    append(il);
  }

  iterator erase(const_iterator cit) {
    // Just cast away constness because this is a non-const member function.
    iterator it = const_cast<iterator>(cit);

    assert(it >= this->begin() && "Iterator to erase is out of bounds.");
    assert(it < this->end() && "Erasing at past-the-end iterator.");

    iterator N = it;
    // Shift all elts down one.
    std::move(it + 1, this->end(), it);
    // Drop the last element.
    this->pop_back();
    return N;
  }

  iterator erase(const_iterator const_it_start, const_iterator const_it_end) {
    // Just cast away constness because this is a non-const member function.
    iterator start = const_cast<iterator>(const_it_start);
    iterator end = const_cast<iterator>(const_it_end);

    assert(start >= this->begin() && "Range to erase is out of bounds.");
    assert(start <= end && "Trying to erase invalid range.");
    assert(end <= this->end() && "Trying to erase past the end.");

    iterator N = start;
    // Shift all elts down.
    iterator it = std::move(end, this->end(), start);
    // Drop the last elts.
    this->destroy_range(it, this->end());
    this->set_end(it);
    return N;
  }

  iterator insert(iterator it, T &&element) {
    if (it == this->end()) {  // Important special case for empty vector.
      this->push_back(::std::move(element));
      return this->end() - 1;
    }

    assert(it >= this->begin() && "Insertion iterator is out of bounds.");
    assert(it <= this->end() && "Inserting past the end of the vector.");

    if (this->end_p >= this->capacity_p) {
      size_t elementNo = it - this->begin();
      this->Grow();
      it = this->begin() + elementNo;
    }

    ::new ((void *)this->end()) T(::std::move(this->back()));
    // Push everything else over.
    std::move_backward(it, this->end() - 1, this->end());
    this->set_end(this->end() + 1);

    // If we just moved the element we're inserting, be sure to update
    // the reference.
    T *element_ptr = &element;
    if (it <= element_ptr && element_ptr < this->end_p) {
      ++element_ptr;
    }

    *it = ::std::move(*element_ptr);
    return it;
  }

  iterator insert(iterator it, const T &element) {
    if (it == this->end()) {  // Important special case for empty vector.
      this->push_back(element);
      return this->end() - 1;
    }

    assert(it >= this->begin() && "Insertion iterator is out of bounds.");
    assert(it <= this->end() && "Inserting past the end of the vector.");

    if (this->end_p >= this->capacity_p) {
      size_t elementNo = it - this->begin();
      this->Grow();
      it = this->begin() + elementNo;
    }
    ::new ((void *)this->end()) T(std::move(this->back()));
    // Push everything else over.
    std::move_backward(it, this->end() - 1, this->end());
    this->set_end(this->end() + 1);

    // If we just moved the element we're inserting, be sure to update
    // the reference.
    const T *element_ptr = &element;
    if (it <= element_ptr && element_ptr < this->end_p) {
      ++element_ptr;
    }

    *it = *element_ptr;
    return it;
  }

  iterator insert(iterator it, size_type num_to_insert, const T &element) {
    // Convert iterator to element# to avoid invalidating iterator when we reserve()
    size_t insert_element = it - this->begin();

    if (it == this->end()) {  // Important special case for empty vector.
      append(num_to_insert, element);
      return this->begin() + insert_element;
    }

    assert(it >= this->begin() && "Insertion iterator is out of bounds.");
    assert(it <= this->end() && "Inserting past the end of the vector.");

    // Ensure there is enough space.
    reserve(this->size() + num_to_insert);

    // Uninvalidate the iterator.
    it = this->begin() + insert_element;

    // If there are more elements between the insertion point and the end of the
    // range than there are being inserted, we can use a simple approach to
    // insertion.  Since we already reserved space, we know that this won't
    // reallocate the vector.
    if (size_t(this->end() - it) >= num_to_insert) {
      T *old_end = this->end();
      append(std::move_iterator<iterator>(this->end() - num_to_insert),
             std::move_iterator<iterator>(this->end()));

      // Copy the existing elements that get replaced.
      std::move_backward(it, old_end - num_to_insert, old_end);

      std::fill_n(it, num_to_insert, element);
      return it;
    }

    // Otherwise, we're inserting more elements than exist already, and we're
    // not inserting at the end.

    // Move over the elements that we're about to overwrite.
    T *old_end = this->end();
    this->set_end(this->end() + num_to_insert);
    size_t num_overwritten = old_end - it;
    this->uninitialized_move(it, old_end, this->end() - num_overwritten);

    // Replace the overwritten part.
    std::fill_n(it, num_overwritten, element);

    // Insert the non-overwritten middle part.
    std::uninitialized_fill_n(old_end, num_to_insert - num_overwritten, element);
    return it;
  }

  template <typename IteratorType>
  iterator insert(iterator it, IteratorType from, IteratorType to) {
    // Convert iterator to element# to avoid invalidating iterator when we reserve()
    size_t insert_element = it - this->begin();

    if (it == this->end()) {  // Important special case for empty vector.
      append(from, to);
      return this->begin() + insert_element;
    }

    assert(it >= this->begin() && "Insertion iterator is out of bounds.");
    assert(it <= this->end() && "Inserting past the end of the vector.");

    size_t num_to_insert = std::distance(from, to);

    // Ensure there is enough space.
    reserve(this->size() + num_to_insert);

    // Uninvalidate the iterator.
    it = this->begin() + insert_element;

    // If there are more elements between the insertion point and the end of the
    // range than there are being inserted, we can use a simple approach to
    // insertion.  Since we already reserved space, we know that this won't
    // reallocate the vector.
    if (size_t(this->end() - it) >= num_to_insert) {
      T *old_end = this->end();
      append(std::move_iterator<iterator>(this->end() - num_to_insert),
             std::move_iterator<iterator>(this->end()));

      // Copy the existing elements that get replaced.
      std::move_backward(it, old_end - num_to_insert, old_end);

      std::copy(from, to, it);
      return it;
    }

    // Otherwise, we're inserting more elements than exist already, and we're
    // not inserting at the end.

    // Move over the elements that we're about to overwrite.
    T *old_end = this->end();
    this->set_end(this->end() + num_to_insert);
    size_t num_overwritten = old_end - it;
    this->uninitialized_move(it, old_end, this->end() - num_overwritten);

    // Replace the overwritten part.
    for (T *j = it; num_overwritten > 0; --num_overwritten) {
      *j = *from;
      ++j;
      ++from;
    }

    // Insert the non-overwritten middle part.
    this->uninitialized_copy(from, to, old_end);
    return it;
  }

  void insert(iterator it, std::initializer_list<T> il) { insert(it, il.begin(), il.end()); }

  template <typename... ArgTypes>
  void emplace_back(ArgTypes &&... Args) {
    if (UNLIKELY(this->end_p >= this->capacity_p)) {
      this->Grow();
    }
    ::new ((void *)this->end()) T(std::forward<ArgTypes>(Args)...);
    this->set_end(this->end() + 1);
  }

  SmallVectorImpl &operator=(const SmallVectorImpl &rhs);

  SmallVectorImpl &operator=(SmallVectorImpl &&rhs);

  bool operator==(const SmallVectorImpl &rhs) const {
    if (this->size() != rhs.size()) {
      return false;
    }
    return std::equal(this->begin(), this->end(), rhs.begin());
  }
  bool operator!=(const SmallVectorImpl &rhs) const { return !(*this == rhs); }

  bool operator<(const SmallVectorImpl &rhs) const {
    return std::lexicographical_compare(this->begin(), this->end(), rhs.begin(), rhs.end());
  }

  /// Set the array size to \p N, which the current array must have enough
  /// capacity for.
  ///
  /// This does not construct or destroy any elements in the vector.
  ///
  /// Clients can use this in conjunction with capacity() to write past the end
  /// of the buffer when they know that more elements are available, and only
  /// update the size later. This avoids the cost of value initializing elements
  /// which will only be overwritten.
  void set_size(size_type N) {
    assert(N <= this->capacity());
    this->set_end(this->begin() + N);
  }
};

template <typename T>
void SmallVectorImpl<T>::swap(SmallVectorImpl<T> &rhs) {
  if (this == &rhs) {
    return;
  }

  // We can only avoid copying elements if neither vector is small.
  if (!this->is_small() && !rhs.is_small()) {
    std::swap(this->begin_p, rhs.begin_p);
    std::swap(this->end_p, rhs.end_p);
    std::swap(this->capacity_p, rhs.capacity_p);
    return;
  }
  if (rhs.size() > this->capacity()) {
    this->Grow(rhs.size());
  }
  if (this->size() > rhs.capacity()) {
    rhs.Grow(this->size());
  }

  // Swap the shared elements.
  size_t num_shared = this->size();
  if (num_shared > rhs.size()) {
    num_shared = rhs.size();
  }
  for (size_type it = 0; it != num_shared; ++it) {
    std::swap((*this)[it], rhs[it]);
  }

  // Copy over the extra elts.
  if (this->size() > rhs.size()) {
    size_t element_diff = this->size() - rhs.size();
    this->uninitialized_copy(this->begin() + num_shared, this->end(), rhs.end());
    rhs.set_end(rhs.end() + element_diff);
    this->destroy_range(this->begin() + num_shared, this->end());
    this->set_end(this->begin() + num_shared);
  } else if (rhs.size() > this->size()) {
    size_t element_diff = rhs.size() - this->size();
    this->uninitialized_copy(rhs.begin() + num_shared, rhs.end(), this->end());
    this->set_end(this->end() + element_diff);
    this->destroy_range(rhs.begin() + num_shared, rhs.end());
    rhs.set_end(rhs.begin() + num_shared);
  }
}

template <typename T>
SmallVectorImpl<T> &SmallVectorImpl<T>::operator=(const SmallVectorImpl<T> &rhs) {
  // Avoid self-assignment.
  if (this == &rhs) {
    return *this;
  }

  // If we already have sufficient space, assign the common elements, then
  // destroy any excess.
  size_t rhs_size = rhs.size();
  size_t cur_size = this->size();
  if (cur_size >= rhs_size) {
    // Assign common elements.
    iterator new_end;
    if (rhs_size) {
      new_end = std::copy(rhs.begin(), rhs.begin() + rhs_size, this->begin());
    } else {
      new_end = this->begin();
    }

    // Destroy excess elements.
    this->destroy_range(new_end, this->end());

    // Trim.
    this->set_end(new_end);
    return *this;
  }

  // If we have to grow to have enough elements, destroy the current elements.
  // This allows us to avoid copying them during the grow.
  // FIXME: don't do this if they're efficiently moveable.
  if (this->capacity() < rhs_size) {
    // Destroy current elements.
    this->destroy_range(this->begin(), this->end());
    this->set_end(this->begin());
    cur_size = 0;
    this->Grow(rhs_size);
  } else if (cur_size) {
    // Otherwise, use assignment for the already-constructed elements.
    std::copy(rhs.begin(), rhs.begin() + cur_size, this->begin());
  }

  // Copy construct the new elements in place.
  this->uninitialized_copy(rhs.begin() + cur_size, rhs.end(), this->begin() + cur_size);

  // Set end.
  this->set_end(this->begin() + rhs_size);
  return *this;
}

template <typename T>
SmallVectorImpl<T> &SmallVectorImpl<T>::operator=(SmallVectorImpl<T> &&rhs) {
  // Avoid self-assignment.
  if (this == &rhs) {
    return *this;
  }

  // If the rhs isn't small, clear this vector and then steal its buffer.
  if (!rhs.is_small()) {
    this->destroy_range(this->begin(), this->end());
    if (!this->is_small()) {
      free(this->begin());
    }
    this->begin_p = rhs.begin_p;
    this->end_p = rhs.end_p;
    this->capacity_p = rhs.capacity_p;
    rhs.reset_to_small();
    return *this;
  }

  // If we already have sufficient space, assign the common elements, then
  // destroy any excess.
  size_t rhs_size = rhs.size();
  size_t cur_size = this->size();
  if (cur_size >= rhs_size) {
    // Assign common elements.
    iterator new_end = this->begin();
    if (rhs_size) {
      new_end = std::move(rhs.begin(), rhs.end(), new_end);
    }

    // Destroy excess elements and trim the bounds.
    this->destroy_range(new_end, this->end());
    this->set_end(new_end);

    // Clear the rhs.
    rhs.clear();

    return *this;
  }

  // If we have to grow to have enough elements, destroy the current elements.
  // This allows us to avoid copying them during the grow.
  // FIXME: this may not actually make any sense if we can efficiently move
  // elements.
  if (this->capacity() < rhs_size) {
    // Destroy current elements.
    this->destroy_range(this->begin(), this->end());
    this->set_end(this->begin());
    cur_size = 0;
    this->Grow(rhs_size);
  } else if (cur_size) {
    // Otherwise, use assignment for the already-constructed elements.
    std::move(rhs.begin(), rhs.begin() + cur_size, this->begin());
  }

  // Move-construct the new elements in place.
  this->uninitialized_move(rhs.begin() + cur_size, rhs.end(), this->begin() + cur_size);

  // Set end.
  this->set_end(this->begin() + rhs_size);

  rhs.clear();
  return *this;
}

/// Storage for the SmallVector elements which aren't contained in
/// SmallVectorTemplateCommon. There are 'N-1' elements here. The remaining '1'
/// element is in the base class. This is specialized for the N=1 and N=0 cases
/// to avoid allocating unnecessary storage.
template <typename T, unsigned N>
struct SmallVectorStorage {
  typename SmallVectorTemplateCommon<T>::U inline_elements[N - 1];
};
template <typename T>
struct SmallVectorStorage<T, 1> {};
template <typename T>
struct SmallVectorStorage<T, 0> {};

/// This is a 'vector' (really, a variable-sized array), optimized
/// for the case when the array is small.  It contains some number of elements
/// in-place, which allows it to avoid heap allocation when the actual number of
/// elements is below that threshold.  This allows normal "small" cases to be
/// fast without losing generality for large inputs.
///
/// Note that this does not attempt to be exception safe.
///
template <typename T, unsigned N>
class SmallVector : public SmallVectorImpl<T> {
  /// Inline space for elements which aren't stored in the base class.
  SmallVectorStorage<T, N> Storage;

 public:
  SmallVector() : SmallVectorImpl<T>(N) {}

  explicit SmallVector(size_t size, const T &value = T()) : SmallVectorImpl<T>(N) {
    this->assign(size, value);
  }

  template <typename IteratorType>
  SmallVector(IteratorType start, IteratorType end) : SmallVectorImpl<T>(N) {
    this->append(start, end);
  }

#if 0
  template <typename RangeType>
  explicit SmallVector(const llvm::iterator_range<RangeType> &range)
      : SmallVectorImpl<T>(N) {
    this->append(range.begin(), range.end());
  }
#endif

  SmallVector(std::initializer_list<T> il) : SmallVectorImpl<T>(N) { this->assign(il); }

  SmallVector(const SmallVector &rhs) : SmallVectorImpl<T>(N) {
    if (!rhs.empty()) {
      SmallVectorImpl<T>::operator=(rhs);
    }
  }

  const SmallVector &operator=(const SmallVector &rhs) {
    SmallVectorImpl<T>::operator=(rhs);
    return *this;
  }

  SmallVector(SmallVector &&rhs) : SmallVectorImpl<T>(N) {
    if (!rhs.empty()) {
      SmallVectorImpl<T>::operator=(::std::move(rhs));
    }
  }

  const SmallVector &operator=(SmallVector &&rhs) {
    SmallVectorImpl<T>::operator=(::std::move(rhs));
    return *this;
  }

  SmallVector(SmallVectorImpl<T> &&rhs) : SmallVectorImpl<T>(N) {
    if (!rhs.empty()) {
      SmallVectorImpl<T>::operator=(::std::move(rhs));
    }
  }

  const SmallVector &operator=(SmallVectorImpl<T> &&rhs) {
    SmallVectorImpl<T>::operator=(::std::move(rhs));
    return *this;
  }

  const SmallVector &operator=(std::initializer_list<T> il) {
    this->assign(il);
    return *this;
  }
};

template <typename T, unsigned N>
static inline size_t capacity_in_bytes(const SmallVector<T, N> &x) {
  return x.capacity_in_bytes();
}

}  // namespace foc

namespace std {
/// Implement std::swap in terms of SmallVector swap.
template <typename T>
inline void swap(foc::SmallVectorImpl<T> &lhs, foc::SmallVectorImpl<T> &rhs) {
  lhs.swap(rhs);
}

/// Implement std::swap in terms of SmallVector swap.
template <typename T, unsigned N>
inline void swap(foc::SmallVector<T, N> &lhs, foc::SmallVector<T, N> &rhs) {
  lhs.swap(rhs);
}
}  // namespace std

#ifdef SMALL_VECTOR_IMPLEMENTATION

/// GrowPod - This is an implementation of the Grow() method which only works
/// on POD-like datatypes and is out of line to reduce code duplication.
void foc::SmallVectorBase::GrowPod(void *first_el, size_t min_size_in_bytes, size_t type_size) {
  size_t cur_size_in_bytes = size_in_bytes();
  // Always grow
  size_t new_capacity_in_bytes = 2 * capacity_in_bytes() + type_size;
  if (new_capacity_in_bytes < min_size_in_bytes) {
    new_capacity_in_bytes = min_size_in_bytes;
  }

  void *new_elements;
  if (this->begin_p == first_el) {
    new_elements = malloc(new_capacity_in_bytes);

    // Copy the elements over.  No need to run dtors on PODs.
    memcpy(new_elements, this->begin_p, cur_size_in_bytes);
  } else {
    // If this wasn't grown from the inline copy, grow the allocated space.
    new_elements = realloc(this->begin_p, new_capacity_in_bytes);
  }
  assert(new_elements && "Out of memory");

  this->end_p = (char *)new_elements + cur_size_in_bytes;
  this->begin_p = new_elements;
  this->capacity_p = (char *)this->begin_p + new_capacity_in_bytes;
}

#endif  // SMALL_VECTOR_IMPLEMENTATION
