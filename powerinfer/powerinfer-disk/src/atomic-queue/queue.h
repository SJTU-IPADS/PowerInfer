/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED
#define ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include "defs.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

using std::uint32_t;
using std::uint64_t;
using std::uint8_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace details {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<size_t elements_per_cache_line> struct GetCacheLineIndexBits { static int constexpr value = 0; };
template<> struct GetCacheLineIndexBits<256> { static int constexpr value = 8; };
template<> struct GetCacheLineIndexBits<128> { static int constexpr value = 7; };
template<> struct GetCacheLineIndexBits< 64> { static int constexpr value = 6; };
template<> struct GetCacheLineIndexBits< 32> { static int constexpr value = 5; };
template<> struct GetCacheLineIndexBits< 16> { static int constexpr value = 4; };
template<> struct GetCacheLineIndexBits<  8> { static int constexpr value = 3; };
template<> struct GetCacheLineIndexBits<  4> { static int constexpr value = 2; };
template<> struct GetCacheLineIndexBits<  2> { static int constexpr value = 1; };

template<bool minimize_contention, unsigned array_size, size_t elements_per_cache_line>
struct GetIndexShuffleBits {
    static int constexpr bits = GetCacheLineIndexBits<elements_per_cache_line>::value;
    static unsigned constexpr min_size = 1u << (bits * 2);
    static int constexpr value = array_size < min_size ? 0 : bits;
};

template<unsigned array_size, size_t elements_per_cache_line>
struct GetIndexShuffleBits<false, array_size, elements_per_cache_line> {
    static int constexpr value = 0;
};

// Multiple writers/readers contend on the same cache line when storing/loading elements at
// subsequent indexes, aka false sharing. For power of 2 ring buffer size it is possible to re-map
// the index in such a way that each subsequent element resides on another cache line, which
// minimizes contention. This is done by swapping the lowest order N bits (which are the index of
// the element within the cache line) with the next N bits (which are the index of the cache line)
// of the element index.
template<int BITS>
constexpr unsigned remap_index(unsigned index) noexcept {
    unsigned constexpr mix_mask{(1u << BITS) - 1};
    unsigned const mix{(index ^ (index >> BITS)) & mix_mask};
    return index ^ mix ^ (mix << BITS);
}

template<>
constexpr unsigned remap_index<0>(unsigned index) noexcept {
    return index;
}

template<int BITS, class T>
constexpr T& map(T* elements, unsigned index) noexcept {
    return elements[remap_index<BITS>(index)];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Implement a "bit-twiddling hack" for finding the next power of 2 in either 32 bits or 64 bits
// in C++11 compatible constexpr functions. The library no longer maintains C++11 compatibility.

// "Runtime" version for 32 bits
// --a;
// a |= a >> 1;
// a |= a >> 2;
// a |= a >> 4;
// a |= a >> 8;
// a |= a >> 16;
// ++a;

template<class T>
constexpr T decrement(T x) noexcept {
    return x - 1;
}

template<class T>
constexpr T increment(T x) noexcept {
    return x + 1;
}

template<class T>
constexpr T or_equal(T x, unsigned u) noexcept {
    return x | x >> u;
}

template<class T, class... Args>
constexpr T or_equal(T x, unsigned u, Args... rest) noexcept {
    return or_equal(or_equal(x, u), rest...);
}

constexpr uint32_t round_up_to_power_of_2(uint32_t a) noexcept {
    return increment(or_equal(decrement(a), 1, 2, 4, 8, 16));
}

constexpr uint64_t round_up_to_power_of_2(uint64_t a) noexcept {
    return increment(or_equal(decrement(a), 1, 2, 4, 8, 16, 32));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T>
constexpr T nil() noexcept {
#if __cpp_lib_atomic_is_always_lock_free // Better compile-time error message requires C++17.
    static_assert(std::atomic<T>::is_always_lock_free, "Queue element type T is not atomic. Use AtomicQueue2/AtomicQueueB2 for such element types.");
#endif
    return {};
}

template<class T>
inline void destroy_n(T* p, unsigned n) noexcept {
    for(auto q = p + n; p != q;)
        (p++)->~T();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace details

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Derived>
class AtomicQueueCommon {
protected:
    // Put these on different cache lines to avoid false sharing between readers and writers.
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> head_ = {};
    alignas(CACHE_LINE_SIZE) std::atomic<unsigned> tail_ = {};

    // The special member functions are not thread-safe.

    AtomicQueueCommon() noexcept = default;

    AtomicQueueCommon(AtomicQueueCommon const& b) noexcept
        : head_(b.head_.load(X))
        , tail_(b.tail_.load(X)) {}

    AtomicQueueCommon& operator=(AtomicQueueCommon const& b) noexcept {
        head_.store(b.head_.load(X), X);
        tail_.store(b.tail_.load(X), X);
        return *this;
    }

    void swap(AtomicQueueCommon& b) noexcept {
        unsigned h = head_.load(X);
        unsigned t = tail_.load(X);
        head_.store(b.head_.load(X), X);
        tail_.store(b.tail_.load(X), X);
        b.head_.store(h, X);
        b.tail_.store(t, X);
    }

    template<class T, T NIL>
    static T do_pop_atomic(std::atomic<T>& q_element) noexcept {
        if(Derived::spsc_) {
            for(;;) {
                T element = q_element.load(A);
                if(ATOMIC_QUEUE_LIKELY(element != NIL)) {
                    q_element.store(NIL, X);
                    return element;
                }
                if(Derived::maximize_throughput_)
                    spin_loop_pause();
            }
        }
        else {
            for(;;) {
                T element = q_element.exchange(NIL, A); // (2) The store to wait for.
                if(ATOMIC_QUEUE_LIKELY(element != NIL))
                    return element;
                // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                do
                    spin_loop_pause();
                while(Derived::maximize_throughput_ && q_element.load(X) == NIL);
            }
        }
    }

    template<class T, T NIL>
    static void do_push_atomic(T element, std::atomic<T>& q_element) noexcept {
        assert(element != NIL);
        if(Derived::spsc_) {
            while(ATOMIC_QUEUE_UNLIKELY(q_element.load(X) != NIL))
                if(Derived::maximize_throughput_)
                    spin_loop_pause();
            q_element.store(element, R);
        }
        else {
            for(T expected = NIL; ATOMIC_QUEUE_UNLIKELY(!q_element.compare_exchange_weak(expected, element, R, X)); expected = NIL) {
                do
                    spin_loop_pause(); // (1) Wait for store (2) to complete.
                while(Derived::maximize_throughput_ && q_element.load(X) != NIL);
            }
        }
    }

    enum State : unsigned char { EMPTY, STORING, STORED, LOADING };

    template<class T>
    static T do_pop_any(std::atomic<unsigned char>& state, T& q_element) noexcept {
        if(Derived::spsc_) {
            while(ATOMIC_QUEUE_UNLIKELY(state.load(A) != STORED))
                if(Derived::maximize_throughput_)
                    spin_loop_pause();
            T element{std::move(q_element)};
            state.store(EMPTY, R);
            return element;
        }
        else {
            for(;;) {
                unsigned char expected = STORED;
                if(ATOMIC_QUEUE_LIKELY(state.compare_exchange_weak(expected, LOADING, A, X))) {
                    T element{std::move(q_element)};
                    state.store(EMPTY, R);
                    return element;
                }
                // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                do
                    spin_loop_pause();
                while(Derived::maximize_throughput_ && state.load(X) != STORED);
            }
        }
    }

    template<class U, class T>
    static void do_push_any(U&& element, std::atomic<unsigned char>& state, T& q_element) noexcept {
        if(Derived::spsc_) {
            while(ATOMIC_QUEUE_UNLIKELY(state.load(A) != EMPTY))
                if(Derived::maximize_throughput_)
                    spin_loop_pause();
            q_element = std::forward<U>(element);
            state.store(STORED, R);
        }
        else {
            for(;;) {
                unsigned char expected = EMPTY;
                if(ATOMIC_QUEUE_LIKELY(state.compare_exchange_weak(expected, STORING, A, X))) {
                    q_element = std::forward<U>(element);
                    state.store(STORED, R);
                    return;
                }
                // Do speculative loads while busy-waiting to avoid broadcasting RFO messages.
                do
                    spin_loop_pause();
                while(Derived::maximize_throughput_ && state.load(X) != EMPTY);
            }
        }
    }

public:
    template<class T>
    bool try_push(T&& element) noexcept {
        auto head = head_.load(X);
        if(Derived::spsc_) {
            if(static_cast<int>(head - tail_.load(X)) >= static_cast<int>(static_cast<Derived&>(*this).size_))
                return false;
            head_.store(head + 1, X);
        }
        else {
            do {
                if(static_cast<int>(head - tail_.load(X)) >= static_cast<int>(static_cast<Derived&>(*this).size_))
                    return false;
            } while(ATOMIC_QUEUE_UNLIKELY(!head_.compare_exchange_weak(head, head + 1, X, X))); // This loop is not FIFO.
        }

        static_cast<Derived&>(*this).do_push(std::forward<T>(element), head);
        return true;
    }

    template<class T>
    bool try_pop(T& element) noexcept {
        auto tail = tail_.load(X);
        if(Derived::spsc_) {
            if(static_cast<int>(head_.load(X) - tail) <= 0)
                return false;
            tail_.store(tail + 1, X);
        }
        else {
            do {
                if(static_cast<int>(head_.load(X) - tail) <= 0)
                    return false;
            } while(ATOMIC_QUEUE_UNLIKELY(!tail_.compare_exchange_weak(tail, tail + 1, X, X))); // This loop is not FIFO.
        }

        element = static_cast<Derived&>(*this).do_pop(tail);
        return true;
    }

    template<class T>
    void push(T&& element) noexcept {
        unsigned head;
        if(Derived::spsc_) {
            head = head_.load(X);
            head_.store(head + 1, X);
        }
        else {
            constexpr auto memory_order = Derived::total_order_ ? std::memory_order_seq_cst : std::memory_order_relaxed;
            head = head_.fetch_add(1, memory_order); // FIFO and total order on Intel regardless, as of 2019.
        }
        static_cast<Derived&>(*this).do_push(std::forward<T>(element), head);
    }

    auto pop() noexcept {
        unsigned tail;
        if(Derived::spsc_) {
            tail = tail_.load(X);
            tail_.store(tail + 1, X);
        }
        else {
            constexpr auto memory_order = Derived::total_order_ ? std::memory_order_seq_cst : std::memory_order_relaxed;
            tail = tail_.fetch_add(1, memory_order); // FIFO and total order on Intel regardless, as of 2019.
        }
        return static_cast<Derived&>(*this).do_pop(tail);
    }

    bool was_empty() const noexcept {
        return !was_size();
    }

    bool was_full() const noexcept {
        return was_size() >= static_cast<int>(static_cast<Derived const&>(*this).size_);
    }

    unsigned was_size() const noexcept {
        // tail_ can be greater than head_ because of consumers doing pop, rather that try_pop, when the queue is empty.
        return std::max(static_cast<int>(head_.load(X) - tail_.load(X)), 0);
    }

    unsigned capacity() const noexcept {
        return static_cast<Derived const&>(*this).size_;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE, T NIL = details::nil<T>(), bool MINIMIZE_CONTENTION = true, bool MAXIMIZE_THROUGHPUT = true, bool TOTAL_ORDER = false, bool SPSC = false>
class AtomicQueue : public AtomicQueueCommon<AtomicQueue<T, SIZE, NIL, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>> {
    using Base = AtomicQueueCommon<AtomicQueue<T, SIZE, NIL, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>>;
    friend Base;

    static constexpr unsigned size_ = MINIMIZE_CONTENTION ? details::round_up_to_power_of_2(SIZE) : SIZE;
    static constexpr int SHUFFLE_BITS = details::GetIndexShuffleBits<MINIMIZE_CONTENTION, size_, CACHE_LINE_SIZE / sizeof(std::atomic<T>)>::value;
    static constexpr bool total_order_ = TOTAL_ORDER;
    static constexpr bool spsc_ = SPSC;
    static constexpr bool maximize_throughput_ = MAXIMIZE_THROUGHPUT;

    alignas(CACHE_LINE_SIZE) std::atomic<T> elements_[size_];

    T do_pop(unsigned tail) noexcept {
        std::atomic<T>& q_element = details::map<SHUFFLE_BITS>(elements_, tail % size_);
        return Base::template do_pop_atomic<T, NIL>(q_element);
    }

    void do_push(T element, unsigned head) noexcept {
        std::atomic<T>& q_element = details::map<SHUFFLE_BITS>(elements_, head % size_);
        Base::template do_push_atomic<T, NIL>(element, q_element);
    }

public:
    using value_type = T;

    AtomicQueue() noexcept {
        assert(std::atomic<T>{NIL}.is_lock_free()); // Queue element type T is not atomic. Use AtomicQueue2/AtomicQueueB2 for such element types.
        for(auto p = elements_, q = elements_ + size_; p != q; ++p)
            p->store(NIL, X);
    }

    AtomicQueue(AtomicQueue const&) = delete;
    AtomicQueue& operator=(AtomicQueue const&) = delete;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, unsigned SIZE, bool MINIMIZE_CONTENTION = true, bool MAXIMIZE_THROUGHPUT = true, bool TOTAL_ORDER = false, bool SPSC = false>
class AtomicQueue2 : public AtomicQueueCommon<AtomicQueue2<T, SIZE, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>> {
    using Base = AtomicQueueCommon<AtomicQueue2<T, SIZE, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>>;
    using State = typename Base::State;
    friend Base;

    static constexpr unsigned size_ = MINIMIZE_CONTENTION ? details::round_up_to_power_of_2(SIZE) : SIZE;
    static constexpr int SHUFFLE_BITS = details::GetIndexShuffleBits<MINIMIZE_CONTENTION, size_, CACHE_LINE_SIZE / sizeof(State)>::value;
    static constexpr bool total_order_ = TOTAL_ORDER;
    static constexpr bool spsc_ = SPSC;
    static constexpr bool maximize_throughput_ = MAXIMIZE_THROUGHPUT;

    alignas(CACHE_LINE_SIZE) std::atomic<unsigned char> states_[size_] = {};
    alignas(CACHE_LINE_SIZE) T elements_[size_] = {};

    T do_pop(unsigned tail) noexcept {
        unsigned index = details::remap_index<SHUFFLE_BITS>(tail % size_);
        return Base::template do_pop_any<T>(states_[index], elements_[index]);
    }

    template<class U>
    void do_push(U&& element, unsigned head) noexcept {
        unsigned index = details::remap_index<SHUFFLE_BITS>(head % size_);
        Base::template do_push_any<U, T>(std::forward<U>(element), states_[index], elements_[index]);
    }

public:
    using value_type = T;

    AtomicQueue2() noexcept = default;
    AtomicQueue2(AtomicQueue2 const&) = delete;
    AtomicQueue2& operator=(AtomicQueue2 const&) = delete;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class A = std::allocator<T>, T NIL = details::nil<T>(), bool MAXIMIZE_THROUGHPUT = true, bool TOTAL_ORDER = false, bool SPSC = false>
class AtomicQueueB : private std::allocator_traits<A>::template rebind_alloc<std::atomic<T>>,
                     public AtomicQueueCommon<AtomicQueueB<T, A, NIL, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>> {
    using AllocatorElements = typename std::allocator_traits<A>::template rebind_alloc<std::atomic<T>>;
    using Base = AtomicQueueCommon<AtomicQueueB<T, A, NIL, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>>;
    friend Base;

    static constexpr bool total_order_ = TOTAL_ORDER;
    static constexpr bool spsc_ = SPSC;
    static constexpr bool maximize_throughput_ = MAXIMIZE_THROUGHPUT;

    static constexpr auto ELEMENTS_PER_CACHE_LINE = CACHE_LINE_SIZE / sizeof(std::atomic<T>);
    static_assert(ELEMENTS_PER_CACHE_LINE, "Unexpected ELEMENTS_PER_CACHE_LINE.");

    static constexpr auto SHUFFLE_BITS = details::GetCacheLineIndexBits<ELEMENTS_PER_CACHE_LINE>::value;
    static_assert(SHUFFLE_BITS, "Unexpected SHUFFLE_BITS.");

    // AtomicQueueCommon members are stored into by readers and writers.
    // Allocate these immutable members on another cache line which never gets invalidated by stores.
    alignas(CACHE_LINE_SIZE) unsigned size_;
    std::atomic<T>* elements_;

    T do_pop(unsigned tail) noexcept {
        std::atomic<T>& q_element = details::map<SHUFFLE_BITS>(elements_, tail & (size_ - 1));
        return Base::template do_pop_atomic<T, NIL>(q_element);
    }

    void do_push(T element, unsigned head) noexcept {
        std::atomic<T>& q_element = details::map<SHUFFLE_BITS>(elements_, head & (size_ - 1));
        Base::template do_push_atomic<T, NIL>(element, q_element);
    }

public:
    using value_type = T;
    using allocator_type = A;

    // The special member functions are not thread-safe.

    AtomicQueueB(unsigned size, A const& allocator = A{})
        : AllocatorElements(allocator)
        , size_(std::max(details::round_up_to_power_of_2(size), 1u << (SHUFFLE_BITS * 2)))
        , elements_(AllocatorElements::allocate(size_)) {
        assert(std::atomic<T>{NIL}.is_lock_free()); // Queue element type T is not atomic. Use AtomicQueue2/AtomicQueueB2 for such element types.
        std::uninitialized_fill_n(elements_, size_, NIL);
        assert(get_allocator() == allocator); // The standard requires the original and rebound allocators to manage the same state.
    }

    AtomicQueueB(AtomicQueueB&& b) noexcept
        : AllocatorElements(static_cast<AllocatorElements&&>(b)) // TODO: This must be noexcept, static_assert that.
        , Base(static_cast<Base&&>(b))
        , size_(std::exchange(b.size_, 0))
        , elements_(std::exchange(b.elements_, nullptr))
    {}

    AtomicQueueB& operator=(AtomicQueueB&& b) noexcept {
        b.swap(*this);
        return *this;
    }

    ~AtomicQueueB() noexcept {
        if(elements_) {
            details::destroy_n(elements_, size_);
            AllocatorElements::deallocate(elements_, size_); // TODO: This must be noexcept, static_assert that.
        }
    }

    A get_allocator() const noexcept {
        return *this; // The standard requires implicit conversion between rebound allocators.
    }

    void swap(AtomicQueueB& b) noexcept {
        using std::swap;
        swap(static_cast<AllocatorElements&>(*this), static_cast<AllocatorElements&>(b));
        Base::swap(b);
        swap(size_, b.size_);
        swap(elements_, b.elements_);
    }

    friend void swap(AtomicQueueB& a, AtomicQueueB& b) noexcept {
        a.swap(b);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class T, class A = std::allocator<T>, bool MAXIMIZE_THROUGHPUT = true, bool TOTAL_ORDER = false, bool SPSC = false>
class AtomicQueueB2 : private std::allocator_traits<A>::template rebind_alloc<unsigned char>,
                      public AtomicQueueCommon<AtomicQueueB2<T, A, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>> {
    using StorageAllocator = typename std::allocator_traits<A>::template rebind_alloc<unsigned char>;
    using Base = AtomicQueueCommon<AtomicQueueB2<T, A, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>>;
    using State = typename Base::State;
    using AtomicState = std::atomic<unsigned char>;
    friend Base;

    static constexpr bool total_order_ = TOTAL_ORDER;
    static constexpr bool spsc_ = SPSC;
    static constexpr bool maximize_throughput_ = MAXIMIZE_THROUGHPUT;

    // AtomicQueueCommon members are stored into by readers and writers.
    // Allocate these immutable members on another cache line which never gets invalidated by stores.
    alignas(CACHE_LINE_SIZE) unsigned size_;
    AtomicState* states_;
    T* elements_;

    static constexpr auto STATES_PER_CACHE_LINE = CACHE_LINE_SIZE / sizeof(AtomicState);
    static_assert(STATES_PER_CACHE_LINE, "Unexpected STATES_PER_CACHE_LINE.");

    static constexpr auto SHUFFLE_BITS = details::GetCacheLineIndexBits<STATES_PER_CACHE_LINE>::value;
    static_assert(SHUFFLE_BITS, "Unexpected SHUFFLE_BITS.");

    T do_pop(unsigned tail) noexcept {
        unsigned index = details::remap_index<SHUFFLE_BITS>(tail & (size_ - 1));
        return Base::template do_pop_any<T>(states_[index], elements_[index]);
    }

    template<class U>
    void do_push(U&& element, unsigned head) noexcept {
        unsigned index = details::remap_index<SHUFFLE_BITS>(head & (size_ - 1));
        Base::template do_push_any<U, T>(std::forward<U>(element), states_[index], elements_[index]);
    }

    template<class U>
    U* allocate_() {
        U* p = reinterpret_cast<U*>(StorageAllocator::allocate(size_ * sizeof(U)));
        assert(reinterpret_cast<uintptr_t>(p) % alignof(U) == 0); // Allocated storage must be suitably aligned for U.
        return p;
    }

    template<class U>
    void deallocate_(U* p) noexcept {
        StorageAllocator::deallocate(reinterpret_cast<unsigned char*>(p), size_ * sizeof(U)); // TODO: This must be noexcept, static_assert that.
    }

public:
    using value_type = T;
    using allocator_type = A;

    // The special member functions are not thread-safe.

    AtomicQueueB2(unsigned size, A const& allocator = A{})
        : StorageAllocator(allocator)
        , size_(std::max(details::round_up_to_power_of_2(size), 1u << (SHUFFLE_BITS * 2)))
        , states_(allocate_<AtomicState>())
        , elements_(allocate_<T>()) {
        std::uninitialized_fill_n(states_, size_, Base::EMPTY);
        A a = get_allocator();
        assert(a == allocator); // The standard requires the original and rebound allocators to manage the same state.
        for(auto p = elements_, q = elements_ + size_; p < q; ++p)
            std::allocator_traits<A>::construct(a, p);
    }

    AtomicQueueB2(AtomicQueueB2&& b) noexcept
        : StorageAllocator(static_cast<StorageAllocator&&>(b)) // TODO: This must be noexcept, static_assert that.
        , Base(static_cast<Base&&>(b))
        , size_(std::exchange(b.size_, 0))
        , states_(std::exchange(b.states_, nullptr))
        , elements_(std::exchange(b.elements_, nullptr))
    {}

    AtomicQueueB2& operator=(AtomicQueueB2&& b) noexcept {
        b.swap(*this);
        return *this;
    }

    ~AtomicQueueB2() noexcept {
        if(elements_) {
            A a = get_allocator();
            for(auto p = elements_, q = elements_ + size_; p < q; ++p)
                std::allocator_traits<A>::destroy(a, p);
            deallocate_(elements_);
            details::destroy_n(states_, size_);
            deallocate_(states_);
        }
    }

    A get_allocator() const noexcept {
        return *this; // The standard requires implicit conversion between rebound allocators.
    }

    void swap(AtomicQueueB2& b) noexcept {
        using std::swap;
        swap(static_cast<StorageAllocator&>(*this), static_cast<StorageAllocator&>(b));
        Base::swap(b);
        swap(size_, b.size_);
        swap(states_, b.states_);
        swap(elements_, b.elements_);
    }

    friend void swap(AtomicQueueB2& a, AtomicQueueB2& b) noexcept {
        a.swap(b);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class Queue>
struct RetryDecorator : Queue {
    using T = typename Queue::value_type;

    using Queue::Queue;

    void push(T element) noexcept {
        while(!this->try_push(element))
            spin_loop_pause();
    }

    T pop() noexcept {
        T element;
        while(!this->try_pop(element))
            spin_loop_pause();
        return element;
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_ATOMIC_QUEUE_H_INCLUDED
