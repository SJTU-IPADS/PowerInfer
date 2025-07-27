#define CL_TARGET_OPENCL_VERSION GGML_OPENCL_TARGET_VERSION
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

// suppress warnings in CL headers for GCC and Clang
#pragma GCC diagnostic ignored "-Woverlength-strings"
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wgnu-anonymous-struct"
#endif

#include "ggml-opencl.h"
#include "ggml-backend.h"
#include "ggml-impl.h"
#include "ggml-backend-impl.h"
#include "ggml.h"

#include <CL/cl.h>

#include <string.h>

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <fstream>
#include <limits>
#include <vector>
#include <string>
#include <cmath>
#include <memory>
#include <charconv>
#include <mutex>

#undef MIN
#undef MAX
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define UNUSED(x) (void)(x)

#define CL_CHECK(err)                                               \
    do {                                                            \
        cl_int err_ = (err);                                        \
        if (err_ != CL_SUCCESS) {                                   \
            GGML_LOG_ERROR("ggml_opencl: %s error %d at %s:%d\n",  \
                #err, err_, __FILE__, __LINE__);                    \
            GGML_ASSERT(0);                                         \
        }                                                           \
    } while (0)

//------------------------------------------------------------------------------
// OpenCL
//------------------------------------------------------------------------------

bool ggml_cl_compute_forward(ggml_backend_t backend, struct ggml_tensor * tensor);

enum GPU_FAMILY {
    ADRENO,
    INTEL,
    UNKNOWN,
};

enum ADRENO_GPU_GEN {
    ADRENO_UNKNOWN,
    A7X,
    A8X,
    X1E,
};

enum ADRENO_CL_COMPILER_TYPE {
    E031,
    DX,
};

struct ggml_cl_version {
    cl_uint major = 0;
    cl_uint minor = 0;
};


struct ggml_cl_compiler_version {
    ADRENO_CL_COMPILER_TYPE type;
    int major = -1;
    int minor = -1;
    int patch = -1;

    bool same(ADRENO_CL_COMPILER_TYPE t, int x, int y, int z) const {
        return major == x && minor == y && patch == z && type == t;
    }
    bool newer_than(ADRENO_CL_COMPILER_TYPE t, int x, int y, int z) const {
        return major*10000 + minor*100 + patch > x*10000 + y*100 + z && type == t;
    }
    bool newer_than_or_same(ADRENO_CL_COMPILER_TYPE t, int x, int y, int z) const {
        return same(t, x, y, z) || newer_than(t, x, y, z);
    }
};

static size_t align_to(size_t value, size_t to_alignment) {
    GGML_ASSERT(to_alignment && "Invalid alignment (must be non-zero)");
    GGML_ASSERT((to_alignment & (to_alignment - 1)) == 0 && "to_alignment must be power-of-two");

    return ((value + to_alignment - 1) / to_alignment) * to_alignment;
}


// Parses a version string of form "XX.YY ". On an error returns ggml_cl_version with all zeroes.
static ggml_cl_version parse_cl_version(std::string_view str) {
    size_t major_str_begin = 0;
    size_t major_str_end   = str.find(".", major_str_begin);
    if (major_str_end == std::string::npos) {
        return {};
    }

    size_t minor_str_begin = major_str_end + 1;
    size_t minor_str_end   = str.find(" ", minor_str_begin);
    if (minor_str_end == std::string::npos) {
        return {};
    }

    cl_uint version_major;
    if (std::from_chars(str.data() + major_str_begin, str.data() + major_str_end, version_major).ec != std::errc{}) {
        return {};
    }

    cl_uint version_minor;
    if (std::from_chars(str.data() + minor_str_begin, str.data() + minor_str_end, version_minor).ec != std::errc{}) {
        return {};
    }
    return { version_major, version_minor };
}

// Returns OpenCL platform's version. On an error returns ggml_cl_version with all zeroes.
static ggml_cl_version get_opencl_platform_version(cl_platform_id platform) {
    size_t param_size;
    CL_CHECK(clGetPlatformInfo(platform, CL_PLATFORM_VERSION, 0, nullptr, &param_size));
    std::unique_ptr<char[]> param_storage(new char[param_size]);
    CL_CHECK(clGetPlatformInfo(platform, CL_PLATFORM_VERSION, param_size, param_storage.get(), nullptr));

    auto              param_value    = std::string_view(param_storage.get(), param_size);
    const std::string version_prefix = "OpenCL ";  // Suffix: "XX.YY <platform-specific-info>"
    if (param_value.find(version_prefix) != 0) {
        return {};
    }
    param_value.remove_prefix(version_prefix.length());
    return parse_cl_version(param_value);
}

// Return a version to use in OpenCL C compilation. On an error returns ggml_cl_version with all zeroes.
static ggml_cl_version get_opencl_c_version(ggml_cl_version platform_version, cl_device_id device) {
    size_t param_size;

#if CL_TARGET_OPENCL_VERSION >= 300
    if (platform_version.major >= 3) {
        CL_CHECK(clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_ALL_VERSIONS, 0, nullptr, &param_size));
        if (!param_size) {
            return {};
        }

        std::unique_ptr<cl_name_version[]> versions(new cl_name_version[param_size]);
        CL_CHECK(clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_ALL_VERSIONS, param_size, versions.get(), nullptr));
        unsigned versions_count = param_size / sizeof(cl_name_version);

        cl_version version_max = 0;
        for (unsigned i = 0; i < versions_count; i++) {
            version_max = std::max<cl_version>(versions[i].version, version_max);
        }

        return { CL_VERSION_MAJOR(version_max), CL_VERSION_MINOR(version_max) };
    }
#else
    GGML_UNUSED(platform_version);
#endif  // CL_TARGET_OPENCL_VERSION >= 300

    CL_CHECK(clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_VERSION, 0, nullptr, &param_size));
    if (!param_size) {
        return {};
    }

    std::unique_ptr<char[]> param_storage(new char[param_size]);
    CL_CHECK(clGetDeviceInfo(device, CL_DEVICE_OPENCL_C_VERSION, param_size, param_storage.get(), nullptr));
    auto param_value = std::string_view(param_storage.get(), param_size);

    const std::string version_prefix = "OpenCL C ";  // Suffix: "XX.YY <platform-specific-info>"
    if (param_value.find(version_prefix) != 0) {
        return {};
    }
    param_value.remove_prefix(version_prefix.length());

    return parse_cl_version(param_value);
}

static ADRENO_GPU_GEN get_adreno_gpu_gen(const char *device_name) {
    if (strstr(device_name, "730") ||
        strstr(device_name, "740") ||
        strstr(device_name, "750")) {
        return ADRENO_GPU_GEN::A7X;
    }

    if (strstr(device_name, "830")) {
        return ADRENO_GPU_GEN::A8X;
    }

    if (strstr(device_name, "X1")) {
        return ADRENO_GPU_GEN::X1E;
    }

    return ADRENO_GPU_GEN::ADRENO_UNKNOWN;
}

static ggml_cl_compiler_version get_adreno_cl_compiler_version(const char *driver_version) {
    std::string driver_ver_str(driver_version);
    ADRENO_CL_COMPILER_TYPE type = ADRENO_CL_COMPILER_TYPE::E031;
    size_t compiler_ver_pos = driver_ver_str.find("E031");
    size_t compiler_ver_len = 13;
    size_t compiler_major_offset = 5;
    size_t compiler_minor_offset = 8;
    size_t compiler_patch_offset = 11;

    if (compiler_ver_pos == std::string::npos) {
        compiler_ver_pos = driver_ver_str.find("DX");
        if (compiler_ver_pos == std::string::npos) {
            return {};
        }
        type = ADRENO_CL_COMPILER_TYPE::DX;
        compiler_ver_len = 11;
        compiler_major_offset = 3;
    }

    std::string compiler_ver_str = driver_ver_str.substr(compiler_ver_pos, compiler_ver_len);
    int major = std::atoi(compiler_ver_str.substr(compiler_major_offset, 2).c_str());
    int minor = std::atoi(compiler_ver_str.substr(compiler_minor_offset, 2).c_str());
    int patch = std::atoi(compiler_ver_str.substr(compiler_patch_offset, 2).c_str());
    return { type, major, minor, patch };
}

struct ggml_backend_opencl_context;

// backend device context
struct ggml_backend_opencl_device_context {
    cl_platform_id platform;
    std::string platform_name;

    cl_device_id   device;
    std::string    device_name;
    cl_device_type device_type;
    std::string    device_version;

    // Initialized by ggml_cl2_init().
    ggml_backend_opencl_context * backend_ctx = nullptr;

    // Initialized by ggml_backend_opencl_device_get_buffer_type()
    ggml_backend_buffer_type buffer_type;

    cl_context context = nullptr;
};

// backend context
struct ggml_backend_opencl_context {
    cl_device_id device;
    std::string device_name;

    std::string driver_version;

    GPU_FAMILY gpu_family;
    ADRENO_GPU_GEN adreno_gen;

    cl_int alignment;
    size_t max_alloc_size;
    bool fp16_support;
    bool has_vector_subgroup_broadcast;
    ggml_cl_compiler_version adreno_cl_compiler_version;

    int adreno_wave_size;

    cl_bool non_uniform_workgroups;

    cl_context context;
    cl_command_queue queue;

    cl_program program_add;
    cl_program program_clamp;
    cl_program program_cpy;
    cl_program program_cvt;
    cl_program program_diag_mask_inf;
    cl_program program_gelu;
    cl_program program_gemv_noshuffle_general;
    cl_program program_gemv_noshuffle;
    cl_program program_get_rows;
    cl_program program_im2col_f16;
    cl_program program_im2col_f32;
    cl_program program_mul_mat_Ab_Bi_8x4;
    cl_program program_mul_mv_q4_0_f32;
    cl_program program_mul_mv_q4_0_f32_v;
    cl_program program_mul_mv_q4_0_f32_8x_flat;
    cl_program program_mul_mv_q4_0_f32_1d_8x_flat;
    cl_program program_mul_mv_q4_0_f32_1d_16x_flat;
    cl_program program_mul_mv_q6_K;
    cl_program program_mul_mv_f16_f16;
    cl_program program_mul_mv_f16_f32_1row;
    cl_program program_mul_mv_f16_f32_l4;
    cl_program program_mul_mv_f16_f32;
    cl_program program_mul_mv_f32_f32;
    cl_program program_mul;
    cl_program program_div;
    cl_program program_sub;
    cl_program program_norm;
    cl_program program_relu;
    cl_program program_rms_norm;
    cl_program program_group_norm;
    cl_program program_rope;
    cl_program program_scale;
    cl_program program_silu;
    cl_program program_sigmoid;
    cl_program program_softmax_f32;
    cl_program program_softmax_f16;
    cl_program program_softmax_4_f32;
    cl_program program_softmax_4_f16;
    cl_program program_argsort_f32_i32;
    cl_program program_sum_rows_f32;
    cl_program program_repeat;
    cl_program program_pad;
    cl_program program_tanh;
    cl_program program_upscale;
    cl_program program_concat;
    cl_program program_tsembd;

    cl_kernel kernel_add, kernel_add_row;
    cl_kernel kernel_mul, kernel_mul_row;
    cl_kernel kernel_div, kernel_div_row;
    cl_kernel kernel_sub, kernel_sub_row;
    cl_kernel kernel_scale;
    cl_kernel kernel_silu, kernel_silu_4;
    cl_kernel kernel_gelu, kernel_gelu_4;
    cl_kernel kernel_gelu_quick, kernel_gelu_quick_4;
    cl_kernel kernel_relu;
    cl_kernel kernel_sigmoid_f32, kernel_sigmoid_f16;
    cl_kernel kernel_clamp;
    cl_kernel kernel_norm;
    cl_kernel kernel_rms_norm;
    cl_kernel kernel_group_norm;
    cl_kernel kernel_diag_mask_inf, kernel_diag_mask_inf_8;
    cl_kernel kernel_soft_max, kernel_soft_max_4;
    cl_kernel kernel_soft_max_f16, kernel_soft_max_4_f16;
    cl_kernel kernel_get_rows_f32, kernel_get_rows_f16, kernel_get_rows_q4_0;
    cl_kernel kernel_rope_norm_f32, kernel_rope_norm_f16, kernel_rope_neox_f32, kernel_rope_neox_f16;
    cl_kernel kernel_rope_multi_f32, kernel_rope_multi_f16, kernel_rope_vision_f32, kernel_rope_vision_f16;
    cl_kernel kernel_cpy_f16_f16, kernel_cpy_f16_f32, kernel_cpy_f32_f16, kernel_cpy_f32_f32;
    cl_kernel kernel_mul_mat_f32_f32;
    cl_kernel kernel_mul_mat_f16_f16;
    cl_kernel kernel_mul_mat_f16_f32_1row;
    cl_kernel kernel_mul_mat_f16_f32;
    cl_kernel kernel_mul_mat_f16_f32_l4;
    cl_kernel kernel_mul_mat_q4_0_f32, kernel_mul_mat_q4_0_f32_v;
    cl_kernel kernel_convert_block_q4_0, kernel_restore_block_q4_0;
    cl_kernel kernel_mul_mat_q4_0_f32_8x_flat;
    cl_kernel kernel_convert_block_q4_0_noshuffle;
    cl_kernel kernel_mul_mat_q4_0_f32_1d_8x_flat, kernel_mul_mat_q4_0_f32_1d_16x_flat;
    cl_kernel kernel_mul_mv_q6_K_f32;
    cl_kernel kernel_im2col_f32, kernel_im2col_f16;
    cl_kernel kernel_argsort_f32_i32;
    cl_kernel kernel_sum_rows_f32;
    cl_kernel kernel_repeat;
    cl_kernel kernel_pad;
    cl_kernel kernel_tanh_f32_nd;
    cl_kernel kernel_tanh_f16_nd;
    cl_kernel kernel_upscale;
    cl_kernel kernel_upscale_bilinear;
    cl_kernel kernel_concat_f32_contiguous;
    cl_kernel kernel_concat_f32_non_contiguous;
    cl_kernel kernel_timestep_embedding;

#ifdef GGML_OPENCL_USE_ADRENO_KERNELS
    // Transpose kernels
    cl_program program_transpose;

    cl_kernel kernel_transpose_32;
    cl_kernel kernel_transpose_32_16;
    cl_kernel kernel_transpose_16;

    cl_mem A_s_d_max;            // max scale buffer size for transpose
    cl_mem A_q_d_max;            // max weight buffer size for transpose
    cl_mem B_d_max;              // max activation buffer size for transpose

    // Gemm and Gemv related programs, kernels, etc
    cl_program program_CL_gemm;
    cl_program program_CL_gemv_general;
    cl_program program_CL_gemv_4096_1_11008;
    cl_program program_CL_gemv_4096_1_4096;
    cl_program program_CL_gemv_11008_1_4096;
    cl_program program_CL_gemv_32000_1_4096;
    cl_kernel CL_mul_mat_Ab_Bi_8x4;
    cl_kernel CL_mul_mat_vec_q4_0_f32_1d_4x_flat_general;
    cl_kernel CL_mul_mat_vec_q4_0_f32_1d_4x_flat_4096_1_11008;
    cl_kernel CL_mul_mat_vec_q4_0_f32_1d_4x_flat_4096_1_4096;
    cl_kernel CL_mul_mat_vec_q4_0_f32_1d_4x_flat_11008_1_4096;
    cl_kernel CL_mul_mat_vec_q4_0_f32_1d_4x_flat_32000_1_4096;
#endif // GGML_OPENCL_USE_ADRENO_KERNELS
};

// All registered devices with a default device in the front.
static std::vector<ggml_backend_device> g_ggml_backend_opencl_devices;

// Profiling
#ifdef GGML_OPENCL_PROFILING
struct ProfilingInfo {
    std::string op_name;
    std::string kernel_name;

    cl_kernel kernel;
    cl_event evt;

    cl_ulong cmd_queued;
    cl_ulong cmd_submit;
    cl_ulong cmd_start;
    cl_ulong cmd_end;
    cl_ulong overhead_start;
    cl_ulong overhead_end;
    // For the times below, see spec for clGetEventProfilingInfo
    // The time kernel spent in cmd queue - SUBMIT - QUEUED
    cl_ulong cmd_queued_duration_ns;
    // The time kernel spent for submission - START - SUBMIT
    cl_ulong cmd_submit_duration_ns;
    // Kernel execution time in nanoseconds - END - START
    cl_ulong cmd_duration_ns;
    // The time for the kernel to complete - COMPLETE - END
    cl_ulong cmd_complete_duration_ns;
    // Total time to finish the kernel - COMPELTE - QUEUED
    cl_ulong cmd_total_duration_ns;
    // Global and local work sizes.
    size_t global_size[3];
    size_t local_size[3];
    // Op output size.
    size_t output_size[4];
};

std::vector<ProfilingInfo> g_profiling_info;
#endif

inline std::string read_file(const std::string &path) {
  std::ifstream ifs(path);
  if (!ifs) {
    return "";
  }
  std::string text;
  ifs.seekg(0, std::ios::end);
  text.resize(ifs.tellg());
  ifs.seekg(0, std::ios::beg);
  ifs.read(&text[0], text.size());
  return text;
}

static cl_program build_program_from_source(cl_context ctx, cl_device_id dev, const char* program_buffer, const std::string &compile_opts) {
    cl_program p;
    char *program_log;
    size_t program_size;
    size_t log_size;
    int err;

    program_size = strlen(program_buffer);

    p = clCreateProgramWithSource(ctx, 1, (const char**)&program_buffer, &program_size, &err);
    if(err < 0) {
        GGML_LOG_ERROR("OpenCL error creating program");
        exit(1);
    }

    err = clBuildProgram(p, 0, NULL, compile_opts.c_str(), NULL, NULL);
    if(err < 0) {
        clGetProgramBuildInfo(p, dev, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        program_log = (char*) malloc(log_size + 1);
        program_log[log_size] = '\0';
        clGetProgramBuildInfo(p, dev, CL_PROGRAM_BUILD_LOG, log_size + 1, program_log, NULL);
        GGML_LOG_ERROR("ggml_opencl: kernel compile error:\n\n%s\n", program_log);
        free(program_log);
        exit(1);
    }

    return p;
}

static void load_cl_kernels(ggml_backend_opencl_context *backend_ctx, ggml_cl_version opencl_c_version) {
    cl_int err;

    // compiler options for general kernels
    auto opencl_c_std =
        std::string("CL") + std::to_string(opencl_c_version.major) + "." + std::to_string(opencl_c_version.minor);
    std::string compile_opts = std::string("-cl-std=") + opencl_c_std +
                               " -cl-mad-enable -cl-unsafe-math-optimizations"
                               " -cl-finite-math-only -cl-fast-relaxed-math";

    GGML_LOG_INFO("ggml_opencl: loading OpenCL kernels");

    // add
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "add.cl.h"
        };
#else
        const std::string kernel_src = read_file("add.cl");
#endif
        backend_ctx->program_add =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_add     = clCreateKernel(backend_ctx->program_add, "kernel_add", &err), err));
        CL_CHECK((backend_ctx->kernel_add_row = clCreateKernel(backend_ctx->program_add, "kernel_add_row", &err), err));
        GGML_LOG_CONT(".");
    }

    // clamp
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "clamp.cl.h"
        };
#else
        const std::string kernel_src = read_file("clamp.cl");
#endif
        backend_ctx->program_clamp =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_clamp = clCreateKernel(backend_ctx->program_clamp, "kernel_clamp", &err), err));
        GGML_LOG_CONT(".");
    }

    // cpy
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "cpy.cl.h"
        };
#else
        const std::string kernel_src = read_file("cpy.cl");
#endif
        backend_ctx->program_cpy =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_cpy_f16_f16 = clCreateKernel(backend_ctx->program_cpy, "kernel_cpy_f16_f16", &err), err));
        CL_CHECK((backend_ctx->kernel_cpy_f16_f32 = clCreateKernel(backend_ctx->program_cpy, "kernel_cpy_f16_f32", &err), err));
        CL_CHECK((backend_ctx->kernel_cpy_f32_f16 = clCreateKernel(backend_ctx->program_cpy, "kernel_cpy_f32_f16", &err), err));
        CL_CHECK((backend_ctx->kernel_cpy_f32_f32 = clCreateKernel(backend_ctx->program_cpy, "kernel_cpy_f32_f32", &err), err));
        GGML_LOG_CONT(".");
    }

    // cvt
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "cvt.cl.h"
        };
#else
        const std::string kernel_src = read_file("cvt.cl");
#endif
        backend_ctx->program_cvt =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_convert_block_q4_0_noshuffle = clCreateKernel(backend_ctx->program_cvt, "kernel_convert_block_q4_0_noshuffle", &err), err));
        CL_CHECK((backend_ctx->kernel_convert_block_q4_0  = clCreateKernel(backend_ctx->program_cvt, "kernel_convert_block_q4_0", &err), err));
        CL_CHECK((backend_ctx->kernel_restore_block_q4_0  = clCreateKernel(backend_ctx->program_cvt, "kernel_restore_block_q4_0", &err), err));
        GGML_LOG_CONT(".");
    }

    // diag_mask_inf
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "diag_mask_inf.cl.h"
        };
#else
        const std::string kernel_src = read_file("diag_mask_inf.cl");
#endif
        backend_ctx->program_diag_mask_inf =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_diag_mask_inf_8 = clCreateKernel(backend_ctx->program_diag_mask_inf, "kernel_diag_mask_inf_8", &err), err));
        CL_CHECK((backend_ctx->kernel_diag_mask_inf   = clCreateKernel(backend_ctx->program_diag_mask_inf, "kernel_diag_mask_inf", &err), err));
        GGML_LOG_CONT(".");
    }

    // gelu
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "gelu.cl.h"
        };
#else
        const std::string kernel_src = read_file("gelu.cl");
#endif
        backend_ctx->program_gelu =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_gelu         = clCreateKernel(backend_ctx->program_gelu, "kernel_gelu", &err), err));
        CL_CHECK((backend_ctx->kernel_gelu_4       = clCreateKernel(backend_ctx->program_gelu, "kernel_gelu_4", &err), err));
        CL_CHECK((backend_ctx->kernel_gelu_quick   = clCreateKernel(backend_ctx->program_gelu, "kernel_gelu_quick", &err), err));
        CL_CHECK((backend_ctx->kernel_gelu_quick_4 = clCreateKernel(backend_ctx->program_gelu, "kernel_gelu_quick_4", &err), err));
        GGML_LOG_CONT(".");
    }

    // get_rows
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "get_rows.cl.h"
        };
#else
        const std::string kernel_src = read_file("get_rows.cl");
#endif
        backend_ctx->program_get_rows =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_get_rows_f32  = clCreateKernel(backend_ctx->program_get_rows, "kernel_get_rows_f32", &err), err));
        CL_CHECK((backend_ctx->kernel_get_rows_f16  = clCreateKernel(backend_ctx->program_get_rows, "kernel_get_rows_f16", &err), err));
        CL_CHECK((backend_ctx->kernel_get_rows_q4_0 = clCreateKernel(backend_ctx->program_get_rows, "kernel_get_rows_q4_0", &err), err));
        GGML_LOG_CONT(".");
    }

    // im2col_f32
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "im2col_f32.cl.h"
        };
#else
        const std::string kernel_src = read_file("im2col_f32.cl");
#endif
        backend_ctx->program_im2col_f32 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_im2col_f32 = clCreateKernel(backend_ctx->program_im2col_f32, "kernel_im2col_f32", &err), err));
        GGML_LOG_CONT(".");
    }

    // im2col_f16
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "im2col_f16.cl.h"
        };
#else
        const std::string kernel_src = read_file("im2col_f16.cl");
#endif
        backend_ctx->program_im2col_f16 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_im2col_f16 = clCreateKernel(backend_ctx->program_im2col_f16, "kernel_im2col_f16", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul_mv_q4_0_f32
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "mul_mv_q4_0_f32.cl.h"
        };
#else
        const std::string kernel_src = read_file("mul_mv_q4_0_f32.cl");
#endif
        backend_ctx->program_mul_mv_q4_0_f32 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_mul_mat_q4_0_f32 = clCreateKernel(backend_ctx->program_mul_mv_q4_0_f32, "kernel_mul_mat_q4_0_f32", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul_mv_q4_0_f32_v
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "mul_mv_q4_0_f32_v.cl.h"
        };
#else
        const std::string kernel_src = read_file("mul_mv_q4_0_f32_v.cl");
#endif
        backend_ctx->program_mul_mv_q4_0_f32_v =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_mul_mat_q4_0_f32_v = clCreateKernel(backend_ctx->program_mul_mv_q4_0_f32_v, "kernel_mul_mat_q4_0_f32_v", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul_mv_q4_0_f32_8x_flat
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "mul_mv_q4_0_f32_8x_flat.cl.h"
        };
#else
        const std::string kernel_src = read_file("mul_mv_q4_0_f32_8x_flat.cl");
#endif
        backend_ctx->program_mul_mv_q4_0_f32_8x_flat =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_mul_mat_q4_0_f32_8x_flat = clCreateKernel(backend_ctx->program_mul_mv_q4_0_f32_8x_flat, "kernel_mul_mat_q4_0_f32_8x_flat", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul_mv_q4_0_f32_1d_8x_flat
    // This kernel does not compiler on Adreno cl compiler 38.01. Skip it for
    // those compiler versions since it is anyway not used for Adreno.
    if (backend_ctx->gpu_family != ADRENO ||
        backend_ctx->adreno_cl_compiler_version.newer_than_or_same(E031, 38, 11, 0) ||
        backend_ctx->adreno_cl_compiler_version.type == DX) {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "mul_mv_q4_0_f32_1d_8x_flat.cl.h"
        };
#else
        const std::string kernel_src = read_file("mul_mv_q4_0_f32_1d_8x_flat.cl");
#endif
        backend_ctx->program_mul_mv_q4_0_f32_1d_8x_flat =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_mul_mat_q4_0_f32_1d_8x_flat = clCreateKernel(backend_ctx->program_mul_mv_q4_0_f32_1d_8x_flat, "kernel_mul_mat_q4_0_f32_1d_8x_flat", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul_mv_q4_0_f32_1d_16x_flat
    // This kernel does not compiler on Adreno cl compiler 38.01. Skip it for
    // those compiler versions since it is anyway not used for Adreno.
    if (backend_ctx->gpu_family != ADRENO ||
        backend_ctx->adreno_cl_compiler_version.newer_than_or_same(E031, 38, 11, 0) ||
    backend_ctx->adreno_cl_compiler_version.type == DX) {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "mul_mv_q4_0_f32_1d_16x_flat.cl.h"
        };
#else
        const std::string kernel_src = read_file("mul_mv_q4_0_f32_1d_16x_flat.cl");
#endif
        backend_ctx->program_mul_mv_q4_0_f32_1d_16x_flat =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_mul_mat_q4_0_f32_1d_16x_flat = clCreateKernel(backend_ctx->program_mul_mv_q4_0_f32_1d_16x_flat, "kernel_mul_mat_q4_0_f32_1d_16x_flat", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul_mv_q6_k
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "mul_mv_q6_k.cl.h"
        };
#else
        const std::string kernel_src = read_file("mul_mv_q6_k.cl");
#endif
        backend_ctx->program_mul_mv_q6_K =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_mul_mv_q6_K_f32 = clCreateKernel(backend_ctx->program_mul_mv_q6_K, "kernel_mul_mv_q6_K_f32", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul_mv_f16_f16
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "mul_mv_f16_f16.cl.h"
        };
#else
        const std::string kernel_src = read_file("mul_mv_f16_f16.cl");
#endif
        backend_ctx->program_mul_mv_f16_f16 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_mul_mat_f16_f16 = clCreateKernel(backend_ctx->program_mul_mv_f16_f16, "kernel_mul_mat_f16_f16", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul_mv_f16_f32_1row
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "mul_mv_f16_f32_1row.cl.h"
        };
#else
        const std::string kernel_src = read_file("mul_mv_f16_f32_1row.cl");
#endif
        backend_ctx->program_mul_mv_f16_f32_1row =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_mul_mat_f16_f32_1row = clCreateKernel(backend_ctx->program_mul_mv_f16_f32_1row, "kernel_mul_mat_f16_f32_1row", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul_mv_f16_f32_l4
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "mul_mv_f16_f32_l4.cl.h"
        };
#else
        const std::string kernel_src = read_file("mul_mv_f16_f32_l4.cl");
#endif
        backend_ctx->program_mul_mv_f16_f32_l4 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_mul_mat_f16_f32_l4   = clCreateKernel(backend_ctx->program_mul_mv_f16_f32_l4, "kernel_mul_mat_f16_f32_l4", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul_mv_f16_f32
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "mul_mv_f16_f32.cl.h"
        };
#else
        const std::string kernel_src = read_file("mul_mv_f16_f32.cl");
#endif
        backend_ctx->program_mul_mv_f16_f32 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_mul_mat_f16_f32 = clCreateKernel(backend_ctx->program_mul_mv_f16_f32, "kernel_mul_mat_f16_f32", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul_mv_f32_f32
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "mul_mv_f32_f32.cl.h"
        };
#else
        const std::string kernel_src = read_file("mul_mv_f32_f32.cl");
#endif
        backend_ctx->program_mul_mv_f32_f32 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_mul_mat_f32_f32 = clCreateKernel(backend_ctx->program_mul_mv_f32_f32, "kernel_mul_mat_f32_f32", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "mul.cl.h"
        };
#else
        const std::string kernel_src = read_file("mul.cl");
#endif
        backend_ctx->program_mul =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_mul     = clCreateKernel(backend_ctx->program_mul, "kernel_mul", &err), err));
        CL_CHECK((backend_ctx->kernel_mul_row = clCreateKernel(backend_ctx->program_mul, "kernel_mul_row", &err), err));
        GGML_LOG_CONT(".");
    }

    // norm
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "norm.cl.h"
        };
#else
        const std::string kernel_src = read_file("norm.cl");
#endif
        backend_ctx->program_norm =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_norm = clCreateKernel(backend_ctx->program_norm, "kernel_norm", &err), err));
        GGML_LOG_CONT(".");
    }

    // relu
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "relu.cl.h"
        };
#else
        const std::string kernel_src = read_file("relu.cl");
#endif
        backend_ctx->program_relu =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_relu = clCreateKernel(backend_ctx->program_relu, "kernel_relu", &err), err));
        GGML_LOG_CONT(".");
    }

    // rms_norm
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "rms_norm.cl.h"
        };
#else
        const std::string kernel_src = read_file("rms_norm.cl");
#endif
        backend_ctx->program_rms_norm =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_rms_norm = clCreateKernel(backend_ctx->program_rms_norm, "kernel_rms_norm", &err), err));
        GGML_LOG_CONT(".");
    }

    // rope
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "rope.cl.h"
        };
#else
        const std::string kernel_src = read_file("rope.cl");
#endif
        backend_ctx->program_rope =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_rope_norm_f32   = clCreateKernel(backend_ctx->program_rope, "kernel_rope_norm_f32", &err), err));
        CL_CHECK((backend_ctx->kernel_rope_norm_f16   = clCreateKernel(backend_ctx->program_rope, "kernel_rope_norm_f16", &err), err));
        CL_CHECK((backend_ctx->kernel_rope_neox_f32   = clCreateKernel(backend_ctx->program_rope, "kernel_rope_neox_f32", &err), err));
        CL_CHECK((backend_ctx->kernel_rope_neox_f16   = clCreateKernel(backend_ctx->program_rope, "kernel_rope_neox_f16", &err), err));
        CL_CHECK((backend_ctx->kernel_rope_multi_f32  = clCreateKernel(backend_ctx->program_rope, "kernel_rope_multi_f32", &err), err));
        CL_CHECK((backend_ctx->kernel_rope_multi_f16  = clCreateKernel(backend_ctx->program_rope, "kernel_rope_multi_f16", &err), err));
        CL_CHECK((backend_ctx->kernel_rope_vision_f32 = clCreateKernel(backend_ctx->program_rope, "kernel_rope_vision_f32", &err), err));
        CL_CHECK((backend_ctx->kernel_rope_vision_f16 = clCreateKernel(backend_ctx->program_rope, "kernel_rope_vision_f16", &err), err));
        GGML_LOG_CONT(".");
    }

    // scale
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "scale.cl.h"
        };
#else
        const std::string kernel_src = read_file("scale.cl");
#endif
        backend_ctx->program_scale =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_scale = clCreateKernel(backend_ctx->program_scale, "kernel_scale", &err), err));
        GGML_LOG_CONT(".");
    }

    // silu
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "silu.cl.h"
        };
#else
        const std::string kernel_src = read_file("silu.cl");
#endif
        backend_ctx->program_silu =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_silu   = clCreateKernel(backend_ctx->program_silu, "kernel_silu", &err), err));
        CL_CHECK((backend_ctx->kernel_silu_4 = clCreateKernel(backend_ctx->program_silu, "kernel_silu_4", &err), err));
        GGML_LOG_CONT(".");
    }

    // softmax_f32
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "softmax_f32.cl.h"
        };
#else
        const std::string kernel_src = read_file("softmax_f32.cl");
#endif
        backend_ctx->program_softmax_f32 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_soft_max = clCreateKernel(backend_ctx->program_softmax_f32, "kernel_soft_max", &err), err));
        GGML_LOG_CONT(".");
    }

    // softmax_f16
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "softmax_f16.cl.h"
        };
#else
        const std::string kernel_src = read_file("softmax_f16.cl");
#endif
        backend_ctx->program_softmax_f16 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_soft_max_f16 = clCreateKernel(backend_ctx->program_softmax_f16, "kernel_soft_max_f16", &err), err));
        GGML_LOG_CONT(".");
    }

    // softmax_4_f32
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "softmax_4_f32.cl.h"
        };
#else
        const std::string kernel_src = read_file("softmax_4_f32.cl");
#endif
        backend_ctx->program_softmax_4_f32 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_soft_max_4 = clCreateKernel(backend_ctx->program_softmax_4_f32, "kernel_soft_max_4", &err), err));
        GGML_LOG_CONT(".");
    }

    // softmax_4_f16
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "softmax_4_f16.cl.h"
        };
#else
        const std::string kernel_src = read_file("softmax_4_f16.cl");
#endif
        backend_ctx->program_softmax_4_f16 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_soft_max_4_f16 = clCreateKernel(backend_ctx->program_softmax_4_f16, "kernel_soft_max_4_f16", &err), err));
        GGML_LOG_CONT(".");
    }

    // argsort
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "argsort.cl.h"
        };
#else
        const std::string kernel_src = read_file("argsort.cl");
#endif
        backend_ctx->program_argsort_f32_i32 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_argsort_f32_i32 = clCreateKernel(backend_ctx->program_argsort_f32_i32, "kernel_argsort_f32_i32", &err), err));
        GGML_LOG_CONT(".");
    }

    // div
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "div.cl.h"
        };
#else
        const std::string kernel_src = read_file("div.cl");
#endif
        backend_ctx->program_div =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_div     = clCreateKernel(backend_ctx->program_div, "kernel_div", &err), err));
        CL_CHECK((backend_ctx->kernel_div_row = clCreateKernel(backend_ctx->program_div, "kernel_div_row", &err), err));
        GGML_LOG_CONT(".");
    }

    // sub
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "sub.cl.h"
        };
#else
        const std::string kernel_src = read_file("sub.cl");
#endif
        backend_ctx->program_sub =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_sub     = clCreateKernel(backend_ctx->program_sub, "kernel_sub", &err), err));
        CL_CHECK((backend_ctx->kernel_sub_row = clCreateKernel(backend_ctx->program_sub, "kernel_sub_row", &err), err));
        GGML_LOG_CONT(".");
    }

    // sum_rows
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "sum_rows.cl.h"
        };
#else
        const std::string kernel_src = read_file("sum_rows.cl");
#endif
        backend_ctx->program_sum_rows_f32 =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_sum_rows_f32 = clCreateKernel(backend_ctx->program_sum_rows_f32, "kernel_sum_rows_f32", &err), err));
        GGML_LOG_CONT(".");
    }

    // sigmoid
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "sigmoid.cl.h"
        };
#else
        const std::string kernel_src = read_file("sigmoid.cl");
#endif
        backend_ctx->program_sigmoid =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_sigmoid_f32 = clCreateKernel(backend_ctx->program_sigmoid, "kernel_sigmoid_f32", &err), err));
        CL_CHECK((backend_ctx->kernel_sigmoid_f16 = clCreateKernel(backend_ctx->program_sigmoid, "kernel_sigmoid_f16", &err), err));
        GGML_LOG_CONT(".");
    }

    // group_norm
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "group_norm.cl.h"
        };
#else
        const std::string kernel_src = read_file("group_norm.cl");
#endif
        backend_ctx->program_group_norm =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_group_norm = clCreateKernel(backend_ctx->program_group_norm, "kernel_group_norm", &err), err));
        GGML_LOG_CONT(".");
    }

        // repeat
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "repeat.cl.h"
        };
#else
        const std::string kernel_src = read_file("repeat.cl");
#endif
        if (!kernel_src.empty()) {
            backend_ctx->program_repeat =
                build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);
            CL_CHECK((backend_ctx->kernel_repeat = clCreateKernel(backend_ctx->program_repeat, "kernel_repeat", &err), err));
            GGML_LOG_CONT(".");
        } else {
            GGML_LOG_WARN("ggml_opencl: repeat kernel source not found or empty. Repeat operations will not be available.\n");
            backend_ctx->program_repeat = nullptr;
            backend_ctx->kernel_repeat = nullptr;
        }
    }

    // pad
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "pad.cl.h"
        };
#else
        const std::string kernel_src = read_file("pad.cl");
#endif
        if (!kernel_src.empty()) {
            backend_ctx->program_pad =
                build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);
            CL_CHECK((backend_ctx->kernel_pad = clCreateKernel(backend_ctx->program_pad, "kernel_pad", &err), err));
            GGML_LOG_CONT(".");
        } else {
            GGML_LOG_WARN("ggml_opencl: pad kernel source not found or empty. Pad operations will not be available.\n");
            backend_ctx->program_pad = nullptr;
            backend_ctx->kernel_pad = nullptr;
        }
    }

    // tanh
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "tanh.cl.h"
        };
#else
        const std::string kernel_src = read_file("tanh.cl");
#endif
        if (!kernel_src.empty()) {
            backend_ctx->program_tanh =
                build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);
            CL_CHECK((backend_ctx->kernel_tanh_f32_nd = clCreateKernel(backend_ctx->program_tanh, "kernel_tanh_f32_nd", &err), err));
            CL_CHECK((backend_ctx->kernel_tanh_f16_nd = clCreateKernel(backend_ctx->program_tanh, "kernel_tanh_f16_nd", &err), err));
            GGML_LOG_CONT(".");
        } else {
            GGML_LOG_WARN("ggml_opencl: tanh kernel source not found or empty. Tanh operation will not be available.\n");
            backend_ctx->program_tanh = nullptr;
            backend_ctx->kernel_tanh_f32_nd = nullptr;
            backend_ctx->kernel_tanh_f16_nd = nullptr;
        }
    }

    // upscale
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "upscale.cl.h"
        };
#else
        const std::string kernel_src = read_file("upscale.cl");
#endif
        if (!kernel_src.empty()) {
            backend_ctx->program_upscale =
                build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);
            CL_CHECK((backend_ctx->kernel_upscale = clCreateKernel(backend_ctx->program_upscale, "kernel_upscale", &err), err));
            if (backend_ctx->program_upscale) {
                 cl_int err_bilinear;
                 backend_ctx->kernel_upscale_bilinear = clCreateKernel(backend_ctx->program_upscale, "kernel_upscale_bilinear", &err_bilinear);
                 if (err_bilinear != CL_SUCCESS) {
                    GGML_LOG_WARN("ggml_opencl: kernel_upscale_bilinear not found in upscale.cl. Bilinear upscale will not be available. Error: %d\n", err_bilinear);
                    backend_ctx->kernel_upscale_bilinear = nullptr;
                 }
            } else {
                backend_ctx->kernel_upscale_bilinear = nullptr;
            }
            GGML_LOG_CONT(".");
        } else {
            GGML_LOG_WARN("ggml_opencl: upscale kernel source not found or empty. Upscale operations will not be available.\n");
            backend_ctx->program_upscale = nullptr;
            backend_ctx->kernel_upscale = nullptr;
            backend_ctx->kernel_upscale_bilinear = nullptr;
        }
    }

    // concat
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "concat.cl.h"
        };
#else

        const std::string kernel_src = read_file("concat.cl");
#endif
        if (!kernel_src.empty()) {
            backend_ctx->program_concat =
                build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

            CL_CHECK((backend_ctx->kernel_concat_f32_contiguous = clCreateKernel(backend_ctx->program_concat, "kernel_concat_f32_contiguous", &err), err));
            CL_CHECK((backend_ctx->kernel_concat_f32_non_contiguous = clCreateKernel(backend_ctx->program_concat, "kernel_concat_f32_non_contiguous", &err), err));
            GGML_LOG_CONT(".");
        } else {
            GGML_LOG_WARN("ggml_opencl: concat kernel source not found or empty. Concat operations will not be available.\n");
            backend_ctx->program_concat = nullptr;
            backend_ctx->kernel_concat_f32_contiguous = nullptr;
            backend_ctx->kernel_concat_f32_non_contiguous = nullptr;
        }
    }

    // timestep_embedding
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "tsembd.cl.h"
        };
#else

        const std::string kernel_src = read_file("tsembd.cl");
#endif
        if (!kernel_src.empty()) {
            backend_ctx->program_tsembd =
                build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);
            CL_CHECK((backend_ctx->kernel_timestep_embedding = clCreateKernel(backend_ctx->program_tsembd, "kernel_timestep_embedding", &err), err));
            GGML_LOG_CONT(".");
        } else {
            GGML_LOG_WARN("ggml_opencl: timestep_embedding kernel source not found or empty. This op will not be available.\n");
            backend_ctx->program_tsembd = nullptr;
            backend_ctx->kernel_timestep_embedding = nullptr;
        }
    }

    // Adreno kernels
#ifdef GGML_OPENCL_USE_ADRENO_KERNELS
    // transpose
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src {
            #include "transpose.cl.h"
        };
#else
        const std::string kernel_src = read_file("transpose.cl");
#endif
        backend_ctx->program_transpose =
            build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src.c_str(), compile_opts);

        CL_CHECK((backend_ctx->kernel_transpose_32_16 = clCreateKernel(backend_ctx->program_transpose, "kernel_transpose_32_16", &err), err));
        CL_CHECK((backend_ctx->kernel_transpose_32    = clCreateKernel(backend_ctx->program_transpose, "kernel_transpose_32", &err), err));
        CL_CHECK((backend_ctx->kernel_transpose_16    = clCreateKernel(backend_ctx->program_transpose, "kernel_transpose_16", &err), err));
        GGML_LOG_CONT(".");
    }

    // gemv_noshuffle_general
    {
        std::string CL_gemv_compile_opts = std::string("-cl-std=") + opencl_c_std +
                                       " -cl-mad-enable "
                                       " -DSIMDGROUP_WIDTH=" +
                                       std::to_string(backend_ctx->adreno_wave_size);
        if (backend_ctx->has_vector_subgroup_broadcast) {
            CL_gemv_compile_opts += " -DVECTOR_SUB_GROUP_BROADCAT ";
        }

#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src_CL_gemv_general {
            #include "gemv_noshuffle_general.cl.h"
        };
#else
        const std::string kernel_src_CL_gemv_general = read_file("gemv_noshuffle_general.cl");
#endif

        backend_ctx->program_CL_gemv_general = build_program_from_source(
            backend_ctx->context, backend_ctx->device, kernel_src_CL_gemv_general.c_str(), CL_gemv_compile_opts);

        CL_CHECK((backend_ctx->CL_mul_mat_vec_q4_0_f32_1d_4x_flat_general = clCreateKernel(backend_ctx->program_CL_gemv_general, "kernel_gemv_noshuffle", &err), err));
        GGML_LOG_CONT(".");
    }

    // gemv_noshuffle
    {
        // Gemv 2048, 16384
        std::string CL_gemv_compile_opts = std::string("-cl-std=") + opencl_c_std +
            " -cl-mad-enable "
            " -DLINE_STRIDE_A=2048 "
            " -DBLOCK_STRIDE_A=16384 "
            " -DSIMDGROUP_WIDTH=" +
            std::to_string(backend_ctx->adreno_wave_size);
        if (backend_ctx->has_vector_subgroup_broadcast) {
            CL_gemv_compile_opts += " -DVECTOR_SUB_GROUP_BROADCAT ";
        }

#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src_CL_gemv {
            #include "gemv_noshuffle.cl.h"
        };
#else
        const std::string kernel_src_CL_gemv = read_file("gemv_noshuffle.cl");
#endif

        backend_ctx->program_CL_gemv_4096_1_4096 = build_program_from_source(
            backend_ctx->context, backend_ctx->device, kernel_src_CL_gemv.c_str(), CL_gemv_compile_opts);
        CL_CHECK((backend_ctx->CL_mul_mat_vec_q4_0_f32_1d_4x_flat_4096_1_4096 = clCreateKernel(backend_ctx->program_CL_gemv_4096_1_4096, "kernel_gemv_noshuffle", &err), err));
        GGML_LOG_CONT(".");

        // Gemv 2048, 16384
        CL_gemv_compile_opts = std::string("-cl-std=") + opencl_c_std +
            " -cl-mad-enable "
            " -DLINE_STRIDE_A=2048 "
            " -DBLOCK_STRIDE_A=16384 "
            " -DSIMDGROUP_WIDTH=" +
            std::to_string(backend_ctx->adreno_wave_size);
        if (backend_ctx->has_vector_subgroup_broadcast) {
            CL_gemv_compile_opts += " -DVECTOR_SUB_GROUP_BROADCAT ";
        }

        backend_ctx->program_CL_gemv_4096_1_11008 = build_program_from_source(
            backend_ctx->context, backend_ctx->device, kernel_src_CL_gemv.c_str(), CL_gemv_compile_opts);
        CL_CHECK((backend_ctx->CL_mul_mat_vec_q4_0_f32_1d_4x_flat_4096_1_11008 = clCreateKernel(backend_ctx->program_CL_gemv_4096_1_11008, "kernel_gemv_noshuffle", &err), err));
        GGML_LOG_CONT(".");

        // Gemv 5504, 44032
        CL_gemv_compile_opts = std::string("-cl-std=") + opencl_c_std +
            " -cl-mad-enable "
            " -DLINE_STRIDE_A=5504 "
            " -DBLOCK_STRIDE_A=44032 "
            " -DSIMDGROUP_WIDTH=" +
            std::to_string(backend_ctx->adreno_wave_size);
        if (backend_ctx->has_vector_subgroup_broadcast) {
            CL_gemv_compile_opts += " -DVECTOR_SUB_GROUP_BROADCAT ";
        }

        backend_ctx->program_CL_gemv_11008_1_4096 = build_program_from_source(
            backend_ctx->context, backend_ctx->device, kernel_src_CL_gemv.c_str(), CL_gemv_compile_opts);
        CL_CHECK((backend_ctx->CL_mul_mat_vec_q4_0_f32_1d_4x_flat_11008_1_4096 = clCreateKernel(backend_ctx->program_CL_gemv_11008_1_4096, "kernel_gemv_noshuffle", &err), err));
        GGML_LOG_CONT(".");

        // Gemv 16000, 128000
        CL_gemv_compile_opts = std::string("-cl-std=") + opencl_c_std +
            " -cl-mad-enable "
            " -DLINE_STRIDE_A=16000 "
            " -DBLOCK_STRIDE_A=128000 "
            " -DSIMDGROUP_WIDTH=" +
            std::to_string(backend_ctx->adreno_wave_size);

        if (backend_ctx->has_vector_subgroup_broadcast) {
            CL_gemv_compile_opts += " -DVECTOR_SUB_GROUP_BROADCAT ";
        }

        backend_ctx->program_CL_gemv_32000_1_4096 = build_program_from_source(
            backend_ctx->context, backend_ctx->device, kernel_src_CL_gemv.c_str(), CL_gemv_compile_opts);
        CL_CHECK((backend_ctx->CL_mul_mat_vec_q4_0_f32_1d_4x_flat_32000_1_4096 = clCreateKernel(backend_ctx->program_CL_gemv_32000_1_4096, "kernel_gemv_noshuffle", &err), err));
        GGML_LOG_CONT(".");
    }

    // mul_mat_Ab_Bi_8x4
    {
#ifdef GGML_OPENCL_EMBED_KERNELS
        const std::string kernel_src_CL_gemm {
            #include "mul_mat_Ab_Bi_8x4.cl.h"
        };
#else
        const std::string kernel_src_CL_gemm = read_file("mul_mat_Ab_Bi_8x4.cl");
#endif
        backend_ctx->program_CL_gemm = build_program_from_source(backend_ctx->context, backend_ctx->device, kernel_src_CL_gemm.c_str(), compile_opts);
        CL_CHECK((backend_ctx->CL_mul_mat_Ab_Bi_8x4 = clCreateKernel(backend_ctx->program_CL_gemm, "kernel_mul_mat_Ab_Bi_8x4", &err), err));
        GGML_LOG_CONT(".");
    }
#endif // GGML_OPENCL_USE_ADRENO_KERNELS
    GGML_LOG_CONT("\n");
}

// XXX static ggml_backend_opencl_context * ggml_cl2_init(ggml_backend_dev_t dev) {
// XXX    static bool initialized = false;
// XXX    static ggml_backend_opencl_context *backend_ctx = nullptr;

static ggml_backend_opencl_context * ggml_cl2_init(ggml_backend_dev_t dev);

namespace /* anonymous */ {
extern struct ggml_backend_device_i ggml_backend_opencl_device_i;
}

// Look for available and suitable devices.
static std::vector<ggml_backend_device> ggml_opencl_probe_devices(ggml_backend_reg * reg) {
    std::vector<ggml_backend_device> found_devices;

#ifdef GGML_OPENCL_PROFILING
    GGML_LOG_INFO("ggml_opencl: OpenCL profiling enabled\n");
#endif

    struct cl_device;
    struct cl_platform {
        cl_platform_id id;
        unsigned number;
        char name[128];
        char vendor[128];
        struct cl_device * devices;
        unsigned n_devices;
        struct cl_device * default_device;
    };

    struct cl_device {
        struct cl_platform * platform;
        cl_device_id id;
        unsigned number;
        cl_device_type type;
        char name[128];
        char version[128];
    };

    enum { NPLAT = 16, NDEV = 16 };

    struct cl_platform platforms[NPLAT];
    unsigned n_platforms = 0;
    struct cl_device devices[NDEV];
    unsigned n_devices = 0;
    struct cl_device * default_device = NULL;
    unsigned           default_platform_number = 0;

    cl_platform_id platform_ids[NPLAT];
    if (clGetPlatformIDs(NPLAT, platform_ids, &n_platforms) != CL_SUCCESS) {
        GGML_LOG_ERROR("ggml_opencl: plaform IDs not available.\n");
        return found_devices;
    }

    for (unsigned i = 0; i < n_platforms; i++) {
        struct cl_platform * p = &platforms[i];
        p->number = i;
        p->id = platform_ids[i];
        CL_CHECK(clGetPlatformInfo(p->id, CL_PLATFORM_NAME, sizeof(p->name), &p->name, NULL));
        CL_CHECK(clGetPlatformInfo(p->id, CL_PLATFORM_VENDOR, sizeof(p->vendor), &p->vendor, NULL));

        cl_device_id device_ids[NDEV];
        cl_int clGetDeviceIDsError = clGetDeviceIDs(p->id, CL_DEVICE_TYPE_ALL, NDEV, device_ids, &p->n_devices);
        if (clGetDeviceIDsError == CL_DEVICE_NOT_FOUND) {
            p->n_devices = 0;
        } else {
            CL_CHECK(clGetDeviceIDsError);
        }
        p->devices = p->n_devices > 0 ? &devices[n_devices] : NULL;
        p->default_device = NULL;

        for (unsigned j = 0; j < p->n_devices; j++) {
            struct cl_device * d = &devices[n_devices];
            d->number = n_devices++;
            d->id = device_ids[j];
            d->platform = p;
            CL_CHECK(clGetDeviceInfo(d->id, CL_DEVICE_NAME, sizeof(d->name), &d->name, NULL));
            CL_CHECK(clGetDeviceInfo(d->id, CL_DEVICE_TYPE, sizeof(d->type), &d->type, NULL));
            CL_CHECK(clGetDeviceInfo(d->id, CL_DEVICE_VERSION, sizeof(d->version), &d->version, NULL));

            if (p->default_device == NULL && d->type == CL_DEVICE_TYPE_GPU) {
                p->default_device = d;
            }
        }

        if (default_device == NULL && p->default_device != NULL) {
            default_device          = p->default_device;
            default_platform_number = i;
        }
    }

    if (n_devices == 0) {
        GGML_LOG_ERROR("ggml_opencl: could find any OpenCL devices.\n");
        return found_devices;
    }

    char *      user_platform_string = getenv("GGML_OPENCL_PLATFORM");
    char *      user_device_string   = getenv("GGML_OPENCL_DEVICE");
    int         user_platform_number = -1;
    int         user_device_number   = -1;
    cl_device * candidate_devices    = nullptr;
    unsigned    n_candidate_devices  = 0;

    unsigned n;
    if (user_platform_string != NULL && sscanf(user_platform_string, " %u", &n) == 1 && n < n_platforms) {
        user_platform_number = (int)n;
    }
    if (user_device_string != NULL && sscanf(user_device_string, " %u", &n) == 1 && n < n_devices) {
        user_device_number = (int)n;
    }
    if (user_platform_number != -1 && user_device_number != -1) {
        cl_platform* platform = &platforms[user_platform_number];
        if ((unsigned)user_device_number >= platform->n_devices) {
            GGML_LOG_ERROR("ggml_opencl: invalid device number %d\n", user_device_number);
            exit(1);
        }
        default_device      = &platform->devices[user_device_number];
        candidate_devices   = platform->devices;
        n_candidate_devices = platform->n_devices;
    } else {
        // Choose a platform by matching a substring.
        if (user_platform_number == -1 && user_platform_string != NULL && user_platform_string[0] != 0) {
            for (unsigned i = 0; i < n_platforms; i++) {
                struct cl_platform * p = &platforms[i];
                if (strstr(p->name, user_platform_string) != NULL ||
                    strstr(p->vendor, user_platform_string) != NULL) {
                    user_platform_number = (int)i;
                    break;
                }
            }
            if (user_platform_number == -1) {
                GGML_LOG_ERROR("ggml_opencl: no platform matching '%s' was found.\n", user_platform_string);
                exit(1);
            }
        }

        int                  platform_idx = user_platform_number != -1 ? user_platform_number : default_platform_number;
        struct cl_platform * p            = &platforms[platform_idx];
        candidate_devices                 = p->devices;
        n_candidate_devices               = p->n_devices;
        default_device                    = p->default_device;
        if (n_candidate_devices == 0) {
            GGML_LOG_ERROR("ggml_opencl: selected platform '%s' does not have any devices.\n", p->name);
            exit(1);
        }

        if (user_device_number == -1 && user_device_string != NULL && user_device_string[0] != 0) {
            for (unsigned i = 0; i < n_candidate_devices; i++) {
                struct cl_device * d = &candidate_devices[i];
                if (strstr(d->name, user_device_string) != NULL) {
                    user_device_number = d->number;
                    break;
                }
            }
            if (user_device_number == -1) {
                GGML_LOG_ERROR("ggml_opencl: no device matching '%s' was found.\n", user_device_string);
                exit(1);
            }
        }
        if (user_device_number != -1) {
            candidate_devices   = &devices[user_device_number];
            n_candidate_devices = 1;
            default_device      = &candidate_devices[0];
        }

        GGML_ASSERT(n_candidate_devices > 0);

        if (default_device == NULL) {
            default_device = &candidate_devices[0];
        }
    }

    GGML_ASSERT(n_candidate_devices != 0 && candidate_devices);

    // Put the default device in front.
    for (unsigned i = 1; i < n_candidate_devices; i++) {
        if (&candidate_devices[i] == default_device) {
            std::swap(candidate_devices[0], candidate_devices[i]);
            default_device = &candidate_devices[0];
            break;
        }
    }

    GGML_LOG_INFO("ggml_opencl: selected platform: '%s'\n", default_device->platform->name);

    std::vector<cl_device_id> device_ids;
    for (auto dev = candidate_devices, dev_end = candidate_devices + n_candidate_devices; dev != dev_end; dev++) {
        device_ids.push_back(dev->id);
    }

    cl_int                err;
    cl_context            shared_context;
    cl_context_properties properties[] = { (intptr_t) CL_CONTEXT_PLATFORM, (intptr_t) default_device->platform->id, 0 };

    CL_CHECK(
        (shared_context = clCreateContext(properties, device_ids.size(), device_ids.data(), NULL, NULL, &err), err));

    for (auto dev = candidate_devices, dev_end = candidate_devices + n_candidate_devices; dev != dev_end; dev++) {
        GGML_LOG_INFO("\nggml_opencl: device: '%s (%s)'\n", dev->name, dev->version);

        auto dev_ctx = std::unique_ptr<ggml_backend_opencl_device_context>(new ggml_backend_opencl_device_context{
            /*.platform         =*/dev->platform->id,
            /*.platform_nane    =*/dev->platform->name,
            /*.device           =*/dev->id,
            /*.device_name      =*/dev->name,
            /*.device_type      =*/dev->type,
            /*.device_version   =*/dev->version,
            /*.backend_ctx      =*/nullptr,
            /*.buffer_type      =*/{},
            /*.context          =*/shared_context,
        });

        found_devices.push_back(ggml_backend_device{
            /* .iface   = */ ggml_backend_opencl_device_i,
            /* .reg     = */ reg,
            /* .context = */ dev_ctx.get(),
        });

        if (!ggml_cl2_init(&found_devices.back())) {
            found_devices.pop_back();
            GGML_LOG_INFO("ggml_opencl: drop unsupported device.\n");
            continue;
        }

        dev_ctx.release();
    }

    if (found_devices.size()) {
        auto * dev_ctx = static_cast<ggml_backend_opencl_device_context *>(found_devices.front().context);
        GGML_LOG_INFO("ggml_opencl: default device: '%s (%s)'\n", dev_ctx->device_name.c_str(),
                      dev_ctx->device_version.c_str());

        if (dev_ctx->device_type != CL_DEVICE_TYPE_GPU) {
            GGML_LOG_WARN("ggml_opencl: warning, the default device is not a GPU: '%s'.\n",
                          dev_ctx->device_name.c_str());
        }
    }

    return found_devices;
}

// Initialize device if it is supported (returns nullptr if it is not).
static ggml_backend_opencl_context * ggml_cl2_init(ggml_backend_dev_t dev) {
    GGML_ASSERT(dev);
    GGML_ASSERT(dev->context);

    ggml_backend_opencl_device_context * dev_ctx = (ggml_backend_opencl_device_context *) dev->context;
    GGML_ASSERT(dev_ctx->platform);
    GGML_ASSERT(dev_ctx->device);

    if (dev_ctx->backend_ctx) {
        return dev_ctx->backend_ctx;
    }

    auto backend_ctx        = std::make_unique<ggml_backend_opencl_context>();
    backend_ctx->device     = dev_ctx->device;
    backend_ctx->gpu_family = GPU_FAMILY::UNKNOWN;

    if (strstr(dev_ctx->device_name.c_str(), "Adreno") ||
        strstr(dev_ctx->device_name.c_str(), "Qualcomm") ||
        strstr(dev_ctx->device_version.c_str(), "Adreno")) {
        backend_ctx->gpu_family = GPU_FAMILY::ADRENO;
        // Usually device version contains the detailed device name
        backend_ctx->adreno_gen = get_adreno_gpu_gen(dev_ctx->device_version.c_str());
        if (backend_ctx->adreno_gen == ADRENO_GPU_GEN::ADRENO_UNKNOWN) {
            backend_ctx->adreno_gen = get_adreno_gpu_gen(dev_ctx->device_name.c_str());
        }

        // Use wave size of 64 for all Adreno GPUs.
        backend_ctx->adreno_wave_size = 64;
    } else if (strstr(dev_ctx->device_name.c_str(), "Intel")) {
        backend_ctx->gpu_family = GPU_FAMILY::INTEL;
    } else {
        GGML_LOG_ERROR("Unsupported GPU: %s\n", dev_ctx->device_name.c_str());
        backend_ctx->gpu_family = GPU_FAMILY::UNKNOWN;
        return nullptr;
    }

#ifdef GGML_OPENCL_USE_ADRENO_KERNELS
    if (backend_ctx->gpu_family != GPU_FAMILY::ADRENO) {
        GGML_LOG_ERROR("ggml_opencl: Adreno-specific kernels should not be enabled for non-Adreno GPUs; "
            "run on an Adreno GPU or recompile with CMake option `-DGGML_OPENCL_USE_ADRENO_KERNELS=OFF`\n");
        return nullptr;
    }
#endif

    // Populate backend device name
    backend_ctx->device_name = dev_ctx->device_name;

    // A local ref of cl_device_id for convenience
    cl_device_id device = backend_ctx->device;

    ggml_cl_version platform_version = get_opencl_platform_version(dev_ctx->platform);

    // Check device OpenCL version, OpenCL 2.0 or above is required
    ggml_cl_version opencl_c_version = get_opencl_c_version(platform_version, device);
    if (opencl_c_version.major < 2) {
        GGML_LOG_ERROR("ggml_opencl: OpenCL 2.0 or above is required\n");
        return nullptr;
    }

    // Check driver version
    size_t driver_version_str_size;
    clGetDeviceInfo(device, CL_DRIVER_VERSION, 0, NULL, &driver_version_str_size);
    char *driver_version = (char *)alloca(driver_version_str_size + 1);
    clGetDeviceInfo(device, CL_DRIVER_VERSION, driver_version_str_size, driver_version, NULL);
    driver_version[driver_version_str_size] = '\0';
    GGML_LOG_INFO("ggml_opencl: OpenCL driver: %s\n", driver_version);
    backend_ctx->driver_version = driver_version;

    backend_ctx->adreno_cl_compiler_version = get_adreno_cl_compiler_version(driver_version);
    backend_ctx->has_vector_subgroup_broadcast =
        backend_ctx->adreno_cl_compiler_version.major >= 47 ||
        backend_ctx->adreno_cl_compiler_version.major == 17;
    GGML_LOG_INFO("ggml_opencl: vector subgroup broadcast support: %s\n",
        backend_ctx->has_vector_subgroup_broadcast ? "true" : "false");

    size_t ext_str_size;
    clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0, NULL, &ext_str_size);
    char *ext_buffer = (char *)alloca(ext_str_size + 1);
    clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, ext_str_size, ext_buffer, NULL);
    ext_buffer[ext_str_size] = '\0'; // ensure it is null terminated
    // Check if ext_buffer contains cl_khr_fp16
    backend_ctx->fp16_support = strstr(ext_buffer, "cl_khr_fp16") != NULL;
    GGML_LOG_INFO("ggml_opencl: device FP16 support: %s\n", backend_ctx->fp16_support ? "true" : "false");

    // fp16 is required
    if (!backend_ctx->fp16_support) {
        GGML_LOG_ERROR("ggml_opencl: device does not support FP16\n");
        return nullptr;
    }

    // If OpenCL 3.0 is supported, then check for cl_khr_subgroups, which becomes
    // optional in OpenCL 3.0 (cl_khr_subgroup is mandatory in OpenCL 2.x)
    if (opencl_c_version.major == 3 && strstr(ext_buffer, "cl_khr_subgroups") == NULL &&
        strstr(ext_buffer, "cl_intel_subgroups") == NULL) {
        GGML_LOG_ERROR("ggml_opencl: device does not support subgroups (cl_khr_subgroups or cl_intel_subgroups) "
            "(note that subgroups is an optional feature in OpenCL 3.0)\n");
        return nullptr;
    }

    cl_uint base_align_in_bits;
    CL_CHECK(clGetDeviceInfo(device, CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(cl_uint), &base_align_in_bits, NULL));
    GGML_ASSERT(base_align_in_bits % 8u == 0);
    backend_ctx->alignment = base_align_in_bits / 8u;
    GGML_LOG_INFO("ggml_opencl: mem base addr align: %u\n", backend_ctx->alignment);

    clGetDeviceInfo(device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(size_t), &backend_ctx->max_alloc_size, NULL);
    GGML_LOG_INFO("ggml_opencl: max mem alloc size: %zu MB\n", backend_ctx->max_alloc_size/1024/1024);

    // Check SVM.
    cl_device_svm_capabilities svm_caps;
    CL_CHECK(clGetDeviceInfo(device, CL_DEVICE_SVM_CAPABILITIES, sizeof(cl_device_svm_capabilities), &svm_caps, 0));
    GGML_LOG_INFO("ggml_opencl: SVM coarse grain buffer support: %s\n",
        svm_caps & CL_DEVICE_SVM_COARSE_GRAIN_BUFFER ? "true" : "false");
    GGML_LOG_INFO("ggml_opencl: SVM fine grain buffer support: %s\n",
        svm_caps & CL_DEVICE_SVM_FINE_GRAIN_BUFFER ? "true" : "false");
    GGML_LOG_INFO("ggml_opencl: SVM fine grain system support: %s\n",
        svm_caps & CL_DEVICE_SVM_FINE_GRAIN_SYSTEM ? "true" : "false");
    GGML_LOG_INFO("ggml_opencl: SVM atomics support: %s\n",
        svm_caps & CL_DEVICE_SVM_ATOMICS ? "true" : "false");

    if (opencl_c_version.major >= 3) {
        CL_CHECK(clGetDeviceInfo(device, CL_DEVICE_NON_UNIFORM_WORK_GROUP_SUPPORT, sizeof(cl_bool),
                                 &backend_ctx->non_uniform_workgroups, 0));
    } else {
        GGML_ASSERT(opencl_c_version.major == 2);
        // Non-uniform workgroup sizes is mandatory feature in v2.x.
        backend_ctx->non_uniform_workgroups = true;
    }

    // Print out configurations
#ifdef GGML_OPENCL_SOA_Q
    GGML_LOG_INFO("ggml_opencl: flattening quantized weights representation as struct of arrays (GGML_OPENCL_SOA_Q)\n");
#endif // GGML_OPENCL_SOA_Q

#ifdef GGML_OPENCL_USE_ADRENO_KERNELS
    GGML_LOG_INFO("ggml_opencl: using kernels optimized for Adreno (GGML_OPENCL_USE_ADRENO_KERNELS)\n");
#endif // GGML_OPENCL_USE_ADRENO_KERNELS

    cl_int err;

    // A local ref of cl_context for convenience
    cl_context context = backend_ctx->context = dev_ctx->context;

    //CL_CHECK((queue = clCreateCommandQueue(context, device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err),
    //    (err != CL_INVALID_QUEUE_PROPERTIES && err != CL_INVALID_VALUE ? err :
    //    (queue = clCreateCommandQueue(context, device, 0, &err), err)
    //)));
    cl_command_queue_properties command_queue_props = 0;
#ifdef GGML_OPENCL_PROFILING
    command_queue_props |= CL_QUEUE_PROFILING_ENABLE;
#endif
    CL_CHECK((backend_ctx->queue = clCreateCommandQueue(context, device, command_queue_props, &err), err));

    // Load kernels
    load_cl_kernels(backend_ctx.get(), opencl_c_version);

#ifdef GGML_OPENCL_USE_ADRENO_KERNELS
    // Allocate intermediate buffers and images
    size_t required_A_q_d_bytes = 311164928;
    size_t required_A_s_d_bytes = 38895616;
    size_t required_B_d_bytes = 45088768;

    // Ensure buffer sizes do not exceed the maximum allocation size
    size_t max_A_q_d_bytes = MIN(required_A_q_d_bytes, backend_ctx->max_alloc_size);
    size_t max_A_s_d_bytes = MIN(required_A_s_d_bytes, backend_ctx->max_alloc_size);
    size_t max_B_d_bytes   = MIN(required_B_d_bytes, backend_ctx->max_alloc_size);
    if (required_A_q_d_bytes > backend_ctx->max_alloc_size) {
        GGML_LOG_WARN("ggml_opencl: A_q_d buffer size reduced from %zu to %zu due to device limitations.\n",
                      required_A_q_d_bytes, max_A_q_d_bytes);
    }
    if (required_A_s_d_bytes > backend_ctx->max_alloc_size) {
        GGML_LOG_WARN("ggml_opencl: A_s_d buffer size reduced from %zu to %zu due to device limitations.\n",
                      required_A_s_d_bytes, max_A_s_d_bytes);
    }
    if (required_B_d_bytes > backend_ctx->max_alloc_size) {
        GGML_LOG_WARN("ggml_opencl: B_d buffer size reduced from %zu to %zu due to device limitations.\n",
                      required_B_d_bytes, max_B_d_bytes);
    }

    CL_CHECK((backend_ctx->A_q_d_max = clCreateBuffer(context, 0, max_A_q_d_bytes, NULL, &err), err));
    CL_CHECK((backend_ctx->A_s_d_max = clCreateBuffer(context, 0, max_A_s_d_bytes, NULL, &err), err));
    CL_CHECK((backend_ctx->B_d_max   = clCreateBuffer(context, 0, max_B_d_bytes,   NULL, &err), err));
#endif // GGML_OPENCL_USE_ADRENO_KERNELS

    dev_ctx->backend_ctx = backend_ctx.release();
    return dev_ctx->backend_ctx;
}

static void ggml_cl2_free(void) {
#ifdef GGML_OPENCL_PROFILING
    FILE * fperf = fopen("cl_profiling.csv", "w");
    if (!fperf) {
        GGML_LOG_ERROR("Failed to open cl_profiling.csv\n");
        return;
    }

    // Populate profiling info
    for (ProfilingInfo & info : g_profiling_info) {
        cl_ulong cmd_queued;
        cl_ulong cmd_submit;
        cl_ulong cmd_start;
        cl_ulong cmd_end;
        cl_ulong cmd_complete;

        CL_CHECK(clWaitForEvents(1, &info.evt));
        CL_CHECK(clGetEventProfilingInfo(
            info.evt, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &cmd_queued, NULL));
        CL_CHECK(clGetEventProfilingInfo(
            info.evt, CL_PROFILING_COMMAND_SUBMIT, sizeof(cl_ulong), &cmd_submit, NULL));
        CL_CHECK(clGetEventProfilingInfo(
            info.evt, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &cmd_start, NULL));
        CL_CHECK(clGetEventProfilingInfo(
            info.evt, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &cmd_end, NULL));
        CL_CHECK(clGetEventProfilingInfo(
            info.evt, CL_PROFILING_COMMAND_COMPLETE, sizeof(cl_ulong), &cmd_complete, NULL));
        CL_CHECK(clReleaseEvent(info.evt));

        char kernel_name[512];
        CL_CHECK(clGetKernelInfo(info.kernel, CL_KERNEL_FUNCTION_NAME,
            sizeof(kernel_name), kernel_name, NULL));
        info.kernel_name = kernel_name;

        info.cmd_queued = cmd_queued;
        info.cmd_submit = cmd_submit;
        info.cmd_start  = cmd_start;
        info.cmd_end    = cmd_end;

        info.cmd_queued_duration_ns     = cmd_submit    - cmd_queued;
        info.cmd_submit_duration_ns     = cmd_start     - cmd_submit;
        info.cmd_duration_ns            = cmd_end       - cmd_start;
        info.cmd_complete_duration_ns   = cmd_complete  - cmd_end;
        info.cmd_total_duration_ns      = cmd_complete  - cmd_queued;
    }

    // Dump a csv
    float total_kernel_time = 0;
    fprintf(fperf, "op name, kernel name, queued duration (ms), submit duration(ms), exec duration (ms), complete duration (ms), total duration (ms), global size, local size, output size\n");
    for (const ProfilingInfo & info : g_profiling_info) {
        total_kernel_time += info.cmd_duration_ns/1.e6f;
        fprintf(fperf, "%s,%s,%f,%f,%f,%f,%f,%zux%zux%zu,%zux%zux%zu,%zux%zux%zux%zu\n",
            info.op_name.c_str(), info.kernel_name.c_str(),
            info.cmd_queued_duration_ns/1.e6f,
            info.cmd_submit_duration_ns/1.e6f,
            info.cmd_duration_ns/1.e6f,
            info.cmd_complete_duration_ns/1.e6f,
            info.cmd_total_duration_ns/1.e6f,
            info.global_size[0], info.global_size[1], info.global_size[2],
            info.local_size[0], info.local_size[1], info.local_size[2],
            info.output_size[0], info.output_size[1], info.output_size[2], info.output_size[3]);
    }
    fclose(fperf);

    GGML_LOG_INFO("ggml_opencl: total kernel time: %f\n", total_kernel_time);

    // Dump a simple chrome trace
    FILE* ftrace = fopen("cl_trace.json", "w");
    if (!ftrace) {
        GGML_LOG_ERROR("Failed to open cl_trace.json\n");
        return;
    }

    fprintf(ftrace, "[\n");
    for (const ProfilingInfo & info : g_profiling_info) {
        fprintf(ftrace, "{\"name\": \"%s\", \"cat\": \"OpenCL\", \"ph\": \"B\", \"ts\": %lu, \"pid\": \"\", \"tid\": \"Host\"},\n",
            info.kernel_name.c_str(), info.cmd_queued/1000);
        fprintf(ftrace, "{\"name\": \"%s\", \"cat\": \"OpenCL\", \"ph\": \"E\", \"ts\": %lu, \"pid\": \"\", \"tid\": \"Host\"},\n",
            info.kernel_name.c_str(), info.cmd_submit/1000);

        fprintf(ftrace, "{\"name\": \"%s\", \"cat\": \"OpenCL\", \"ph\": \"B\", \"ts\": %lu, \"pid\": \"\", \"tid\": \"Device\"},\n",
            info.kernel_name.c_str(), info.cmd_start/1000);
        fprintf(ftrace, "{\"name\": \"%s\", \"cat\": \"OpenCL\", \"ph\": \"E\", \"ts\": %lu, \"pid\": \"\", \"tid\": \"Device\"},\n",
            info.kernel_name.c_str(), info.cmd_end/1000);
    }
    fclose(ftrace);
#endif
}

//------------------------------------------------------------------------------
// Tensor extra management
//------------------------------------------------------------------------------
struct ggml_tensor_extra_cl {
    // The buffer object that holds the data.
    cl_mem data_device;
    // The offset into the buffer object. This is primarily for scratch buffer
    // and view operation.
    // NB: this offset no longer includes view offset (view_offs). Whenever this
    // offset is used, view_offs should be considered.
    cl_ulong offset;
    // The actual size of the cl_mem object. This is needed when returning the
    // block to the pool.
    size_t actual_size;

    void reset() {
        data_device = nullptr;
        offset = 0;
        actual_size = 0;
    }
};

// Additional tensor extra structs for quantized tensors.
// These tensors are loaded from files and should not be allocated in scratch --
// they should always be allocated from the pool. Hence, they do not have an
// `offset`, which indicate their locations in the scratch buffer.
struct ggml_tensor_extra_cl_q4_0 {
    // Quantized values.
    cl_mem q = nullptr;
    // Quantized values in image1d_buffer_t.
    cl_mem q_img = nullptr;
    // Scales.
    cl_mem d = nullptr;
    // Scales in image1d_buffer_t.
    cl_mem d_img = nullptr;
    // Size of quantized values.
    size_t size_q = 0;
    // Size of scales.
    size_t size_d = 0;

    ~ggml_tensor_extra_cl_q4_0() {
        reset();
    }

    void reset() {
        // q and d are subbuffers into the bigger buffer allocated in ggml_backend_buffer.
        // They must be properly released so that the original buffer can be
        // properly released to avoid memory leak.
        if (q != nullptr) {
            CL_CHECK(clReleaseMemObject(q));
            q = nullptr;
        }
        if (d != nullptr) {
            CL_CHECK(clReleaseMemObject(d));
            d = nullptr;
        }
        // Currently, q_img and d_img are only initialized when SMALL_ALLOC is
        // enabled. They point to the images in ggml_backend_opencl_buffer_context.
        // So, there is no need to release them here.
        // TODO: initialize them for non SMALL_PATH path, or remove them.
        q_img = nullptr;
        d_img = nullptr;
        size_q = 0;
        size_d = 0;
    }
};

//------------------------------------------------------------------------------
// Backend API
//------------------------------------------------------------------------------

//
// backend
//
static const char * ggml_backend_opencl_name(ggml_backend_t backend) {
    return "OpenCL";

    UNUSED(backend);
}

static void ggml_backend_opencl_free(ggml_backend_t backend) {
    ggml_cl2_free();

    GGML_UNUSED(backend);
}

static void ggml_backend_opencl_set_tensor_async(ggml_backend_t backend, ggml_tensor * tensor, const void * data, size_t offset, size_t size) {
    GGML_UNUSED(backend);
    GGML_UNUSED(tensor);
    GGML_UNUSED(data);
    GGML_UNUSED(offset);
    GGML_UNUSED(size);
}

static void ggml_backend_opencl_get_tensor_async(ggml_backend_t backend, const ggml_tensor * tensor, void * data, size_t offset, size_t size) {
    GGML_UNUSED(backend);
    GGML_UNUSED(tensor);
    GGML_UNUSED(data);
    GGML_UNUSED(offset);
    GGML_UNUSED(size);
}

static bool ggml_backend_opencl_cpy_tensor_async(ggml_backend_t backend, const ggml_tensor * src, ggml_tensor * dst) {
    GGML_UNUSED(backend);
    GGML_UNUSED(src);
    GGML_UNUSED(dst);
    return false;
}

static void ggml_backend_opencl_synchronize(ggml_backend_t backend) {
    auto * backend_ctx = static_cast<ggml_backend_opencl_context *>(backend->context);

    cl_event evt;
    CL_CHECK(clEnqueueBarrierWithWaitList(backend_ctx->queue, 0, nullptr, &evt));
    CL_CHECK(clWaitForEvents(1, &evt));
    CL_CHECK(clReleaseEvent(evt));
}

// Syncronizes the 'backend_ctx's device with others so that commands
// enqueued to it won't start until commands in the other devices have
// completed.
static void sync_with_other_backends(ggml_backend_opencl_context * backend_ctx) {
    if (g_ggml_backend_opencl_devices.size() < 2)
      return; // No other devices to synchronize with.

    std::vector<cl_event> events;
    events.reserve(g_ggml_backend_opencl_devices.size());

    for (ggml_backend_device & backend_dev : g_ggml_backend_opencl_devices) {
        auto * other_backend_ctx = ggml_cl2_init(&backend_dev);
        if (backend_ctx != other_backend_ctx) {
            cl_event ev;
            CL_CHECK(clEnqueueMarkerWithWaitList(other_backend_ctx->queue, 0, nullptr, &ev));
            CL_CHECK(clFlush(other_backend_ctx->queue));
            events.push_back(ev);
        }
    }

    CL_CHECK(clEnqueueBarrierWithWaitList(backend_ctx->queue, events.size(), events.data(), nullptr));
    for (auto ev : events) {
        CL_CHECK(clReleaseEvent(ev));
    }
}

static void sync_with_other_backends(ggml_backend_t backend) {
    auto * backend_ctx = static_cast<ggml_backend_opencl_context *>(backend->context);
    sync_with_other_backends(backend_ctx);
}

static ggml_status ggml_backend_opencl_graph_compute(ggml_backend_t backend, ggml_cgraph * cgraph) {
    for (int i = 0; i < cgraph->n_nodes; i++) {
        ggml_tensor * node = cgraph->nodes[i];

        // NOTE: this may oversynchronize by synchronizing with
        //       backends/devices which don't compute 'cgraph's
        //       dependencies.
        sync_with_other_backends(backend);

        if (node->op == GGML_OP_RESHAPE || node->op == GGML_OP_TRANSPOSE || node->op == GGML_OP_VIEW || node->op == GGML_OP_PERMUTE || node->op == GGML_OP_NONE) {
            continue;
        }

        bool ok = ggml_cl_compute_forward(backend, node);
        if (!ok) {
            GGML_LOG_ERROR("%s: error: op not supported %s (%s)\n", __func__, node->name, ggml_op_name(node->op));
        }
        GGML_ASSERT(ok);
    }

    return GGML_STATUS_SUCCESS;
}

static bool ggml_opencl_supports_op(ggml_backend_dev_t dev, const struct ggml_tensor * op) {
    GGML_UNUSED(dev);

    switch (op->op) {
        case GGML_OP_NONE:
            return true;
        case GGML_OP_GET_ROWS:
            switch (op->src[0]->type) {
                case GGML_TYPE_F32:
                case GGML_TYPE_F16:
                    return true;
                case GGML_TYPE_Q4_0:
#ifdef GGML_OPENCL_SOA_Q
                    // We do not support flattened Q4_0 (and possibly other Q's)
                    return false;
#else // GGML_OPENCL_SOA_Q
                    return true;
#endif // GGML_OPENCL_SOA_Q
                default:
                    return false;
            }
        case GGML_OP_CPY:
        case GGML_OP_DUP:
        case GGML_OP_CONT:
            switch (op->src[0]->type) {
                case GGML_TYPE_F32:
                    switch (op->type) {
                        case GGML_TYPE_F16:
                        case GGML_TYPE_F32:
                            return true;
                        default:
                            return false;
                    }
                case GGML_TYPE_F16:
                    switch (op->type) {
                        case GGML_TYPE_F16:
                        case GGML_TYPE_F32:
                            return true;
                        default:
                            return false;
                    }
                default:
                    return false;
            }
        case GGML_OP_ADD:
        case GGML_OP_SCALE:
        case GGML_OP_MUL:
        case GGML_OP_DIV:
        case GGML_OP_SUB:
            return op->src[0]->type == GGML_TYPE_F32;
        case GGML_OP_UNARY:
            switch (ggml_get_unary_op(op)) {
                case GGML_UNARY_OP_GELU:
                case GGML_UNARY_OP_SILU:
                case GGML_UNARY_OP_RELU:
                case GGML_UNARY_OP_GELU_QUICK:
                   return ggml_is_contiguous(op->src[0]) && op->src[0]->type == GGML_TYPE_F32;
                case GGML_UNARY_OP_SIGMOID:
                    return ggml_is_contiguous(op->src[0]);
                case GGML_UNARY_OP_TANH:
                   return (op->src[0]->type == GGML_TYPE_F32 && op->type == GGML_TYPE_F32) ||
                          (op->src[0]->type == GGML_TYPE_F16 && op->type == GGML_TYPE_F16);
                default:
                    return false;
            }
        case GGML_OP_CLAMP:
            return op->src[0]->type == GGML_TYPE_F32;
        case GGML_OP_SOFT_MAX:
        case GGML_OP_NORM:
        case GGML_OP_RMS_NORM:
            return true;
        case GGML_OP_REPEAT:
            return op->src[0]->type == GGML_TYPE_F32 && op->type == GGML_TYPE_F32; // Assuming F32 for now, can be expanded
        case GGML_OP_PAD:
            return op->src[0]->type == GGML_TYPE_F32 && op->type == GGML_TYPE_F32 &&
                   op->src[0]->ne[3] == 1 && op->ne[3] == 1;
        case GGML_OP_UPSCALE:
            return op->src[0]->type == GGML_TYPE_F32 && op->type == GGML_TYPE_F32;
        case GGML_OP_CONCAT:
            return op->src[0]->type == GGML_TYPE_F32 && op->src[1]->type == GGML_TYPE_F32 && op->type == GGML_TYPE_F32;
        case GGML_OP_TIMESTEP_EMBEDDING:
            return op->src[0]->type == GGML_TYPE_F32 && op->type == GGML_TYPE_F32;
        case GGML_OP_GROUP_NORM:
            return ggml_is_contiguous(op->src[0]);
        case GGML_OP_MUL_MAT:
            if (op->src[0]->type == GGML_TYPE_F16) {
                return true;
            } else if (op->src[0]->type == GGML_TYPE_F32) {
                return op->src[1]->type == GGML_TYPE_F32;
            } else if (op->src[0]->type == GGML_TYPE_Q4_0 ||
                       op->src[0]->type == GGML_TYPE_Q6_K) {
                return op->src[1]->type == GGML_TYPE_F32 && ggml_is_contiguous(op->src[0]) && ggml_is_contiguous(op->src[1]);
            }
            return false;
        case GGML_OP_RESHAPE:
        case GGML_OP_VIEW:
        case GGML_OP_PERMUTE:
        case GGML_OP_TRANSPOSE:
            return true;
        case GGML_OP_DIAG_MASK_INF:
            return op->ne[3] == 1;
        case GGML_OP_ROPE: {
            const int mode = ((const int32_t *) op->op_params)[2];
            const bool is_mrope = mode & GGML_ROPE_TYPE_MROPE;
            const bool is_vision = mode == GGML_ROPE_TYPE_VISION;
            if (is_mrope && !is_vision) {
                if (op->src[0]->type == GGML_TYPE_F32 ||
                    op->src[0]->type == GGML_TYPE_F16) {
                    return true;
                }
                return false;
            }
            if (is_vision) {
                if (op->src[0]->type == GGML_TYPE_F32 ||
                    op->src[0]->type == GGML_TYPE_F16) {
                    return true;
                }
                return false;
            }
            return true;
        }
        case GGML_OP_IM2COL:
            return true;
        case GGML_OP_ARGSORT:
            return op->src[0]->type == GGML_TYPE_F32;
        case GGML_OP_SUM_ROWS:
            return op->src[0]->type == GGML_TYPE_F32 && ggml_is_contiguous(op->src[0]);
        default:
            return false;
    }
}

// Forward declaration - implementation appears later in the file.
static const char * ggml_backend_opencl_buffer_type_get_name(ggml_backend_buffer_type_t buffer_type);

static ggml_guid_t ggml_backend_opencl_guid() {
    static ggml_guid guid = { 0xde, 0xe0, 0x70, 0xa2, 0x73, 0x4e, 0x4d, 0xbc, 0xb0, 0xc7, 0x4f, 0xd4, 0x6d, 0x4e, 0x90, 0xfe };
    return &guid;
}

static ggml_backend_i ggml_backend_opencl_i = {
    /* .get_name                = */ ggml_backend_opencl_name,
    /* .free                    = */ ggml_backend_opencl_free,
    /* .set_tensor_async        = */ NULL,  /* ggml_backend_opencl_set_tensor_async */
    /* .get_tensor_async        = */ NULL,  /* ggml_backend_opencl_get_tensor_async */
    /* .cpy_tensor_async        = */ NULL,  /* ggml_backend_opencl_cpy_tensor_async */
    /* .synchronize             = */ ggml_backend_opencl_synchronize,
    /* .graph_plan_create       = */ NULL,
    /* .graph_plan_free         = */ NULL,
    /* .graph_plan_update       = */ NULL,
    /* .graph_plan_compute      = */ NULL,
    /* .graph_compute           = */ ggml_backend_opencl_graph_compute,
    /* .event_record            = */ NULL,
    /* .event_wait              = */ NULL,
};

ggml_backend_t ggml_backend_opencl_init(void) {
    ggml_backend_dev_t dev = ggml_backend_reg_dev_get(ggml_backend_opencl_reg(), 0);
    ggml_backend_opencl_context *backend_ctx = ggml_cl2_init(dev);

    ggml_backend_t backend = new ggml_backend {
        /* .guid      = */ ggml_backend_opencl_guid(),
        /* .interface = */ ggml_backend_opencl_i,
        /* .device    = */ dev,
        /* .context   = */ backend_ctx
    };

    return backend;
}

bool ggml_backend_is_opencl(ggml_backend_t backend) {
    return backend && backend->iface.get_name == ggml_backend_opencl_name;
}

//
// buffer
//
struct ggml_backend_opencl_buffer_context {
    // A buffer context can hold multiple cl_mem objects. This is for flattening
    // quantized weights and should be used with GGML_OPENCL_SMALL_ALLOC where
    // each tensor is allocated a separate buffer. When flattening is enabled
    // with small allocation, each tensor is backed by two cl_mem objects (for
    // quants and scales) packed into a backend_opencl_buffer.
    ggml_backend_opencl_buffer_context(cl_mem buf)
        : name("OpenCL") {
        buffer.push_back(buf);
    }

    ~ggml_backend_opencl_buffer_context() {
        for (cl_mem buf : buffer) {
            CL_CHECK(clReleaseMemObject(buf));
        }
        for (cl_mem im : img) {
            CL_CHECK(clReleaseMemObject(im));
        }

        // Delete all extras to trigger their destructors
        for (ggml_tensor_extra_cl * e : temp_tensor_extras) {
            delete e;
        }
        for (ggml_tensor_extra_cl * e : temp_tensor_extras_in_use) {
            delete e;
        }
        for (ggml_tensor_extra_cl_q4_0 * e : temp_tensor_extras_q4_0) {
            delete e;
        }
        for (ggml_tensor_extra_cl_q4_0 * e : temp_tensor_extras_q4_0_in_use) {
            delete e;
        }
    }

    ggml_tensor_extra_cl * ggml_opencl_alloc_temp_tensor_extra() {
        ggml_tensor_extra_cl * extra;
        if (temp_tensor_extras.empty()) {
            extra = new ggml_tensor_extra_cl();
        } else {
            extra = temp_tensor_extras.back();
            temp_tensor_extras.pop_back();
        }

        temp_tensor_extras_in_use.push_back(extra);

        extra->reset();
        return extra;
    }

    ggml_tensor_extra_cl_q4_0 * ggml_opencl_alloc_temp_tensor_extra_q4_0() {
        ggml_tensor_extra_cl_q4_0 * extra;
        if (temp_tensor_extras_q4_0.empty()) {
            extra = new ggml_tensor_extra_cl_q4_0();
        } else {
            extra = temp_tensor_extras_q4_0.back();
            temp_tensor_extras_q4_0.pop_back();
        }

        temp_tensor_extras_q4_0_in_use.push_back(extra);

        extra->reset();
        return extra;
    }

    void reset() {
        for (ggml_tensor_extra_cl * e : temp_tensor_extras_in_use) {
            temp_tensor_extras.push_back(e);
        }
        temp_tensor_extras_in_use.clear();

        for (ggml_tensor_extra_cl_q4_0 * e : temp_tensor_extras_q4_0_in_use) {
            temp_tensor_extras_q4_0.push_back(e);
        }
        temp_tensor_extras_q4_0_in_use.clear();
    }

    // Pools for extras. Available extras are in `temp_tensor_extras`. Extras
    // being used are in `temp_tensor_extras_in_use`. At the first run, new
    // extras get created and put in `in_use`. When the buffer is reset via
    // the `reset` callback, all extras in `in_use` get moved to available extras
    // for reuse.
    std::vector<ggml_tensor_extra_cl *> temp_tensor_extras;
    std::vector<ggml_tensor_extra_cl *> temp_tensor_extras_in_use;
    std::vector<ggml_tensor_extra_cl_q4_0 *> temp_tensor_extras_q4_0;
    std::vector<ggml_tensor_extra_cl_q4_0 *> temp_tensor_extras_q4_0_in_use;

    // The buffer_context is initially created by ggml_backend_buft_alloc_buffer
    // before any tensor is initialized (at the beginning of alloc_tensor_range).
    // Hence, there is alway a buffer object in this vector. When each tensor is
    // being initialized, this original buffer object will be released if both
    // flattening and small allocation are enabled, and additional buffer
    // objects will be created in init_tensor to represent flattened quantized
    // weights.
    std::vector<cl_mem> buffer;
    // These are image1d_buffer_t objects that wrap around the quants and scales.
    // For Q4_0 quantization, there should be two of them - one for quants and
    // one for scales. They should be populated only when flattening and small
    // allocation are enabled.
    std::vector<cl_mem> img;
    std::string name;
};

static void ggml_backend_opencl_buffer_free_buffer(ggml_backend_buffer_t buffer) {
    ggml_backend_opencl_buffer_context * ctx = (ggml_backend_opencl_buffer_context *) buffer->context;
    delete ctx;
}

static void * ggml_backend_opencl_buffer_get_base(ggml_backend_buffer_t buffer) {
    ggml_backend_opencl_context * backend_ctx = ggml_cl2_init(buffer->buft->device);
    return (void *) (uintptr_t) backend_ctx->alignment;
}

static enum ggml_status ggml_backend_opencl_buffer_init_tensor(ggml_backend_buffer_t buffer, ggml_tensor * tensor) {
    ggml_backend_opencl_buffer_context * ctx = (ggml_backend_opencl_buffer_context *) buffer->context;

    ggml_cl2_init(buffer->buft->device);

    if (tensor->view_src != nullptr) {
        GGML_ASSERT(tensor->view_src->buffer->buft == buffer->buft);

        ggml_tensor_extra_cl * view_extra = (ggml_tensor_extra_cl *) tensor->view_src->extra;
        GGML_ASSERT(view_extra && "view_extra is nullptr?");

        // Reuse extra of the parent tensor. The offset of this view tensor
        // becomes `extra->offset + view_offs` and needs to be calculated when
        // it is used. This changes is needed because of the change to
        // ggml_alloc.c in https://github.com/ggerganov/llama.cpp/pull/7640.
        // `buffer` passed in here will always be `tensor->buffer`. It is OK
        // to allocate extras from the same buffer context for ordinary
        // intermediate tensors. But for views into kv cache tensors, doing so
        // would mess up the extras used by kv cache.
        // Before #7640, `buffer` is for intermediate tensors, which is always
        // different from that of kv cache tensors.
        //
        // NB: now extra->offset no longer accounts for view_offs.
        // NB: this should not apply to weight tensors (for end-to-end runs, but
        //     may apply for test-backend-ops).
        // FIXME: if any unexpected results are seen, double check the offset -
        // there could be other places that need fix.
        tensor->extra = view_extra;
    } else {
        {
            size_t offset = (char *) tensor->data - (char *) ggml_backend_opencl_buffer_get_base(buffer);

            ggml_tensor_extra_cl * extra = ctx->ggml_opencl_alloc_temp_tensor_extra();
            extra->offset = offset;
            extra->data_device = ctx->buffer[0];
            extra->actual_size = ggml_nbytes(tensor);

            tensor->extra = extra;
        }
    }
    return GGML_STATUS_SUCCESS;
}

// The optimized gemm and gemv kernels are used for large matrices without batch.
// tensor is the quantized weights matrix.
inline bool use_adreno_kernels(const ggml_backend_opencl_context *backend_ctx, const ggml_tensor *tensor) {
    int64_t threshold_ne0 = 512;
    int64_t threshold_ne1 = 512;
    if (!backend_ctx->adreno_cl_compiler_version.newer_than_or_same(E031, 38, 11, 0) &&
         backend_ctx->adreno_cl_compiler_version.type != DX) {
        threshold_ne0 = 128;
        threshold_ne1 = 128;
    }
    return tensor->ne[0] >= threshold_ne0 && tensor->ne[1] >= threshold_ne1 &&
            tensor->ne[2] == 1 && tensor->ne[3] == 1;
}

static void ggml_backend_opencl_buffer_set_tensor(ggml_backend_buffer_t buffer, ggml_tensor * tensor, const void * data, size_t offset, size_t size) {
    ggml_backend_opencl_context *backend_ctx = ggml_cl2_init(buffer->buft->device);

    cl_context context = backend_ctx->context;
    cl_command_queue queue = backend_ctx->queue;

#ifdef GGML_OPENCL_SOA_Q
    // We separate the quantized bits and scale from block_q4_0 by using an
    // additional kernel, where each thread handles a block. We first read the
    // original weights into a temporary buffer, then create two separate
    // buffers for quantized bits and scales, which are then populated by the
    // conversion kernel.
    if (tensor->type == GGML_TYPE_Q4_0) {
        // Tensors should have been preallocated, therefore they should
        // already have ggml_tensor_extra_cl as extra.
        ggml_tensor_extra_cl * extra_orig = (ggml_tensor_extra_cl *)tensor->extra;
        GGML_ASSERT(extra_orig && "Tesnors in OpenCL backend should have been allocated and initialized");

        // Allocate the new extra and create aliases from the original.
        ggml_backend_opencl_buffer_context * ctx = (ggml_backend_opencl_buffer_context *) buffer->context;
        ggml_tensor_extra_cl_q4_0 * extra = ctx->ggml_opencl_alloc_temp_tensor_extra_q4_0();

        size_t size_d = ggml_nelements(tensor)/ggml_blck_size(tensor->type)*sizeof(ggml_fp16_t);
        size_t size_q = ggml_nelements(tensor)/ggml_blck_size(tensor->type)*ggml_blck_size(tensor->type)/2;
        GGML_ASSERT(size_d + size_q == ggml_nbytes(tensor) && "Incorrect tensor size");

        cl_int err;
        cl_mem data_device = clCreateBuffer(context, CL_MEM_READ_WRITE,
            ggml_nbytes(tensor), NULL, &err);
        CL_CHECK(err);
        CL_CHECK(clEnqueueWriteBuffer(
            queue, data_device, CL_TRUE, 0,
            ggml_nbytes(tensor), data, 0, NULL, NULL));

        // We consider the specified offset arg as always, although For weights
        // the offset arg should be 0 (we do not assert this).
        //GGML_ASSERT(offset == 0);

        // We create subbuffers from the original tensor buffer for scales and
        // quants - i.e., scales and quants are aliases into the buffer obejct
        // that backs the original tensor. This is a cleaner way to adapt to the
        // new memory management.
        // In the old code, we allocate new buffers for scales and quants
        // respectively, which could still be done but would result in double
        // allocation; properly deallocating the preallocated buffer that backs
        // the tensors is tricky and would leak the backend specific information
        // into the general backend code.
        // Does this create misaligned subbuffers (alignment is 1024) in certain
        // cases ?
        cl_buffer_region region;

        // The original tensor memory is divided into scales and quants, i.e.,
        // we first store scales, then quants.
        // Create subbuffer for scales.
        region.origin = align_to(extra_orig->offset + tensor->view_offs + offset, backend_ctx->alignment);
        region.size = size_d;
        extra->d = clCreateSubBuffer(
            extra_orig->data_device, CL_MEM_READ_WRITE,
            CL_BUFFER_CREATE_TYPE_REGION, &region, &err);
        CL_CHECK(err);
        auto previous_origin = region.origin;

        // Create subbuffer for quants.
        region.origin = align_to(previous_origin + size_d, backend_ctx->alignment);
        region.size = size_q;
        extra->q = clCreateSubBuffer(
            extra_orig->data_device, CL_MEM_READ_WRITE,
            CL_BUFFER_CREATE_TYPE_REGION, &region, &err);
        CL_CHECK(err);

        //cl_kernel kernel = backend_ctx->kernel_convert_block_q4_0;
    #ifdef GGML_OPENCL_USE_ADRENO_KERNELS
        cl_kernel kernel = backend_ctx->kernel_convert_block_q4_0;

        // The optimized kernels need weights in natural order, so unshuffle.
        if (use_adreno_kernels(backend_ctx, tensor)) {
            kernel = backend_ctx->kernel_convert_block_q4_0_noshuffle;
        }
    #else
        cl_kernel kernel = backend_ctx->kernel_convert_block_q4_0;
    #endif // GGML_OPENCL_USE_ADRENO_KERNELS
        CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem), &data_device));
        CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_mem), &extra->q));
        CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem), &extra->d));

        size_t global_work_size[] = {(size_t)ggml_nelements(tensor)/ggml_blck_size(tensor->type), 1, 1};
        size_t local_work_size[] = {64, 1, 1};

        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));
        CL_CHECK(clWaitForEvents(1, &evt));
        CL_CHECK(clReleaseMemObject(data_device));

        tensor->extra = extra;

        // transpose the weights and scales
    #ifdef GGML_OPENCL_USE_ADRENO_KERNELS
        // Only do transpose for large, non batched matrix
        // TODO: use preallocated images instead of sub-buffer then image
        if (use_adreno_kernels(backend_ctx, tensor)) {
        // <----------------------------------------------------------------------------------> //
        // start transpose
        // <----------------------------------------------------------------------------------> //
        int M = tensor->ne[1];   // ne01
        int K = tensor->ne[0];   // ne00

        //For matrix-vector multiplication kernel, we assume K is a multiple of 32
        GGML_ASSERT(K % 32 == 0);
        //For transpose kernels, we assume K is a multiple of 4 (satisfied by prior assert), and M is a multiple of 4
        GGML_ASSERT(M % 4 == 0);

        // transpose is out of place, so we need to allocate transposed buffers
        // <----------------------------------------------------------------------------------> //
        // use sub_buffer of max buffer size instead

        size_t q_size_bytes = K * M / 8 * sizeof(float);
        cl_buffer_region region;
        region.origin = 0;
        region.size = q_size_bytes;
        cl_mem qT_d = clCreateSubBuffer(
            backend_ctx->A_q_d_max,
            0,
            CL_BUFFER_CREATE_TYPE_REGION,
            &region,
            &err);
        // cl_mem qT_d = clCreateBuffer(context, CL_MEM_READ_WRITE, q_size_bytes, NULL, &err);
        CL_CHECK(err);

        // size_t d_size_bytes = M * (K / 32) / 2 * sizeof(float);
        size_t d_size_bytes = M * (K / 32) * 2;
        region.origin = 0;
        region.size = d_size_bytes;
        cl_mem dT_d = clCreateSubBuffer(
            backend_ctx->A_s_d_max,
            0,
            CL_BUFFER_CREATE_TYPE_REGION,
            &region,
            &err);
        // cl_mem dT_d = clCreateBuffer(context, CL_MEM_READ_WRITE, d_size_bytes, NULL, &err);
        CL_CHECK(err);

        // <----------------------------------------------------------------------------------> //


        // create images from the buffers
        // <----------------------------------------------------------------------------------> //
        cl_mem q_d_image1D;
        cl_mem d_d_image1D;
        cl_mem qT_d_image1D;
        cl_mem dT_d_image1D;

        cl_image_format img_fmt_1d = { CL_RGBA, CL_HALF_FLOAT };
        cl_image_desc img_desc_1d;

        memset(&img_desc_1d, 0, sizeof(img_desc_1d));
        img_desc_1d.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
        img_desc_1d.image_width = M * K / 4 / 4;
        img_desc_1d.buffer = extra->q;
        q_d_image1D = clCreateImage(context, 0, &img_fmt_1d, &img_desc_1d, NULL, &err);
        CL_CHECK(err);

        img_fmt_1d = { CL_RGBA, CL_HALF_FLOAT };
        memset(&img_desc_1d, 0, sizeof(img_desc_1d));
        img_desc_1d.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
        img_desc_1d.image_width = M * K / 4 / 4;
        img_desc_1d.buffer = qT_d;
        qT_d_image1D = clCreateImage(context, 0, &img_fmt_1d, &img_desc_1d, NULL, &err);
        CL_CHECK(err);

        img_fmt_1d = { CL_RGBA, CL_HALF_FLOAT };
        memset(&img_desc_1d, 0, sizeof(img_desc_1d));
        img_desc_1d.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
        img_desc_1d.image_width = M * K / 32 / 4;
        img_desc_1d.buffer = extra->d;
        d_d_image1D = clCreateImage(context, 0, &img_fmt_1d, &img_desc_1d, NULL, &err);
        CL_CHECK(err);

        img_fmt_1d = { CL_RGBA, CL_HALF_FLOAT };
        memset(&img_desc_1d, 0, sizeof(img_desc_1d));
        img_desc_1d.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
        img_desc_1d.image_width = M * K / 32 / 4;
        img_desc_1d.buffer = dT_d;
        dT_d_image1D = clCreateImage(context, 0, &img_fmt_1d, &img_desc_1d, NULL, &err);
        CL_CHECK(err);
        // <----------------------------------------------------------------------------------> //

        // set up and call the transpose kernels
        // <----------------------------------------------------------------------------------> //
        // weights
        int height_q = M / 4;
        int width_q = K / 4 / 4;
        kernel = backend_ctx->kernel_transpose_16;

        CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem), &q_d_image1D));
        CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_mem), &qT_d_image1D));
        CL_CHECK(clSetKernelArg(kernel, 2, sizeof(int),    &height_q));
        CL_CHECK(clSetKernelArg(kernel, 3, sizeof(int),    &width_q));

        size_t local_size_q[3] = {4, 16, 1};
        size_t global_size_q[3] = {static_cast<size_t>(width_q), static_cast<size_t>(height_q), 1};
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_size_q, local_size_q, 0, NULL, &evt));
        CL_CHECK(clWaitForEvents(1, &evt));

        // scales
        int height_s = M / 4;
        int width_s = K / 32 / 4;

        kernel = backend_ctx->kernel_transpose_16;
        CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_d_image1D));
        CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_mem), &dT_d_image1D));
        CL_CHECK(clSetKernelArg(kernel, 2, sizeof(int), &height_s));
        CL_CHECK(clSetKernelArg(kernel, 3, sizeof(int), &width_s));

        size_t local_size_s[3] = {4, 16, 1};
        size_t global_size_s[3] = {static_cast<size_t>(width_s), static_cast<size_t>(height_s), 1};
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_size_s, local_size_s, 0, NULL, &evt));
        CL_CHECK(clWaitForEvents(1, &evt));
        // <----------------------------------------------------------------------------------> //

        // copy transposed buffer contents to original buffers
        // <----------------------------------------------------------------------------------> //
        // weights
        CL_CHECK(clEnqueueCopyBuffer(queue, qT_d, extra->q, 0, 0, q_size_bytes, 0, NULL, &evt));
        CL_CHECK(clWaitForEvents(1, &evt));

        // scales
        CL_CHECK(clEnqueueCopyBuffer(queue, dT_d, extra->d, 0, 0, d_size_bytes, 0, NULL, &evt));
        CL_CHECK(clWaitForEvents(1, &evt));
        // <----------------------------------------------------------------------------------> //

        // deallocate transpose buffers
        // <----------------------------------------------------------------------------------> //
        CL_CHECK(clReleaseMemObject(qT_d));
        CL_CHECK(clReleaseMemObject(dT_d));

        // deallocate temporary images
        CL_CHECK(clReleaseMemObject(q_d_image1D));
        CL_CHECK(clReleaseMemObject(d_d_image1D));
        CL_CHECK(clReleaseMemObject(qT_d_image1D));
        CL_CHECK(clReleaseMemObject(dT_d_image1D));
        // <----------------------------------------------------------------------------------> //
        // end transpose
        // <----------------------------------------------------------------------------------> //
        }
    #endif // GGML_OPENCL_USE_ADRENO_KERNELS

        return;
    }
#endif // GGML_OPENCL_SOA_Q

    ggml_tensor_extra_cl * extra = (ggml_tensor_extra_cl *) tensor->extra;
    GGML_ASSERT(extra);

    CL_CHECK(clEnqueueWriteBuffer(
        queue, extra->data_device, CL_TRUE, extra->offset + offset,
        size, data, 0, NULL, NULL));

    GGML_UNUSED(buffer);
}

static void ggml_backend_opencl_buffer_get_tensor(ggml_backend_buffer_t buffer, const ggml_tensor * tensor, void * data, size_t offset, size_t size) {
    GGML_ASSERT(tensor->extra);

    ggml_backend_opencl_context *backend_ctx = ggml_cl2_init(buffer->buft->device);

    cl_context context = backend_ctx->context;
    cl_command_queue queue = backend_ctx->queue;

    // Make sure all previously submitted commands in other devices are finished.
    sync_with_other_backends(backend_ctx);

#ifdef GGML_OPENCL_SOA_Q
    // In end-to-end runs, get_tensor is usually used to get back the logits,
    // where we can simply do clEnqueueReadBuffer since they are f32.
    // However, in test-backend-ops, the GPU graph is copied to the CPU backend,
    // which requires reading back quantized weight tensors.
    // To properly support this, we need to restore block_q4_0 struct arrays
    // from the flattened buffers.
    if (tensor->type == GGML_TYPE_Q4_0) {
        ggml_tensor_extra_cl_q4_0 * extra = (ggml_tensor_extra_cl_q4_0 *)tensor->extra;

        cl_int err;
        cl_mem data_device = clCreateBuffer(context, CL_MEM_READ_WRITE,
            ggml_nbytes(tensor), NULL, &err);
        CL_CHECK(err);

        cl_kernel kernel = backend_ctx->kernel_restore_block_q4_0;
        CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem), &extra->q));
        CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_mem), &extra->d));
        CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem), &data_device));

        size_t global_work_size[] = {(size_t)ggml_nelements(tensor)/ggml_blck_size(tensor->type), 1, 1};
        size_t local_work_size[] = {1, 1, 1};

        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL,
            global_work_size, local_work_size, 0, NULL, &evt));
        CL_CHECK(clWaitForEvents(1, &evt));
        CL_CHECK(clEnqueueReadBuffer(
            queue, data_device, CL_TRUE, offset,
            size, data, 0, NULL, NULL));
        CL_CHECK(clReleaseMemObject(data_device));
        return;
    }
#endif // GGML_OPENCL_SOA_Q

    ggml_tensor_extra_cl * extra = (ggml_tensor_extra_cl *) tensor->extra;

    CL_CHECK(clEnqueueReadBuffer(
        queue, extra->data_device, CL_TRUE, extra->offset + tensor->view_offs + offset,
        size, data, 0, NULL, NULL));

    GGML_UNUSED(buffer);
}

static void ggml_backend_opencl_buffer_clear(ggml_backend_buffer_t buffer, uint8_t value) {
    ggml_backend_dev_t dev = buffer->buft->device;
    ggml_backend_opencl_context *backend_ctx = ggml_cl2_init(dev);
    cl_command_queue queue = backend_ctx->queue;

    ggml_backend_opencl_buffer_context * ctx = (ggml_backend_opencl_buffer_context *) buffer->context;
    for (cl_mem buf : ctx->buffer) {
        CL_CHECK(clEnqueueFillBuffer(queue, buf, &value, sizeof(value), 0, buffer->size, 0, NULL, NULL));
    }
    CL_CHECK(clFinish(queue));
}

static void ggml_backend_opencl_buffer_reset(ggml_backend_buffer_t buffer) {
    ggml_backend_opencl_buffer_context * ctx = (ggml_backend_opencl_buffer_context *) buffer->context;
    ctx->reset();
}

static ggml_backend_buffer_i ggml_backend_opencl_buffer_interface = {
    /* .free_buffer     = */ ggml_backend_opencl_buffer_free_buffer,
    /* .get_base        = */ ggml_backend_opencl_buffer_get_base,
    /* .init_tensor     = */ ggml_backend_opencl_buffer_init_tensor,
    /* .memset_tensor   = */ NULL,
    /* .set_tensor      = */ ggml_backend_opencl_buffer_set_tensor,
    /* .get_tensor      = */ ggml_backend_opencl_buffer_get_tensor,
    /* .cpy_tensor      = */ NULL,
    /* .clear           = */ ggml_backend_opencl_buffer_clear,
    /* .reset           = */ ggml_backend_opencl_buffer_reset,
};

//
// buffer type
//

static const char * ggml_backend_opencl_buffer_type_get_name(ggml_backend_buffer_type_t buffer_type) {
    return "OpenCL";

    GGML_UNUSED(buffer_type);
}

static ggml_backend_buffer_t ggml_backend_opencl_buffer_type_alloc_buffer(ggml_backend_buffer_type_t buffer_type, size_t size) {
    ggml_backend_opencl_context *backend_ctx = ggml_cl2_init(buffer_type->device);

    // clCreateBuffer returns -61 for size 0
    size = std::max(size, (size_t)1);

    cl_int err;
    cl_mem mem = clCreateBuffer(backend_ctx->context, CL_MEM_READ_WRITE, size, NULL, &err);
    if (err != CL_SUCCESS) {
        GGML_LOG_INFO("%s: failed to allocate %.2f MiB\n", __func__, size / 1024.0 / 1024.0);
        return nullptr;
    }

    ggml_backend_opencl_buffer_context * ctx = new ggml_backend_opencl_buffer_context(mem);

    return ggml_backend_buffer_init(buffer_type, ggml_backend_opencl_buffer_interface, ctx, size);
}

static size_t ggml_backend_opencl_buffer_type_get_alignment(ggml_backend_buffer_type_t buffer_type) {
    ggml_backend_opencl_context * backend_ctx = ggml_cl2_init(buffer_type->device);
    return backend_ctx->alignment;
}

static size_t ggml_backend_opencl_buffer_type_get_max_size(ggml_backend_buffer_type_t buffer_type) {
    static size_t max_size = -1;
    if (max_size == (size_t)-1) {
        ggml_backend_opencl_context * backend_ctx = ggml_cl2_init(buffer_type->device);
        max_size = backend_ctx->max_alloc_size;
    }
    return max_size;
}

static bool ggml_backend_opencl_buffer_type_supports_backend(ggml_backend_buffer_type_t buft, ggml_backend_t backend) {
    return ggml_backend_is_opencl(backend);

    UNUSED(buft);
}

static ggml_backend_buffer_type_i ggml_backend_opencl_buffer_type_interface = {
    /* .get_name         = */ ggml_backend_opencl_buffer_type_get_name,
    /* .alloc_buffer     = */ ggml_backend_opencl_buffer_type_alloc_buffer,
    /* .get_alignment    = */ ggml_backend_opencl_buffer_type_get_alignment,
    /* .get_max_size     = */ ggml_backend_opencl_buffer_type_get_max_size,
    /* .get_alloc_size   = */ NULL,
    /* .is_host          = */ NULL,
};

//
// backend device
//

static const char * ggml_backend_opencl_device_get_name(ggml_backend_dev_t dev) {
    return "GPUOpenCL";

    GGML_UNUSED(dev);
}

static const char * ggml_backend_opencl_device_get_description(ggml_backend_dev_t dev) {
    ggml_backend_opencl_device_context *dev_ctx = (ggml_backend_opencl_device_context *) dev->context;
    return dev_ctx->device_name.c_str();
}

static void ggml_backend_opencl_device_get_memory(ggml_backend_dev_t dev, size_t * free, size_t * total) {
    *free = 1;
    *total = 1;

    GGML_UNUSED(dev);
}

static enum ggml_backend_dev_type ggml_backend_opencl_device_get_type(ggml_backend_dev_t dev) {
    return GGML_BACKEND_DEVICE_TYPE_GPU;

    GGML_UNUSED(dev);
}

static void ggml_backend_opencl_device_get_props(ggml_backend_dev_t dev, struct ggml_backend_dev_props * props) {
    props->name        = ggml_backend_opencl_device_get_name(dev);
    props->description = ggml_backend_opencl_device_get_description(dev);
    props->type        = ggml_backend_opencl_device_get_type(dev);
    ggml_backend_opencl_device_get_memory(dev, &props->memory_free, &props->memory_total);
    props->caps = ggml_backend_dev_caps {
        /* .async                 = */ false,
        /* .host_buffer           = */ false,
        /* .buffer_from_host_ptr  = */ false,
        /* .events                = */ false,
    };
}

static ggml_backend_t ggml_backend_opencl_device_init(ggml_backend_dev_t dev, const char * params) {
    ggml_backend_opencl_context * backend_ctx = ggml_cl2_init(dev);

    ggml_backend_t backend = new ggml_backend {
        /* .guid      = */ ggml_backend_opencl_guid(),
        /* .interface = */ ggml_backend_opencl_i,
        /* .device    = */ dev,
        /* .context   = */ backend_ctx,
    };

    return backend;

    GGML_UNUSED(params);
}

static ggml_backend_buffer_type_t ggml_backend_opencl_device_get_buffer_type(ggml_backend_dev_t dev) {
    auto * dev_ctx = static_cast<ggml_backend_opencl_device_context *>(dev->context);

    dev_ctx->buffer_type = ggml_backend_buffer_type{
        /* .iface   = */ ggml_backend_opencl_buffer_type_interface,
        /* .device  = */ dev,
        /* .context = */ nullptr,
    };

    return &dev_ctx->buffer_type;
}

static ggml_backend_buffer_t ggml_backend_opencl_device_buffer_from_ptr(ggml_backend_dev_t dev, void * ptr, size_t size, size_t max_tensor_size) {
    GGML_UNUSED(dev);
    GGML_UNUSED(ptr);
    GGML_UNUSED(size);
    GGML_UNUSED(max_tensor_size);
    return nullptr;
}

static bool ggml_backend_opencl_device_supports_op(ggml_backend_dev_t dev, const struct ggml_tensor * op) {
    return ggml_opencl_supports_op(dev, op);
}

static bool ggml_backend_opencl_device_supports_buft(ggml_backend_dev_t dev, ggml_backend_buffer_type_t buft) {
    // Check 'dev' and 'buffer_type' are not objects belonging to this backend.
    if (dev->iface.get_name != ggml_backend_opencl_device_get_name ||
        buft->iface.get_name != ggml_backend_opencl_buffer_type_get_name) {
        return false;
    }

    // Check cl_context is the same. clEnqueue* commands may not use
    // buffers from another cl_context.
    ggml_backend_opencl_context * backend_ctx0 = ggml_cl2_init(dev);
    ggml_backend_opencl_context * backend_ctx1 = ggml_cl2_init(buft->device);
    return backend_ctx0->context == backend_ctx1->context;
}

namespace /* anonymous */ {
struct ggml_backend_device_i ggml_backend_opencl_device_i = {
    /* .get_name             = */ ggml_backend_opencl_device_get_name,
    /* .get_description      = */ ggml_backend_opencl_device_get_description,
    /* .get_memory           = */ ggml_backend_opencl_device_get_memory,
    /* .get_type             = */ ggml_backend_opencl_device_get_type,
    /* .get_props            = */ ggml_backend_opencl_device_get_props,
    /* .init_backend         = */ ggml_backend_opencl_device_init,
    /* .get_buffer_type      = */ ggml_backend_opencl_device_get_buffer_type,
    /* .get_host_buffer_type = */ NULL,
    /* .buffer_from_host_ptr = */ ggml_backend_opencl_device_buffer_from_ptr,
    /* .supports_op          = */ ggml_backend_opencl_device_supports_op,
    /* .supports_buft        = */ ggml_backend_opencl_device_supports_buft,
    /* .offload_op           = */ NULL,
    /* .event_new            = */ NULL,
    /* .event_free           = */ NULL,
    /* .event_synchronize    = */ NULL,
};
}

// Backend registry

static const char * ggml_backend_opencl_reg_get_name(ggml_backend_reg_t reg) {
    return "OpenCL";

    GGML_UNUSED(reg);
}

static size_t ggml_backend_opencl_reg_device_count(ggml_backend_reg_t reg) {
    return g_ggml_backend_opencl_devices.size();

    GGML_UNUSED(reg);
}

static ggml_backend_dev_t ggml_backend_opencl_reg_device_get(ggml_backend_reg_t reg, size_t index) {
    GGML_ASSERT(index < ggml_backend_opencl_reg_device_count(reg));

    return &g_ggml_backend_opencl_devices[index];

    GGML_UNUSED(reg);
    GGML_UNUSED(index);
}

static struct ggml_backend_reg_i ggml_backend_opencl_reg_i = {
    /* .get_name         = */ ggml_backend_opencl_reg_get_name,
    /* .device_count     = */ ggml_backend_opencl_reg_device_count,
    /* .device_get       = */ ggml_backend_opencl_reg_device_get,
    /* .get_proc_address = */ NULL,
};

ggml_backend_reg_t ggml_backend_opencl_reg(void) {
    static std::mutex mutex;
    static ggml_backend_reg reg;
    static bool initialized = false;
    std::lock_guard<std::mutex> lock(mutex);

    if (initialized) {
        return &reg;
    }
    initialized = true;

    g_ggml_backend_opencl_devices = ggml_opencl_probe_devices(&reg);

    reg = ggml_backend_reg{
        /* .api_version = */ GGML_BACKEND_API_VERSION,
        /* .iface       = */ ggml_backend_opencl_reg_i,
        /* .context     = */ NULL,
    };

    return &reg;
}

GGML_BACKEND_DL_IMPL(ggml_backend_opencl_reg)

//------------------------------------------------------------------------------
// Debugging utils
//------------------------------------------------------------------------------
#if 0
#define QK4_0 32
typedef struct {
    ggml_fp16_t d;          // delta
    uint8_t qs[QK4_0 / 2];  // nibbles / quants
} block_q4_0;
static_assert(sizeof(block_q4_0) == sizeof(ggml_fp16_t) + QK4_0 / 2,
    "wrong q4_0 block size/padding");

#include <math.h>
#ifdef __cplusplus
#include "half.hpp"
#endif

static void dump_tensor(ggml_backend_t backend, const struct ggml_tensor * tensor) {
    void * buf = malloc(ggml_nbytes(tensor));

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;
#ifdef GGML_OPENCL_SOA_Q
    void * buf_q;
    void * buf_d;
#endif

    // Make sure everything is done.
    CL_CHECK(clFinish(queue));

#ifdef GGML_OPENCL_SOA_Q
    if (tensor->type == GGML_TYPE_Q4_0) {
        ggml_tensor_extra_cl_q4_0 * extra = (ggml_tensor_extra_cl_q4_0 *) tensor->extra;
        GGML_ASSERT(extra);

        size_t size_q = ggml_nelements(tensor)/QK4_0 * QK4_0/2;
        size_t size_d = ggml_nelements(tensor)/QK4_0 * sizeof(ggml_fp16_t);
        GGML_ASSERT(size_q + size_d == ggml_nbytes(tensor));
        buf_q = malloc(size_q);
        buf_d = malloc(size_d);

        CL_CHECK(clEnqueueReadBuffer(queue, extra->q, CL_TRUE, 0, size_q, buf_q, 0, NULL, NULL));
        CL_CHECK(clEnqueueReadBuffer(queue, extra->d, CL_TRUE, 0, size_d, buf_d, 0, NULL, NULL));
        CL_CHECK(clFinish(queue));
    } else {
        // Read out the tensor from GPU memory.
        ggml_tensor_extra_cl * extra = (ggml_tensor_extra_cl *) tensor->extra;
        GGML_ASSERT(extra);

        CL_CHECK(clEnqueueReadBuffer(queue, extra->data_device, CL_TRUE,
        extra->offset, ggml_nbytes(tensor), buf, 0, NULL, NULL));
        CL_CHECK(clFinish(queue));
    }
#else
    // Read out the tensor from GPU memory.
    ggml_tensor_extra_cl * extra = (ggml_tensor_extra_cl *) tensor->extra;
    GGML_ASSERT(extra);

    CL_CHECK(clEnqueueReadBuffer(queue, extra->data_device, CL_TRUE,
        extra->offset, ggml_nbytes(tensor), buf, 0, NULL, NULL));
    CL_CHECK(clFinish(queue));
#endif // GGML_OPENCL_SOA_Q

    // Open file and dump.
    char fname[512];
    sprintf(fname, "./tensor-dumps/%s.txt", tensor->name);
    FILE * f = fopen(fname, "w");
    if (!f) {
        printf("Failed to open %s\n", fname);
        return;
    }

    if (tensor->type == GGML_TYPE_F32) {
        float * data = (float *) buf;
        for (int i = 0; i < ggml_nelements(tensor); ++i) {
            if (isnan(data[i])) {
                printf("NaN found: %s\n", tensor->name);
                break;
            }
            fprintf(f, "%f\n", data[i]);
        }
    } else if (tensor->type == GGML_TYPE_I32) {
        int * data = (int *) buf;
        for (int i = 0; i < ggml_nelements(tensor); ++i) {
            if (isnan(data[i])) {
                printf("NaN found: %s\n", tensor->name);
                break;
            }
            fprintf(f, "%d\n", data[i]);
        }
    } else if (tensor->type == GGML_TYPE_F16) {
#ifdef __cplusplus
        half_float::half * data = (half_float::half *) buf;
        for (int i = 0; i < ggml_nelements(tensor); ++i) {
            if (std::isnan(data[i])) {
                printf("NaN found: %s\n", tensor->name);
                break;
            }
            fprintf(f, "%f\n", float(data[i]));
        }
#endif
    } else if (tensor->type == GGML_TYPE_Q4_0) {
#ifdef GGML_OPENCL_SOA_Q
        ggml_fp16_t * data_d = (ggml_fp16_t *)buf_d;
        unsigned char * data_q = (unsigned char *)buf_q;

        for (int i = 0; i < ggml_nelements(tensor)/QK4_0; ++i) {
            fprintf(f, "%04x, ", data_d[i]);
            for (int k = 0; k < QK4_0/2; ++k) {
                fprintf(f, "%02x, ", data_q[k]);
            }
            fprintf(f, "\n");
            data_q += QK4_0/2;
        }
        free(buf_d);
        free(buf_q);
#else
        block_q4_0 * data = (block_q4_0 *) buf;
        for (int i = 0; i < ggml_nelements(tensor)/QK4_0; ++i) {
            fprintf(f, "%04x, ", data[i].d);
            for (int k = 0; k < QK4_0/2; ++k) {
                fprintf(f, "%02x, ", data[i].qs[k]);
            }
            fprintf(f, "\n");
        }
#endif // GGML_OPENCL_SOA_Q
    }
    free(buf);
    fflush(f);
    fclose(f);
}
#else
#define dump_tensor(tensor)
#endif

//------------------------------------------------------------------------------
// Profiling utility
//------------------------------------------------------------------------------
#ifdef GGML_OPENCL_PROFILING
static void populateProfilingInfo(
        ProfilingInfo& info, cl_event evt, cl_kernel kernel,
        size_t global_size[3], size_t local_size[3],
        const ggml_tensor * tensor) {
    info.op_name     = tensor->name;
    info.kernel      = kernel;
    info.evt         = evt;

    info.local_size[0]  = local_size[0];
    info.local_size[1]  = local_size[1];
    info.local_size[2]  = local_size[2];
    info.global_size[0] = global_size[0];
    info.global_size[1] = global_size[1];
    info.global_size[2] = global_size[2];
    info.output_size[0] = tensor->ne[0];
    info.output_size[1] = tensor->ne[1];
    info.output_size[2] = tensor->ne[2];
    info.output_size[3] = tensor->ne[3];
}
#endif

//------------------------------------------------------------------------------
// Ops
//------------------------------------------------------------------------------

static bool ggml_cl_can_mul_mat(const struct ggml_tensor * src0, const struct ggml_tensor * src1, struct ggml_tensor * dst) {
    const int64_t ne10 = src1->ne[0];

    const int64_t ne0 = dst->ne[0];
    const int64_t ne1 = dst->ne[1];

    // TODO: find the optimal values for these
    return (src0->type == GGML_TYPE_F32 || src0->type == GGML_TYPE_F16 || ggml_is_quantized(src0->type)) &&
            src1->type == GGML_TYPE_F32 &&
             dst->type == GGML_TYPE_F32 &&
            (ne0 >= 32 && ne1 >= 32 && ne10 >= 32);
}

static void ggml_cl_nop(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    UNUSED(backend);
    UNUSED(src0);
    UNUSED(src1);
    UNUSED(dst);
}

static void ggml_cl_get_rows(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(src1);
    GGML_ASSERT(src1->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    const int      ne00 = src0 ? src0->ne[0] : 0;
    const cl_ulong nb01 = src0 ? src0->nb[1] : 0;
    const cl_ulong nb02 = src0 ? src0->nb[2] : 0;
    const int      ne10 = src1 ? src1->ne[0] : 0;
    const cl_ulong nb10 = src1 ? src1->nb[0] : 0;
    const int      ne11 = src1 ? src1->ne[1] : 0;
    const cl_ulong nb11 = src1 ? src1->nb[1] : 0;
    const cl_ulong nb1  = dst  ?  dst->nb[1] : 0;
    const cl_ulong nb2  = dst  ?  dst->nb[2] : 0;

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra1 = (ggml_tensor_extra_cl *)src1->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offset1 = extra1->offset + src1->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    cl_kernel kernel;

    switch (src0->type) {
        case GGML_TYPE_F32:
            kernel = backend_ctx->kernel_get_rows_f32;
            break;
        case GGML_TYPE_F16:
            kernel = backend_ctx->kernel_get_rows_f16;
            break;
        case GGML_TYPE_Q4_0:
            kernel = backend_ctx->kernel_get_rows_q4_0;
            break;
        default:
            GGML_ASSERT(false && "not implemented");
    }

    CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
    CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
    CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offsetd));
    CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),      &ne00));
    CL_CHECK(clSetKernelArg(kernel,  7, sizeof(cl_ulong), &nb01));
    CL_CHECK(clSetKernelArg(kernel,  8, sizeof(cl_ulong), &nb02));
    CL_CHECK(clSetKernelArg(kernel,  9, sizeof(int),      &ne10));
    CL_CHECK(clSetKernelArg(kernel, 10, sizeof(cl_ulong), &nb10));
    CL_CHECK(clSetKernelArg(kernel, 11, sizeof(cl_ulong), &nb11));
    CL_CHECK(clSetKernelArg(kernel, 12, sizeof(cl_ulong), &nb1));
    CL_CHECK(clSetKernelArg(kernel, 13, sizeof(cl_ulong), &nb2));

    size_t global_work_size[] = {(size_t)ne10, (size_t)ne11, 1};
    size_t local_work_size[] = {1, 1, 1};

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
}

static void ggml_cl_add(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(src1);
    GGML_ASSERT(src1->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    const int  ne00 = src0 ? src0->ne[0] : 0;
    const int  ne01 = src0 ? src0->ne[1] : 0;
    const int  ne02 = src0 ? src0->ne[2] : 0;
    const int  ne03 = src0 ? src0->ne[3] : 0;

    const cl_ulong nb00 = src0 ? src0->nb[0] : 0;
    const cl_ulong nb01 = src0 ? src0->nb[1] : 0;
    const cl_ulong nb02 = src0 ? src0->nb[2] : 0;
    const cl_ulong nb03 = src0 ? src0->nb[3] : 0;

    const int  ne10 = src1 ? src1->ne[0] : 0;
    const int  ne11 = src1 ? src1->ne[1] : 0;
    const int  ne12 = src1 ? src1->ne[2] : 0;
    const int  ne13 = src1 ? src1->ne[3] : 0; UNUSED(ne13);

    const cl_ulong nb10 = src1 ? src1->nb[0] : 0;
    const cl_ulong nb11 = src1 ? src1->nb[1] : 0;
    const cl_ulong nb12 = src1 ? src1->nb[2] : 0;
    const cl_ulong nb13 = src1 ? src1->nb[3] : 0; UNUSED(nb13);

    const int  ne0  = dst ? dst->ne[0] : 0;
    const int  ne1  = dst ? dst->ne[1] : 0;
    const int  ne2  = dst ? dst->ne[2] : 0;
    const int  ne3  = dst ? dst->ne[3] : 0;

    const cl_ulong nb0  = dst ? dst->nb[0] : 0;
    const cl_ulong nb1  = dst ? dst->nb[1] : 0;
    const cl_ulong nb2  = dst ? dst->nb[2] : 0;
    const cl_ulong nb3  = dst ? dst->nb[3] : 0;

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra1 = (ggml_tensor_extra_cl *)src1->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offset1 = extra1->offset + src1->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    bool bcast_row = false;
    cl_kernel kernel;

    if (ggml_nelements(src1) == ne10 && ggml_is_contiguous(src1) && ne00 % 4 == 0 && ne10 % 4 == 0) {
        GGML_ASSERT(ggml_is_contiguous(src0));

        // src1 is a row
        GGML_ASSERT(ne11 == 1);

        bcast_row = true;
        int ne = ne00 / 4;
        kernel = backend_ctx->kernel_add_row;

        CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
        CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
        CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extra1->data_device));
        CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offset1));
        CL_CHECK(clSetKernelArg(kernel, 4, sizeof(cl_mem),   &extrad->data_device));
        CL_CHECK(clSetKernelArg(kernel, 5, sizeof(cl_ulong), &offsetd));
        CL_CHECK(clSetKernelArg(kernel, 6, sizeof(int),      &ne));
    } else {
        kernel = backend_ctx->kernel_add;

        CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0->data_device));
        CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong), &offset0));
        CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
        CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
        CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   &extrad->data_device));
        CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offsetd));
        CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),      &ne00));
        CL_CHECK(clSetKernelArg(kernel,  7, sizeof(int),      &ne01));
        CL_CHECK(clSetKernelArg(kernel,  8, sizeof(int),      &ne02));
        CL_CHECK(clSetKernelArg(kernel,  9, sizeof(int),      &ne03));
        CL_CHECK(clSetKernelArg(kernel, 10, sizeof(cl_ulong), &nb00));
        CL_CHECK(clSetKernelArg(kernel, 11, sizeof(cl_ulong), &nb01));
        CL_CHECK(clSetKernelArg(kernel, 12, sizeof(cl_ulong), &nb02));
        CL_CHECK(clSetKernelArg(kernel, 13, sizeof(cl_ulong), &nb03));
        CL_CHECK(clSetKernelArg(kernel, 14, sizeof(int),      &ne10));
        CL_CHECK(clSetKernelArg(kernel, 15, sizeof(int),      &ne11));
        CL_CHECK(clSetKernelArg(kernel, 16, sizeof(int),      &ne12));
        CL_CHECK(clSetKernelArg(kernel, 17, sizeof(int),      &ne13));
        CL_CHECK(clSetKernelArg(kernel, 18, sizeof(cl_ulong), &nb10));
        CL_CHECK(clSetKernelArg(kernel, 19, sizeof(cl_ulong), &nb11));
        CL_CHECK(clSetKernelArg(kernel, 20, sizeof(cl_ulong), &nb12));
        CL_CHECK(clSetKernelArg(kernel, 21, sizeof(cl_ulong), &nb13));
        CL_CHECK(clSetKernelArg(kernel, 22, sizeof(int),      &ne0));
        CL_CHECK(clSetKernelArg(kernel, 23, sizeof(int),      &ne1));
        CL_CHECK(clSetKernelArg(kernel, 24, sizeof(int),      &ne2));
        CL_CHECK(clSetKernelArg(kernel, 25, sizeof(int),      &ne3));
        CL_CHECK(clSetKernelArg(kernel, 26, sizeof(cl_ulong), &nb0));
        CL_CHECK(clSetKernelArg(kernel, 27, sizeof(cl_ulong), &nb1));
        CL_CHECK(clSetKernelArg(kernel, 28, sizeof(cl_ulong), &nb2));
        CL_CHECK(clSetKernelArg(kernel, 29, sizeof(cl_ulong), &nb3));
    }

    if (bcast_row) {
        int n = ggml_nelements(dst)/4;
        size_t global_work_size[] = {(size_t)n, 1, 1};
        size_t local_work_size[] = {64, 1, 1};

        size_t * local_work_size_ptr = local_work_size;
        if (n % 64 != 0 && !backend_ctx->non_uniform_workgroups) {
            local_work_size_ptr = nullptr;  // Let driver choose the work-group sizes.
        }

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size_ptr, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, NULL));
#endif
    } else {
        unsigned int nth = MIN(64, ne0);
        size_t global_work_size[] = {ne01*nth, (size_t)ne02, (size_t)ne03};
        size_t local_work_size[] = {nth, 1, 1};

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
    }
}

static void ggml_cl_mul(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(src1);
    GGML_ASSERT(src1->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    const int ne00 = src0 ? src0->ne[0] : 0;
    const int ne01 = src0 ? src0->ne[1] : 0;
    const int ne02 = src0 ? src0->ne[2] : 0;
    const int ne03 = src0 ? src0->ne[3] : 0;

    const cl_ulong nb00 = src0 ? src0->nb[0] : 0;
    const cl_ulong nb01 = src0 ? src0->nb[1] : 0;
    const cl_ulong nb02 = src0 ? src0->nb[2] : 0;
    const cl_ulong nb03 = src0 ? src0->nb[3] : 0;

    const int ne10 = src1 ? src1->ne[0] : 0;
    const int ne11 = src1 ? src1->ne[1] : 0;
    const int ne12 = src1 ? src1->ne[2] : 0;
    const int ne13 = src1 ? src1->ne[3] : 0; UNUSED(ne13);

    const cl_ulong nb10 = src1 ? src1->nb[0] : 0;
    const cl_ulong nb11 = src1 ? src1->nb[1] : 0;
    const cl_ulong nb12 = src1 ? src1->nb[2] : 0;
    const cl_ulong nb13 = src1 ? src1->nb[3] : 0; UNUSED(nb13);

    const int ne0  = dst ? dst->ne[0] : 0;
    const int ne1  = dst ? dst->ne[1] : 0;
    const int ne2  = dst ? dst->ne[2] : 0;
    const int ne3  = dst ? dst->ne[3] : 0;

    const cl_ulong nb0  = dst ? dst->nb[0] : 0;
    const cl_ulong nb1  = dst ? dst->nb[1] : 0;
    const cl_ulong nb2  = dst ? dst->nb[2] : 0;
    const cl_ulong nb3  = dst ? dst->nb[3] : 0;

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra1 = (ggml_tensor_extra_cl *)src1->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offset1 = extra1->offset + src1->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    bool bcast_row = false;
    cl_kernel kernel;

    if (ggml_nelements(src1) == ne10 && ggml_is_contiguous(src1) && ne00 % 4 == 0 && ne10 % 4 == 0) {
        GGML_ASSERT(ggml_is_contiguous(src0));

        // src1 is a row
        GGML_ASSERT(ne11 == 1);

        bcast_row = true;
        int ne = ne00 / 4;
        kernel = backend_ctx->kernel_mul_row;

        CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
        CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
        CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extra1->data_device));
        CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offset1));
        CL_CHECK(clSetKernelArg(kernel, 4, sizeof(cl_mem),   &extrad->data_device));
        CL_CHECK(clSetKernelArg(kernel, 5, sizeof(cl_ulong), &offsetd));
        CL_CHECK(clSetKernelArg(kernel, 6, sizeof(int),      &ne));
    } else {
        kernel = backend_ctx->kernel_mul;

        CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0->data_device));
        CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong), &offset0));
        CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
        CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
        CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   &extrad->data_device));
        CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offsetd));
        CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),      &ne00));
        CL_CHECK(clSetKernelArg(kernel,  7, sizeof(int),      &ne01));
        CL_CHECK(clSetKernelArg(kernel,  8, sizeof(int),      &ne02));
        CL_CHECK(clSetKernelArg(kernel,  9, sizeof(int),      &ne03));
        CL_CHECK(clSetKernelArg(kernel, 10, sizeof(cl_ulong), &nb00));
        CL_CHECK(clSetKernelArg(kernel, 11, sizeof(cl_ulong), &nb01));
        CL_CHECK(clSetKernelArg(kernel, 12, sizeof(cl_ulong), &nb02));
        CL_CHECK(clSetKernelArg(kernel, 13, sizeof(cl_ulong), &nb03));
        CL_CHECK(clSetKernelArg(kernel, 14, sizeof(int),      &ne10));
        CL_CHECK(clSetKernelArg(kernel, 15, sizeof(int),      &ne11));
        CL_CHECK(clSetKernelArg(kernel, 16, sizeof(int),      &ne12));
        CL_CHECK(clSetKernelArg(kernel, 17, sizeof(int),      &ne13));
        CL_CHECK(clSetKernelArg(kernel, 18, sizeof(cl_ulong), &nb10));
        CL_CHECK(clSetKernelArg(kernel, 19, sizeof(cl_ulong), &nb11));
        CL_CHECK(clSetKernelArg(kernel, 20, sizeof(cl_ulong), &nb12));
        CL_CHECK(clSetKernelArg(kernel, 21, sizeof(cl_ulong), &nb13));
        CL_CHECK(clSetKernelArg(kernel, 22, sizeof(int),      &ne0));
        CL_CHECK(clSetKernelArg(kernel, 23, sizeof(int),      &ne1));
        CL_CHECK(clSetKernelArg(kernel, 24, sizeof(int),      &ne2));
        CL_CHECK(clSetKernelArg(kernel, 25, sizeof(int),      &ne3));
        CL_CHECK(clSetKernelArg(kernel, 26, sizeof(cl_ulong), &nb0));
        CL_CHECK(clSetKernelArg(kernel, 27, sizeof(cl_ulong), &nb1));
        CL_CHECK(clSetKernelArg(kernel, 28, sizeof(cl_ulong), &nb2));
        CL_CHECK(clSetKernelArg(kernel, 29, sizeof(cl_ulong), &nb3));
    }

    if (bcast_row) {
        int n = ggml_nelements(dst)/4;
        size_t global_work_size[] = {(size_t)n, 1, 1};
        size_t local_work_size[] = {64, 1, 1};

        size_t * local_work_size_ptr = local_work_size;
        if (n % 64 != 0 && !backend_ctx->non_uniform_workgroups) {
            local_work_size_ptr = nullptr;  // Let driver choose the work-group sizes.
        }

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size_ptr, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, NULL));
#endif
    } else {
        unsigned int nth = MIN(64, ne0);
        size_t global_work_size[] = {ne01*nth, (size_t)ne02, (size_t)ne03};
        size_t local_work_size[] = {nth, 1, 1};

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
    }
}

static void ggml_cl_div(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(src1);
    GGML_ASSERT(src1->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    const int ne00 = src0->ne[0];
    const int ne01 = src0->ne[1];
    const int ne02 = src0->ne[2];
    const int ne03 = src0->ne[3];

    const cl_ulong nb00 = src0->nb[0];
    const cl_ulong nb01 = src0->nb[1];
    const cl_ulong nb02 = src0->nb[2];
    const cl_ulong nb03 = src0->nb[3];

    const int ne10 = src1->ne[0];
    const int ne11 = src1->ne[1];
    const int ne12 = src1->ne[2];
    const int ne13 = src1->ne[3];

    const cl_ulong nb10 = src1->nb[0];
    const cl_ulong nb11 = src1->nb[1];
    const cl_ulong nb12 = src1->nb[2];
    const cl_ulong nb13 = src1->nb[3];

    const int ne0  = dst->ne[0];

    const cl_ulong nb0  = dst->nb[0];
    const cl_ulong nb1  = dst->nb[1];
    const cl_ulong nb2  = dst->nb[2];
    const cl_ulong nb3  = dst->nb[3];

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra1 = (ggml_tensor_extra_cl *)src1->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offset1 = extra1->offset + src1->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    bool bcast_row = false;
    cl_kernel kernel;

    if (ggml_nelements(src1) == ne10 && ggml_is_contiguous(src1) && ne00 % 4 == 0 && ne10 % 4 == 0) {
        GGML_ASSERT(ggml_is_contiguous(src0));

        // src1 is a row
        GGML_ASSERT(ne11 == 1);

        bcast_row = true;
        int ne = ne00 / 4;
        kernel = backend_ctx->kernel_div_row;

        CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
        CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
        CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extra1->data_device));
        CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offset1));
        CL_CHECK(clSetKernelArg(kernel, 4, sizeof(cl_mem),   &extrad->data_device));
        CL_CHECK(clSetKernelArg(kernel, 5, sizeof(cl_ulong), &offsetd));
        CL_CHECK(clSetKernelArg(kernel, 6, sizeof(int),      &ne));
    } else {
        kernel = backend_ctx->kernel_div;

        CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0->data_device));
        CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong), &offset0));
        CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
        CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
        CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   &extrad->data_device));
        CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offsetd));
        CL_CHECK(clSetKernelArg(kernel,  6, sizeof(cl_ulong), &nb00));
        CL_CHECK(clSetKernelArg(kernel,  7, sizeof(cl_ulong), &nb01));
        CL_CHECK(clSetKernelArg(kernel,  8, sizeof(cl_ulong), &nb02));
        CL_CHECK(clSetKernelArg(kernel,  9, sizeof(cl_ulong), &nb03));
        CL_CHECK(clSetKernelArg(kernel, 10, sizeof(int),      &ne10));
        CL_CHECK(clSetKernelArg(kernel, 11, sizeof(int),      &ne11));
        CL_CHECK(clSetKernelArg(kernel, 12, sizeof(int),      &ne12));
        CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &ne13));
        CL_CHECK(clSetKernelArg(kernel, 14, sizeof(cl_ulong), &nb10));
        CL_CHECK(clSetKernelArg(kernel, 15, sizeof(cl_ulong), &nb11));
        CL_CHECK(clSetKernelArg(kernel, 16, sizeof(cl_ulong), &nb12));
        CL_CHECK(clSetKernelArg(kernel, 17, sizeof(cl_ulong), &nb13));
        CL_CHECK(clSetKernelArg(kernel, 18, sizeof(int),      &ne0));
        CL_CHECK(clSetKernelArg(kernel, 19, sizeof(cl_ulong), &nb0));
        CL_CHECK(clSetKernelArg(kernel, 20, sizeof(cl_ulong), &nb1));
        CL_CHECK(clSetKernelArg(kernel, 21, sizeof(cl_ulong), &nb2));
        CL_CHECK(clSetKernelArg(kernel, 22, sizeof(cl_ulong), &nb3));
    }

    if (bcast_row) {
        int n = ggml_nelements(dst)/4;
        size_t global_work_size[] = {(size_t)n, 1, 1};
        size_t local_work_size[] = {64, 1, 1};

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
    } else {
        unsigned int nth = MIN(64, ne0);
        size_t global_work_size[] = {ne01*nth, (size_t)ne02, (size_t)ne03};
        size_t local_work_size[] = {nth, 1, 1};

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
    }
}

static void ggml_cl_sub(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(src1);
    GGML_ASSERT(src1->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    const int ne00 = src0->ne[0];
    const int ne01 = src0->ne[1];
    const int ne02 = src0->ne[2];
    const int ne03 = src0->ne[3];

    const cl_ulong nb00 = src0->nb[0];
    const cl_ulong nb01 = src0->nb[1];
    const cl_ulong nb02 = src0->nb[2];
    const cl_ulong nb03 = src0->nb[3];

    const int ne10 = src1->ne[0];
    const int ne11 = src1->ne[1];
    const int ne12 = src1->ne[2];
    const int ne13 = src1->ne[3];

    const cl_ulong nb10 = src1->nb[0];
    const cl_ulong nb11 = src1->nb[1];
    const cl_ulong nb12 = src1->nb[2];
    const cl_ulong nb13 = src1->nb[3];

    const int ne0  = dst->ne[0];

    const cl_ulong nb0  = dst->nb[0];
    const cl_ulong nb1  = dst->nb[1];
    const cl_ulong nb2  = dst->nb[2];
    const cl_ulong nb3  = dst->nb[3];

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra1 = (ggml_tensor_extra_cl *)src1->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offset1 = extra1->offset + src1->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    bool bcast_row = false;
    cl_kernel kernel;

    if (ggml_nelements(src1) == ne10 && ggml_is_contiguous(src1) && ne00 % 4 == 0 && ne10 % 4 == 0) {
        GGML_ASSERT(ggml_is_contiguous(src0));

        // src1 is a row
        GGML_ASSERT(ne11 == 1);

        bcast_row = true;
        int ne = ne00 / 4;
        kernel = backend_ctx->kernel_sub_row;

        CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
        CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
        CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extra1->data_device));
        CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offset1));
        CL_CHECK(clSetKernelArg(kernel, 4, sizeof(cl_mem),   &extrad->data_device));
        CL_CHECK(clSetKernelArg(kernel, 5, sizeof(cl_ulong), &offsetd));
        CL_CHECK(clSetKernelArg(kernel, 6, sizeof(int),      &ne));
    } else {
        kernel = backend_ctx->kernel_sub;

        CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0->data_device));
        CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong), &offset0));
        CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
        CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
        CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   &extrad->data_device));
        CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offsetd));
        CL_CHECK(clSetKernelArg(kernel,  6, sizeof(cl_ulong), &nb00));
        CL_CHECK(clSetKernelArg(kernel,  7, sizeof(cl_ulong), &nb01));
        CL_CHECK(clSetKernelArg(kernel,  8, sizeof(cl_ulong), &nb02));
        CL_CHECK(clSetKernelArg(kernel,  9, sizeof(cl_ulong), &nb03));
        CL_CHECK(clSetKernelArg(kernel, 10, sizeof(int),      &ne10));
        CL_CHECK(clSetKernelArg(kernel, 11, sizeof(int),      &ne11));
        CL_CHECK(clSetKernelArg(kernel, 12, sizeof(int),      &ne12));
        CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &ne13));
        CL_CHECK(clSetKernelArg(kernel, 14, sizeof(cl_ulong), &nb10));
        CL_CHECK(clSetKernelArg(kernel, 15, sizeof(cl_ulong), &nb11));
        CL_CHECK(clSetKernelArg(kernel, 16, sizeof(cl_ulong), &nb12));
        CL_CHECK(clSetKernelArg(kernel, 17, sizeof(cl_ulong), &nb13));
        CL_CHECK(clSetKernelArg(kernel, 18, sizeof(int),      &ne0));
        CL_CHECK(clSetKernelArg(kernel, 19, sizeof(cl_ulong), &nb0));
        CL_CHECK(clSetKernelArg(kernel, 20, sizeof(cl_ulong), &nb1));
        CL_CHECK(clSetKernelArg(kernel, 21, sizeof(cl_ulong), &nb2));
        CL_CHECK(clSetKernelArg(kernel, 22, sizeof(cl_ulong), &nb3));
    }

    if (bcast_row) {
        int n = ggml_nelements(dst)/4;
        size_t global_work_size[] = {(size_t)n, 1, 1};
        size_t local_work_size[] = {64, 1, 1};

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
    } else {
        unsigned int nth = MIN(64, ne0);
        size_t global_work_size[] = {ne01*nth, (size_t)ne02, (size_t)ne03};
        size_t local_work_size[] = {nth, 1, 1};

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
    }
}

static void ggml_cl_gelu(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    UNUSED(src1);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    cl_kernel kernel;

    int n = ggml_nelements(dst);

    if (n % 4 == 0) {
        kernel = backend_ctx->kernel_gelu_4;
        n /= 4;
    } else {
        kernel = backend_ctx->kernel_gelu;
    }

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offsetd));

    size_t global_work_size[] = {(size_t)n, 1, 1};
    size_t local_work_size[] = {64, 1, 1};

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt);

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
    clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL);
#endif
}

static void ggml_cl_gelu_quick(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    UNUSED(src1);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    cl_kernel kernel;

    int n = ggml_nelements(dst);

    if (n % 4 == 0) {
        kernel = backend_ctx->kernel_gelu_quick_4;
        n /= 4;
    } else {
        kernel = backend_ctx->kernel_gelu_quick;
    }

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offsetd));

    size_t global_work_size[] = {(size_t)n, 1, 1};
    size_t local_work_size[] = {64, 1, 1};

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt);

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
    clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL);
#endif
}

static void ggml_cl_silu(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    UNUSED(src1);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    cl_kernel kernel;

    int n = ggml_nelements(dst);

    if (n % 4 == 0) {
        kernel = backend_ctx->kernel_silu_4;
        n /= 4;
    } else {
        kernel = backend_ctx->kernel_silu;
    }

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offsetd));

    size_t global_work_size[] = {(size_t)n, 1, 1};
    size_t local_work_size[] = {64, 1, 1};

    size_t * local_work_size_ptr = local_work_size;
    if (n % 64 != 0 && !backend_ctx->non_uniform_workgroups) {
        local_work_size_ptr = nullptr;  // Let driver choose the work-group sizes.
    }

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size_ptr, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, NULL));
#endif
}

static void ggml_cl_relu(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    UNUSED(src1);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    cl_kernel kernel = backend_ctx->kernel_relu;

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offsetd));

    const int64_t n = ggml_nelements(dst);

    size_t global_work_size[] = {(size_t)n, 1, 1};
    size_t local_work_size[] = {64, 1, 1};

    size_t * local_work_size_ptr = local_work_size;
    if (n % 64 != 0 && !backend_ctx->non_uniform_workgroups) {
        local_work_size_ptr = nullptr;  // Let driver choose the work-group sizes.
    }

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size_ptr, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, NULL));
#endif
}

static void ggml_cl_sigmoid(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    UNUSED(src1);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    cl_kernel kernel;
    if (src0->type == GGML_TYPE_F32 && dst->type == GGML_TYPE_F32) {
        kernel = backend_ctx->kernel_sigmoid_f32;
    } else if (src0->type == GGML_TYPE_F16 && dst->type == GGML_TYPE_F16) {
        kernel = backend_ctx->kernel_sigmoid_f16;
    } else {
        GGML_ASSERT(false && "Unsupported data types for sigmoid (input and output must be both f32 or f16)");
    }

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offsetd));

    const int64_t n = ggml_nelements(dst);

    size_t global_work_size[] = {(size_t)n, 1, 1};
    size_t local_work_size[] = {64, 1, 1};

    size_t * local_work_size_ptr = local_work_size;
    if (n % 64 != 0 && !backend_ctx->non_uniform_workgroups) {
        local_work_size_ptr = nullptr;  // Let driver choose the work-group sizes.
    }

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size_ptr, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, NULL));
#endif
}

static void ggml_cl_clamp(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    UNUSED(src1);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    float min;
    float max;
    memcpy(&min, ((int32_t *) dst->op_params) + 0, sizeof(float));
    memcpy(&max, ((int32_t *) dst->op_params) + 1, sizeof(float));

    cl_kernel kernel = backend_ctx->kernel_clamp;

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offsetd));
    CL_CHECK(clSetKernelArg(kernel, 4, sizeof(float),    &min));
    CL_CHECK(clSetKernelArg(kernel, 5, sizeof(float),    &max));

    const int64_t n = ggml_nelements(dst);

    size_t global_work_size[] = {(size_t)n, 1, 1};
    size_t local_work_size[] = {64, 1, 1};

    size_t * local_work_size_ptr = local_work_size;
    if (n % 64 != 0 && !backend_ctx->non_uniform_workgroups) {
        local_work_size_ptr = nullptr;  // Let driver choose the work-group sizes.
    }

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size_ptr, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, NULL));
#endif
}

static void ggml_cl_norm(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    UNUSED(src1);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    float eps;
    memcpy(&eps, dst->op_params, sizeof(float));

    const int ne00 = src0 ? src0->ne[0] : 0;
    const int ne01 = src0 ? src0->ne[1] : 0;
    const int ne02 = src0 ? src0->ne[2] : 0;
    const int ne03 = src0 ? src0->ne[3] : 0;

    const cl_ulong nb01 = src0 ? src0->nb[1] : 0;
    const cl_ulong nb02 = src0 ? src0->nb[2] : 0;
    const cl_ulong nb03 = src0 ? src0->nb[3] : 0;

    const int nth = MIN(64, ne00);

    cl_kernel kernel = backend_ctx->kernel_norm;

    CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),    &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong),  &offset0));
    CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),    &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong),  &offsetd));
    CL_CHECK(clSetKernelArg(kernel,  4, sizeof(int),       &ne00));
    CL_CHECK(clSetKernelArg(kernel,  5, sizeof(int),       &ne01));
    CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),       &ne02));
    CL_CHECK(clSetKernelArg(kernel,  7, sizeof(int),       &ne03));
    CL_CHECK(clSetKernelArg(kernel,  8, sizeof(cl_ulong),  &nb01));
    CL_CHECK(clSetKernelArg(kernel,  9, sizeof(cl_ulong),  &nb02));
    CL_CHECK(clSetKernelArg(kernel, 10, sizeof(cl_ulong),  &nb03));
    CL_CHECK(clSetKernelArg(kernel, 11, sizeof(float),     &eps));
    CL_CHECK(clSetKernelArg(kernel, 12, sizeof(float)*nth, NULL));

    size_t global_work_size[] = {(size_t)ne01*nth, (size_t)ne02, (size_t)ne03};
    size_t local_work_size[] = {(size_t)nth, 1, 1};

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
}

static void ggml_cl_rms_norm(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    UNUSED(src1);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    //ggml_backend_opencl_device_context * dev_ctx =
    //    (ggml_backend_opencl_device_context *)backend->device->context;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    float eps;
    memcpy(&eps, dst->op_params, sizeof(float));

    const int ne00 = src0 ? src0->ne[0] : 0;
    const int ne01 = src0 ? src0->ne[1] : 0;
    const int ne02 = src0 ? src0->ne[2] : 0;
    const int ne03 = src0 ? src0->ne[3] : 0;

    const cl_ulong nb01 = src0 ? src0->nb[1] : 0;
    const cl_ulong nb02 = src0 ? src0->nb[2] : 0;
    const cl_ulong nb03 = src0 ? src0->nb[3] : 0;

    GGML_ASSERT(ne00 % 4 == 0);

    const int nth = MIN(64, ne00);

    size_t global_work_size[] = {(size_t)ne01*nth, (size_t)ne02, (size_t)ne03};
    size_t local_work_size[] = {(size_t)nth, 1, 1};

    cl_kernel kernel = backend_ctx->kernel_rms_norm;

    // Note, this kernel declares local memory in kernel args and the size
    // depends on subgroup size.
    // Note, this requires OpenCL 2.1 and above
    // For now we use fixed subgroup size to simplify support for OpenCL 2.0.
    size_t sgs;
    //CL_CHECK(clGetKernelSubGroupInfo(kernel, dev_ctx->device,
    //    CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE,
    //    sizeof(local_work_size), local_work_size,
    //    sizeof(size_t), &sgs, NULL));
    if (backend_ctx->gpu_family == ADRENO) {
        sgs = 64;
    } else if (backend_ctx->gpu_family == INTEL) {
        sgs = 32;
    } else {
        GGML_ASSERT(false && "Unsupported GPU");
    }

    CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),    &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong),  &offset0));
    CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),    &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong),  &offsetd));
    CL_CHECK(clSetKernelArg(kernel,  4, sizeof(int),       &ne00));
    CL_CHECK(clSetKernelArg(kernel,  5, sizeof(int),       &ne01));
    CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),       &ne02));
    CL_CHECK(clSetKernelArg(kernel,  7, sizeof(int),       &ne03));
    CL_CHECK(clSetKernelArg(kernel,  8, sizeof(cl_ulong),  &nb01));
    CL_CHECK(clSetKernelArg(kernel,  9, sizeof(cl_ulong),  &nb02));
    CL_CHECK(clSetKernelArg(kernel, 10, sizeof(cl_ulong),  &nb03));
    CL_CHECK(clSetKernelArg(kernel, 11, sizeof(float),     &eps));
    // This is local memory - the size depends on subgroup size.
    CL_CHECK(clSetKernelArg(kernel, 12, sizeof(float)*nth/sgs,  NULL));

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
}

static void ggml_cl_group_norm(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    UNUSED(src1);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    int32_t n_groups   = ((const int32_t *) dst->op_params)[0];
    int32_t group_size = src0->ne[0] * src0->ne[1] * ((src0->ne[2] + n_groups - 1) / n_groups);
    float   eps        = ((const float *) dst->op_params)[1];

    const int ne00 = src0->ne[0];
    const int ne01 = src0->ne[1];
    const int ne02 = src0->ne[2];
    const int ne = ne00*ne01*ne02;

    cl_kernel kernel = backend_ctx->kernel_group_norm;

    size_t sgs = 64;
    if (backend_ctx->gpu_family == ADRENO) {
        sgs = 64;
    } else if (backend_ctx->gpu_family == INTEL) {
        sgs = 32;
    } else {
        GGML_ASSERT(false && "Unsupported GPU");
    }

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offsetd));
    CL_CHECK(clSetKernelArg(kernel, 4, sizeof(int),      &ne));
    CL_CHECK(clSetKernelArg(kernel, 5, sizeof(int),      &group_size));
    CL_CHECK(clSetKernelArg(kernel, 6, sizeof(float),    &eps));

    size_t global_work_size[] = {(size_t)n_groups*sgs, 1, 1};
    size_t local_work_size[] = {(size_t)sgs, 1, 1};

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
}

static void ggml_cl_tanh(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    UNUSED(src1);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0_abs = extra0->offset + src0->view_offs;
    cl_ulong offsetd_abs = extrad->offset + dst->view_offs;

    cl_kernel kernel;
    if (dst->type == GGML_TYPE_F32) {
        kernel = backend_ctx->kernel_tanh_f32_nd;
    } else if (dst->type == GGML_TYPE_F16) {
        kernel = backend_ctx->kernel_tanh_f16_nd;
    } else {
        GGML_ASSERT(false && "Unsupported type for ggml_cl_tanh");
    }
    GGML_ASSERT(kernel != nullptr);

    const int ne00 = src0->ne[0]; const int ne01 = src0->ne[1]; const int ne02 = src0->ne[2]; const int ne03 = src0->ne[3];
    const cl_ulong nb00 = src0->nb[0]; const cl_ulong nb01 = src0->nb[1]; const cl_ulong nb02 = src0->nb[2]; const cl_ulong nb03 = src0->nb[3];

    const int ne10 = dst->ne[0]; const int ne11 = dst->ne[1]; const int ne12 = dst->ne[2]; const int ne13 = dst->ne[3];
    const cl_ulong nb10 = dst->nb[0]; const cl_ulong nb11 = dst->nb[1]; const cl_ulong nb12 = dst->nb[2]; const cl_ulong nb13 = dst->nb[3];

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0_abs));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offsetd_abs));

    CL_CHECK(clSetKernelArg(kernel, 4, sizeof(int),      &ne00));
    CL_CHECK(clSetKernelArg(kernel, 5, sizeof(int),      &ne01));
    CL_CHECK(clSetKernelArg(kernel, 6, sizeof(int),      &ne02));
    CL_CHECK(clSetKernelArg(kernel, 7, sizeof(int),      &ne03));
    CL_CHECK(clSetKernelArg(kernel, 8, sizeof(cl_ulong), &nb00));
    CL_CHECK(clSetKernelArg(kernel, 9, sizeof(cl_ulong), &nb01));
    CL_CHECK(clSetKernelArg(kernel, 10, sizeof(cl_ulong),&nb02));
    CL_CHECK(clSetKernelArg(kernel, 11, sizeof(cl_ulong),&nb03));

    CL_CHECK(clSetKernelArg(kernel, 12, sizeof(int),     &ne10));
    CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),     &ne11));
    CL_CHECK(clSetKernelArg(kernel, 14, sizeof(int),     &ne12));
    CL_CHECK(clSetKernelArg(kernel, 15, sizeof(int),     &ne13));
    CL_CHECK(clSetKernelArg(kernel, 16, sizeof(cl_ulong),&nb10));
    CL_CHECK(clSetKernelArg(kernel, 17, sizeof(cl_ulong),&nb11));
    CL_CHECK(clSetKernelArg(kernel, 18, sizeof(cl_ulong),&nb12));
    CL_CHECK(clSetKernelArg(kernel, 19, sizeof(cl_ulong),&nb13));

    size_t global_work_size[3];
    if (ne10 == 0 || ne11 == 0 || ne12 == 0 || ne13 == 0) { // Handle case of 0 elements
        return;
    }
    global_work_size[0] = (size_t)ne10;
    global_work_size[1] = (size_t)ne11;
    global_work_size[2] = (size_t)ne12;

    size_t lws0 = 16, lws1 = 4, lws2 = 1;
    if (ne10 < 16) lws0 = ne10;
    if (ne11 < 4) lws1 = ne11;
    if (ne12 < 1) lws2 = ne12 > 0 ? ne12 : 1;

    while (lws0 * lws1 * lws2 > 256 && lws0 > 1) lws0 /= 2;
    while (lws0 * lws1 * lws2 > 256 && lws1 > 1) lws1 /= 2;
    while (lws0 * lws1 * lws2 > 256 && lws2 > 1) lws2 /= 2;


    size_t local_work_size[] = {lws0, lws1, lws2};

    size_t* local_work_size_ptr = local_work_size;
    if (!backend_ctx->non_uniform_workgroups) {
        if (global_work_size[0] % local_work_size[0] != 0 ||
            global_work_size[1] % local_work_size[1] != 0 ||
            global_work_size[2] % local_work_size[2] != 0) {
            local_work_size_ptr = NULL;
        }
    }
    if (global_work_size[0] == 0 || global_work_size[1] == 0 || global_work_size[2] == 0) return;


#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size_ptr ? local_work_size : (size_t[3]){0,0,0}, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, NULL));
#endif
}

static void ggml_cl_repeat(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1_shape_def, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);
    GGML_ASSERT(dst->type == src0->type);

    UNUSED(src1_shape_def);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    if (backend_ctx->kernel_repeat == nullptr) {
        GGML_LOG_WARN("%s: repeat kernel not available, skipping OpenCL execution.\n", __func__);
        return;
    }

    ggml_tensor_extra_cl * extra_src0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra_dst  = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong off_src0 = extra_src0->offset + src0->view_offs;
    cl_ulong off_dst  = extra_dst->offset  + dst->view_offs;

    const int src0_ne0 = src0->ne[0]; const int src0_ne1 = src0->ne[1]; const int src0_ne2 = src0->ne[2]; const int src0_ne3 = src0->ne[3];
    const cl_ulong src0_nb0 = src0->nb[0]; const cl_ulong src0_nb1 = src0->nb[1]; const cl_ulong src0_nb2 = src0->nb[2]; const cl_ulong src0_nb3 = src0->nb[3];

    const int dst_ne0 = dst->ne[0]; const int dst_ne1 = dst->ne[1]; const int dst_ne2 = dst->ne[2]; const int dst_ne3 = dst->ne[3];
    const cl_ulong dst_nb0 = dst->nb[0]; const cl_ulong dst_nb1 = dst->nb[1]; const cl_ulong dst_nb2 = dst->nb[2]; const cl_ulong dst_nb3 = dst->nb[3];

    cl_kernel kernel = backend_ctx->kernel_repeat;

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),    &extra_src0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_mem),    &extra_dst->data_device));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_ulong),  &off_src0));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong),  &off_dst));
    CL_CHECK(clSetKernelArg(kernel, 4, sizeof(int),       &src0_ne0));
    CL_CHECK(clSetKernelArg(kernel, 5, sizeof(int),       &src0_ne1));
    CL_CHECK(clSetKernelArg(kernel, 6, sizeof(int),       &src0_ne2));
    CL_CHECK(clSetKernelArg(kernel, 7, sizeof(int),       &src0_ne3));
    CL_CHECK(clSetKernelArg(kernel, 8, sizeof(cl_ulong),  &src0_nb0));
    CL_CHECK(clSetKernelArg(kernel, 9, sizeof(cl_ulong),  &src0_nb1));
    CL_CHECK(clSetKernelArg(kernel, 10, sizeof(cl_ulong), &src0_nb2));
    CL_CHECK(clSetKernelArg(kernel, 11, sizeof(cl_ulong), &src0_nb3));
    CL_CHECK(clSetKernelArg(kernel, 12, sizeof(int),      &dst_ne0));
    CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &dst_ne1));
    CL_CHECK(clSetKernelArg(kernel, 14, sizeof(int),      &dst_ne2));
    CL_CHECK(clSetKernelArg(kernel, 15, sizeof(int),      &dst_ne3));
    CL_CHECK(clSetKernelArg(kernel, 16, sizeof(cl_ulong), &dst_nb0));
    CL_CHECK(clSetKernelArg(kernel, 17, sizeof(cl_ulong), &dst_nb1));
    CL_CHECK(clSetKernelArg(kernel, 18, sizeof(cl_ulong), &dst_nb2));
    CL_CHECK(clSetKernelArg(kernel, 19, sizeof(cl_ulong), &dst_nb3));

    size_t gws0 = dst_ne1 > 0 ? (size_t)dst_ne1 : 1;
    size_t gws1 = dst_ne2 > 0 ? (size_t)dst_ne2 : 1;
    size_t gws2 = dst_ne3 > 0 ? (size_t)dst_ne3 : 1;

    size_t global_work_size[] = { gws0, gws1, gws2 };

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, NULL, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, (size_t[3]){0,0,0}, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, NULL, 0, NULL, NULL));
#endif
}

static void ggml_cl_pad(ggml_backend_t backend, const ggml_tensor * src0, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);
    GGML_ASSERT(src0->type == GGML_TYPE_F32);
    GGML_ASSERT(dst->type == GGML_TYPE_F32);
    GGML_ASSERT(src0->ne[3] == 1 && dst->ne[3] == 1);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    if (backend_ctx->kernel_pad == nullptr) {
        GGML_LOG_WARN("%s: pad kernel not available, skipping OpenCL execution.\n", __func__);
        return;
    }

    ggml_tensor_extra_cl * extra_src0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra_dst  = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong off_src0 = extra_src0->offset + src0->view_offs;
    cl_ulong off_dst  = extra_dst->offset  + dst->view_offs;

    const int s_ne0 = src0->ne[0];
    const int s_ne1 = src0->ne[1];
    const int s_ne2 = src0->ne[2];

    const int d_ne0 = dst->ne[0];
    const int d_ne1 = dst->ne[1];
    const int d_ne2 = dst->ne[2];

    cl_kernel kernel = backend_ctx->kernel_pad;

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),    &extra_src0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong),  &off_src0));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),    &extra_dst->data_device));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong),  &off_dst));
    CL_CHECK(clSetKernelArg(kernel, 4, sizeof(int),       &s_ne0));
    CL_CHECK(clSetKernelArg(kernel, 5, sizeof(int),       &s_ne1));
    CL_CHECK(clSetKernelArg(kernel, 6, sizeof(int),       &s_ne2));
    CL_CHECK(clSetKernelArg(kernel, 7, sizeof(int),       &d_ne0));
    CL_CHECK(clSetKernelArg(kernel, 8, sizeof(int),       &d_ne1));
    CL_CHECK(clSetKernelArg(kernel, 9, sizeof(int),       &d_ne2));

    size_t lws0 = 64;
    size_t gws0 = (( (size_t)d_ne0 + lws0 - 1 ) / lws0) * lws0;

    size_t global_work_size[] = { gws0, (size_t)d_ne1, (size_t)d_ne2 };
    size_t local_work_size[]  = { lws0, 1, 1 };

    size_t * local_work_size_ptr = local_work_size;
     if (d_ne0 % lws0 != 0 && !backend_ctx->non_uniform_workgroups) {
        local_work_size_ptr = nullptr;
    }

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size_ptr ? local_work_size : (size_t[3]){0,0,0}, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, NULL));
#endif
}

static void ggml_cl_upscale(ggml_backend_t backend, const ggml_tensor * src0, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);
    GGML_ASSERT(src0->type == GGML_TYPE_F32);
    GGML_ASSERT(dst->type == GGML_TYPE_F32);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    const ggml_scale_mode mode = (ggml_scale_mode) ggml_get_op_params_i32(dst, 0);
    cl_kernel kernel = nullptr;

    if (mode == GGML_SCALE_MODE_NEAREST) {
        kernel = backend_ctx->kernel_upscale;
        if (kernel == nullptr) {
            GGML_LOG_WARN("%s: nearest upscale kernel not available, skipping OpenCL execution.\n", __func__);
            return;
        }
    } else if (mode == GGML_SCALE_MODE_BILINEAR) {
        kernel = backend_ctx->kernel_upscale_bilinear;
        if (kernel == nullptr) {
            GGML_LOG_WARN("%s: bilinear upscale kernel not available, skipping OpenCL execution.\n", __func__);
            return;
        }
    } else {
        GGML_LOG_WARN("%s: unsupported upscale mode %d, skipping OpenCL execution.\n", __func__, mode);
        return;
    }

    ggml_tensor_extra_cl * extra_src0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra_dst  = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong off_src0 = extra_src0->offset + src0->view_offs;
    cl_ulong off_dst  = extra_dst->offset  + dst->view_offs;

    const cl_ulong nb00 = src0->nb[0];
    const cl_ulong nb01 = src0->nb[1];
    const cl_ulong nb02 = src0->nb[2];
    const cl_ulong nb03 = src0->nb[3];

    const int ne00_src = src0->ne[0];
    const int ne01_src = src0->ne[1];

    const int ne10_dst = dst->ne[0];
    const int ne11_dst = dst->ne[1];
    const int ne12_dst = dst->ne[2];
    const int ne13_dst = dst->ne[3];

    const float sf0 = (float)dst->ne[0] / src0->ne[0];
    const float sf1 = (float)dst->ne[1] / src0->ne[1];
    const float sf2 = (float)dst->ne[2] / src0->ne[2];
    const float sf3 = (float)dst->ne[3] / src0->ne[3];

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),    &extra_src0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong),  &off_src0));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),    &extra_dst->data_device));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong),  &off_dst));
    CL_CHECK(clSetKernelArg(kernel, 4, sizeof(cl_ulong),  &nb00));
    CL_CHECK(clSetKernelArg(kernel, 5, sizeof(cl_ulong),  &nb01));
    CL_CHECK(clSetKernelArg(kernel, 6, sizeof(cl_ulong),  &nb02));
    CL_CHECK(clSetKernelArg(kernel, 7, sizeof(cl_ulong),  &nb03));

    if (mode == GGML_SCALE_MODE_NEAREST) {
        CL_CHECK(clSetKernelArg(kernel, 8, sizeof(int),       &ne10_dst));
        CL_CHECK(clSetKernelArg(kernel, 9, sizeof(int),       &ne11_dst));
        CL_CHECK(clSetKernelArg(kernel, 10, sizeof(int),      &ne12_dst));
        CL_CHECK(clSetKernelArg(kernel, 11, sizeof(int),      &ne13_dst));
        CL_CHECK(clSetKernelArg(kernel, 12, sizeof(float),    &sf0));
        CL_CHECK(clSetKernelArg(kernel, 13, sizeof(float),    &sf1));
        CL_CHECK(clSetKernelArg(kernel, 14, sizeof(float),    &sf2));
        CL_CHECK(clSetKernelArg(kernel, 15, sizeof(float),    &sf3));
    } else if (mode == GGML_SCALE_MODE_BILINEAR) {
        CL_CHECK(clSetKernelArg(kernel, 8, sizeof(int),       &ne00_src));
        CL_CHECK(clSetKernelArg(kernel, 9, sizeof(int),       &ne01_src));
        CL_CHECK(clSetKernelArg(kernel, 10, sizeof(int),      &ne10_dst));
        CL_CHECK(clSetKernelArg(kernel, 11, sizeof(int),      &ne11_dst));
        CL_CHECK(clSetKernelArg(kernel, 12, sizeof(int),      &ne12_dst));
        CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &ne13_dst));
        CL_CHECK(clSetKernelArg(kernel, 14, sizeof(float),    &sf0));
        CL_CHECK(clSetKernelArg(kernel, 15, sizeof(float),    &sf1));
        CL_CHECK(clSetKernelArg(kernel, 16, sizeof(float),    &sf2));
        CL_CHECK(clSetKernelArg(kernel, 17, sizeof(float),    &sf3));
    }


    size_t dst_total_elements = (size_t)ne10_dst * ne11_dst * ne12_dst * ne13_dst;
    if (dst_total_elements == 0) {
        return;
    }
    size_t global_work_size[] = { dst_total_elements, 1, 1 };
    size_t local_work_size_pref = 256;
    size_t local_work_size[] = { MIN(local_work_size_pref, dst_total_elements), 1, 1};

    size_t * local_work_size_ptr = local_work_size;
    if (dst_total_elements % local_work_size[0] != 0 && !backend_ctx->non_uniform_workgroups) {
        local_work_size_ptr = nullptr;
    }

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 1, NULL, global_work_size, local_work_size_ptr, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    size_t profiling_gws[3] = {global_work_size[0], 1, 1};
    size_t profiling_lws[3] = {local_work_size_ptr ? local_work_size[0] : 0, 1, 1};
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, profiling_gws, profiling_lws, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 1, NULL, global_work_size, local_work_size_ptr, 0, NULL, NULL));
#endif
}

static void ggml_cl_concat(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(src1);
    GGML_ASSERT(src1->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);
    GGML_ASSERT(src0->type == GGML_TYPE_F32);
    GGML_ASSERT(src1->type == GGML_TYPE_F32);
    GGML_ASSERT(dst->type == GGML_TYPE_F32);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    if (backend_ctx->kernel_concat_f32_contiguous == nullptr || backend_ctx->kernel_concat_f32_non_contiguous == nullptr) {
        GGML_LOG_WARN("%s: concat kernels not available, skipping OpenCL execution.\n", __func__);
        return;
    }

    ggml_tensor_extra_cl * extra0_cl = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra1_cl = (ggml_tensor_extra_cl *)src1->extra;
    ggml_tensor_extra_cl * extrad_cl = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong off_src0 = extra0_cl->offset + src0->view_offs;
    cl_ulong off_src1 = extra1_cl->offset + src1->view_offs;
    cl_ulong off_dst  = extrad_cl->offset + dst->view_offs;

    const int32_t dim = ((const int32_t *) dst->op_params)[0];
    GGML_ASSERT(dim >= 0 && dim <= 3);

    if (ggml_is_contiguous(src0) && ggml_is_contiguous(src1) && ggml_is_contiguous(dst)) {
        if (dim == 3) {

            size_t nbytes_src0 = ggml_nbytes(src0);
            size_t nbytes_src1 = ggml_nbytes(src1);

            CL_CHECK(clEnqueueCopyBuffer(queue, extra0_cl->data_device, extrad_cl->data_device,
                                         off_src0, off_dst, nbytes_src0, 0, NULL, NULL));
            CL_CHECK(clEnqueueCopyBuffer(queue, extra1_cl->data_device, extrad_cl->data_device,
                                         off_src1, off_dst + nbytes_src0, nbytes_src1, 0, NULL, NULL));
        } else {

            cl_kernel kernel = backend_ctx->kernel_concat_f32_contiguous;
            size_t global_work_size[3];

            for (int i3 = 0; i3 < dst->ne[3]; ++i3) {
                cl_ulong current_off_src0 = off_src0 + (i3 * src0->nb[3]);
                cl_ulong current_off_src1 = off_src1 + (i3 * src1->nb[3]);
                cl_ulong current_off_dst  = off_dst  + (i3 * dst->nb[3]);

                int d_ne00 = src0->ne[0]; int d_ne01 = src0->ne[1]; int d_ne02 = src0->ne[2];
                int d_ne10 = src1->ne[0]; int d_ne11 = src1->ne[1]; int d_ne12 = src1->ne[2];
                int d_ne0  = dst->ne[0];  int d_ne1  = dst->ne[1];  int d_ne2  = dst->ne[2];

                CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),    &extra0_cl->data_device));
                CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong),  &current_off_src0));
                CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),    &extra1_cl->data_device));
                CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong),  &current_off_src1));
                CL_CHECK(clSetKernelArg(kernel, 4, sizeof(cl_mem),    &extrad_cl->data_device));
                CL_CHECK(clSetKernelArg(kernel, 5, sizeof(cl_ulong),  &current_off_dst));
                CL_CHECK(clSetKernelArg(kernel, 6, sizeof(int),       &d_ne00));
                CL_CHECK(clSetKernelArg(kernel, 7, sizeof(int),       &d_ne01));
                CL_CHECK(clSetKernelArg(kernel, 8, sizeof(int),       &d_ne02));
                CL_CHECK(clSetKernelArg(kernel, 9, sizeof(int),       &d_ne10));
                CL_CHECK(clSetKernelArg(kernel, 10, sizeof(int),      &d_ne11));
                CL_CHECK(clSetKernelArg(kernel, 11, sizeof(int),      &d_ne12));
                CL_CHECK(clSetKernelArg(kernel, 12, sizeof(int),      &d_ne0));
                CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &d_ne1));
                CL_CHECK(clSetKernelArg(kernel, 14, sizeof(int),      &d_ne2));
                CL_CHECK(clSetKernelArg(kernel, 15, sizeof(int),      &dim));

                global_work_size[0] = d_ne0;
                global_work_size[1] = d_ne1;
                global_work_size[2] = d_ne2;

                CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, NULL, 0, NULL, NULL));
            }
        }
    } else {
        cl_kernel kernel = backend_ctx->kernel_concat_f32_non_contiguous;

        long ne00 = src0->ne[0], ne01 = src0->ne[1], ne02 = src0->ne[2], ne03 = src0->ne[3];
        cl_ulong nb00 = src0->nb[0], nb01 = src0->nb[1], nb02 = src0->nb[2], nb03 = src0->nb[3];

        cl_ulong nb10 = src1->nb[0], nb11 = src1->nb[1], nb12 = src1->nb[2], nb13 = src1->nb[3];

        long d_ne0 = dst->ne[0], d_ne1 = dst->ne[1], d_ne2 = dst->ne[2], d_ne3 = dst->ne[3];
        cl_ulong d_nb0 = dst->nb[0], d_nb1 = dst->nb[1], d_nb2 = dst->nb[2], d_nb3 = dst->nb[3];


        CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),    &extra0_cl->data_device));
        CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong),  &off_src0));
        CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),    &extra1_cl->data_device));
        CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong),  &off_src1));
        CL_CHECK(clSetKernelArg(kernel, 4, sizeof(cl_mem),    &extrad_cl->data_device));
        CL_CHECK(clSetKernelArg(kernel, 5, sizeof(cl_ulong),  &off_dst));

        CL_CHECK(clSetKernelArg(kernel, 6, sizeof(long),      &ne00));
        CL_CHECK(clSetKernelArg(kernel, 7, sizeof(long),      &ne01));
        CL_CHECK(clSetKernelArg(kernel, 8, sizeof(long),      &ne02));
        CL_CHECK(clSetKernelArg(kernel, 9, sizeof(long),      &ne03));
        CL_CHECK(clSetKernelArg(kernel, 10, sizeof(cl_ulong),    &nb00));
        CL_CHECK(clSetKernelArg(kernel, 11, sizeof(cl_ulong),    &nb01));
        CL_CHECK(clSetKernelArg(kernel, 12, sizeof(cl_ulong),    &nb02));
        CL_CHECK(clSetKernelArg(kernel, 13, sizeof(cl_ulong),    &nb03));

        CL_CHECK(clSetKernelArg(kernel, 14, sizeof(cl_ulong),    &nb10));
        CL_CHECK(clSetKernelArg(kernel, 15, sizeof(cl_ulong),    &nb11));
        CL_CHECK(clSetKernelArg(kernel, 16, sizeof(cl_ulong),    &nb12));
        CL_CHECK(clSetKernelArg(kernel, 17, sizeof(cl_ulong),    &nb13));

        CL_CHECK(clSetKernelArg(kernel, 18, sizeof(long),     &d_ne0));
        CL_CHECK(clSetKernelArg(kernel, 19, sizeof(long),     &d_ne1));
        CL_CHECK(clSetKernelArg(kernel, 20, sizeof(long),     &d_ne2));
        CL_CHECK(clSetKernelArg(kernel, 21, sizeof(long),     &d_ne3));
        CL_CHECK(clSetKernelArg(kernel, 22, sizeof(cl_ulong),    &d_nb0));
        CL_CHECK(clSetKernelArg(kernel, 23, sizeof(cl_ulong),    &d_nb1));
        CL_CHECK(clSetKernelArg(kernel, 24, sizeof(cl_ulong),    &d_nb2));
        CL_CHECK(clSetKernelArg(kernel, 25, sizeof(cl_ulong),    &d_nb3));
        CL_CHECK(clSetKernelArg(kernel, 26, sizeof(int),      &dim));

        size_t global_work_size_nc[] = { d_ne1 > 0 ? (size_t)d_ne1 : 1,
                                         d_ne2 > 0 ? (size_t)d_ne2 : 1,
                                         d_ne3 > 0 ? (size_t)d_ne3 : 1 };

        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size_nc, NULL, 0, NULL, NULL));
    }
}

static void ggml_cl_timestep_embedding(ggml_backend_t backend, const ggml_tensor * src0, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);
    GGML_ASSERT(src0->type == GGML_TYPE_F32);
    GGML_ASSERT(dst->type == GGML_TYPE_F32);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    if (backend_ctx->kernel_timestep_embedding == nullptr) {
        GGML_LOG_WARN("%s: timestep_embedding kernel not available, skipping OpenCL execution.\n", __func__);
        return;
    }

    ggml_tensor_extra_cl * extra_src0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra_dst  = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong off_src0 = extra_src0->offset + src0->view_offs;
    cl_ulong off_dst  = extra_dst->offset  + dst->view_offs;

    const int logical_dim = dst->op_params[0];
    const int max_period  = dst->op_params[1];
    const int dst_nb1_bytes = dst->nb[1];

    cl_kernel kernel = backend_ctx->kernel_timestep_embedding;

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),    &extra_src0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong),  &off_src0));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),    &extra_dst->data_device));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong),  &off_dst));
    CL_CHECK(clSetKernelArg(kernel, 4, sizeof(int),       &dst_nb1_bytes));
    CL_CHECK(clSetKernelArg(kernel, 5, sizeof(int),       &logical_dim));
    CL_CHECK(clSetKernelArg(kernel, 6, sizeof(int),       &max_period));

    size_t gws0 = (size_t)(((logical_dim + 1) / 2) + 1);

    size_t gws1 = (size_t)src0->ne[0];

    size_t global_work_size[] = {gws0, gws1, 1};

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global_work_size, NULL, 0, NULL, &evt)); // Pass 2 for 2D problem

    g_profiling_info.emplace_back();
    size_t profiling_gws[3] = {global_work_size[0], global_work_size[1], 1};
    size_t profiling_lws[3] = {0,0,0}; // Reflects NULL LWS
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, profiling_gws, profiling_lws, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global_work_size, NULL, 0, NULL, NULL)); // Pass 2 for 2D problem
#endif
}

static void ggml_cl_mul_mat(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(src1);
    GGML_ASSERT(src1->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    const enum ggml_type src0t = src0 ? src0->type : GGML_TYPE_COUNT;
    const enum ggml_type src1t = src1 ? src1->type : GGML_TYPE_COUNT;

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra1 = (ggml_tensor_extra_cl *)src1->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offset1 = extra1->offset + src1->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

#ifdef GGML_OPENCL_SOA_Q
    ggml_tensor_extra_cl_q4_0 * extra0_q4_0 = (ggml_tensor_extra_cl_q4_0 *)src0->extra;
#endif

    const int  ne00 = src0 ? src0->ne[0] : 0;
    const int  ne01 = src0 ? src0->ne[1] : 0;
    const int  ne02 = src0 ? src0->ne[2] : 0;
    const int  ne03 = src0 ? src0->ne[3] : 0;

    const cl_ulong nb00 = src0 ? src0->nb[0] : 0;
    const cl_ulong nb01 = src0 ? src0->nb[1] : 0;
    const cl_ulong nb02 = src0 ? src0->nb[2] : 0;
    const cl_ulong nb03 = src0 ? src0->nb[3] : 0;

    const int  ne10 = src1 ? src1->ne[0] : 0;
    const int  ne11 = src1 ? src1->ne[1] : 0;
    const int  ne12 = src1 ? src1->ne[2] : 0;
    const int  ne13 = src1 ? src1->ne[3] : 0;

    const cl_ulong nb10 = src1 ? src1->nb[0] : 0;
    const cl_ulong nb11 = src1 ? src1->nb[1] : 0;
    const cl_ulong nb12 = src1 ? src1->nb[2] : 0;
    const cl_ulong nb13 = src1 ? src1->nb[3] : 0;

    const int  ne0 = dst ? dst->ne[0] : 0;
    const int  ne1 = dst ? dst->ne[1] : 0;

    int r2 = ne12/ne02;
    int r3 = ne13/ne03;

    GGML_ASSERT(ne00 == ne10);

    int nth0 = 32;
    int nth1 = 1;
    int nrows = 1;
    // The number of values produced by each subgroup
    int ndst = 4;

    cl_kernel kernel;

#ifdef GGML_OPENCL_USE_ADRENO_KERNELS
    cl_context context = backend_ctx->context;

    if (ne01 && ne1 && use_adreno_kernels(backend_ctx, src0)) {

    // init CL objects
    // <--------------------------------------------> //
    cl_int              status;
    cl_image_format     img_fmt_1d;
    cl_image_desc       img_desc_1d;
    cl_buffer_region    region;
    cl_mem              A_image1d = nullptr;
    cl_mem              B_image1d = nullptr;
    cl_mem              B_sub_buffer = nullptr;
    cl_mem              C_d = nullptr;
    // for B transpose
    cl_mem B_d = nullptr;
    cl_mem B_d_input_image = nullptr;
    // <--------------------------------------------> //

    // define matrix dimensions
    // <--------------------------------------------> //
    int M = ne01;
    int N = ne1;
    int K = ne00;
    int padding;
    // <--------------------------------------------> //

    // q4_0 x fp32
    if(src0t == GGML_TYPE_Q4_0 && src1t == GGML_TYPE_F32) {
        // TODO: remove duplicate definitions of image description + format -- move to top

        // create an image for A
        // <--------------------------------------------> //
        if (N == 1) {
            img_fmt_1d = { CL_R, CL_UNSIGNED_INT32};
        } else {
            img_fmt_1d = { CL_R, CL_FLOAT};
        }
        memset(&img_desc_1d, 0, sizeof(img_desc_1d));
        img_desc_1d.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
        img_desc_1d.image_width = M * K / 2 / 4;    // Divide by 4 for char -> float
        img_desc_1d.buffer = extra0_q4_0->q;
        A_image1d = clCreateImage(
            context,
            CL_MEM_READ_ONLY,
            &img_fmt_1d,
            &img_desc_1d,
            NULL,
            &status);
        CL_CHECK(status);
        // <--------------------------------------------> //


        // create a sub_buffer for B
        // <--------------------------------------------> //
        region.origin = (extra1->offset);
        region.size = K * N * sizeof(float);
        B_sub_buffer = clCreateSubBuffer(
            extra1->data_device,
            0,
            CL_BUFFER_CREATE_TYPE_REGION,
            &region,
            &status);
        CL_CHECK(status);
        // <--------------------------------------------> //

        // transpose activation for Skyler's gemm
        if (N != 1) {
            //how many extra elements beyond multiple of 8
            int extra_elements = N % 8;

            //how much padding to add
            padding = 0;
            if (extra_elements > 0){
                padding = 8 - extra_elements;
            }

            // Specify the starting offset (in bytes)
            region.origin = 0;
            // Specify the size of the sub-buffer (divide by 2 for FP16)
            region.size = K * (N + padding) * sizeof(float)/2;
            B_d = clCreateSubBuffer(
                backend_ctx->B_d_max,
                0,
                CL_BUFFER_CREATE_TYPE_REGION,
                &region,
                &status);
            CL_CHECK(status);

            cl_image_format image_format_B_d_input = { CL_RGBA, CL_FLOAT };
            cl_image_desc image_desc_B_d_input = {
                CL_MEM_OBJECT_IMAGE1D_BUFFER,
                static_cast<size_t>(K * N / 4),
                0, 0, 0, 0, 0, 0, 0, { B_sub_buffer }
            };
            B_d_input_image = clCreateImage(
                context,
                0,
                &image_format_B_d_input,
                &image_desc_B_d_input,
                NULL,
                &status);
            CL_CHECK(status);

            cl_image_format image_format_B_d_output = { CL_RGBA, CL_HALF_FLOAT }; //(CL_HALF_FLOAT for FP16)
            cl_image_desc image_desc_B_d_output = {
                CL_MEM_OBJECT_IMAGE1D_BUFFER,
                static_cast<size_t>(K * (N + padding)/4),
                0, 0, 0, 0, 0, 0, 0, { B_d }
            };
            B_image1d = clCreateImage(
                context,
                0,
                &image_format_B_d_output,
                &image_desc_B_d_output,
                NULL,
                &status);
            CL_CHECK(status);

            int height_B = N/4;
            if (height_B == 0) {
                height_B = 1;
            }
            int width_B = K/4;
            int padded_height_B = (N + padding)/4;

            kernel = backend_ctx->kernel_transpose_32_16;
            CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem), &B_d_input_image));
            CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_mem), &B_image1d));
            CL_CHECK(clSetKernelArg(kernel, 2, sizeof(int),    &height_B));
            CL_CHECK(clSetKernelArg(kernel, 3, sizeof(int),    &width_B));
            CL_CHECK(clSetKernelArg(kernel, 4, sizeof(int),    &padded_height_B));

            size_t local_size_t[2] = { 1, 16 };
            //WGS tuning
            if (ne0 == 4096 && ne1 == 128 && ne10 == 4096) {
                local_size_t[0]=4;
                local_size_t[1]=8;
            } else if (ne0 == 11008 && ne1 == 128 && ne10 == 4096) {
                local_size_t[0]=2;
                local_size_t[1]=8;
            } else if(ne0 == 4096 && ne1 == 128 && ne10 == 11008) {
                local_size_t[0]=1;
                local_size_t[1]=8;
            } else if(ne0 == 32000 && ne1 == 128 && ne10 == 4096) {
                local_size_t[0]=2;
                local_size_t[1]=8;
            }

            size_t global_size_t[2] = {
                static_cast<size_t>(width_B),
                static_cast<size_t>(padded_height_B)
            };

            #ifdef GGML_OPENCL_PROFILING
                cl_event evt;
                CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global_size_t, local_size_t, 0, NULL, &evt));

                g_profiling_info.emplace_back();
                populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_size_t, local_size_t, dst);
            #else
                CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 2, NULL, global_size_t, local_size_t, 0, NULL, NULL));
            #endif
        } else {
            // no need to transpose B in other cases
            // create an image for B from sub_buffer
            // <--------------------------------------------> //
            img_fmt_1d = {CL_RGBA, CL_FLOAT};

            memset(&img_desc_1d, 0, sizeof(img_desc_1d));
            img_desc_1d.image_width = K * N / 4;
            img_desc_1d.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
            img_desc_1d.buffer = B_sub_buffer;
            B_image1d = clCreateImage(
                context,
                CL_MEM_READ_ONLY,
                &img_fmt_1d,
                &img_desc_1d,
                NULL,
                &status);
            CL_CHECK(status);
            // <--------------------------------------------> //
        }

        // choose gemm or gemv kernel
        // <--------------------------------------------> //
        if (N == 1) {
            kernel = backend_ctx->CL_mul_mat_vec_q4_0_f32_1d_4x_flat_general;
            if (M == 4096 && K == 4096) {
                kernel = backend_ctx->CL_mul_mat_vec_q4_0_f32_1d_4x_flat_4096_1_4096;
            } else if (M == 4096 && K == 11008) {
                kernel = backend_ctx->CL_mul_mat_vec_q4_0_f32_1d_4x_flat_4096_1_11008;
            } else if (M == 11008 && K == 4096) {
                kernel = backend_ctx->CL_mul_mat_vec_q4_0_f32_1d_4x_flat_11008_1_4096;
            } else if (M == 32000 && K == 4096) {
                kernel = backend_ctx->CL_mul_mat_vec_q4_0_f32_1d_4x_flat_32000_1_4096;
            }
        } else {
            kernel = backend_ctx->CL_mul_mat_Ab_Bi_8x4;
        }
        // <--------------------------------------------> //

        // set kernel args
        // <--------------------------------------------> //
        cl_uint k_arg = 0;

        if (N == 1) {
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(cl_mem),   &A_image1d));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(cl_mem),   &extra0_q4_0->d));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(cl_mem),   &B_image1d));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(cl_ulong), &extra1->offset));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(cl_mem),   &extrad->data_device));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(cl_ulong), &extrad->offset));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(int),      &ne00));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(int),      &ne01));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(int),      &ne02));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(int),      &ne10));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(int),      &ne12));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(int),      &ne0));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(int),      &ne1));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(int),      &r2));
            CL_CHECK(clSetKernelArg(kernel,  k_arg++, sizeof(int),      &r3));
        } else {
            region.origin = extrad->offset; // Specify the starting offset (in bytes)
            region.size = M * N * sizeof(float); // Specify the size of the sub-buffer
            C_d = clCreateSubBuffer(extrad->data_device, CL_MEM_WRITE_ONLY, CL_BUFFER_CREATE_TYPE_REGION, &region, &status);
            CL_CHECK(status);

            int padded_N = ne1 + padding;

            CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem), &extra0_q4_0->q)); //A_q_dextra0_q4_0->q
            CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_mem), &extra0_q4_0->d)); //A_s_d
            CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem), &B_image1d)); //B_d
            CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_mem), &C_d)); //C_d
            CL_CHECK(clSetKernelArg(kernel, 4, sizeof(int),    &ne01)); //M
            CL_CHECK(clSetKernelArg(kernel, 5, sizeof(int),    &padded_N)); //N with padding
            CL_CHECK(clSetKernelArg(kernel, 6, sizeof(int),    &ne00)); //K
            CL_CHECK(clSetKernelArg(kernel, 7, sizeof(int),    &ne1)); //N without padding
        }
        // <--------------------------------------------> //

        // choose workgroup size
        // <--------------------------------------------> //
        size_t global_work_size[3] = {
            64, static_cast<size_t>((M+63)/64), static_cast<size_t>((N+31)/32)};
        size_t local_work_size[3] = {64, 2, 4};

        global_work_size[0] = (size_t)(ceil((float)ne1/8));
        global_work_size[1] = (size_t)(ne01/4);
        global_work_size[2] = (size_t)(1);

        local_work_size[0]  = (size_t)(1); //4x32 for FP32
        local_work_size[1]  = (size_t)(128);
        local_work_size[2]  = (size_t)(1);

        //WGS tuning
        if (ne0 == 4096 && ne1 == 128 && ne10 == 4096) {
            local_work_size[0] = 1;
            local_work_size[1] = 128;
        } else if (ne0 == 11008 && ne1 == 128 && ne10 == 4096) {
            local_work_size[0] = 2;
            local_work_size[1] = 64;
        } else if (ne0 == 4096 && ne1 == 128 && ne10 == 11008) {
            local_work_size[0] = 2;
            local_work_size[1] = 64;
        } else if (ne0 == 32000 && ne1 == 128 && ne10 == 4096) {
            local_work_size[0] = 2;
            local_work_size[1] = 64;
        }

        if (N == 1) {
            size_t wavesize = backend_ctx->adreno_wave_size;
            local_work_size[0] = wavesize; // localsize
            local_work_size[1] = 4; // reduce factor
            local_work_size[2] = 1;

            global_work_size[0] = (((M / 2) + wavesize - 1) / wavesize) * wavesize;
            global_work_size[1] = 4; // reduce factor
            global_work_size[2] = 1;
        }
        // <--------------------------------------------> //

        // enqueue kernel with profiling
        // <--------------------------------------------> //
    #ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
        // enqueue kernel without profiling
    #else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
    #endif
        // <--------------------------------------------> //

        // deallocate sub buffers and images
        // <--------------------------------------------> //
        CL_CHECK(clReleaseMemObject(A_image1d));
        CL_CHECK(clReleaseMemObject(B_sub_buffer));
        CL_CHECK(clReleaseMemObject(B_image1d));

        if (N != 1) {
            CL_CHECK(clReleaseMemObject(B_d));
            CL_CHECK(clReleaseMemObject(B_d_input_image));
            CL_CHECK(clReleaseMemObject(C_d));
        }
        // <--------------------------------------------> //

        return;
    }
    } // if (ne01 && ne1)
#endif // GGML_OPENCL_USE_ADRENO_KERNELS

    if (!ggml_is_transposed(src0) &&
        !ggml_is_transposed(src1) &&
        src1t == GGML_TYPE_F32 &&
        ne00%32 == 0 &&
        ne11 > 2) {
#ifdef GGML_OPENCL_SOA_Q
        // Set up kernel.
        switch(src0t) {
            case GGML_TYPE_Q4_0:
                // This should have been satisfied.
                GGML_ASSERT(ne11 == ne1);
                GGML_ASSERT(ne01 == ne0);

                if (backend_ctx->gpu_family == INTEL) {
                    nth0 = 16;
                    nth1 = 1;

                    kernel = backend_ctx->kernel_mul_mat_q4_0_f32_1d_16x_flat;
                } else if (backend_ctx->gpu_family == ADRENO) {
                    nth0 = 64;
                    nth1 = 1;

                    kernel = backend_ctx->kernel_mul_mat_q4_0_f32_1d_8x_flat;
                } else {
                    GGML_ASSERT(false && "TODO: Unknown GPU");
                }

                CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0_q4_0->q));
                CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_mem),   &extra0_q4_0->d));
                CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
                CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
                CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   &extrad->data_device));
                CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offsetd));
                CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),      &ne00));
                CL_CHECK(clSetKernelArg(kernel,  7, sizeof(int),      &ne01));
                CL_CHECK(clSetKernelArg(kernel,  8, sizeof(int),      &ne02));
                CL_CHECK(clSetKernelArg(kernel,  9, sizeof(int),      &ne10));
                CL_CHECK(clSetKernelArg(kernel, 10, sizeof(int),      &ne12));
                CL_CHECK(clSetKernelArg(kernel, 11, sizeof(int),      &ne0));
                CL_CHECK(clSetKernelArg(kernel, 12, sizeof(int),      &ne1));
                CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &r2));
                CL_CHECK(clSetKernelArg(kernel, 14, sizeof(int),      &r3));
                break;
            default:
                break;
        }

        // Launch kernel.
        if (src0t == GGML_TYPE_Q4_0) {
            size_t global_work_size[] = {(size_t)(ne01 + 7)/8*nth0, (size_t)ne11*nth1, (size_t)ne12*ne13};
            size_t local_work_size[] = {(size_t)nth0, (size_t)nth1, 1};

            if (backend_ctx->gpu_family == INTEL) {
                // Set global size for Intel. It uses 16x output values.
                global_work_size[0] = (size_t)(ne01 + 15)/16*nth0;
                global_work_size[1] = (size_t)ne11*nth1;
                global_work_size[2] = (size_t)ne12*ne13;
            }

#ifdef GGML_OPENCL_PROFILING
            cl_event evt;
            CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

            g_profiling_info.emplace_back();
            populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
            CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
            return;
        }
#else // GGML_OPENCL_SOA_Q
        // TODO: add block_q4_0 variant.
#endif // GGML_OPENCL_SOA_Q
    }

    // use custom matrix x vector kernel
    switch (src0t) {
        case GGML_TYPE_F32:
            //GGML_ASSERT(ne02 == ne12);
            GGML_ASSERT(src1t == GGML_TYPE_F32);
            kernel = backend_ctx->kernel_mul_mat_f32_f32;
            nrows = 4;

            if (backend_ctx->gpu_family == INTEL) {
                nth0 = 32;
                nth1 = 1;
            } else if (backend_ctx->gpu_family == ADRENO) {
                nth0 = 64;
                nth1 = 1;
            } else {
                GGML_ASSERT(false && "TODO: Unknown GPU");
            }

            CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0->data_device));
            CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong), &offset0));
            CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
            CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
            CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   &extrad->data_device));
            CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offsetd));
            CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),      &ne00));
            CL_CHECK(clSetKernelArg(kernel,  7, sizeof(int),      &ne01));
            CL_CHECK(clSetKernelArg(kernel,  8, sizeof(int),      &ne02));
            CL_CHECK(clSetKernelArg(kernel,  9, sizeof(cl_ulong), &nb00));
            CL_CHECK(clSetKernelArg(kernel, 10, sizeof(cl_ulong), &nb01));
            CL_CHECK(clSetKernelArg(kernel, 11, sizeof(cl_ulong), &nb02));
            CL_CHECK(clSetKernelArg(kernel, 12, sizeof(cl_ulong), &nb03));
            CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &ne10));
            CL_CHECK(clSetKernelArg(kernel, 14, sizeof(int),      &ne11));
            CL_CHECK(clSetKernelArg(kernel, 15, sizeof(int),      &ne12));
            CL_CHECK(clSetKernelArg(kernel, 16, sizeof(cl_ulong), &nb10));
            CL_CHECK(clSetKernelArg(kernel, 17, sizeof(cl_ulong), &nb11));
            CL_CHECK(clSetKernelArg(kernel, 18, sizeof(cl_ulong), &nb12));
            CL_CHECK(clSetKernelArg(kernel, 19, sizeof(cl_ulong), &nb13));
            CL_CHECK(clSetKernelArg(kernel, 20, sizeof(int),      &ne0));
            CL_CHECK(clSetKernelArg(kernel, 21, sizeof(int),      &ne1));
            CL_CHECK(clSetKernelArg(kernel, 22, sizeof(int),      &r2));
            CL_CHECK(clSetKernelArg(kernel, 23, sizeof(int),      &r3));
            break;
        case GGML_TYPE_F16:
            //GGML_ASSERT(ne02 == ne12);
            if (backend_ctx->gpu_family == INTEL) {
                nth0 = 32;
                nth1 = 1;
            } else if (backend_ctx->gpu_family == ADRENO) {
                nth0 = 64;
                nth1 = 1;
            } else {
                GGML_ASSERT(false && "TODO: Unknown GPU");
            }

            if (src1t == GGML_TYPE_F32) {
                if (ne11 * ne12 < 4) {
                    kernel = backend_ctx->kernel_mul_mat_f16_f32_1row;
                } else if (ne00 >= 128 && ne01 >= 8 && ne00%4 == 0) {
                    kernel = backend_ctx->kernel_mul_mat_f16_f32_l4;
                    nrows = ne11;
                } else {
                    kernel = backend_ctx->kernel_mul_mat_f16_f32;
                    nrows = 4;
                }
            } else {
                kernel = backend_ctx->kernel_mul_mat_f16_f16;
                nrows = 4;
            }

            CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0->data_device));
            CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong), &offset0));
            CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
            CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
            CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   &extrad->data_device));
            CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offsetd));
            CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),      &ne00));
            CL_CHECK(clSetKernelArg(kernel,  7, sizeof(int),      &ne01));
            CL_CHECK(clSetKernelArg(kernel,  8, sizeof(int),      &ne02));
            CL_CHECK(clSetKernelArg(kernel,  9, sizeof(cl_ulong), &nb00));
            CL_CHECK(clSetKernelArg(kernel, 10, sizeof(cl_ulong), &nb01));
            CL_CHECK(clSetKernelArg(kernel, 11, sizeof(cl_ulong), &nb02));
            CL_CHECK(clSetKernelArg(kernel, 12, sizeof(cl_ulong), &nb03));
            CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &ne10));
            CL_CHECK(clSetKernelArg(kernel, 14, sizeof(int),      &ne11));
            CL_CHECK(clSetKernelArg(kernel, 15, sizeof(int),      &ne12));
            CL_CHECK(clSetKernelArg(kernel, 16, sizeof(cl_ulong), &nb10));
            CL_CHECK(clSetKernelArg(kernel, 17, sizeof(cl_ulong), &nb11));
            CL_CHECK(clSetKernelArg(kernel, 18, sizeof(cl_ulong), &nb12));
            CL_CHECK(clSetKernelArg(kernel, 19, sizeof(cl_ulong), &nb13));
            CL_CHECK(clSetKernelArg(kernel, 20, sizeof(int),      &ne0));
            CL_CHECK(clSetKernelArg(kernel, 21, sizeof(int),      &ne1));
            CL_CHECK(clSetKernelArg(kernel, 22, sizeof(int),      &r2));
            CL_CHECK(clSetKernelArg(kernel, 23, sizeof(int),      &r3));
            break;
        case GGML_TYPE_Q4_0:
            // This should have been satisfied.
            GGML_ASSERT(ne11 == ne1);
            GGML_ASSERT(ne01 == ne0);

#ifdef GGML_OPENCL_SOA_Q
            if (backend_ctx->gpu_family == INTEL) {
                nth0 = 16;
                nth1 = 1;

                kernel = backend_ctx->kernel_mul_mat_q4_0_f32_8x_flat;
                ndst = 8;
            } else if (backend_ctx->gpu_family == ADRENO) {
                nth0 = 64;
                nth1 = 1;

                kernel = backend_ctx->kernel_mul_mat_q4_0_f32_8x_flat;
                ndst =8;
            } else {
                GGML_ASSERT(false && "TODO: Unknown GPU");
            }

            CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0_q4_0->q));
            CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_mem),   &extra0_q4_0->d));
            CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
            CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
            CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   &extrad->data_device));
            CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offsetd));
            CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),      &ne00));
            CL_CHECK(clSetKernelArg(kernel,  7, sizeof(int),      &ne01));
            CL_CHECK(clSetKernelArg(kernel,  8, sizeof(int),      &ne02));
            CL_CHECK(clSetKernelArg(kernel,  9, sizeof(int),      &ne10));
            CL_CHECK(clSetKernelArg(kernel, 10, sizeof(int),      &ne12));
            CL_CHECK(clSetKernelArg(kernel, 11, sizeof(int),      &ne0));
            CL_CHECK(clSetKernelArg(kernel, 12, sizeof(int),      &ne1));
            CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &r2));
            CL_CHECK(clSetKernelArg(kernel, 14, sizeof(int),      &r3));
#else // GGML_OPENCL_SOA_Q
            if (backend_ctx->gpu_family == INTEL) {
                // Use 1D local size. Each workgroup is a SIMD group. Each SIMD
                // group produces N_DST (4 for Q4_0 kernel) values in the result.
                // The number of workgroups on dim 0 (the leading dimension) is
                // the nearest multiple of 4 that covers ne0 (equals ne01).
                nth0 = 16;
                nth1 = 1;

                kernel = backend_ctx->kernel_mul_mat_q4_0_f32;
                ndst = 4;
            } else if (backend_ctx->gpu_family == ADRENO) {
                nth0 = 64;
                nth1 = 1;

                kernel = backend_ctx->kernel_mul_mat_q4_0_f32_v;
                ndst = 4;
            } else {
                GGML_ASSERT(false && "TODO: Unknown GPU");
            }

            CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0->data_device));
            CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong), &offset0));
            CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
            CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
            CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   &extrad->data_device));
            CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offsetd));
            CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),      &ne00));
            CL_CHECK(clSetKernelArg(kernel,  7, sizeof(int),      &ne01));
            CL_CHECK(clSetKernelArg(kernel,  8, sizeof(int),      &ne02));
            CL_CHECK(clSetKernelArg(kernel,  9, sizeof(int),      &ne10));
            CL_CHECK(clSetKernelArg(kernel, 10, sizeof(int),      &ne12));
            CL_CHECK(clSetKernelArg(kernel, 11, sizeof(int),      &ne0));
            CL_CHECK(clSetKernelArg(kernel, 12, sizeof(int),      &ne1));
            CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &r2));
            CL_CHECK(clSetKernelArg(kernel, 14, sizeof(int),      &r3));
#endif // GGML_OPENCL_SOA_Q
            break;
        case GGML_TYPE_Q4_1:
        case GGML_TYPE_Q8_0:
        case GGML_TYPE_Q2_K:
        case GGML_TYPE_Q3_K:
        case GGML_TYPE_Q4_K:
        case GGML_TYPE_Q5_K:
        case GGML_TYPE_Q6_K:
            kernel = backend_ctx->kernel_mul_mv_q6_K_f32;

            if (backend_ctx->gpu_family == INTEL) {
                nth0 = 2;
                nth1 = 16;
            } else if (backend_ctx->gpu_family == ADRENO) {
                nth0 = 2;
                nth1 = 64;
            } else {
                GGML_ASSERT(false && "TODO: Unknown GPU");
            }

            CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0->data_device));
            CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong), &offset0));
            CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
            CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
            CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   &extrad->data_device));
            CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offsetd));
            CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),      &ne00));
            CL_CHECK(clSetKernelArg(kernel,  7, sizeof(int),      &ne01));
            CL_CHECK(clSetKernelArg(kernel,  8, sizeof(int),      &ne02));
            CL_CHECK(clSetKernelArg(kernel,  9, sizeof(int),      &ne10));
            CL_CHECK(clSetKernelArg(kernel, 10, sizeof(int),      &ne12));
            CL_CHECK(clSetKernelArg(kernel, 11, sizeof(int),      &ne0));
            CL_CHECK(clSetKernelArg(kernel, 12, sizeof(int),      &ne1));
            CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &r2));
            CL_CHECK(clSetKernelArg(kernel, 14, sizeof(int),      &r3));
            break;
        default:
            GGML_ASSERT(false && "not implemented");
    }

    if (src0t == GGML_TYPE_Q4_0 ||
        src0t == GGML_TYPE_Q4_1 ||
        src0t == GGML_TYPE_Q8_0 ||
        src0t == GGML_TYPE_Q2_K) {
        // Each SIMD group produces N_DST values in the result. Assuming each
        // workgroup has N_SIMDGROUP SIMD groups, then each workgroup will
        // produce N_DST*N_SIMDGROUP values in the result. Hence, the grid size
        // (number of workgroups) will be a nearest multiple of
        // N_DST*N_SIMDGROUP to cover the size of the dimension. Below, 4 is
        // N_DST*N_SIMDGROUP (see the kernel for Q4_0 matmul).
        size_t global_work_size[] = {(size_t)(ne01 + ndst-1)/ndst*nth0, (size_t)ne11*nth1, (size_t)ne12*ne13};
        size_t local_work_size[] = {(size_t)nth0, (size_t)nth1, 1};

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
    } else if (src0t == GGML_TYPE_Q4_K) {
        GGML_ASSERT(false && "not implemented");
    } else if (src0t == GGML_TYPE_Q3_K) {
        GGML_ASSERT(false && "not implemented");
    } else if (src0t == GGML_TYPE_Q5_K) {
        GGML_ASSERT(false && "not implemented");
    } else if (src0t == GGML_TYPE_Q6_K) {
        size_t global_work_size[] = {(size_t)(ne01+1)/2*nth0, (size_t)ne11*nth1, (size_t)ne12*ne13};
        size_t local_work_size[] = {(size_t)nth0, (size_t)nth1, 1};

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
    } else {
        int64_t ny = (ne11 + nrows - 1)/nrows;

        size_t global_work_size[] = {(size_t)ne01*nth0, (size_t)ny*nth1, (size_t)ne12*ne13};
        size_t local_work_size[] = {(size_t)nth0, (size_t)nth1, 1};

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
    }
}

static void ggml_cl_scale(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);
    GGML_UNUSED(src1);

    GGML_ASSERT(ggml_is_contiguous(src0));

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    float scale;
    memcpy(&scale, dst->op_params, sizeof(scale));

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    cl_kernel kernel = backend_ctx->kernel_scale;

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offsetd));
    CL_CHECK(clSetKernelArg(kernel, 4, sizeof(float),    &scale));

    int n = ggml_nelements(dst)/4;

    size_t global_work_size[] = {(size_t)n, 1, 1};
    size_t local_work_size[] = {64, 1, 1};

    size_t * local_work_size_ptr = local_work_size;
    if (n % 64 != 0 && !backend_ctx->non_uniform_workgroups) {
        local_work_size_ptr = nullptr;  // Let driver choose the work-group sizes.
    }

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size_ptr, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, NULL));
#endif
}

static void ggml_cl_cpy(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(src1);
    GGML_ASSERT(src1->extra);

    // GGML_OP_CPY happens between src0 and src1.
    // GGML_OP_DUP and GGML_OP_CONT happen between src0 and dst.
    UNUSED(dst);

    const int ne00 = src0 ? src0->ne[0] : 0;
    const int ne01 = src0 ? src0->ne[1] : 0;
    const int ne02 = src0 ? src0->ne[2] : 0;
    const int ne03 = src0 ? src0->ne[3] : 0;

    const cl_ulong nb00 = src0 ? src0->nb[0] : 0;
    const cl_ulong nb01 = src0 ? src0->nb[1] : 0;
    const cl_ulong nb02 = src0 ? src0->nb[2] : 0;
    const cl_ulong nb03 = src0 ? src0->nb[3] : 0;

    const int ne10 = src1 ? src1->ne[0] : 0;
    const int ne11 = src1 ? src1->ne[1] : 0;
    const int ne12 = src1 ? src1->ne[2] : 0;
    const int ne13 = src1 ? src1->ne[3] : 0;

    const cl_ulong nb10 = src1 ? src1->nb[0] : 0;
    const cl_ulong nb11 = src1 ? src1->nb[1] : 0;
    const cl_ulong nb12 = src1 ? src1->nb[2] : 0;
    const cl_ulong nb13 = src1 ? src1->nb[3] : 0;

    const enum ggml_type src0t = src0 ? src0->type : GGML_TYPE_COUNT;
    const enum ggml_type src1t = src1 ? src1->type : GGML_TYPE_COUNT;

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra1 = (ggml_tensor_extra_cl *)src1->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offset1 = extra1->offset + src1->view_offs;

    cl_kernel kernel;

    switch (src0t) {
        case GGML_TYPE_F32:
            switch (src1t) {
                case GGML_TYPE_F16:
                    kernel = backend_ctx->kernel_cpy_f32_f16;
                    break;
                case GGML_TYPE_F32:
                    kernel = backend_ctx->kernel_cpy_f32_f32;
                    break;
                default:
                    GGML_ASSERT(false && "not implemented");
            }
            break;
        case GGML_TYPE_F16:
            switch (src1t) {
                case GGML_TYPE_F16:
                    kernel = backend_ctx->kernel_cpy_f16_f16;
                    break;
                case GGML_TYPE_F32:
                    kernel = backend_ctx->kernel_cpy_f16_f32;
                    break;
                default:
                    GGML_ASSERT(false && "not implemented");
            }
            break;
        default:
            GGML_ASSERT(false && "not implemented");
    }

    CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
    CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
    CL_CHECK(clSetKernelArg(kernel,  4, sizeof(int),      &ne00));
    CL_CHECK(clSetKernelArg(kernel,  5, sizeof(int),      &ne01));
    CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),      &ne02));
    CL_CHECK(clSetKernelArg(kernel,  7, sizeof(int),      &ne03));
    CL_CHECK(clSetKernelArg(kernel,  8, sizeof(cl_ulong), &nb00));
    CL_CHECK(clSetKernelArg(kernel,  9, sizeof(cl_ulong), &nb01));
    CL_CHECK(clSetKernelArg(kernel, 10, sizeof(cl_ulong), &nb02));
    CL_CHECK(clSetKernelArg(kernel, 11, sizeof(cl_ulong), &nb03));
    CL_CHECK(clSetKernelArg(kernel, 12, sizeof(int),      &ne10));
    CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &ne11));
    CL_CHECK(clSetKernelArg(kernel, 14, sizeof(int),      &ne12));
    CL_CHECK(clSetKernelArg(kernel, 15, sizeof(int),      &ne13));
    CL_CHECK(clSetKernelArg(kernel, 16, sizeof(cl_ulong), &nb10));
    CL_CHECK(clSetKernelArg(kernel, 17, sizeof(cl_ulong), &nb11));
    CL_CHECK(clSetKernelArg(kernel, 18, sizeof(cl_ulong), &nb12));
    CL_CHECK(clSetKernelArg(kernel, 19, sizeof(cl_ulong), &nb13));

    const int nth = MIN(64, ne00);

    size_t global_work_size[] = {(size_t)ne01*nth, (size_t)ne02, (size_t)ne03};
    size_t local_work_size[] = {(size_t)nth, 1, 1};

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, src1);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
}

static void ggml_cl_dup(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    ggml_cl_cpy(backend, src0, dst, nullptr);
    UNUSED(src1);
}

static void ggml_cl_diag_mask_inf(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    UNUSED(src1);

    int n_past = ((int32_t *)(dst->op_params))[0];

    const int  ne00 = src0 ? src0->ne[0] : 0;
    const int  ne01 = src0 ? src0->ne[1] : 0;
    const int  ne02 = src0 ? src0->ne[2] : 0;

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    cl_kernel kernel;

    if (ne00%8 == 0) {
        kernel = backend_ctx->kernel_diag_mask_inf_8;

        CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
        CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
        CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extrad->data_device));
        CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offsetd));
        CL_CHECK(clSetKernelArg(kernel, 4, sizeof(int),      &ne00));
        CL_CHECK(clSetKernelArg(kernel, 5, sizeof(int),      &ne01));
        CL_CHECK(clSetKernelArg(kernel, 6, sizeof(int),      &n_past));

        size_t global_work_size[] = {(size_t)ne00*ne01*ne02/8, 1, 1};
        size_t local_work_size[] = {64, 1, 1};

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
    } else {
        kernel = backend_ctx->kernel_diag_mask_inf;

        CL_CHECK(clSetKernelArg(kernel, 0, sizeof(cl_mem),   &extra0->data_device));
        CL_CHECK(clSetKernelArg(kernel, 1, sizeof(cl_ulong), &offset0));
        CL_CHECK(clSetKernelArg(kernel, 2, sizeof(cl_mem),   &extrad->data_device));
        CL_CHECK(clSetKernelArg(kernel, 3, sizeof(cl_ulong), &offsetd));
        CL_CHECK(clSetKernelArg(kernel, 4, sizeof(int),      &ne00));
        CL_CHECK(clSetKernelArg(kernel, 5, sizeof(int),      &ne01));
        CL_CHECK(clSetKernelArg(kernel, 6, sizeof(int),      &n_past));

        size_t global_work_size[] = {(size_t)ne00, (size_t)ne01, (size_t)ne02};
        size_t local_work_size[] = {64, 1, 1};

        size_t * local_work_size_ptr = local_work_size;
        if (ne00 % 64 != 0 && !backend_ctx->non_uniform_workgroups) {
            local_work_size_ptr = nullptr;  // Let driver choose the work-group sizes.
        }

#ifdef GGML_OPENCL_PROFILING
        cl_event evt;
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, &evt));

        g_profiling_info.emplace_back();
        populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size_ptr, dst);
#else
        CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size_ptr, 0, NULL, NULL));
#endif
    }
}

static void ggml_cl_soft_max(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    // Softmax can now fuse KQ mask and KQ scale, which used to be two additional
    // ops before softmax. It now also fuses alibi if `max_bias > 0`. For llama,
    // alibi is not used; however, for some other models, it is used.
    // KQ_mask
    if (src1) {
        GGML_ASSERT(src1);
        GGML_ASSERT(src1->extra);
    }

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    ggml_tensor_extra_cl * extra1 = src1 ? (ggml_tensor_extra_cl *)src1->extra : nullptr;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    cl_ulong offset1 = extra1 ? extra1->offset + src1->view_offs : offset0;

    const int  ne00 = src0 ? src0->ne[0] : 0;
    const int  ne01 = src0 ? src0->ne[1] : 0;
    const int  ne02 = src0 ? src0->ne[2] : 0;
    const int  ne03 = src0 ? src0->ne[3] : 0;

    float scale, max_bias;
    memcpy(&scale,    dst->op_params + 0, sizeof(float));
    memcpy(&max_bias, dst->op_params + 1, sizeof(float));

    const int nrows_x = ggml_nrows(src0);
    const int nrows_y = src0->ne[1];

    const int n_head      = nrows_x/nrows_y;
    const int n_head_log2 = 1u << (uint32_t) floorf(log2f((float) n_head));

    const float m0 = powf(2.0f, -(max_bias       ) / n_head_log2);
    const float m1 = powf(2.0f, -(max_bias / 2.0f) / n_head_log2);

    const bool use_f16 = (src1 && src1->type == GGML_TYPE_F16);

    // Local size must be wave size. Each workgroup is a wave, working on a row,
    // where a row corresponds to leading dimension.
    int nth = MIN(32, ne00);

    if (backend_ctx->gpu_family == INTEL) {
        // This is the same as the initial value.
        nth = MIN(32, ne00);
    }
    else if (backend_ctx->gpu_family == ADRENO) {
        nth = 64;
    } else {
        GGML_ASSERT(false && "TODO: Unknown GPU");
    }

    cl_kernel kernel;

    if (ne00%4 == 0) {
        if (use_f16) {
            kernel = backend_ctx->kernel_soft_max_4_f16;
        } else {
            kernel = backend_ctx->kernel_soft_max_4;
        }
    } else {
        if (use_f16) {
            kernel = backend_ctx->kernel_soft_max_f16;
        } else {
            kernel = backend_ctx->kernel_soft_max;
        }
    }

    CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   extra1 ? &extra1->data_device : &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
    CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offsetd));
    CL_CHECK(clSetKernelArg(kernel,  6, sizeof(int),      &ne00));
    CL_CHECK(clSetKernelArg(kernel,  7, sizeof(int),      &ne01));
    CL_CHECK(clSetKernelArg(kernel,  8, sizeof(int),      &ne02));
    CL_CHECK(clSetKernelArg(kernel,  9, sizeof(float),    &scale));
    CL_CHECK(clSetKernelArg(kernel, 10, sizeof(float),    &max_bias));
    CL_CHECK(clSetKernelArg(kernel, 11, sizeof(float),    &m0));
    CL_CHECK(clSetKernelArg(kernel, 12, sizeof(float),    &m1));
    CL_CHECK(clSetKernelArg(kernel, 13, sizeof(int),      &n_head_log2));

    size_t global_work_size[] = {(size_t)ne01*nth, (size_t)ne02, (size_t)ne03};
    size_t local_work_size[] = {(size_t)nth, 1, 1};

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
}

static void ggml_cl_rope(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(src1);
    GGML_ASSERT(src1->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extra1 = (ggml_tensor_extra_cl *)src1->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offset1 = extra1->offset + src1->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    ggml_tensor * src2 = dst->src[2];
    ggml_tensor_extra_cl * extra2 = src2 ? (ggml_tensor_extra_cl *)src2->extra : nullptr;

    cl_ulong offset2 = extra2 ? extra2->offset + src2->view_offs : offset0;

    const int  ne00 = src0 ? src0->ne[0] : 0;
    const int  ne01 = src0 ? src0->ne[1] : 0;
    const int  ne02 = src0 ? src0->ne[2] : 0;
    const int  ne03 = src0 ? src0->ne[3] : 0;

    const cl_ulong  nb00 = src0 ? src0->nb[0] : 0;
    const cl_ulong  nb01 = src0 ? src0->nb[1] : 0;
    const cl_ulong  nb02 = src0 ? src0->nb[2] : 0;
    const cl_ulong  nb03 = src0 ? src0->nb[3] : 0;

    const int ne10 = src1 ? src1->ne[0] : 0;
    const int ne11 = src1 ? src1->ne[1] : 0; UNUSED(ne11);
    const int ne12 = src1 ? src1->ne[2] : 0; UNUSED(ne12);
    const int ne13 = src1 ? src1->ne[3] : 0; UNUSED(ne13);

    const int  ne0 = dst ? dst->ne[0] : 0;
    const int  ne1 = dst ? dst->ne[1] : 0;
    const int  ne2 = dst ? dst->ne[2] : 0;
    const int  ne3 = dst ? dst->ne[3] : 0;

    const cl_ulong  nb0 = dst ? dst->nb[0] : 0;
    const cl_ulong  nb1 = dst ? dst->nb[1] : 0;
    const cl_ulong  nb2 = dst ? dst->nb[2] : 0;
    const cl_ulong  nb3 = dst ? dst->nb[3] : 0;

    GGML_ASSERT(ne10 % ne02 == 0);
    GGML_ASSERT(ne10 >= ne02);

    int nth = MIN(64, ne00);

    const int n_past     = ((int *) dst->op_params)[0];
    const int n_dims     = ((int *) dst->op_params)[1];
    const int mode       = ((int *) dst->op_params)[2];
    const int n_ctx_orig = ((int32_t *) dst->op_params)[4];

    float freq_base;
    float freq_scale;
    float ext_factor;
    float attn_factor;
    float beta_fast;
    float beta_slow;
    int32_t sections[4];

    memcpy(&freq_base,   (int32_t *) dst->op_params + 5, sizeof(float));
    memcpy(&freq_scale,  (int32_t *) dst->op_params + 6, sizeof(float));
    memcpy(&ext_factor,  (int32_t *) dst->op_params + 7, sizeof(float));
    memcpy(&attn_factor, (int32_t *) dst->op_params + 8, sizeof(float));
    memcpy(&beta_fast,   (int32_t *) dst->op_params + 9, sizeof(float));
    memcpy(&beta_slow,   (int32_t *) dst->op_params + 10, sizeof(float));
    memcpy(&sections,    (int32_t *) dst->op_params + 11, sizeof(int32_t)*4);

    const bool is_neox = mode & 2;
    const bool is_mrope = mode & GGML_ROPE_TYPE_MROPE;
    const bool is_vision = mode == GGML_ROPE_TYPE_VISION;

    if (is_mrope) {
        GGML_ASSERT(sections[0] > 0 || sections[1] > 0 || sections[2] > 0);
    }

    if (is_vision) {
        GGML_ASSERT(n_dims == ne00/2);
    }

    cl_kernel kernel;

    if (is_neox) {
        switch (src0->type) {
            case GGML_TYPE_F32:
                kernel = backend_ctx->kernel_rope_neox_f32;
                break;
            case GGML_TYPE_F16:
                kernel = backend_ctx->kernel_rope_neox_f16;
                break;
            default:
                GGML_ASSERT(false);
        };
    } else if (is_mrope && !is_vision) {
        switch (src0->type) {
            case GGML_TYPE_F32:
                kernel = backend_ctx->kernel_rope_multi_f32;
                break;
            case GGML_TYPE_F16:
                kernel = backend_ctx->kernel_rope_multi_f16;
                break;
            default:
                GGML_ASSERT(false);
        };
    } else if (is_vision) {
        switch (src0->type) {
            case GGML_TYPE_F32:
                kernel = backend_ctx->kernel_rope_vision_f32;
                break;
            case GGML_TYPE_F16:
                kernel = backend_ctx->kernel_rope_vision_f16;
                break;
            default:
                GGML_ASSERT(false);
        }
    } else {
        switch (src0->type) {
            case GGML_TYPE_F32:
                kernel = backend_ctx->kernel_rope_norm_f32;
                break;
            case GGML_TYPE_F16:
                kernel = backend_ctx->kernel_rope_norm_f16;
                break;
            default:
                GGML_ASSERT(false);
        };
    }

    CL_CHECK(clSetKernelArg(kernel,  0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel,  1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel,  2, sizeof(cl_mem),   &extra1->data_device));
    CL_CHECK(clSetKernelArg(kernel,  3, sizeof(cl_ulong), &offset1));
    CL_CHECK(clSetKernelArg(kernel,  4, sizeof(cl_mem),   extra2 ? &extra2->data_device : &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel,  5, sizeof(cl_ulong), &offset2));
    CL_CHECK(clSetKernelArg(kernel,  6, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel,  7, sizeof(cl_ulong), &offsetd));
    CL_CHECK(clSetKernelArg(kernel,  8, sizeof(int),      &ne00));
    CL_CHECK(clSetKernelArg(kernel,  9, sizeof(int),      &ne01));
    CL_CHECK(clSetKernelArg(kernel, 10, sizeof(int),      &ne02));
    CL_CHECK(clSetKernelArg(kernel, 11, sizeof(int),      &ne03));
    CL_CHECK(clSetKernelArg(kernel, 12, sizeof(cl_ulong), &nb00));
    CL_CHECK(clSetKernelArg(kernel, 13, sizeof(cl_ulong), &nb01));
    CL_CHECK(clSetKernelArg(kernel, 14, sizeof(cl_ulong), &nb02));
    CL_CHECK(clSetKernelArg(kernel, 15, sizeof(cl_ulong), &nb03));
    CL_CHECK(clSetKernelArg(kernel, 16, sizeof(int),      &ne0));
    CL_CHECK(clSetKernelArg(kernel, 17, sizeof(int),      &ne1));
    CL_CHECK(clSetKernelArg(kernel, 18, sizeof(int),      &ne2));
    CL_CHECK(clSetKernelArg(kernel, 19, sizeof(int),      &ne3));
    CL_CHECK(clSetKernelArg(kernel, 20, sizeof(cl_ulong), &nb0));
    CL_CHECK(clSetKernelArg(kernel, 21, sizeof(cl_ulong), &nb1));
    CL_CHECK(clSetKernelArg(kernel, 22, sizeof(cl_ulong), &nb2));
    CL_CHECK(clSetKernelArg(kernel, 23, sizeof(cl_ulong), &nb3));
    CL_CHECK(clSetKernelArg(kernel, 24, sizeof(int),      &n_past));
    CL_CHECK(clSetKernelArg(kernel, 25, sizeof(int),      &n_dims));
    CL_CHECK(clSetKernelArg(kernel, 26, sizeof(int),      &n_ctx_orig));
    CL_CHECK(clSetKernelArg(kernel, 27, sizeof(float),    &freq_base));
    CL_CHECK(clSetKernelArg(kernel, 28, sizeof(float),    &freq_scale));
    CL_CHECK(clSetKernelArg(kernel, 29, sizeof(float),    &ext_factor));
    CL_CHECK(clSetKernelArg(kernel, 30, sizeof(float),    &attn_factor));
    CL_CHECK(clSetKernelArg(kernel, 31, sizeof(float),    &beta_fast));
    CL_CHECK(clSetKernelArg(kernel, 32, sizeof(float),    &beta_slow));
    if (is_mrope || is_vision) {
        CL_CHECK(clSetKernelArg(kernel, 33, sizeof(int32_t)*4, &sections));
    }

    size_t global_work_size[] = {(size_t)ne01*nth, (size_t)ne02, (size_t)ne03};
    size_t local_work_size[] = {(size_t)nth, 1, 1};

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
}

static void ggml_cl_im2col(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src1);
    GGML_ASSERT(src1->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);

    // src0 - filter, src1 - input
    GGML_ASSERT(src1->type == GGML_TYPE_F32);
    GGML_ASSERT(dst->type == GGML_TYPE_F16 || dst->type == GGML_TYPE_F32);

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra1 = (ggml_tensor_extra_cl *)src1->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset1 = extra1->offset + src1->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    const int32_t s0 = ((const int32_t*)(dst->op_params))[0];
    const int32_t s1 = ((const int32_t*)(dst->op_params))[1];
    const int32_t p0 = ((const int32_t*)(dst->op_params))[2];
    const int32_t p1 = ((const int32_t*)(dst->op_params))[3];
    const int32_t d0 = ((const int32_t*)(dst->op_params))[4];
    const int32_t d1 = ((const int32_t*)(dst->op_params))[5];

    const bool is_2D = ((const int32_t*)(dst->op_params))[6] == 1;

    const cl_long IC = src1->ne[is_2D ? 2 : 1];
    const cl_long IH = is_2D ? src1->ne[1] : 1;
    const cl_long IW =         src1->ne[0];

    const cl_long KH = is_2D ? src0->ne[1] : 1;
    const cl_long KW =         src0->ne[0];

    const cl_long OH = is_2D ? dst->ne[2] : 1;
    const cl_long OW =         dst->ne[1];

    // nb is byte offset, src is type float32
    const cl_ulong delta_offset = src1->nb[is_2D ? 2 : 1]/4;
    const cl_long  batch        = src1->ne[is_2D ? 3 : 2];
    const cl_ulong batch_offset = src1->nb[is_2D ? 3 : 2]/4;

    const cl_long pelements = OW*KW*KH;
    const cl_long CHW       = IC*KH*KW;

    cl_kernel kernel;

    if(dst->type == GGML_TYPE_F16) {
        kernel = backend_ctx->kernel_im2col_f16;
    } else {
        kernel = backend_ctx->kernel_im2col_f32;
    }

    CL_CHECK(clSetKernelArg(kernel,   0, sizeof(cl_mem),   &extra1->data_device));
    CL_CHECK(clSetKernelArg(kernel,   1, sizeof(cl_ulong), &offset1));
    CL_CHECK(clSetKernelArg(kernel,   2, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel,   3, sizeof(cl_ulong), &offsetd));
    CL_CHECK(clSetKernelArg(kernel,   4, sizeof(cl_ulong), &batch_offset));
    CL_CHECK(clSetKernelArg(kernel,   5, sizeof(cl_ulong), &delta_offset));
    CL_CHECK(clSetKernelArg(kernel,   6, sizeof(cl_long),  &IW));
    CL_CHECK(clSetKernelArg(kernel,   7, sizeof(cl_long),  &IH));
    CL_CHECK(clSetKernelArg(kernel,   8, sizeof(cl_long),  &IC));
    CL_CHECK(clSetKernelArg(kernel,   9, sizeof(cl_long),  &OW));
    CL_CHECK(clSetKernelArg(kernel,  10, sizeof(cl_long),  &OH));
    CL_CHECK(clSetKernelArg(kernel,  11, sizeof(cl_long),  &KW));
    CL_CHECK(clSetKernelArg(kernel,  12, sizeof(cl_long),  &KH));
    CL_CHECK(clSetKernelArg(kernel,  13, sizeof(cl_long),  &pelements));
    CL_CHECK(clSetKernelArg(kernel,  14, sizeof(cl_long),  &CHW));
    CL_CHECK(clSetKernelArg(kernel,  15, sizeof(int),      &s0));
    CL_CHECK(clSetKernelArg(kernel,  16, sizeof(int),      &s1));
    CL_CHECK(clSetKernelArg(kernel,  17, sizeof(int),      &p0));
    CL_CHECK(clSetKernelArg(kernel,  18, sizeof(int),      &p1));
    CL_CHECK(clSetKernelArg(kernel,  19, sizeof(int),      &d0));
    CL_CHECK(clSetKernelArg(kernel,  20, sizeof(int),      &d1));

    const int num_blocks = (pelements + 256 - 1) / 256;
    size_t global_work_size[] = {(size_t)num_blocks*256, (size_t)OH, (size_t)batch*IC};
    size_t local_work_size[] = {256, 1, 1};

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
}

static void ggml_cl_argsort(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);
    GGML_UNUSED(src1);

    GGML_ASSERT(src0->type == GGML_TYPE_F32);
    GGML_ASSERT( dst->type == GGML_TYPE_I32);
    GGML_ASSERT(ggml_is_contiguous(src0));

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    const int ne00  = src0->ne[0];
    const int nrows = ggml_nrows(src0);

    int ne00_padded = 1;
    while (ne00_padded < ne00) {
        ne00_padded *= 2;
    }

    int order = (enum ggml_sort_order) dst->op_params[0];

    cl_kernel kernel = backend_ctx->kernel_argsort_f32_i32;

    CL_CHECK(clSetKernelArg(kernel,   0, sizeof(cl_mem),            &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel,   1, sizeof(cl_ulong),          &offset0));
    CL_CHECK(clSetKernelArg(kernel,   2, sizeof(cl_mem),            &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel,   3, sizeof(cl_ulong),          &offsetd));
    CL_CHECK(clSetKernelArg(kernel,   4, sizeof(int),               &ne00));
    CL_CHECK(clSetKernelArg(kernel,   5, sizeof(int),               &ne00_padded));
    CL_CHECK(clSetKernelArg(kernel,   6, sizeof(int),               &order));
    CL_CHECK(clSetKernelArg(kernel,   7, ne00_padded*sizeof(int),   NULL));

    size_t global_work_size[] = {(size_t)ne00_padded, (size_t)nrows, (size_t)1};
    size_t local_work_size[] = {(size_t)ne00_padded, 1, 1};

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
}

static void ggml_cl_sum_rows(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst) {
    GGML_ASSERT(src0);
    GGML_ASSERT(src0->extra);
    GGML_ASSERT(dst);
    GGML_ASSERT(dst->extra);
    GGML_UNUSED(src1);

    GGML_ASSERT(src0->nb[0] == ggml_type_size(src0->type));
    GGML_ASSERT(ggml_is_contiguous(src0));

    ggml_backend_opencl_context *backend_ctx = (ggml_backend_opencl_context *)backend->context;
    cl_command_queue queue = backend_ctx->queue;

    ggml_tensor_extra_cl * extra0 = (ggml_tensor_extra_cl *)src0->extra;
    ggml_tensor_extra_cl * extrad = (ggml_tensor_extra_cl *)dst->extra;

    cl_ulong offset0 = extra0->offset + src0->view_offs;
    cl_ulong offsetd = extrad->offset + dst->view_offs;

    const int ne00 = src0->ne[0];
    const int ne01 = src0->ne[1];
    const int ne02 = src0->ne[2];
    const int ne03 = src0->ne[3];

    const cl_ulong nb01 = src0->nb[1];
    const cl_ulong nb02 = src0->nb[2];
    const cl_ulong nb03 = src0->nb[3];

    const cl_ulong nb1  = dst->nb[1];
    const cl_ulong nb2  = dst->nb[2];
    const cl_ulong nb3  = dst->nb[3];

    cl_kernel kernel = backend_ctx->kernel_sum_rows_f32;

    CL_CHECK(clSetKernelArg(kernel,   0, sizeof(cl_mem),   &extra0->data_device));
    CL_CHECK(clSetKernelArg(kernel,   1, sizeof(cl_ulong), &offset0));
    CL_CHECK(clSetKernelArg(kernel,   2, sizeof(cl_mem),   &extrad->data_device));
    CL_CHECK(clSetKernelArg(kernel,   3, sizeof(cl_ulong), &offsetd));
    CL_CHECK(clSetKernelArg(kernel,   4, sizeof(int),      &ne00));
    CL_CHECK(clSetKernelArg(kernel,   5, sizeof(int),      &ne01));
    CL_CHECK(clSetKernelArg(kernel,   6, sizeof(int),      &ne02));
    CL_CHECK(clSetKernelArg(kernel,   7, sizeof(int),      &ne03));
    CL_CHECK(clSetKernelArg(kernel,   8, sizeof(cl_ulong), &nb01));
    CL_CHECK(clSetKernelArg(kernel,   9, sizeof(cl_ulong), &nb02));
    CL_CHECK(clSetKernelArg(kernel,  10, sizeof(cl_ulong), &nb03));
    CL_CHECK(clSetKernelArg(kernel,  11, sizeof(cl_ulong), &nb1));
    CL_CHECK(clSetKernelArg(kernel,  12, sizeof(cl_ulong), &nb2));
    CL_CHECK(clSetKernelArg(kernel,  13, sizeof(cl_ulong), &nb3));

    size_t global_work_size[] = {(size_t)ne01, (size_t)ne02, (size_t)ne03};
    size_t local_work_size[] = {(size_t)64, 1, 1};

#ifdef GGML_OPENCL_PROFILING
    cl_event evt;
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, &evt));

    g_profiling_info.emplace_back();
    populateProfilingInfo(g_profiling_info.back(), evt, kernel, global_work_size, local_work_size, dst);
#else
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 3, NULL, global_work_size, local_work_size, 0, NULL, NULL));
#endif
}

//------------------------------------------------------------------------------
// Op offloading
//------------------------------------------------------------------------------

typedef void (*ggml_cl_func_t)(ggml_backend_t backend, const ggml_tensor * src0, const ggml_tensor * src1, ggml_tensor * dst);

bool ggml_cl_compute_forward(ggml_backend_t backend, struct ggml_tensor * tensor) {
    ggml_cl_func_t func = nullptr;

    ggml_tensor * src0 = tensor->src[0];
    ggml_tensor * src1 = tensor->src[1];

    const bool any_on_device = tensor->extra
        || (src0 != nullptr && src0->extra)
        || (src1 != nullptr && src1->extra);

    switch (tensor->op) {
        case GGML_OP_GET_ROWS:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_get_rows;
            break;
        case GGML_OP_CPY:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_cpy;
            break;
        case GGML_OP_DUP:
        case GGML_OP_CONT:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_dup;
            break;
        case GGML_OP_ADD:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_add;
            break;
        case GGML_OP_MUL:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_mul;
            break;
        case GGML_OP_DIV:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_div;
            break;
        case GGML_OP_SUB:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_sub;
            break;
        case GGML_OP_UNARY:
            switch (ggml_get_unary_op(tensor)) {
                case GGML_UNARY_OP_GELU:
                    if (!any_on_device) {
                        return false;
                    }
                    func = ggml_cl_gelu;
                    break;
                case GGML_UNARY_OP_GELU_QUICK:
                    if (!any_on_device) {
                        return false;
                    }
                    func = ggml_cl_gelu_quick;
                    break;
                case GGML_UNARY_OP_SILU:
                    if (!any_on_device) {
                        return false;
                    }
                    func = ggml_cl_silu;
                    break;
                case GGML_UNARY_OP_RELU:
                    if (!any_on_device) {
                        return false;
                    }
                    func = ggml_cl_relu;
                    break;
                case GGML_UNARY_OP_SIGMOID:
                    if (!any_on_device) {
                        return false;
                    }
                    func = ggml_cl_sigmoid;
                    break;
                case GGML_UNARY_OP_TANH:
                    if (!any_on_device) {
                        return false;
                    }
                    func = ggml_cl_tanh;
                    break;
                default:
                    return false;
            } break;
        case GGML_OP_CLAMP:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_clamp;
            break;
        case GGML_OP_NORM:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_norm;
            break;
        case GGML_OP_RMS_NORM:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_rms_norm;
            break;
        case GGML_OP_GROUP_NORM:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_group_norm;
            break;
                case GGML_OP_REPEAT:
             if (!any_on_device) {
                return false;
            }
            func = ggml_cl_repeat;
            break;
        case GGML_OP_PAD:
            if (!any_on_device) {
                return false;
            }
            ggml_cl_pad(backend, tensor->src[0], tensor);
            return true;
        case GGML_OP_UPSCALE:
            if (!any_on_device) {
                return false;
            }
            ggml_cl_upscale(backend, tensor->src[0], tensor);
            return true;
        case GGML_OP_CONCAT:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_concat;
            break;
        case GGML_OP_TIMESTEP_EMBEDDING:
            if (!any_on_device) {
                return false;
            }
            ggml_cl_timestep_embedding(backend, tensor->src[0], tensor);
            return true;
        case GGML_OP_MUL_MAT:
            if (!any_on_device && !ggml_cl_can_mul_mat(tensor->src[0], tensor->src[1], tensor)) {
                return false;
            }
            func = ggml_cl_mul_mat;
            break;
        case GGML_OP_SCALE:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_scale;
            break;
        case GGML_OP_RESHAPE:
        case GGML_OP_VIEW:
        case GGML_OP_PERMUTE:
        case GGML_OP_TRANSPOSE:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_nop;
            break;
        case GGML_OP_DIAG_MASK_INF:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_diag_mask_inf;
            break;
        case GGML_OP_SOFT_MAX:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_soft_max;
            break;
        case GGML_OP_ROPE:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_rope;
            break;
        case GGML_OP_IM2COL:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_im2col;
            break;
        case GGML_OP_ARGSORT:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_argsort;
            break;
        case GGML_OP_SUM_ROWS:
            if (!any_on_device) {
                return false;
            }
            func = ggml_cl_sum_rows;
            break;
        default:
            return false;
    }

    func(backend, tensor->src[0], tensor->src[1], tensor);
    return true;
}
