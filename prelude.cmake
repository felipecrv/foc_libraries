# Sensible defaults for cross-platform C and C++ compilation

# used for check_c_compiler_flag
include(CheckCCompilerFlag)
# used for check_cxx_compiler_flag
include(CheckCXXCompilerFlag)

#
# Build-type: Release
#
# Default to -O2 on release builds.
#
if(CMAKE_C_FLAGS_RELEASE MATCHES "-O3")
  message(STATUS "Replacing -O3 in CMAKE_C_FLAGS_RELEASE with -O2.")
  string(REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
endif()
if(CMAKE_CXX_FLAGS_RELEASE MATCHES "-O3")
  message(STATUS "Replacing -O3 in CMAKE_CXX_FLAGS_RELEASE with -O2.")
  string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
endif()

#
# Build-type: RelWithDebInfo
#
# Enable -Og and asserts
#
if(CMAKE_COMPILER_IS_GNUCC)
  check_c_compiler_flag(-Og CC_HAS_OG_FLAG)
else()
  set(CC_HAS_OG_FLAG 0)
endif()
if(CMAKE_COMPILER_IS_GNUCXX)
  check_cxx_compiler_flag(-Og CXX_HAS_OG_FLAG)
else()
  set(CXX_HAS_OG_FLAG 0)
endif()
if(CC_HAS_OG_FLAG)
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -Og -g")
endif()
if(CXX_HAS_OG_FLAG)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Og -g")
endif()
if(CMAKE_C_FLAGS_RELWITHDEBINFO MATCHES DNDEBUG)
  string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
endif()
if(CMAKE_CXX_FLAGS_RELWITHDEBINFO MATCHES DNDEBUG)
  string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
endif()

# Enable -Wconversion.
if(NOT MSVC)
  # TODO(philix): try to enable -Wconversion later
  # set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wconversion")
  # set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wconversion")
endif()

if(MSVC)
  # XXX: /W4 gives too many warnings. #3241
  add_definitions(/W3 -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
  add_definitions(-DWIN32)
else()
  add_definitions(-Wall -Wextra -pedantic -Wno-unused-parameter
    -Wstrict-prototypes)

  check_c_compiler_flag(-Wimplicit-fallthrough CC_HAS_WIMPLICIT_FALLTHROUGH_FLAG)
  if(CC_HAS_WIMPLICIT_FALLTHROUGH_FLAG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wimplicit-fallthrough")
  endif()
  check_cxx_compiler_flag(-Wimplicit-fallthrough CXX_HAS_WIMPLICIT_FALLTHROUGH_FLAG)
  if(CXX_HAS_WIMPLICIT_FALLTHROUGH_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wimplicit-fallthrough")
  endif()

  # Allow for __VA_ARGS__
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments")
endif()

if(MINGW)
  # Use POSIX compatible stdio in Mingw
  add_definitions(-D__USE_MINGW_ANSI_STDIO)
endif()
if(WIN32)
  # Windows Vista is the minimum supported version
  add_definitions(-D_WIN32_WINNT=0x0600)
endif()

# OpenBSD's GCC (4.2.1) doesn't have -Wvla
check_c_compiler_flag(-Wvla HAS_WVLA_FLAG)
if(HAS_WVLA_FLAG)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wvla")
endif()

option(TRAVIS_CI_BUILD "Travis/QuickBuild CI. Extra flags will be set." OFF)

if(TRAVIS_CI_BUILD)
  message(STATUS "Travis/QuickBuild CI build enabled.")
  add_definitions(-Werror)
  if(DEFINED ENV{BUILD_32BIT})
    # Get some test coverage for unsigned char
    add_definitions(-funsigned-char)
  endif()
endif()

if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-undefined")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-undefined")

  # For O_CLOEXEC, O_DIRECTORY, and O_NOFOLLOW flags on older systems
  # (pre POSIX.1-2008: glibc 2.11 and earlier).
  add_definitions(-D_GNU_SOURCE)
endif()
