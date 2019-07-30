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

// ## [Unreleased]
//
// ## [0.0.1] - 2019-07-30
// 
// - Get code from Chromium's base/logging.h and base/logging.cc
// - Use std::string and char for PathString and PathChar (needs testing on
// Windows)
// - Include the OS and ARCH macros into support.h with an FOC_ prefix and
// make sure the prefix is used here as well.
// - Drop NACL support
// - Drop LogAssertHandlerFunction functionality
// - Always lock the log file on POSIX, remove the configuration option for this
// - Drop VerifyDebugger() (a function used to check GDB configuration)
//
// [0.0.2]: https://github.com/philix/foc_libraries/compare/v0.0.1...v0.0.2
// [0.0.1]: https://github.com/philix/foc_libraries/releases/tag/v0.0.1

#pragma once

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <ostream>
#include <sstream>
#include <string>
#include "debugger.h"
#include "support.h"

#ifdef FOC_LOGGING_IMPLEMENTATION
# include <vector>
#endif

#if defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
# include <sys/time.h>  // for gettimeofday
#endif

#if defined(FOC_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif
#endif  // FOC_OS_MACOS

// Provides system-dependent string type conversions for cases where it's
// necessary to not use ICU. Generally, you should not need this in Chrome,
// but it is used in some shared code. Dependencies should be minimal.

/* #include "base/base_export.h" */
/* #include "base/strings/string16.h" */
/* #include "base/strings/string_piece.h" */
/* #include "build/build_config.h" */

namespace foc {
namespace strings {

size_t strlcpy(char* dst, const char* src, size_t dst_size) {
  for (size_t i = 0; i < dst_size; ++i) {
    if ((dst[i] = src[i]) == 0)  // We hit and copied the terminating NULL.
      return i;
  }

  // We were left off at dst_size.  We over copied 1 byte.  Null terminate.
  if (dst_size != 0)
    dst[dst_size - 1] = 0;

  // Count the rest of the |src|, and return it's length in characters.
  while (src[dst_size])
    ++dst_size;
  return dst_size;
}

// Converts between wide and UTF-8 representations of a string. On error, the
// result is system-dependent.
/* std::string SysWideToUTF8(const std::wstring& wide); */
/* std::wstring SysUTF8ToWide(StringPiece utf8); */

// Converts between wide and the system multi-byte representations of a string.
// DANGER: This will lose information and can change (on Windows, this can
// change between reboots).
/* std::string SysWideToNativeMB(const std::wstring& wide); */
/* std::wstring SysNativeMBToWide(StringPiece native_mb); */

// Windows-specific ------------------------------------------------------------

#if defined(FOC_OS_WIN)

// Converts between 8-bit and wide strings, using the given code page. The
// code page identifier is one accepted by the Windows function
// MultiByteToWideChar().
/* std::wstring SysMultiByteToWide(StringPiece mb, uint32_t code_page); */
/* std::string SysWideToMultiByte(const std::wstring& wide, */
/*                                            uint32_t code_page); */

#endif  // defined(FOC_OS_WIN)

// Mac-specific ----------------------------------------------------------------

#if defined(FOC_OS_MACOS)

// Converts between STL strings and CFStringRefs/NSStrings.

// Creates a string, and returns it with a refcount of 1. You are responsible
// for releasing it. Returns NULL on failure.
/* CFStringRef SysUTF8ToCFStringRef(StringPiece utf8); */
/* CFStringRef SysUTF16ToCFStringRef(StringPiece16 utf16); */

// Same, but returns an autoreleased NSString.
/* NSString* SysUTF8ToNSString(StringPiece utf8); */
/* NSString* SysUTF16ToNSString(StringPiece16 utf16); */

// Converts a CFStringRef to an STL string. Returns an empty string on failure.
std::string SysCFStringRefToUTF8(CFStringRef ref);
/* string16 SysCFStringRefToUTF16(CFStringRef ref); */

// Same, but accepts NSString input. Converts nil NSString* to the appropriate
// string type of length 0.
/* std::string SysNSStringToUTF8(NSString* ref); */
/* string16 SysNSStringToUTF16(NSString* ref); */

#endif  // defined(FOC_OS_MACOS)
}  // namespace strings
}  // namespace foc

namespace foc {
namespace strings {
#ifdef FOC_LOGGING_IMPLEMENTATION

namespace {

// Convert the supplied CFString into the specified encoding, and return it as
// an STL string of the template type.  Returns an empty string on failure.
//
// Do not assert in this function since it is used by the asssertion code!
template<typename StringType>
static StringType CFStringToSTLStringWithEncodingT(CFStringRef cfstring,
                                                   CFStringEncoding encoding) {
  CFIndex length = CFStringGetLength(cfstring);
  if (length == 0)
    return StringType();

  CFRange whole_string = CFRangeMake(0, length);
  CFIndex out_size;
  CFIndex converted = CFStringGetBytes(cfstring,
                                       whole_string,
                                       encoding,
                                       0,      // lossByte
                                       false,  // isExternalRepresentation
                                       NULL,   // buffer
                                       0,      // maxBufLen
                                       &out_size);
  if (converted == 0 || out_size == 0)
    return StringType();

  // out_size is the number of UInt8-sized units needed in the destination.
  // A buffer allocated as UInt8 units might not be properly aligned to
  // contain elements of StringType::value_type.  Use a container for the
  // proper value_type, and convert out_size by figuring the number of
  // value_type elements per UInt8.  Leave room for a NUL terminator.
  typename StringType::size_type elements =
      out_size * sizeof(UInt8) / sizeof(typename StringType::value_type) + 1;

  std::vector<typename StringType::value_type> out_buffer(elements);
  converted = CFStringGetBytes(cfstring,
                               whole_string,
                               encoding,
                               0,      // lossByte
                               false,  // isExternalRepresentation
                               reinterpret_cast<UInt8*>(&out_buffer[0]),
                               out_size,
                               NULL);  // usedBufLen
  if (converted == 0)
    return StringType();

  out_buffer[elements - 1] = '\0';
  return StringType(&out_buffer[0], elements - 1);
}

}  // namespace

std::string SysCFStringRefToUTF8(CFStringRef ref) {
  return CFStringToSTLStringWithEncodingT<std::string>(
    ref, kCFStringEncodingUTF8);
}

#endif  // !FOC_LOGGING_IMPLEMENTATION
}  // namespace strings
}  // namespace foc

namespace foc {
namespace internal {

// Uses expression SFINAE to detect whether using operator<< would work.
template <typename T, typename = void>
struct SupportsOstreamOperator : std::false_type {};

template <typename T>
struct SupportsOstreamOperator<T,
                               decltype(void(std::declval<std::ostream&>() << std::declval<T>()))>
    : std::true_type {};

template <typename T, typename = void>
struct SupportsToString : std::false_type {};

template <typename T>
struct SupportsToString<T, decltype(void(std::declval<T>().ToString()))> : std::true_type {};

// ScopedClearLastError stores and resets the value of thread local error codes
// (errno, GetLastError()), and restores them in the destructor. This is useful
// to avoid side effects on these values in instrumentation functions that
// interact with the OS.

// Common implementation of ScopedClearLastError for all platforms. Use
// ScopedClearLastError instead.
class ScopedClearLastErrorBase {
 public:
  ScopedClearLastErrorBase() : _last_errno(errno) { errno = 0; }
  ~ScopedClearLastErrorBase() { errno = _last_errno; }

 private:
  const int _last_errno;

  FOC_DISALLOW_COPY_AND_ASSIGN(ScopedClearLastErrorBase);
};

#if defined(FOC_OS_WIN)

// Windows specific implementation of ScopedClearLastError.
class ScopedClearLastError : public ScopedClearLastErrorBase {
 public:
  ScopedClearLastError();
  ~ScopedClearLastError();

 private:
  unsigned int _last_system_error;

  FOC_DISALLOW_COPY_AND_ASSIGN(ScopedClearLastError);
};

#ifdef FOC_LOGGING_IMPLEMENTATION

ScopedClearLastError::ScopedClearLastError() : _last_system_error(::GetLastError()) {
  ::SetLastError(0);
}

ScopedClearLastError::~ScopedClearLastError() { ::SetLastError(_last_system_error); }

#endif  // FOC_LOGGING_IMPLEMENTATION

#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)

using ScopedClearLastError = ScopedClearLastErrorBase;

#endif  // defined(FOC_OS_WIN)

}  // namespace internal
}  // namespace foc

namespace foc {
namespace logging {

#if defined(FOC_OS_WIN)
typedef base::char16 PathChar;
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
typedef char PathChar;
#endif

// A bitmask of potential logging destinations.
using LoggingDestination = uint32_t;

// Specifies where logs will be written. Multiple destinations can be specified
// with bitwise OR.
// Unless destination is LOG_NONE, all logs with severity ERROR and above will
// be written to stderr in addition to the specified destination.
enum : uint32_t {
  LOG_NONE = 0,
  LOG_TO_FILE = 1 << 0,
  LOG_TO_SYSTEM_DEBUG_LOG = 1 << 1,
  LOG_TO_STDERR = 1 << 2,

  LOG_TO_ALL = LOG_TO_FILE | LOG_TO_SYSTEM_DEBUG_LOG | LOG_TO_STDERR,
  // On POSIX platforms, where it may not even be possible to locate the
  // executable on disk, use stderr.
  // TODO(philix): On Windows, use a file next to the exe.
  LOG_DEFAULT = LOG_TO_SYSTEM_DEBUG_LOG | LOG_TO_STDERR,
};

// On startup, should we delete or append to an existing log file (if any)?
// Defaults to APPEND_TO_OLD_LOG_FILE.
enum OldFileDeletionState { DELETE_OLD_LOG_FILE, APPEND_TO_OLD_LOG_FILE };

struct LoggingSettings {
  // Equivalent to logging destination enum, but allows for multiple
  // destinations.
  uint32_t logging_dest = LOG_DEFAULT;

  // The three settings below have an effect only when LOG_TO_FILE is
  // set in |logging_dest|.
  std::string log_file;
  OldFileDeletionState delete_old = APPEND_TO_OLD_LOG_FILE;
};

bool BaseInitLoggingImpl(const LoggingSettings& settings);

// Sets the log file name and other global logging state. Calling this function
// is recommended, and is normally done at the beginning of application init.
// If you don't call it, all the flags will be initialized to their default
// values, and there is a race condition that may leak a critical section
// object if two threads try to do the first log at the same time.
// See the definition of the enums above for descriptions and default values.
//
// The default log file is initialized to "debug.log" in the application
// directory. You probably don't want this, especially since the program
// directory may not be writable on an enduser's system.
//
// This function may be called a second time to re-direct logging (e.g after
// loging in to a user partition), however it should never be called more than
// twice.
inline bool InitLogging(const LoggingSettings& settings) { return BaseInitLoggingImpl(settings); }

// clang-format off

// Sets the log level. Anything at or above this level will be written to the
// log file/displayed to the user (if applicable). Anything below this level
// will be silently ignored. The log level defaults to 0 (everything is logged
// up to level INFO) if this function is not called.
// Note that log messages for VLOG(x) are logged at level -x, so setting
// the min log level to negative values enables verbose logging.
void SetMinLogLevel(int level);

// Gets the current log level.
int GetMinLogLevel();

// Used by LOG_IS_ON to lazy-evaluate stream arguments.
bool ShouldCreateLogMessage(int severity);

// Gets the VLOG default verbosity level.
int GetVlogVerbosity();

// Note that |N| is the size *with* the null terminator.
int GetVlogLevelHelper(const char* file_start, size_t N);

// Gets the current vlog level for the given file (usually taken from __FILE__).
template <size_t N>
int GetVlogLevel(const char (&file)[N]) {
  return GetVlogLevelHelper(file, N);
}

// Sets the common items you want to be prepended to each log message.
// process and thread IDs default to off, the timestamp defaults to on.
// If this function is not called, logging defaults to writing the timestamp
// only.
void SetLogItems(bool enable_process_id, bool enable_thread_id,
                             bool enable_timestamp, bool enable_tickcount);

// Sets an optional prefix to add to each log message. |prefix| is not copied
// and should be a raw string constant. |prefix| must only contain ASCII letters
// to avoid confusion with PIDs and timestamps. Pass null to remove the prefix.
// Logging defaults to no prefix.
void SetLogPrefix(const char* prefix);

// Sets whether or not you'd like to see fatal debug messages popped up in
// a dialog box or not.
// Dialogs are not shown by default.
void SetShowErrorDialogs(bool enable_dialogs);

// Sets the Log Assert Handler that will be used to notify of check failures.
// Resets Log Assert Handler on object destruction.
// The default handler shows a dialog box and then terminate the process,
// however clients can use this function to override with their own handling
// (e.g. a silent one for Unit Tests)
/* using LogAssertHandlerFunction = */
/*     base::RepeatingCallback<void(const char* file, */
/*                                  int line, */
/*                                  const base::StringPiece message, */
/*                                  const base::StringPiece stack_trace)>; */

/* class ScopedLogAssertHandler { */
/*  public: */
/*   explicit ScopedLogAssertHandler(LogAssertHandlerFunction handler); */
/*   ~ScopedLogAssertHandler(); */

/*  private: */
/*   FOC_DISALLOW_COPY_AND_ASSIGN(ScopedLogAssertHandler); */
/* }; */

// Sets the Log Message Handler that gets passed every log message before
// it's sent to other log destinations (if any).
// Returns true to signal that it handled the message and the message
// should not be sent to other log destinations.
typedef bool (*LogMessageHandlerFunction)(int severity,
    const char* file, int line, size_t message_start, const std::string& str);
void SetLogMessageHandler(LogMessageHandlerFunction handler);
LogMessageHandlerFunction GetLogMessageHandler();

// The ANALYZER_ASSUME_TRUE(bool arg) macro adds compiler-specific hints
// to Clang which control what code paths are statically analyzed,
// and is meant to be used in conjunction with assert & assert-like functions.
// The expression is passed straight through if analysis isn't enabled.
//
// ANALYZER_SKIP_THIS_PATH() suppresses static analysis for the current
// codepath and any other branching codepaths that might follow.
#if defined(__clang_analyzer__)

inline constexpr bool AnalyzerNoReturn() __attribute__((analyzer_noreturn)) {
  return false;
}

inline constexpr bool AnalyzerAssumeTrue(bool arg) {
  // AnalyzerNoReturn() is invoked and analysis is terminated if |arg| is
  // false.
  return arg || AnalyzerNoReturn();
}

#define ANALYZER_ASSUME_TRUE(arg) foc::logging::AnalyzerAssumeTrue(!!(arg))
#define ANALYZER_SKIP_THIS_PATH() \
  static_cast<void>(foc::logging::AnalyzerNoReturn())
#define ANALYZER_ALLOW_UNUSED(var) static_cast<void>(var);

#else  // !defined(__clang_analyzer__)

#define ANALYZER_ASSUME_TRUE(arg) (arg)
#define ANALYZER_SKIP_THIS_PATH()
#define ANALYZER_ALLOW_UNUSED(var) static_cast<void>(var);

#endif  // defined(__clang_analyzer__)

typedef int LogSeverity;
const LogSeverity LOG_VERBOSE = -1;  // This is level 1 verbosity
// Note: the log severities are used to index into the array of names,
// see log_severity_names.
const LogSeverity LOG_INFO = 0;
const LogSeverity LOG_WARNING = 1;
const LogSeverity LOG_ERROR = 2;
const LogSeverity LOG_FATAL = 3;
const LogSeverity LOG_NUM_SEVERITIES = 4;

// LOG_DFATAL is LOG_FATAL in debug mode, ERROR in normal mode
#if defined(NDEBUG)
const LogSeverity LOG_DFATAL = LOG_ERROR;
#else
const LogSeverity LOG_DFATAL = LOG_FATAL;
#endif

// A few definitions of macros that don't generate much code. These are used
// by LOG() and LOG_IF, etc. Since these are used all over our code, it's
// better to have compact code for these operations.
#define COMPACT_GOOGLE_LOG_EX_INFO(ClassName, ...) \
  foc::logging::ClassName(__FILE__, __LINE__, foc::logging::LOG_INFO, ##__VA_ARGS__)
#define COMPACT_GOOGLE_LOG_EX_WARNING(ClassName, ...)          \
  foc::logging::ClassName(__FILE__, __LINE__, foc::logging::LOG_WARNING, \
                       ##__VA_ARGS__)
#define COMPACT_GOOGLE_LOG_EX_ERROR(ClassName, ...) \
  foc::logging::ClassName(__FILE__, __LINE__, foc::logging::LOG_ERROR, ##__VA_ARGS__)
#define COMPACT_GOOGLE_LOG_EX_FATAL(ClassName, ...) \
  foc::logging::ClassName(__FILE__, __LINE__, foc::logging::LOG_FATAL, ##__VA_ARGS__)
#define COMPACT_GOOGLE_LOG_EX_DFATAL(ClassName, ...) \
  foc::logging::ClassName(__FILE__, __LINE__, foc::logging::LOG_DFATAL, ##__VA_ARGS__)
#define COMPACT_GOOGLE_LOG_EX_DCHECK(ClassName, ...) \
  foc::logging::ClassName(__FILE__, __LINE__, foc::logging::LOG_DCHECK, ##__VA_ARGS__)

#define COMPACT_GOOGLE_LOG_INFO COMPACT_GOOGLE_LOG_EX_INFO(LogMessage)
#define COMPACT_GOOGLE_LOG_WARNING COMPACT_GOOGLE_LOG_EX_WARNING(LogMessage)
#define COMPACT_GOOGLE_LOG_ERROR COMPACT_GOOGLE_LOG_EX_ERROR(LogMessage)
#define COMPACT_GOOGLE_LOG_FATAL COMPACT_GOOGLE_LOG_EX_FATAL(LogMessage)
#define COMPACT_GOOGLE_LOG_DFATAL COMPACT_GOOGLE_LOG_EX_DFATAL(LogMessage)
#define COMPACT_GOOGLE_LOG_DCHECK COMPACT_GOOGLE_LOG_EX_DCHECK(LogMessage)

#if defined(FOC_OS_WIN)
// wingdi.h defines ERROR to be 0. When we call LOG(ERROR), it gets
// substituted with 0, and it expands to COMPACT_GOOGLE_LOG_0. To allow us
// to keep using this syntax, we define this macro to do the same thing
// as COMPACT_GOOGLE_LOG_ERROR, and also define ERROR the same way that
// the Windows SDK does for consistency.
#define ERROR 0
#define COMPACT_GOOGLE_LOG_EX_0(ClassName, ...) \
  COMPACT_GOOGLE_LOG_EX_ERROR(ClassName , ##__VA_ARGS__)
#define COMPACT_GOOGLE_LOG_0 COMPACT_GOOGLE_LOG_ERROR
// Needed for LOG_IS_ON(ERROR).
const LogSeverity LOG_0 = LOG_ERROR;
#endif

// As special cases, we can assume that LOG_IS_ON(FATAL) always holds. Also,
// LOG_IS_ON(DFATAL) always holds in debug mode. In particular, CHECK()s will
// always fire if they fail.
#define LOG_IS_ON(severity) \
  (foc::logging::ShouldCreateLogMessage(foc::logging::LOG_##severity))

// We don't do any caching tricks with VLOG_IS_ON() like the
// google-glog version since it increases binary size.  This means
// that using the v-logging functions in conjunction with --vmodule
// may be slow.
#define VLOG_IS_ON(verboselevel) \
  ((verboselevel) <= foc::logging::GetVlogLevel(__FILE__))

// Helper macro which avoids evaluating the arguments to a stream if
// the condition doesn't hold. Condition is evaluated once and only once.
#define LAZY_STREAM(stream, condition)                                  \
  !(condition) ? (void) 0 : foc::logging::LogMessageVoidify() & (stream)

// We use the preprocessor's merging operator, "##", so that, e.g.,
// LOG(INFO) becomes the token COMPACT_GOOGLE_LOG_INFO.  There's some funny
// subtle difference between ostream member streaming functions (e.g.,
// ostream::operator<<(int) and ostream non-member streaming functions
// (e.g., ::operator<<(ostream&, string&): it turns out that it's
// impossible to stream something like a string directly to an unnamed
// ostream. We employ a neat hack by calling the stream() member
// function of LogMessage which seems to avoid the problem.
#define LOG_STREAM(severity) COMPACT_GOOGLE_LOG_ ## severity.stream()

#define LOG(severity) LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity))
#define LOG_IF(severity, condition) \
  LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

// The VLOG macros log with negative verbosities.
#define VLOG_STREAM(verbose_level) \
  foc::logging::LogMessage(__FILE__, __LINE__, -verbose_level).stream()

#define VLOG(verbose_level) \
  LAZY_STREAM(VLOG_STREAM(verbose_level), VLOG_IS_ON(verbose_level))

#define VLOG_IF(verbose_level, condition) \
  LAZY_STREAM(VLOG_STREAM(verbose_level), \
      VLOG_IS_ON(verbose_level) && (condition))

#if defined (FOC_OS_WIN)
#define VPLOG_STREAM(verbose_level) \
  foc::logging::Win32ErrorLogMessage(__FILE__, __LINE__, -verbose_level, \
    foc::logging::GetLastSystemErrorCode()).stream()
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
#define VPLOG_STREAM(verbose_level) \
  foc::logging::ErrnoLogMessage(__FILE__, __LINE__, -verbose_level, \
    foc::logging::GetLastSystemErrorCode()).stream()
#endif

#define VPLOG(verbose_level) \
  LAZY_STREAM(VPLOG_STREAM(verbose_level), VLOG_IS_ON(verbose_level))

#define VPLOG_IF(verbose_level, condition) \
  LAZY_STREAM(VPLOG_STREAM(verbose_level), \
    VLOG_IS_ON(verbose_level) && (condition))

#define LOG_ASSERT(condition)                       \
  LOG_IF(FATAL, !(ANALYZER_ASSUME_TRUE(condition))) \
      << "Assert failed: " #condition ". "

#if defined(FOC_OS_WIN)
#define PLOG_STREAM(severity) \
  COMPACT_GOOGLE_LOG_EX_ ## severity(Win32ErrorLogMessage, \
      foc::logging::GetLastSystemErrorCode()).stream()
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
#define PLOG_STREAM(severity) \
  COMPACT_GOOGLE_LOG_EX_ ## severity(ErrnoLogMessage, \
      foc::logging::GetLastSystemErrorCode()).stream()
#endif

#define PLOG(severity)                                          \
  LAZY_STREAM(PLOG_STREAM(severity), LOG_IS_ON(severity))

#define PLOG_IF(severity, condition) \
  LAZY_STREAM(PLOG_STREAM(severity), LOG_IS_ON(severity) && (condition))

extern std::ostream* g_swallow_stream;

// Note that g_swallow_stream is used instead of an arbitrary LOG() stream to
// avoid the creation of an object with a non-trivial destructor (LogMessage).
// On MSVC x86 (checked on 2015 Update 3), this causes a few additional
// pointless instructions to be emitted even at full optimization level, even
// though the : arm of the ternary operator is clearly never executed. Using a
// simpler object to be &'d with Voidify() avoids these extra instructions.
// Using a simpler POD object with a templated operator<< also works to avoid
// these instructions. However, this causes warnings on statically defined
// implementations of operator<<(std::ostream, ...) in some .cc files, because
// they become defined-but-unreferenced functions. A reinterpret_cast of 0 to an
// ostream* also is not suitable, because some compilers warn of undefined
// behavior.
#define EAT_STREAM_PARAMETERS \
  true ? (void)0              \
       : foc::logging::LogMessageVoidify() & (*foc::logging::g_swallow_stream)

// Captures the result of a CHECK_EQ (for example) and facilitates testing as a
// boolean.
class CheckOpResult {
 public:
  // |message| must be non-null if and only if the check failed.
  CheckOpResult(std::string* message) : message_(message) {}
  // Returns true if the check succeeded.
  operator bool() const { return !message_; }
  // Returns the message.
  std::string* message() { return message_; }

 private:
  std::string* message_;
};

// CHECK dies with a fatal error if condition is not true.  It is *not*
// controlled by NDEBUG, so the check will be executed regardless of
// compilation mode.
//
// We make sure CHECK et al. always evaluates their arguments, as
// doing CHECK(FunctionWithSideEffect()) is a common idiom.

#if defined(OFFICIAL_BUILD) && defined(NDEBUG)

// Make all CHECK functions discard their log strings to reduce code bloat, and
// improve performance, for official release builds.
//
// This is not calling BreakDebugger since this is called frequently, and
// calling an out-of-line function instead of a noreturn inline macro prevents
// compiler optimizations.
#define CHECK(condition) \
  UNLIKELY(!(condition)) ? IMMEDIATE_CRASH() : EAT_STREAM_PARAMETERS

// PCHECK includes the system error code, which is useful for determining
// why the condition failed. In official builds, preserve only the error code
// message so that it is available in crash reports. The stringified
// condition and any additional stream parameters are dropped.
#define PCHECK(condition)                                  \
  LAZY_STREAM(PLOG_STREAM(FATAL), UNLIKELY(!(condition))); \
  EAT_STREAM_PARAMETERS

#define CHECK_OP(name, op, val1, val2) CHECK((val1) op (val2))

#else  // !(OFFICIAL_BUILD && NDEBUG)

// Do as much work as possible out of line to reduce inline code size.
#define CHECK(condition)                                                    \
  LAZY_STREAM(foc::logging::LogMessage(__FILE__, __LINE__, #condition).stream(), \
              !ANALYZER_ASSUME_TRUE(condition))

#define PCHECK(condition)                                           \
  LAZY_STREAM(PLOG_STREAM(FATAL), !ANALYZER_ASSUME_TRUE(condition)) \
      << "Check failed: " #condition ". "

// Helper macro for binary operators.
// Don't use this macro directly in your code, use CHECK_EQ et al below.
// The 'switch' is used to prevent the 'else' from being ambiguous when the
// macro is used in an 'if' clause such as:
// if (a == 1)
//   CHECK_EQ(2, a);
#define CHECK_OP(name, op, val1, val2)                                         \
  switch (0) case 0: default:                                                  \
  if (foc::logging::CheckOpResult true_if_passed =                             \
      foc::logging::Check##name##Impl((val1), (val2),                          \
                                   #val1 " " #op " " #val2))                   \
   ;                                                                           \
  else                                                                         \
    foc::logging::LogMessage(__FILE__, __LINE__, true_if_passed.message()).stream()

#endif  // !(OFFICIAL_BUILD && NDEBUG)

// This formats a value for a failing CHECK_XX statement.  Ordinarily,
// it uses the definition for operator<<, with a few special cases below.
template <typename T>
inline typename std::enable_if<
    internal::SupportsOstreamOperator<const T&>::value &&
        !std::is_function<typename std::remove_pointer<T>::type>::value,
    void>::type
MakeCheckOpValueString(std::ostream* os, const T& v) {
  (*os) << v;
}

// Overload for types that no operator<< but do have .ToString() defined.
template <typename T>
inline typename std::enable_if<
    !internal::SupportsOstreamOperator<const T&>::value &&
        internal::SupportsToString<const T&>::value,
    void>::type
MakeCheckOpValueString(std::ostream* os, const T& v) {
  (*os) << v.ToString();
}

// Provide an overload for functions and function pointers. Function pointers
// don't implicitly convert to void* but do implicitly convert to bool, so
// without this function pointers are always printed as 1 or 0. (MSVC isn't
// standards-conforming here and converts function pointers to regular
// pointers, so this is a no-op for MSVC.)
template <typename T>
inline typename std::enable_if<
    std::is_function<typename std::remove_pointer<T>::type>::value,
    void>::type
MakeCheckOpValueString(std::ostream* os, const T& v) {
  (*os) << reinterpret_cast<const void*>(v);
}

// We need overloads for enums that don't support operator<<.
// (i.e. scoped enums where no operator<< overload was declared).
template <typename T>
inline typename std::enable_if<
    !internal::SupportsOstreamOperator<const T&>::value &&
        std::is_enum<T>::value,
    void>::type
MakeCheckOpValueString(std::ostream* os, const T& v) {
  (*os) << static_cast<typename std::underlying_type<T>::type>(v);
}

// We need an explicit overload for std::nullptr_t.
void MakeCheckOpValueString(std::ostream* os, std::nullptr_t p);

// Build the error message string.  This is separate from the "Impl"
// function template because it is not performance critical and so can
// be out of line, while the "Impl" code should be inline.  Caller
// takes ownership of the returned string.
template<class t1, class t2>
std::string* MakeCheckOpString(const t1& v1, const t2& v2, const char* names) {
  std::ostringstream ss;
  ss << names << " (";
  MakeCheckOpValueString(&ss, v1);
  ss << " vs. ";
  MakeCheckOpValueString(&ss, v2);
  ss << ")";
  std::string* msg = new std::string(ss.str());
  return msg;
}

// Commonly used instantiations of MakeCheckOpString<>. Explicitly instantiated
// in logging.cc.
extern template std::string* MakeCheckOpString<int, int>(
    const int&, const int&, const char* names);
extern template
std::string* MakeCheckOpString<unsigned long, unsigned long>(
    const unsigned long&, const unsigned long&, const char* names);
extern template
std::string* MakeCheckOpString<unsigned long, unsigned int>(
    const unsigned long&, const unsigned int&, const char* names);
extern template
std::string* MakeCheckOpString<unsigned int, unsigned long>(
    const unsigned int&, const unsigned long&, const char* names);
extern template
std::string* MakeCheckOpString<std::string, std::string>(
    const std::string&, const std::string&, const char* name);

// Helper functions for CHECK_OP macro.
// The (int, int) specialization works around the issue that the compiler
// will not instantiate the template version of the function on values of
// unnamed enum type - see comment below.
//
// The checked condition is wrapped with ANALYZER_ASSUME_TRUE, which under
// static analysis builds, blocks analysis of the current path if the
// condition is false.
#define DEFINE_CHECK_OP_IMPL(name, op)                                       \
  template <class t1, class t2>                                              \
  inline std::string* Check##name##Impl(const t1& v1, const t2& v2,          \
                                        const char* names) {                 \
    if (ANALYZER_ASSUME_TRUE(v1 op v2))                                      \
      return NULL;                                                           \
    else                                                                     \
      return foc::logging::MakeCheckOpString(v1, v2, names);                 \
  }                                                                          \
  inline std::string* Check##name##Impl(int v1, int v2, const char* names) { \
    if (ANALYZER_ASSUME_TRUE(v1 op v2))                                      \
      return NULL;                                                           \
    else                                                                     \
      return foc::logging::MakeCheckOpString(v1, v2, names);                 \
  }
DEFINE_CHECK_OP_IMPL(EQ, ==)
DEFINE_CHECK_OP_IMPL(NE, !=)
DEFINE_CHECK_OP_IMPL(LE, <=)
DEFINE_CHECK_OP_IMPL(LT, < )
DEFINE_CHECK_OP_IMPL(GE, >=)
DEFINE_CHECK_OP_IMPL(GT, > )
#undef DEFINE_CHECK_OP_IMPL

#define CHECK_EQ(val1, val2) CHECK_OP(EQ, ==, val1, val2)
#define CHECK_NE(val1, val2) CHECK_OP(NE, !=, val1, val2)
#define CHECK_LE(val1, val2) CHECK_OP(LE, <=, val1, val2)
#define CHECK_LT(val1, val2) CHECK_OP(LT, < , val1, val2)
#define CHECK_GE(val1, val2) CHECK_OP(GE, >=, val1, val2)
#define CHECK_GT(val1, val2) CHECK_OP(GT, > , val1, val2)

#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
#define DCHECK_IS_ON() 0
#else
#define DCHECK_IS_ON() 1
#endif

// Definitions for DLOG et al.

#if DCHECK_IS_ON()

#define DLOG_IS_ON(severity) LOG_IS_ON(severity)
#define DLOG_IF(severity, condition) LOG_IF(severity, condition)
#define DLOG_ASSERT(condition) LOG_ASSERT(condition)
#define DPLOG_IF(severity, condition) PLOG_IF(severity, condition)
#define DVLOG_IF(verboselevel, condition) VLOG_IF(verboselevel, condition)
#define DVPLOG_IF(verboselevel, condition) VPLOG_IF(verboselevel, condition)

#else  // DCHECK_IS_ON()

// If !DCHECK_IS_ON(), we want to avoid emitting any references to |condition|
// (which may reference a variable defined only if DCHECK_IS_ON()).
// Contrast this with DCHECK et al., which has different behavior.

#define DLOG_IS_ON(severity) false
#define DLOG_IF(severity, condition) EAT_STREAM_PARAMETERS
#define DLOG_ASSERT(condition) EAT_STREAM_PARAMETERS
#define DPLOG_IF(severity, condition) EAT_STREAM_PARAMETERS
#define DVLOG_IF(verboselevel, condition) EAT_STREAM_PARAMETERS
#define DVPLOG_IF(verboselevel, condition) EAT_STREAM_PARAMETERS

#endif  // DCHECK_IS_ON()

#define DLOG(severity)                                          \
  LAZY_STREAM(LOG_STREAM(severity), DLOG_IS_ON(severity))

#define DPLOG(severity)                                         \
  LAZY_STREAM(PLOG_STREAM(severity), DLOG_IS_ON(severity))

#define DVLOG(verboselevel) DVLOG_IF(verboselevel, true)

#define DVPLOG(verboselevel) DVPLOG_IF(verboselevel, true)

// Definitions for DCHECK et al.

#if DCHECK_IS_ON()

#if defined(DCHECK_IS_CONFIGURABLE)
extern LogSeverity LOG_DCHECK;
#else
const LogSeverity LOG_DCHECK = LOG_FATAL;
#endif  // defined(DCHECK_IS_CONFIGURABLE)

#else  // DCHECK_IS_ON()

// There may be users of LOG_DCHECK that are enabled independently
// of DCHECK_IS_ON(), so default to FATAL logging for those.
const LogSeverity LOG_DCHECK = LOG_FATAL;

#endif  // DCHECK_IS_ON()

// DCHECK et al. make sure to reference |condition| regardless of
// whether DCHECKs are enabled; this is so that we don't get unused
// variable warnings if the only use of a variable is in a DCHECK.
// This behavior is different from DLOG_IF et al.
//
// Note that the definition of the DCHECK macros depends on whether or not
// DCHECK_IS_ON() is true. When DCHECK_IS_ON() is false, the macros use
// EAT_STREAM_PARAMETERS to avoid expressions that would create temporaries.

#if DCHECK_IS_ON()

#define DCHECK(condition)                                           \
  LAZY_STREAM(LOG_STREAM(DCHECK), !ANALYZER_ASSUME_TRUE(condition)) \
      << "Check failed: " #condition ". "
#define DPCHECK(condition)                                           \
  LAZY_STREAM(PLOG_STREAM(DCHECK), !ANALYZER_ASSUME_TRUE(condition)) \
      << "Check failed: " #condition ". "

#else  // DCHECK_IS_ON()

#define DCHECK(condition) EAT_STREAM_PARAMETERS << !(condition)
#define DPCHECK(condition) EAT_STREAM_PARAMETERS << !(condition)

#endif  // DCHECK_IS_ON()

// Helper macro for binary operators.
// Don't use this macro directly in your code, use DCHECK_EQ et al below.
// The 'switch' is used to prevent the 'else' from being ambiguous when the
// macro is used in an 'if' clause such as:
// if (a == 1)
//   DCHECK_EQ(2, a);
#if DCHECK_IS_ON()

#define DCHECK_OP(name, op, val1, val2)                                    \
  switch (0) case 0: default:                                              \
  if (foc::logging::CheckOpResult true_if_passed =                         \
      foc::logging::Check##name##Impl((val1), (val2),                      \
                                   #val1 " " #op " " #val2))               \
   ;                                                                       \
  else                                                                     \
    foc::logging::LogMessage(__FILE__, __LINE__, foc::logging::LOG_DCHECK, \
                          true_if_passed.message()).stream()

#else  // DCHECK_IS_ON()

// When DCHECKs aren't enabled, DCHECK_OP still needs to reference operator<<
// overloads for |val1| and |val2| to avoid potential compiler warnings about
// unused functions. For the same reason, it also compares |val1| and |val2|
// using |op|.
//
// Note that the contract of DCHECK_EQ, etc is that arguments are only evaluated
// once. Even though |val1| and |val2| appear twice in this version of the macro
// expansion, this is OK, since the expression is never actually evaluated.
#define DCHECK_OP(name, op, val1, val2)                                \
  EAT_STREAM_PARAMETERS << (foc::logging::MakeCheckOpValueString(      \
                                foc::logging::g_swallow_stream, val1), \
                            foc::logging::MakeCheckOpValueString(      \
                                foc::logging::g_swallow_stream, val2), \
                            (val1)op(val2))

#endif  // DCHECK_IS_ON()

// Equality/Inequality checks - compare two values, and log a
// LOG_DCHECK message including the two values when the result is not
// as expected.  The values must have operator<<(ostream, ...)
// defined.
//
// You may append to the error message like so:
//   DCHECK_NE(1, 2) << "The world must be ending!";
//
// We are very careful to ensure that each argument is evaluated exactly
// once, and that anything which is legal to pass as a function argument is
// legal here.  In particular, the arguments may be temporary expressions
// which will end up being destroyed at the end of the apparent statement,
// for example:
//   DCHECK_EQ(string("abc")[1], 'b');
//
// WARNING: These don't compile correctly if one of the arguments is a pointer
// and the other is NULL.  In new code, prefer nullptr instead.  To
// work around this for C++98, simply static_cast NULL to the type of the
// desired pointer.

#define DCHECK_EQ(val1, val2) DCHECK_OP(EQ, ==, val1, val2)
#define DCHECK_NE(val1, val2) DCHECK_OP(NE, !=, val1, val2)
#define DCHECK_LE(val1, val2) DCHECK_OP(LE, <=, val1, val2)
#define DCHECK_LT(val1, val2) DCHECK_OP(LT, < , val1, val2)
#define DCHECK_GE(val1, val2) DCHECK_OP(GE, >=, val1, val2)
#define DCHECK_GT(val1, val2) DCHECK_OP(GT, > , val1, val2)

void LogErrorNotReached(const char* file, int line);
#define NOTREACHED()                                          \
  true ? foc::logging::LogErrorNotReached(__FILE__, __LINE__) \
       : EAT_STREAM_PARAMETERS

// This class more or less represents a particular log message.  You
// create an instance of LogMessage and then stream stuff to it.
// When you finish streaming to it, ~LogMessage is called and the
// full message gets streamed to the appropriate destination.
//
// You shouldn't actually use LogMessage's constructor to log things,
// though.  You should use the LOG() macro (and variants thereof)
// above.
class LogMessage {
 public:
  // Used for LOG(severity).
  LogMessage(const char* file, int line, LogSeverity severity);

  // Used for CHECK().  Implied severity = LOG_FATAL.
  LogMessage(const char* file, int line, const char* condition);

  // Used for CHECK_EQ(), etc. Takes ownership of the given string.
  // Implied severity = LOG_FATAL.
  LogMessage(const char* file, int line, std::string* result);

  // Used for DCHECK_EQ(), etc. Takes ownership of the given string.
  LogMessage(const char* file, int line, LogSeverity severity,
             std::string* result);

  ~LogMessage();

  std::ostream& stream() { return stream_; }

  LogSeverity severity() { return severity_; }
  std::string str() { return stream_.str(); }

 private:
  void Init(const char* file, int line);

  LogSeverity severity_;
  std::ostringstream stream_;
  size_t message_start_;  // Offset of the start of the message (past prefix
                          // info).
  // The file and line information passed in to the constructor.
  const char* file_;
  const int line_;

  // This is useful since the LogMessage class uses a lot of Win32 calls
  // that will lose the value of GLE and the code that called the log function
  // will have lost the thread error value when the log call returns.
  internal::ScopedClearLastError _last_error;

  FOC_DISALLOW_COPY_AND_ASSIGN(LogMessage);
};

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".
class LogMessageVoidify {
 public:
  LogMessageVoidify() = default;
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) { }
};

#if defined(FOC_OS_WIN)
typedef unsigned long SystemErrorCode;
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
typedef int SystemErrorCode;
#endif

// Alias for ::GetLastError() on Windows and errno on POSIX. Avoids having to
// pull in windows.h just for GetLastError() and DWORD.
SystemErrorCode GetLastSystemErrorCode();
std::string SystemErrorCodeToString(SystemErrorCode error_code);

#if defined(FOC_OS_WIN)
// Appends a formatted system message of the GetLastError() type.
class Win32ErrorLogMessage {
 public:
  Win32ErrorLogMessage(const char* file,
                       int line,
                       LogSeverity severity,
                       SystemErrorCode err);

  // Appends the error message before destructing the encapsulated class.
  ~Win32ErrorLogMessage();

  std::ostream& stream() { return log_message_.stream(); }

 private:
  SystemErrorCode err_;
  LogMessage log_message_;

  FOC_DISALLOW_COPY_AND_ASSIGN(Win32ErrorLogMessage);
};
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
// Appends a formatted system message of the errno type
class ErrnoLogMessage {
 public:
  ErrnoLogMessage(const char* file,
                  int line,
                  LogSeverity severity,
                  SystemErrorCode err);

  // Appends the error message before destructing the encapsulated class.
  ~ErrnoLogMessage();

  std::ostream& stream() { return log_message_.stream(); }

 private:
  SystemErrorCode err_;
  LogMessage log_message_;

  FOC_DISALLOW_COPY_AND_ASSIGN(ErrnoLogMessage);
};
#endif  // FOC_OS_WIN

// Closes the log file explicitly if open.
// NOTE: Since the log file is opened as necessary by the action of logging
//       statements, there's no guarantee that it will stay closed
//       after this call.
void CloseLogFile();

// Async signal safe logging mechanism.
void RawLog(int level, const char* message);

#define RAW_LOG(level, message) \
  foc::logging::RawLog(foc::logging::LOG_##level, message)

#define RAW_CHECK(condition)                             \
  do {                                                   \
    if (!(condition))                                    \
      foc::logging::RawLog(foc::logging::LOG_FATAL,      \
                      "Check failed: " #condition "\n"); \
  } while (0)

#if defined(FOC_OS_WIN)
// Returns true if logging to file is enabled.
bool IsLoggingToFileEnabled();

// Returns the default log file path.
// TODO(philix): create a string16 class for Windows similar to Chromium's base::string16
base::string16 GetLogFileFullPath();
#endif

}  // namespace logging
}  // namespace foc

namespace foc {
#if defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
namespace posix {

// Portable alternatives to the POSIX strerror() function. strerror() is
// inherently unsafe in multi-threaded apps and should never be used. Doing so
// can cause crashes. Additionally, the thread-safe alternative strerror_r
// varies in semantics across platforms. Use these functions instead.

// Thread-safe strerror function with dependable semantics that never fails.
// It will write the string form of error "err" to buffer buf of length len.
// If there is an error calling the OS's strerror_r() function then a message to
// that effect will be printed into buf, truncating if necessary. The final
// result is always null-terminated. The value of errno is never changed.
//
// Use this instead of strerror_r().
void safe_strerror_r(int err, char* buf, size_t len);

// Calls safe_strerror_r with a buffer of suitable size and returns the result
// in a C++ string.
//
// Use this instead of strerror(). Note though that safe_strerror_r will be
// more robust in the case of heap corruption errors, since it doesn't need to
// allocate a string.
std::string safe_strerror(int err);

#if defined(FOC_OS_POSIX)
# if defined(NDEBUG)

#  define HANDLE_EINTR(x) ({ \
  decltype(x) eintr_wrapper_result; \
  do { \
    eintr_wrapper_result = (x); \
  } while (eintr_wrapper_result == -1 && errno == EINTR); \
  eintr_wrapper_result; \
})

# else

#  define HANDLE_EINTR(x) ({ \
  int eintr_wrapper_counter = 0; \
  decltype(x) eintr_wrapper_result; \
  do { \
    eintr_wrapper_result = (x); \
  } while (eintr_wrapper_result == -1 && errno == EINTR && \
           eintr_wrapper_counter++ < 100); \
  eintr_wrapper_result; \
})

# endif  // NDEBUG

# define IGNORE_EINTR(x) ({ \
  decltype(x) eintr_wrapper_result; \
  do { \
    eintr_wrapper_result = (x); \
    if (eintr_wrapper_result == -1 && errno == EINTR) { \
      eintr_wrapper_result = 0; \
    } \
  } while (0); \
  eintr_wrapper_result; \
})

#else  // !FOC_OS_POSIX

# define HANDLE_EINTR(x) (x)
# define IGNORE_EINTR(x) (x)

#endif  // !FOC_OS_POSIX

#ifdef FOC_LOGGING_IMPLEMENTATION
// clang-format off

#if defined(__GLIBC__)
# define FOC_USE_HISTORICAL_STRERRO_R 1
#else
# define FOC_USE_HISTORICAL_STRERRO_R 0
#endif

#if FOC_USE_HISTORICAL_STRERRO_R && defined(__GNUC__)
// GCC will complain about the unused second wrap function unless we tell it
// that we meant for them to be potentially unused, which is exactly what this
// attribute is for.
#define FOC_POSSIBLY_UNUSED __attribute__((unused))
#else
#define FOC_POSSIBLY_UNUSED
#endif

#if FOC_USE_HISTORICAL_STRERRO_R
// glibc has two strerror_r functions: a historical GNU-specific one that
// returns type char *, and a POSIX.1-2001 compliant one available since 2.3.4
// that returns int. This wraps the GNU-specific one.
static void FOC_POSSIBLY_UNUSED wrap_posix_strerror_r(
    char *(*strerror_r_ptr)(int, char *, size_t),
    int err,
    char *buf,
    size_t len) {
  // GNU version.
  char *rc = (*strerror_r_ptr)(err, buf, len);
  if (rc != buf) {
    // glibc did not use buf and returned a static string instead. Copy it
    // into buf.
    buf[0] = '\0';
    strncat(buf, rc, len - 1);
  }
  // The GNU version never fails. Unknown errors get an "unknown error" message.
  // The result is always null terminated.
}
#endif  // FOC_USE_HISTORICAL_STRERRO_R

// Wrapper for strerror_r functions that implement the POSIX interface. POSIX
// does not define the behaviour for some of the edge cases, so we wrap it to
// guarantee that they are handled. This is compiled on all POSIX platforms, but
// it will only be used on Linux if the POSIX strerror_r implementation is
// being used (see below).
static void FOC_POSSIBLY_UNUSED wrap_posix_strerror_r(
    int (*strerror_r_ptr)(int, char *, size_t),
    int err,
    char *buf,
    size_t len) {
  int old_errno = errno;
  // Have to cast since otherwise we get an error if this is the GNU version
  // (but in such a scenario this function is never called). Sadly we can't use
  // C++-style casts because the appropriate one is reinterpret_cast but it's
  // considered illegal to reinterpret_cast a type to itself, so we get an
  // error in the opposite case.
  int result = (*strerror_r_ptr)(err, buf, len);
  if (result == 0) {
    // POSIX is vague about whether the string will be terminated, although
    // it indirectly implies that typically ERANGE will be returned, instead
    // of truncating the string. We play it safe by always terminating the
    // string explicitly.
    buf[len - 1] = '\0';
  } else {
    // Error. POSIX is vague about whether the return value is itself a system
    // error code or something else. On Linux currently it is -1 and errno is
    // set. On BSD-derived systems it is a system error and errno is unchanged.
    // We try and detect which case it is so as to put as much useful info as
    // we can into our message.
    int strerror_error;  // The error encountered in strerror
    int new_errno = errno;
    if (new_errno != old_errno) {
      // errno was changed, so probably the return value is just -1 or something
      // else that doesn't provide any info, and errno is the error.
      strerror_error = new_errno;
    } else {
      // Either the error from strerror_r was the same as the previous value, or
      // errno wasn't used. Assume the latter.
      strerror_error = result;
    }
    // snprintf truncates and always null-terminates.
    snprintf(buf,
             len,
             "Error %d while retrieving error %d",
             strerror_error,
             err);
  }
  errno = old_errno;
}
// clang-format on

void safe_strerror_r(int err, char* buf, size_t len) {
  if (buf == nullptr || len <= 0) {
    return;
  }
  // If using glibc (i.e., Linux), the compiler will automatically select the
  // appropriate overloaded function based on the function type of strerror_r.
  // The other one will be elided from the translation unit since both are
  // static.
  wrap_posix_strerror_r(&strerror_r, err, buf, len);
}

std::string safe_strerror(int err) {
  const int buffer_size = 256;
  char buf[buffer_size];
  safe_strerror_r(err, buf, sizeof(buf));
  return std::string(buf);
}

#endif  // FOC_LOGGING_IMPLEMENTATION
}  // namespace posix
#endif  // FOC_OS_POSIX || FOC_OS_FUCHSIA
}  // namespace foc

// Note that "The behavior of a C++ program is undefined if it adds declarations
// or definitions to namespace std or to a namespace within namespace std unless
// otherwise specified." --C++11[namespace.std]
//
// We've checked that this particular definition has the intended behavior on
// our implementations, but it's prone to breaking in the future, and please
// don't imitate this in your own definitions without checking with some
// standard library experts.
namespace std {

// These functions are provided as a convenience for logging, which is where we
// use streams (it is against Google style to use streams in other places). It
// is designed to allow you to emit non-ASCII Unicode strings to the log file,
// which is normally ASCII. It is relatively slow, so try not to use it for
// common cases. Non-ASCII characters will be converted to UTF-8 by these
// operators.
std::ostream& operator<<(std::ostream& out, const wchar_t* wstr);
inline std::ostream& operator<<(std::ostream& out, const std::wstring& wstr) {
  return out << wstr.c_str();
}

}  // namespace std

// The NOTIMPLEMENTED() macro annotates codepaths which have not been
// implemented yet. If output spam is a serious concern,
// NOTIMPLEMENTED_LOG_ONCE can be used.

#if defined(FOC_COMPILER_GCC)
// On Linux, with GCC, we can use __PRETTY_FUNCTION__ to get the demangled name
// of the current function in the NOTIMPLEMENTED message.
#define NOTIMPLEMENTED_MSG "Not implemented reached in " << __PRETTY_FUNCTION__
#else
#define NOTIMPLEMENTED_MSG "NOT IMPLEMENTED"
#endif

#define NOTIMPLEMENTED() DLOG(ERROR) << NOTIMPLEMENTED_MSG
#define NOTIMPLEMENTED_LOG_ONCE()                       \
  do {                                                  \
    static bool logged_once = false;                    \
    DLOG_IF(ERROR, !logged_once) << NOTIMPLEMENTED_MSG; \
    logged_once = true;                                 \
  } while (0);                                          \
  EAT_STREAM_PARAMETERS

// clang-format on

#ifdef FOC_LOGGING_IMPLEMENTATION
// Logging Implementation {{{
// clang-format off

/* #include "base/pending_task.h" */
/* #include "base/stl_util.h" */
/* #include "base/task/common/task_annotator.h" */
/* #include "build/build_config.h" */

#if defined(FOC_OS_WIN)
/* #include <io.h> */
/* #include <windows.h> */
typedef HANDLE FileHandle;
typedef HANDLE MutexHandle;
// Windows warns on using write().  It prefers _write().
# define write(fd, buf, count) _write(fd, buf, static_cast<unsigned int>(count))
// Windows doesn't define STDERR_FILENO.  Define it here.
# define STDERR_FILENO 2

#elif defined(FOC_OS_MACOS)
// In MacOS 10.12 and iOS 10.0 and later ASL (Apple System Log) was deprecated
// in favor of FOC_OS_LOG (Unified Logging).
# include <AvailabilityMacros.h>
# if defined(FOC_OS_IOS)
#  if !defined(__IPHONE_10_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0
#   define USE_ASL
#  endif
# else  // !defined(FOC_OS_IOS)
#  if !defined(MAC_OS_X_VERSION_10_12) || \
     MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_12
#   define USE_ASL
#  endif
# endif  // defined(FOC_OS_IOS)

# if defined(USE_ASL)
#  include <asl.h>
# else
#  include <os/log.h>
# endif

# include <CoreFoundation/CoreFoundation.h>
# include <mach/mach.h>
# include <mach/mach_time.h>
# include <mach-o/dyld.h>

#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
# include <time.h>
#endif

#if defined(FOC_OS_FUCHSIA)
# include <lib/syslog/global.h>
# include <lib/syslog/logger.h>
# include <zircon/process.h>
# include <zircon/syscalls.h>
#endif

#if defined(FOC_OS_ANDROID)
# include <android/log.h>
#endif

#if defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
# include <errno.h>
# include <paths.h>
# include <pthread.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/stat.h>
# include <unistd.h>
# define MAX_PATH PATH_MAX
typedef FILE* FileHandle;
typedef pthread_mutex_t* MutexHandle;
#endif

#include <algorithm>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <string>
#include <utility>

/*
#include "base/base_switches.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/stack.h"
#include "base/debug/activity_tracker.h"
#include "base/debug/alias.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/debug/task_trace.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock_impl.h"
#include "base/threading/platform_thread.h"
#include "base/vlog.h"
*/

#if defined(FOC_OS_WIN)
# include "base/win/win_util.h"
#endif

namespace foc {
namespace logging {

namespace {

/* VlogInfo* g_vlog_info = nullptr; */
/* VlogInfo* g_vlog_info_prev = nullptr; */

const char* const log_severity_names[] = {"INFO", "WARNING", "ERROR", "FATAL"};
static_assert(LOG_NUM_SEVERITIES == 4, "Incorrect number of log_severity_names");

const char* log_severity_name(int severity) {
  if (severity >= 0 && severity < LOG_NUM_SEVERITIES)
    return log_severity_names[severity];
  return "UNKNOWN";
}

int g_min_log_level = 0;

// Specifies the process' logging sink(s), represented as a combination of
// LoggingDestination values joined by bitwise OR.
int g_logging_destination = LOG_DEFAULT;

// For LOG_ERROR and above, always print to stderr.
const int kAlwaysPrintErrorLevel = LOG_ERROR;

// Which log file to use? This is initialized by InitLogging or
// will be lazily initialized to the default value when it is
// first needed.
/* using PathString = base::FilePath::StringType; */
using PathString = std::string;
PathString* g_log_file_name = nullptr;

// This file is lazily opened and the handle may be nullptr
FileHandle g_log_file = nullptr;

// What should be prepended to each message?
bool g_log_process_id = false;
bool g_log_thread_id = false;
bool g_log_timestamp = true;
bool g_log_tickcount = false;
const char* g_log_prefix = nullptr;

// Should we pop up fatal debug messages in a dialog?
bool show_error_dialogs = false;

// An assert handler override specified by the client to be called instead of
// the debug message dialog and process termination. Assert handlers are stored
// in stack to allow overriding and restoring.
/* base::stack<LogAssertHandlerFunction>& GetLogAssertHandlerStack() { */
/*   static base::NoDestructor<base::stack<LogAssertHandlerFunction> > instance; */
/*   return *instance; */
/* } */

// A log message handler that gets notified of every log message we process.
LogMessageHandlerFunction log_message_handler = nullptr;

// Helper functions to wrap platform differences.

int64_t g_thread_id = -1;

int32_t CurrentProcessId() {
#if defined(FOC_OS_WIN)
  return GetCurrentProcessId();
#elif defined(FOC_OS_FUCHSIA)
  zx_info_handle_basic_t basic = {};
  zx_object_get_info(zx_process_self(), ZX_INFO_HANDLE_BASIC, &basic,
                     sizeof(basic), nullptr, nullptr);
  return basic.koid;
#elif defined(FOC_OS_POSIX)
  return getpid();
#endif
}

#if defined(FOC_OS_LINUX)

// Store the thread ids in local storage since calling the SWI can
// expensive and CurrentThreadId() is used liberally. Clear
// the stored value after a fork() because forking changes the thread
// id. Forking without going through fork() (e.g. clone()) is not
// supported, but there is no known usage. Using thread_local is
// fine here (despite being banned) since it is going to be allowed
// but is blocked on a clang bug for Mac (https://crbug.com/829078)
// and we can't use ThreadLocalStorage because of re-entrancy due to
// CHECK/DCHECKs.
thread_local pid_t g_thread_id = -1;

void ClearTidCache() {
  g_thread_id = -1;
}

class InitAtFork {
 public:
  InitAtFork() { pthread_atfork(nullptr, nullptr, ClearTidCache); }
};

#endif  // defined(FOC_OS_LINUX)

int64_t CurrentThreadId() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if defined(FOC_OS_MACOS)
  return pthread_mach_thread_np(pthread_self());
#elif defined(FOC_OS_LINUX)
  static NoDestructor<InitAtFork> init_at_fork;
  if (g_thread_id == -1) {
    g_thread_id = syscall(__NR_gettid);
  } else {
    DCHECK_EQ(g_thread_id, syscall(__NR_gettid))
        << "Thread id stored in TLS is different from thread id returned by "
           "the system. It is likely that the process was forked without going "
           "through fork().";
  }
  return g_thread_id;
#elif defined(FOC_OS_ANDROID)
  return gettid();
#elif defined(FOC_OS_FUCHSIA)
  return zx_thread_self();
#elif defined(FOC_OS_SOLARIS) || defined(FOC_OS_QNX)
  return pthread_self();
#elif defined(FOC_OS_NACL) && defined(__GLIBC__)
  return pthread_self();
#elif defined(FOC_OS_NACL) && !defined(__GLIBC__)
  // Pointers are 32-bits in NaCl.
  return reinterpret_cast<int32_t>(pthread_self());
#elif defined(FOC_OS_POSIX) && defined(FOC_OS_AIX)
  return pthread_self();
#elif defined(FOC_OS_POSIX) && !defined(FOC_OS_AIX)
  return reinterpret_cast<int64_t>(pthread_self());
#endif
}

uint64_t TickCount() {
#if defined(FOC_OS_WIN)
  return GetTickCount();
#elif defined(FOC_OS_FUCHSIA)
  return zx_clock_get_monotonic() /
         static_cast<zx_time_t>(base::Time::kNanosecondsPerMicrosecond);
#elif defined(FOC_OS_MACOS)
  return mach_absolute_time();
#elif defined(FOC_OS_NACL)
  // NaCl sadly does not have _POSIX_TIMERS enabled in sys/features.h
  // So we have to use clock() for now.
  return clock();
#elif defined(FOC_OS_POSIX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  uint64_t absolute_micro = static_cast<int64_t>(ts.tv_sec) * 1000000 +
                            static_cast<int64_t>(ts.tv_nsec) / 1000;

  return absolute_micro;
#endif
}

void DeleteFilePath(const PathString& log_name) {
#if defined(FOC_OS_WIN)
  DeleteFile(base::as_wcstr(log_name));
#elif defined(FOC_OS_NACL)
  // Do nothing; unlink() isn't supported on NaCl.
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
  unlink(log_name.c_str());
#else
#error Unsupported platform
#endif
}

PathString GetDefaultLogFile() {
#if defined(FOC_OS_WIN)
  // On Windows we use the same path as the exe.
  base::char16 module_name[MAX_PATH];
  GetModuleFileName(nullptr, base::as_writable_wcstr(module_name), MAX_PATH);

  PathString log_name = module_name;
  PathString::size_type last_backslash = log_name.rfind('\\', log_name.size());
  if (last_backslash != PathString::npos)
    log_name.erase(last_backslash + 1);
  log_name += STRING16_LITERAL("debug.log");
  return log_name;
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
  // On other platforms we just use the current directory.
  return PathString("debug.log");
#endif
}

// We don't need locks on Windows for atomically appending to files. The OS
// provides this functionality.
#if defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
// LoggingLock::Init() should be called from the main thread before any logging
// is done. Then whenever logging, be sure to have a local LoggingLock
// instance on the stack. This will ensure that the lock is unlocked upon
// exiting the frame.
// LoggingLocks can not be nested.
class LoggingLock {
 public:
  LoggingLock() {
    LockLogging();
  }

  ~LoggingLock() {
    UnlockLogging();
  }

 private:
  static void LockLogging() {
    pthread_mutex_lock(&log_mutex);
  }

  static void UnlockLogging() {
    pthread_mutex_unlock(&log_mutex);
  }

  static pthread_mutex_t log_mutex;
};

// static
pthread_mutex_t LoggingLock::log_mutex = PTHREAD_MUTEX_INITIALIZER;

#endif  // FOC_OS_POSIX || FOC_OS_FUCHSIA

// Called by logging functions to ensure that |g_log_file| is initialized
// and can be used for writing. Returns false if the file could not be
// initialized. |g_log_file| will be nullptr in this case.
bool InitializeLogFileHandle() {
  if (g_log_file)
    return true;

  if (!g_log_file_name) {
    // Nobody has called InitLogging to specify a debug log file, so here we
    // initialize the log file name to a default.
    g_log_file_name = new PathString(GetDefaultLogFile());
  }

  if ((g_logging_destination & LOG_TO_FILE) != 0) {
#if defined(FOC_OS_WIN)
    // The FILE_APPEND_DATA access mask ensures that the file is atomically
    // appended to across accesses from multiple threads.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa364399(v=vs.85).aspx
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa363858(v=vs.85).aspx
    g_log_file = CreateFile(base::as_wcstr(*g_log_file_name), FILE_APPEND_DATA,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_log_file == INVALID_HANDLE_VALUE || g_log_file == nullptr) {
      // We are intentionally not using FilePath or FileUtil here to reduce the
      // dependencies of the logging implementation. For e.g. FilePath and
      // FileUtil depend on shell32 and user32.dll. This is not acceptable for
      // some consumers of base logging like chrome_elf, etc.
      // Please don't change the code below to use FilePath.
      // try the current directory
      base::char16 system_buffer[MAX_PATH];
      system_buffer[0] = 0;
      DWORD len = ::GetCurrentDirectory(base::size(system_buffer),
                                        base::as_writable_wcstr(system_buffer));
      if (len == 0 || len > base::size(system_buffer))
        return false;

      *g_log_file_name = system_buffer;
      // Append a trailing backslash if needed.
      if (g_log_file_name->back() != L'\\')
        *g_log_file_name += STRING16_LITERAL("\\");
      *g_log_file_name += STRING16_LITERAL("debug.log");

      g_log_file =
          CreateFile(base::as_wcstr(*g_log_file_name), FILE_APPEND_DATA,
                     FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL, nullptr);
      if (g_log_file == INVALID_HANDLE_VALUE || g_log_file == nullptr) {
        g_log_file = nullptr;
        return false;
      }
    }
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
    g_log_file = fopen(g_log_file_name->c_str(), "a");
    if (g_log_file == nullptr)
      return false;
#else
#error Unsupported platform
#endif
  }

  return true;
}

void CloseFile(FileHandle log) {
#if defined(FOC_OS_WIN)
  CloseHandle(log);
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
  fclose(log);
#else
#error Unsupported platform
#endif
}

void CloseLogFileUnlocked() {
  if (!g_log_file)
    return;

  CloseFile(g_log_file);
  g_log_file = nullptr;
}

}  // namespace

#if defined(DCHECK_IS_CONFIGURABLE)
// In DCHECK-enabled Chrome builds, allow the meaning of LOG_DCHECK to be
// determined at run-time. We default it to INFO, to avoid it triggering
// crashes before the run-time has explicitly chosen the behaviour.
logging::LogSeverity LOG_DCHECK = LOG_INFO;
#endif  // defined(DCHECK_IS_CONFIGURABLE)

// This is never instantiated, it's just used for EAT_STREAM_PARAMETERS to have
// an object of the correct type on the LHS of the unused part of the ternary
// operator.
std::ostream* g_swallow_stream;

bool BaseInitLoggingImpl(const LoggingSettings& settings) {
  /*
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Don't bother initializing |g_vlog_info| unless we use one of the
  // vlog switches.
  if (command_line->HasSwitch(switches::kV) ||
      command_line->HasSwitch(switches::kVModule)) {
    // NOTE: If |g_vlog_info| has already been initialized, it might be in use
    // by another thread. Don't delete the old VLogInfo, just create a second
    // one. We keep track of both to avoid memory leak warnings.
    CHECK(!g_vlog_info_prev);
    g_vlog_info_prev = g_vlog_info;

    g_vlog_info =
        new VlogInfo(command_line->GetSwitchValueASCII(switches::kV),
                     command_line->GetSwitchValueASCII(switches::kVModule),
                     &g_min_log_level);
  }
  */

  g_logging_destination = settings.logging_dest;

#if defined(FOC_OS_FUCHSIA)
  if (g_logging_destination & LOG_TO_SYSTEM_DEBUG_LOG) {
    fx_logger_config_t config;
    config.min_severity = FX_LOG_INFO;
    config.console_fd = -1;
    config.log_service_channel = ZX_HANDLE_INVALID;
    std::string log_tag = command_line->GetProgram().BaseName().AsUTF8Unsafe();
    const char* log_tag_data = log_tag.data();
    config.tags = &log_tag_data;
    config.num_tags = 1;
    fx_log_init_with_config(&config);
  }
#endif

  // ignore file options unless logging to file is set.
  if ((g_logging_destination & LOG_TO_FILE) == 0)
    return true;

#if defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
  LoggingLock logging_lock;
#endif

  // Calling InitLogging twice or after some log call has already opened the
  // default log file will re-initialize to the new options.
  CloseLogFileUnlocked();

  if (!g_log_file_name)
    g_log_file_name = new PathString();
  *g_log_file_name = settings.log_file;
  if (settings.delete_old == DELETE_OLD_LOG_FILE)
    DeleteFilePath(*g_log_file_name);

  return InitializeLogFileHandle();
}

void SetMinLogLevel(int level) {
  g_min_log_level = std::min(LOG_FATAL, level);
}

int GetMinLogLevel() {
  return g_min_log_level;
}

bool ShouldCreateLogMessage(int severity) {
  if (severity < g_min_log_level)
    return false;

  // Return true here unless we know ~LogMessage won't do anything.
  return g_logging_destination != LOG_NONE || log_message_handler ||
         severity >= kAlwaysPrintErrorLevel;
}

// Returns true when LOG_TO_STDERR flag is set, or |severity| is high.
// If |severity| is high then true will be returned when no log destinations are
// set, or only LOG_TO_FILE is set, since that is useful for local development
// and debugging.
bool ShouldLogToStderr(int severity) {
  if (g_logging_destination & LOG_TO_STDERR)
    return true;
  if (severity >= kAlwaysPrintErrorLevel)
    return (g_logging_destination & ~LOG_TO_FILE) == LOG_NONE;
  return false;
}

int GetVlogVerbosity() {
  return std::max(-1, LOG_INFO - GetMinLogLevel());
}

int GetVlogLevelHelper(const char* file, size_t N) {
  /* DCHECK_GT(N, 0U); */
  // Note: |g_vlog_info| may change on a different thread during startup
  // (but will always be valid or nullptr).
  /*
  VlogInfo* vlog_info = g_vlog_info;
  return vlog_info ?
      vlog_info->GetVlogLevel(base::StringPiece(file, N - 1)) :
      GetVlogVerbosity();
      */
  return -1;
}

void SetLogItems(bool enable_process_id, bool enable_thread_id,
                 bool enable_timestamp, bool enable_tickcount) {
  g_log_process_id = enable_process_id;
  g_log_thread_id = enable_thread_id;
  g_log_timestamp = enable_timestamp;
  g_log_tickcount = enable_tickcount;
}

void SetLogPrefix(const char* prefix) {
  g_log_prefix = prefix;
}

void SetShowErrorDialogs(bool enable_dialogs) {
  show_error_dialogs = enable_dialogs;
}

/* ScopedLogAssertHandler::ScopedLogAssertHandler( */
/*     LogAssertHandlerFunction handler) { */
/*   GetLogAssertHandlerStack().push(std::move(handler)); */
/* } */

/* ScopedLogAssertHandler::~ScopedLogAssertHandler() { */
/*   GetLogAssertHandlerStack().pop(); */
/* } */

void SetLogMessageHandler(LogMessageHandlerFunction handler) {
  log_message_handler = handler;
}

LogMessageHandlerFunction GetLogMessageHandler() {
  return log_message_handler;
}

// Explicit instantiations for commonly used comparisons.
template std::string* MakeCheckOpString<int, int>(
    const int&, const int&, const char* names);
template std::string* MakeCheckOpString<unsigned long, unsigned long>(
    const unsigned long&, const unsigned long&, const char* names);
template std::string* MakeCheckOpString<unsigned long, unsigned int>(
    const unsigned long&, const unsigned int&, const char* names);
template std::string* MakeCheckOpString<unsigned int, unsigned long>(
    const unsigned int&, const unsigned long&, const char* names);
template std::string* MakeCheckOpString<std::string, std::string>(
    const std::string&, const std::string&, const char* name);

void MakeCheckOpValueString(std::ostream* os, std::nullptr_t p) {
  (*os) << "nullptr";
}

#if !defined(NDEBUG)
// Displays a message box to the user with the error message in it.
// Used for fatal messages, where we close the app simultaneously.
// This is for developers only; we don't use this in circumstances
// (like release builds) where users could see it, since users don't
// understand these messages anyway.
void DisplayDebugMessageInDialog(const std::string& str) {
  if (str.empty())
    return;

  if (!show_error_dialogs)
    return;

# if defined(FOC_OS_WIN)
  // We intentionally don't implement a dialog on other platforms.
  // You can just look at stderr.
  if (base::win::IsUser32AndGdi32Available()) {
    MessageBoxW(nullptr, base::as_wcstr(base::UTF8ToUTF16(str)), L"Fatal error",
                MB_OK | MB_ICONHAND | MB_TOPMOST);
  } else {
    OutputDebugStringW(base::as_wcstr(base::UTF8ToUTF16(str)));
  }
# endif  // defined(FOC_OS_WIN)
}
#endif  // !defined(NDEBUG)

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : severity_(severity), file_(file), line_(line) {
  Init(file, line);
}

LogMessage::LogMessage(const char* file, int line, const char* condition)
    : severity_(LOG_FATAL), file_(file), line_(line) {
  Init(file, line);
  stream_ << "Check failed: " << condition << ". ";
}

LogMessage::LogMessage(const char* file, int line, std::string* result)
    : severity_(LOG_FATAL), file_(file), line_(line) {
  Init(file, line);
  stream_ << "Check failed: " << *result;
  delete result;
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity,
                       std::string* result)
    : severity_(severity), file_(file), line_(line) {
  Init(file, line);
  stream_ << "Check failed: " << *result;
  delete result;
}

LogMessage::~LogMessage() {
  /* size_t stack_start = stream_.tellp(); */
#if !defined(OFFICIAL_BUILD) && !defined(FOC_OS_NACL) && !defined(__UCLIBC__) && \
    !defined(FOC_OS_AIX)
  if (severity_ == LOG_FATAL && !debug::BeingDebugged()) {
    /*
    // Include a stack trace on a fatal, unless a debugger is attached.
    debug::StackTrace stack_trace;
    stream_ << std::endl;  // Newline to separate from log message.
    stack_trace.OutputToStream(&stream_);
    */
  }
#endif
  stream_ << std::endl;
  std::string str_newline(stream_.str());

  // Give any log message handler first dibs on the message.
  if (log_message_handler &&
      log_message_handler(severity_, file_, line_,
                          message_start_, str_newline)) {
    // The handler took care of it, no further processing.
    return;
  }

  if ((g_logging_destination & LOG_TO_SYSTEM_DEBUG_LOG) != 0) {
#if defined(FOC_OS_WIN)
    OutputDebugStringA(str_newline.c_str());
#elif defined(FOC_OS_MACOS)
    // In LOG_TO_SYSTEM_DEBUG_LOG mode, log messages are always written to
    // stderr. If stderr is /dev/null, also log via ASL (Apple System Log) or
    // its successor FOC_OS_LOG. If there's something weird about stderr, assume
    // that log messages are going nowhere and log via ASL/FOC_OS_LOG too.
    // Messages logged via ASL/FOC_OS_LOG show up in Console.app.
    //
    // Programs started by launchd, as UI applications normally are, have had
    // stderr connected to /dev/null since OS X 10.8. Prior to that, stderr was
    // a pipe to launchd, which logged what it received (see log_redirect_fd in
    // 10.7.5 launchd-392.39/launchd/src/launchd_core_logic.c).
    //
    // Another alternative would be to determine whether stderr is a pipe to
    // launchd and avoid logging via ASL only in that case. See 10.7.5
    // CF-635.21/CFUtilities.c also_do_stderr(). This would result in logging to
    // both stderr and ASL/FOC_OS_LOG even in tests, where it's undesirable to log
    // to the system log at all.
    //
    // Note that the ASL client by default discards messages whose levels are
    // below ASL_LEVEL_NOTICE. It's possible to change that with
    // asl_set_filter(), but this is pointless because syslogd normally applies
    // the same filter.
    const bool log_to_system = []() {
      struct stat stderr_stat;
      if (fstat(fileno(stderr), &stderr_stat) == -1) {
        return true;
      }
      if (!S_ISCHR(stderr_stat.st_mode)) {
        return false;
      }

      struct stat dev_null_stat;
      if (stat(_PATH_DEVNULL, &dev_null_stat) == -1) {
        return true;
      }

      return !S_ISCHR(dev_null_stat.st_mode) ||
             stderr_stat.st_rdev == dev_null_stat.st_rdev;
    }();

    if (log_to_system) {
      // Log roughly the same way that CFLog() and NSLog() would. See 10.10.5
      // CF-1153.18/CFUtilities.c __CFLogCString().
      CFBundleRef main_bundle = CFBundleGetMainBundle();
      CFStringRef main_bundle_id_cf =
          main_bundle ? CFBundleGetIdentifier(main_bundle) : nullptr;
      std::string main_bundle_id =
          main_bundle_id_cf ? strings::SysCFStringRefToUTF8(main_bundle_id_cf)
                            : std::string("");
#if defined(USE_ASL)
      // The facility is set to the main bundle ID if available. Otherwise,
      // "com.apple.console" is used.
      const class ASLClient {
       public:
        explicit ASLClient(const std::string& facility)
            : client_(asl_open(nullptr, facility.c_str(), ASL_OPT_NO_DELAY)) {}
        ~ASLClient() { asl_close(client_); }

        aslclient get() const { return client_; }

       private:
        aslclient client_;
        FOC_DISALLOW_COPY_AND_ASSIGN(ASLClient);
      } asl_client(main_bundle_id.empty() ? main_bundle_id
                                          : "com.apple.console");

      const class ASLMessage {
       public:
        ASLMessage() : message_(asl_new(ASL_TYPE_MSG)) {}
        ~ASLMessage() { asl_free(message_); }

        aslmsg get() const { return message_; }

       private:
        aslmsg message_;
        FOC_DISALLOW_COPY_AND_ASSIGN(ASLMessage);
      } asl_message;

      // By default, messages are only readable by the admin group. Explicitly
      // make them readable by the user generating the messages.
      char euid_string[12];
      snprintf(euid_string, sizeof(euid_string), "%d", geteuid());
      asl_set(asl_message.get(), ASL_KEY_READ_UID, euid_string);

      // Map Chrome log severities to ASL log levels.
      const char* const asl_level_string = [](LogSeverity severity) {
        // ASL_LEVEL_* are ints, but ASL needs equivalent strings. This
        // non-obvious two-step macro trick achieves what's needed.
        // https://gcc.gnu.org/onlinedocs/cpp/Stringification.html
#define ASL_LEVEL_STR(level) ASL_LEVEL_STR_X(level)
#define ASL_LEVEL_STR_X(level) #level
        switch (severity) {
          case LOG_INFO:
            return ASL_LEVEL_STR(ASL_LEVEL_INFO);
          case LOG_WARNING:
            return ASL_LEVEL_STR(ASL_LEVEL_WARNING);
          case LOG_ERROR:
            return ASL_LEVEL_STR(ASL_LEVEL_ERR);
          case LOG_FATAL:
            return ASL_LEVEL_STR(ASL_LEVEL_CRIT);
          default:
            return severity < 0 ? ASL_LEVEL_STR(ASL_LEVEL_DEBUG)
                                : ASL_LEVEL_STR(ASL_LEVEL_NOTICE);
        }
#undef ASL_LEVEL_STR
#undef ASL_LEVEL_STR_X
      }(severity_);
      asl_set(asl_message.get(), ASL_KEY_LEVEL, asl_level_string);

      asl_set(asl_message.get(), ASL_KEY_MSG, str_newline.c_str());

      asl_send(asl_client.get(), asl_message.get());
#else   // !defined(USE_ASL)
      const class OSLog {
       public:
        explicit OSLog(const char* subsystem)
            : os_log_(subsystem ? os_log_create(subsystem, "chromium_logging")
                                : FOC_OS_LOG_DEFAULT) {}
        ~OSLog() {
          if (os_log_ != FOC_OS_LOG_DEFAULT) {
            os_release(os_log_);
          }
        }
        os_log_t get() const { return os_log_; }

       private:
        os_log_t os_log_;
        FOC_DISALLOW_COPY_AND_ASSIGN(OSLog);
      } log(main_bundle_id.empty() ? nullptr : main_bundle_id.c_str());
      const os_log_type_t os_log_type = [](LogSeverity severity) {
        switch (severity) {
          case LOG_INFO:
            return FOC_OS_LOG_TYPE_INFO;
          case LOG_WARNING:
            return FOC_OS_LOG_TYPE_DEFAULT;
          case LOG_ERROR:
            return FOC_OS_LOG_TYPE_ERROR;
          case LOG_FATAL:
            return FOC_OS_LOG_TYPE_FAULT;
          default:
            return severity < 0 ? FOC_OS_LOG_TYPE_DEBUG : FOC_OS_LOG_TYPE_DEFAULT;
        }
      }(severity_);
      os_log_with_type(log.get(), os_log_type, "%{public}s",
                       str_newline.c_str());
#endif  // defined(USE_ASL)
    }
#elif defined(FOC_OS_ANDROID)
    android_LogPriority priority =
        (severity_ < 0) ? ANDROID_LOG_VERBOSE : ANDROID_LOG_UNKNOWN;
    switch (severity_) {
      case LOG_INFO:
        priority = ANDROID_LOG_INFO;
        break;
      case LOG_WARNING:
        priority = ANDROID_LOG_WARN;
        break;
      case LOG_ERROR:
        priority = ANDROID_LOG_ERROR;
        break;
      case LOG_FATAL:
        priority = ANDROID_LOG_FATAL;
        break;
    }
    const char kAndroidLogTag[] = "chromium";
#if DCHECK_IS_ON()
    // Split the output by new lines to prevent the Android system from
    // truncating the log.
    std::vector<std::string> lines = base::SplitString(
        str_newline, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    // str_newline has an extra newline appended to it (at the top of this
    // function), so skip the last split element to avoid needlessly
    // logging an empty string.
    lines.pop_back();
    for (const auto& line : lines)
      __android_log_write(priority, kAndroidLogTag, line.c_str());
#else
    // The Android system may truncate the string if it's too long.
    __android_log_write(priority, kAndroidLogTag, str_newline.c_str());
#endif
#elif defined(FOC_OS_FUCHSIA)
    fx_log_severity_t severity = FX_LOG_INFO;
    switch (severity_) {
      case LOG_INFO:
        severity = FX_LOG_INFO;
        break;
      case LOG_WARNING:
        severity = FX_LOG_WARNING;
        break;
      case LOG_ERROR:
        severity = FX_LOG_ERROR;
        break;
      case LOG_FATAL:
        // Don't use FX_LOG_FATAL, otherwise fx_logger_log() will abort().
        severity = FX_LOG_ERROR;
        break;
    }

    fx_logger_t* logger = fx_log_get_logger();
    if (logger) {
      // Temporarily pop the trailing newline, since fx_logger will add one.
      str_newline.pop_back();
      fx_logger_log(logger, severity, nullptr, str_newline.c_str());
      str_newline.push_back('\n');
    }
#endif  // FOC_OS_FUCHSIA
  }

  if (ShouldLogToStderr(severity_)) {
    fwrite(str_newline.data(), str_newline.size(), 1, stderr);
    fflush(stderr);
  }

  if ((g_logging_destination & LOG_TO_FILE) != 0) {
    // We can have multiple threads and/or processes, so try to prevent them
    // from clobbering each other's writes.
    // If the client app did not call InitLogging, and the lock has not
    // been created do it now. We do this on demand, but if two threads try
    // to do this at the same time, there will be a race condition to create
    // the lock. This is why InitLogging should be called from the main
    // thread at the beginning of execution.
#if defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
    LoggingLock logging_lock;
#endif
    if (InitializeLogFileHandle()) {
#if defined(FOC_OS_WIN)
      DWORD num_written;
      WriteFile(g_log_file,
                static_cast<const void*>(str_newline.c_str()),
                static_cast<DWORD>(str_newline.length()),
                &num_written,
                nullptr);
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
      fwrite(str_newline.data(), str_newline.size(), 1, g_log_file);
      fflush(g_log_file);
#else
#error Unsupported platform
#endif
    }
  }

  if (severity_ == LOG_FATAL) {
    // Write the log message to the global activity tracker, if running.
    /* base::debug::GlobalActivityTracker* tracker = */
    /*     base::debug::GlobalActivityTracker::Get(); */
    /* if (tracker) */
    /*   tracker->RecordLogMessage(str_newline); */

    // Ensure the first characters of the string are on the stack so they
    // are contained in minidumps for diagnostic purposes. We place start
    // and end marker values at either end, so we can scan captured stacks
    // for the data easily.
    struct {
      uint32_t start_marker = 0xbedead01;
      char data[1024];
      uint32_t end_marker = 0x5050dead;
    } str_stack;
    strings::strlcpy(str_stack.data, str_newline.data(), sizeof(str_stack.data));
    debug::Alias(&str_stack);

    /*
    if (!GetLogAssertHandlerStack().empty()) {
      LogAssertHandlerFunction log_assert_handler =
          GetLogAssertHandlerStack().top();

      if (log_assert_handler) {
        log_assert_handler.Run(
            file_, line_,
            base::StringPiece(str_newline.c_str() + message_start_,
                              stack_start - message_start_),
            base::StringPiece(str_newline.c_str() + stack_start));
      }
    } else {
    */
      // Don't use the string with the newline, get a fresh version to send to
      // the debug message process. We also don't display assertions to the
      // user in release mode. The enduser can't do anything with this
      // information, and displaying message boxes when the application is
      // hosed can cause additional problems.
#ifndef NDEBUG
      if (!debug::BeingDebugged()) {
        // Displaying a dialog is unnecessary when debugging and can complicate
        // debugging.
        DisplayDebugMessageInDialog(stream_.str());
      }
#endif
      // Crash the process to generate a dump.
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
      IMMEDIATE_CRASH();
#else
      debug::BreakDebugger();
#endif
    /* } */
  }
}

// writes the common header info to the stream
void LogMessage::Init(const char* file, int line) {
  // Find last \ or / and change file to be only the suffix after \ or /
  const size_t len = strlen(file);
  const char *p = (file + len) - 1;
  for (; p > file; p--) {
    if (*p == '\\' || *p == '/') {
      file = p + 1;
      break;
    }
  }
  /* base::StringPiece filename(file); */
  /* size_t last_slash_pos = filename.find_last_of("\\/"); */
  /* if (last_slash_pos != base::StringPiece::npos) */
  /*   filename.remove_prefix(last_slash_pos + 1); */

  // TODO(darin): It might be nice if the columns were fixed width.

  stream_ <<  '[';
  if (g_log_prefix)
    stream_ << g_log_prefix << ':';
  if (g_log_process_id)
    stream_ << CurrentProcessId() << ':';
  if (g_log_thread_id)
    stream_ << CurrentThreadId() << ':';
    /* stream_ << base::PlatformThread::CurrentId() << ':'; */
  if (g_log_timestamp) {
#if defined(FOC_OS_WIN)
    SYSTEMTIME local_time;
    GetLocalTime(&local_time);
    stream_ << std::setfill('0')
            << std::setw(2) << local_time.wMonth
            << std::setw(2) << local_time.wDay
            << '/'
            << std::setw(2) << local_time.wHour
            << std::setw(2) << local_time.wMinute
            << std::setw(2) << local_time.wSecond
            << '.'
            << std::setw(3)
            << local_time.wMilliseconds
            << ':';
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
    timeval tv;
    gettimeofday(&tv, nullptr);
    time_t t = tv.tv_sec;
    struct tm local_time;
    localtime_r(&t, &local_time);
    struct tm* tm_time = &local_time;
    stream_ << std::setfill('0')
            << std::setw(2) << 1 + tm_time->tm_mon
            << std::setw(2) << tm_time->tm_mday
            << '/'
            << std::setw(2) << tm_time->tm_hour
            << std::setw(2) << tm_time->tm_min
            << std::setw(2) << tm_time->tm_sec
            << '.'
            << std::setw(6) << tv.tv_usec
            << ':';
#else
#error Unsupported platform
#endif
  }
  if (g_log_tickcount)
    stream_ << TickCount() << ':';
  if (severity_ >= 0)
    stream_ << log_severity_name(severity_);
  else
    stream_ << "VERBOSE" << -severity_;

  stream_ << ":" << file << "(" << line << ")] ";

  message_start_ = stream_.str().length();
}

#if defined(FOC_OS_WIN)
// This has already been defined in the header, but defining it again as DWORD
// ensures that the type used in the header is equivalent to DWORD. If not,
// the redefinition is a compile error.
typedef DWORD SystemErrorCode;
#endif

SystemErrorCode GetLastSystemErrorCode() {
#if defined(FOC_OS_WIN)
  return ::GetLastError();
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
  return errno;
#endif
}

std::string SystemErrorCodeToString(SystemErrorCode error_code) {
#if defined(FOC_OS_WIN)
  const int kErrorMessageBufferSize = 256;
  char msgbuf[kErrorMessageBufferSize];
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageA(flags, nullptr, error_code, 0, msgbuf,
                             base::size(msgbuf), nullptr);
  if (len) {
    // Messages returned by system end with line breaks.
    return base::CollapseWhitespaceASCII(msgbuf, true) +
           base::StringPrintf(" (0x%lX)", error_code);
  }
  return base::StringPrintf("Error (0x%lX) while retrieving error. (0x%lX)",
                            GetLastError(), error_code);
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
  std::string ret = posix::safe_strerror(error_code);
  ret += " (";
  ret += error_code;
  ret += ")";
  return ret;
#endif  // defined(FOC_OS_WIN)
}


#if defined(FOC_OS_WIN)
Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file,
                                           int line,
                                           LogSeverity severity,
                                           SystemErrorCode err)
    : err_(err),
      log_message_(file, line, severity) {
}

Win32ErrorLogMessage::~Win32ErrorLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  DWORD last_error = err_;
  debug::Alias(&last_error);
}
#elif defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
ErrnoLogMessage::ErrnoLogMessage(const char* file,
                                 int line,
                                 LogSeverity severity,
                                 SystemErrorCode err)
    : err_(err),
      log_message_(file, line, severity) {
}

ErrnoLogMessage::~ErrnoLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  int last_error = err_;
  debug::Alias(&last_error);
}
#endif  // defined(FOC_OS_WIN)

void CloseLogFile() {
#if defined(FOC_OS_POSIX) || defined(FOC_OS_FUCHSIA)
  LoggingLock logging_lock;
#endif
  CloseLogFileUnlocked();
}

void RawLog(int level, const char* message) {
  if (level >= g_min_log_level && message) {
    size_t bytes_written = 0;
    const size_t message_len = strlen(message);
    int rv;
    while (bytes_written < message_len) {
      rv = HANDLE_EINTR(
          write(STDERR_FILENO, message + bytes_written,
                message_len - bytes_written));
      if (rv < 0) {
        // Give up, nothing we can do now.
        break;
      }
      bytes_written += rv;
    }

    if (message_len > 0 && message[message_len - 1] != '\n') {
      do {
        rv = HANDLE_EINTR(write(STDERR_FILENO, "\n", 1));
        if (rv < 0) {
          // Give up, nothing we can do now.
          break;
        }
      } while (rv != 1);
    }
  }

  if (level == LOG_FATAL)
    debug::BreakDebugger();
}

// This was defined at the beginning of this file.
#undef write

#if defined(FOC_OS_WIN)
bool IsLoggingToFileEnabled() {
  return g_logging_destination & LOG_TO_FILE;
}

base::string16 GetLogFileFullPath() {
  if (g_log_file_name)
    return *g_log_file_name;
  return base::string16();
}
#endif

void LogErrorNotReached(const char* file, int line) {
  LogMessage(file, line, LOG_ERROR).stream()
      << "NOTREACHED() hit.";
}

}  // namespace logging
}  // namespace foc

std::ostream& std::operator<<(std::ostream& out, const wchar_t* wstr) {
  NOTIMPLEMENTED();
  return out /*<< (wstr ? base::WideToUTF8(wstr) : std::string())*/;
}

// }}}
// clang-format on
#endif  // FOC_LOGGING_IMPLEMENTATION
