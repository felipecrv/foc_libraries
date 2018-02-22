#pragma once

// clang-format off
#ifndef __has_feature
# define __has_feature(x) 0
#endif

#ifndef __has_extension
# define __has_extension(x) 0
#endif

#ifndef __has_attribute
# define __has_attribute(x) 0
#endif

#ifndef __has_builtin
# define __has_builtin(x) 0
#endif

// Extend the default __GNUC_PREREQ even if glibc's features.h isn't available.
#ifndef GNUC_PREREQ
# if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#  define GNUC_PREREQ(maj, min, patch) \
    ((__GNUC__ << 20) + (__GNUC_MINOR__ << 10) + __GNUC_PATCHLEVEL__ >= \
     ((maj) << 20) + ((min) << 10) + (patch))
# elif defined(__GNUC__) && defined(__GNUC_MINOR__)
#  define GNUC_PREREQ(maj, min, patch) \
    ((__GNUC__ << 20) + (__GNUC_MINOR__ << 10) >= ((maj) << 20) + ((min) << 10))
# else
#  define GNUC_PREREQ(maj, min, patch) 0
# endif
#endif

// Is the compiler MSVC of at least the specified version?
// The common \param version values to check for are:
//  * 1800: Microsoft Visual Studio 2013 / 12.0
//  * 1900: Microsoft Visual Studio 2015 / 14.0
#ifdef _MSC_VER
# define MSC_PREREQ(version) (_MSC_VER >= (version))
#else
# define MSC_PREREQ(version) 0
#endif

#if __has_feature(cxx_constexpr) || defined(__GXX_EXPERIMENTAL_CXX0X__) || MSC_PREREQ(1900)
# define CONSTEXPR constexpr
#else
# define CONSTEXPR
#endif

// ATTRIBUTE_ALWAYS_INLINE - On compilers where we have a directive to do so,
// mark a method "always inline" because it is performance sensitive. GCC 3.4
// supported this but is buggy in various cases and produces unimplemented
// errors, just use it in GCC 4.0 and later.
#if __has_attribute(always_inline) || GNUC_PREREQ(4, 0, 0)
# define ATTRIBUTE_ALWAYS_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER)
# define ATTRIBUTE_ALWAYS_INLINE __forceinline
#else
# define ATTRIBUTE_ALWAYS_INLINE
#endif

#if __has_attribute(returns_nonnull) || GNUC_PREREQ(4, 9, 0)
# define ATTRIBUTE_RETURNS_NONNULL __attribute__((returns_nonnull))
#elif defined(_MSC_VER)
# define ATTRIBUTE_RETURNS_NONNULL _Ret_notnull_
#else
# define ATTRIBUTE_RETURNS_NONNULL
#endif

// \macro ATTRIBUTE_RETURNS_NOALIAS Used to mark a function as returning a
// pointer that does not alias any other valid pointer.
#ifdef __GNUC__
# define ATTRIBUTE_RETURNS_NOALIAS __attribute__((__malloc__))
#elif defined(_MSC_VER)
# define ATTRIBUTE_RETURNS_NOALIAS __declspec(restrict)
#else
# define ATTRIBUTE_RETURNS_NOALIAS
#endif

#if __has_attribute(used) || GNUC_PREREQ(3, 1, 0)
# define ATTRIBUTE_USED __attribute__((__used__))
#else
# define ATTRIBUTE_USED
#endif

#if __has_attribute(warn_unused_result) || GNUC_PREREQ(3, 4, 0)
# define ATTRIBUTE_UNUSED_RESULT __attribute__((__warn_unused_result__))
#elif defined(_MSC_VER)
# define ATTRIBUTE_UNUSED_RESULT _Check_return_
#else
# define ATTRIBUTE_UNUSED_RESULT
#endif

// Some compilers warn about unused functions. When a function is sometimes
// used or not depending on build settings (e.g. a function only called from
// within "assert"), this attribute can be used to suppress such warnings.
//
// However, it shouldn't be used for unused *variables*, as those have a much
// more portable solution:
//   (void)unused_var_name;
// Prefer cast-to-void wherever it is sufficient.
#if __has_attribute(unused) || GNUC_PREREQ(3, 1, 0)
# define ATTRIBUTE_UNUSED __attribute__((__unused__))
#else
# define ATTRIBUTE_UNUSED
#endif

#if __has_builtin(__builtin_expect) || GNUC_PREREQ(4, 0, 0)
# define LIKELY(EXPR) __builtin_expect((bool)(EXPR), true)
# define UNLIKELY(EXPR) __builtin_expect((bool)(EXPR), false)
#else
# define LIKELY(EXPR) (EXPR)
# define UNLIKELY(EXPR) (EXPR)
#endif

// MEMORY_SANITIZER_BUILD
// If built with MemorySanitizer instrumentation.
#if __has_feature(memory_sanitizer)
# define MEMORY_SANITIZER_BUILD 1
# include <sanitizer/msan_interface.h>
#else
# define MEMORY_SANITIZER_BUILD 0
# define __msan_allocated_memory(p, size)
# define __msan_unpoison(p, size)
#endif

// ADDRESS_SANITIZER_BUILD
// If built with AddressSanitizer instrumentation.
#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
# define ADDRESS_SANITIZER_BUILD 1
# include <sanitizer/asan_interface.h>
#else
# define ADDRESS_SANITIZER_BUILD 0
# define __asan_poison_memory_region(p, size)
# define __asan_unpoison_memory_region(p, size)
#endif
// clang-format on

// isPodLike - This is a type trait that is used to determine whether a given
// type can be copied around with memcpy instead of running ctors etc.
template <typename T>
struct isPodLike {
// std::is_trivially_copyable is available in libc++ with clang, libstdc++
// that comes with GCC 5.
#if (__has_feature(is_trivially_copyable) && defined(_LIBCPP_VERSION)) || \
    (defined(__GNUC__) && __GNUC__ >= 5)
  // If the compiler supports the is_trivially_copyable trait use it, as it
  // matches the definition of isPodLike closely.
  static const bool value = std::is_trivially_copyable<T>::value;
#elif __has_feature(is_trivially_copyable)
  // Use the internal name if the compiler supports is_trivially_copyable but we
  // don't know if the standard library does. This is the case for clang in
  // conjunction with libstdc++ from GCC 4.x.
  static const bool value = __is_trivially_copyable(T);
#else
  // If we don't know anything else, we can (at least) assume that all non-class
  // types are PODs.
  static const bool value = !std::is_class<T>::value;
#endif
};

// std::pair's are pod-like if their elements are.
template <typename T, typename U>
struct isPodLike<std::pair<T, U> > {
  static const bool value = isPodLike<T>::value && isPodLike<U>::value;
};

// next_power_of_2 - Returns the next power of two (in 64-bits)
// that is strictly greater than x.  Returns zero on overflow.
inline uint64_t next_power_of_2(uint64_t x) {
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  x |= (x >> 32);
  return x + 1;
}
