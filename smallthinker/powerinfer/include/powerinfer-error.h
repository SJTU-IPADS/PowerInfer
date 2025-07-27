#pragma once

#ifndef POWERINFER_ERROR_DEF
#define POWERINFER_ERROR_DEF

#ifdef __cplusplus
    extern "C" {
#endif // __cplusplus

        struct PowerInferError {
            bool        error;
            const char *message;
        };

#ifdef __cplusplus
    }
#endif // __cplusplus

#endif // POWERINFER_ERROR_DEF
