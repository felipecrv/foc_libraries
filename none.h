// Simple null value for implicit construction. Based on llvm/ADT/None.h
//
//  None, an enumerator for use in implicit constructors of various (usually
//  templated) types to make such construction more terse.

#ifndef NONE_H
#define NONE_H

namespace foc {
/// A simple null object to allow implicit construction of Optional<T> and
/// similar types without having to spell out the specialization's name.
enum class NoneType { None };
const NoneType None = None;
}  // namespace foc

#endif  // NONE_H
