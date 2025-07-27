#pragma once

#include <cstdarg>
#include <cstdlib>
#include <cstdio>

#ifndef NDEBUG
#   define POWERINFER_UNREACHABLE() do { fprintf(stderr, "statement should be unreachable\n"); abort(); } while(0)
#elif defined(__GNUC__)
#   define POWERINFER_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#   define POWERINFER_UNREACHABLE() __assume(0)
#else
#   define POWERINFER_UNREACHABLE() ((void) 0)
#endif

inline void powerinfer_abort(const char * file, int line, const char * fmt, ...) {
    fflush(stdout);

    fprintf(stderr, "%s:%d: ", file, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fprintf(stderr, "\n");
    abort();
}

#define POWERINFER_ABORT(...) powerinfer_abort(__FILE__, __LINE__, __VA_ARGS__)
#define POWERINFER_ASSERT(x) if (!((x))) POWERINFER_ABORT("POWERINFER_ASSERT(%s) failed", #x)

#define CHECK_TRY_ERROR(expr)                                               \
  [&]() {                                                                   \
    try {                                                                   \
      expr;                                                                 \
      return 0;                                                             \
    } catch (const std::exception & err) {                                  \
      std::cerr << err.what() << "\nException caught at file:" << __FILE__  \
                << ", line:" << __LINE__ << ", func:" << __func__           \
                << std::endl;                                               \
      return 999;                                                           \
    }                                                                       \
  }()
