// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// This include is only needed for IDEs to discover symbols
#include "../Argv.hpp"

#include "../Encoding.hpp"

// [CLI11:public_includes:set]
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
// [CLI11:public_includes:end]

// [CLI11:argv_inl_includes:verbatim]
#if defined(_WIN32)
#if !(defined(_AMD64_) || defined(_X86_) || defined(_ARM_))
#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) || defined(_M_X64) ||           \
    defined(_M_AMD64)
#define _AMD64_
#elif defined(i386) || defined(__i386) || defined(__i386__) || defined(__i386__) || defined(_M_IX86)
#define _X86_
#elif defined(__arm__) || defined(_M_ARM) || defined(_M_ARMT)
#define _ARM_
#elif defined(__aarch64__) || defined(_M_ARM64)
#define _ARM64_
#elif defined(_M_ARM64EC)
#define _ARM64EC_
#endif
#endif

// first
#ifndef NOMINMAX
// if NOMINMAX is already defined we don't want to mess with that either way
#define NOMINMAX
#include <windef.h>
#undef NOMINMAX
#else
#include <windef.h>
#endif

// second
#include <winbase.h>
// third
#include <processthreadsapi.h>
#include <shellapi.h>
#endif
// [CLI11:argv_inl_includes:end]

namespace CLI {
// [CLI11:argv_inl_hpp:verbatim]

namespace detail {

#ifdef _WIN32
CLI11_INLINE std::vector<std::string> compute_win32_argv() {
    std::vector<std::string> result;
    int argc = 0;

    auto deleter = [](wchar_t **ptr) { LocalFree(ptr); };
    // NOLINTBEGIN(*-avoid-c-arrays)
    auto wargv = std::unique_ptr<wchar_t *[], decltype(deleter)>(CommandLineToArgvW(GetCommandLineW(), &argc), deleter);
    // NOLINTEND(*-avoid-c-arrays)

    if(wargv == nullptr) {
        throw std::runtime_error("CommandLineToArgvW failed with code " + std::to_string(GetLastError()));
    }

    result.reserve(static_cast<size_t>(argc));
    for(size_t i = 0; i < static_cast<size_t>(argc); ++i) {
        result.push_back(narrow(wargv[i]));
    }

    return result;
}
#endif

}  // namespace detail

// [CLI11:argv_inl_hpp:end]
}  // namespace CLI
