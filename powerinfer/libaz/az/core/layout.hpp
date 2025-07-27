#pragma once

#include "az/assert.hpp"

namespace az {

template <size_t N>
struct Layout {
    static constexpr size_t n_dims = N;

    size_t dims[N];

    template <typename... Args>
    Layout(Args... args) {
        static_assert(sizeof...(args) <= N);

        size_t i = 0;
        auto assign = [&](size_t v) { dims[i++] = v; };

        (assign(args), ...);

        for (; i < N; i++) {
            dims[i] = 1;
        }
    }

    size_t operator[](size_t i) const {
        AZ_DEBUG_ASSERT(i < N);
        return dims[i];
    }

    template <typename... Args>
    size_t index(Args... args) const {
        static_assert(sizeof...(args) <= N);

        size_t i = 0;
        size_t ret = 0;
        auto accumulate = [&](size_t v) {
            const size_t d = dims[i++];
            AZ_DEBUG_ASSERT(v < d);
            ret = ret * d + v;
        };

        (accumulate(args), ...);

        for (; i < N; i++) {
            ret *= dims[i];
        }

        return ret;
    }
};

}  // namespace az
