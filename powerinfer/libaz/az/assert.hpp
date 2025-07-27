#pragma once

#include "az/common.hpp"

namespace az {

/**
 * Allow using trailing lambdas to print additional info at assertion failure:
 *   AZ_ASSERT(false) << [] { fmt::println("{}", aha); };
 */
struct BaseAssertionResult {
    const char *file;
    size_t line;
    const char *func;
    const char *expr;
    bool result;

    BaseAssertionResult(const char *file, size_t line, const char *func, const char *expr, bool result) :
        file(file),
        line(line),
        func(func),
        expr(expr),
        result(result) {
        if (AZ_UNLIKELY(!result)) {
            fmt::println(stderr, "{}:{}: {}: Assertion failed: \"{}\"", file, line, func, expr);
        }
    }

    ~BaseAssertionResult() {
        if (AZ_UNLIKELY(!result)) {
            abort();
        }
    }

    template <typename Fn>
    const BaseAssertionResult &operator<<(Fn &&fn) const {
        if (AZ_UNLIKELY(!result)) {
            fn();
        }
        return *this;
    }
};

struct DummyAssertionResult {
    DummyAssertionResult(...) {}

    template <typename Fn>
    const DummyAssertionResult &operator<<(Fn &&) const {
        return *this;
    }
};

#if defined(AZ_DISABLE_ASSERT)
using AssertionResult = DummyAssertionResult;
#else
using AssertionResult = BaseAssertionResult;
#endif

#if defined(AZ_DISABLE_DEBUG_ASSERT)
using DebugAssertionResult = DummyAssertionResult;
#else
using DebugAssertionResult = AssertionResult;
#endif

}  // namespace az

#define AZ_ASSERT(expr) az::AssertionResult(__FILE__, __LINE__, __func__, #expr, static_cast<bool>(expr))
#define AZ_ASSERT_EQ(expr, value)                                                                                      \
    AZ_ASSERT((expr) == (value)) << [&] { fmt::println("Expect \"{}\", but got \"{}\"", (value), (expr)); }

#define AZ_DEBUG_ASSERT(expr) az::DebugAssertionResult(__FILE__, __LINE__, __func__, #expr, static_cast<bool>(expr))
