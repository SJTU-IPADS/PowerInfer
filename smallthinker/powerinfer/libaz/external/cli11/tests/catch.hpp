// Copyright (c) 2017-2025, University of Cincinnati, developed by Henry Schreiner
// under NSF AWARD 1414736 and by the respective contributors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

#ifdef CLI11_CATCH3

#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using Catch::Approx;                  // NOLINT(google-global-names-in-headers)
using Catch::Generators::from_range;  // NOLINT(google-global-names-in-headers)
using Catch::Matchers::Equals;        // NOLINT(google-global-names-in-headers)
using Catch::Matchers::WithinRel;     // NOLINT(google-global-names-in-headers)

inline auto Contains(const std::string &x) { return Catch::Matchers::ContainsSubstring(x); }

#else

#include <catch2/catch.hpp>

using Catch::Equals;              // NOLINT(google-global-names-in-headers)
using Catch::WithinRel;           // NOLINT(google-global-names-in-headers)
using Catch::Matchers::Contains;  // NOLINT(google-global-names-in-headers)

#endif
