#pragma once

#include "fmt/base.h"
#include "fmt/format.h"
#include "fmt/ranges.h"
#include "fmt/std.h"

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

#if !defined(ALWAYS_INLINE)
#define ALWAYS_INLINE __attribute__((always_inline))
#endif

#define AZ_UNUSED(x) ((void)(x))

#define AZ_BUILTIN_EXPECT(expr, value) __builtin_expect((expr), (value))
#define AZ_LIKELY(expr) AZ_BUILTIN_EXPECT((expr), 1)
#define AZ_UNLIKELY(expr) AZ_BUILTIN_EXPECT((expr), 0)

#ifdef __cplusplus
    // restrict not standard in C++
#    if defined(__GNUC__)
#        define AZ_RESTRICT __restrict__
#    elif defined(__clang__)
#        define AZ_RESTRICT __restrict
#    elif defined(_MSC_VER)
#        define AZ_RESTRICT __restrict
#    else
#        define AZ_RESTRICT
#    endif
#else
#    if defined (_MSC_VER) && (__STDC_VERSION__ < 201112L)
#        define AZ_RESTRICT __restrict
#    else
#        define AZ_RESTRICT restrict
#    endif
#endif
/**
 * Allow fmt to print enum values.
 *
 * Ref: https://github.com/fmtlib/fmt/issues/2704#issuecomment-1046052415
 */
using fmt::enums::format_as;

namespace az {

using Path = std::filesystem::path;
using Timer = std::chrono::steady_clock;
using TimePoint = Timer::time_point;

/**
 * "Arc" stands for "Atomically Reference Counted"
 */
template <typename T>
using Arc = std::shared_ptr<T>;

struct Noncopyable {
    Noncopyable(const Noncopyable &) = delete;
    auto operator=(const Noncopyable &) = delete;

protected:
    Noncopyable() = default;
    ~Noncopyable() = default;
};

/**
 * Usage:
 *   static RunOnce xxx([&] { ... });
 */
struct RunOnce {
    std::once_flag flag;

    template <typename Fn>
    RunOnce(const Fn &fn) {
        std::call_once(flag, fn);
    }
};

}  // namespace az
