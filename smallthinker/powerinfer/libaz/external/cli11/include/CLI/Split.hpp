// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// [CLI11:public_includes:set]
#include <string>
#include <tuple>
#include <utility>
#include <vector>
// [CLI11:public_includes:end]

#include "Macros.hpp"

namespace CLI {
// [CLI11:split_hpp:verbatim]

namespace detail {

// Returns false if not a short option. Otherwise, sets opt name and rest and returns true
CLI11_INLINE bool split_short(const std::string &current, std::string &name, std::string &rest);

// Returns false if not a long option. Otherwise, sets opt name and other side of = and returns true
CLI11_INLINE bool split_long(const std::string &current, std::string &name, std::string &value);

// Returns false if not a windows style option. Otherwise, sets opt name and value and returns true
CLI11_INLINE bool split_windows_style(const std::string &current, std::string &name, std::string &value);

// Splits a string into multiple long and short names
CLI11_INLINE std::vector<std::string> split_names(std::string current);

/// extract default flag values either {def} or starting with a !
CLI11_INLINE std::vector<std::pair<std::string, std::string>> get_default_flag_values(const std::string &str);

/// Get a vector of short names, one of long names, and a single name
CLI11_INLINE std::tuple<std::vector<std::string>, std::vector<std::string>, std::string>
get_names(const std::vector<std::string> &input, bool allow_non_standard = false);

}  // namespace detail
// [CLI11:split_hpp:end]
}  // namespace CLI

#ifndef CLI11_COMPILE
#include "impl/Split_inl.hpp"  // IWYU pragma: export
#endif
