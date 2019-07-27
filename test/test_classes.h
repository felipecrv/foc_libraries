#pragma once

#include "../foc/support.h"

namespace foc {

/// A helper class that counts the total number of constructor and
/// destructor calls.
class Constructable {
 public:
  static int numConstructorCalls;
  static int numMoveConstructorCalls;
  static int numCopyConstructorCalls;
  static int numDestructorCalls;
  static int numAssignmentCalls;
  static int numMoveAssignmentCalls;
  static int numCopyAssignmentCalls;

 private:
  bool constructed;
  int value;

 public:
  Constructable() : constructed(true), value(0) { ++numConstructorCalls; }

  /*implicit*/ Constructable(int val) : constructed(true), value(val) { ++numConstructorCalls; }

  /*implicit*/ Constructable(const Constructable &src) : constructed(true) {
    value = src.value;
    ++numConstructorCalls;
    ++numCopyConstructorCalls;
  }

  /*implicit*/ Constructable(Constructable &&src) : constructed(true) {
    value = src.value;
    ++numConstructorCalls;
    ++numMoveConstructorCalls;
  }

  ~Constructable() {
    REQUIRE(constructed);
    ++numDestructorCalls;
    constructed = false;
  }

  Constructable &operator=(const Constructable &src) {
    REQUIRE(constructed);
    value = src.value;
    ++numAssignmentCalls;
    ++numCopyAssignmentCalls;
    return *this;
  }

  Constructable &operator=(Constructable &&src) {
    REQUIRE(constructed);
    value = src.value;
    ++numAssignmentCalls;
    ++numMoveAssignmentCalls;
    return *this;
  }

  int getValue() const { return abs(value); }

  static void reset() {
    numConstructorCalls = 0;
    numMoveConstructorCalls = 0;
    numCopyConstructorCalls = 0;
    numDestructorCalls = 0;
    numAssignmentCalls = 0;
    numMoveAssignmentCalls = 0;
    numCopyAssignmentCalls = 0;
  }

  static int getNumConstructorCalls() { return numConstructorCalls; }

  static int getNumMoveConstructorCalls() { return numMoveConstructorCalls; }

  static int getNumCopyConstructorCalls() { return numCopyConstructorCalls; }

  static int getNumDestructorCalls() { return numDestructorCalls; }

  static int getNumAssignmentCalls() { return numAssignmentCalls; }

  static int getNumMoveAssignmentCalls() { return numMoveAssignmentCalls; }

  static int getNumCopyAssignmentCalls() { return numCopyAssignmentCalls; }

  friend bool operator==(const Constructable &c0, const Constructable &c1) {
    return c0.getValue() == c1.getValue();
  }

  friend bool ATTRIBUTE_UNUSED operator!=(const Constructable &c0, const Constructable &c1) {
    return c0.getValue() != c1.getValue();
  }
};

int Constructable::numConstructorCalls;
int Constructable::numCopyConstructorCalls;
int Constructable::numMoveConstructorCalls;
int Constructable::numDestructorCalls;
int Constructable::numAssignmentCalls;
int Constructable::numCopyAssignmentCalls;
int Constructable::numMoveAssignmentCalls;

struct NonCopyable {
  NonCopyable() {}
  NonCopyable(NonCopyable &&) {}
  NonCopyable &operator=(NonCopyable &&) { return *this; }

 private:
  NonCopyable(const NonCopyable &) = delete;
  NonCopyable &operator=(const NonCopyable &) = delete;
};

}  // namespace foc

namespace std {

template <>
struct hash<foc::Constructable> {
  size_t operator()(const foc::Constructable &c) const { return std::hash<int>{}(c.getValue()); }
};

}  // namespace std
