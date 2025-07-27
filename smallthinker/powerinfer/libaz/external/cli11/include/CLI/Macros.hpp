// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// [CLI11:macros_hpp:verbatim]

// The following version macro is very similar to the one in pybind11
#if !(defined(_MSC_VER) && __cplusplus == 199711L) && !defined(__INTEL_COMPILER)
#if __cplusplus >= 201402L
#define CLI11_CPP14
#if __cplusplus >= 201703L
#define CLI11_CPP17
#if __cplusplus > 201703L
#define CLI11_CPP20
#if __cplusplus > 202002L
#define CLI11_CPP23
#if __cplusplus > 202302L
#define CLI11_CPP26
#endif
#endif
#endif
#endif
#endif
#elif defined(_MSC_VER) && __cplusplus == 199711L
// MSVC sets _MSVC_LANG rather than __cplusplus (supposedly until the standard was fully implemented)
// Unless you use the /Zc:__cplusplus flag on Visual Studio 2017 15.7 Preview 3 or newer
#if _MSVC_LANG >= 201402L
#define CLI11_CPP14
#if _MSVC_LANG > 201402L && _MSC_VER >= 1910
#define CLI11_CPP17
#if _MSVC_LANG > 201703L && _MSC_VER >= 1910
#define CLI11_CPP20
#if _MSVC_LANG > 202002L && _MSC_VER >= 1922
#define CLI11_CPP23
#endif
#endif
#endif
#endif
#endif

#if defined(CLI11_CPP14)
#define CLI11_DEPRECATED(reason) [[deprecated(reason)]]
#elif defined(_MSC_VER)
#define CLI11_DEPRECATED(reason) __declspec(deprecated(reason))
#else
#define CLI11_DEPRECATED(reason) __attribute__((deprecated(reason)))
#endif

// GCC < 10 doesn't ignore this in unevaluated contexts
#if !defined(CLI11_CPP17) ||                                                                                           \
    (defined(__GNUC__) && !defined(__llvm__) && !defined(__INTEL_COMPILER) && __GNUC__ < 10 && __GNUC__ > 4)
#define CLI11_NODISCARD
#else
#define CLI11_NODISCARD [[nodiscard]]
#endif

/** detection of rtti */
#ifndef CLI11_USE_STATIC_RTTI
#if (defined(_HAS_STATIC_RTTI) && _HAS_STATIC_RTTI)
#define CLI11_USE_STATIC_RTTI 1
#elif defined(__cpp_rtti)
#if (defined(_CPPRTTI) && _CPPRTTI == 0)
#define CLI11_USE_STATIC_RTTI 1
#else
#define CLI11_USE_STATIC_RTTI 0
#endif
#elif (defined(__GCC_RTTI) && __GXX_RTTI)
#define CLI11_USE_STATIC_RTTI 0
#else
#define CLI11_USE_STATIC_RTTI 1
#endif
#endif

/** <filesystem> availability */
#if defined CLI11_CPP17 && defined __has_include && !defined CLI11_HAS_FILESYSTEM
#if __has_include(<filesystem>)
// Filesystem cannot be used if targeting macOS < 10.15
#if defined __MAC_OS_X_VERSION_MIN_REQUIRED && __MAC_OS_X_VERSION_MIN_REQUIRED < 101500
#define CLI11_HAS_FILESYSTEM 0
#elif defined(__wasi__)
// As of wasi-sdk-14, filesystem is not implemented
#define CLI11_HAS_FILESYSTEM 0
#else
#include <filesystem>
#if defined __cpp_lib_filesystem && __cpp_lib_filesystem >= 201703
#if defined _GLIBCXX_RELEASE && _GLIBCXX_RELEASE >= 9
#define CLI11_HAS_FILESYSTEM 1
#elif defined(__GLIBCXX__)
// if we are using gcc and Version <9 default to no filesystem
#define CLI11_HAS_FILESYSTEM 0
#else
#define CLI11_HAS_FILESYSTEM 1
#endif
#else
#define CLI11_HAS_FILESYSTEM 0
#endif
#endif
#endif
#endif

/** <codecvt> availability */
#if !defined(CLI11_CPP26) && !defined(CLI11_HAS_CODECVT)
#if defined(__GNUC__) && !defined(__llvm__) && !defined(__INTEL_COMPILER) && __GNUC__ < 5
#define CLI11_HAS_CODECVT 0
#else
#define CLI11_HAS_CODECVT 1
#include <codecvt>
#endif
#else
#if defined(CLI11_HAS_CODECVT)
#if CLI11_HAS_CODECVT > 0
#include <codecvt>
#endif
#else
#define CLI11_HAS_CODECVT 0
#endif
#endif

/** disable deprecations */
#if defined(__GNUC__)  // GCC or clang
#define CLI11_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")
#define CLI11_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")

#define CLI11_DIAGNOSTIC_IGNORE_DEPRECATED _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")

#elif defined(_MSC_VER)
#define CLI11_DIAGNOSTIC_PUSH __pragma(warning(push))
#define CLI11_DIAGNOSTIC_POP __pragma(warning(pop))

#define CLI11_DIAGNOSTIC_IGNORE_DEPRECATED __pragma(warning(disable : 4996))

#else
#define CLI11_DIAGNOSTIC_PUSH
#define CLI11_DIAGNOSTIC_POP

#define CLI11_DIAGNOSTIC_IGNORE_DEPRECATED

#endif

/** Inline macro **/
#ifdef CLI11_COMPILE
#define CLI11_INLINE
#else
#define CLI11_INLINE inline
#endif
// [CLI11:macros_hpp:end]
