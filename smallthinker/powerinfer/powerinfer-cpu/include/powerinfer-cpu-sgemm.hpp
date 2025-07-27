#pragma once

#include <cstdint>

#include "powerinfer-macro.hpp"

HOST_API bool llamafile_sgemm(int64_t, int64_t, int64_t, const void *, int64_t,
                     const void *, int64_t, void *, int64_t, int, int,
                     int, int, int);

