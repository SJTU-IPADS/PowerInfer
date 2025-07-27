// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// [CLI11:public_includes:set]
#include <string>
#include <vector>
// [CLI11:public_includes:end]

#include "Macros.hpp"

namespace CLI {
// [CLI11:argv_hpp:verbatim]
namespace detail {
#ifdef _WIN32
/// Decode and return UTF-8 argv from GetCommandLineW.
CLI11_INLINE std::vector<std::string> compute_win32_argv();
#endif
}  // namespace detail
// [CLI11:argv_hpp:end]
}  // namespace CLI

#ifndef CLI11_COMPILE
#include "impl/Argv_inl.hpp"  // IWYU pragma: export
#endif
