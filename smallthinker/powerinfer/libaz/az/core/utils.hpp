#pragma once

#include "az/common.hpp"

namespace az {

size_t elapsed_ns(const TimePoint &start_ts, const TimePoint &end_ts);

struct Range {
    struct Iterator {
        size_t value;

        bool operator==(const Iterator &other) const {
            return value == other.value;
        }

        void operator++() {
            value++;
        }

        size_t operator*() const {
            return value;
        }
    };

    size_t offset = 0;
    size_t count = 0;

    bool operator==(const Range &other) const = default;

    size_t first() const {
        return offset;
    }

    size_t last() const {
        return offset + count - 1;
    }

    auto begin() const -> Iterator {
        return Iterator{first()};
    }

    auto end() const -> Iterator {
        return Iterator{last() + 1};
    }
};

/**
 * Distribute items evenly among workers, at the granularity of `block_size`.
 */
auto distribute_items(size_t n_items, size_t n_workers, size_t worker_id, size_t block_size = 1) -> Range;

template <typename T>
constexpr size_t div_round_up(T a, T b) {
    return (a + b - 1) / b;
}

template <typename T, typename TOut = double, TOut eps = 1e-7>
constexpr TOut relative_error(T x, T y) {
    TOut x0 = std::abs(x);
    TOut y0 = std::abs(y);
    return std::abs((x0 - y0) / std::max(std::max(x0, y0), eps));
}

template <typename T, typename TOut = double>
constexpr TOut absolute_error(T x, T y) {
    return std::abs((TOut)x - (TOut)y);
}

template <typename T, typename TOut = double, TOut eps = 1e-7>
constexpr TOut rel_or_abs_error(T x, T y) {
    return std::min(relative_error<T, TOut, eps>(x, y), absolute_error<T, TOut>(x, y));
}

template <typename T>
size_t vector_size(const typename std::vector<T> &vec) {
    return sizeof(T) * vec.size();
}

}  // namespace az
