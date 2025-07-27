#include "az/core/cpu_yield.hpp"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <emmintrin.h>
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM64)
#include <arm_acle.h>
#endif

namespace az {

void cpu_yield() {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    _mm_pause();
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM64)
    __yield();
#else
    asm volatile("nop" ::: "memory");
#endif
}

}  // namespace az
