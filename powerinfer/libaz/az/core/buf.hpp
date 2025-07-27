#pragma once

#include "az/assert.hpp"

#include <span>

namespace az {

struct BufView {
    void *data = nullptr;
    size_t size = 0;

    BufView() = default;

    BufView(void *data, size_t size) : data(data), size(size) {}

    explicit operator bool() const {
        return data != nullptr;
    }

    template <typename T>
    auto ptr() -> T * {
        return reinterpret_cast<T *>(data);
    }

    template <typename T>
    auto const_ptr() const -> const T * {
        return reinterpret_cast<const T *>(data);
    }

    /**
     * `for (int v : buf_view.span<int>()) { ... }`
     */
    template <typename T>
    auto span() -> std::span<T> {
        AZ_DEBUG_ASSERT(size % sizeof(T) == 0);
        return std::span(ptr<T>(), size / sizeof(T));
    }

    template <typename T>
    auto const_span() const -> std::span<const T> {
        AZ_DEBUG_ASSERT(size % sizeof(T) == 0);
        return std::span(const_ptr<T>(), size / sizeof(T));
    }

    auto view() const -> BufView {
        return BufView(data, size);
    }

    auto slice(size_t offset, size_t new_size) const -> BufView {
        AZ_DEBUG_ASSERT(offset <= size);
        new_size = std::min(new_size, size - offset);
        return BufView((char *)data + offset, new_size);
    }

    /**
     * Partition the buffer view into chunks of `chunk_size`.
     * Return the i-th chunk.
     */
    auto chunk(size_t chunk_size, size_t i) const -> BufView {
        AZ_DEBUG_ASSERT(size % chunk_size == 0);
        return slice(i * chunk_size, chunk_size);
    }
};

/**
 * Base class for memory buffer objects.
 * Cannot be copied.
 */
struct Buf : BufView, Noncopyable {
    virtual ~Buf() = default;

    Buf(Buf &&other);
    Buf &operator=(Buf &&other);

protected:
    /**
     * The base class cannot be constructed directly.
     */
    Buf() = default;

    Buf(size_t size) : BufView(nullptr, size) {}
};

struct AlignedBuf : Buf {
    static constexpr size_t alignment = 64;

    AlignedBuf() = default;
    AlignedBuf(AlignedBuf &&other) = default;
    AlignedBuf &operator=(AlignedBuf &&other) = default;

    AlignedBuf(size_t size);
    ~AlignedBuf();
};

template <typename T>
struct AlignedArray {
    AlignedBuf buf;

    AlignedArray() = default;

    AlignedArray(size_t n) : buf(n * sizeof(T)) {}

    auto size() const -> size_t {
        return buf.size / sizeof(T);
    }

    auto ptr() -> T * {
        return buf.ptr<T>();
    }

    auto const_ptr() const -> const T * {
        return buf.const_ptr<T>();
    }

    auto span() -> std::span<T> {
        return buf.span<T>();
    }

    auto const_span() const -> std::span<const T> {
        return buf.const_span<T>();
    }

    T &operator[](size_t i) {
        AZ_DEBUG_ASSERT(i < size());
        return ptr()[i];
    }

    const T &operator[](size_t i) const {
        AZ_DEBUG_ASSERT(i < size());
        return const_ptr()[i];
    }

    auto slice(size_t offset, size_t new_size) const -> BufView {
        return buf.slice(offset * sizeof(T), new_size * sizeof(T));
    }

    auto chunk(size_t chunk_size, size_t i) const -> BufView {
        AZ_DEBUG_ASSERT(size() % chunk_size == 0);
        return slice(i * chunk_size, chunk_size);
    }
};

}  // namespace az
