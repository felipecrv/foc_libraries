// Copyright 2019 Felipe Oliveira Carvalho
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Copyright 2015 The Chromium Authors. All rights reserved.
#pragma once

#include <assert.h>

// A set of macros to use for OS, architecture, and compiler
// detection/configuration.

// clang-format off

// A set of macros to use for platform detection.
#if defined(ANDROID)
# define FOC_OS_ANDROID 1
#elif defined(__APPLE__)
// only include TargetConditions after testing ANDROID as some android builds
// on mac don't have this header available and it's not needed unless the target
// is really mac/ios.
# include <TargetConditionals.h>
# define FOC_OS_MACOSX 1
# define FOC_OS_MACOS 1
# if defined(FOC_TARGET_OS_IPHONE) && FOC_TARGET_OS_IPHONE
#  define FOC_OS_IOS 1
# endif  // defined(FOC_TARGET_OS_IPHONE) && FOC_TARGET_OS_IPHONE
#elif defined(__linux__)
# define FOC_OS_LINUX 1
#elif defined(_WIN32)
# define FOC_OS_WIN 1
#elif defined(__Fuchsia__)
# define FOC_OS_FUCHSIA 1
#elif defined(__FreeBSD__)
# define FOC_OS_FREEBSD 1
#elif defined(__NetBSD__)
# define FOC_OS_NETBSD 1
#elif defined(__OpenBSD__)
# define FOC_OS_OPENBSD 1
#elif defined(__sun)
# define FOC_OS_SOLARIS 1
#elif defined(__QNXNTO__)
# define FOC_OS_QNX 1
#elif defined(_AIX)
# define FOC_OS_AIX 1
#elif defined(__asmjs__)
# define FOC_OS_ASMJS
#else
# error Please add support for your platform in support.h
#endif

// For access to standard BSD features, use FOC_OS_BSD instead of a
// more specific macro.
#if defined(FOC_OS_FREEBSD) || defined(FOC_OS_NETBSD) || defined(FOC_OS_OPENBSD)
# define FOC_OS_BSD 1
#endif

// For access to standard POSIXish features, use FOC_OS_POSIX instead of a
// more specific macro.
#if defined(FOC_OS_AIX) || defined(FOC_OS_ANDROID) || defined(FOC_OS_ASMJS) ||    \
    defined(FOC_OS_FREEBSD) || defined(FOC_OS_LINUX) || defined(FOC_OS_MACOS) || \
    defined(FOC_OS_NACL) || defined(FOC_OS_NETBSD) || defined(FOC_OS_OPENBSD) ||  \
    defined(FOC_OS_QNX) || defined(FOC_OS_SOLARIS)
# define FOC_OS_POSIX 1
#endif

#if defined(FOC_OS_POSIX)
# include <errno.h>
#endif

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
# define FOC_ARCH_CPU_X86_FAMILY 1
# define FOC_ARCH_CPU_X86_64 1
# define FOC_ARCH_CPU_64_BITS 1
# define FOC_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(_M_IX86) || defined(__i386__)
# define FOC_ARCH_CPU_X86_FAMILY 1
# define FOC_ARCH_CPU_X86 1
# define FOC_ARCH_CPU_32_BITS 1
# define FOC_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__s390x__)
# define FOC_ARCH_CPU_S390_FAMILY 1
# define FOC_ARCH_CPU_S390X 1
# define FOC_ARCH_CPU_64_BITS 1
# define FOC_ARCH_CPU_BIG_ENDIAN 1
#elif defined(__s390__)
# define FOC_ARCH_CPU_S390_FAMILY 1
# define FOC_ARCH_CPU_S390 1
# define FOC_ARCH_CPU_31_BITS 1
# define FOC_ARCH_CPU_BIG_ENDIAN 1
#elif (defined(__PPC64__) || defined(__PPC__)) && defined(__BIG_ENDIAN__)
# define FOC_ARCH_CPU_PPC64_FAMILY 1
# define FOC_ARCH_CPU_PPC64 1
# define FOC_ARCH_CPU_64_BITS 1
# define FOC_ARCH_CPU_BIG_ENDIAN 1
#elif defined(__PPC64__)
# define FOC_ARCH_CPU_PPC64_FAMILY 1
# define FOC_ARCH_CPU_PPC64 1
# define FOC_ARCH_CPU_64_BITS 1
# define FOC_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__ARMEL__)
# define FOC_ARCH_CPU_ARM_FAMILY 1
# define FOC_ARCH_CPU_ARMEL 1
# define FOC_ARCH_CPU_32_BITS 1
# define FOC_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__aarch64__) || defined(_M_ARM64)
# define FOC_ARCH_CPU_ARM_FAMILY 1
# define FOC_ARCH_CPU_ARM64 1
# define FOC_ARCH_CPU_64_BITS 1
# define FOC_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__pnacl__) || defined(__asmjs__)
# define FOC_ARCH_CPU_32_BITS 1
# define FOC_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__MIPSEL__)
# if defined(__LP64__)
#  define FOC_ARCH_CPU_MIPS_FAMILY 1
#  define FOC_ARCH_CPU_MIPS64EL 1
#  define FOC_ARCH_CPU_64_BITS 1
#  define FOC_ARCH_CPU_LITTLE_ENDIAN 1
# else
#  define FOC_ARCH_CPU_MIPS_FAMILY 1
#  define FOC_ARCH_CPU_MIPSEL 1
#  define FOC_ARCH_CPU_32_BITS 1
#  define FOC_ARCH_CPU_LITTLE_ENDIAN 1
# endif
#elif defined(__MIPSEB__)
# if defined(__LP64__)
#  define FOC_ARCH_CPU_MIPS_FAMILY 1
#  define FOC_ARCH_CPU_MIPS64 1
#  define FOC_ARCH_CPU_64_BITS 1
#  define FOC_ARCH_CPU_BIG_ENDIAN 1
# else
#  define FOC_ARCH_CPU_MIPS_FAMILY 1
#  define FOC_ARCH_CPU_MIPS 1
#  define FOC_ARCH_CPU_32_BITS 1
#  define FOC_ARCH_CPU_BIG_ENDIAN 1
# endif
#else
# error Please add support for your architecture in build/build_config.h
#endif

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
#ifndef FOC_GNUC_PREREQ
# if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#  define FOC_COMPILER_GCC 1
#  define FOC_GNUC_PREREQ(maj, min, patch) \
    ((__GNUC__ << 20) + (__GNUC_MINOR__ << 10) + __GNUC_PATCHLEVEL__ >= \
     ((maj) << 20) + ((min) << 10) + (patch))
# elif defined(__GNUC__) && defined(__GNUC_MINOR__)
#  define FOC_GNUC_PREREQ(maj, min, patch) \
    ((__GNUC__ << 20) + (__GNUC_MINOR__ << 10) >= ((maj) << 20) + ((min) << 10))
# else
#  define FOC_GNUC_PREREQ(maj, min, patch) 0
# endif
#endif

// Is the compiler MSVC of at least the specified version?
// The common \param version values to check for are:
//  * 1800: Microsoft Visual Studio 2013 / 12.0
//  * 1900: Microsoft Visual Studio 2015 / 14.0
#ifdef _MSC_VER
# define FOC_COMPILER_MSVC 1
# define FOC_MSC_PREREQ(version) (_MSC_VER >= (version))
#else
# define FOC_MSC_PREREQ(version) 0
#endif

#if __has_feature(cxx_constexpr) || defined(__GXX_EXPERIMENTAL_CXX0X__) || FOC_MSC_PREREQ(1900)
# define CONSTEXPR constexpr
#else
# define CONSTEXPR
#endif

// FOC_ATTRIBUTE_NOINLINE - On compilers where we have a directive to do so,
// mark a method "not for inlining".
#if __has_attribute(noinline) || FOC_GNUC_PREREQ(3, 4, 0)
# define FOC_ATTRIBUTE_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
# define FOC_ATTRIBUTE_NOINLINE __declspec(noinline)
#else
# define FOC_ATTRIBUTE_NOINLINE
#endif

// FOC_ATTRIBUTE_ALWAYS_INLINE - On compilers where we have a directive to do
// so, mark a method "always inline" because it is performance sensitive. GCC
// 3.4 supported this but is buggy in various cases and produces unimplemented
// errors, just use it in GCC 4.0 and later.
#if __has_attribute(always_inline) || FOC_GNUC_PREREQ(4, 0, 0)
# define FOC_ATTRIBUTE_ALWAYS_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER)
# define FOC_ATTRIBUTE_ALWAYS_INLINE __forceinline
#else
# define FOC_ATTRIBUTE_ALWAYS_INLINE
#endif

#if __has_attribute(returns_nonnull) || FOC_GNUC_PREREQ(4, 9, 0)
# define FOC_ATTRIBUTE_RETURNS_NONNULL __attribute__((returns_nonnull))
#elif defined(_MSC_VER)
# define FOC_ATTRIBUTE_RETURNS_NONNULL _Ret_notnull_
#else
# define FOC_ATTRIBUTE_RETURNS_NONNULL
#endif

// \macro FOC_ATTRIBUTE_RETURNS_NOALIAS Used to mark a function as returning a
// pointer that does not alias any other valid pointer.
#ifdef __GNUC__
# define FOC_ATTRIBUTE_RETURNS_NOALIAS __attribute__((__malloc__))
#elif defined(_MSC_VER)
# define FOC_ATTRIBUTE_RETURNS_NOALIAS __declspec(restrict)
#else
# define FOC_ATTRIBUTE_RETURNS_NOALIAS
#endif

#if __has_attribute(used) || FOC_GNUC_PREREQ(3, 1, 0)
# define FOC_ATTRIBUTE_USED __attribute__((__used__))
#else
# define FOC_ATTRIBUTE_USED
#endif

#if __has_attribute(warn_unused_result) || FOC_GNUC_PREREQ(3, 4, 0)
# define FOC_ATTRIBUTE_UNUSED_RESULT __attribute__((__warn_unused_result__))
#elif defined(_MSC_VER)
# define FOC_ATTRIBUTE_UNUSED_RESULT _Check_return_
#else
# define FOC_ATTRIBUTE_UNUSED_RESULT
#endif

// Some compilers warn about unused functions. When a function is sometimes
// used or not depending on build settings (e.g. a function only called from
// within "assert"), this attribute can be used to suppress such warnings.
//
// However, it shouldn't be used for unused *variables*, as those have a much
// more portable solution:
//   (void)unused_var_name;
// Prefer cast-to-void wherever it is sufficient.
#if __has_attribute(unused) || FOC_GNUC_PREREQ(3, 1, 0)
# define FOC_ATTRIBUTE_UNUSED __attribute__((__unused__))
#else
# define FOC_ATTRIBUTE_UNUSED
#endif

#if __has_builtin(__builtin_expect) || FOC_GNUC_PREREQ(4, 0, 0)
# define FOC_LIKELY(EXPR) __builtin_expect((bool)(EXPR), true)
# define FOC_UNLIKELY(EXPR) __builtin_expect((bool)(EXPR), false)
#else
# define FOC_LIKELY(EXPR) (EXPR)
# define FOC_UNLIKELY(EXPR) (EXPR)
#endif

// FOC_MEMORY_SANITIZER_BUILD
// If built with MemorySanitizer instrumentation.
#if __has_feature(memory_sanitizer)
# define FOC_MEMORY_SANITIZER_BUILD 1
# include <sanitizer/msan_interface.h>
#else
# define FOC_MEMORY_SANITIZER_BUILD 0
# define __msan_allocated_memory(p, size)
# define __msan_unpoison(p, size)
#endif

// FOC_ADDRESS_SANITIZER_BUILD
// If built with AddressSanitizer instrumentation.
#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
# define FOC_ADDRESS_SANITIZER_BUILD 1
# include <sanitizer/asan_interface.h>
#else
# define FOC_ADDRESS_SANITIZER_BUILD 0
# define __asan_poison_memory_region(p, size)
# define __asan_unpoison_memory_region(p, size)
#endif
// clang-format on

namespace foc {

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

// Return true if the argument is a power of two > 0.
// Ex. isPowerOf2_32(0x00100000U) == true (32 bit edition.)
constexpr inline bool is_power_of2_32(uint32_t value) { return value && !(value & (value - 1)); }

// Return true if the argument is a power of two > 0 (64 bit edition.)
constexpr inline bool is_power_of2_64(uint64_t value) { return value && !(value & (value - 1)); }

// Aligns `addr` to `alignment` bytes, rounding up.
//
// alignment should be a power of two.  This method rounds up, so
// alignAddr(7, 4) == 8 and alignAddr(8, 4) == 8.
inline uintptr_t align_addr(const void *addr, size_t alignment) {
  assert(alignment && is_power_of2_64((uint64_t)alignment) && "alignment is not a power of two!");

  assert((uintptr_t)addr + alignment - 1 >= (uintptr_t)addr);

  return (((uintptr_t)addr + alignment - 1) & ~(uintptr_t)(alignment - 1));
}

/// \brief Returns the necessary adjustment for aligning ptr to alignment
/// bytes, rounding up.
inline size_t alignment_adjustment(const void *ptr, size_t alignment) {
  return align_addr(ptr, alignment) - (uintptr_t)ptr;
}

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

}  // namespace foc
