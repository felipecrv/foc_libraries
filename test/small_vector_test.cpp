// SmallVector unit tests.
//
// Based on llvm/unittest/ADT/SmallVectorTest.cpp

#include <algorithm>
#include <cstdarg>
#include <list>
#include <utility>

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#define SMALL_VECTOR_IMPLEMENTATION
#include "../foc/array_ref.h"
#include "../foc/small_vector.h"

#include "test_classes.h"

using foc::SmallVector;
using foc::SmallVectorImpl;
using foc::Constructable;
using foc::NonCopyable;

FOC_ATTRIBUTE_USED void CompileTest() {
  SmallVector<NonCopyable, 0> v;
  v.resize(42);
}

template <typename VectorT>
void assertEmpty(VectorT &v) {
  // Size tests
  REQUIRE(0u == v.size());
  REQUIRE(v.empty());

  // Iterator tests
  REQUIRE(v.begin() == v.end());
}

// Assert that v contains the specified values, in order.
template <typename VectorT>
void assertValuesInOrder(VectorT &v, size_t size, ...) {
  REQUIRE(size == v.size());

  va_list ap;
  va_start(ap, size);
  for (size_t i = 0; i < size; ++i) {
    int value = va_arg(ap, int);
    REQUIRE(value == v[i].getValue());
  }

  va_end(ap);
}

// Generate a sequence of values to initialize the vector.
template <typename VectorT>
void MakeSequence(VectorT &v, int start, int end) {
  for (int i = start; i <= end; ++i) {
    v.push_back(Constructable(i));
  }
}

template <typename VectorT>
struct Vectors {
  VectorT theVector;
  VectorT otherVector;
};

// New vector test.
template <class VectorT>
void emptyVectorTest() {
  Vectors<VectorT> vectors;
  assertEmpty(vectors.theVector);
  REQUIRE(vectors.theVector.rbegin() == vectors.theVector.rend());
  REQUIRE(0 == Constructable::numConstructorCalls);
  REQUIRE(0 == Constructable::getNumDestructorCalls());
}

// Simple insertions and deletions.
template <class VectorT>
void pushPopTest() {
  Vectors<VectorT> vectors;

  // Track whether the vector will potentially have to grow.
  bool RequiresGrowth = vectors.theVector.capacity() < 3;

  // Push an element
  vectors.theVector.push_back(Constructable(1));

  // Size tests
  assertValuesInOrder(vectors.theVector, 1u, 1);
  REQUIRE_FALSE(vectors.theVector.begin() == vectors.theVector.end());
  REQUIRE_FALSE(vectors.theVector.empty());

  // Push another element
  vectors.theVector.push_back(Constructable(2));
  assertValuesInOrder(vectors.theVector, 2u, 1, 2);

  // Insert at beginning
  vectors.theVector.insert(vectors.theVector.begin(), vectors.theVector[1]);
  assertValuesInOrder(vectors.theVector, 3u, 2, 1, 2);

  // Pop one element
  vectors.theVector.pop_back();
  assertValuesInOrder(vectors.theVector, 2u, 2, 1);

  // Pop remaining elements
  vectors.theVector.pop_back();
  vectors.theVector.pop_back();
  assertEmpty(vectors.theVector);

  // Check number of constructor calls. Should be 2 for each list element,
  // one for the argument to push_back, one for the argument to insert,
  // and one for the list element itself.
  if (!RequiresGrowth) {
    REQUIRE(5 == Constructable::numConstructorCalls);
    REQUIRE(5 == Constructable::getNumDestructorCalls());
  } else {
    // If we had to grow the vector, these only have a lower bound, but should
    // always be equal.
    REQUIRE(5 <= Constructable::numConstructorCalls);
    REQUIRE(Constructable::numConstructorCalls == Constructable::getNumDestructorCalls());
  }
}

// Clear test.
template <class VectorT>
void clearTest() {
  Vectors<VectorT> vectors;

  vectors.theVector.reserve(2);
  MakeSequence(vectors.theVector, 1, 2);
  vectors.theVector.clear();

  assertEmpty(vectors.theVector);
  REQUIRE(4 == Constructable::numConstructorCalls);
  REQUIRE(4 == Constructable::getNumDestructorCalls());
}

// Resize smaller test.
template <class VectorT>
void resizeShrinkTest() {
  Vectors<VectorT> vectors;

  vectors.theVector.reserve(3);
  MakeSequence(vectors.theVector, 1, 3);
  vectors.theVector.resize(1);

  assertValuesInOrder(vectors.theVector, 1u, 1);
  REQUIRE(6 == Constructable::numConstructorCalls);
  REQUIRE(5 == Constructable::getNumDestructorCalls());
}

// Resize bigger test.
template <class VectorT>
void resizeGrowTest() {
  Vectors<VectorT> vectors;

  vectors.theVector.resize(2);

  REQUIRE(2 == Constructable::numConstructorCalls);
  REQUIRE(0 == Constructable::getNumDestructorCalls());
  REQUIRE(2u == vectors.theVector.size());
}

template <class VectorT>
void resizeWithElementsTest() {
  Vectors<VectorT> vectors;
  vectors.theVector.resize(2);

  Constructable::reset();

  vectors.theVector.resize(4);

  size_t Ctors = Constructable::numConstructorCalls;
  REQUIRE((Ctors == 2 || Ctors == 4));
  size_t MoveCtors = Constructable::getNumMoveConstructorCalls();
  REQUIRE((MoveCtors == 0 || MoveCtors == 2));
  size_t Dtors = Constructable::getNumDestructorCalls();
  REQUIRE((Dtors == 0 || Dtors == 2));
}

// Resize with fill value.
template <class VectorT>
void resizeFillTest() {
  Vectors<VectorT> vectors;

  vectors.theVector.resize(3, Constructable(77));
  assertValuesInOrder(vectors.theVector, 3u, 77, 77, 77);
}

// Overflow past fixed size.
template <class VectorT>
void overflowTest() {
  Vectors<VectorT> vectors;

  // Push more elements than the fixed size.
  MakeSequence(vectors.theVector, 1, 10);

  // Test size and values.
  REQUIRE(10u == vectors.theVector.size());
  for (int i = 0; i < 10; ++i) {
    REQUIRE(i + 1 == vectors.theVector[i].getValue());
  }

  // Now resize back to fixed size.
  vectors.theVector.resize(1);

  assertValuesInOrder(vectors.theVector, 1u, 1);
}

// Iteration tests.
template <class VectorT>
void iterationTest() {
  Vectors<VectorT> vectors;
  MakeSequence(vectors.theVector, 1, 2);

  // Forward Iteration
  typename VectorT::iterator it = vectors.theVector.begin();
  REQUIRE(*it == vectors.theVector.front());
  REQUIRE(*it == vectors.theVector[0]);
  REQUIRE(1 == it->getValue());
  ++it;
  REQUIRE(*it == vectors.theVector[1]);
  REQUIRE(*it == vectors.theVector.back());
  REQUIRE(2 == it->getValue());
  ++it;
  REQUIRE(it == vectors.theVector.end());
  --it;
  REQUIRE(*it == vectors.theVector[1]);
  REQUIRE(2 == it->getValue());
  --it;
  REQUIRE(*it == vectors.theVector[0]);
  REQUIRE(1 == it->getValue());

  // Reverse Iteration
  typename VectorT::reverse_iterator rit = vectors.theVector.rbegin();
  REQUIRE(*rit == vectors.theVector[1]);
  REQUIRE(2 == rit->getValue());
  ++rit;
  REQUIRE(*rit == vectors.theVector[0]);
  REQUIRE(1 == rit->getValue());
  ++rit;
  REQUIRE(rit == vectors.theVector.rend());
  --rit;
  REQUIRE(*rit == vectors.theVector[0]);
  REQUIRE(1 == rit->getValue());
  --rit;
  REQUIRE(*rit == vectors.theVector[1]);
  REQUIRE(2 == rit->getValue());
}

// Swap test.
template <class VectorT>
void swapTest() {
  Vectors<VectorT> vectors;

  MakeSequence(vectors.theVector, 1, 2);
  std::swap(vectors.theVector, vectors.otherVector);

  assertEmpty(vectors.theVector);
  assertValuesInOrder(vectors.otherVector, 2u, 1, 2);
}

// Append test
template <class VectorT>
void appendTest() {
  Vectors<VectorT> vectors;

  MakeSequence(vectors.otherVector, 2, 3);

  vectors.theVector.push_back(Constructable(1));
  vectors.theVector.append(vectors.otherVector.begin(), vectors.otherVector.end());

  assertValuesInOrder(vectors.theVector, 3u, 1, 2, 3);
}

// Append repeated test
template <class VectorT>
void appendRepeatedTest() {
  Vectors<VectorT> vectors;

  vectors.theVector.push_back(Constructable(1));
  vectors.theVector.append(2, Constructable(77));
  assertValuesInOrder(vectors.theVector, 3u, 1, 77, 77);
}

// Assign test
template <class VectorT>
void assignTest() {
  Vectors<VectorT> vectors;

  vectors.theVector.push_back(Constructable(1));
  vectors.theVector.assign(2, Constructable(77));
  assertValuesInOrder(vectors.theVector, 2u, 77, 77);
}

// Move-assign test
template <class VectorT>
void moveAssignTest() {
  Vectors<VectorT> vectors;

  // Set up our vector with a single element, but enough capacity for 4.
  vectors.theVector.reserve(4);
  vectors.theVector.push_back(Constructable(1));

  // Set up the other vector with 2 elements.
  vectors.otherVector.push_back(Constructable(2));
  vectors.otherVector.push_back(Constructable(3));

  // Move-assign from the other vector.
  vectors.theVector = std::move(vectors.otherVector);

  // Make sure we have the right result.
  assertValuesInOrder(vectors.theVector, 2u, 2, 3);

  // Make sure the # of constructor/destructor calls line up. There
  // are two live objects after clearing the other vector.
  vectors.otherVector.clear();
  REQUIRE(Constructable::numConstructorCalls - 2 == Constructable::getNumDestructorCalls());

  // There shouldn't be any live objects any more.
  vectors.theVector.clear();
  REQUIRE(Constructable::numConstructorCalls == Constructable::getNumDestructorCalls());
}

// Erase a single element
template <class VectorT>
void eraseTest() {
  Vectors<VectorT> vectors;

  MakeSequence(vectors.theVector, 1, 3);
  const auto &theConstVector = vectors.theVector;
  vectors.theVector.erase(theConstVector.begin());
  assertValuesInOrder(vectors.theVector, 2u, 2, 3);
}

// Erase a range of elements
template <class VectorT>
void eraseRangeTest() {
  Vectors<VectorT> vectors;

  MakeSequence(vectors.theVector, 1, 3);
  const auto &theConstVector = vectors.theVector;
  vectors.theVector.erase(theConstVector.begin(), theConstVector.begin() + 2);
  assertValuesInOrder(vectors.theVector, 1u, 3);
}

// Insert a single element.
template <class VectorT>
void insertTest() {
  Vectors<VectorT> vectors;

  MakeSequence(vectors.theVector, 1, 3);
  typename VectorT::iterator I =
      vectors.theVector.insert(vectors.theVector.begin() + 1, Constructable(77));
  REQUIRE(vectors.theVector.begin() + 1 == I);
  assertValuesInOrder(vectors.theVector, 4u, 1, 77, 2, 3);
}

// Insert a copy of a single element.
template <class VectorT>
void insertCopy() {
  Vectors<VectorT> vectors;

  MakeSequence(vectors.theVector, 1, 3);
  Constructable C(77);
  typename VectorT::iterator I = vectors.theVector.insert(vectors.theVector.begin() + 1, C);
  REQUIRE(vectors.theVector.begin() + 1 == I);
  assertValuesInOrder(vectors.theVector, 4u, 1, 77, 2, 3);
}

// Insert repeated elements.
template <class VectorT>
void insertRepeatedTest() {
  Vectors<VectorT> vectors;

  MakeSequence(vectors.theVector, 1, 4);
  Constructable::reset();
  auto I = vectors.theVector.insert(vectors.theVector.begin() + 1, 2, Constructable(16));
  // Move construct the top element into newly allocated space, and optionally
  // reallocate the whole buffer, move constructing into it.
  // FIXME: This is inefficient, we shouldn't move things into newly allocated
  // space, then move them up/around, there should only be 2 or 4 move
  // constructions here.
  REQUIRE((Constructable::getNumMoveConstructorCalls() == 2 ||
           Constructable::getNumMoveConstructorCalls() == 6));
  // Move assign the next two to shift them up and make a gap.
  REQUIRE(1 == Constructable::getNumMoveAssignmentCalls());
  // Copy construct the two new elements from the parameter.
  REQUIRE(2 == Constructable::getNumCopyAssignmentCalls());
  // All without any copy construction.
  REQUIRE(0 == Constructable::getNumCopyConstructorCalls());
  REQUIRE(vectors.theVector.begin() + 1 == I);
  assertValuesInOrder(vectors.theVector, 6u, 1, 16, 16, 2, 3, 4);
}

template <class VectorT>
void insertRepeatedAtEndTest() {
  Vectors<VectorT> vectors;

  MakeSequence(vectors.theVector, 1, 4);
  Constructable::reset();
  auto I = vectors.theVector.insert(vectors.theVector.end(), 2, Constructable(16));
  // Just copy construct them into newly allocated space
  REQUIRE(2 == Constructable::getNumCopyConstructorCalls());
  // Move everything across if reallocation is needed.
  REQUIRE((Constructable::getNumMoveConstructorCalls() == 0 ||
           Constructable::getNumMoveConstructorCalls() == 4));
  // Without ever moving or copying anything else.
  REQUIRE(0 == Constructable::getNumCopyAssignmentCalls());
  REQUIRE(0 == Constructable::getNumMoveAssignmentCalls());

  REQUIRE(vectors.theVector.begin() + 4 == I);
  assertValuesInOrder(vectors.theVector, 6u, 1, 2, 3, 4, 16, 16);
}

template <class VectorT>
void insertRepeatedEmptyTest() {
  Vectors<VectorT> vectors;

  MakeSequence(vectors.theVector, 10, 15);

  // Empty insert.
  REQUIRE(vectors.theVector.end() ==
          vectors.theVector.insert(vectors.theVector.end(), 0, Constructable(42)));
  REQUIRE(vectors.theVector.begin() + 1 ==
          vectors.theVector.insert(vectors.theVector.begin() + 1, 0, Constructable(42)));
}

// Insert range.
template <class VectorT>
void insertRangeTest() {
  Vectors<VectorT> vectors;

  Constructable Arr[3] = {Constructable(77), Constructable(77), Constructable(77)};

  MakeSequence(vectors.theVector, 1, 3);
  Constructable::reset();
  auto I = vectors.theVector.insert(vectors.theVector.begin() + 1, Arr, Arr + 3);
  // Move construct the top 3 elements into newly allocated space.
  // Possibly move the whole sequence into new space first.
  // FIXME: This is inefficient, we shouldn't move things into newly allocated
  // space, then move them up/around, there should only be 2 or 3 move
  // constructions here.
  REQUIRE((Constructable::getNumMoveConstructorCalls() == 2 ||
           Constructable::getNumMoveConstructorCalls() == 5));
  // Copy assign the lower 2 new elements into existing space.
  REQUIRE(2 == Constructable::getNumCopyAssignmentCalls());
  // Copy construct the third element into newly allocated space.
  REQUIRE(1 == Constructable::getNumCopyConstructorCalls());
  REQUIRE(vectors.theVector.begin() + 1 == I);
  assertValuesInOrder(vectors.theVector, 6u, 1, 77, 77, 77, 2, 3);
}

template <class VectorT>
void insertRangeAtEndTest() {
  Vectors<VectorT> vectors;

  Constructable Arr[3] = {Constructable(77), Constructable(77), Constructable(77)};

  MakeSequence(vectors.theVector, 1, 3);

  // Insert at end.
  Constructable::reset();
  auto I = vectors.theVector.insert(vectors.theVector.end(), Arr, Arr + 3);
  // Copy construct the 3 elements into new space at the top.
  REQUIRE(3 == Constructable::getNumCopyConstructorCalls());
  // Don't copy/move anything else.
  REQUIRE(0 == Constructable::getNumCopyAssignmentCalls());
  // Reallocation might occur, causing all elements to be moved into the new
  // buffer.
  REQUIRE((Constructable::getNumMoveConstructorCalls() == 0 ||
           Constructable::getNumMoveConstructorCalls() == 3));
  REQUIRE(0 == Constructable::getNumMoveAssignmentCalls());
  REQUIRE(vectors.theVector.begin() + 3 == I);
  assertValuesInOrder(vectors.theVector, 6u, 1, 2, 3, 77, 77, 77);
}

template <class VectorT>
void insertEmptyRangeTest() {
  Vectors<VectorT> vectors;

  MakeSequence(vectors.theVector, 1, 3);

  // Empty insert.
  REQUIRE(vectors.theVector.end() == vectors.theVector.insert(vectors.theVector.end(),
                                                              vectors.theVector.begin(),
                                                              vectors.theVector.begin()));
  REQUIRE(vectors.theVector.begin() + 1 == vectors.theVector.insert(vectors.theVector.begin() + 1,
                                                                    vectors.theVector.begin(),
                                                                    vectors.theVector.begin()));
}

// Comparison tests.
template <class VectorT>
void comparisonTest() {
  Vectors<VectorT> vectors;

  MakeSequence(vectors.theVector, 1, 3);
  MakeSequence(vectors.otherVector, 1, 3);

  REQUIRE(vectors.theVector == vectors.otherVector);
  REQUIRE_FALSE(vectors.theVector != vectors.otherVector);

  vectors.otherVector.clear();
  MakeSequence(vectors.otherVector, 2, 4);

  REQUIRE_FALSE(vectors.theVector == vectors.otherVector);
  REQUIRE(vectors.theVector != vectors.otherVector);
}

// Constant vector tests.
template <class VectorT>
void constVectorTest() {
  const VectorT constVector;
  Vectors<VectorT> vectors;

  REQUIRE(0u == constVector.size());
  REQUIRE(constVector.empty());
  REQUIRE(constVector.begin() == constVector.end());
}

// Direct array access.
template <class VectorT>
void directVectorTest() {
  Vectors<VectorT> vectors;
  REQUIRE(0u == vectors.theVector.size());
  vectors.theVector.reserve(4);
  REQUIRE(4u <= vectors.theVector.capacity());
  REQUIRE(0 == Constructable::numConstructorCalls);
  vectors.theVector.push_back(1);
  vectors.theVector.push_back(2);
  vectors.theVector.push_back(3);
  vectors.theVector.push_back(4);
  REQUIRE(4u == vectors.theVector.size());
  REQUIRE(8 == Constructable::numConstructorCalls);
  REQUIRE(1 == vectors.theVector[0].getValue());
  REQUIRE(2 == vectors.theVector[1].getValue());
  REQUIRE(3 == vectors.theVector[2].getValue());
  REQUIRE(4 == vectors.theVector[3].getValue());
}

template <class VectorT>
void iteratorTest() {
  std::list<int> L;
  Vectors<VectorT> vectors;
  vectors.theVector.insert(vectors.theVector.end(), L.begin(), L.end());
}

template <typename VectorT1, typename VectorT2>
struct DualSmallVectors {
  VectorT1 theVector;
  VectorT2 otherVector;

  template <typename T, unsigned N>
  static unsigned NumBuiltinElts(const SmallVector<T, N> &) {
    return N;
  }
};

template <class VectorT1, class VectorT2>
void moveAssignmentTest() {
  DualSmallVectors<VectorT1, VectorT2> vectors;

  // Set up our vector with four elements.
  for (unsigned I = 0; I < 4; ++I)
    vectors.otherVector.push_back(Constructable(I));

  const Constructable *OrigDataPtr = vectors.otherVector.data();

  // Move-assign from the other vector.
  vectors.theVector = std::move(static_cast<SmallVectorImpl<Constructable> &>(vectors.otherVector));

  // Make sure we have the right result.
  assertValuesInOrder(vectors.theVector, 4u, 0, 1, 2, 3);

  // Make sure the # of constructor/destructor calls line up. There
  // are two live objects after clearing the other vector.
  vectors.otherVector.clear();
  REQUIRE(Constructable::numConstructorCalls - 4 == Constructable::getNumDestructorCalls());

  // If the source vector (otherVector) was in small-mode, assert that we just
  // moved the data pointer over.
  REQUIRE((vectors.NumBuiltinElts(vectors.otherVector) == 4 ||
           vectors.theVector.data() == OrigDataPtr));

  // There shouldn't be any live objects any more.
  vectors.theVector.clear();
  REQUIRE(Constructable::numConstructorCalls == Constructable::getNumDestructorCalls());

  // We shouldn't have copied anything in this whole process.
  REQUIRE(Constructable::getNumCopyConstructorCalls() == 0);
}

struct notassignable {
  int &x;
  explicit notassignable(int &x) : x(x) {}
};

void noAssignTest() {
  int x = 0;
  SmallVector<notassignable, 2> vec;
  vec.push_back(notassignable(x));
  x = 42;
  REQUIRE(42 == vec.pop_back_val().x);
}

struct MovedFrom {
  bool hasValue;
  MovedFrom() : hasValue(true) {}
  MovedFrom(MovedFrom &&m) : hasValue(m.hasValue) { m.hasValue = false; }
  MovedFrom &operator=(MovedFrom &&m) {
    hasValue = m.hasValue;
    m.hasValue = false;
    return *this;
  }
};

void midInsert() {
  SmallVector<MovedFrom, 3> v;
  v.push_back(MovedFrom());
  v.insert(v.begin(), MovedFrom());
  for (MovedFrom &m : v) {
    REQUIRE(m.hasValue);
  }
}

enum EmplaceableArgState { EAS_Defaulted, EAS_Arg, EAS_LValue, EAS_RValue, EAS_Failure };
template <int I>
struct EmplaceableArg {
  EmplaceableArgState State;
  EmplaceableArg() : State(EAS_Defaulted) {}
  EmplaceableArg(EmplaceableArg &&X) : State(X.State == EAS_Arg ? EAS_RValue : EAS_Failure) {}
  EmplaceableArg(EmplaceableArg &X) : State(X.State == EAS_Arg ? EAS_LValue : EAS_Failure) {}

  explicit EmplaceableArg(bool) : State(EAS_Arg) {}

 private:
  EmplaceableArg &operator=(EmplaceableArg &&) = delete;
  EmplaceableArg &operator=(const EmplaceableArg &) = delete;
};

enum EmplaceableState { ES_Emplaced, ES_Moved };
struct Emplaceable {
  EmplaceableArg<0> A0;
  EmplaceableArg<1> A1;
  EmplaceableArg<2> A2;
  EmplaceableArg<3> A3;
  EmplaceableState State;

  Emplaceable() : State(ES_Emplaced) {}

  template <class A0Ty>
  explicit Emplaceable(A0Ty &&A0) : A0(std::forward<A0Ty>(A0)), State(ES_Emplaced) {}

  template <class A0Ty, class A1Ty>
  Emplaceable(A0Ty &&A0, A1Ty &&A1)
      : A0(std::forward<A0Ty>(A0)), A1(std::forward<A1Ty>(A1)), State(ES_Emplaced) {}

  template <class A0Ty, class A1Ty, class A2Ty>
  Emplaceable(A0Ty &&A0, A1Ty &&A1, A2Ty &&A2)
      : A0(std::forward<A0Ty>(A0)),
        A1(std::forward<A1Ty>(A1)),
        A2(std::forward<A2Ty>(A2)),
        State(ES_Emplaced) {}

  template <class A0Ty, class A1Ty, class A2Ty, class A3Ty>
  Emplaceable(A0Ty &&A0, A1Ty &&A1, A2Ty &&A2, A3Ty &&A3)
      : A0(std::forward<A0Ty>(A0)),
        A1(std::forward<A1Ty>(A1)),
        A2(std::forward<A2Ty>(A2)),
        A3(std::forward<A3Ty>(A3)),
        State(ES_Emplaced) {}

  Emplaceable(Emplaceable &&) : State(ES_Moved) {}
  Emplaceable &operator=(Emplaceable &&) {
    State = ES_Moved;
    return *this;
  }

 private:
  Emplaceable(const Emplaceable &) = delete;
  Emplaceable &operator=(const Emplaceable &) = delete;
};

void emplaceBack() {
  EmplaceableArg<0> A0(true);
  EmplaceableArg<1> A1(true);
  EmplaceableArg<2> A2(true);
  EmplaceableArg<3> A3(true);
  {
    SmallVector<Emplaceable, 3> V;
    V.emplace_back();
    REQUIRE(V.size() == 1);
    REQUIRE(V.back().State == ES_Emplaced);
    REQUIRE(V.back().A0.State == EAS_Defaulted);
    REQUIRE(V.back().A1.State == EAS_Defaulted);
    REQUIRE(V.back().A2.State == EAS_Defaulted);
    REQUIRE(V.back().A3.State == EAS_Defaulted);
  }
  {
    SmallVector<Emplaceable, 3> V;
    V.emplace_back(std::move(A0));
    REQUIRE(V.size() == 1);
    REQUIRE(V.back().State == ES_Emplaced);
    REQUIRE(V.back().A0.State == EAS_RValue);
    REQUIRE(V.back().A1.State == EAS_Defaulted);
    REQUIRE(V.back().A2.State == EAS_Defaulted);
    REQUIRE(V.back().A3.State == EAS_Defaulted);
  }
  {
    SmallVector<Emplaceable, 3> V;
    V.emplace_back(A0);
    REQUIRE(V.size() == 1);
    REQUIRE(V.back().State == ES_Emplaced);
    REQUIRE(V.back().A0.State == EAS_LValue);
    REQUIRE(V.back().A1.State == EAS_Defaulted);
    REQUIRE(V.back().A2.State == EAS_Defaulted);
    REQUIRE(V.back().A3.State == EAS_Defaulted);
  }
  {
    SmallVector<Emplaceable, 3> V;
    V.emplace_back(A0, A1);
    REQUIRE(V.size() == 1);
    REQUIRE(V.back().State == ES_Emplaced);
    REQUIRE(V.back().A0.State == EAS_LValue);
    REQUIRE(V.back().A1.State == EAS_LValue);
    REQUIRE(V.back().A2.State == EAS_Defaulted);
    REQUIRE(V.back().A3.State == EAS_Defaulted);
  }
  {
    SmallVector<Emplaceable, 3> V;
    V.emplace_back(std::move(A0), std::move(A1));
    REQUIRE(V.size() == 1);
    REQUIRE(V.back().State == ES_Emplaced);
    REQUIRE(V.back().A0.State == EAS_RValue);
    REQUIRE(V.back().A1.State == EAS_RValue);
    REQUIRE(V.back().A2.State == EAS_Defaulted);
    REQUIRE(V.back().A3.State == EAS_Defaulted);
  }
  {
    SmallVector<Emplaceable, 3> V;
    V.emplace_back(std::move(A0), A1, std::move(A2), A3);
    REQUIRE(V.size() == 1);
    REQUIRE(V.back().State == ES_Emplaced);
    REQUIRE(V.back().A0.State == EAS_RValue);
    REQUIRE(V.back().A1.State == EAS_LValue);
    REQUIRE(V.back().A2.State == EAS_RValue);
    REQUIRE(V.back().A3.State == EAS_LValue);
  }
  {
    SmallVector<int, 1> V;
    V.emplace_back();
    V.emplace_back(42);
    REQUIRE(2U == V.size());
    REQUIRE(0 == V[0]);
    REQUIRE(42 == V[1]);
  }
}

void initializerList() {
  SmallVector<int, 2> V1 = {};
  REQUIRE(V1.empty());
  V1 = {0, 0};
  REQUIRE(makeArrayRef(V1).equals({0, 0}));
  V1 = {-1, -1};
  REQUIRE(makeArrayRef(V1).equals({-1, -1}));

  SmallVector<int, 2> V2 = {1, 2, 3, 4};
  REQUIRE(makeArrayRef(V2).equals({1, 2, 3, 4}));
  V2.assign({4});
  REQUIRE(makeArrayRef(V2).equals({4}));
  V2.append({3, 2});
  REQUIRE(makeArrayRef(V2).equals({4, 3, 2}));
  V2.insert(V2.begin() + 1, 5);
  REQUIRE(makeArrayRef(V2).equals({4, 5, 3, 2}));
}

template <class VectorT>
void smallVectorTest() {
  Constructable::reset();
  emptyVectorTest<VectorT>();
  Constructable::reset();
  pushPopTest<VectorT>();
  Constructable::reset();
  clearTest<VectorT>();
  Constructable::reset();
  resizeShrinkTest<VectorT>();
  Constructable::reset();
  resizeGrowTest<VectorT>();
  Constructable::reset();
  resizeWithElementsTest<VectorT>();
  Constructable::reset();
  resizeFillTest<VectorT>();
  Constructable::reset();
  overflowTest<VectorT>();
  Constructable::reset();
  iterationTest<VectorT>();
  Constructable::reset();
  swapTest<VectorT>();
  Constructable::reset();
  appendTest<VectorT>();
  Constructable::reset();
  appendRepeatedTest<VectorT>();
  Constructable::reset();
  assignTest<VectorT>();
  Constructable::reset();
  moveAssignTest<VectorT>();
  Constructable::reset();
  eraseTest<VectorT>();
  Constructable::reset();
  eraseRangeTest<VectorT>();
  Constructable::reset();
  insertTest<VectorT>();
  Constructable::reset();
  insertCopy<VectorT>();
  Constructable::reset();
  insertRepeatedTest<VectorT>();
  Constructable::reset();
  insertRepeatedAtEndTest<VectorT>();
  Constructable::reset();
  insertRepeatedEmptyTest<VectorT>();
  Constructable::reset();
  insertRangeTest<VectorT>();
  Constructable::reset();
  insertRangeAtEndTest<VectorT>();
  Constructable::reset();
  insertEmptyRangeTest<VectorT>();
  Constructable::reset();
  comparisonTest<VectorT>();
  Constructable::reset();
  constVectorTest<VectorT>();
  Constructable::reset();
  directVectorTest<VectorT>();
  Constructable::reset();
  iteratorTest<VectorT>();
}

TEST_CASE("SmallVectorTest", "[SmallVector]") {
  smallVectorTest<SmallVector<Constructable, 0>>();
  smallVectorTest<SmallVector<Constructable, 1>>();
  smallVectorTest<SmallVector<Constructable, 2>>();
  smallVectorTest<SmallVector<Constructable, 3>>();
  smallVectorTest<SmallVector<Constructable, 4>>();
  smallVectorTest<SmallVector<Constructable, 5>>();

  Constructable::reset();
  noAssignTest();
  Constructable::reset();
  midInsert();
  Constructable::reset();
  emplaceBack();
  Constructable::reset();
  initializerList();
}

template <class VectorT1, class VectorT2>
void dualSmallVectorTest() {
  Constructable::reset();
  moveAssignmentTest<VectorT1, VectorT2>();
}

TEST_CASE("DualSmallVectorsTest", "[SmallVector]") {
  // Small mode -> Small mode.
  dualSmallVectorTest<SmallVector<Constructable, 4>, SmallVector<Constructable, 4>>();
  // Small mode -> Big mode.
  dualSmallVectorTest<SmallVector<Constructable, 4>, SmallVector<Constructable, 2>>();
  // Big mode -> Small mode.
  dualSmallVectorTest<SmallVector<Constructable, 2>, SmallVector<Constructable, 4>>();
  // Big mode -> Big mode.
  dualSmallVectorTest<SmallVector<Constructable, 2>, SmallVector<Constructable, 2>>();
}
