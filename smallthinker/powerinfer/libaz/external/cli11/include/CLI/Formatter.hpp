// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// IWYU pragma: private, include "CLI/CLI.hpp"

// [CLI11:public_includes:set]
#include <algorithm>
#include <string>
#include <vector>
// [CLI11:public_includes:end]

#include "App.hpp"
#include "FormatterFwd.hpp"

namespace CLI {
// [CLI11:formatter_hpp:verbatim]
// [CLI11:formatter_hpp:end]
}  // namespace CLI

#ifndef CLI11_COMPILE
#include "impl/Formatter_inl.hpp"  // IWYU pragma: export
#endif
