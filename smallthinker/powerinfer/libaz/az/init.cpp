#include "az/init.hpp"

#include "az/core/fp16.h"
#include "az/core/perfetto_trace.hpp"
#include "az/cpu/exp_lut.hpp"
#include "az/cpu/silu_lut.hpp"

namespace az {

void init() {
    az_precompute_fp16_to_fp32_table();
    cpu::init_exp_lut();
    cpu::init_silu_lut();

#if defined(AZ_ENABLE_PERFETTO)
    const char *trace_path = getenv("AZ_TRACE_PATH");
    if (trace_path) {
        PerfettoTrace::instance().start_tracing(trace_path);
    }
#endif

}

void deinit() {
}

}  // namespace az
