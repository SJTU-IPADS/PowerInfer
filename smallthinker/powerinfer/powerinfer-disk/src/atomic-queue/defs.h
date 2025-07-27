/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
#ifndef ATOMIC_QUEUE_DEFS_H_INCLUDED
#define ATOMIC_QUEUE_DEFS_H_INCLUDED

// Copyright (c) 2019 Maxim Egorushkin. MIT License. See the full licence in file LICENSE.

#include <atomic>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <emmintrin.h>
namespace atomic_queue {
constexpr int CACHE_LINE_SIZE = 64;
static inline void spin_loop_pause() noexcept {
    _mm_pause();
}
} // namespace atomic_queue
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM64)
namespace atomic_queue {
constexpr int CACHE_LINE_SIZE = 64;
static inline void spin_loop_pause() noexcept {
#if (defined(__ARM_ARCH_6K__) || \
     defined(__ARM_ARCH_6Z__) || \
     defined(__ARM_ARCH_6ZK__) || \
     defined(__ARM_ARCH_6T2__) || \
     defined(__ARM_ARCH_7__) || \
     defined(__ARM_ARCH_7A__) || \
     defined(__ARM_ARCH_7R__) || \
     defined(__ARM_ARCH_7M__) || \
     defined(__ARM_ARCH_7S__) || \
     defined(__ARM_ARCH_8A__) || \
     defined(__aarch64__))
    asm volatile ("yield" ::: "memory");
#elif defined(_M_ARM64)
    __yield();
#else
    asm volatile ("nop" ::: "memory");
#endif
}
} // namespace atomic_queue
#elif defined(__ppc64__) || defined(__powerpc64__)
namespace atomic_queue {
constexpr int CACHE_LINE_SIZE = 128; // TODO: Review that this is the correct value.
static inline void spin_loop_pause() noexcept {
    asm volatile("or 31,31,31 # very low priority"); // TODO: Review and benchmark that this is the right instruction.
}
} // namespace atomic_queue
#elif defined(__s390x__)
namespace atomic_queue {
constexpr int CACHE_LINE_SIZE = 256; // TODO: Review that this is the correct value.
static inline void spin_loop_pause() noexcept {} // TODO: Find the right instruction to use here, if any.
} // namespace atomic_queue
#elif defined(__riscv)
namespace atomic_queue {
constexpr int CACHE_LINE_SIZE = 64;
static inline void spin_loop_pause() noexcept {
    asm volatile (".insn i 0x0F, 0, x0, x0, 0x010");
}
} // namespace atomic_queue
#else
#ifdef _MSC_VER
#pragma message("Unknown CPU architecture. Using L1 cache line size of 64 bytes and no spinloop pause instruction.")
#else
#warning "Unknown CPU architecture. Using L1 cache line size of 64 bytes and no spinloop pause instruction."
#endif
namespace atomic_queue {
constexpr int CACHE_LINE_SIZE = 64; // TODO: Review that this is the correct value.
static inline void spin_loop_pause() noexcept {}
} // namespace atomic_queue
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace atomic_queue {

#if defined(__GNUC__) || defined(__clang__)
#define ATOMIC_QUEUE_LIKELY(expr) __builtin_expect(static_cast<bool>(expr), 1)
#define ATOMIC_QUEUE_UNLIKELY(expr) __builtin_expect(static_cast<bool>(expr), 0)
#define ATOMIC_QUEUE_NOINLINE __attribute__((noinline))
#else
#define ATOMIC_QUEUE_LIKELY(expr) (expr)
#define ATOMIC_QUEUE_UNLIKELY(expr) (expr)
#define ATOMIC_QUEUE_NOINLINE
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

auto constexpr A = std::memory_order_acquire;
auto constexpr R = std::memory_order_release;
auto constexpr X = std::memory_order_relaxed;
auto constexpr C = std::memory_order_seq_cst;
auto constexpr AR = std::memory_order_acq_rel;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace atomic_queue

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // ATOMIC_QUEUE_DEFS_H_INCLUDED
