#pragma once

#if defined(_WIN32) && !defined(__MINGW32__)
#    define POWERINFER_IMPORT __declspec(dllimport)
#else
#    define POWERINFER_IMPORT __attribute__ ((visibility ("default")))
#endif 


#if defined(_WIN32) && !defined(__MINGW32__)
#    define POWERINFER_EXPORT __declspec(dllexport)
#else
#    define POWERINFER_EXPORT __attribute__ ((visibility ("default")))
#endif 


#ifdef DISK_SHARED
    #ifdef DISK_EXPORT
        #define DISK_API POWERINFER_EXPORT
    #else // DISK_EXPORT
        #define DISK_API POWERINFER_IMPORT
    #endif // DISK_EXPORT
#else  // DISK_SHARED
    #define DISK_API
#endif // DISK_SHARED

#ifdef HOST_SHARED
    #ifdef HOST_EXPORT
        #define HOST_API POWERINFER_EXPORT
    #else // HOST_EXPORT
        #define HOST_API POWERINFER_IMPORT
    #endif // HOST_EXPORT
#else  // HOST_SHARED
    #define HOST_API
#endif // HOST_SHARED

#ifdef CACHE_SHARED
    #ifdef CACHE_EXPORT
        #define CACHE_API POWERINFER_EXPORT
    #else // CACHE_EXPORT
        #define CACHE_API POWERINFER_IMPORT
    #endif // CACHE_EXPORT
#else  // CACHE_SHARED
    #define CACHE_API
#endif // CACHE_SHARED
