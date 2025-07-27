include(CheckCXXCompilerFlag)

if (CMAKE_OSX_ARCHITECTURES      STREQUAL "arm64" OR
    CMAKE_GENERATOR_PLATFORM_LWR STREQUAL "arm64" OR
    (NOT CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_GENERATOR_PLATFORM_LWR AND
        CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm.*|ARM64)$"))

    message(STATUS "ARM detected")

    if (MSVC AND NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "MSVC is not supported for ARM, use clang")
    elseif (NOT DISABLE_ARM_FEATURE_CHECK)
        check_cxx_compiler_flag(-mfp16-format=ieee POWERINFER_COMPILER_SUPPORTS_FP16_FORMAT_I3E)
        if (NOT "${POWERINFER_COMPILER_SUPPORTS_FP16_FORMAT_I3E}" STREQUAL "")
            list(APPEND ARCH_FLAGS -mfp16-format=ieee)
        endif()

        # -mcpu=native does not always enable all the features in some compilers,
        # so we check for them manually and enable them if available
        if (NOT ANDROID)
            execute_process(
                COMMAND ${CMAKE_C_COMPILER} -mcpu=native -E -v -
                INPUT_FILE "/dev/null"
                OUTPUT_QUIET
                ERROR_VARIABLE ARM_MCPU
                RESULT_VARIABLE ARM_MCPU_RESULT
            )
            if (NOT ARM_MCPU_RESULT)
                string(REGEX MATCH "-mcpu=[^ ']+" ARM_MCPU_FLAG "${ARM_MCPU}")
            endif()
            if ("${ARM_MCPU_FLAG}" STREQUAL "")
                set(ARM_MCPU_FLAG -mcpu=native)
                message(STATUS "ARM -mcpu not found, -mcpu=native will be used")
            endif()

            include(CheckCXXSourceRuns)

            function(check_arm_feature tag code)
                set(CMAKE_REQUIRED_FLAGS_SAVE ${CMAKE_REQUIRED_FLAGS})
                set(CMAKE_REQUIRED_FLAGS "${ARM_MCPU_FLAG}+${tag}")
                check_cxx_source_runs(
                    "${code}"
                    POWERINFER_MACHINE_SUPPORTS_${tag}
                )
                if (POWERINFER_MACHINE_SUPPORTS_${tag})
                    set(ARM_MCPU_FLAG_FIX "${ARM_MCPU_FLAG_FIX}+${tag}" PARENT_SCOPE)
                else()
                    set(ARM_MCPU_FLAG_FIX "${ARM_MCPU_FLAG_FIX}+no${tag}" PARENT_SCOPE)
                endif()
                set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_SAVE})
            endfunction()

            check_arm_feature(dotprod "#include <arm_neon.h>\nint main() { int8x16_t _a, _b; volatile int32x4_t _s = vdotq_s32(_s, _a, _b); return 0; }")
            check_arm_feature(i8mm    "#include <arm_neon.h>\nint main() { int8x16_t _a, _b; volatile int32x4_t _s = vmmlaq_s32(_s, _a, _b); return 0; }")
            check_arm_feature(sve     "#include <arm_sve.h>\nint main()  { svfloat32_t _a, _b; volatile svfloat32_t _c = svadd_f32_z(svptrue_b8(), _a, _b); return 0; }")

            list(APPEND ARCH_FLAGS "${ARM_MCPU_FLAG}${ARM_MCPU_FLAG_FIX}")

            # show enabled features
            if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
                set(FEAT_INPUT_FILE "NUL")
            else()
                set(FEAT_INPUT_FILE "/dev/null")
            endif()

            execute_process(
                COMMAND ${CMAKE_C_COMPILER} ${ARCH_FLAGS} -dM -E -
                INPUT_FILE ${FEAT_INPUT_FILE}
                OUTPUT_VARIABLE ARM_FEATURE
                RESULT_VARIABLE ARM_FEATURE_RESULT
            )
            if (ARM_FEATURE_RESULT)
                message(WARNING "Failed to get ARM features")
            else()
                foreach(feature DOTPROD SVE MATMUL_INT8 FMA FP16_VECTOR_ARITHMETIC)
                    string(FIND "${ARM_FEATURE}" "__ARM_FEATURE_${feature} 1" feature_pos)
                    if (NOT ${feature_pos} EQUAL -1)
                        message(STATUS "ARM feature ${feature} enabled")
                    endif()
                endforeach()
            endif()
        else()
            message(STATUS "Android NDK cross-compilation detected. Detecting CPU features.")
            set(ARM_BASE_ARCH_FLAG "-march=armv8-a")
            set(ARM_FEATURE_ADDITIONS "+fp16")
            list(APPEND ARCH_FLAGS "${ARM_BASE_ARCH_FLAG}${ARM_FEATURE_ADDITIONS}")
        endif()
    endif()
elseif (CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64" OR CMAKE_GENERATOR_PLATFORM_LWR MATCHES "^(x86_64|i686|amd64|x64|win32)$" OR
        (NOT CMAKE_OSX_ARCHITECTURES AND NOT CMAKE_GENERATOR_PLATFORM_LWR AND
        CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|i686|AMD64|amd64)$"))

    message(STATUS "x86 detected")

    if (MSVC)
        # instruction set detection for MSVC only
        include(${PROJECT_SOURCE_DIR}/cmake/FindSIMD.cmake)

        if (POWERINFER_AVX512)
            list(APPEND ARCH_FLAGS /arch:AVX512)
            # /arch:AVX512 includes: __AVX512F__, __AVX512CD__, __AVX512BW__, __AVX512DQ__, and __AVX512VL__
            # MSVC has no compile-time flags enabling specific
            # AVX512 extensions, neither it defines the
            # macros corresponding to the extensions.
            # Do it manually.
            list(APPEND ARCH_DEFINITIONS POWERINFER_AVX512)
            if (POWERINFER_AVX512_VBMI)
                list(APPEND ARCH_DEFINITIONS __AVX512VBMI__)
                if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
                    list(APPEND ARCH_FLAGS -mavx512vbmi)
                endif()
            endif()
            if (POWERINFER_AVX512_VNNI)
                list(APPEND ARCH_DEFINITIONS __AVX512VNNI__ POWERINFER_AVX512_VNNI)
                if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
                    list(APPEND ARCH_FLAGS -mavx512vnni)
                endif()
            endif()
            if (POWERINFER_AVX512_BF16)
                list(APPEND ARCH_DEFINITIONS __AVX512BF16__ POWERINFER_AVX512_BF16)
                if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
                    list(APPEND ARCH_FLAGS -mavx512bf16)
                endif()
            endif()
            if (POWERINFER_AMX_TILE)
                list(APPEND ARCH_DEFINITIONS __AMX_TILE__ POWERINFER_AMX_TILE)
            endif()
            if (POWERINFER_AMX_INT8)
                list(APPEND ARCH_DEFINITIONS __AMX_INT8__ POWERINFER_AMX_INT8)
            endif()
            if (POWERINFER_AMX_BF16)
                list(APPEND ARCH_DEFINITIONS __AMX_BF16__ POWERINFER_AMX_BF16)
            endif()
        elseif (POWERINFER_AVX2)
            list(APPEND ARCH_FLAGS /arch:AVX2)
            list(APPEND ARCH_DEFINITIONS POWERINFER_AVX2 POWERINFER_FMA POWERINFER_F16C)
        elseif (POWERINFER_AVX)
            list(APPEND ARCH_FLAGS /arch:AVX)
            list(APPEND ARCH_DEFINITIONS POWERINFER_AVX)
        else ()
            list(APPEND ARCH_FLAGS /arch:SSE4.2)
            list(APPEND ARCH_DEFINITIONS POWERINFER_SSE42)
        endif()
        if (POWERINFER_AVX_VNNI)
            list(APPEND ARCH_DEFINITIONS __AVXVNNI__ POWERINFER_AVX_VNNI)
        endif()
    else ()
        list(APPEND ARCH_FLAGS -march=native)
    endif()
else()
    message(STATUS "Unknown architecture")
endif()

add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${ARCH_FLAGS}>")
add_compile_options("$<$<COMPILE_LANGUAGE:C>:${ARCH_FLAGS}>")

list(APPEND OTHER_FLAGS -fPIC)
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${OTHER_FLAGS}>")
add_compile_options("$<$<COMPILE_LANGUAGE:C>:${OTHER_FLAGS}>")