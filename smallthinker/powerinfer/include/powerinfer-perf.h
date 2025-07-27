#pragma once

#include "powerinfer-api.h"

#ifdef __cplusplus
    extern "C" {
#endif // __cplusplus

    POWERINFER_API void powerinfer_perf_begin(const char *name);

    POWERINFER_API void powerinfer_perf_end(void);

#ifdef __cplusplus
    }
#endif // __cplusplus