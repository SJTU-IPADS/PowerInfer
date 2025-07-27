#include <cstdint>
#include <cstring>

#include "powerinfer-cpu-data.hpp"
#include "powerinfer-cpu.hpp"

static const float *powerinfer_ggml_table_f32_f16 = nullptr;

void powerinfer_init_f16_table_impl(const float *table_ptr) {
    powerinfer_ggml_table_f32_f16 = table_ptr;
}

float ggml_lookup_fp16_to_fp32(const ggml_fp16_t f) {
    uint16_t s;
    std::memcpy(&s, &f, sizeof(uint16_t));
    return powerinfer_ggml_table_f32_f16[s];
}