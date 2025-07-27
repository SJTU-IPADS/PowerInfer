// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"
#include "Macros.hpp"

// [CLI11:public_includes:set]
#include <string>
// [CLI11:public_includes:end]

// [CLI11:encoding_includes:verbatim]
#ifdef CLI11_CPP17
#include <string_view>
#endif  // CLI11_CPP17

#if defined CLI11_HAS_FILESYSTEM && CLI11_HAS_FILESYSTEM > 0
#include <filesystem>
#include <string_view>  // NOLINT(build/include)
#endif                  // CLI11_HAS_FILESYSTEM
// [CLI11:encoding_includes:end]

namespace CLI {
// [CLI11:encoding_hpp:verbatim]

/// Convert a wide string to a narrow string.
CLI11_INLINE std::string narrow(const std::wstring &str);
CLI11_INLINE std::string narrow(const wchar_t *str);
CLI11_INLINE std::string narrow(const wchar_t *str, std::size_t size);

/// Convert a narrow string to a wide string.
CLI11_INLINE std::wstring widen(const std::string &str);
CLI11_INLINE std::wstring widen(const char *str);
CLI11_INLINE std::wstring widen(const char *str, std::size_t size);

#ifdef CLI11_CPP17
CLI11_INLINE std::string narrow(std::wstring_view str);
CLI11_INLINE std::wstring widen(std::string_view str);
#endif  // CLI11_CPP17

#if defined CLI11_HAS_FILESYSTEM && CLI11_HAS_FILESYSTEM > 0
/// Convert a char-string to a native path correctly.
CLI11_INLINE std::filesystem::path to_path(std::string_view str);
#endif  // CLI11_HAS_FILESYSTEM

// [CLI11:encoding_hpp:end]
}  // namespace CLI

#ifndef CLI11_COMPILE
#include "impl/Encoding_inl.hpp"  // IWYU pragma: export
#endif
